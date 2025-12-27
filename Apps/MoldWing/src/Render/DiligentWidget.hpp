/*
 *  MoldWing - DiligentWidget
 *  Qt widget that hosts DiligentEngine rendering
 *  Enhanced with Blender-style camera controls
 */

#pragma once

#include <QWidget>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QElapsedTimer>

// DiligentEngine headers
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Common/interface/RefCntAutoPtr.hpp"

#include "OrbitCamera.hpp"
#include "MeshRenderer.hpp"
#include "PivotIndicator.hpp"
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
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    QPaintEngine* paintEngine() const override { return nullptr; }

private slots:
    void onContextMenuViewFront();
    void onContextMenuViewBack();
    void onContextMenuViewLeft();
    void onContextMenuViewRight();
    void onContextMenuViewTop();
    void onContextMenuViewBottom();
    void onContextMenuViewIsometric();
    void onContextMenuResetView();
    void onContextMenuToggleOrtho();

private:
    void initializeDiligent();
    void render();
    void setupContextMenu();

    // Get current rotation constraint based on modifier keys
    RotationConstraint getRotationConstraint() const;

    // DiligentEngine objects
    Diligent::RefCntAutoPtr<Diligent::IRenderDevice>  m_pDevice;
    Diligent::RefCntAutoPtr<Diligent::IDeviceContext> m_pContext;
    Diligent::RefCntAutoPtr<Diligent::ISwapChain>     m_pSwapChain;

    // Render timer
    QTimer* m_renderTimer = nullptr;
    QElapsedTimer m_frameTimer;

    // Rendering
    MeshRenderer m_meshRenderer;
    PivotIndicator m_pivotIndicator;
    OrbitCamera m_camera;

    // Mouse tracking
    QPoint m_lastMousePos;
    bool m_rotating = false;
    bool m_panning = false;

    // Modifier keys state
    bool m_shiftHeld = false;
    bool m_ctrlHeld = false;
    bool m_altHeld = false;

    // Context menu
    QMenu* m_contextMenu = nullptr;

    // Current mesh for ray picking (not owned)
    const MeshData* m_currentMesh = nullptr;

    // State
    bool m_initialized = false;
};

} // namespace MoldWing
