/*
 *  MoldWing - Main Window Implementation
 *  S1.7/S1.8: With QUndoStack, menus, DockWidgets, and layout persistence
 */

#include "MainWindow.hpp"
#include "Render/DiligentWidget.hpp"
#include "Core/MeshData.hpp"
#include "Core/Logger.hpp"
#include "IO/MeshLoader.hpp"
#include "Selection/SelectionSystem.hpp"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeySequence>
#include <QDockWidget>
#include <QUndoView>
#include <QListWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QApplication>

namespace MoldWing
{

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    LOG_INFO("MainWindow 构造开始");

    setWindowTitle(tr("MoldWing - Oblique Photography 3D Model Editor"));
    setMinimumSize(1280, 720);

    // Create undo stack
    m_undoStack = new QUndoStack(this);

    // Create DiligentEngine viewport as central widget
    m_viewport3D = new DiligentWidget(this);
    setCentralWidget(m_viewport3D);

    // Connect viewport to undo stack
    m_viewport3D->setUndoStack(m_undoStack);

    // Connect selection changes to update property panel
    connect(m_viewport3D->selectionSystem(), &SelectionSystem::selectionChanged,
            this, &MainWindow::onSelectionChanged);

    // Setup UI
    setupMenus();
    setupToolBar();
    setupStatusBar();
    setupDockWidgets();

    // Restore window state
    restoreWindowState();

    LOG_INFO("MainWindow 构造完成");
}

MainWindow::~MainWindow()
{
    LOG_INFO("MainWindow 析构开始");
    saveWindowState();
    LOG_INFO("MainWindow 析构完成");
}

void MainWindow::setupMenus()
{
    // File menu
    m_fileMenu = menuBar()->addMenu(tr("&File"));

    m_openAction = m_fileMenu->addAction(tr("&Open..."));
    m_openAction->setShortcut(QKeySequence::Open);
    connect(m_openAction, &QAction::triggered, this, &MainWindow::onOpenFile);

    m_saveAction = m_fileMenu->addAction(tr("&Save"));
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveFile);
    m_saveAction->setEnabled(false);

    m_fileMenu->addSeparator();

    m_exitAction = m_fileMenu->addAction(tr("E&xit"));
    m_exitAction->setShortcut(QKeySequence::Quit);
    connect(m_exitAction, &QAction::triggered, this, &QMainWindow::close);

    // Edit menu
    m_editMenu = menuBar()->addMenu(tr("&Edit"));

    m_undoAction = m_undoStack->createUndoAction(this, tr("&Undo"));
    m_undoAction->setShortcut(QKeySequence::Undo);
    m_editMenu->addAction(m_undoAction);

    m_redoAction = m_undoStack->createRedoAction(this, tr("&Redo"));
    m_redoAction->setShortcut(QKeySequence::Redo);
    m_editMenu->addAction(m_redoAction);

    m_editMenu->addSeparator();

    // Selection actions
    m_selectAllAction = m_editMenu->addAction(tr("Select &All"));
    m_selectAllAction->setShortcut(QKeySequence::SelectAll);
    connect(m_selectAllAction, &QAction::triggered, this, &MainWindow::onSelectAll);

    m_deselectAction = m_editMenu->addAction(tr("&Deselect"));
    m_deselectAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_D));
    connect(m_deselectAction, &QAction::triggered, this, &MainWindow::onDeselect);

    m_invertSelectionAction = m_editMenu->addAction(tr("&Invert Selection"));
    m_invertSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_I));
    connect(m_invertSelectionAction, &QAction::triggered, this, &MainWindow::onInvertSelection);

    // View menu
    m_viewMenu = menuBar()->addMenu(tr("&View"));

    m_resetViewAction = m_viewMenu->addAction(tr("&Reset View"));
    m_resetViewAction->setShortcut(QKeySequence(Qt::Key_Home));
    connect(m_resetViewAction, &QAction::triggered, this, &MainWindow::onResetView);
}

void MainWindow::setupToolBar()
{
    QToolBar* mainToolBar = addToolBar(tr("Main"));
    mainToolBar->setMovable(false);

    mainToolBar->addAction(m_openAction);
    mainToolBar->addAction(m_saveAction);
    mainToolBar->addSeparator();
    mainToolBar->addAction(m_undoAction);
    mainToolBar->addAction(m_redoAction);
}

void MainWindow::setupStatusBar()
{
    statusBar()->showMessage(tr("Ready"));
}

void MainWindow::setupDockWidgets()
{
    LOG_DEBUG("设置 DockWidgets");

    // =========================================================================
    // Left side: Tool Dock
    // =========================================================================
    m_toolDock = new QDockWidget(tr("Tools"), this);
    m_toolDock->setObjectName("ToolDock");
    m_toolDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_toolList = new QListWidget(m_toolDock);
    m_toolList->addItem(tr("🔲 Box Select"));
    m_toolList->addItem(tr("🖌️ Brush Select"));
    m_toolList->addItem(tr("⭕ Lasso Select"));
    m_toolList->addItem(tr("🔗 Connected Select"));
    m_toolList->addItem(tr("🎨 Paint Brush"));
    m_toolList->addItem(tr("🧹 Eraser"));
    m_toolList->addItem(tr("📍 Clone Stamp"));
    m_toolList->addItem(tr("🩹 Healing Brush"));
    m_toolList->setMinimumWidth(150);
    m_toolDock->setWidget(m_toolList);

    // Connect tool selection
    connect(m_toolList, &QListWidget::currentRowChanged, this, &MainWindow::onToolSelected);

    addDockWidget(Qt::LeftDockWidgetArea, m_toolDock);

    // =========================================================================
    // Right side: Property Dock
    // =========================================================================
    m_propertyDock = new QDockWidget(tr("Properties"), this);
    m_propertyDock->setObjectName("PropertyDock");
    m_propertyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    QWidget* propertyWidget = new QWidget(m_propertyDock);
    QVBoxLayout* propertyLayout = new QVBoxLayout(propertyWidget);

    m_propertyLabel = new QLabel(tr("No selection"), propertyWidget);
    m_propertyLabel->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    m_propertyLabel->setWordWrap(true);
    propertyLayout->addWidget(m_propertyLabel);
    propertyLayout->addStretch();

    propertyWidget->setMinimumWidth(200);
    m_propertyDock->setWidget(propertyWidget);

    addDockWidget(Qt::RightDockWidgetArea, m_propertyDock);

    // =========================================================================
    // Right side: History Dock (tabified with Property)
    // =========================================================================
    m_historyDock = new QDockWidget(tr("History"), this);
    m_historyDock->setObjectName("HistoryDock");
    m_historyDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_undoView = new QUndoView(m_undoStack, m_historyDock);
    m_undoView->setEmptyLabel(tr("<empty>"));
    m_historyDock->setWidget(m_undoView);

    addDockWidget(Qt::RightDockWidgetArea, m_historyDock);

    // Tabify history dock with property dock
    tabifyDockWidget(m_propertyDock, m_historyDock);

    // Make property dock the visible tab by default
    m_propertyDock->raise();

    // =========================================================================
    // Add dock toggle actions to View menu
    // =========================================================================
    m_viewMenu->addSeparator();
    m_viewMenu->addAction(m_toolDock->toggleViewAction());
    m_viewMenu->addAction(m_propertyDock->toggleViewAction());
    m_viewMenu->addAction(m_historyDock->toggleViewAction());

    LOG_DEBUG("DockWidgets 设置完成");
}

void MainWindow::saveWindowState()
{
    LOG_DEBUG("保存窗口状态");

    QSettings settings(QApplication::organizationName(), QApplication::applicationName());
    settings.setValue("geometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::restoreWindowState()
{
    LOG_DEBUG("恢复窗口状态");

    QSettings settings(QApplication::organizationName(), QApplication::applicationName());

    if (settings.contains("geometry")) {
        restoreGeometry(settings.value("geometry").toByteArray());
    }

    if (settings.contains("windowState")) {
        restoreState(settings.value("windowState").toByteArray());
    }
}

void MainWindow::onOpenFile()
{
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Open 3D Model"),
        QString(),
        tr("3D Models (*.obj *.fbx *.gltf *.glb *.dae *.3ds);;OBJ Files (*.obj);;All Files (*)")
    );

    if (filePath.isEmpty())
        return;

    LOG_INFO("打开文件: {}", filePath.toStdString());
    statusBar()->showMessage(tr("Loading %1...").arg(filePath));

    MeshLoader loader;
    m_currentMesh = loader.load(filePath);

    if (!m_currentMesh)
    {
        MW_LOG_ERROR("加载模型失败: {}", loader.lastError().toStdString());
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load model:\n%1").arg(loader.lastError()));
        statusBar()->showMessage(tr("Failed to load model"));
        return;
    }

    LOG_INFO("模型加载成功: {} 顶点, {} 面",
             m_currentMesh->vertexCount(), m_currentMesh->faceCount());

    // Load mesh into renderer
    if (!m_viewport3D->loadMesh(*m_currentMesh))
    {
        MW_LOG_ERROR("加载网格到渲染器失败");
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load mesh into renderer"));
        statusBar()->showMessage(tr("Failed to load mesh"));
        return;
    }

    // Update window title
    QFileInfo fileInfo(filePath);
    setWindowTitle(tr("MoldWing - %1").arg(fileInfo.fileName()));

    // Update property panel
    m_propertyLabel->setText(tr("Model: %1\nVertices: %2\nFaces: %3")
        .arg(fileInfo.fileName())
        .arg(m_currentMesh->vertexCount())
        .arg(m_currentMesh->faceCount()));

    // Enable save action
    m_saveAction->setEnabled(true);

    // Clear undo stack for new file
    m_undoStack->clear();

    statusBar()->showMessage(tr("Loaded: %1 vertices, %2 faces")
        .arg(m_currentMesh->vertexCount())
        .arg(m_currentMesh->faceCount()));
}

void MainWindow::onSaveFile()
{
    if (!m_currentMesh)
        return;

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save 3D Model"),
        QString(),
        tr("OBJ Files (*.obj);;All Files (*)")
    );

    if (filePath.isEmpty())
        return;

    LOG_INFO("保存文件: {}", filePath.toStdString());

    // TODO: Implement mesh saving
    QMessageBox::information(this, tr("Info"),
        tr("Save functionality will be implemented in a future update."));
}

void MainWindow::onResetView()
{
    LOG_DEBUG("重置视图");

    if (m_currentMesh && m_viewport3D)
    {
        const auto& b = m_currentMesh->bounds;
        m_viewport3D->camera().fitToModel(
            b.min[0], b.min[1], b.min[2],
            b.max[0], b.max[1], b.max[2]);
    }
    else
    {
        m_viewport3D->camera().reset();
    }
}

void MainWindow::onToolSelected(int index)
{
    LOG_DEBUG("工具选择: {}", index);

    // First 4 items are selection tools (0-3)
    if (index >= 0 && index <= 3)
    {
        m_viewport3D->setInteractionMode(DiligentWidget::InteractionMode::Selection);
        statusBar()->showMessage(tr("Selection mode - drag to select faces"));
    }
    else
    {
        m_viewport3D->setInteractionMode(DiligentWidget::InteractionMode::Camera);
        statusBar()->showMessage(tr("Tool selected (not yet implemented)"));
    }
}

void MainWindow::onSelectAll()
{
    if (m_viewport3D && m_viewport3D->selectionSystem())
    {
        m_viewport3D->selectionSystem()->selectAll();
        LOG_DEBUG("全选");
    }
}

void MainWindow::onDeselect()
{
    if (m_viewport3D && m_viewport3D->selectionSystem())
    {
        m_viewport3D->selectionSystem()->clearSelection();
        LOG_DEBUG("取消选择");
    }
}

void MainWindow::onInvertSelection()
{
    if (m_viewport3D && m_viewport3D->selectionSystem())
    {
        m_viewport3D->selectionSystem()->invertSelection();
        LOG_DEBUG("反选");
    }
}

void MainWindow::onSelectionChanged()
{
    if (!m_viewport3D || !m_viewport3D->selectionSystem())
        return;

    size_t count = m_viewport3D->selectionSystem()->selectionCount();

    if (count == 0)
    {
        if (m_currentMesh)
        {
            m_propertyLabel->setText(tr("Model: %1\nVertices: %2\nFaces: %3\n\nNo selection")
                .arg(windowTitle().section(" - ", 1))
                .arg(m_currentMesh->vertexCount())
                .arg(m_currentMesh->faceCount()));
        }
        else
        {
            m_propertyLabel->setText(tr("No selection"));
        }
        statusBar()->showMessage(tr("Selection cleared"));
    }
    else
    {
        if (m_currentMesh)
        {
            m_propertyLabel->setText(tr("Model: %1\nVertices: %2\nFaces: %3\n\nSelected: %4 faces")
                .arg(windowTitle().section(" - ", 1))
                .arg(m_currentMesh->vertexCount())
                .arg(m_currentMesh->faceCount())
                .arg(count));
        }
        else
        {
            m_propertyLabel->setText(tr("Selected: %1 faces").arg(count));
        }
        statusBar()->showMessage(tr("%1 faces selected").arg(count));
    }
}

} // namespace MoldWing
