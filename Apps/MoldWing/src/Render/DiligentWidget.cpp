/*
 *  MoldWing - DiligentWidget Implementation
 *  S1.2/S1.5/S1.6: Qt + DiligentEngine + Rendering + Camera
 */

#include "DiligentWidget.hpp"

#include <QResizeEvent>
#include <QDebug>

// DiligentEngine - use loader for shared DLL
#include "Graphics/GraphicsEngineD3D11/interface/EngineFactoryD3D11.h"

#ifdef _WIN32
#include <Windows.h>
#endif

using namespace Diligent;

namespace MoldWing
{

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
}

DiligentWidget::~DiligentWidget()
{
    m_renderTimer->stop();

    // Release resources in correct order
    m_pSwapChain.Release();
    m_pContext.Release();
    m_pDevice.Release();
}

void DiligentWidget::initializeDiligent()
{
    if (m_initialized)
        return;

#ifdef _WIN32
    HWND hwnd = reinterpret_cast<HWND>(winId());

    // Load D3D11 engine DLL and get factory
    auto GetEngineFactoryD3D11 = LoadGraphicsEngineD3D11();
    if (!GetEngineFactoryD3D11)
    {
        qCritical() << "Failed to load GraphicsEngineD3D11 DLL!";
        return;
    }

    auto* pFactoryD3D11 = GetEngineFactoryD3D11();
    if (!pFactoryD3D11)
    {
        qCritical() << "Failed to get D3D11 engine factory!";
        return;
    }

    EngineD3D11CreateInfo engineCI;
#ifdef _DEBUG
    engineCI.EnableValidation = true;
#endif

    pFactoryD3D11->CreateDeviceAndContextsD3D11(engineCI, &m_pDevice, &m_pContext);

    if (!m_pDevice)
    {
        qCritical() << "Failed to create D3D11 render device!";
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
        qCritical() << "Failed to create swap chain!";
        return;
    }

    // Initialize mesh renderer
    if (!m_meshRenderer.initialize(m_pDevice, m_pSwapChain))
    {
        qCritical() << "Failed to initialize mesh renderer!";
        return;
    }

    // Set camera aspect ratio
    m_camera.setAspectRatio(static_cast<float>(width()) / static_cast<float>(height()));

    m_initialized = true;
    emit initialized();


#else
    qCritical() << "Non-Windows platforms not yet supported";
#endif
}

bool DiligentWidget::loadMesh(const MeshData& mesh)
{
    if (!m_initialized)
        return false;

    if (!m_meshRenderer.loadMesh(mesh))
        return false;

    // Fit camera to mesh bounds
    const auto& b = mesh.bounds;
    m_camera.fitToModel(b.min[0], b.min[1], b.min[2], b.max[0], b.max[1], b.max[2]);

    return true;
}

void DiligentWidget::render()
{
    if (!m_initialized || !m_pSwapChain)
        return;

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
    m_pContext->ClearDepthStencil(pDSV, CLEAR_DEPTH_FLAG, 1.0f, 0, RESOURCE_STATE_TRANSITION_MODE_TRANSITION);  // Standard depth: clear to 1

    // Render mesh if loaded
    if (m_meshRenderer.hasMesh())
    {
        m_meshRenderer.render(m_pContext, m_camera);
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
        }
    }
}

void DiligentWidget::mousePressEvent(QMouseEvent* event)
{
    m_lastMousePos = event->pos();

    if (event->button() == Qt::LeftButton)
    {
        m_rotating = true;
    }
    else if (event->button() == Qt::MiddleButton)
    {
        m_panning = true;
    }
    else if (event->button() == Qt::RightButton)
    {
        m_panning = true;
    }
}

void DiligentWidget::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
    {
        m_rotating = false;
    }
    else if (event->button() == Qt::MiddleButton)
    {
        m_panning = false;
    }
    else if (event->button() == Qt::RightButton)
    {
        m_panning = false;
    }
}

void DiligentWidget::mouseMoveEvent(QMouseEvent* event)
{
    QPoint delta = event->pos() - m_lastMousePos;
    m_lastMousePos = event->pos();

    if (m_rotating)
    {
        // Rotate camera (sensitivity: 0.5 degrees per pixel)
        m_camera.rotate(delta.x() * 0.5f, delta.y() * 0.5f);
    }
    else if (m_panning)
    {
        // Pan camera (scale by window size for consistent feel)
        float scaleX = delta.x() / static_cast<float>(width());
        float scaleY = delta.y() / static_cast<float>(height());
        m_camera.pan(scaleX * 5.0f, -scaleY * 5.0f);
    }
}

void DiligentWidget::wheelEvent(QWheelEvent* event)
{
    // Zoom camera (positive delta = zoom in)
    float delta = event->angleDelta().y() / 120.0f;
    m_camera.zoom(delta);
}

} // namespace MoldWing
