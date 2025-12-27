/*
 *  MoldWing - DiligentWidget Implementation
 *  Enhanced with Blender-style camera controls
 */

#include "DiligentWidget.hpp"
#include "Core/Logger.hpp"
#include "Core/RayIntersection.hpp"

#include <QResizeEvent>
#include <QContextMenuEvent>
#include <QDebug>

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

    // Set camera aspect ratio
    m_camera.setAspectRatio(static_cast<float>(width()) / static_cast<float>(height()));

    m_initialized = true;
    LOG_INFO("DiligentEngine initialized successfully");
    emit initialized();

#else
    MW_LOG_ERROR("Non-Windows platforms not yet supported");
#endif
}

bool DiligentWidget::loadMesh(const MeshData& mesh)
{
    if (!m_initialized)
        return false;

    if (!m_meshRenderer.loadMesh(mesh))
        return false;

    // Store mesh pointer for ray picking
    m_currentMesh = &mesh;

    // Load mesh into face picker
    if (m_facePicker.isInitialized())
    {
        m_facePicker.loadMesh(mesh);
    }

    // Load mesh into selection renderer
    if (m_selectionRenderer.isInitialized())
    {
        m_selectionRenderer.loadMesh(mesh);
    }

    // Update selection system face count
    m_selectionSystem->setFaceCount(static_cast<uint32_t>(mesh.faceCount()));

    // Fit camera to mesh bounds
    const auto& b = mesh.bounds;
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

    // Render mesh if loaded
    if (m_meshRenderer.hasMesh())
    {
        m_meshRenderer.render(m_pContext, m_camera);
    }

    // Render selection highlight
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

    // Selection mode: LMB starts box selection
    if (m_interactionMode == InteractionMode::Selection)
    {
        if (event->button() == Qt::LeftButton)
        {
            beginBoxSelect(event->pos());
            return;
        }
    }

    // Camera mode controls:
    // LMB = rotate (use current pivot)
    // MMB = pan (recalculate pivot after release)
    // RMB = context menu (handled by contextMenuEvent)

    if (event->button() == Qt::LeftButton)
    {
        m_rotating = true;
        m_camera.beginRotate();
    }
    else if (event->button() == Qt::MiddleButton)
    {
        m_panning = true;
        m_camera.beginPan();
    }
    // RMB is reserved for context menu
}

void DiligentWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        // Handle box selection end
        if (m_boxSelecting)
        {
            endBoxSelect();
            return;
        }

        if (m_rotating)
        {
            m_rotating = false;
            m_camera.endRotate();
        }
    }
    else if (event->button() == Qt::MiddleButton)
    {
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

    // Handle box selection drag
    if (m_boxSelecting)
    {
        updateBoxSelect(event->pos());
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

        // Cancel any ongoing box selection when switching modes
        if (m_boxSelecting)
        {
            m_boxSelecting = false;
        }

        LOG_DEBUG("Interaction mode changed to {}",
                  mode == InteractionMode::Camera ? "Camera" : "Selection");
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
    if (m_shiftHeld && m_ctrlHeld)
        return SelectionOp::Toggle;
    else if (m_shiftHeld)
        return SelectionOp::Add;
    else if (m_ctrlHeld)
        return SelectionOp::Subtract;
    else
        return SelectionOp::Replace;
}

} // namespace MoldWing
