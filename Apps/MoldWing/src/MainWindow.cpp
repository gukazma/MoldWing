/*
 *  MoldWing - Main Window Implementation
 *  S1.7/S1.8: With QUndoStack, menus, and file handling
 */

#include "MainWindow.hpp"
#include "Render/DiligentWidget.hpp"
#include "Core/MeshData.hpp"
#include "IO/MeshLoader.hpp"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeySequence>

namespace MoldWing
{

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(tr("MoldWing - Oblique Photography 3D Model Editor"));
    setMinimumSize(1280, 720);

    // Create undo stack
    m_undoStack = new QUndoStack(this);

    // Create DiligentEngine viewport as central widget
    m_viewport3D = new DiligentWidget(this);
    setCentralWidget(m_viewport3D);

    // Setup UI
    setupMenus();
    setupToolBar();
    setupStatusBar();
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

    statusBar()->showMessage(tr("Loading %1...").arg(filePath));

    MeshLoader loader;
    m_currentMesh = loader.load(filePath);

    if (!m_currentMesh)
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load model:\n%1").arg(loader.lastError()));
        statusBar()->showMessage(tr("Failed to load model"));
        return;
    }

    // Load mesh into renderer
    if (!m_viewport3D->loadMesh(*m_currentMesh))
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load mesh into renderer"));
        statusBar()->showMessage(tr("Failed to load mesh"));
        return;
    }

    // Update window title
    QFileInfo fileInfo(filePath);
    setWindowTitle(tr("MoldWing - %1").arg(fileInfo.fileName()));

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

    // TODO: Implement mesh saving
    QMessageBox::information(this, tr("Info"),
        tr("Save functionality will be implemented in a future update."));
}

void MainWindow::onResetView()
{
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

} // namespace MoldWing
