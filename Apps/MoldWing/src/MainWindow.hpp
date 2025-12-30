/*
 *  MoldWing - Main Window
 *  S1.7/S1.8: With QUndoStack, menu system, and DockWidgets
 */

#pragma once

#include <QMainWindow>
#include <QUndoStack>
#include <QSettings>

class QMenu;
class QAction;
class QToolBar;
class QDockWidget;
class QUndoView;
class QListWidget;
class QLabel;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QGroupBox;

namespace MoldWing
{

class DiligentWidget;
struct MeshData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    // Get undo stack for command execution
    QUndoStack* undoStack() { return m_undoStack; }

private slots:
    void onOpenFile();
    void onSaveFile();
    void onResetView();
    void onToolSelected(int index);
    void onSelectAll();
    void onDeselect();
    void onInvertSelection();
    void onSelectionChanged();
    void onBrushRadiusChanged(int radius);
    void onLinkAngleThresholdChanged(double angle);
    void onGrowSelection();
    void onShrinkSelection();

    // M7: Texture editing slots
    void onEnterTextureEditMode();
    void onExitTextureEditMode();
    void onSaveTexture();

private:
    void setupMenus();
    void setupToolBar();
    void setupStatusBar();
    void setupDockWidgets();

    void saveWindowState();
    void restoreWindowState();

    // Central widget
    DiligentWidget* m_viewport3D = nullptr;

    // Undo system
    QUndoStack* m_undoStack = nullptr;
    QUndoView* m_undoView = nullptr;

    // Dock widgets
    QDockWidget* m_toolDock = nullptr;
    QDockWidget* m_propertyDock = nullptr;
    QDockWidget* m_historyDock = nullptr;

    // Dock widget content
    QListWidget* m_toolList = nullptr;
    QLabel* m_propertyLabel = nullptr;

    // Brush settings widgets
    QGroupBox* m_brushSettingsGroup = nullptr;
    QSlider* m_brushRadiusSlider = nullptr;
    QSpinBox* m_brushRadiusSpinBox = nullptr;

    // M5: Link settings widgets
    QGroupBox* m_linkSettingsGroup = nullptr;
    QSlider* m_linkAngleSlider = nullptr;
    QDoubleSpinBox* m_linkAngleSpinBox = nullptr;

    // Menus
    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_viewMenu = nullptr;
    QMenu* m_selectMenu = nullptr;  // New selection menu
    QMenu* m_textureMenu = nullptr; // M7: Texture editing menu

    // Actions
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_selectAllAction = nullptr;
    QAction* m_deselectAction = nullptr;
    QAction* m_invertSelectionAction = nullptr;
    QAction* m_growSelectionAction = nullptr;
    QAction* m_shrinkSelectionAction = nullptr;
    QAction* m_resetViewAction = nullptr;

    // M7: Texture editing actions
    QAction* m_enterTextureEditAction = nullptr;
    QAction* m_exitTextureEditAction = nullptr;
    QAction* m_saveTextureAction = nullptr;

    // Current mesh data
    std::shared_ptr<MeshData> m_currentMesh;
};

} // namespace MoldWing
