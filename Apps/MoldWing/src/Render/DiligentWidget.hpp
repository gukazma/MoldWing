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
#include "BrushCursorRenderer.hpp"
#include "LassoRenderer.hpp"
#include "Core/MeshData.hpp"
#include "Selection/SelectionSystem.hpp"
#include "Selection/FacePicker.hpp"
#include "Selection/SelectionRenderer.hpp"
#include "Texture/ScreenToTextureMapper.hpp"
#include "Texture/TextureEditBuffer.hpp"
#include "Texture/TextureEditCommand.hpp"

#include <QPolygonF>

#include <memory>
#include <unordered_set>

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
    bool loadMesh(std::shared_ptr<MeshData> mesh);

    // Get camera for external access
    OrbitCamera& camera() { return m_camera; }
    const OrbitCamera& camera() const { return m_camera; }

    // Selection system access
    SelectionSystem* selectionSystem() { return m_selectionSystem; }
    const SelectionSystem* selectionSystem() const { return m_selectionSystem; }

    // Set undo stack for selection commands
    void setUndoStack(QUndoStack* stack);

    // Update GPU texture from edit buffer (called after undo/redo)
    void syncTextureToGPU();

    // Save edited texture to file
    bool saveTexture(const QString& filePath);

    // Interaction mode
    enum class InteractionMode
    {
        Camera,       // Camera manipulation (default)
        Selection,    // Selection mode (box select)
        TextureEdit   // Texture editing mode (clone stamp, etc.)
    };

    InteractionMode interactionMode() const { return m_interactionMode; }
    void setInteractionMode(InteractionMode mode);

    // Brush selection settings
    int brushRadius() const { return m_brushRadius; }
    void setBrushRadius(int radius);

    // M5: Link selection settings
    float linkAngleThreshold() const { return m_linkAngleThreshold; }
    void setLinkAngleThreshold(float angle);

    // Brush radius limits
    static constexpr int MIN_BRUSH_RADIUS = 5;
    static constexpr int MAX_BRUSH_RADIUS = 200;
    static constexpr int DEFAULT_BRUSH_RADIUS = 30;

    // Link selection angle limits
    static constexpr float MIN_ANGLE_THRESHOLD = 0.0f;
    static constexpr float MAX_ANGLE_THRESHOLD = 180.0f;
    static constexpr float DEFAULT_ANGLE_THRESHOLD = 30.0f;

signals:
    void initialized();
    void interactionModeChanged(InteractionMode mode);
    void brushRadiusChanged(int radius);
    void linkAngleThresholdChanged(float angle);

    // Step 1: 纹理坐标拾取信号
    void textureCoordPicked(float u, float v, int texX, int texY, uint32_t faceId);

    // Step 4: Clone source set signal
    void cloneSourceSet(int texX, int texY);

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

    // Brush selection helpers
    void beginBrushSelect(const QPoint& pos);
    void updateBrushSelect(const QPoint& pos);
    void endBrushSelect();

    // Lasso selection helpers
    void beginLassoSelect(const QPoint& pos);
    void updateLassoSelect(const QPoint& pos);
    void endLassoSelect();

    // M5: Link selection helper
    void performLinkSelect(const QPoint& pos);

    // Step 3: Texture painting helpers
    void beginTexturePaint(const QPoint& pos);
    void updateTexturePaint(const QPoint& pos);
    void endTexturePaint();
    void paintBrushAtPosition(int texX, int texY);

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

    // Brush selection
    BrushCursorRenderer m_brushCursorRenderer;
    QPoint m_brushPosition;           // Current brush position
    int m_brushRadius = DEFAULT_BRUSH_RADIUS;
    bool m_brushSelecting = false;    // True when dragging with brush
    std::unordered_set<uint32_t> m_brushSelectAccumulated;  // Accumulated selection during drag

    // Lasso selection
    LassoRenderer m_lassoRenderer;
    QPolygonF m_lassoPath;            // Lasso path in screen coordinates
    bool m_lassoSelecting = false;    // True when drawing lasso

    // M5: Link selection
    float m_linkAngleThreshold = DEFAULT_ANGLE_THRESHOLD;

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

    // Step 1: Screen to texture mapper
    ScreenToTextureMapper m_textureMapper;
    std::shared_ptr<MeshData> m_meshPtr;  // Shared ptr for mapper

    // Step 2: Texture edit buffer for real-time editing
    std::unique_ptr<TextureEditBuffer> m_editBuffer;

    // Step 3: Texture painting state
    bool m_texturePainting = false;    // True when dragging to paint
    int m_textureBrushRadius = 10;     // Texture space brush radius

    // Step 4: Clone stamp state
    bool m_cloneSourceSet = false;     // True if clone source is set
    int m_cloneSourceTexX = 0;         // Clone source texture X
    int m_cloneSourceTexY = 0;         // Clone source texture Y
    int m_cloneFirstDestTexX = -1;     // First destination X (for offset calculation)
    int m_cloneFirstDestTexY = -1;     // First destination Y

    // Step 6: Current texture edit command for undo
    TextureEditCommand* m_currentEditCommand = nullptr;

    // State
    bool m_initialized = false;
};

} // namespace MoldWing
