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

    // Menus
    QMenu* m_fileMenu = nullptr;
    QMenu* m_editMenu = nullptr;
    QMenu* m_viewMenu = nullptr;

    // Actions
    QAction* m_openAction = nullptr;
    QAction* m_saveAction = nullptr;
    QAction* m_exitAction = nullptr;
    QAction* m_undoAction = nullptr;
    QAction* m_redoAction = nullptr;
    QAction* m_resetViewAction = nullptr;

    // Current mesh data
    std::shared_ptr<MeshData> m_currentMesh;
};

} // namespace MoldWing
