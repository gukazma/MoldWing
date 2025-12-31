/*
 *  MoldWing - Export Dialog
 *  B6: Model selection export dialog implementation
 */

#include "ExportDialog.hpp"
#include "Render/DiligentWidget.hpp"
#include "Core/MeshData.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGroupBox>
#include <QFileDialog>
#include <QHeaderView>
#include <QFileInfo>

namespace MoldWing
{

ExportDialog::ExportDialog(DiligentWidget* viewport, QWidget* parent)
    : QDialog(parent)
    , m_viewport(viewport)
{
    setWindowTitle(tr("Export Models"));
    setMinimumSize(500, 400);
    setupUI();
    populateModelList();
    updateStatistics();
    updateExportButton();
}

ExportDialog::~ExportDialog() = default;

void ExportDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);

    // Model list group
    auto* modelGroup = new QGroupBox(tr("Models to Export"));
    auto* modelLayout = new QVBoxLayout(modelGroup);

    m_modelTree = new QTreeWidget;
    m_modelTree->setHeaderLabels({tr("Model"), tr("Faces")});
    m_modelTree->setRootIsDecorated(false);
    m_modelTree->setAlternatingRowColors(true);
    m_modelTree->header()->setStretchLastSection(false);
    m_modelTree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_modelTree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    connect(m_modelTree, &QTreeWidget::itemChanged, this, &ExportDialog::onItemChanged);

    // Select all / Deselect all buttons
    auto* selectionLayout = new QHBoxLayout;
    m_selectAllButton = new QPushButton(tr("Select All"));
    m_deselectAllButton = new QPushButton(tr("Deselect All"));
    selectionLayout->addWidget(m_selectAllButton);
    selectionLayout->addWidget(m_deselectAllButton);
    selectionLayout->addStretch();
    connect(m_selectAllButton, &QPushButton::clicked, this, &ExportDialog::onSelectAll);
    connect(m_deselectAllButton, &QPushButton::clicked, this, &ExportDialog::onDeselectAll);

    modelLayout->addWidget(m_modelTree);
    modelLayout->addLayout(selectionLayout);

    // Statistics label
    m_statisticsLabel = new QLabel;
    modelLayout->addWidget(m_statisticsLabel);

    mainLayout->addWidget(modelGroup);

    // Output directory group
    auto* outputGroup = new QGroupBox(tr("Output Directory"));
    auto* outputLayout = new QHBoxLayout(outputGroup);

    m_outputDirEdit = new QLineEdit;
    m_outputDirEdit->setPlaceholderText(tr("Select output directory..."));
    m_browseButton = new QPushButton(tr("Browse..."));
    outputLayout->addWidget(m_outputDirEdit);
    outputLayout->addWidget(m_browseButton);
    connect(m_browseButton, &QPushButton::clicked, this, &ExportDialog::onBrowseOutputDir);
    connect(m_outputDirEdit, &QLineEdit::textChanged, this, &ExportDialog::updateExportButton);

    mainLayout->addWidget(outputGroup);

    // Dialog buttons
    auto* buttonLayout = new QHBoxLayout;
    buttonLayout->addStretch();
    m_exportButton = new QPushButton(tr("Export"));
    m_cancelButton = new QPushButton(tr("Cancel"));
    m_exportButton->setDefault(true);
    buttonLayout->addWidget(m_exportButton);
    buttonLayout->addWidget(m_cancelButton);
    connect(m_exportButton, &QPushButton::clicked, this, &ExportDialog::onExport);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);

    mainLayout->addLayout(buttonLayout);
}

void ExportDialog::populateModelList()
{
    m_modelInfos.clear();
    m_modelTree->clear();

    if (!m_viewport)
        return;

    int meshCount = m_viewport->meshCount();
    for (int i = 0; i < meshCount; ++i)
    {
        const MeshInstance* instance = m_viewport->getMeshInstance(i);
        if (!instance || !instance->mesh)
            continue;

        ExportModelInfo info;
        info.meshIndex = i;
        info.faceCount = instance->mesh->faceCount();
        info.selected = true;  // Default to selected

        // Extract name from source path
        if (!instance->mesh->sourcePath.empty())
        {
            QFileInfo fileInfo(QString::fromStdString(instance->mesh->sourcePath));
            info.name = fileInfo.baseName();
        }
        else
        {
            info.name = tr("Model %1").arg(i + 1);
        }

        m_modelInfos.push_back(info);

        // Create tree item
        auto* item = new QTreeWidgetItem;
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(0, Qt::Checked);
        item->setText(0, info.name);
        item->setText(1, QString::number(info.faceCount));
        item->setData(0, Qt::UserRole, i);  // Store mesh index
        m_modelTree->addTopLevelItem(item);
    }
}

void ExportDialog::onBrowseOutputDir()
{
    QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Output Directory"),
        m_outputDirEdit->text().isEmpty() ? QDir::homePath() : m_outputDirEdit->text()
    );

    if (!dir.isEmpty())
    {
        m_outputDirEdit->setText(dir);
    }
}

void ExportDialog::onSelectAll()
{
    m_modelTree->blockSignals(true);
    for (int i = 0; i < m_modelTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = m_modelTree->topLevelItem(i);
        item->setCheckState(0, Qt::Checked);
    }
    m_modelTree->blockSignals(false);

    for (auto& info : m_modelInfos)
    {
        info.selected = true;
    }
    updateStatistics();
    updateExportButton();
}

void ExportDialog::onDeselectAll()
{
    m_modelTree->blockSignals(true);
    for (int i = 0; i < m_modelTree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = m_modelTree->topLevelItem(i);
        item->setCheckState(0, Qt::Unchecked);
    }
    m_modelTree->blockSignals(false);

    for (auto& info : m_modelInfos)
    {
        info.selected = false;
    }
    updateStatistics();
    updateExportButton();
}

void ExportDialog::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (column != 0)
        return;

    int meshIndex = item->data(0, Qt::UserRole).toInt();
    bool checked = (item->checkState(0) == Qt::Checked);

    // Update model info
    for (auto& info : m_modelInfos)
    {
        if (info.meshIndex == meshIndex)
        {
            info.selected = checked;
            break;
        }
    }

    updateStatistics();
    updateExportButton();
}

void ExportDialog::updateStatistics()
{
    int selectedCount = 0;
    int totalFaces = 0;

    for (const auto& info : m_modelInfos)
    {
        if (info.selected)
        {
            selectedCount++;
            totalFaces += info.faceCount;
        }
    }

    m_statisticsLabel->setText(
        tr("Selected: %1 models, %2 faces")
            .arg(selectedCount)
            .arg(totalFaces)
    );
}

void ExportDialog::updateExportButton()
{
    bool hasSelection = false;
    for (const auto& info : m_modelInfos)
    {
        if (info.selected)
        {
            hasSelection = true;
            break;
        }
    }

    bool hasOutputDir = !m_outputDirEdit->text().isEmpty();

    m_exportButton->setEnabled(hasSelection && hasOutputDir);
}

void ExportDialog::onExport()
{
    accept();
}

std::vector<int> ExportDialog::getSelectedModelIndices() const
{
    std::vector<int> indices;
    for (const auto& info : m_modelInfos)
    {
        if (info.selected)
        {
            indices.push_back(info.meshIndex);
        }
    }
    return indices;
}

QString ExportDialog::getOutputDirectory() const
{
    return m_outputDirEdit->text();
}

} // namespace MoldWing
