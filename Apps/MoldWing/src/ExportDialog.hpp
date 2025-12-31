/*
 *  MoldWing - Export Dialog
 *  B6: Model selection export dialog
 */

#pragma once

#include <QDialog>
#include <QStringList>
#include <vector>

class QTreeWidget;
class QTreeWidgetItem;
class QLineEdit;
class QPushButton;
class QLabel;

namespace MoldWing
{

class DiligentWidget;

// Export model info
struct ExportModelInfo
{
    int meshIndex;        // Index in DiligentWidget
    QString name;         // Model name (from filename)
    int faceCount;        // Number of faces
    bool selected;        // Export checkbox state
};

class ExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDialog(DiligentWidget* viewport, QWidget* parent = nullptr);
    ~ExportDialog() override;

    // Get selected model indices for export
    std::vector<int> getSelectedModelIndices() const;

    // Get output directory
    QString getOutputDirectory() const;

private slots:
    void onBrowseOutputDir();
    void onSelectAll();
    void onDeselectAll();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void updateExportButton();
    void onExport();

private:
    void setupUI();
    void populateModelList();
    void updateStatistics();

    DiligentWidget* m_viewport;

    // UI elements
    QTreeWidget* m_modelTree;
    QLineEdit* m_outputDirEdit;
    QPushButton* m_browseButton;
    QPushButton* m_selectAllButton;
    QPushButton* m_deselectAllButton;
    QPushButton* m_exportButton;
    QPushButton* m_cancelButton;
    QLabel* m_statisticsLabel;

    // Model data
    std::vector<ExportModelInfo> m_modelInfos;
};

} // namespace MoldWing
