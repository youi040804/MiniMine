#ifndef BATCHIMPORTDIALOG_H
#define BATCHIMPORTDIALOG_H

#include <QGroupBox>
#include <QDialog>
#include <QShowEvent>
#include <QComboBox>
#include <QTableWidget>
#include <QProgressBar>
#include <QLabel>
#include <QPushButton>
#include <QHash>
#include <QJsonArray>
#include <QJsonObject>

#include "FieldMappingHelper.h"

class BatchImportDialog : public QDialog
{
    Q_OBJECT

public:
    explicit BatchImportDialog(QWidget* parent = nullptr);

protected:
    void showEvent(QShowEvent* event) override;

signals:
    void importCompleted(int totalCount,
                         int insertedCount,
                         int overwrittenCount,
                         int mergedCount,
                         int skippedCount,
                         int failedCount,
                         const QString& targetTable,
                         const QJsonArray& errorRecords,
                         const QJsonObject& sampleRenameInfo,
                         const QString& logPath);

private slots:
    void onSelectFileClicked();
    void onTargetTableChanged(int index);
    void onSaveMappingClicked();
    void onLoadMappingClicked();
    void onStartImportClicked();
    void onCancelClicked();
    void onDownloadLogClicked();
    void onCleanLogsClicked();

private:
    void setupUI();
    void resetProgress();
    void updateLogActionButtons();
    bool copyLogToUserPath(const QString& defaultFileName) const;
    int cleanAllImportLogs() const;
    void updateResultLabel(int totalCount,
                           int insertedCount,
                           int overwrittenCount,
                           int mergedCount,
                           int skippedCount,
                           int failedCount);
    bool resolveImportConflicts(const QString& mappingPath, QHash<QString, QString>* resolutions);
    QString writeConflictResolutionsFile(const QHash<QString, QString>& resolutions) const;
    bool readFileColumns(const QString& filePath, QStringList* columns, QString* errorMessage);
    void rebuildMappingTable(const QStringList& fileColumns);
    bool applyFileColumnsWithMatchCheck(const QStringList& fileColumns, bool isTargetTableChange);
    void updateImportButtonState();
    FieldMatchDegreeResult currentFieldMatchDegreeResult() const;
    void applySavedMapping(const QHash<QString, QString>& savedMapping);
    void applyLoadedMappingProfile(const MappingProfile& profile);
    QStringList currentFileColumns() const;
    MappingProfile currentMappingProfile() const;
    QHash<QString, QString> currentColumnMapping() const;
    QString currentTargetTable() const;
    bool validateCriticalMapping() const;
    void updateCriticalFieldStatus();
    bool areKeyFieldsMapped(QStringList* missingKeyLabels) const;
    void rebuildGradeElementTable();
    void adjustGradeElementTableHeight();
    void adjustMappingTableHeight();
    void ensureFitsScreen();
    QHash<QString, QString> currentGradeElementMapping() const;
    QStringList columnsMappedTo(const QString& systemField) const;
    bool confirmLoadMappingProfile(const MappingProfile& profile, const QString& configName);
    bool loadMappingConfigAtPath(const QString& filePath, bool interactiveConfirm);
    void tryAutoLoadMappingConfig();
    bool showLoadMappingDialog(SavedMappingConfigSummary* selectedSummary);
    QString writeTemporaryMappingFile() const;
    QComboBox* mappingComboBox(int row) const;

    QLabel* m_filePathLabel;
    QComboBox* m_targetTableCombo;
    QLabel* m_importStatusLabel;
    QGroupBox* m_gradeElementGroup;
    QTableWidget* m_gradeElementTable;
    QTableWidget* m_mappingTable;
    QProgressBar* m_progressBar;
    QLabel* m_resultLabel;
    QPushButton* m_btnStartImport;
    QPushButton* m_btnSelectFile;
    QPushButton* m_btnDownloadLog;
    QPushButton* m_btnCleanLogs;

    QString m_selectedFilePath;
    QStringList m_currentFileColumns;
    QString m_lastLogPath;
    int m_lastTargetTableIndex = 0;
    int m_matchDegreePercent = 0;
};

#endif // BATCHIMPORTDIALOG_H
