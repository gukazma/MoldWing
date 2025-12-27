/*
 *  MoldWing - DiligentWidget
 *  Qt widget that hosts DiligentEngine rendering
 *  Enhanced with Blender-style camera controls and selection
 */

#pragma once

#include <QWidget>
#include <QTimer>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QKeyEvent>
#include <QMenu>
#include <QElapsedTimer>
#include <QUndoStack>

// DiligentEngine headers
#include "Graphics/GraphicsEngine/interface/RenderDevice.h"
#include "Graphics/GraphicsEngine/interface/DeviceContext.h"
#include "Graphics/GraphicsEngine/interface/SwapChain.h"
#include "Common/interface/RefCntAutoPtr.hpp"

#include "OrbitCamera.hpp"
#include "MeshRenderer.hpp"
#include "PivotIndicator.hpp"
#include "SelectionBoxRenderer.hpp"
#include "Core/MeshData.hpp"
#include "Selection/SelectionSystem.hpp"
#include "Selection/FacePicker.hpp"
#include "Selection/SelectionRenderer.hpp"

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

    // Selection system access
    SelectionSystem* selectionSystem() { return m_selectionSystem; }
    const SelectionSystem* selectionSystem() const { return m_selectionSystem; }

    // Set undo stack for selection commands
    void setUndoStack(QUndoStack* stack) { m_undoStack = stack; }

    // Interaction mode
    enum class InteractionMode
    {
        Camera,     // Camera manipulation (default)
        Selection   // Selection mode (box select)
    };

    InteractionMode interactionMode() const { return m_interactionMode; }
    void setInteractionMode(InteractionMode mode);

signals:
    void initialized();
    void interactionModeChanged(InteractionMode mode);

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

    // Selection helpers
    void beginBoxSelect(const QPoint& pos);
    void updateBoxSelect(const QPoint& pos);
    void endBoxSelect();
    SelectionOp getSelectionOp() const;

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

    // Selection system
    SelectionSystem* m_selectionSystem = nullptr;
    FacePicker m_facePicker;
    SelectionRenderer m_selectionRenderer;
    QUndoStack* m_undoStack = nullptr;

    // Box selection
    SelectionBoxRenderer m_selectionBoxRenderer;
    QPoint m_boxSelectStart;
    QPoint m_boxSelectCurrent;
    bool m_boxSelecting = false;

    // Interaction mode
    InteractionMode m_interactionMode = InteractionMode::Camera;

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
