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
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
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

    m_editMenu->addSeparator();

    // M5: Grow/Shrink selection
    m_growSelectionAction = m_editMenu->addAction(tr("&Grow Selection"));
    m_growSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Plus));
    connect(m_growSelectionAction, &QAction::triggered, this, &MainWindow::onGrowSelection);

    m_shrinkSelectionAction = m_editMenu->addAction(tr("S&hrink Selection"));
    m_shrinkSelectionAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Minus));
    connect(m_shrinkSelectionAction, &QAction::triggered, this, &MainWindow::onShrinkSelection);

    // View menu
    m_viewMenu = menuBar()->addMenu(tr("&View"));

    m_resetViewAction = m_viewMenu->addAction(tr("&Reset View"));
    m_resetViewAction->setShortcut(QKeySequence(Qt::Key_Home));
    connect(m_resetViewAction, &QAction::triggered, this, &MainWindow::onResetView);

    // M7: Texture menu
    m_textureMenu = menuBar()->addMenu(tr("&Texture"));

    m_enterTextureEditAction = m_textureMenu->addAction(tr("&Enter Edit Mode"));
    m_enterTextureEditAction->setShortcut(QKeySequence(Qt::Key_T));
    connect(m_enterTextureEditAction, &QAction::triggered, this, &MainWindow::onEnterTextureEditMode);

    m_exitTextureEditAction = m_textureMenu->addAction(tr("E&xit Edit Mode"));
    m_exitTextureEditAction->setShortcut(QKeySequence(Qt::Key_Escape));
    connect(m_exitTextureEditAction, &QAction::triggered, this, &MainWindow::onExitTextureEditMode);
    m_exitTextureEditAction->setEnabled(false);

    m_textureMenu->addSeparator();

    m_saveTextureAction = m_textureMenu->addAction(tr("&Save Texture..."));
    m_saveTextureAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    connect(m_saveTextureAction, &QAction::triggered, this, &MainWindow::onSaveTexture);
    m_saveTextureAction->setEnabled(false);
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

    // -------------------------------------------------------------------------
    // Brush Settings Group
    // -------------------------------------------------------------------------
    m_brushSettingsGroup = new QGroupBox(tr("Brush Settings"), propertyWidget);
    QVBoxLayout* brushLayout = new QVBoxLayout(m_brushSettingsGroup);

    // Brush radius row
    QHBoxLayout* radiusRow = new QHBoxLayout();
    QLabel* radiusLabel = new QLabel(tr("Size:"), m_brushSettingsGroup);
    radiusRow->addWidget(radiusLabel);

    m_brushRadiusSlider = new QSlider(Qt::Horizontal, m_brushSettingsGroup);
    m_brushRadiusSlider->setRange(DiligentWidget::MIN_BRUSH_RADIUS, DiligentWidget::MAX_BRUSH_RADIUS);
    m_brushRadiusSlider->setValue(DiligentWidget::DEFAULT_BRUSH_RADIUS);
    radiusRow->addWidget(m_brushRadiusSlider, 1);

    m_brushRadiusSpinBox = new QSpinBox(m_brushSettingsGroup);
    m_brushRadiusSpinBox->setRange(DiligentWidget::MIN_BRUSH_RADIUS, DiligentWidget::MAX_BRUSH_RADIUS);
    m_brushRadiusSpinBox->setValue(DiligentWidget::DEFAULT_BRUSH_RADIUS);
    m_brushRadiusSpinBox->setSuffix(tr(" px"));
    radiusRow->addWidget(m_brushRadiusSpinBox);

    brushLayout->addLayout(radiusRow);
    m_brushSettingsGroup->setLayout(brushLayout);

    // Connect slider and spinbox bidirectionally
    connect(m_brushRadiusSlider, &QSlider::valueChanged, m_brushRadiusSpinBox, &QSpinBox::setValue);
    connect(m_brushRadiusSpinBox, QOverload<int>::of(&QSpinBox::valueChanged), m_brushRadiusSlider, &QSlider::setValue);

    // Connect to viewport brush radius
    connect(m_brushRadiusSlider, &QSlider::valueChanged, this, &MainWindow::onBrushRadiusChanged);
    connect(m_viewport3D, &DiligentWidget::brushRadiusChanged, this, [this](int radius) {
        // Update UI when brush radius changes externally (e.g., via keyboard)
        m_brushRadiusSlider->blockSignals(true);
        m_brushRadiusSpinBox->blockSignals(true);
        m_brushRadiusSlider->setValue(radius);
        m_brushRadiusSpinBox->setValue(radius);
        m_brushRadiusSlider->blockSignals(false);
        m_brushRadiusSpinBox->blockSignals(false);
    });

    propertyLayout->addWidget(m_brushSettingsGroup);

    // Initially hide brush settings (only show when brush mode is active)
    m_brushSettingsGroup->setVisible(false);

    // -------------------------------------------------------------------------
    // M5: Link Settings Group
    // -------------------------------------------------------------------------
    m_linkSettingsGroup = new QGroupBox(tr("Link Settings"), propertyWidget);
    QVBoxLayout* linkLayout = new QVBoxLayout(m_linkSettingsGroup);

    // Angle threshold row
    QHBoxLayout* angleRow = new QHBoxLayout();
    QLabel* angleLabel = new QLabel(tr("Angle:"), m_linkSettingsGroup);
    angleRow->addWidget(angleLabel);

    m_linkAngleSlider = new QSlider(Qt::Horizontal, m_linkSettingsGroup);
    m_linkAngleSlider->setRange(static_cast<int>(DiligentWidget::MIN_ANGLE_THRESHOLD),
                                 static_cast<int>(DiligentWidget::MAX_ANGLE_THRESHOLD));
    m_linkAngleSlider->setValue(static_cast<int>(DiligentWidget::DEFAULT_ANGLE_THRESHOLD));
    angleRow->addWidget(m_linkAngleSlider, 1);

    m_linkAngleSpinBox = new QDoubleSpinBox(m_linkSettingsGroup);
    m_linkAngleSpinBox->setRange(DiligentWidget::MIN_ANGLE_THRESHOLD, DiligentWidget::MAX_ANGLE_THRESHOLD);
    m_linkAngleSpinBox->setValue(DiligentWidget::DEFAULT_ANGLE_THRESHOLD);
    m_linkAngleSpinBox->setSuffix(tr("°"));
    m_linkAngleSpinBox->setDecimals(1);
    angleRow->addWidget(m_linkAngleSpinBox);

    linkLayout->addLayout(angleRow);

    // Hint label
    QLabel* hintLabel = new QLabel(tr("180° = select all connected\n0° = select coplanar only"), m_linkSettingsGroup);
    hintLabel->setStyleSheet("color: gray; font-size: 10px;");
    linkLayout->addWidget(hintLabel);

    m_linkSettingsGroup->setLayout(linkLayout);

    // Connect slider and spinbox bidirectionally
    connect(m_linkAngleSlider, &QSlider::valueChanged, this, [this](int value) {
        m_linkAngleSpinBox->blockSignals(true);
        m_linkAngleSpinBox->setValue(value);
        m_linkAngleSpinBox->blockSignals(false);
        onLinkAngleThresholdChanged(value);
    });
    connect(m_linkAngleSpinBox, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this](double value) {
        m_linkAngleSlider->blockSignals(true);
        m_linkAngleSlider->setValue(static_cast<int>(value));
        m_linkAngleSlider->blockSignals(false);
        onLinkAngleThresholdChanged(value);
    });

    // Connect to viewport angle threshold
    connect(m_viewport3D, &DiligentWidget::linkAngleThresholdChanged, this, [this](float angle) {
        // Update UI when angle threshold changes externally
        m_linkAngleSlider->blockSignals(true);
        m_linkAngleSpinBox->blockSignals(true);
        m_linkAngleSlider->setValue(static_cast<int>(angle));
        m_linkAngleSpinBox->setValue(angle);
        m_linkAngleSlider->blockSignals(false);
        m_linkAngleSpinBox->blockSignals(false);
    });

    // Step 1: Connect texture coordinate picked signal to status bar
    connect(m_viewport3D, &DiligentWidget::textureCoordPicked,
            this, [this](float u, float v, int texX, int texY, uint32_t faceId) {
        statusBar()->showMessage(
            tr("UV: (%1, %2) | Pixel: (%3, %4) | Face: %5")
                .arg(u, 0, 'f', 3)
                .arg(v, 0, 'f', 3)
                .arg(texX)
                .arg(texY)
                .arg(faceId),
            3000);  // Show for 3 seconds
    });

    // Step 4: Connect clone source set signal
    connect(m_viewport3D, &DiligentWidget::cloneSourceSet,
            this, [this](int texX, int texY) {
        statusBar()->showMessage(
            tr("Clone Source Set: Pixel (%1, %2) - Drag to clone")
                .arg(texX)
                .arg(texY),
            5000);  // Show for 5 seconds
    });

    propertyLayout->addWidget(m_linkSettingsGroup);

    // Initially hide link settings (only show when link mode is active)
    m_linkSettingsGroup->setVisible(false);

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
    if (!m_viewport3D->loadMesh(m_currentMesh))
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

    // Show/hide brush settings based on tool
    bool isBrushTool = (index == 1);
    bool isLinkTool = (index == 3);
    bool isTextureTool = (index >= 4 && index <= 7);  // Paint, Eraser, Clone, Healing

    if (m_brushSettingsGroup)
    {
        m_brushSettingsGroup->setVisible(isBrushTool || isTextureTool);
    }
    if (m_linkSettingsGroup)
    {
        m_linkSettingsGroup->setVisible(isLinkTool);
    }

    // Selection tools (0-3)
    if (index >= 0 && index <= 3)
    {
        m_viewport3D->setInteractionMode(DiligentWidget::InteractionMode::Selection);

        // Set the specific selection mode
        switch (index)
        {
            case 0:  // Box Select
                m_viewport3D->selectionSystem()->setMode(SelectionMode::Box);
                statusBar()->showMessage(tr("Box selection mode - drag to select faces"));
                break;
            case 1:  // Brush Select
                m_viewport3D->selectionSystem()->setMode(SelectionMode::Brush);
                statusBar()->showMessage(tr("Brush selection mode - paint to select faces ([ ] to adjust size)"));
                break;
            case 2:  // Lasso Select
                m_viewport3D->selectionSystem()->setMode(SelectionMode::Lasso);
                statusBar()->showMessage(tr("Lasso selection mode - draw a closed path to select faces"));
                break;
            case 3:  // Connected Select
                m_viewport3D->selectionSystem()->setMode(SelectionMode::Link);
                statusBar()->showMessage(tr("Connected selection mode - click to select connected faces"));
                break;
        }
    }
    // Texture editing tools (4-7)
    else if (index >= 4 && index <= 7)
    {
        m_viewport3D->setInteractionMode(DiligentWidget::InteractionMode::TextureEdit);

        switch (index)
        {
            case 4:  // Paint Brush
                statusBar()->showMessage(tr("Paint mode - drag to paint red on texture"));
                break;
            case 5:  // Eraser
                statusBar()->showMessage(tr("Eraser mode (not yet implemented)"));
                break;
            case 6:  // Clone Stamp
                statusBar()->showMessage(tr("Clone Stamp - Alt+click to set source, drag to clone"));
                break;
            case 7:  // Healing Brush
                statusBar()->showMessage(tr("Healing Brush (not yet implemented)"));
                break;
        }
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

void MainWindow::onBrushRadiusChanged(int radius)
{
    if (m_viewport3D)
    {
        m_viewport3D->setBrushRadius(radius);
    }
}

void MainWindow::onLinkAngleThresholdChanged(double angle)
{
    if (m_viewport3D)
    {
        m_viewport3D->setLinkAngleThreshold(static_cast<float>(angle));
    }
}

void MainWindow::onGrowSelection()
{
    if (!m_viewport3D || !m_viewport3D->selectionSystem() || !m_currentMesh)
        return;

    if (m_currentMesh->faceAdjacency.empty())
    {
        LOG_WARN("Mesh has no adjacency data for grow selection");
        return;
    }

    // Save current selection for undo
    auto oldSelection = m_viewport3D->selectionSystem()->selectedFaces();

    // Grow selection
    m_viewport3D->selectionSystem()->growSelection(m_currentMesh->faceAdjacency);

    // Create undo command
    if (m_undoStack)
    {
        auto newSelection = m_viewport3D->selectionSystem()->selectedFaces();
        m_undoStack->push(new SelectFacesCommand(m_viewport3D->selectionSystem(),
                          newSelection, tr("Grow Selection")));
    }

    LOG_DEBUG("扩展选择");
}

void MainWindow::onShrinkSelection()
{
    if (!m_viewport3D || !m_viewport3D->selectionSystem() || !m_currentMesh)
        return;

    if (m_currentMesh->faceAdjacency.empty())
    {
        LOG_WARN("Mesh has no adjacency data for shrink selection");
        return;
    }

    // Save current selection for undo
    auto oldSelection = m_viewport3D->selectionSystem()->selectedFaces();

    // Shrink selection
    m_viewport3D->selectionSystem()->shrinkSelection(m_currentMesh->faceAdjacency);

    // Create undo command
    if (m_undoStack)
    {
        auto newSelection = m_viewport3D->selectionSystem()->selectedFaces();
        m_undoStack->push(new SelectFacesCommand(m_viewport3D->selectionSystem(),
                          newSelection, tr("Shrink Selection")));
    }

    LOG_DEBUG("收缩选择");
}

// M7: Texture editing slots

void MainWindow::onEnterTextureEditMode()
{
    if (!m_viewport3D || !m_currentMesh)
    {
        QMessageBox::warning(this, tr("Warning"),
            tr("Please load a model with textures first."));
        return;
    }

    // Switch to texture edit mode with Clone Stamp tool selected
    m_viewport3D->setInteractionMode(DiligentWidget::InteractionMode::TextureEdit);

    // Update tool list selection
    if (m_toolList)
    {
        m_toolList->setCurrentRow(6);  // Clone Stamp
    }

    // Enable/disable menu actions
    m_enterTextureEditAction->setEnabled(false);
    m_exitTextureEditAction->setEnabled(true);
    m_saveTextureAction->setEnabled(true);

    statusBar()->showMessage(tr("[Texture Edit Mode] Alt+Click to set clone source, drag to clone. Press Esc to exit."));
    LOG_INFO("进入纹理编辑模式");
}

void MainWindow::onExitTextureEditMode()
{
    if (!m_viewport3D)
        return;

    // Switch to camera mode
    m_viewport3D->setInteractionMode(DiligentWidget::InteractionMode::Camera);

    // Clear tool list selection or select a selection tool
    if (m_toolList)
    {
        m_toolList->setCurrentRow(0);  // Box Select
    }

    // Enable/disable menu actions
    m_enterTextureEditAction->setEnabled(true);
    m_exitTextureEditAction->setEnabled(false);

    statusBar()->showMessage(tr("Exited texture edit mode"));
    LOG_INFO("退出纹理编辑模式");
}

void MainWindow::onSaveTexture()
{
    if (!m_viewport3D)
        return;

    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Texture"),
        QString(),
        tr("PNG Images (*.png);;JPEG Images (*.jpg *.jpeg);;BMP Images (*.bmp);;All Files (*)")
    );

    if (filePath.isEmpty())
        return;

    LOG_INFO("保存纹理: {}", filePath.toStdString());

    if (m_viewport3D->saveTexture(filePath))
    {
        statusBar()->showMessage(tr("Texture saved: %1").arg(filePath), 5000);
    }
    else
    {
        QMessageBox::warning(this, tr("Error"),
            tr("Failed to save texture to:\n%1").arg(filePath));
    }
}

} // namespace MoldWing
