#include "FieldMappingHelper.h"
#include "AppConfig.h"

#include <QDir>
#include <QFileInfo>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QDateTime>
#include <QSet>
#include <QList>
#include <algorithm>
#include <utility>

namespace {

const QHash<QString, QStringList> kFieldAliases = {
    { QStringLiteral("borehole_id"), {
        QStringLiteral("钻孔编号"), QStringLiteral("钻孔号"), QStringLiteral("孔号"),
        QStringLiteral("钻孔ID"), QStringLiteral("井号"), QStringLiteral("井ID"),
        QStringLiteral("钻孔"), QStringLiteral("BoreholeID"), QStringLiteral("HoleID"),
        QStringLiteral("BHID"), QStringLiteral("HOLE_ID"), QStringLiteral("BOREHOLE_ID"),
        QStringLiteral("GCJCBN"), QStringLiteral("borehole_id")
    } },
    { QStringLiteral("area_id"), {
        QStringLiteral("勘探区编号"), QStringLiteral("矿区编号"), QStringLiteral("工区编号"),
        QStringLiteral("区域编号"), QStringLiteral("勘探区"), QStringLiteral("矿区"),
        QStringLiteral("工区"), QStringLiteral("AreaID"), QStringLiteral("AREA_ID"),
        QStringLiteral("DISTRICT"), QStringLiteral("MDBTAD"), QStringLiteral("area_id")
    } },
    { QStringLiteral("x_coord"), {
        QStringLiteral("X坐标"), QStringLiteral("坐标X"), QStringLiteral("北向值"),
        QStringLiteral("北坐标"), QStringLiteral("N坐标"), QStringLiteral("Easting"),
        QStringLiteral("XCoord"), QStringLiteral("X_COORD"), QStringLiteral("COORD_X"),
        QStringLiteral("X"), QStringLiteral("N"), QStringLiteral("x_coord")
    } },
    { QStringLiteral("y_coord"), {
        QStringLiteral("Y坐标"), QStringLiteral("坐标Y"), QStringLiteral("东向值"),
        QStringLiteral("东坐标"), QStringLiteral("E坐标"), QStringLiteral("Northing"),
        QStringLiteral("YCoord"), QStringLiteral("Y_COORD"), QStringLiteral("COORD_Y"),
        QStringLiteral("Y"), QStringLiteral("E"), QStringLiteral("y_coord")
    } },
    { QStringLiteral("z_coord"), {
        QStringLiteral("Z坐标"), QStringLiteral("坐标Z"), QStringLiteral("海拔高"),
        QStringLiteral("高程"), QStringLiteral("标高"), QStringLiteral("孔口高程"),
        QStringLiteral("Elevation"), QStringLiteral("ZCoord"), QStringLiteral("Z_COORD"),
        QStringLiteral("COORD_Z"), QStringLiteral("ALTITUDE"), QStringLiteral("Z"),
        QStringLiteral("z_coord")
    } },
    { QStringLiteral("total_depth"), {
        QStringLiteral("终孔深度"), QStringLiteral("孔深"), QStringLiteral("总深"),
        QStringLiteral("总钻深"), QStringLiteral("钻孔深度"), QStringLiteral("深度"),
        QStringLiteral("Depth"), QStringLiteral("TOTAL_DEPTH"), QStringLiteral("TD"),
        QStringLiteral("HOLE_DEPTH"), QStringLiteral("total_depth")
    } },
    { QStringLiteral("azimuth"), {
        QStringLiteral("钻孔方位角"), QStringLiteral("方位角"), QStringLiteral("方位角采用值"),
        QStringLiteral("方向角"), QStringLiteral("方位"), QStringLiteral("Azimuth"),
        QStringLiteral("AZIMUTH"), QStringLiteral("BEARING"), QStringLiteral("WTHGFF"),
        QStringLiteral("azimuth")
    } },
    { QStringLiteral("dip_angle"), {
        QStringLiteral("钻孔倾角"), QStringLiteral("岩层倾角"), QStringLiteral("倾角"),
        QStringLiteral("倾斜角"), QStringLiteral("DipAngle"), QStringLiteral("DIP_ANGLE"),
        QStringLiteral("DIP"), QStringLiteral("INCLINATION"), QStringLiteral("dip_angle")
    } },
    { QStringLiteral("point_id"), {
        QStringLiteral("测点号"), QStringLiteral("测点编号"), QStringLiteral("测点"),
        QStringLiteral("点号"), QStringLiteral("PointID"), QStringLiteral("POINT_ID"),
        QStringLiteral("MEASURE_POINT"), QStringLiteral("WTJDIH"), QStringLiteral("point_id")
    } },
    { QStringLiteral("point_depth"), {
        QStringLiteral("测点深度"), QStringLiteral("测量深度"), QStringLiteral("深度"),
        QStringLiteral("PointDepth"), QStringLiteral("POINT_DEPTH"),
        QStringLiteral("MEASURED_DEPTH"), QStringLiteral("WTHEBA"), QStringLiteral("point_depth")
    } },
    { QStringLiteral("deviation_angle"), {
        QStringLiteral("偏斜角"), QStringLiteral("偏斜角采用值"), QStringLiteral("顶角"),
        QStringLiteral("偏角"), QStringLiteral("DeviationAngle"), QStringLiteral("DEVIATION_ANGLE"),
        QStringLiteral("DEVIATION"), QStringLiteral("WTHGFC"), QStringLiteral("deviation_angle")
    } },
    { QStringLiteral("layer_order"), {
        QStringLiteral("岩层序号"), QStringLiteral("层序"), QStringLiteral("层号"),
        QStringLiteral("地层序号"), QStringLiteral("LayerOrder"), QStringLiteral("LAYER_ORDER"),
        QStringLiteral("STRATA_SEQ"), QStringLiteral("layer_order")
    } },
    { QStringLiteral("layer_no"), {
        QStringLiteral("分层号"), QStringLiteral("分层编号"), QStringLiteral("LayerNo"),
        QStringLiteral("LAYER_NO"), QStringLiteral("layer_no")
    } },
    { QStringLiteral("bottom_depth"), {
        QStringLiteral("岩石分层孔深"), QStringLiteral("分层底深"), QStringLiteral("底深"),
        QStringLiteral("层底深度"), QStringLiteral("BottomDepth"), QStringLiteral("BOTTOM_DEPTH"),
        QStringLiteral("bottom_depth")
    } },
    { QStringLiteral("rock_name"), {
        QStringLiteral("岩石全名"), QStringLiteral("岩石名称"), QStringLiteral("岩性"),
        QStringLiteral("RockName"), QStringLiteral("ROCK_NAME"), QStringLiteral("LITHOLOGY"),
        QStringLiteral("rock_name")
    } },
    { QStringLiteral("sample_id"), {
        QStringLiteral("样品编号"), QStringLiteral("样号"), QStringLiteral("样品号"),
        QStringLiteral("样品ID"), QStringLiteral("SampleID"), QStringLiteral("SAMPLE_ID"),
        QStringLiteral("sample_id")
    } },
    { QStringLiteral("start_depth"), {
        QStringLiteral("采样起始孔深"), QStringLiteral("起始深度"), QStringLiteral("采样起始深度"),
        QStringLiteral("开始深度"), QStringLiteral("采样起始"), QStringLiteral("StartDepth"),
        QStringLiteral("START_DEPTH"), QStringLiteral("start_depth")
    } },
    { QStringLiteral("end_depth"), {
        QStringLiteral("采样终止孔深"), QStringLiteral("终止深度"), QStringLiteral("采样终止深度"),
        QStringLiteral("结束深度"), QStringLiteral("采样终止"), QStringLiteral("EndDepth"),
        QStringLiteral("END_DEPTH"), QStringLiteral("end_depth")
    } },
    { QStringLiteral("sample_length"), {
        QStringLiteral("样长"), QStringLiteral("样品长度"), QStringLiteral("长度"),
        QStringLiteral("SampleLength"), QStringLiteral("SAMPLE_LENGTH"), QStringLiteral("LENGTH"),
        QStringLiteral("sample_length")
    } },
    { QStringLiteral("core_length"), {
        QStringLiteral("岩矿心长度"), QStringLiteral("岩心长度"), QStringLiteral("岩芯长度"),
        QStringLiteral("矿心长度"), QStringLiteral("CoreLength"), QStringLiteral("CORE_LENGTH"),
        QStringLiteral("GCLCAA"), QStringLiteral("core_length")
    } },
    { QStringLiteral("sample_type"), {
        QStringLiteral("样品类型"), QStringLiteral("类型"), QStringLiteral("SampleType"),
        QStringLiteral("SAMPLE_TYPE"), QStringLiteral("sample_type")
    } },
    { QStringLiteral("element_name"), {
        QStringLiteral("元素名称"), QStringLiteral("元素"), QStringLiteral("ElementName"),
        QStringLiteral("ELEMENT_NAME"), QStringLiteral("ELEMENT"), QStringLiteral("element_name")
    } },
    { QStringLiteral("grade_value"), {
        QStringLiteral("品位值"), QStringLiteral("品位"), QStringLiteral("GradeValue"),
        QStringLiteral("GRADE_VALUE"), QStringLiteral("GRADE"),
        QStringLiteral("铜品位"), QStringLiteral("锌品位"), QStringLiteral("硫品位"),
        QStringLiteral("铅品位"), QStringLiteral("铁品位"), QStringLiteral("金品位"),
        QStringLiteral("银品位"), QStringLiteral("HXD001"), QStringLiteral("HXD002"),
        QStringLiteral("HXD003"), QStringLiteral("grade_value")
    } },
};

const QHash<QString, QStringList> kTableFields = {
    { QStringLiteral("DrillHoleInfo"), { QStringLiteral("borehole_id"), QStringLiteral("area_id"), QStringLiteral("x_coord"), QStringLiteral("y_coord"), QStringLiteral("z_coord"), QStringLiteral("total_depth"), QStringLiteral("azimuth"), QStringLiteral("dip_angle") } },
    { QStringLiteral("InclineInfo"), { QStringLiteral("borehole_id"), QStringLiteral("point_id"), QStringLiteral("area_id"), QStringLiteral("point_depth"), QStringLiteral("deviation_angle"), QStringLiteral("azimuth") } },
    { QStringLiteral("StrataInfo"), { QStringLiteral("borehole_id"), QStringLiteral("layer_order"), QStringLiteral("area_id"), QStringLiteral("layer_no"), QStringLiteral("bottom_depth"), QStringLiteral("rock_name"), QStringLiteral("dip_angle") } },
    { QStringLiteral("SampleRecord"), { QStringLiteral("sample_id"), QStringLiteral("borehole_id"), QStringLiteral("area_id"), QStringLiteral("start_depth"), QStringLiteral("end_depth"), QStringLiteral("sample_length"), QStringLiteral("core_length"), QStringLiteral("sample_type") } },
    { QStringLiteral("GradeInfo"), { QStringLiteral("sample_id"), QStringLiteral("borehole_id"), QStringLiteral("element_name"), QStringLiteral("grade_value") } },
};

const QHash<QString, QStringList> kKeyFields = {
    { QStringLiteral("DrillHoleInfo"), { QStringLiteral("borehole_id") } },
    { QStringLiteral("InclineInfo"), { QStringLiteral("borehole_id"), QStringLiteral("point_id") } },
    { QStringLiteral("StrataInfo"), { QStringLiteral("borehole_id"), QStringLiteral("layer_order") } },
    { QStringLiteral("SampleRecord"), { QStringLiteral("sample_id") } },
    { QStringLiteral("GradeInfo"), { QStringLiteral("sample_id"), QStringLiteral("element_name") } },
};

const QHash<QString, QStringList> kCoreFields = {
    { QStringLiteral("DrillHoleInfo"), {
        QStringLiteral("borehole_id"), QStringLiteral("x_coord"), QStringLiteral("y_coord"),
        QStringLiteral("z_coord"), QStringLiteral("total_depth"), QStringLiteral("dip_angle"),
        QStringLiteral("azimuth"), QStringLiteral("area_id")
    } },
    { QStringLiteral("InclineInfo"), {
        QStringLiteral("borehole_id"), QStringLiteral("point_id"), QStringLiteral("point_depth"),
        QStringLiteral("deviation_angle"), QStringLiteral("azimuth")
    } },
    { QStringLiteral("StrataInfo"), {
        QStringLiteral("borehole_id"), QStringLiteral("layer_order"), QStringLiteral("bottom_depth"),
        QStringLiteral("rock_name"), QStringLiteral("dip_angle")
    } },
    { QStringLiteral("SampleRecord"), {
        QStringLiteral("sample_id"), QStringLiteral("borehole_id"), QStringLiteral("start_depth"),
        QStringLiteral("end_depth"), QStringLiteral("sample_length")
    } },
    { QStringLiteral("GradeInfo"), {
        QStringLiteral("sample_id"), QStringLiteral("element_name"), QStringLiteral("grade_value")
    } },
};

const QHash<QString, QStringList> kRequiredFields = {
    { QStringLiteral("DrillHoleInfo"), { QStringLiteral("borehole_id") } },
    { QStringLiteral("InclineInfo"), { QStringLiteral("borehole_id"), QStringLiteral("point_id") } },
    { QStringLiteral("StrataInfo"), { QStringLiteral("borehole_id"), QStringLiteral("layer_order") } },
    { QStringLiteral("SampleRecord"), { QStringLiteral("sample_id") } },
    { QStringLiteral("GradeInfo"), { QStringLiteral("sample_id"), QStringLiteral("element_name") } },
};

const QHash<QString, QString> kTableDisplayNames = {
    { QStringLiteral("DrillHoleInfo"), QStringLiteral("钻孔概况表") },
    { QStringLiteral("InclineInfo"), QStringLiteral("测斜记录表") },
    { QStringLiteral("StrataInfo"), QStringLiteral("地层分层表") },
    { QStringLiteral("SampleRecord"), QStringLiteral("样品记录表") },
    { QStringLiteral("GradeInfo"), QStringLiteral("品位信息表") },
};

const QHash<QString, QString> kFieldDisplayNames = {
    { QStringLiteral("borehole_id"), QStringLiteral("borehole_id (钻孔编号)") },
    { QStringLiteral("area_id"), QStringLiteral("area_id (勘探区编号)") },
    { QStringLiteral("x_coord"), QStringLiteral("x_coord (X坐标)") },
    { QStringLiteral("y_coord"), QStringLiteral("y_coord (Y坐标)") },
    { QStringLiteral("z_coord"), QStringLiteral("z_coord (Z坐标)") },
    { QStringLiteral("total_depth"), QStringLiteral("total_depth (终孔深度)") },
    { QStringLiteral("azimuth"), QStringLiteral("azimuth (方位角)") },
    { QStringLiteral("dip_angle"), QStringLiteral("dip_angle (倾角)") },
    { QStringLiteral("point_id"), QStringLiteral("point_id (测点号)") },
    { QStringLiteral("point_depth"), QStringLiteral("point_depth (测点深度)") },
    { QStringLiteral("deviation_angle"), QStringLiteral("deviation_angle (偏斜角采用值)") },
    { QStringLiteral("layer_order"), QStringLiteral("layer_order (岩层序号)") },
    { QStringLiteral("layer_no"), QStringLiteral("layer_no (分层号)") },
    { QStringLiteral("bottom_depth"), QStringLiteral("bottom_depth (岩石分层孔深（底深）)") },
    { QStringLiteral("rock_name"), QStringLiteral("rock_name (岩石全名)") },
    { QStringLiteral("sample_id"), QStringLiteral("sample_id (样品编号)") },
    { QStringLiteral("start_depth"), QStringLiteral("start_depth (采样起始孔深)") },
    { QStringLiteral("end_depth"), QStringLiteral("end_depth (采样终止孔深)") },
    { QStringLiteral("sample_length"), QStringLiteral("sample_length (样长)") },
    { QStringLiteral("core_length"), QStringLiteral("core_length (岩矿心长度)") },
    { QStringLiteral("sample_type"), QStringLiteral("sample_type (样品类型)") },
    { QStringLiteral("element_name"), QStringLiteral("element_name (元素名称)") },
    { QStringLiteral("grade_value"), QStringLiteral("grade_value (元素品位值)") },
};

QString tableSpecificChineseFieldLabel(const QString& targetTable, const QString& fieldName)
{
    static const QHash<QString, QHash<QString, QString>> overrides = {
        { QStringLiteral("DrillHoleInfo"), {
            { QStringLiteral("azimuth"), QStringLiteral("钻孔方位角") },
            { QStringLiteral("dip_angle"), QStringLiteral("钻孔倾角") },
        }},
        { QStringLiteral("InclineInfo"), {
            { QStringLiteral("azimuth"), QStringLiteral("方位角采用值") },
            { QStringLiteral("deviation_angle"), QStringLiteral("偏斜角采用值") },
        }},
        { QStringLiteral("StrataInfo"), {
            { QStringLiteral("dip_angle"), QStringLiteral("岩层倾角") },
        }},
    };

    const auto tableOverrides = overrides.constFind(targetTable);
    if (tableOverrides != overrides.constEnd()) {
        const QString label = tableOverrides->value(fieldName);
        if (!label.isEmpty()) {
            return label;
        }
    }
    return QString();
}

QString formatFieldDisplayLabel(const QString& fieldName, const QString& chineseLabel)
{
    return QStringLiteral("%1 (%2)").arg(fieldName, chineseLabel);
}

QString normalizeForAliasMatch(const QString& text)
{
    QString normalized;
    normalized.reserve(text.size());
    for (const QChar& ch : text) {
        if (ch.isLetterOrNumber()) {
            normalized.append(ch.toLower());
        }
    }
    return normalized;
}

int scoreFieldAliasMatch(const QString& normalizedColumn, const QString& alias)
{
    const QString normalizedAlias = normalizeForAliasMatch(alias);
    if (normalizedAlias.isEmpty() || normalizedColumn.isEmpty()) {
        return 0;
    }

    if (normalizedColumn == normalizedAlias) {
        return 100000 + static_cast<int>(normalizedAlias.size());
    }

    if (normalizedAlias.size() >= 2 && normalizedColumn.contains(normalizedAlias)) {
        return static_cast<int>(normalizedAlias.size());
    }

    return 0;
}

QSet<QString> toColumnSet(const QStringList& columns)
{
    QSet<QString> result;
    for (const QString& column : columns) {
        result.insert(column.trimmed());
    }
    return result;
}

QJsonObject profileToJson(const MappingProfile& profile)
{
    QJsonObject columnsObject;
    for (auto it = profile.columnMapping.constBegin(); it != profile.columnMapping.constEnd(); ++it) {
        if (!it.value().isEmpty()) {
            columnsObject.insert(it.key(), it.value());
        }
    }

    QJsonArray sourceColumnsArray;
    for (const QString& column : profile.sourceColumns) {
        sourceColumnsArray.append(column);
    }

    QJsonObject root;
    root.insert(QStringLiteral("config_name"), profile.configName);
    root.insert(QStringLiteral("target_table"), profile.targetTable);
    root.insert(QStringLiteral("source_file_name"), profile.sourceFileName);
    root.insert(QStringLiteral("source_columns"), sourceColumnsArray);
    root.insert(QStringLiteral("version"), profile.version.isEmpty() ? QStringLiteral("1.1") : profile.version);
    root.insert(QStringLiteral("created"), profile.created.isEmpty()
        ? QDateTime::currentDateTime().toString(Qt::ISODate)
        : profile.created);
    if (!profile.lastUsed.isEmpty()) {
        root.insert(QStringLiteral("last_used"), profile.lastUsed);
    }
    root.insert(QStringLiteral("columns"), columnsObject);

    if (!profile.gradeElementMapping.isEmpty()) {
        QJsonObject elementObject;
        for (auto it = profile.gradeElementMapping.constBegin();
             it != profile.gradeElementMapping.constEnd();
             ++it) {
            if (!it.value().isEmpty()) {
                elementObject.insert(it.key(), it.value());
            }
        }
        if (!elementObject.isEmpty()) {
            root.insert(QStringLiteral("grade_element_mapping"), elementObject);
        }
    }

    root.insert(QStringLiteral("description"),
                QStringLiteral("映射配置绑定：文件列名结构 + 目标表 + 字段映射规则。"
                               "仅当两者都匹配时才适合复用。"));
    return root;
}

QString sanitizeConfigFileStem(const QString& configName)
{
    QString stem = configName.trimmed();
    for (const QChar& ch : QStringLiteral("\\/:*?\"<>|")) {
        stem.replace(ch, QChar('_'));
    }
    return stem;
}

QString formatSummaryDateTime(const QString& isoText)
{
    if (isoText.trimmed().isEmpty()) {
        return QStringLiteral("—");
    }

    QDateTime dateTime = QDateTime::fromString(isoText, Qt::ISODate);
    if (!dateTime.isValid()) {
        return isoText;
    }
    return dateTime.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
}

QString configTimestamp(const MappingProfile& profile)
{
    if (!profile.lastUsed.trimmed().isEmpty()) {
        return profile.lastUsed;
    }
    return profile.created;
}

bool columnStructuresMatch(const QStringList& left, const QStringList& right)
{
    return toColumnSet(left) == toColumnSet(right);
}

SavedMappingConfigSummary buildConfigSummary(const QString& filePath, const MappingProfile& profile)
{
    SavedMappingConfigSummary summary;
    summary.configName = profile.configName.trimmed().isEmpty()
        ? QFileInfo(filePath).completeBaseName()
        : profile.configName.trimmed();
    summary.targetTable = profile.targetTable;
    summary.filePath = filePath;
    summary.columnCount = FieldMappingHelper::effectiveSourceColumns(profile).size();
    summary.created = profile.created;
    summary.lastUsed = profile.lastUsed;
    return summary;
}

} // namespace

QStringList FieldMappingHelper::tableNames()
{
    return {
        QStringLiteral("DrillHoleInfo"),
        QStringLiteral("InclineInfo"),
        QStringLiteral("StrataInfo"),
        QStringLiteral("SampleRecord"),
        QStringLiteral("GradeInfo")
    };
}

QString FieldMappingHelper::tableDisplayName(const QString& tableName)
{
    return kTableDisplayNames.value(tableName, tableName);
}

QStringList FieldMappingHelper::fieldsForTable(const QString& tableName)
{
    return kTableFields.value(tableName);
}

QStringList FieldMappingHelper::requiredFieldsForTable(const QString& tableName)
{
    return kRequiredFields.value(tableName);
}

QStringList FieldMappingHelper::keyFieldsForTable(const QString& tableName)
{
    return kKeyFields.value(tableName);
}

QStringList FieldMappingHelper::coreFieldsForTable(const QString& tableName)
{
    return kCoreFields.value(tableName);
}

FieldMatchDegreeResult FieldMappingHelper::computeFieldMatchDegree(
    const QString& targetTable,
    const QStringList& fileColumns)
{
    FieldMatchDegreeResult result;
    const QStringList coreFields = coreFieldsForTable(targetTable);
    result.totalCount = coreFields.size();
    if (result.totalCount == 0 || fileColumns.isEmpty()) {
        return result;
    }

    for (const QString& coreField : coreFields) {
        bool matched = false;
        for (const QString& fileColumn : fileColumns) {
            if (suggestField(fileColumn, coreFields) == coreField) {
                matched = true;
                result.matchedCoreFields.append(coreField);
                break;
            }
        }
        if (!matched) {
            result.missingCoreFields.append(coreField);
        }
    }

    result.matchedCount = result.matchedCoreFields.size();
    result.percent = (result.matchedCount * 100) / result.totalCount;
    return result;
}

FieldMatchDegreeResult FieldMappingHelper::computeFieldMatchDegreeFromMapping(
    const QString& targetTable,
    const QHash<QString, QString>& columnMapping,
    const QHash<QString, QString>& gradeElementMapping)
{
    FieldMatchDegreeResult result;
    const QStringList coreFields = coreFieldsForTable(targetTable);
    result.totalCount = coreFields.size();
    if (result.totalCount == 0) {
        return result;
    }

    QSet<QString> mappedFields;
    QStringList gradeColumns;
    for (auto it = columnMapping.constBegin(); it != columnMapping.constEnd(); ++it) {
        const QString systemField = it.value();
        if (systemField.isEmpty() || systemField == QStringLiteral("__ignore__")) {
            continue;
        }
        mappedFields.insert(systemField);
        if (targetTable == QStringLiteral("GradeInfo")
            && systemField == QStringLiteral("grade_value")) {
            gradeColumns.append(it.key());
        }
    }

    for (const QString& coreField : coreFields) {
        bool matched = false;
        if (targetTable == QStringLiteral("GradeInfo")
            && coreField == QStringLiteral("element_name")) {
            if (!gradeColumns.isEmpty()) {
                matched = true;
                for (const QString& column : gradeColumns) {
                    if (gradeElementMapping.value(column).trimmed().isEmpty()) {
                        matched = false;
                        break;
                    }
                }
            }
        } else {
            matched = mappedFields.contains(coreField);
        }

        if (matched) {
            result.matchedCoreFields.append(coreField);
        } else {
            result.missingCoreFields.append(coreField);
        }
    }

    result.matchedCount = result.matchedCoreFields.size();
    result.percent = (result.matchedCount * 100) / result.totalCount;
    return result;
}

QString FieldMappingHelper::formatCoreFieldList(const QStringList& fieldNames,
                                                const QString& targetTable)
{
    if (fieldNames.isEmpty()) {
        return QStringLiteral("（无）");
    }

    QStringList labels;
    for (const QString& fieldName : fieldNames) {
        labels.append(fieldDisplayName(fieldName, targetTable));
    }
    return labels.join(QStringLiteral("、"));
}

QString FieldMappingHelper::buildFieldMatchDegreeBlockMessage(
    const QString& targetTable,
    const FieldMatchDegreeResult& result)
{
    return QStringLiteral(
        "当前映射与目标表「%1」的核心字段匹配度过低（%2%），无法导入。\n\n"
        "已映射核心字段（%3/%4）：%5\n"
        "未映射核心字段：%6")
        .arg(tableDisplayName(targetTable))
        .arg(result.percent)
        .arg(result.matchedCount)
        .arg(result.totalCount)
        .arg(formatCoreFieldList(result.matchedCoreFields, targetTable))
        .arg(formatCoreFieldList(result.missingCoreFields, targetTable));
}

QString FieldMappingHelper::buildFieldMatchDegreeWarnMessage(
    const QString& targetTable,
    const FieldMatchDegreeResult& result)
{
    return QStringLiteral(
        "匹配度偏低（%1%），请确认目标表是否选对。\n\n"
        "目标表：%2\n"
        "已识别核心字段（%3/%4）：%5\n"
        "未能识别核心字段：%6\n\n"
        "是否仍要继续使用该目标表？")
        .arg(result.percent)
        .arg(tableDisplayName(targetTable))
        .arg(result.matchedCount)
        .arg(result.totalCount)
        .arg(formatCoreFieldList(result.matchedCoreFields, targetTable))
        .arg(formatCoreFieldList(result.missingCoreFields, targetTable));
}

QString FieldMappingHelper::buildFieldMatchDegreeImportWarnMessage(
    const QString& targetTable,
    const FieldMatchDegreeResult& result)
{
    return QStringLiteral(
        "匹配度偏低（%1%），导入前请确认映射是否正确。\n\n"
        "目标表：%2\n"
        "已映射核心字段（%3/%4）：%5\n"
        "未映射核心字段：%6\n\n"
        "是否仍要继续导入？")
        .arg(result.percent)
        .arg(tableDisplayName(targetTable))
        .arg(result.matchedCount)
        .arg(result.totalCount)
        .arg(formatCoreFieldList(result.matchedCoreFields, targetTable))
        .arg(formatCoreFieldList(result.missingCoreFields, targetTable));
}

QString FieldMappingHelper::fieldDisplayName(const QString& fieldName)
{
    return fieldDisplayName(fieldName, QString());
}

QString FieldMappingHelper::fieldDisplayName(const QString& fieldName, const QString& targetTable)
{
    if (!targetTable.isEmpty()) {
        const QString tableLabel = tableSpecificChineseFieldLabel(targetTable, fieldName);
        if (!tableLabel.isEmpty()) {
            return formatFieldDisplayLabel(fieldName, tableLabel);
        }
    }
    return kFieldDisplayNames.value(fieldName, fieldName);
}

QString FieldMappingHelper::fieldComboLabel(const QString& fieldName,
                                            bool isKeyField,
                                            const QString& targetTable)
{
    const QString base = fieldDisplayName(fieldName, targetTable);
    if (isKeyField) {
        return base + QStringLiteral(" (关键)");
    }
    return base;
}

QString FieldMappingHelper::suggestAlternateTableHint(const QString& currentTable,
                                                      const QStringList& missingKeyFields)
{
    if (missingKeyFields.isEmpty()) {
        return QString();
    }

    QHash<QString, int> scores;

    for (auto it = kKeyFields.constBegin(); it != kKeyFields.constEnd(); ++it) {
        if (it.key() == currentTable) {
            continue;
        }
        int matchCount = 0;
        for (const QString& missingField : missingKeyFields) {
            if (it.value().contains(missingField)) {
                ++matchCount;
            }
        }
        if (matchCount > 0) {
            scores.insert(it.key(), matchCount);
        }
    }

    if (scores.isEmpty()) {
        return QString();
    }

    QString bestTable;
    int bestScore = 0;
    for (auto it = scores.constBegin(); it != scores.constEnd(); ++it) {
        if (it.value() > bestScore) {
            bestScore = it.value();
            bestTable = it.key();
        }
    }

    if (bestTable.isEmpty()) {
        return QString();
    }

    return QStringLiteral("检查是否选错了目标表（您可能想导入到「%1」？）")
        .arg(tableDisplayName(bestTable));
}

QString FieldMappingHelper::suggestField(const QString& fileColumn, const QStringList& candidateFields)
{
    const QString normalizedColumn = normalizeForAliasMatch(fileColumn.trimmed());
    if (normalizedColumn.isEmpty()) {
        return QString();
    }

    QString bestField;
    int bestScore = 0;

    for (const QString& field : candidateFields) {
        const QString normalizedField = normalizeForAliasMatch(field);
        if (!normalizedField.isEmpty() && normalizedColumn == normalizedField) {
            const int score = 100000 + static_cast<int>(normalizedField.size());
            if (score > bestScore) {
                bestScore = score;
                bestField = field;
            }
        }

        for (const QString& alias : kFieldAliases.value(field)) {
            const int score = scoreFieldAliasMatch(normalizedColumn, alias);
            if (score > bestScore) {
                bestScore = score;
                bestField = field;
            }
        }
    }

    return bestField;
}

QStringList FieldMappingHelper::commonElementNames()
{
    return {
        QStringLiteral("Cu"),
        QStringLiteral("Zn"),
        QStringLiteral("S"),
        QStringLiteral("Pb"),
        QStringLiteral("Fe"),
        QStringLiteral("Au"),
        QStringLiteral("Ag"),
        QStringLiteral("Mo"),
        QStringLiteral("Ni"),
        QStringLiteral("Co"),
        QStringLiteral("As"),
        QStringLiteral("Sb"),
    };
}

QString FieldMappingHelper::suggestElementName(const QString& fileColumn)
{
    const QString normalized = fileColumn.trimmed();
    if (normalized.isEmpty()) {
        return QString();
    }

    static const QList<QPair<QString, QString>> hints = {
        { QStringLiteral("铜"), QStringLiteral("Cu") },
        { QStringLiteral("锌"), QStringLiteral("Zn") },
        { QStringLiteral("硫"), QStringLiteral("S") },
        { QStringLiteral("铅"), QStringLiteral("Pb") },
        { QStringLiteral("铁"), QStringLiteral("Fe") },
        { QStringLiteral("金"), QStringLiteral("Au") },
        { QStringLiteral("银"), QStringLiteral("Ag") },
        { QStringLiteral("钼"), QStringLiteral("Mo") },
        { QStringLiteral("镍"), QStringLiteral("Ni") },
        { QStringLiteral("钴"), QStringLiteral("Co") },
        { QStringLiteral("砷"), QStringLiteral("As") },
        { QStringLiteral("锑"), QStringLiteral("Sb") },
    };

    for (const auto& hint : hints) {
        if (normalized.contains(hint.first, Qt::CaseInsensitive)) {
            return hint.second;
        }
    }

    for (const QString& element : commonElementNames()) {
        if (normalized.compare(element, Qt::CaseInsensitive) == 0) {
            return element;
        }
    }

    return QString();
}

MappingProfile FieldMappingHelper::buildMappingProfile(const QString& configName,
                                                       const QString& targetTable,
                                                       const QString& sourceFilePath,
                                                       const QStringList& sourceColumns,
                                                       const QHash<QString, QString>& columnMapping)
{
    MappingProfile profile;
    profile.configName = configName.trimmed();
    profile.targetTable = targetTable;
    profile.sourceFileName = QFileInfo(sourceFilePath).fileName();
    profile.sourceColumns = sourceColumns;
    profile.columnMapping = columnMapping;
    profile.version = QStringLiteral("1.2");
    profile.created = QDateTime::currentDateTime().toString(Qt::ISODate);
    return profile;
}

QString FieldMappingHelper::mappingsDirectory()
{
    const QString dirPath = AppConfig::mappingsDir();
    QDir().mkpath(dirPath);
    return dirPath;
}

QString FieldMappingHelper::suggestedConfigName(const QString& targetTable)
{
    const QString date = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd"));
    return QStringLiteral("%1_自定义_%2")
        .arg(tableDisplayName(targetTable), date);
}

QString FieldMappingHelper::mappingPathForProfile(const QString& targetTable,
                                                    const QString& configName)
{
    const QString tableStem = sanitizeConfigFileStem(targetTable.trimmed());
    const QString nameStem = sanitizeConfigFileStem(configName.trimmed());
    return mappingsDirectory() + QStringLiteral("/") + tableStem + QStringLiteral("_")
        + nameStem + QStringLiteral(".json");
}

bool FieldMappingHelper::configExistsForTable(const QString& targetTable,
                                                const QString& configName,
                                                QString* existingFilePath)
{
    const QString trimmedTable = targetTable.trimmed();
    const QString trimmedName = configName.trimmed();
    if (trimmedTable.isEmpty() || trimmedName.isEmpty()) {
        return false;
    }

    for (const SavedMappingConfigSummary& summary : listSavedConfigSummaries()) {
        if (summary.targetTable == trimmedTable && summary.configName == trimmedName) {
            if (existingFilePath) {
                *existingFilePath = summary.filePath;
            }
            return true;
        }
    }

    return false;
}

QList<SavedMappingConfigSummary> FieldMappingHelper::listSavedConfigSummaries()
{
    QList<SavedMappingConfigSummary> summaries;
    QDir dir(mappingsDirectory());
    const QFileInfoList files = dir.entryInfoList({ QStringLiteral("*.json") }, QDir::Files, QDir::Name);

    for (const QFileInfo& fileInfo : files) {
        MappingProfile profile;
        if (!loadMappingProfile(fileInfo.absoluteFilePath(), &profile)) {
            continue;
        }
        summaries.append(buildConfigSummary(fileInfo.absoluteFilePath(), profile));
    }

    std::sort(summaries.begin(), summaries.end(), [](const SavedMappingConfigSummary& left,
                                                     const SavedMappingConfigSummary& right) {
        const QString leftStamp = left.lastUsed.isEmpty() ? left.created : left.lastUsed;
        const QString rightStamp = right.lastUsed.isEmpty() ? right.created : right.lastUsed;
        return leftStamp > rightStamp;
    });

    return summaries;
}

QString FieldMappingHelper::findBestConfigForColumns(const QStringList& fileColumns)
{
    if (fileColumns.isEmpty()) {
        return QString();
    }

    QString bestPath;
    QString bestStamp;

    QDir dir(mappingsDirectory());
    const QFileInfoList files = dir.entryInfoList({ QStringLiteral("*.json") }, QDir::Files);

    for (const QFileInfo& fileInfo : files) {
        MappingProfile profile;
        if (!loadMappingProfile(fileInfo.absoluteFilePath(), &profile)) {
            continue;
        }

        const QStringList configColumns = effectiveSourceColumns(profile);
        if (!columnStructuresMatch(configColumns, fileColumns)) {
            continue;
        }

        const QString stamp = configTimestamp(profile);
        if (bestPath.isEmpty() || stamp > bestStamp) {
            bestPath = fileInfo.absoluteFilePath();
            bestStamp = stamp;
        }
    }

    return bestPath;
}

bool FieldMappingHelper::touchConfigLastUsed(const QString& filePath)
{
    MappingProfile profile;
    if (!loadMappingProfile(filePath, &profile)) {
        return false;
    }

    profile.lastUsed = QDateTime::currentDateTime().toString(Qt::ISODate);
    return saveMappingProfile(filePath, profile);
}

QString FieldMappingHelper::mappingPathForConfigName(const QString& configName)
{
    return mappingsDirectory() + QStringLiteral("/") + sanitizeConfigFileStem(configName)
        + QStringLiteral(".json");
}


QStringList FieldMappingHelper::listSavedConfigNames()
{
    QStringList names;
    for (const SavedMappingConfigSummary& summary : listSavedConfigSummaries()) {
        if (!names.contains(summary.configName)) {
            names.append(summary.configName);
        }
    }
    return names;
}

QString FieldMappingHelper::findConfigFileByName(const QString& configName)
{
    const QString trimmedName = configName.trimmed();
    for (const SavedMappingConfigSummary& summary : listSavedConfigSummaries()) {
        if (summary.configName == trimmedName) {
            return summary.filePath;
        }
    }
    return QString();
}
bool FieldMappingHelper::saveMappingProfile(const QString& filePath, const MappingProfile& profile)
{
    QDir().mkpath(QFileInfo(filePath).absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return false;
    }

    const QJsonDocument doc(profileToJson(profile));
    file.write(doc.toJson(QJsonDocument::Indented));
    return true;
}

bool FieldMappingHelper::loadMappingProfile(const QString& filePath, MappingProfile* profile)
{
    if (!profile) {
        return false;
    }

    profile->columnMapping.clear();
    profile->sourceColumns.clear();
    profile->gradeElementMapping.clear();

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        return false;
    }

    const QJsonObject root = doc.object();
    profile->configName = root.value(QStringLiteral("config_name")).toString();
    profile->targetTable = root.value(QStringLiteral("target_table")).toString();
    profile->sourceFileName = root.value(QStringLiteral("source_file_name")).toString();
    profile->version = root.value(QStringLiteral("version")).toString();
    profile->created = root.value(QStringLiteral("created")).toString();
    profile->lastUsed = root.value(QStringLiteral("last_used")).toString();

    const QJsonArray sourceColumnsArray = root.value(QStringLiteral("source_columns")).toArray();
    for (const QJsonValue& value : sourceColumnsArray) {
        profile->sourceColumns.append(value.toString());
    }

    const QJsonObject columnsObject = root.value(QStringLiteral("columns")).toObject();
    for (auto it = columnsObject.constBegin(); it != columnsObject.constEnd(); ++it) {
        profile->columnMapping.insert(it.key(), it.value().toString());
    }

    const QJsonObject elementObject = root.value(QStringLiteral("grade_element_mapping")).toObject();
    for (auto it = elementObject.constBegin(); it != elementObject.constEnd(); ++it) {
        profile->gradeElementMapping.insert(it.key(), it.value().toString());
    }

    if (profile->configName.isEmpty()) {
        profile->configName = QFileInfo(filePath).completeBaseName();
    }

    return true;
}

bool FieldMappingHelper::validateMappingCompatibility(const MappingProfile& profile,
                                                        const QString& currentTargetTable,
                                                        const QStringList& currentFileColumns,
                                                        QString* errorMessage)
{
    if (!profile.targetTable.isEmpty()
        && profile.targetTable != currentTargetTable) {
        if (errorMessage) {
            *errorMessage = QStringLiteral(
                "目标表不匹配，无法复用此配置。\n\n"
                "配置绑定目标表：%1\n"
                "当前选择目标表：%2\n\n"
                "换目标表后需要重新配置并保存新的映射。")
                                .arg(tableDisplayName(profile.targetTable),
                                     tableDisplayName(currentTargetTable));
        }
        return false;
    }

    if (!profile.sourceColumns.isEmpty()) {
        const QSet<QString> savedColumns = toColumnSet(profile.sourceColumns);
        const QSet<QString> currentColumns = toColumnSet(currentFileColumns);

        if (savedColumns != currentColumns) {
            QStringList onlyInSaved;
            QStringList onlyInCurrent;

            for (const QString& column : savedColumns) {
                if (!currentColumns.contains(column)) {
                    onlyInSaved.append(column);
                }
            }
            for (const QString& column : currentColumns) {
                if (!savedColumns.contains(column)) {
                    onlyInCurrent.append(column);
                }
            }

            QString detail;
            if (!onlyInSaved.isEmpty()) {
                detail += QStringLiteral("\n配置中有但当前文件缺少的列：\n- ")
                    + onlyInSaved.join(QStringLiteral("\n- "));
            }
            if (!onlyInCurrent.isEmpty()) {
                detail += QStringLiteral("\n当前文件新增/变更的列：\n- ")
                    + onlyInCurrent.join(QStringLiteral("\n- "));
            }

            if (errorMessage) {
                *errorMessage = QStringLiteral(
                    "文件列名结构已变化，无法复用此配置。\n"
                    "映射配置绑定的是「文件列名结构 + 目标表」组合。%1\n\n"
                    "请重新配置映射并保存一份新配置。")
                                    .arg(detail);
            }
            return false;
        }
    } else {
        for (auto it = profile.columnMapping.constBegin(); it != profile.columnMapping.constEnd(); ++it) {
            if (!currentFileColumns.contains(it.key())) {
                if (errorMessage) {
                    *errorMessage = QStringLiteral(
                        "这是旧版映射配置，缺少列结构信息，且列「%1」在当前文件中不存在。\n"
                        "请重新配置并保存新版配置。").arg(it.key());
                }
                return false;
            }
        }
    }

    return true;
}

QStringList FieldMappingHelper::effectiveSourceColumns(const MappingProfile& profile)
{
    if (!profile.sourceColumns.isEmpty()) {
        return profile.sourceColumns;
    }

    QStringList columns = profile.columnMapping.keys();
    columns.sort();
    return columns;
}

MappingLoadComparison FieldMappingHelper::compareForLoad(const MappingProfile& profile,
                                                         const QString& currentTargetTable,
                                                         const QStringList& currentFileColumns)
{
    MappingLoadComparison result;
    result.currentFileColumns = currentFileColumns;
    result.configColumns = effectiveSourceColumns(profile);

    result.targetTableMatch = profile.targetTable.isEmpty()
        || profile.targetTable == currentTargetTable;

    const QSet<QString> currentSet = toColumnSet(currentFileColumns);
    const QSet<QString> configSet = toColumnSet(result.configColumns);

    result.columnStructureMatch = !result.configColumns.isEmpty()
        && currentSet == configSet;

    for (const QString& column : currentSet) {
        if (!configSet.contains(column)) {
            result.onlyInCurrentFile.append(column);
        }
    }
    for (const QString& column : configSet) {
        if (!currentSet.contains(column)) {
            result.onlyInConfig.append(column);
        }
    }

    result.onlyInCurrentFile.sort();
    result.onlyInConfig.sort();
    return result;
}

QString FieldMappingHelper::formatColumnList(const QStringList& columns)
{
    if (columns.isEmpty()) {
        return QStringLiteral("（无）");
    }
    return columns.join(QStringLiteral("、"));
}

QString FieldMappingHelper::buildLoadConfirmMessage(const QString& configName,
                                                    const QString& currentTargetTable,
                                                    const MappingProfile& profile,
                                                    const MappingLoadComparison& comparison)
{
    QString message = QStringLiteral("配置名称：%1\n\n").arg(configName);

    message += QStringLiteral("当前目标表：%1\n")
                   .arg(tableDisplayName(currentTargetTable));
    if (!profile.targetTable.isEmpty()) {
        message += QStringLiteral("配置目标表：%1\n")
                       .arg(tableDisplayName(profile.targetTable));
    }
    message += QStringLiteral("\n");

    message += QStringLiteral("当前文件列名：%1\n")
                   .arg(formatColumnList(comparison.currentFileColumns));
    message += QStringLiteral("配置文件列名：%1\n\n")
                   .arg(formatColumnList(comparison.configColumns));

    if (comparison.columnStructureMatch && comparison.targetTableMatch) {
        message += QStringLiteral("✅ 列名匹配，目标表一致，可以安全加载。");
        return message;
    }

    if (!comparison.targetTableMatch) {
        message += QStringLiteral("⚠️ 目标表不一致，加载后映射字段可能不适用！\n");
    }
    if (!comparison.columnStructureMatch) {
        message += QStringLiteral("⚠️ 列名不完全匹配，加载后可能导致数据错乱！\n");
        if (!comparison.onlyInCurrentFile.isEmpty()) {
            message += QStringLiteral("\n当前文件有、配置中没有的列：\n- ")
                + comparison.onlyInCurrentFile.join(QStringLiteral("\n- "));
            message += QChar('\n');
        }
        if (!comparison.onlyInConfig.isEmpty()) {
            message += QStringLiteral("\n配置中有、当前文件没有的列：\n- ")
                + comparison.onlyInConfig.join(QStringLiteral("\n- "));
            message += QChar('\n');
        }
    }

    message += QStringLiteral("\n是否仍要加载此配置？");
    return message;
}
