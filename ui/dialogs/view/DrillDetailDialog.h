#ifndef DRILLDETAILDIALOG_H
#define DRILLDETAILDIALOG_H

#include <QDialog>
#include <QHash>
#include <QString>
#include <QVector>

class QTabWidget;
class QTableWidget;
class QLabel;
class QWidget;
class QGridLayout;

struct DrillHoleDetailRecord;
struct InclineDetailRecord;
struct StrataDetailRecord;
struct SampleDetailRecord;
struct GradeDetailRecord;

class DrillDetailDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DrillDetailDialog(const QString& boreholeId, QWidget* parent = nullptr);

private:
    void setupUI();
    bool loadData();
    QWidget* createBasicInfoTab(const DrillHoleDetailRecord& basic);
    QWidget* createInclineTab(const QVector<InclineDetailRecord>& records);
    QWidget* createStrataTab(const QVector<StrataDetailRecord>& records);
    QWidget* createSampleTab(const QVector<SampleDetailRecord>& samples,
                             const QHash<QString, QVector<GradeDetailRecord>>& gradesBySampleId);
    void setupReadOnlyTable(QTableWidget* table);
    void populateGradeTable(const QString& sampleId);
    void populateSampleExtraData(int sampleRow);
    void populateGradeExtraData(int gradeRow);
    QWidget* createEmptyStateWidget(const QString& message);

    QString m_boreholeId;
    QTabWidget* m_tabWidget = nullptr;
    QTableWidget* m_sampleTable = nullptr;
    QTableWidget* m_gradeTable = nullptr;
    QLabel* m_gradeSectionLabel = nullptr;
    QWidget* m_gradeContainer = nullptr;
    QLabel* m_gradeEmptyLabel = nullptr;
    QGridLayout* m_sampleExtraGrid = nullptr;
    QGridLayout* m_gradeExtraGrid = nullptr;
    QVector<SampleDetailRecord> m_samples;
    QVector<GradeDetailRecord> m_currentGrades;
    QHash<QString, QVector<GradeDetailRecord>> m_gradesBySampleId;
};

#endif // DRILLDETAILDIALOG_H
