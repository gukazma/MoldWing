/*
 *  MoldWing - Main Window Implementation
 *  S1.7/S1.8: With QUndoStack, menus, DockWidgets, and layout persistence
 */

#include "MainWindow.hpp"
#include "ExportDialog.hpp"
#include "Render/DiligentWidget.hpp"
#include "Core/MeshData.hpp"
#include "Core/Logger.hpp"
#include "Core/CompositeId.hpp"  // M8: For Mesh:Face display
#include "IO/MeshLoader.hpp"
#include "IO/MeshExporter.hpp"
#include "Selection/SelectionSystem.hpp"
#include "Texture/TextureEditBuffer.hpp"

#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QKeySequence>
#include <QDockWidget>
#include <QUndoView>
#include <QListWidget>
#include <QTreeWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QApplication>
#include <QProgressDialog>
#include <QtConcurrent>
#include <QFutureWatcher>
#include <QDirIterator>  // M8.1.1: 文件夹批量导入

#include <map>

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

    // Connect async model loader
    connect(&m_loadWatcher, &QFutureWatcher<std::shared_ptr<MeshData>>::finished,
            this, &MainWindow::onModelLoadFinished);

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

    // M8.1.1: Import Folder action
    m_importFolderAction = m_fileMenu->addAction(tr("Import &Folder..."));
    m_importFolderAction->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
    connect(m_importFolderAction, &QAction::triggered, this, &MainWindow::onImportFolder);

    m_saveAction = m_fileMenu->addAction(tr("&Save"));
    m_saveAction->setShortcut(QKeySequence::Save);
    connect(m_saveAction, &QAction::triggered, this, &MainWindow::onSaveFile);
    m_saveAction->setEnabled(false);

    // M7.5: Export As action
    m_exportAction = m_fileMenu->addAction(tr("Export &As..."));
    m_exportAction->setShortcut(QKeySequence(tr("Ctrl+Shift+S")));
    connect(m_exportAction, &QAction::triggered, this, &MainWindow::onExportFile);
    m_exportAction->setEnabled(false);

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

    m_viewMenu->addSeparator();

    m_toggleWhiteModelAction = m_viewMenu->addAction(tr("&White Model"));
    m_toggleWhiteModelAction->setCheckable(true);
    m_toggleWhiteModelAction->setShortcut(QKeySequence(Qt::Key_W));
    connect(m_toggleWhiteModelAction, &QAction::toggled, this, &MainWindow::onToggleWhiteModel);

    m_toggleWireframeAction = m_viewMenu->addAction(tr("Wire&frame"));
    m_toggleWireframeAction->setCheckable(true);
    m_toggleWireframeAction->setShortcut(QKeySequence(Qt::Key_F));
    connect(m_toggleWireframeAction, &QAction::toggled, this, &MainWindow::onToggleWireframe);

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
    // Left side: Layer Dock (M8: Layer tree for multi-model management)
    // =========================================================================
    m_layerDock = new QDockWidget(tr("Layers"), this);
    m_layerDock->setObjectName("LayerDock");
    m_layerDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);

    m_layerTree = new QTreeWidget(m_layerDock);
    m_layerTree->setHeaderLabel(tr("Model Layers"));
    m_layerTree->setMinimumWidth(150);
    m_layerDock->setWidget(m_layerTree);

    // M8: Connect visibility toggle signal
    connect(m_layerTree, &QTreeWidget::itemChanged,
            this, &MainWindow::onLayerVisibilityChanged);

    addDockWidget(Qt::LeftDockWidgetArea, m_layerDock);

    // Tabify layer dock with tool dock (make them share the same area)
    tabifyDockWidget(m_toolDock, m_layerDock);

    // Make tool dock the visible tab by default
    m_toolDock->raise();

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
    // M8: Display Mesh:Face format for composite IDs
    connect(m_viewport3D, &DiligentWidget::textureCoordPicked,
            this, [this](float u, float v, int texX, int texY, uint32_t compositeId) {
        // M8: Decode composite ID to Mesh:Face format
        uint32_t meshId = CompositeId::meshId(compositeId);
        uint32_t faceId = CompositeId::faceId(compositeId);

        statusBar()->showMessage(
            tr("UV: (%1, %2) | Pixel: (%3, %4) | Mesh:Face: %5:%6")
                .arg(u, 0, 'f', 3)
                .arg(v, 0, 'f', 3)
                .arg(texX)
                .arg(texY)
                .arg(meshId)
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
    m_viewMenu->addAction(m_layerDock->toggleViewAction());
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

    // Store file path for completion handler
    m_loadingFilePath = filePath;

    // Create progress dialog with busy indicator (0,0 range = infinite animation)
    QFileInfo fileInfo(filePath);
    m_loadProgressDialog = new QProgressDialog(
        tr("Loading %1...").arg(fileInfo.fileName()),
        QString(),  // No cancel button
        0, 0,       // Infinite animation
        this
    );
    m_loadProgressDialog->setWindowTitle(tr("Loading Model"));
    m_loadProgressDialog->setWindowModality(Qt::WindowModal);
    m_loadProgressDialog->setMinimumDuration(0);  // Show immediately
    m_loadProgressDialog->show();

    statusBar()->showMessage(tr("Loading %1...").arg(filePath));

    // Start async loading using QtConcurrent
    QFuture<std::shared_ptr<MeshData>> future = QtConcurrent::run([filePath]() {
        MeshLoader loader;
        return loader.load(filePath);
    });

    m_loadWatcher.setFuture(future);
}

// M8.1.1: 文件夹批量导入
void MainWindow::onImportFolder()
{
    // 选择目录
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Folder to Import"),
        QString(),
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks
    );

    if (dir.isEmpty())
        return;

    LOG_INFO("批量导入目录: {}", dir.toStdString());

    // 递归扫描所有 .obj 文件
    m_pendingFiles.clear();
    QDirIterator it(dir, QStringList() << "*.obj", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
    {
        m_pendingFiles.append(it.next());
    }

    if (m_pendingFiles.isEmpty())
    {
        QMessageBox::information(this, tr("Import Folder"),
            tr("No OBJ files found in the selected folder."));
        return;
    }

    LOG_INFO("找到 {} 个 OBJ 文件", m_pendingFiles.size());

    // 初始化批量加载状态
    m_loadedCount = 0;
    m_totalFilesToLoad = m_pendingFiles.size();
    m_batchLoadMode = true;

    // 创建进度对话框
    m_loadProgressDialog = new QProgressDialog(
        tr("Loading models: 0/%1").arg(m_totalFilesToLoad),
        tr("Cancel"),
        0, m_totalFilesToLoad,
        this
    );
    m_loadProgressDialog->setWindowTitle(tr("Batch Import"));
    m_loadProgressDialog->setWindowModality(Qt::WindowModal);
    m_loadProgressDialog->setMinimumDuration(0);
    m_loadProgressDialog->show();

    // 连接取消按钮
    connect(m_loadProgressDialog, &QProgressDialog::canceled, this, [this]() {
        m_pendingFiles.clear();
        m_batchLoadMode = false;
        statusBar()->showMessage(tr("Batch import cancelled. Loaded %1 models.").arg(m_loadedCount));
    });

    // 开始加载第一个文件
    loadNextPendingFile();
}

void MainWindow::loadNextPendingFile()
{
    if (m_pendingFiles.isEmpty())
    {
        // 批量加载完成
        m_batchLoadMode = false;

        if (m_loadProgressDialog)
        {
            m_loadProgressDialog->close();
            m_loadProgressDialog->deleteLater();
            m_loadProgressDialog = nullptr;
        }

        // 适配相机到所有模型
        if (!m_meshList.empty())
        {
            BoundingBox combinedBounds;
            combinedBounds.reset();
            for (const auto& mesh : m_meshList)
            {
                if (mesh)
                {
                    const auto& b = mesh->bounds;
                    combinedBounds.expand(b.min[0], b.min[1], b.min[2]);
                    combinedBounds.expand(b.max[0], b.max[1], b.max[2]);
                }
            }
            m_viewport3D->camera().fitToModel(
                combinedBounds.min[0], combinedBounds.min[1], combinedBounds.min[2],
                combinedBounds.max[0], combinedBounds.max[1], combinedBounds.max[2]);
        }

        LOG_INFO("批量导入完成，共加载 {} 个模型", m_loadedCount);
        statusBar()->showMessage(tr("Batch import complete. Loaded %1 models.").arg(m_loadedCount));
        setWindowTitle(tr("MoldWing - %1 models loaded").arg(m_meshList.size()));
        return;
    }

    // 获取下一个文件
    QString filePath = m_pendingFiles.takeFirst();
    m_loadingFilePath = filePath;

    // 更新进度对话框
    if (m_loadProgressDialog)
    {
        m_loadProgressDialog->setLabelText(
            tr("Loading: %1\n(%2/%3)")
            .arg(QFileInfo(filePath).fileName())
            .arg(m_loadedCount + 1)
            .arg(m_totalFilesToLoad));
        m_loadProgressDialog->setValue(m_loadedCount);
    }

    statusBar()->showMessage(tr("Loading %1 (%2/%3)...")
        .arg(QFileInfo(filePath).fileName())
        .arg(m_loadedCount + 1)
        .arg(m_totalFilesToLoad));

    // 开始异步加载
    QFuture<std::shared_ptr<MeshData>> future = QtConcurrent::run([filePath]() {
        MeshLoader loader;
        return loader.load(filePath);
    });

    m_loadWatcher.setFuture(future);
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

    // M7.5: Use MeshExporter to save with edited textures
    MeshExporter exporter;

    // Build edit buffer map (texture ID -> edit buffer)
    std::unordered_map<int, std::shared_ptr<TextureEditBuffer>> editBuffers;
    if (m_viewport3D && m_viewport3D->editBuffer() && m_viewport3D->editBuffer()->isValid())
    {
        // Currently we only support single texture editing
        // The first texture (index 0) is the one being edited
        auto buffer = std::make_shared<TextureEditBuffer>();
        *buffer = *m_viewport3D->editBuffer();  // Copy the edit buffer
        editBuffers[0] = buffer;
    }

    if (exporter.exportOBJ(filePath, *m_currentMesh, editBuffers))
    {
        statusBar()->showMessage(tr("Saved: %1").arg(filePath), 5000);
    }
    else
    {
        QMessageBox::critical(this, tr("Error"),
            tr("Failed to save file:\n%1").arg(exporter.lastError()));
    }
}

void MainWindow::onExportFile()
{
    // B6: Check if we have any models loaded
    if (!m_viewport3D || m_viewport3D->meshCount() == 0)
    {
        QMessageBox::warning(this, tr("No Models"),
            tr("No models loaded. Please load models first."));
        return;
    }

    // B6: Show export dialog for multi-model selection
    ExportDialog dialog(m_viewport3D, this);
    if (dialog.exec() != QDialog::Accepted)
        return;

    std::vector<int> selectedIndices = dialog.getSelectedModelIndices();
    m_exportOutputDir = dialog.getOutputDirectory();

    if (selectedIndices.empty())
    {
        QMessageBox::warning(this, tr("No Selection"),
            tr("Please select at least one model to export."));
        return;
    }

    LOG_INFO("批量导出 {} 个模型到目录: {}", selectedIndices.size(), m_exportOutputDir.toStdString());

    // Build export task list
    m_exportTasks.clear();
    for (int meshIndex : selectedIndices)
    {
        const MeshInstance* instance = m_viewport3D->getMeshInstance(meshIndex);
        if (!instance || !instance->mesh)
            continue;

        ExportTask task;
        task.meshIndex = meshIndex;
        task.mesh = instance->mesh;

        // Get model name for filename
        if (!instance->mesh->sourcePath.empty())
        {
            QFileInfo fileInfo(QString::fromStdString(instance->mesh->sourcePath));
            task.modelName = fileInfo.baseName();
        }
        else
        {
            task.modelName = QString("Model_%1").arg(meshIndex);
        }
        task.filePath = QDir(m_exportOutputDir).filePath(task.modelName + ".obj");

        // Copy edit buffers
        for (size_t texIdx = 0; texIdx < instance->editBuffers.size(); ++texIdx)
        {
            if (instance->editBuffers[texIdx] && instance->editBuffers[texIdx]->isValid())
            {
                auto buffer = std::make_shared<TextureEditBuffer>();
                *buffer = *instance->editBuffers[texIdx];
                task.editBuffers[static_cast<int>(texIdx)] = buffer;
            }
        }

        m_exportTasks.push_back(std::move(task));
    }

    if (m_exportTasks.empty())
    {
        QMessageBox::warning(this, tr("No Valid Models"),
            tr("No valid models to export."));
        return;
    }

    // Initialize export state
    m_exportedCount = 0;
    m_exportSuccessCount = 0;
    m_exportFailedModels.clear();

    // Create progress dialog (non-modal to allow async operation)
    m_exportProgressDialog = new QProgressDialog(
        tr("Exporting models..."),
        tr("Cancel"),
        0,
        static_cast<int>(m_exportTasks.size()),
        this
    );
    m_exportProgressDialog->setWindowTitle(tr("Exporting"));
    m_exportProgressDialog->setWindowModality(Qt::WindowModal);
    m_exportProgressDialog->setMinimumDuration(0);
    m_exportProgressDialog->setValue(0);

    // Connect watcher
    connect(&m_exportWatcher, &QFutureWatcher<bool>::finished,
            this, &MainWindow::onExportFinished);

    // Start exporting
    exportNextModel();
}

void MainWindow::exportNextModel()
{
    // Check if cancelled
    if (m_exportProgressDialog && m_exportProgressDialog->wasCanceled())
    {
        m_exportProgressDialog->close();
        delete m_exportProgressDialog;
        m_exportProgressDialog = nullptr;
        m_exportTasks.clear();

        statusBar()->showMessage(tr("Export cancelled. Exported %1 models.").arg(m_exportSuccessCount), 5000);
        return;
    }

    // Check if all done
    if (m_exportedCount >= static_cast<int>(m_exportTasks.size()))
    {
        m_exportProgressDialog->close();
        delete m_exportProgressDialog;
        m_exportProgressDialog = nullptr;
        m_exportTasks.clear();

        // Show result
        if (m_exportFailedModels.isEmpty())
        {
            statusBar()->showMessage(tr("Exported %1 models").arg(m_exportSuccessCount), 5000);
            QMessageBox::information(this, tr("Export Complete"),
                tr("Successfully exported %1 model(s) to:\n%2").arg(m_exportSuccessCount).arg(m_exportOutputDir));
        }
        else
        {
            QString message = tr("Exported %1 model(s).\n\nFailed to export:\n%2")
                .arg(m_exportSuccessCount)
                .arg(m_exportFailedModels.join("\n"));
            QMessageBox::warning(this, tr("Export Partially Complete"), message);
        }
        return;
    }

    // Get current task
    const ExportTask& task = m_exportTasks[m_exportedCount];

    // Update progress dialog
    if (m_exportProgressDialog)
    {
        m_exportProgressDialog->setLabelText(tr("Exporting: %1 (%2/%3)")
            .arg(task.modelName)
            .arg(m_exportedCount + 1)
            .arg(m_exportTasks.size()));
    }

    // Run export in background thread
    QFuture<bool> future = QtConcurrent::run([task]() {
        MeshExporter exporter;
        return exporter.exportOBJ(task.filePath, *task.mesh, task.editBuffers);
    });

    m_exportWatcher.setFuture(future);
}

void MainWindow::onExportFinished()
{
    bool success = m_exportWatcher.result();
    const ExportTask& task = m_exportTasks[m_exportedCount];

    if (success)
    {
        m_exportSuccessCount++;
        LOG_INFO("导出成功: {}", task.filePath.toStdString());
    }
    else
    {
        m_exportFailedModels.append(task.modelName);
        LOG_ERROR("导出失败: {}", task.filePath.toStdString());
    }

    m_exportedCount++;

    // Update progress
    if (m_exportProgressDialog)
    {
        m_exportProgressDialog->setValue(m_exportedCount);
    }

    // Continue with next model
    exportNextModel();
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

    const auto& selectedFaces = m_viewport3D->selectionSystem()->selectedFaces();
    size_t count = selectedFaces.size();

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
        // M8: Count faces per mesh using CompositeId
        std::map<uint32_t, size_t> meshFaceCounts;
        for (uint32_t compositeId : selectedFaces)
        {
            uint32_t meshId = CompositeId::meshId(compositeId);
            meshFaceCounts[meshId]++;
        }
        size_t meshCount = meshFaceCounts.size();

        // Build property panel text
        QString propertyText;
        if (meshCount > 1)
        {
            // Multi-mesh selection
            propertyText = tr("Selected: %1 faces from %2 meshes\n\n").arg(count).arg(meshCount);
            for (const auto& [meshId, faceCount] : meshFaceCounts)
            {
                propertyText += tr("  Mesh %1: %2 faces\n").arg(meshId).arg(faceCount);
            }
        }
        else if (m_currentMesh)
        {
            propertyText = tr("Model: %1\nVertices: %2\nFaces: %3\n\nSelected: %4 faces")
                .arg(windowTitle().section(" - ", 1))
                .arg(m_currentMesh->vertexCount())
                .arg(m_currentMesh->faceCount())
                .arg(count);
        }
        else
        {
            propertyText = tr("Selected: %1 faces").arg(count);
        }
        m_propertyLabel->setText(propertyText);

        // Update status bar
        if (meshCount > 1)
        {
            statusBar()->showMessage(tr("%1 faces selected from %2 meshes").arg(count).arg(meshCount));
        }
        else
        {
            statusBar()->showMessage(tr("%1 faces selected").arg(count));
        }
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

void MainWindow::onToggleWhiteModel(bool checked)
{
    if (!m_viewport3D)
        return;

    m_viewport3D->setWhiteModelMode(checked);

    if (checked)
    {
        statusBar()->showMessage(tr("White model mode enabled"), 3000);
        LOG_INFO("白模模式：开启");
    }
    else
    {
        statusBar()->showMessage(tr("White model mode disabled"), 3000);
        LOG_INFO("白模模式：关闭");
    }
}

void MainWindow::onToggleWireframe(bool checked)
{
    if (!m_viewport3D)
        return;

    m_viewport3D->setShowWireframe(checked);

    if (checked)
    {
        statusBar()->showMessage(tr("Wireframe mode enabled"), 3000);
        LOG_INFO("线框模式：开启");
    }
    else
    {
        statusBar()->showMessage(tr("Wireframe mode disabled"), 3000);
        LOG_INFO("线框模式：关闭");
    }
}

void MainWindow::onModelLoadFinished()
{
    // M8.1.1: 批量加载模式下不关闭进度对话框
    if (!m_batchLoadMode && m_loadProgressDialog)
    {
        m_loadProgressDialog->close();
        m_loadProgressDialog->deleteLater();
        m_loadProgressDialog = nullptr;
    }

    // Get the loaded mesh from the future
    auto loadedMesh = m_loadWatcher.result();

    if (!loadedMesh)
    {
        MW_LOG_ERROR("加载模型失败: {}", m_loadingFilePath.toStdString());

        // M8.1.1: 批量加载模式下继续加载下一个
        if (m_batchLoadMode)
        {
            m_loadedCount++;  // 仍然计数（作为处理过的文件）
            loadNextPendingFile();
            return;
        }

        QMessageBox::critical(this, tr("Error"),
            tr("Failed to load model"));
        statusBar()->showMessage(tr("Failed to load model"));
        return;
    }

    QFileInfo fileInfo(m_loadingFilePath);

    LOG_INFO("模型加载成功: {} 顶点, {} 面",
             loadedMesh->vertexCount(), loadedMesh->faceCount());

    // M8: Use addMesh() for multi-model rendering
    int meshIndex = m_viewport3D->addMesh(loadedMesh);

    if (meshIndex < 0)
    {
        MW_LOG_ERROR("添加网格到渲染器失败");

        // M8.1.1: 批量加载模式下继续加载下一个
        if (m_batchLoadMode)
        {
            m_loadedCount++;
            loadNextPendingFile();
            return;
        }

        QMessageBox::critical(this, tr("Error"),
            tr("Failed to add mesh to renderer"));
        statusBar()->showMessage(tr("Failed to add mesh"));
        return;
    }

    // Also keep legacy loadMesh for selection/texture editing (uses first mesh)
    if (m_meshList.empty())
    {
        m_currentMesh = loadedMesh;
        if (!m_viewport3D->loadMesh(loadedMesh))
        {
            // Non-fatal, multi-model rendering still works
            LOG_WARN("Legacy loadMesh failed, but addMesh succeeded");
        }
    }
    else
    {
        // Update current mesh reference
        m_currentMesh = loadedMesh;
    }

    // Update property panel
    m_propertyLabel->setText(tr("Model: %1\nVertices: %2\nFaces: %3")
        .arg(fileInfo.fileName())
        .arg(loadedMesh->vertexCount())
        .arg(loadedMesh->faceCount()));

    // Enable save and export actions
    m_saveAction->setEnabled(true);
    m_exportAction->setEnabled(true);

    // M8: Add to mesh list and layer tree (append mode)
    m_meshList.push_back(loadedMesh);

    // Block signals to prevent triggering onLayerVisibilityChanged during setup
    m_layerTree->blockSignals(true);
    QTreeWidgetItem* layerItem = new QTreeWidgetItem(m_layerTree);
    layerItem->setText(0, fileInfo.fileName());
    layerItem->setIcon(0, style()->standardIcon(QStyle::SP_FileIcon));
    // M8: Add checkbox for visibility control
    layerItem->setFlags(layerItem->flags() | Qt::ItemIsUserCheckable);
    layerItem->setCheckState(0, Qt::Checked);  // Visible by default
    layerItem->setData(0, Qt::UserRole, meshIndex);  // Store mesh index (from addMesh)
    m_layerTree->addTopLevelItem(layerItem);
    m_layerTree->setCurrentItem(layerItem);
    m_layerTree->blockSignals(false);

    // M8.1.1: 批量加载模式下不更新标题和相机，最后统一处理
    if (m_batchLoadMode)
    {
        m_loadedCount++;
        loadNextPendingFile();
        return;
    }

    // Update window title with model count
    if (m_meshList.size() == 1)
    {
        setWindowTitle(tr("MoldWing - %1").arg(fileInfo.fileName()));
    }
    else
    {
        setWindowTitle(tr("MoldWing - %1 models loaded").arg(m_meshList.size()));
    }

    // Fit camera to all loaded models (compute combined bounding box)
    BoundingBox combinedBounds;
    combinedBounds.reset();
    for (const auto& mesh : m_meshList)
    {
        if (mesh)
        {
            const auto& b = mesh->bounds;
            combinedBounds.expand(b.min[0], b.min[1], b.min[2]);
            combinedBounds.expand(b.max[0], b.max[1], b.max[2]);
        }
    }
    m_viewport3D->camera().fitToModel(
        combinedBounds.min[0], combinedBounds.min[1], combinedBounds.min[2],
        combinedBounds.max[0], combinedBounds.max[1], combinedBounds.max[2]);

    // Clear undo stack for new file
    m_undoStack->clear();

    statusBar()->showMessage(tr("Loaded: %1 vertices, %2 faces (Total: %3 models)")
        .arg(loadedMesh->vertexCount())
        .arg(loadedMesh->faceCount())
        .arg(m_meshList.size()));
}

// M8: Handle layer visibility toggle
void MainWindow::onLayerVisibilityChanged(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(column);

    if (!item)
        return;

    // Get the mesh index from the item data
    int meshIndex = item->data(0, Qt::UserRole).toInt();
    bool isVisible = (item->checkState(0) == Qt::Checked);

    LOG_DEBUG("Layer {} visibility changed to: {}", meshIndex, isVisible);

    // M8: Use indexed setMeshVisible for multi-model support
    m_viewport3D->setMeshVisible(meshIndex, isVisible);

    if (isVisible)
    {
        statusBar()->showMessage(tr("Layer %1 visible").arg(item->text(0)), 2000);
    }
    else
    {
        statusBar()->showMessage(tr("Layer %1 hidden").arg(item->text(0)), 2000);
    }
}

} // namespace MoldWing
