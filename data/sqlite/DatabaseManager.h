#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QString>
#include <QVector>
#include <QHash>
#include <QVariant>

struct DrillHoleRecord
{
    QString boreholeId;
    QString areaId;
    double xCoord = 0.0;
    double yCoord = 0.0;
    double zCoord = 0.0;
    double totalDepth = 0.0;
    double azimuth = 0.0;
    double dipAngle = 0.0;
    QString importTime;
};

struct DrillHoleDetailRecord
{
    QString boreholeId;
    QString areaId;
    QVariant xCoord;
    QVariant yCoord;
    QVariant zCoord;
    QVariant totalDepth;
    QVariant azimuth;
    QVariant dipAngle;
    QString extraDataJson;
};

struct InclineDetailRecord
{
    int pointId = 0;
    QVariant pointDepth;
    QVariant deviationAngle;
    QVariant azimuth;
    QString extraDataJson;
};

struct StrataDetailRecord
{
    int layerOrder = 0;
    QString layerNo;
    QVariant bottomDepth;
    QString rockName;
    QVariant dipAngle;
    QString extraDataJson;
};

struct SampleDetailRecord
{
    QString sampleId;
    QVariant startDepth;
    QVariant endDepth;
    QVariant sampleLength;
    QVariant sampleType;
    QString extraDataJson;
};

struct GradeDetailRecord
{
    QString elementName;
    QVariant gradeValue;
    QString extraDataJson;
};

struct DrillHoleFullDetail
{
    DrillHoleDetailRecord basic;
    QVector<InclineDetailRecord> inclines;
    QVector<StrataDetailRecord> strata;
    QVector<SampleDetailRecord> samples;
    QHash<QString, QVector<GradeDetailRecord>> gradesBySampleId;
};

class DatabaseManager
{
public:
    static DatabaseManager& instance();

    bool open(const QString& dbPath);
    void close();
    bool isOpen() const;
    QString lastError() const;

    QVector<DrillHoleRecord> fetchAllDrillHoles();
    bool fetchDrillHoleDetail(const QString& boreholeId, DrillHoleFullDetail* detail);
    QString fetchAreaIdForBorehole(const QString& boreholeId) const;

private:
    DatabaseManager() = default;
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    QString m_connectionName;
    QString m_lastError;
    bool m_isOpen = false;
};

#endif // DATABASEMANAGER_H
