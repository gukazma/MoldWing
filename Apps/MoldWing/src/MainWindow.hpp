/*
 *  MoldWing - Main Window
 *  S1.7/S1.8: With QUndoStack, menu system, and DockWidgets
 */

#pragma once

#include <QMainWindow>
#include <QUndoStack>
#include <QSettings>
#include <QFutureWatcher>
#include <vector>

class QMenu;
class QProgressDialog;
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
class QTreeWidget;
class QTreeWidgetItem;

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
    void onExportFile();  // M7.5: Export OBJ with textures
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

    // 渲染模式切换
    void onToggleWhiteModel(bool checked);
    void onToggleWireframe(bool checked);

    // 异步加载完成回调
    void onModelLoadFinished();

    // M8: 图层可见性切换
    void onLayerVisibilityChanged(QTreeWidgetItem* item, int column);

    // M8.1.1: 文件夹批量导入
    void onImportFolder();
    void loadNextPendingFile();

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
    QDockWidget* m_layerDock = nullptr;  // M8: Layer tree dock

    // M8: Layer tree widget
    QTreeWidget* m_layerTree = nullptr;

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
    QAction* m_importFolderAction = nullptr;  // M8.1.1: 文件夹批量导入
    QAction* m_saveAction = nullptr;
    QAction* m_exportAction = nullptr;  // M7.5: Export OBJ with textures
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

    // 渲染模式 actions
    QAction* m_toggleWhiteModelAction = nullptr;
    QAction* m_toggleWireframeAction = nullptr;

    // Current mesh data
    std::shared_ptr<MeshData> m_currentMesh;

    // M8: Multi-model support
    std::vector<std::shared_ptr<MeshData>> m_meshList;

    // 异步加载状态
    QFutureWatcher<std::shared_ptr<MeshData>> m_loadWatcher;
    QProgressDialog* m_loadProgressDialog = nullptr;
    QString m_loadingFilePath;

    // M8.1.1: 批量导入状态
    QStringList m_pendingFiles;      // 待加载的文件列表
    int m_loadedCount = 0;           // 已加载计数
    int m_totalFilesToLoad = 0;      // 总文件数
    bool m_batchLoadMode = false;    // 批量加载模式标记
};

} // namespace MoldWing
