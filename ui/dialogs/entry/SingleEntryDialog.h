#ifndef SINGLEENTRYDIALOG_H
#define SINGLEENTRYDIALOG_H

#include "NumericInputField.h"

#include <QDialog>
#include <QStackedWidget>
#include <QTableWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QVBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QHash>

class SingleEntryDialog : public QDialog
{
    Q_OBJECT

public:
    explicit SingleEntryDialog(QWidget* parent = nullptr);
    ~SingleEntryDialog();

signals:
    void dataSaved();

private slots:
    void onPreviousClicked();
    void onNextClicked();
    void onSaveClicked();
    void onCancelClicked();
    void onAddInclineRow();
    void onAddStrataRow();
    void onAddSampleRow();
    void onAddElementRow();
    void onDeleteSelectedRow();
    void onAddExtensionField();
    void onDeleteExtensionField();
    void onStepTabClicked();
    void onStepChanged(int index);
    void onFormChanged();

private:
    void setupUI();
    QWidget* createBasicInfoPage();
    QWidget* createInclinePage();
    QWidget* createStrataPage();
    QWidget* createSamplePage();
    void appendExtensionFieldsSection(QVBoxLayout* layout,
                                      QTableWidget** tableOut,
                                      const QString& hintText,
                                      const QString& groupTitle = QString());
    void switchToStep(int index);
    void updateStepIndicator();
    void updateNavigationButtons();
    void updateSaveButtonState();
    void updateLinkedBoreholeLabels();
    void markStepSaved(int stepIndex);
    void appendAreaIdArg(QStringList* args) const;
    void appendNumericArgIfFilled(QStringList* args,
                                  const QString& flag,
                                  const NumericInputField* field) const;
    void appendOptionalNumericArg(QStringList* args,
                                  const QString& flag,
                                  const NumericInputField* field) const;

    bool validateBasicInfoForSave() const;
    bool validateBasicInfoPartialForSave() const;
    bool ensureBoreholeIdForLinkedSave() const;
    QString basicInfoMissingReason() const;
    QString basicInfoInvalidNumericReason() const;
    QString basicInfoAllZeroCoordsReason() const;
    QStringList buildBasicInfoArgs() const;
    bool hasInclineData() const;
    bool hasStrataData() const;
    bool hasSampleData() const;

    void saveBasicInfo();
    void saveInclineData();
    void saveStrataData();
    void saveSampleData();

    bool resolveConflictsAndSave(const QString& scriptFileName,
                                 const QStringList& args,
                                 int stepIndex,
                                 const QString& successFallback,
                                 const QJsonArray* prefetchedConflicts = nullptr);
    bool runPythonAnalyze(const QString& scriptFileName,
                          const QStringList& args,
                          QJsonArray* conflicts,
                          QString* errorMessage,
                          QJsonObject* analyzeMetadata = nullptr);
    bool runPythonSave(const QString& scriptFileName,
                       const QStringList& args,
                       const QString& fallbackMessage,
                       int stepIndex);
    QString writeConflictResolutionsFile(const QHash<QString, QString>& resolutions) const;

    QJsonObject buildExtraDataObject(QTableWidget* table) const;
    QString extraDataJsonForTable(QTableWidget* table) const;
    QTableWidget* extensionTableForStep(int stepIndex) const;

    QJsonArray buildInclineJson() const;
    QJsonArray buildStrataJson() const;
    QJsonObject buildSamplePayload() const;
    QString tableCellText(QTableWidget* table, int row, int col) const;
    int findSampleHeadRow(int row) const;
    int findSampleGroupEndRow(int headRow) const;
    void stashActiveSampleExtraFields();
    void syncSampleExtraEditorForRow(int row);
    void loadExtraFieldsToTable(QTableWidget* table, const QJsonObject& extra) const;

    QStackedWidget* m_stackedWidget;
    QPushButton* m_stepTabs[4];
    QPushButton* m_btnPrevious;
    QPushButton* m_btnNext;
    QPushButton* m_btnSave;
    QPushButton* m_btnCancel;

    QLineEdit* m_editBoreholeId;
    QLineEdit* m_editAreaId;
    NumericInputField* m_numX;
    NumericInputField* m_numY;
    NumericInputField* m_numZ;
    NumericInputField* m_numTotalDepth;
    NumericInputField* m_numDipAngle;
    NumericInputField* m_numAzimuth;

    QLabel* m_inclineBoreholeLabel;
    QLabel* m_strataBoreholeLabel;
    QLabel* m_sampleBoreholeLabel;

    QTableWidget* m_inclineTable;
    QTableWidget* m_strataTable;
    QTableWidget* m_sampleTable;

    QTableWidget* m_basicExtraTable;
    QTableWidget* m_inclineExtraTable;
    QTableWidget* m_strataExtraTable;
    QTableWidget* m_sampleExtraTable;
    QLabel* m_sampleExtraScopeLabel = nullptr;
    QHash<QString, QJsonObject> m_sampleExtraById;
    QString m_activeSampleExtraId;

    bool m_stepSaved[4];
};

#endif // SINGLEENTRYDIALOG_H
