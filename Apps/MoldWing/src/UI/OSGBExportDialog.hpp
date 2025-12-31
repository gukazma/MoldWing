#pragma once

#include "IO/OSGBExporter.hpp"

#include <QDialog>
#include <QTreeWidget>
#include <QComboBox>
#include <QLineEdit>
#include <QCheckBox>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>

namespace MoldWing {

class DiligentWidget;

struct OSGBExportModelInfo
{
    int meshIndex;
    QString name;
    uint32_t faceCount;
    bool selected;
};

class OSGBExportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit OSGBExportDialog(DiligentWidget* viewport, QWidget* parent = nullptr);

    OSGBExportOptions getExportOptions() const;
    std::vector<int> getSelectedModelIndices() const;
    QString getOutputDirectory() const;

private slots:
    void onBrowseOutputDir();
    void onSelectAll();
    void onDeselectAll();
    void onItemChanged(QTreeWidgetItem* item, int column);
    void updateStatistics();
    void onSourceEpsgChanged(int index);

private:
    void setupUI();
    void populateModelList();
    void populateCoordinateSystems();

    DiligentWidget* m_viewport;
    std::vector<OSGBExportModelInfo> m_modelInfos;

    QTreeWidget* m_modelTree;
    QComboBox* m_sourceEpsgCombo;
    QComboBox* m_targetEpsgCombo;
    QLineEdit* m_customEpsgEdit;
    QDoubleSpinBox* m_originXSpin;
    QDoubleSpinBox* m_originYSpin;
    QDoubleSpinBox* m_originZSpin;
    QCheckBox* m_generateLODCheck;
    QSpinBox* m_lodLevelsSpin;
    QDoubleSpinBox* m_lodRatio1Spin;
    QDoubleSpinBox* m_lodRatio2Spin;
    QDoubleSpinBox* m_lodRatio3Spin;
    QLineEdit* m_outputDirEdit;
    QPushButton* m_browseButton;
    QLabel* m_statisticsLabel;
    QPushButton* m_exportButton;
    QPushButton* m_cancelButton;
};

}
