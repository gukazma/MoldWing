/*
 *  MoldWing - Main Window
 *  S1.7/S1.8: With QUndoStack and menu system
 */

#pragma once

#include <QMainWindow>
#include <QUndoStack>

class QMenu;
class QAction;
class QToolBar;

namespace MoldWing
{

class DiligentWidget;
struct MeshData;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override = default;

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

    // Central widget
    DiligentWidget* m_viewport3D = nullptr;

    // Undo system
    QUndoStack* m_undoStack = nullptr;

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
