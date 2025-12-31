#include "OSGBExportDialog.hpp"
#include "IO/CoordinateSystem.hpp"
#include "Render/DiligentWidget.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFormLayout>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>

namespace MoldWing {

OSGBExportDialog::OSGBExportDialog(DiligentWidget* viewport, QWidget* parent)
    : QDialog(parent)
    , m_viewport(viewport)
{
    setWindowTitle(tr("Export OSGB"));
    setMinimumSize(600, 700);

    setupUI();
    populateModelList();
    populateCoordinateSystems();
    updateStatistics();
}

void OSGBExportDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    auto* modelGroup = new QGroupBox(tr("Models to Export"));
    auto* modelLayout = new QVBoxLayout(modelGroup);

    m_modelTree = new QTreeWidget();
    m_modelTree->setHeaderLabels({tr("Model"), tr("Faces")});
    m_modelTree->setRootIsDecorated(false);
    m_modelTree->header()->setStretchLastSection(false);
    m_modelTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_modelTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    connect(m_modelTree, &QTreeWidget::itemChanged, this, &OSGBExportDialog::onItemChanged);
    modelLayout->addWidget(m_modelTree);

    auto* buttonRow = new QHBoxLayout();
    auto* selectAllBtn = new QPushButton(tr("Select All"));
    auto* deselectAllBtn = new QPushButton(tr("Deselect All"));
    connect(selectAllBtn, &QPushButton::clicked, this, &OSGBExportDialog::onSelectAll);
    connect(deselectAllBtn, &QPushButton::clicked, this, &OSGBExportDialog::onDeselectAll);
    buttonRow->addWidget(selectAllBtn);
    buttonRow->addWidget(deselectAllBtn);
    buttonRow->addStretch();
    modelLayout->addLayout(buttonRow);

    mainLayout->addWidget(modelGroup);

    auto* crsGroup = new QGroupBox(tr("Coordinate Reference System"));
    auto* crsLayout = new QFormLayout(crsGroup);

    m_sourceEpsgCombo = new QComboBox();
    crsLayout->addRow(tr("Source CRS:"), m_sourceEpsgCombo);
    connect(m_sourceEpsgCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &OSGBExportDialog::onSourceEpsgChanged);

    m_targetEpsgCombo = new QComboBox();
    crsLayout->addRow(tr("Target CRS:"), m_targetEpsgCombo);

    m_customEpsgEdit = new QLineEdit();
    m_customEpsgEdit->setPlaceholderText(tr("Enter EPSG code (e.g., 4326)"));
    m_customEpsgEdit->setEnabled(false);
    crsLayout->addRow(tr("Custom EPSG:"), m_customEpsgEdit);

    auto* originLayout = new QHBoxLayout();
    m_originXSpin = new QDoubleSpinBox();
    m_originXSpin->setRange(-1e9, 1e9);
    m_originXSpin->setDecimals(6);
    m_originXSpin->setPrefix("X: ");
    m_originYSpin = new QDoubleSpinBox();
    m_originYSpin->setRange(-1e9, 1e9);
    m_originYSpin->setDecimals(6);
    m_originYSpin->setPrefix("Y: ");
    m_originZSpin = new QDoubleSpinBox();
    m_originZSpin->setRange(-1e9, 1e9);
    m_originZSpin->setDecimals(6);
    m_originZSpin->setPrefix("Z: ");
    originLayout->addWidget(m_originXSpin);
    originLayout->addWidget(m_originYSpin);
    originLayout->addWidget(m_originZSpin);
    crsLayout->addRow(tr("SRS Origin:"), originLayout);

    mainLayout->addWidget(crsGroup);

    auto* lodGroup = new QGroupBox(tr("Level of Detail (LOD)"));
    auto* lodLayout = new QFormLayout(lodGroup);

    m_generateLODCheck = new QCheckBox(tr("Generate LOD levels"));
    m_generateLODCheck->setChecked(true);
    lodLayout->addRow(m_generateLODCheck);

    m_lodLevelsSpin = new QSpinBox();
    m_lodLevelsSpin->setRange(1, 4);
    m_lodLevelsSpin->setValue(3);
    lodLayout->addRow(tr("LOD Levels:"), m_lodLevelsSpin);

    auto* ratioLayout = new QHBoxLayout();
    m_lodRatio1Spin = new QDoubleSpinBox();
    m_lodRatio1Spin->setRange(0.01, 1.0);
    m_lodRatio1Spin->setSingleStep(0.05);
    m_lodRatio1Spin->setValue(0.5);
    m_lodRatio1Spin->setPrefix("L1: ");
    m_lodRatio2Spin = new QDoubleSpinBox();
    m_lodRatio2Spin->setRange(0.01, 1.0);
    m_lodRatio2Spin->setSingleStep(0.05);
    m_lodRatio2Spin->setValue(0.25);
    m_lodRatio2Spin->setPrefix("L2: ");
    m_lodRatio3Spin = new QDoubleSpinBox();
    m_lodRatio3Spin->setRange(0.01, 1.0);
    m_lodRatio3Spin->setSingleStep(0.05);
    m_lodRatio3Spin->setValue(0.1);
    m_lodRatio3Spin->setPrefix("L3: ");
    ratioLayout->addWidget(m_lodRatio1Spin);
    ratioLayout->addWidget(m_lodRatio2Spin);
    ratioLayout->addWidget(m_lodRatio3Spin);
    lodLayout->addRow(tr("Simplify Ratios:"), ratioLayout);

    connect(m_generateLODCheck, &QCheckBox::toggled, [this](bool checked) {
        m_lodLevelsSpin->setEnabled(checked);
        m_lodRatio1Spin->setEnabled(checked);
        m_lodRatio2Spin->setEnabled(checked);
        m_lodRatio3Spin->setEnabled(checked);
    });

    mainLayout->addWidget(lodGroup);

    auto* outputGroup = new QGroupBox(tr("Output"));
    auto* outputLayout = new QHBoxLayout(outputGroup);

    m_outputDirEdit = new QLineEdit();
    m_outputDirEdit->setPlaceholderText(tr("Select output directory..."));
    outputLayout->addWidget(m_outputDirEdit);

    m_browseButton = new QPushButton(tr("Browse..."));
    connect(m_browseButton, &QPushButton::clicked, this, &OSGBExportDialog::onBrowseOutputDir);
    outputLayout->addWidget(m_browseButton);

    mainLayout->addWidget(outputGroup);

    m_statisticsLabel = new QLabel();
    mainLayout->addWidget(m_statisticsLabel);

    auto* dialogButtons = new QHBoxLayout();
    dialogButtons->addStretch();

    m_exportButton = new QPushButton(tr("Export"));
    m_exportButton->setDefault(true);
    connect(m_exportButton, &QPushButton::clicked, this, &QDialog::accept);
    dialogButtons->addWidget(m_exportButton);

    m_cancelButton = new QPushButton(tr("Cancel"));
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
    dialogButtons->addWidget(m_cancelButton);

    mainLayout->addLayout(dialogButtons);
}

void OSGBExportDialog::populateModelList()
{
    m_modelInfos.clear();
    m_modelTree->clear();

    if (!m_viewport)
        return;

    int meshCount = m_viewport->meshCount();
    for (int i = 0; i < meshCount; ++i)
    {
        const auto* instance = m_viewport->getMeshInstance(i);
        if (!instance || !instance->mesh)
            continue;

        OSGBExportModelInfo info;
        info.meshIndex = i;
        info.faceCount = instance->mesh->faceCount();
        info.selected = true;

        if (!instance->mesh->sourcePath.empty())
        {
            QFileInfo fileInfo(QString::fromStdString(instance->mesh->sourcePath));
            info.name = fileInfo.baseName();
        }
        else
        {
            info.name = QString("Model_%1").arg(i);
        }

        m_modelInfos.push_back(info);

        auto* item = new QTreeWidgetItem();
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Checked);
        item->setText(0, info.name);
        item->setText(1, QString::number(info.faceCount));
        item->setData(0, Qt::UserRole, i);
        m_modelTree->addTopLevelItem(item);
    }
}

void OSGBExportDialog::populateCoordinateSystems()
{
    auto systems = CoordinateSystem::getCommonSystems();

    m_sourceEpsgCombo->clear();
    m_targetEpsgCombo->clear();

    for (const auto& sys : systems)
    {
        QString text = QString("%1 - %2").arg(sys.epsgCode).arg(sys.name);
        m_sourceEpsgCombo->addItem(text, sys.epsgCode);
        m_targetEpsgCombo->addItem(text, sys.epsgCode);
    }

    m_sourceEpsgCombo->addItem(tr("Custom..."), -1);
    m_targetEpsgCombo->addItem(tr("Custom..."), -1);

    m_sourceEpsgCombo->setCurrentIndex(0);

    for (int i = 0; i < m_targetEpsgCombo->count(); ++i)
    {
        if (m_targetEpsgCombo->itemData(i).toInt() == 4326)
        {
            m_targetEpsgCombo->setCurrentIndex(i);
            break;
        }
    }
}

void OSGBExportDialog::onBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        m_outputDirEdit->text()
    );

    if (!dir.isEmpty())
    {
        m_outputDirEdit->setText(dir);
    }
}

void OSGBExportDialog::onSelectAll()
{
    for (int i = 0; i < m_modelTree->topLevelItemCount(); ++i)
    {
        m_modelTree->topLevelItem(i)->setCheckState(0, Qt::Checked);
    }
}

void OSGBExportDialog::onDeselectAll()
{
    for (int i = 0; i < m_modelTree->topLevelItemCount(); ++i)
    {
        m_modelTree->topLevelItem(i)->setCheckState(0, Qt::Unchecked);
    }
}

void OSGBExportDialog::onItemChanged(QTreeWidgetItem* item, int column)
{
    Q_UNUSED(item);
    Q_UNUSED(column);
    updateStatistics();
}

void OSGBExportDialog::updateStatistics()
{
    int selectedCount = 0;
    uint64_t totalFaces = 0;

    for (int i = 0; i < m_modelTree->topLevelItemCount(); ++i)
    {
        auto* item = m_modelTree->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
        {
            selectedCount++;
            int modelIndex = item->data(0, Qt::UserRole).toInt();
            if (modelIndex >= 0 && modelIndex < static_cast<int>(m_modelInfos.size()))
            {
                totalFaces += m_modelInfos[modelIndex].faceCount;
            }
        }
    }

    m_statisticsLabel->setText(tr("Selected: %1 model(s), %2 faces")
                                   .arg(selectedCount)
                                   .arg(totalFaces));

    m_exportButton->setEnabled(selectedCount > 0 && !m_outputDirEdit->text().isEmpty());
}

void OSGBExportDialog::onSourceEpsgChanged(int index)
{
    int epsgCode = m_sourceEpsgCombo->itemData(index).toInt();
    m_customEpsgEdit->setEnabled(epsgCode == -1);
}

OSGBExportOptions OSGBExportDialog::getExportOptions() const
{
    OSGBExportOptions options;

    options.outputDirectory = m_outputDirEdit->text();

    int srcEpsg = m_sourceEpsgCombo->currentData().toInt();
    if (srcEpsg == -1)
    {
        srcEpsg = m_customEpsgEdit->text().toInt();
    }
    options.sourceEpsg = srcEpsg;

    int dstEpsg = m_targetEpsgCombo->currentData().toInt();
    if (dstEpsg == -1)
    {
        dstEpsg = m_customEpsgEdit->text().toInt();
    }
    options.targetEpsg = dstEpsg;

    options.srsOriginX = m_originXSpin->value();
    options.srsOriginY = m_originYSpin->value();
    options.srsOriginZ = m_originZSpin->value();

    options.generateLOD = m_generateLODCheck->isChecked();
    options.lodLevels = m_lodLevelsSpin->value();
    options.lodRatio1 = static_cast<float>(m_lodRatio1Spin->value());
    options.lodRatio2 = static_cast<float>(m_lodRatio2Spin->value());
    options.lodRatio3 = static_cast<float>(m_lodRatio3Spin->value());

    return options;
}

std::vector<int> OSGBExportDialog::getSelectedModelIndices() const
{
    std::vector<int> indices;

    for (int i = 0; i < m_modelTree->topLevelItemCount(); ++i)
    {
        auto* item = m_modelTree->topLevelItem(i);
        if (item->checkState(0) == Qt::Checked)
        {
            indices.push_back(item->data(0, Qt::UserRole).toInt());
        }
    }

    return indices;
}

QString OSGBExportDialog::getOutputDirectory() const
{
    return m_outputDirEdit->text();
}

}
