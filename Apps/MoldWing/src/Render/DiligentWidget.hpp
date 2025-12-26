/*
 *  MoldWing - DiligentWidget
 *  Qt widget that hosts DiligentEngine rendering
 *  S1.2/S1.5/S1.6: Qt + DiligentEngine + Rendering + Camera
 */

#pragma once

#include <QWidget>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>

// DiligentEngine headers
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Common/interface/RefCntAutoPtr.hpp"

#include "OrbitCamera.hpp"
#include "MeshRenderer.hpp"
#include "Core/MeshData.hpp"

#include <memory>

namespace MoldWing
{

class DiligentWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DiligentWidget(QWidget* parent = nullptr);
    ~DiligentWidget() override;

    bool isInitialized() const { return m_initialized; }

    // Load mesh for rendering
    bool loadMesh(const MeshData& mesh);

    // Get camera for external access
    OrbitCamera& camera() { return m_camera; }
    const OrbitCamera& camera() const { return m_camera; }

signals:
    void initialized();

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    QPaintEngine* paintEngine() const override { return nullptr; }

private:
    void initializeDiligent();
    void render();

    // DiligentEngine objects
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>     m_pSwapChain;

    // Render timer
    QTimer* m_renderTimer = nullptr;

    // Rendering
    MeshRenderer m_meshRenderer;
    OrbitCamera m_camera;

    // Mouse tracking
    QPoint m_lastMousePos;
    bool m_rotating = false;
    bool m_panning = false;

    // State
    bool m_initialized = false;
};

} // namespace MoldWing
