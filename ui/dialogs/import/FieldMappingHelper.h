#ifndef FIELDMAPPINGHELPER_H
#define FIELDMAPPINGHELPER_H

#include <QString>
#include <QStringList>
#include <QHash>
#include <QList>

struct MappingProfile
{
    QString configName;
    QString targetTable;
    QString sourceFileName;
    QStringList sourceColumns;
    QHash<QString, QString> columnMapping;
    QHash<QString, QString> gradeElementMapping;
    QString created;
    QString lastUsed;
    QString version;
};

struct MappingLoadComparison
{
    bool columnStructureMatch = false;
    bool targetTableMatch = false;
    QStringList onlyInCurrentFile;
    QStringList onlyInConfig;
    QStringList currentFileColumns;
    QStringList configColumns;
};

struct SavedMappingConfigSummary
{
    QString configName;
    QString targetTable;
    QString filePath;
    int columnCount = 0;
    QString created;
    QString lastUsed;
};

struct FieldMatchDegreeResult
{
    int matchedCount = 0;
    int totalCount = 0;
    int percent = 0;
    QStringList matchedCoreFields;
    QStringList missingCoreFields;
};

class FieldMappingHelper
{
public:
    static QStringList tableNames();
    static QString tableDisplayName(const QString& tableName);
    static QStringList fieldsForTable(const QString& tableName);
    static QStringList requiredFieldsForTable(const QString& tableName);
    static QStringList keyFieldsForTable(const QString& tableName);
    static QStringList coreFieldsForTable(const QString& tableName);
    static FieldMatchDegreeResult computeFieldMatchDegree(const QString& targetTable,
                                                           const QStringList& fileColumns);
    static FieldMatchDegreeResult computeFieldMatchDegreeFromMapping(
        const QString& targetTable,
        const QHash<QString, QString>& columnMapping,
        const QHash<QString, QString>& gradeElementMapping = QHash<QString, QString>());
    static QString buildFieldMatchDegreeBlockMessage(const QString& targetTable,
                                                     const FieldMatchDegreeResult& result);
    static QString buildFieldMatchDegreeWarnMessage(const QString& targetTable,
                                                    const FieldMatchDegreeResult& result);
    static QString buildFieldMatchDegreeImportWarnMessage(const QString& targetTable,
                                                          const FieldMatchDegreeResult& result);
    static QString formatCoreFieldList(const QStringList& fieldNames,
                                       const QString& targetTable = QString());
    static QString fieldDisplayName(const QString& fieldName);
    static QString fieldDisplayName(const QString& fieldName, const QString& targetTable);
    static QString fieldComboLabel(const QString& fieldName, bool isKeyField,
                                   const QString& targetTable = QString());
    static QString suggestAlternateTableHint(const QString& currentTable,
                                             const QStringList& missingKeyFields);

    static QString suggestField(const QString& fileColumn, const QStringList& candidateFields);
    static QString suggestElementName(const QString& fileColumn);
    static QStringList commonElementNames();

    static MappingProfile buildMappingProfile(const QString& configName,
                                              const QString& targetTable,
                                              const QString& sourceFilePath,
                                              const QStringList& sourceColumns,
                                              const QHash<QString, QString>& columnMapping);

    static QString mappingsDirectory();
    static QString suggestedConfigName(const QString& targetTable);
    static QString mappingPathForProfile(const QString& targetTable, const QString& configName);
    static bool configExistsForTable(const QString& targetTable,
                                     const QString& configName,
                                     QString* existingFilePath = nullptr);
    static QList<SavedMappingConfigSummary> listSavedConfigSummaries();
    static QString findBestConfigForColumns(const QStringList& fileColumns);
    static bool touchConfigLastUsed(const QString& filePath);

    static QString mappingPathForConfigName(const QString& configName);
    static QStringList listSavedConfigNames();
    static QString findConfigFileByName(const QString& configName);

    static bool saveMappingProfile(const QString& filePath, const MappingProfile& profile);
    static bool loadMappingProfile(const QString& filePath, MappingProfile* profile);

    static bool validateMappingCompatibility(const MappingProfile& profile,
                                             const QString& currentTargetTable,
                                             const QStringList& currentFileColumns,
                                             QString* errorMessage);

    static QStringList effectiveSourceColumns(const MappingProfile& profile);
    static MappingLoadComparison compareForLoad(const MappingProfile& profile,
                                                const QString& currentTargetTable,
                                                const QStringList& currentFileColumns);
    static QString formatColumnList(const QStringList& columns);
    static QString buildLoadConfirmMessage(const QString& configName,
                                           const QString& currentTargetTable,
                                           const MappingProfile& profile,
                                           const MappingLoadComparison& comparison);
};

#endif // FIELDMAPPINGHELPER_H
