/*
 *  MoldWing - DiligentWidget Implementation
 *  Enhanced with Blender-style camera controls
 */

#include "DiligentWidget.hpp"
#include "Core/Logger.hpp"
#include "Core/RayIntersection.hpp"
#include "Core/CompositeId.hpp"

#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QDebug>

#include <queue>
#include <cmath>

// DiligentEngine - use loader for shared DLL
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace Diligent;

namespace MoldWing
{

// =============================================================================
// DiligentWidget Implementation
// =============================================================================

DiligentWidget::DiligentWidget(QWidget* parent)
    : QWidget(parent)
{
    // Required for native window handle
    setAttribute(Qt::WA_NativeWindow);
    setAttribute(Qt::WA_PaintOnScreen);
    setAttribute(Qt::WA_NoSystemBackground);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);

    // Set minimum size
    setMinimumSize(640, 480);

    // Set size policy to expand and fill available space
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // Create render timer for continuous updates (~60 FPS)
    m_renderTimer = new QTimer(this);
    connect(m_renderTimer, &QTimer::timeout, this, QOverload<>::of(&DiligentWidget::update));
    m_renderTimer->start(16);

    // Initialize frame timer for deltaTime
    m_frameTimer.start();

    // Setup context menu
    setupContextMenu();

    // Create selection system
    m_selectionSystem = new SelectionSystem(this);
    connect(m_selectionSystem, &SelectionSystem::selectionChanged, this, [this]() {
        // Update selection renderer when selection changes
        if (m_selectionRenderer.isInitialized())
        {
            m_selectionRenderer.updateSelection(m_selectionSystem->selectedFaces());
        }
    });

    LOG_DEBUG("DiligentWidget created");
}

DiligentWidget::~DiligentWidget()
{
    LOG_DEBUG("DiligentWidget destroying");

    m_renderTimer->stop();

    // Release resources in correct order
    m_pSwapChain.Release();
    m_pContext.Release();
    m_pDevice.Release();
}

void DiligentWidget::setupContextMenu()
{
    m_contextMenu = new QMenu(this);

    // View submenu
    QMenu* viewMenu = m_contextMenu->addMenu(tr("View"));

    QAction* frontAction = viewMenu->addAction(tr("Front (Numpad 1)"));
    connect(frontAction, &QAction::triggered, this, &DiligentWidget::onContextMenuViewFront);

    QAction* backAction = viewMenu->addAction(tr("Back (Ctrl+Numpad 1)"));
    connect(backAction, &QAction::triggered, this, &DiligentWidget::onContextMenuViewBack);

    viewMenu->addSeparator();

    QAction* rightAction = viewMenu->addAction(tr("Right (Numpad 3)"));
    connect(rightAction, &QAction::triggered, this, &DiligentWidget::onContextMenuViewRight);

    QAction* leftAction = viewMenu->addAction(tr("Left (Ctrl+Numpad 3)"));
    connect(leftAction, &QAction::triggered, this, &DiligentWidget::onContextMenuViewLeft);

    viewMenu->addSeparator();

    QAction* topAction = viewMenu->addAction(tr("Top (Numpad 7)"));
    connect(topAction, &QAction::triggered, this, &DiligentWidget::onContextMenuViewTop);

    QAction* bottomAction = viewMenu->addAction(tr("Bottom (Ctrl+Numpad 7)"));
    connect(bottomAction, &QAction::triggered, this, &DiligentWidget::onContextMenuViewBottom);

    viewMenu->addSeparator();

    QAction* isoAction = viewMenu->addAction(tr("Isometric (Numpad 0)"));
    connect(isoAction, &QAction::triggered, this, &DiligentWidget::onContextMenuViewIsometric);

    m_contextMenu->addSeparator();

    QAction* resetAction = m_contextMenu->addAction(tr("Reset View (Home)"));
    connect(resetAction, &QAction::triggered, this, &DiligentWidget::onContextMenuResetView);

    QAction* orthoAction = m_contextMenu->addAction(tr("Toggle Orthographic (Numpad 5)"));
    connect(orthoAction, &QAction::triggered, this, &DiligentWidget::onContextMenuToggleOrtho);
}

void DiligentWidget::initializeDiligent()
{
    if (m_initialized)
        return;

    LOG_INFO("Initializing DiligentEngine...");

#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());

    // Load D3D11 engine DLL and get factory
    auto GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
    if (!GetEngineFactoryD3D11)
    {
        MW_LOG_ERROR("Failed to load GraphicsEngineD3D11 DLL!");
        return;
    }

    auto* pFactoryD3D11 = GetEngineFactoryD3D11();
    if (!pFactoryD3D11)
    {
        MW_LOG_ERROR("Failed to get D3D11 engine factory!");
        return;
    }

    EngineD3D11CreateInfo engineCI;
#ifdef _DEBUG
    engineCI.EnableValidation = true;
#endif

    pFactoryD3D11->CreateDeviceAndContextsD3D11(engineCI, &m_pDevice, &m_pContext);

    if (!m_pDevice)
    {
        MW_LOG_ERROR("Failed to create D3D11 render device!");
        return;
    }

    // Create swap chain (use device pixel ratio for high-DPI)
    qreal dpr = devicePixelRatio();
    SwapChainDesc swapChainDesc;
    swapChainDesc.Width = static_cast<Uint32>(width() * dpr);
    swapChainDesc.Height = static_cast<Uint32>(height() * dpr);
    swapChainDesc.ColorBufferFormat = TEX_FORMAT_RGBA8_UNORM_SRGB;
    swapChainDesc.DepthBufferFormat = TEX_FORMAT_D32_FLOAT;
    swapChainDesc.Usage = SWAP_CHAIN_USAGE_RENDER_TARGET;
    swapChainDesc.BufferCount = 2;
    swapChainDesc.DefaultDepthValue = 1.0f;  // Standard depth: 1 is far

    Win32NativeWindow window{hwnd};

    pFactoryD3D11->CreateSwapChainD3D11(
        m_pDevice, m_pContext, swapChainDesc, FullScreenModeDesc{}, window, &m_pSwapChain);

    if (!m_pSwapChain)
    {
        MW_LOG_ERROR("Failed to create swap chain!");
        return;
    }

    // Initialize mesh renderer
    if (!m_meshRenderer.initialize(m_pDevice, m_pSwapChain))
    {
        MW_LOG_ERROR("Failed to initialize mesh renderer!");
        return;
    }

    // Initialize pivot indicator
    m_pivotIndicator.initialize(m_pDevice);

    // Initialize face picker for GPU picking
    uint32_t pickerWidth = static_cast<uint32_t>(width() * dpr);
    uint32_t pickerHeight = static_cast<uint32_t>(height() * dpr);
    if (!m_facePicker.initialize(m_pDevice, pickerWidth, pickerHeight))
    {
        MW_LOG_ERROR("Failed to initialize face picker!");
        // Non-fatal, selection just won't work
    }

    // Initialize selection renderer
    if (!m_selectionRenderer.initialize(m_pDevice, m_pSwapChain))
    {
        MW_LOG_ERROR("Failed to initialize selection renderer!");
        // Non-fatal
    }

    // Initialize selection box renderer for 2D box selection overlay
    if (!m_selectionBoxRenderer.initialize(m_pDevice, m_pSwapChain))
    {
        MW_LOG_ERROR("Failed to initialize selection box renderer!");
        // Non-fatal
    }

    // Initialize brush cursor renderer for brush selection
    if (!m_brushCursorRenderer.initialize(m_pDevice, m_pSwapChain))
    {
        MW_LOG_ERROR("Failed to initialize brush cursor renderer!");
        // Non-fatal
    }

    // Initialize lasso renderer for lasso selection
    if (!m_lassoRenderer.initialize(m_pDevice, m_pSwapChain))
    {
        MW_LOG_ERROR("Failed to initialize lasso renderer!");
        // Non-fatal
    }

    // Set camera aspect ratio
    m_camera.setAspectRatio(static_cast<float>(width()) / static_cast<float>(height()));

    m_initialized = true;
    LOG_INFO("DiligentEngine initialized successfully");
    emit initialized();

#else
    MW_LOG_ERROR("Non-Windows platforms not yet supported");
#endif
}

bool DiligentWidget::loadMesh(std::shared_ptr<MeshData> mesh)
{
    if (!m_initialized || !mesh)
        return false;

    if (!m_meshRenderer.loadMesh(*mesh))
        return false;

    // Store mesh pointer for ray picking
    m_currentMesh = mesh.get();
    m_meshPtr = mesh;  // Keep shared_ptr for texture mapper

    // Load mesh into face picker
    if (m_facePicker.isInitialized())
    {
        m_facePicker.loadMesh(*mesh);
    }

    // Load mesh into selection renderer
    if (m_selectionRenderer.isInitialized())
    {
        m_selectionRenderer.loadMesh(*mesh);
    }

    // Update selection system face count
    m_selectionSystem->setFaceCount(static_cast<uint32_t>(mesh->faceCount()));

    // Step 1: Setup texture mapper
    m_textureMapper.setMesh(mesh);

    // Step 2: Initialize edit buffer if mesh has textures
    if (!mesh->textures.empty() && mesh->textures[0])
    {
        m_editBuffer = std::make_unique<TextureEditBuffer>();
        if (m_editBuffer->initialize(*mesh->textures[0]))
        {
            LOG_INFO("TextureEditBuffer initialized: {}x{}", m_editBuffer->width(), m_editBuffer->height());
        }
        else
        {
            MW_LOG_ERROR("Failed to initialize TextureEditBuffer");
            m_editBuffer.reset();
        }
    }

    // Fit camera to mesh bounds
    const auto& b = mesh->bounds;
    m_camera.fitToModel(b.min[0], b.min[1], b.min[2], b.max[0], b.max[1], b.max[2]);

    return true;
}

void DiligentWidget::render()
{
    if (!m_initialized || !m_pSwapChain)
        return;

    // Calculate deltaTime
    float deltaTime = m_frameTimer.elapsed() / 1000.0f;
    m_frameTimer.restart();

    // Clamp deltaTime to prevent huge jumps
    if (deltaTime > 0.1f)
        deltaTime = 0.1f;

    // Update camera (smoothing, inertia, animation)
    m_camera.update(deltaTime);

    auto* pRTV = m_pSwapChain->GetCurrentBackBufferRTV();
    auto* pDSV = m_pSwapChain->GetDepthBufferDSV();

    // Set render targets
    m_pContext->SetRenderTargets(1, &pRTV, pDSV, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // Set viewport to cover the entire swap chain
    const auto& swapChainDesc = m_pSwapChain->GetDesc();
    Viewport vp;
    vp.TopLeftX = 0;
    vp.TopLeftY = 0;
    vp.Width = static_cast<float>(swapChainDesc.Width);
    vp.Height = static_cast<float>(swapChainDesc.Height);
    vp.MinDepth = 0.0f;
    vp.MaxDepth = 1.0f;
    m_pContext->SetViewports(1, &vp, swapChainDesc.Width, swapChainDesc.Height);

    // Clear render target with dark gray color
    const float clearColor[] = {0.15f, 0.15f, 0.18f, 1.0f};
    m_pContext->ClearRenderTarget(pRTV, clearColor, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);
    m_pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);

    // M8: Render all visible mesh instances
    for (const auto& instance : m_meshInstances)
    {
        if (instance.visible && instance.renderer && instance.renderer->hasMesh())
        {
            instance.renderer->render(m_pContext, m_camera);
        }
    }

    // Legacy: Render single mesh if no instances but legacy renderer has mesh
    if (m_meshInstances.empty() && m_meshRenderer.hasMesh())
    {
        m_meshRenderer.render(m_pContext, m_camera);
    }

    // Render selection highlight (only when mesh is visible)
    // TODO: Update for multi-mesh selection
    if (m_selectionRenderer.isInitialized() && m_selectionRenderer.hasSelection())
    {
        m_selectionRenderer.render(m_pContext, m_camera);
    }

    // Render pivot indicator when rotating
    if (m_rotating && m_pivotIndicator.isInitialized())
    {
        float tx, ty, tz;
        m_camera.getTarget(tx, ty, tz);
        m_pivotIndicator.render(m_pContext, m_camera, tx, ty, tz, 1.0f);
    }

    // Render selection box overlay when box selecting
    if (m_boxSelecting && m_selectionBoxRenderer.isInitialized())
    {
        QRect selectRect = QRect(m_boxSelectStart, m_boxSelectCurrent).normalized();
        qreal dpr = devicePixelRatio();
        m_selectionBoxRenderer.render(
            m_pContext,
            static_cast<int>(selectRect.left() * dpr),
            static_cast<int>(selectRect.top() * dpr),
            static_cast<int>(selectRect.right() * dpr),
            static_cast<int>(selectRect.bottom() * dpr),
            static_cast<int>(swapChainDesc.Width),
            static_cast<int>(swapChainDesc.Height)
        );
    }

    // Render brush cursor when in brush selection mode
    if (m_interactionMode == InteractionMode::Selection &&
        m_selectionSystem->mode() == SelectionMode::Brush &&
        m_brushCursorRenderer.isInitialized())
    {
        qreal dpr = devicePixelRatio();
        m_brushCursorRenderer.render(
            m_pContext,
            static_cast<int>(m_brushPosition.x() * dpr),
            static_cast<int>(m_brushPosition.y() * dpr),
            static_cast<int>(m_brushRadius * dpr),
            static_cast<int>(swapChainDesc.Width),
            static_cast<int>(swapChainDesc.Height)
        );
    }

    // Render lasso path when in lasso selection mode
    if (m_lassoSelecting && m_lassoRenderer.isInitialized())
    {
        m_lassoRenderer.render(
            m_pContext,
            static_cast<int>(swapChainDesc.Width),
            static_cast<int>(swapChainDesc.Height)
        );
    }

    // Present
    m_pSwapChain->Present();
}

void DiligentWidget::paintEvent(QPaintEvent* /*event*/)
{
    if (!m_initialized)
    {
        initializeDiligent();
    }

    render();
}

void DiligentWidget::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);

    if (m_initialized && m_pSwapChain)
    {
        // Use device pixel ratio for high-DPI displays
        qreal dpr = devicePixelRatio();
        Uint32 newWidth = static_cast<Uint32>(event->size().width() * dpr);
        Uint32 newHeight = static_cast<Uint32>(event->size().height() * dpr);

        if (newWidth > 0 && newHeight > 0)
        {
            m_pSwapChain->Resize(newWidth, newHeight);
            m_camera.setAspectRatio(static_cast<float>(newWidth) / static_cast<float>(newHeight));

            // Resize face picker
            if (m_facePicker.isInitialized())
            {
                m_facePicker.resize(newWidth, newHeight);
            }
        }
    }
}

RotationConstraint DiligentWidget::getRotationConstraint() const
{
    if (m_ctrlHeld)
    {
        return RotationConstraint::Snap45;
    }
    // For horizontal/vertical only constraint, we would need first movement direction
    // For now, just return None when Shift is held (could be enhanced later)
    return RotationConstraint::None;
}

void DiligentWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();

    // Update modifier state
    m_shiftHeld = event->modifiers() & Qt::ShiftModifier;
    m_ctrlHeld = event->modifiers() & Qt::ControlModifier;
    m_altHeld = event->modifiers() & Qt::AltModifier;

    // Step 1: Alt+左键点击 - 纹理坐标拾取 / Step 4: 克隆源设置
    if (m_altHeld && event->button() == Qt::LeftButton)
    {
        if (m_facePicker.isInitialized() && m_facePicker.hasMesh())
        {
            // Render ID buffer
            m_facePicker.renderIDBuffer(m_pContext, m_camera);

            // Read face ID at cursor position
            qreal dpr = devicePixelRatio();
            int x = static_cast<int>(event->pos().x() * dpr);
            int y = static_cast<int>(event->pos().y() * dpr);

            uint32_t compositeId = m_facePicker.readFaceID(m_pContext, x, y);

            if (compositeId != FacePicker::INVALID_FACE_ID)
            {
                // M8: Extract meshId and faceId from composite ID
                uint32_t meshId = CompositeId::meshId(compositeId);
                uint32_t faceId = CompositeId::faceId(compositeId);

                // Use ScreenToTextureMapper to get UV coordinates
                // Note: Pass the actual faceId (not composite) to texture mapper
                auto result = m_textureMapper.mapScreenToTexture(
                    faceId,
                    event->pos().x(), event->pos().y(),
                    m_camera,
                    width(), height());

                if (result.valid)
                {
                    emit textureCoordPicked(result.u, result.v, result.texX, result.texY, compositeId);

                    // Step 4: In TextureEdit mode, Alt+click sets clone source
                    if (m_interactionMode == InteractionMode::TextureEdit)
                    {
                        m_cloneSourceTexX = result.texX;
                        m_cloneSourceTexY = result.texY;
                        m_cloneSourceSet = true;
                        m_cloneFirstDestTexX = -1;  // Reset first dest for new source
                        m_cloneFirstDestTexY = -1;

                        emit cloneSourceSet(result.texX, result.texY);
                        LOG_DEBUG("Clone source set at texture ({}, {})", result.texX, result.texY);
                    }
                    else
                    {
                        // Step 2: 在纹理上绘制红色方块验证（非 TextureEdit 模式）
                        if (m_editBuffer && m_editBuffer->isValid())
                        {
                            const int squareSize = 20;
                            int startX = result.texX - squareSize / 2;
                            int startY = result.texY - squareSize / 2;

                            // 绘制红色方块
                            for (int dy = 0; dy < squareSize; ++dy)
                            {
                                for (int dx = 0; dx < squareSize; ++dx)
                                {
                                    int px = startX + dx;
                                    int py = startY + dy;

                                    // 边界检查
                                    if (px >= 0 && px < m_editBuffer->width() &&
                                        py >= 0 && py < m_editBuffer->height())
                                    {
                                        m_editBuffer->setPixel(px, py, 255, 0, 0, 255);  // 红色
                                    }
                                }
                            }

                            // 更新 GPU 纹理
                            m_meshRenderer.updateTextureFromBuffer(m_pContext, 0, *m_editBuffer);
                            m_editBuffer->clearDirty();

                            LOG_DEBUG("Drew red square at texture ({}, {})", result.texX, result.texY);
                        }
                    }
                }
                else
                {
                    LOG_DEBUG("Texture mapping failed for mesh {} face {}", meshId, faceId);
                }
            }
            else
            {
                LOG_DEBUG("No face hit at ({}, {})", event->pos().x(), event->pos().y());
            }
        }
        return;  // Alt+点击处理完毕，不继续其他逻辑
    }

    // Selection mode: LMB starts selection based on current selection mode
    if (m_interactionMode == InteractionMode::Selection)
    {
        if (event->button() == Qt::LeftButton)
        {
            if (m_selectionSystem->mode() == SelectionMode::Brush)
            {
                beginBrushSelect(event->pos());
            }
            else if (m_selectionSystem->mode() == SelectionMode::Lasso)
            {
                beginLassoSelect(event->pos());
            }
            else if (m_selectionSystem->mode() == SelectionMode::Link)
            {
                performLinkSelect(event->pos());
            }
            else
            {
                beginBoxSelect(event->pos());
            }
            return;
        }
    }

    // Step 3: TextureEdit mode - LMB starts painting
    if (m_interactionMode == InteractionMode::TextureEdit)
    {
        if (event->button() == Qt::LeftButton)
        {
            beginTexturePaint(event->pos());
            return;
        }
    }

    // Camera controls:
    // MMB = rotate
    // Shift + MMB = pan
    // RMB = context menu (handled by contextMenuEvent)

    if (event->button() == Qt::MiddleButton)
    {
        if (m_shiftHeld)
        {
            m_panning = true;
            m_camera.beginPan();
        }
        else
        {
            m_rotating = true;
            m_camera.beginRotate();
        }
    }
    // RMB is reserved for context menu
}

void DiligentWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // Handle texture painting end
        if (m_texturePainting)
        {
            endTexturePaint();
            return;
        }

        // Handle box selection end
        if (m_boxSelecting)
        {
            endBoxSelect();
            return;
        }

        // Handle brush selection end
        if (m_brushSelecting)
        {
            endBrushSelect();
            return;
        }

        // Handle lasso selection end
        if (m_lassoSelecting)
        {
            endLassoSelect();
            return;
        }
    }
    else if (event->button() == Qt::MiddleButton)
    {
        if (m_rotating)
        {
            m_rotating = false;
            m_camera.endRotate();
        }

        if (m_panning)
        {
            m_panning = false;
            m_camera.endPan();

            // After pan, update rotation pivot to screen center hit point
            if (m_currentMesh && m_currentMesh->faceCount() > 0)
            {
                float camPosX, camPosY, camPosZ;
                m_camera.getPosition(camPosX, camPosY, camPosZ);

                float rayDirX, rayDirY, rayDirZ;
                m_camera.screenToWorldRay(0.5f, 0.5f, rayDirX, rayDirY, rayDirZ);

                Ray ray(camPosX, camPosY, camPosZ, rayDirX, rayDirY, rayDirZ);
                HitResult hit;

                if (RayIntersection::rayMesh(ray, *m_currentMesh, hit))
                {
                    m_camera.setTarget(hit.hitX, hit.hitY, hit.hitZ);
                }
            }
        }
    }
    // RMB is reserved for context menu
}

void DiligentWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    // Update modifier state
    m_shiftHeld = event->modifiers() & Qt::ShiftModifier;
    m_ctrlHeld = event->modifiers() & Qt::ControlModifier;

    // Update brush position when in brush selection mode
    if (m_interactionMode == InteractionMode::Selection &&
        m_selectionSystem->mode() == SelectionMode::Brush)
    {
        m_brushPosition = event->pos();
    }

    // Handle box selection drag
    if (m_boxSelecting)
    {
        updateBoxSelect(event->pos());
        return;
    }

    // Handle brush selection drag
    if (m_brushSelecting)
    {
        updateBrushSelect(event->pos());
        return;
    }

    // Handle lasso selection drag
    if (m_lassoSelecting)
    {
        updateLassoSelect(event->pos());
        return;
    }

    // Step 3: Handle texture painting drag
    if (m_texturePainting)
    {
        updateTexturePaint(event->pos());
        return;
    }

    if (m_rotating)
    {
        // Rotate camera with optional constraints
        RotationConstraint constraint = getRotationConstraint();
        m_camera.rotate(static_cast<float>(delta.x()), static_cast<float>(delta.y()), constraint);
    }
    else if (m_panning)
    {
        // Pan camera with 1:1 screen-to-world mapping
        // Pass pixel deltas and viewport size for accurate mapping
        m_camera.pan(static_cast<float>(delta.x()), static_cast<float>(-delta.y()),
                     width(), height());
    }
}

void DiligentWidget::wheelEvent(QWheelEvent* event)
{
    // Get cursor position for zoom-to-cursor
    QPointF pos = event->position();
    float cursorX = static_cast<float>(pos.x()) / static_cast<float>(width());
    float cursorY = static_cast<float>(pos.y()) / static_cast<float>(height());

    // Zoom camera (positive delta = zoom in)
    float delta = event->angleDelta().y() / 120.0f;
    m_camera.zoom(delta, cursorX, cursorY);
}

void DiligentWidget::keyPressEvent(QKeyEvent* event)
{
    // Update modifier state
    m_shiftHeld = event->modifiers() & Qt::ShiftModifier;
    m_ctrlHeld = event->modifiers() & Qt::ControlModifier;
    m_altHeld = event->modifiers() & Qt::AltModifier;

    // Handle view preset keys (Blender-style numpad)
    switch (event->key())
    {
        // Numpad view presets
        case Qt::Key_1:
            if (event->modifiers() & Qt::KeypadModifier)
            {
                if (m_ctrlHeld)
                    m_camera.setViewPreset(ViewPreset::Back);
                else
                    m_camera.setViewPreset(ViewPreset::Front);
            }
            break;

        case Qt::Key_3:
            if (event->modifiers() & Qt::KeypadModifier)
            {
                if (m_ctrlHeld)
                    m_camera.setViewPreset(ViewPreset::Left);
                else
                    m_camera.setViewPreset(ViewPreset::Right);
            }
            break;

        case Qt::Key_7:
            if (event->modifiers() & Qt::KeypadModifier)
            {
                if (m_ctrlHeld)
                    m_camera.setViewPreset(ViewPreset::Bottom);
                else
                    m_camera.setViewPreset(ViewPreset::Top);
            }
            break;

        case Qt::Key_0:
            if (event->modifiers() & Qt::KeypadModifier)
            {
                m_camera.setViewPreset(ViewPreset::Isometric);
            }
            break;

        case Qt::Key_5:
            if (event->modifiers() & Qt::KeypadModifier)
            {
                m_camera.toggleOrthographic();
            }
            break;

        // Home key for reset view
        case Qt::Key_Home:
            m_camera.reset();
            break;

        // Period/Delete for focus (if we had selection)
        case Qt::Key_Period:
            // Future: focus on selection
            break;

        // Bracket keys for brush radius adjustment
        case Qt::Key_BracketLeft:
            if (m_selectionSystem->mode() == SelectionMode::Brush)
            {
                setBrushRadius(m_brushRadius - 5);
            }
            break;

        case Qt::Key_BracketRight:
            if (m_selectionSystem->mode() == SelectionMode::Brush)
            {
                setBrushRadius(m_brushRadius + 5);
            }
            break;

        default:
            QWidget::keyPressEvent(event);
            return;
    }

    event->accept();
}

void DiligentWidget::keyReleaseEvent(QKeyEvent* event)
{
    // Update modifier state
    m_shiftHeld = event->modifiers() & Qt::ShiftModifier;
    m_ctrlHeld = event->modifiers() & Qt::ControlModifier;
    m_altHeld = event->modifiers() & Qt::AltModifier;

    QWidget::keyReleaseEvent(event);
}

void DiligentWidget::contextMenuEvent(QContextMenuEvent* event)
{
    m_contextMenu->exec(event->globalPos());
}

// Context menu slots
void DiligentWidget::onContextMenuViewFront()
{
    m_camera.setViewPreset(ViewPreset::Front);
}

void DiligentWidget::onContextMenuViewBack()
{
    m_camera.setViewPreset(ViewPreset::Back);
}

void DiligentWidget::onContextMenuViewLeft()
{
    m_camera.setViewPreset(ViewPreset::Left);
}

void DiligentWidget::onContextMenuViewRight()
{
    m_camera.setViewPreset(ViewPreset::Right);
}

void DiligentWidget::onContextMenuViewTop()
{
    m_camera.setViewPreset(ViewPreset::Top);
}

void DiligentWidget::onContextMenuViewBottom()
{
    m_camera.setViewPreset(ViewPreset::Bottom);
}

void DiligentWidget::onContextMenuViewIsometric()
{
    m_camera.setViewPreset(ViewPreset::Isometric);
}

void DiligentWidget::onContextMenuResetView()
{
    m_camera.reset();
}

void DiligentWidget::onContextMenuToggleOrtho()
{
    m_camera.toggleOrthographic();
}

// Selection helper methods

void DiligentWidget::setInteractionMode(InteractionMode mode)
{
    if (m_interactionMode != mode)
    {
        m_interactionMode = mode;
        emit interactionModeChanged(mode);

        // Cancel any ongoing operations when switching modes
        if (m_boxSelecting)
        {
            m_boxSelecting = false;
        }
        if (m_texturePainting)
        {
            m_texturePainting = false;
        }

        const char* modeName = "Unknown";
        switch (mode)
        {
            case InteractionMode::Camera: modeName = "Camera"; break;
            case InteractionMode::Selection: modeName = "Selection"; break;
            case InteractionMode::TextureEdit: modeName = "TextureEdit"; break;
        }
        LOG_DEBUG("Interaction mode changed to {}", modeName);
    }
}

void DiligentWidget::beginBoxSelect(const QPoint& pos)
{
    m_boxSelectStart = pos;
    m_boxSelectCurrent = pos;
    m_boxSelecting = true;
}

void DiligentWidget::updateBoxSelect(const QPoint& pos)
{
    m_boxSelectCurrent = pos;
}

void DiligentWidget::endBoxSelect()
{
    m_boxSelecting = false;

    // Get selection rectangle
    QRect selectRect = QRect(m_boxSelectStart, m_boxSelectCurrent).normalized();

    // Skip if rectangle is too small (treat as click)
    if (selectRect.width() < 3 && selectRect.height() < 3)
    {
        // Single click - select single face under cursor
        if (m_facePicker.isInitialized() && m_facePicker.hasMesh())
        {
            // Render ID buffer
            m_facePicker.renderIDBuffer(m_pContext, m_camera);

            // Read face ID at cursor position (account for DPI scaling)
            qreal dpr = devicePixelRatio();
            int x = static_cast<int>(m_lastMousePos.x() * dpr);
            int y = static_cast<int>(m_lastMousePos.y() * dpr);

            uint32_t faceID = m_facePicker.readFaceID(m_pContext, x, y);

            if (faceID != FacePicker::INVALID_FACE_ID)
            {
                // Apply selection operation
                SelectionOp op = getSelectionOp();

                // Create undo command if undo stack is available
                if (m_undoStack)
                {
                    auto newSelection = m_selectionSystem->selectedFaces();
                    switch (op)
                    {
                        case SelectionOp::Replace:
                            newSelection.clear();
                            newSelection.insert(faceID);
                            break;
                        case SelectionOp::Add:
                            newSelection.insert(faceID);
                            break;
                        case SelectionOp::Subtract:
                            newSelection.erase(faceID);
                            break;
                        case SelectionOp::Toggle:
                            if (newSelection.count(faceID))
                                newSelection.erase(faceID);
                            else
                                newSelection.insert(faceID);
                            break;
                    }
                    m_undoStack->push(new SelectFacesCommand(m_selectionSystem, newSelection));
                }
                else
                {
                    m_selectionSystem->selectSingle(faceID, op);
                }
            }
            else if (getSelectionOp() == SelectionOp::Replace)
            {
                // Clicked on background - clear selection
                if (m_undoStack)
                {
                    m_undoStack->push(new SelectFacesCommand(m_selectionSystem, {}));
                }
                else
                {
                    m_selectionSystem->clearSelection();
                }
            }
        }
        return;
    }

    // Box selection
    if (m_facePicker.isInitialized() && m_facePicker.hasMesh())
    {
        // Render ID buffer
        m_facePicker.renderIDBuffer(m_pContext, m_camera);

        // Read face IDs in rectangle (account for DPI scaling)
        qreal dpr = devicePixelRatio();
        int x1 = static_cast<int>(selectRect.left() * dpr);
        int y1 = static_cast<int>(selectRect.top() * dpr);
        int x2 = static_cast<int>(selectRect.right() * dpr);
        int y2 = static_cast<int>(selectRect.bottom() * dpr);

        std::vector<uint32_t> faceIDs = m_facePicker.readFaceIDsInRect(m_pContext, x1, y1, x2, y2);

        if (!faceIDs.empty())
        {
            // Apply selection operation
            SelectionOp op = getSelectionOp();

            if (m_undoStack)
            {
                auto newSelection = m_selectionSystem->selectedFaces();
                switch (op)
                {
                    case SelectionOp::Replace:
                        newSelection.clear();
                        for (uint32_t f : faceIDs)
                            newSelection.insert(f);
                        break;
                    case SelectionOp::Add:
                        for (uint32_t f : faceIDs)
                            newSelection.insert(f);
                        break;
                    case SelectionOp::Subtract:
                        for (uint32_t f : faceIDs)
                            newSelection.erase(f);
                        break;
                    case SelectionOp::Toggle:
                        for (uint32_t f : faceIDs)
                        {
                            if (newSelection.count(f))
                                newSelection.erase(f);
                            else
                                newSelection.insert(f);
                        }
                        break;
                }
                m_undoStack->push(new SelectFacesCommand(m_selectionSystem, newSelection));
            }
            else
            {
                m_selectionSystem->select(faceIDs, op);
            }
        }
        else if (getSelectionOp() == SelectionOp::Replace)
        {
            // Empty box selection - clear selection
            if (m_undoStack)
            {
                m_undoStack->push(new SelectFacesCommand(m_selectionSystem, {}));
            }
            else
            {
                m_selectionSystem->clearSelection();
            }
        }

        LOG_DEBUG("Box select: {} faces", faceIDs.size());
    }
}

SelectionOp DiligentWidget::getSelectionOp() const
{
    if (m_ctrlHeld)
        return SelectionOp::Add;       // Ctrl = 增选
    else if (m_shiftHeld)
        return SelectionOp::Subtract;  // Shift = 减选
    else
        return SelectionOp::Replace;   // 默认 = 替换
}

// Brush selection methods

void DiligentWidget::setBrushRadius(int radius)
{
    int newRadius = std::max(MIN_BRUSH_RADIUS, std::min(radius, MAX_BRUSH_RADIUS));
    if (newRadius != m_brushRadius)
    {
        m_brushRadius = newRadius;
        emit brushRadiusChanged(m_brushRadius);
        LOG_DEBUG("Brush radius changed to {}", m_brushRadius);
    }
}

void DiligentWidget::beginBrushSelect(const QPoint& pos)
{
    m_brushSelecting = true;
    m_brushPosition = pos;
    m_brushSelectAccumulated.clear();

    // If not additive/subtractive mode, clear existing selection
    SelectionOp op = getSelectionOp();
    if (op == SelectionOp::Replace)
    {
        m_selectionSystem->clearSelection();
    }

    // Perform initial brush selection
    updateBrushSelect(pos);
}

void DiligentWidget::updateBrushSelect(const QPoint& pos)
{
    m_brushPosition = pos;

    if (!m_facePicker.isInitialized() || !m_facePicker.hasMesh())
        return;

    // Render ID buffer
    m_facePicker.renderIDBuffer(m_pContext, m_camera);

    // Read face IDs in brush circle (account for DPI scaling)
    qreal dpr = devicePixelRatio();
    int cx = static_cast<int>(pos.x() * dpr);
    int cy = static_cast<int>(pos.y() * dpr);
    int r = static_cast<int>(m_brushRadius * dpr);

    std::vector<uint32_t> faceIDs = m_facePicker.readFaceIDsInCircle(m_pContext, cx, cy, r);

    if (!faceIDs.empty())
    {
        SelectionOp op = getSelectionOp();

        // Accumulate selected faces during drag
        for (uint32_t faceID : faceIDs)
        {
            m_brushSelectAccumulated.insert(faceID);
        }

        // Apply selection immediately for visual feedback
        switch (op)
        {
            case SelectionOp::Replace:
            case SelectionOp::Add:
                m_selectionSystem->select(faceIDs, SelectionOp::Add);
                break;
            case SelectionOp::Subtract:
                m_selectionSystem->select(faceIDs, SelectionOp::Subtract);
                break;
            case SelectionOp::Toggle:
                // For toggle, we accumulate but don't toggle repeatedly
                // Just add during drag, toggle will be applied at end
                m_selectionSystem->select(faceIDs, SelectionOp::Add);
                break;
        }
    }
}

void DiligentWidget::endBrushSelect()
{
    m_brushSelecting = false;

    // Create undo command for the entire brush stroke if we accumulated faces
    if (!m_brushSelectAccumulated.empty() && m_undoStack)
    {
        // Note: The selection has already been applied, so we just need to
        // ensure the undo stack reflects the change.
        // We'll push a command with the final selection state.
        auto finalSelection = m_selectionSystem->selectedFaces();
        m_undoStack->push(new SelectFacesCommand(m_selectionSystem, finalSelection));
    }

    m_brushSelectAccumulated.clear();
    LOG_DEBUG("Brush select ended");
}

// Lasso selection methods

void DiligentWidget::beginLassoSelect(const QPoint& pos)
{
    m_lassoSelecting = true;
    m_lassoPath.clear();
    m_lassoPath.append(QPointF(pos));

    // Initialize LassoRenderer path
    qreal dpr = devicePixelRatio();
    m_lassoRenderer.beginPath(static_cast<int>(pos.x() * dpr), static_cast<int>(pos.y() * dpr));

    LOG_DEBUG("Lasso select started at ({}, {})", pos.x(), pos.y());
}

void DiligentWidget::updateLassoSelect(const QPoint& pos)
{
    if (!m_lassoSelecting)
        return;

    // Add point to path
    m_lassoPath.append(QPointF(pos));

    // Update LassoRenderer
    qreal dpr = devicePixelRatio();
    m_lassoRenderer.addPoint(static_cast<int>(pos.x() * dpr), static_cast<int>(pos.y() * dpr));
}

void DiligentWidget::endLassoSelect()
{
    m_lassoSelecting = false;

    // Need at least 3 points to form a polygon
    if (m_lassoPath.size() < 3)
    {
        m_lassoPath.clear();
        m_lassoRenderer.clearPath();
        LOG_DEBUG("Lasso select cancelled: not enough points");
        return;
    }

    // Close the polygon by connecting back to first point
    // (QPolygonF::containsPoint handles this automatically)

    if (!m_facePicker.isInitialized() || !m_facePicker.hasMesh() || !m_currentMesh)
    {
        m_lassoPath.clear();
        m_lassoRenderer.clearPath();
        return;
    }

    // Render ID buffer to get face IDs
    m_facePicker.renderIDBuffer(m_pContext, m_camera);

    // Get the bounding box of the lasso path for efficient iteration
    QRectF bounds = m_lassoPath.boundingRect();
    qreal dpr = devicePixelRatio();

    int minX = static_cast<int>(bounds.left() * dpr);
    int minY = static_cast<int>(bounds.top() * dpr);
    int maxX = static_cast<int>(bounds.right() * dpr);
    int maxY = static_cast<int>(bounds.bottom() * dpr);

    // Read all face IDs in the bounding box
    std::vector<uint32_t> faceIDsInRect = m_facePicker.readFaceIDsInRect(m_pContext, minX, minY, maxX, maxY);

    // For each unique face ID, check if its screen-space center is inside the lasso polygon
    std::unordered_set<uint32_t> selectedFaces;

    // Get unique face IDs
    std::unordered_set<uint32_t> uniqueFaceIDs(faceIDsInRect.begin(), faceIDsInRect.end());

    // For each face, project its center to screen space and check if inside polygon
    for (uint32_t compositeId : uniqueFaceIDs)
    {
        if (compositeId == FacePicker::INVALID_FACE_ID)
            continue;

        // M8: Decode composite ID to get meshId and faceId
        uint32_t meshId = CompositeId::meshId(compositeId);
        uint32_t faceId = CompositeId::faceId(compositeId);

        // Find the mesh instance by meshId
        const MeshData* meshData = nullptr;
        for (const auto& instance : m_meshInstances)
        {
            if (static_cast<uint32_t>(instance.id) == meshId && instance.mesh)
            {
                meshData = instance.mesh.get();
                break;
            }
        }

        // Fallback to legacy single mesh if no instance found (meshId 0)
        if (!meshData && meshId == 0 && m_currentMesh)
        {
            meshData = m_currentMesh;
        }

        if (!meshData)
            continue;

        // Get face center in world space
        if (faceId >= meshData->faceCount())
            continue;

        // Get face vertices
        uint32_t i0 = meshData->indices[faceId * 3 + 0];
        uint32_t i1 = meshData->indices[faceId * 3 + 1];
        uint32_t i2 = meshData->indices[faceId * 3 + 2];

        const auto& v0 = meshData->vertices[i0];
        const auto& v1 = meshData->vertices[i1];
        const auto& v2 = meshData->vertices[i2];

        // Calculate face center
        float centerX = (v0.position[0] + v1.position[0] + v2.position[0]) / 3.0f;
        float centerY = (v0.position[1] + v1.position[1] + v2.position[1]) / 3.0f;
        float centerZ = (v0.position[2] + v1.position[2] + v2.position[2]) / 3.0f;

        // Project to screen space
        float screenX, screenY;
        bool visible = m_camera.worldToScreen(centerX, centerY, centerZ, screenX, screenY);

        if (!visible)
            continue;

        // Convert normalized screen coords to pixel coords
        float pixelX = screenX * width();
        float pixelY = screenY * height();

        // Check if point is inside the lasso polygon
        if (m_lassoPath.containsPoint(QPointF(pixelX, pixelY), Qt::OddEvenFill))
        {
            selectedFaces.insert(compositeId);
        }
    }

    // Apply selection
    if (!selectedFaces.empty())
    {
        std::vector<uint32_t> faceVector(selectedFaces.begin(), selectedFaces.end());
        SelectionOp op = getSelectionOp();

        if (m_undoStack)
        {
            auto newSelection = m_selectionSystem->selectedFaces();
            switch (op)
            {
                case SelectionOp::Replace:
                    newSelection.clear();
                    for (uint32_t f : faceVector)
                        newSelection.insert(f);
                    break;
                case SelectionOp::Add:
                    for (uint32_t f : faceVector)
                        newSelection.insert(f);
                    break;
                case SelectionOp::Subtract:
                    for (uint32_t f : faceVector)
                        newSelection.erase(f);
                    break;
                case SelectionOp::Toggle:
                    for (uint32_t f : faceVector)
                    {
                        if (newSelection.count(f))
                            newSelection.erase(f);
                        else
                            newSelection.insert(f);
                    }
                    break;
            }
            m_undoStack->push(new SelectFacesCommand(m_selectionSystem, newSelection));
        }
        else
        {
            m_selectionSystem->select(faceVector, op);
        }

        LOG_DEBUG("Lasso select: {} faces", selectedFaces.size());
    }
    else if (getSelectionOp() == SelectionOp::Replace)
    {
        // Empty lasso selection - clear selection
        if (m_undoStack)
        {
            m_undoStack->push(new SelectFacesCommand(m_selectionSystem, {}));
        }
        else
        {
            m_selectionSystem->clearSelection();
        }
        LOG_DEBUG("Lasso select: cleared selection");
    }

    m_lassoPath.clear();
    m_lassoRenderer.clearPath();
}

// M5: Link selection

void DiligentWidget::setLinkAngleThreshold(float angle)
{
    float newAngle = std::max(MIN_ANGLE_THRESHOLD, std::min(angle, MAX_ANGLE_THRESHOLD));
    if (std::abs(newAngle - m_linkAngleThreshold) > 0.01f)
    {
        m_linkAngleThreshold = newAngle;
        emit linkAngleThresholdChanged(m_linkAngleThreshold);
        LOG_DEBUG("Link angle threshold changed to {}", m_linkAngleThreshold);
    }
}

void DiligentWidget::performLinkSelect(const QPoint& pos)
{
    if (!m_facePicker.isInitialized() || !m_facePicker.hasMesh())
        return;

    // Render ID buffer
    m_facePicker.renderIDBuffer(m_pContext, m_camera);

    // Read face ID at cursor position (account for DPI scaling)
    qreal dpr = devicePixelRatio();
    int x = static_cast<int>(pos.x() * dpr);
    int y = static_cast<int>(pos.y() * dpr);

    uint32_t compositeId = m_facePicker.readFaceID(m_pContext, x, y);

    if (compositeId == FacePicker::INVALID_FACE_ID)
    {
        // Clicked on background - clear selection (if Replace mode)
        if (getSelectionOp() == SelectionOp::Replace)
        {
            if (m_undoStack)
            {
                m_undoStack->push(new SelectFacesCommand(m_selectionSystem, {}));
            }
            else
            {
                m_selectionSystem->clearSelection();
            }
        }
        return;
    }

    // M8: Decode composite ID to get meshId and faceId
    uint32_t meshId = CompositeId::meshId(compositeId);
    uint32_t seedFace = CompositeId::faceId(compositeId);

    // Find the mesh instance by meshId
    const MeshData* meshData = nullptr;
    for (const auto& instance : m_meshInstances)
    {
        if (static_cast<uint32_t>(instance.id) == meshId && instance.mesh)
        {
            meshData = instance.mesh.get();
            break;
        }
    }

    // Fallback to legacy single mesh
    if (!meshData && meshId == 0 && m_currentMesh)
    {
        meshData = m_currentMesh;
    }

    if (!meshData)
    {
        LOG_WARN("performLinkSelect: Mesh {} not found", meshId);
        return;
    }

    SelectionOp op = getSelectionOp();

    // Check if we have adjacency data
    if (meshData->faceAdjacency.empty())
    {
        LOG_WARN("Mesh {} has no adjacency data for link selection", meshId);
        return;
    }

    // Validate seed face
    if (seedFace >= meshData->faceAdjacency.size())
    {
        LOG_WARN("performLinkSelect: Invalid seed face {} for mesh {}", seedFace, meshId);
        return;
    }

    // Perform BFS to find connected faces within this mesh
    std::unordered_set<uint32_t> connectedFaces;
    std::queue<uint32_t> queue;
    queue.push(seedFace);
    connectedFaces.insert(seedFace);

    bool useAngleConstraint = m_linkAngleThreshold < MAX_ANGLE_THRESHOLD - 0.01f;
    float cosThreshold = 0.0f;
    if (useAngleConstraint && !meshData->faceNormals.empty())
    {
        cosThreshold = std::cos(m_linkAngleThreshold * 3.14159265358979f / 180.0f);
    }
    else
    {
        useAngleConstraint = false;  // No normals available
    }

    while (!queue.empty())
    {
        uint32_t current = queue.front();
        queue.pop();

        for (uint32_t neighbor : meshData->faceAdjacency[current])
        {
            if (connectedFaces.find(neighbor) == connectedFaces.end())
            {
                bool include = true;

                // Check angle constraint if enabled
                if (useAngleConstraint && current < meshData->faceNormals.size() &&
                    neighbor < meshData->faceNormals.size())
                {
                    const auto& currentNormal = meshData->faceNormals[current];
                    const auto& neighborNormal = meshData->faceNormals[neighbor];
                    float dot = currentNormal[0] * neighborNormal[0] +
                                currentNormal[1] * neighborNormal[1] +
                                currentNormal[2] * neighborNormal[2];
                    include = (dot >= cosThreshold);
                }

                if (include)
                {
                    connectedFaces.insert(neighbor);
                    queue.push(neighbor);
                }
            }
        }
    }

    // Convert plain face IDs to composite IDs
    std::vector<uint32_t> compositeIds;
    compositeIds.reserve(connectedFaces.size());
    for (uint32_t faceId : connectedFaces)
    {
        compositeIds.push_back(CompositeId::make(meshId, faceId));
    }

    // Apply selection with composite IDs
    if (m_undoStack)
    {
        auto newSelection = m_selectionSystem->selectedFaces();
        switch (op)
        {
            case SelectionOp::Replace:
                newSelection.clear();
                for (uint32_t id : compositeIds)
                    newSelection.insert(id);
                break;
            case SelectionOp::Add:
                for (uint32_t id : compositeIds)
                    newSelection.insert(id);
                break;
            case SelectionOp::Subtract:
                for (uint32_t id : compositeIds)
                    newSelection.erase(id);
                break;
            case SelectionOp::Toggle:
                for (uint32_t id : compositeIds)
                {
                    if (newSelection.count(id))
                        newSelection.erase(id);
                    else
                        newSelection.insert(id);
                }
                break;
        }
        m_undoStack->push(new SelectFacesCommand(m_selectionSystem, newSelection,
                          QObject::tr("Link Select")));
    }
    else
    {
        m_selectionSystem->select(compositeIds, op);
    }

    LOG_DEBUG("Link select: {} faces from mesh {} (seed {})", connectedFaces.size(), meshId, seedFace);
}

// Step 3: Texture painting methods

void DiligentWidget::beginTexturePaint(const QPoint& pos)
{
    if (!m_editBuffer || !m_editBuffer->isValid())
        return;

    m_texturePainting = true;

    // Step 6: Create undo command for this stroke
    if (m_undoStack)
    {
        m_currentEditCommand = new TextureEditCommand(m_editBuffer.get(), 0);
    }

    // Paint at initial position
    updateTexturePaint(pos);

    LOG_DEBUG("Texture painting started at ({}, {})", pos.x(), pos.y());
}

void DiligentWidget::updateTexturePaint(const QPoint& pos)
{
    if (!m_texturePainting || !m_editBuffer || !m_editBuffer->isValid())
        return;

    if (!m_facePicker.isInitialized() || !m_facePicker.hasMesh())
        return;

    // Render ID buffer
    m_facePicker.renderIDBuffer(m_pContext, m_camera);

    // Read face ID at cursor position
    qreal dpr = devicePixelRatio();
    int x = static_cast<int>(pos.x() * dpr);
    int y = static_cast<int>(pos.y() * dpr);

    uint32_t faceId = m_facePicker.readFaceID(m_pContext, x, y);

    if (faceId == FacePicker::INVALID_FACE_ID)
        return;

    // Map screen position to texture coordinates
    auto result = m_textureMapper.mapScreenToTexture(
        faceId,
        pos.x(), pos.y(),
        m_camera,
        width(), height());

    if (!result.valid)
        return;

    // Paint brush at texture position
    paintBrushAtPosition(result.texX, result.texY);

    // Update GPU texture
    m_meshRenderer.updateTextureFromBuffer(m_pContext, 0, *m_editBuffer);
    m_editBuffer->clearDirty();
}

void DiligentWidget::endTexturePaint()
{
    m_texturePainting = false;

    // Reset first destination for next stroke
    m_cloneFirstDestTexX = -1;
    m_cloneFirstDestTexY = -1;

    // Step 6: Finalize and push undo command
    if (m_currentEditCommand && m_undoStack)
    {
        if (m_currentEditCommand->pixelCount() > 0)
        {
            m_currentEditCommand->finalize();
            m_undoStack->push(m_currentEditCommand);
            LOG_DEBUG("Texture edit command pushed: {} pixels", m_currentEditCommand->pixelCount());
        }
        else
        {
            delete m_currentEditCommand;
        }
        m_currentEditCommand = nullptr;
    }

    LOG_DEBUG("Texture painting ended");
}

void DiligentWidget::paintBrushAtPosition(int texX, int texY)
{
    if (!m_editBuffer || !m_editBuffer->isValid())
        return;

    const int radius = m_textureBrushRadius;
    const int radiusSq = radius * radius;

    // Step 5: Clone stamp mode - copy from source with offset
    if (m_cloneSourceSet)
    {
        // Calculate offset on first paint
        if (m_cloneFirstDestTexX < 0)
        {
            m_cloneFirstDestTexX = texX;
            m_cloneFirstDestTexY = texY;
        }

        // Calculate source-destination offset
        int offsetX = m_cloneSourceTexX - m_cloneFirstDestTexX;
        int offsetY = m_cloneSourceTexY - m_cloneFirstDestTexY;

        // Clone pixels from source to destination
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                // Circular brush check
                if (dx * dx + dy * dy > radiusSq)
                    continue;

                int destX = texX + dx;
                int destY = texY + dy;
                int srcX = destX + offsetX;
                int srcY = destY + offsetY;

                // Boundary check for both source and destination
                if (destX >= 0 && destX < m_editBuffer->width() &&
                    destY >= 0 && destY < m_editBuffer->height() &&
                    srcX >= 0 && srcX < m_editBuffer->width() &&
                    srcY >= 0 && srcY < m_editBuffer->height())
                {
                    // Read old pixel for undo
                    uint8_t oldR, oldG, oldB, oldA;
                    m_editBuffer->getPixel(destX, destY, oldR, oldG, oldB, oldA);

                    // Read from original texture (not the edited one)
                    uint8_t newR, newG, newB, newA;
                    m_editBuffer->getOriginalPixel(srcX, srcY, newR, newG, newB, newA);

                    // Only record if pixel actually changes
                    if (oldR != newR || oldG != newG || oldB != newB || oldA != newA)
                    {
                        // Step 6: Record pixel change for undo
                        if (m_currentEditCommand)
                        {
                            m_currentEditCommand->recordPixel(destX, destY,
                                oldR, oldG, oldB, oldA,
                                newR, newG, newB, newA);
                        }

                        m_editBuffer->setPixel(destX, destY, newR, newG, newB, newA);
                    }
                }
            }
        }
    }
    else
    {
        // No clone source - paint red for testing
        for (int dy = -radius; dy <= radius; ++dy)
        {
            for (int dx = -radius; dx <= radius; ++dx)
            {
                // Circular brush check
                if (dx * dx + dy * dy > radiusSq)
                    continue;

                int px = texX + dx;
                int py = texY + dy;

                // Boundary check
                if (px >= 0 && px < m_editBuffer->width() &&
                    py >= 0 && py < m_editBuffer->height())
                {
                    // Read old pixel for undo
                    uint8_t oldR, oldG, oldB, oldA;
                    m_editBuffer->getPixel(px, py, oldR, oldG, oldB, oldA);

                    const uint8_t newR = 255, newG = 0, newB = 0, newA = 255;

                    // Only record if pixel actually changes
                    if (oldR != newR || oldG != newG || oldB != newB || oldA != newA)
                    {
                        // Step 6: Record pixel change for undo
                        if (m_currentEditCommand)
                        {
                            m_currentEditCommand->recordPixel(px, py,
                                oldR, oldG, oldB, oldA,
                                newR, newG, newB, newA);
                        }

                        m_editBuffer->setPixel(px, py, newR, newG, newB, newA);
                    }
                }
            }
        }
    }
}

void DiligentWidget::setUndoStack(QUndoStack* stack)
{
    m_undoStack = stack;

    // Connect to undo stack index changes to sync texture after undo/redo
    if (m_undoStack)
    {
        connect(m_undoStack, &QUndoStack::indexChanged, this, [this]() {
            syncTextureToGPU();
        });
    }
}

void DiligentWidget::syncTextureToGPU()
{
    // Update GPU texture from edit buffer after undo/redo
    if (m_editBuffer && m_editBuffer->isDirty() && m_initialized && m_pContext)
    {
        m_meshRenderer.updateTextureFromBuffer(m_pContext, 0, *m_editBuffer);
        m_editBuffer->clearDirty();
        LOG_DEBUG("Synced texture to GPU after undo/redo");
    }
}

bool DiligentWidget::saveTexture(const QString& filePath)
{
    if (!m_editBuffer || !m_editBuffer->isValid())
    {
        MW_LOG_ERROR("Cannot save texture: no edit buffer");
        return false;
    }

    if (m_editBuffer->save(filePath))
    {
        m_editBuffer->setModified(false);
        LOG_INFO("Texture saved to: {}", filePath.toStdString());
        return true;
    }
    else
    {
        MW_LOG_ERROR("Failed to save texture to: {}", filePath.toStdString());
        return false;
    }
}

// M8: Mesh visibility control
void DiligentWidget::setMeshVisible(int index, bool visible)
{
    if (index >= 0 && index < static_cast<int>(m_meshInstances.size()))
    {
        if (m_meshInstances[index].visible != visible)
        {
            m_meshInstances[index].visible = visible;

            // M8: Sync visibility to FacePicker
            if (m_facePicker.isInitialized())
            {
                m_facePicker.setMeshVisible(static_cast<uint32_t>(index), visible);
            }

            LOG_DEBUG("Mesh {} visibility set to: {}", index, visible);
            update();  // Trigger repaint
        }
    }
}

bool DiligentWidget::isMeshVisible(int index) const
{
    if (index >= 0 && index < static_cast<int>(m_meshInstances.size()))
    {
        return m_meshInstances[index].visible;
    }
    return false;
}

int DiligentWidget::addMesh(std::shared_ptr<MeshData> mesh)
{
    if (!m_initialized || !mesh)
        return -1;

    MeshInstance instance;
    instance.mesh = mesh;
    instance.renderer = std::make_unique<MeshRenderer>();
    instance.visible = true;
    instance.id = static_cast<int>(m_meshInstances.size());

    // Initialize renderer
    if (!instance.renderer->initialize(m_pDevice, m_pSwapChain))
    {
        MW_LOG_ERROR("Failed to initialize mesh renderer for instance {}", instance.id);
        return -1;
    }

    // Apply current render mode settings
    instance.renderer->setWhiteModelMode(m_forceWhiteModel);
    instance.renderer->setShowWireframe(m_showWireframe);

    // Load mesh into renderer
    if (!instance.renderer->loadMesh(*mesh))
    {
        MW_LOG_ERROR("Failed to load mesh into renderer for instance {}", instance.id);
        return -1;
    }

    // M8: Add to FacePicker for multi-mesh picking
    if (m_facePicker.isInitialized())
    {
        m_facePicker.addMesh(*mesh, static_cast<uint32_t>(instance.id), instance.visible);
    }

    // M8: Add to SelectionRenderer for multi-mesh highlight
    if (m_selectionRenderer.isInitialized())
    {
        m_selectionRenderer.addMesh(*mesh, static_cast<uint32_t>(instance.id));
    }

    int index = instance.id;
    m_meshInstances.push_back(std::move(instance));

    LOG_INFO("Added mesh instance {}: {} vertices, {} faces",
             index, mesh->vertexCount(), mesh->faceCount());

    return index;
}

void DiligentWidget::clearAllMeshes()
{
    m_meshInstances.clear();
    m_currentMesh = nullptr;
    m_meshPtr.reset();
    m_editBuffer.reset();

    // Clear selection
    if (m_selectionSystem)
    {
        m_selectionSystem->clearSelection();
        m_selectionSystem->setFaceCount(0);
    }

    // M8: Clear face picker meshes
    if (m_facePicker.isInitialized())
    {
        m_facePicker.clearMeshes();
    }

    // M8: Clear selection renderer meshes
    if (m_selectionRenderer.isInitialized())
    {
        m_selectionRenderer.clearMeshes();
    }

    LOG_INFO("All meshes cleared");
    update();
}

void DiligentWidget::setWhiteModelMode(bool enabled)
{
    if (m_forceWhiteModel != enabled)
    {
        m_forceWhiteModel = enabled;

        // Apply to all mesh instances
        for (auto& instance : m_meshInstances)
        {
            if (instance.renderer)
            {
                instance.renderer->setWhiteModelMode(enabled);
            }
        }

        // Also apply to legacy renderer
        m_meshRenderer.setWhiteModelMode(enabled);

        LOG_DEBUG("White model mode set to: {}", enabled);
        update();
    }
}

void DiligentWidget::setShowWireframe(bool enabled)
{
    if (m_showWireframe != enabled)
    {
        m_showWireframe = enabled;

        // Apply to all mesh instances
        for (auto& instance : m_meshInstances)
        {
            if (instance.renderer)
            {
                instance.renderer->setShowWireframe(enabled);
            }
        }

        // Also apply to legacy renderer
        m_meshRenderer.setShowWireframe(enabled);

        LOG_DEBUG("Wireframe mode set to: {}", enabled);
        update();
    }
}

} // namespace MoldWing
