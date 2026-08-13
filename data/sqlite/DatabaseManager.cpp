#include "DatabaseManager.h"

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QUuid>

DatabaseManager& DatabaseManager::instance()
{
    static DatabaseManager manager;
    return manager;
}

bool DatabaseManager::open(const QString& dbPath)
{
    if (m_isOpen) {
        return true;
    }

    m_connectionName = QStringLiteral("minimine_") + QUuid::createUuid().toString(QUuid::WithoutBraces);
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    db.setDatabaseName(dbPath);

    if (!db.open()) {
        m_lastError = db.lastError().text();
        QSqlDatabase::removeDatabase(m_connectionName);
        m_connectionName.clear();
        return false;
    }

    QSqlQuery pragma(db);
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=30000"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

    QSqlQuery columnCheck(db);
    if (columnCheck.exec(QStringLiteral("PRAGMA table_info(DrillHoleInfo)"))) {
        bool hasImportTime = false;
        while (columnCheck.next()) {
            if (columnCheck.value(1).toString().compare(QStringLiteral("import_time"),
                                                         Qt::CaseInsensitive) == 0) {
                hasImportTime = true;
                break;
            }
        }
        if (!hasImportTime) {
            QSqlQuery alter(db);
            alter.exec(QStringLiteral("ALTER TABLE DrillHoleInfo ADD COLUMN import_time TEXT"));
        }
    }

    m_isOpen = true;
    m_lastError.clear();
    return true;
}

void DatabaseManager::close()
{
    if (!m_isOpen) {
        return;
    }

    {
        QSqlDatabase db = QSqlDatabase::database(m_connectionName);
        if (db.isOpen()) {
            db.close();
        }
    }

    QSqlDatabase::removeDatabase(m_connectionName);
    m_connectionName.clear();
    m_isOpen = false;
}

bool DatabaseManager::isOpen() const
{
    return m_isOpen;
}

QString DatabaseManager::lastError() const
{
    return m_lastError;
}

QVector<DrillHoleRecord> DatabaseManager::fetchAllDrillHoles()
{
    QVector<DrillHoleRecord> records;

    if (!m_isOpen) {
        m_lastError = QStringLiteral("数据库未连接");
        return records;
    }

    QSqlQuery query(QSqlDatabase::database(m_connectionName));
    const QString sql = QStringLiteral(
        "SELECT borehole_id, area_id, x_coord, y_coord, z_coord, "
        "total_depth, azimuth, dip_angle, import_time "
        "FROM DrillHoleInfo ORDER BY borehole_id");

    if (!query.exec(sql)) {
        m_lastError = query.lastError().text();
        return records;
    }

    while (query.next()) {
        DrillHoleRecord record;
        record.boreholeId = query.value(0).toString();
        record.areaId = query.value(1).toString();
        record.xCoord = query.value(2).toDouble();
        record.yCoord = query.value(3).toDouble();
        record.zCoord = query.value(4).toDouble();
        record.totalDepth = query.value(5).toDouble();
        record.azimuth = query.value(6).toDouble();
        record.dipAngle = query.value(7).toDouble();
        record.importTime = query.value(8).toString();
        records.append(record);
    }

    m_lastError.clear();
    return records;
}

QString DatabaseManager::fetchAreaIdForBorehole(const QString& boreholeId) const
{
    if (!m_isOpen) {
        return QString();
    }

    const QString trimmedId = boreholeId.trimmed();
    if (trimmedId.isEmpty()) {
        return QString();
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT area_id FROM DrillHoleInfo WHERE borehole_id = ?"));
    query.addBindValue(trimmedId);

    if (!query.exec() || !query.next()) {
        return QString();
    }

    const QVariant value = query.value(0);
    if (value.isNull()) {
        return QString();
    }

    return value.toString().trimmed();
}

bool DatabaseManager::fetchDrillHoleDetail(const QString& boreholeId, DrillHoleFullDetail* detail)
{
    if (!detail) {
        m_lastError = QStringLiteral("内部错误：输出参数为空");
        return false;
    }

    detail->basic = {};
    detail->inclines.clear();
    detail->strata.clear();
    detail->samples.clear();
    detail->gradesBySampleId.clear();

    if (!m_isOpen) {
        m_lastError = QStringLiteral("数据库未连接");
        return false;
    }

    const QString trimmedId = boreholeId.trimmed();
    if (trimmedId.isEmpty()) {
        m_lastError = QStringLiteral("钻孔编号为空");
        return false;
    }

    QSqlDatabase db = QSqlDatabase::database(m_connectionName);
    QSqlQuery query(db);
    query.prepare(QStringLiteral(
        "SELECT area_id, x_coord, y_coord, z_coord, total_depth, azimuth, dip_angle, extra_data "
        "FROM DrillHoleInfo WHERE borehole_id = ?"));
    query.addBindValue(trimmedId);

    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }

    if (!query.next()) {
        m_lastError = QStringLiteral("未找到钻孔 %1").arg(trimmedId);
        return false;
    }

    detail->basic.boreholeId = trimmedId;
    detail->basic.areaId = query.value(0).toString();
    detail->basic.xCoord = query.value(1);
    detail->basic.yCoord = query.value(2);
    detail->basic.zCoord = query.value(3);
    detail->basic.totalDepth = query.value(4);
    detail->basic.azimuth = query.value(5);
    detail->basic.dipAngle = query.value(6);
    detail->basic.extraDataJson = query.value(7).toString();

    query.prepare(QStringLiteral(
        "SELECT point_id, point_depth, deviation_angle, azimuth, extra_data "
        "FROM InclineInfo WHERE borehole_id = ? ORDER BY point_id"));
    query.addBindValue(trimmedId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    while (query.next()) {
        InclineDetailRecord record;
        record.pointId = query.value(0).toInt();
        record.pointDepth = query.value(1);
        record.deviationAngle = query.value(2);
        record.azimuth = query.value(3);
        record.extraDataJson = query.value(4).toString();
        detail->inclines.append(record);
    }

    query.prepare(QStringLiteral(
        "SELECT layer_order, layer_no, bottom_depth, rock_name, dip_angle, extra_data "
        "FROM StrataInfo WHERE borehole_id = ? ORDER BY layer_order"));
    query.addBindValue(trimmedId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    while (query.next()) {
        StrataDetailRecord record;
        record.layerOrder = query.value(0).toInt();
        record.layerNo = query.value(1).toString();
        record.bottomDepth = query.value(2);
        record.rockName = query.value(3).toString();
        record.dipAngle = query.value(4);
        record.extraDataJson = query.value(5).toString();
        detail->strata.append(record);
    }

    query.prepare(QStringLiteral(
        "SELECT sample_id, start_depth, end_depth, sample_length, sample_type, extra_data "
        "FROM SampleRecord WHERE borehole_id = ? ORDER BY start_depth, sample_id"));
    query.addBindValue(trimmedId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    while (query.next()) {
        SampleDetailRecord record;
        record.sampleId = query.value(0).toString();
        record.startDepth = query.value(1);
        record.endDepth = query.value(2);
        record.sampleLength = query.value(3);
        record.sampleType = query.value(4);
        record.extraDataJson = query.value(5).toString();
        detail->samples.append(record);
    }

    query.prepare(QStringLiteral(
        "SELECT g.sample_id, g.element_name, g.grade_value, g.extra_data "
        "FROM GradeInfo g "
        "INNER JOIN SampleRecord s ON s.sample_id = g.sample_id "
        "WHERE s.borehole_id = ? "
        "ORDER BY g.sample_id, g.element_name"));
    query.addBindValue(trimmedId);
    if (!query.exec()) {
        m_lastError = query.lastError().text();
        return false;
    }
    while (query.next()) {
        const QString sampleId = query.value(0).toString();
        GradeDetailRecord record;
        record.elementName = query.value(1).toString();
        record.gradeValue = query.value(2);
        record.extraDataJson = query.value(3).toString();
        detail->gradesBySampleId[sampleId].append(record);
    }

    m_lastError.clear();
    return true;
}
