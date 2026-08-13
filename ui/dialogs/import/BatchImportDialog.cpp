#include "BatchImportDialog.h"
#include "FieldMappingHelper.h"
#include "PythonRunner.h"
#include "AppConfig.h"
#include "ImportConflictDialog.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonDocument>
#include <QSet>
#include <QInputDialog>
#include <QLineEdit>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QScrollArea>
#include <QScreen>
#include <QGuiApplication>
#include <QShowEvent>
#include <QDateTime>

#include <QDialog>
#include <QTableWidgetItem>
#include <QAbstractItemView>

namespace {

const QString kIgnoreValue = QStringLiteral("__ignore__");

bool isSupportedImportFile(const QString& filePath)
{
    const QString suffix = QFileInfo(filePath).suffix().toLower();
    return suffix == QStringLiteral("csv")
        || suffix == QStringLiteral("xlsx")
        || suffix == QStringLiteral("xls");
}

QString formatMappingSummaryTime(const QString& isoText)
{
    if (isoText.trimmed().isEmpty()) {
        return QStringLiteral("—");
    }
    const QDateTime dateTime = QDateTime::fromString(isoText, Qt::ISODate);
    return dateTime.isValid()
        ? dateTime.toString(QStringLiteral("yyyy-MM-dd hh:mm"))
        : isoText;
}

void populateFieldCombo(QComboBox* combo,
                        const QStringList& fields,
                        const QStringList& keyFields,
                        const QString& selectedField,
                        const QString& targetTable)
{
    combo->clear();
    combo->addItem(QStringLiteral("未映射 (→ EXTRA_DATA)"), kIgnoreValue);

    for (const QString& field : fields) {
        combo->addItem(
            FieldMappingHelper::fieldComboLabel(field, keyFields.contains(field), targetTable),
            field);
    }

    const QString effectiveSelection = selectedField.isEmpty() ? kIgnoreValue : selectedField;
    const int index = combo->findData(effectiveSelection);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

void populateElementCombo(QComboBox* combo, const QString& selectedElement)
{
    combo->clear();
    combo->addItem(QStringLiteral("（请选择）"), QString());
    for (const QString& element : FieldMappingHelper::commonElementNames()) {
        combo->addItem(element, element);
    }
    combo->setEditable(true);
    combo->setInsertPolicy(QComboBox::NoInsert);

    const int index = combo->findData(selectedElement);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else if (!selectedElement.isEmpty()) {
        combo->setEditText(selectedElement);
    }
}

} // namespace

BatchImportDialog::BatchImportDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setWindowTitle(QStringLiteral("批量导入钻孔数据"));
    setModal(true);
    setupUI();
    ensureFitsScreen();
    resize(qMin(820, maximumWidth()), qMin(640, maximumHeight()));
}

void BatchImportDialog::showEvent(QShowEvent* event)
{
    QDialog::showEvent(event);
    ensureFitsScreen();
}

void BatchImportDialog::ensureFitsScreen()
{
    const QScreen* screen = QGuiApplication::primaryScreen();
    if (!screen) {
        return;
    }

    const QRect available = screen->availableGeometry();
    const int maxWidth = qMax(640, available.width() - 48);
    const int maxHeight = qMax(480, available.height() - 48);
    setMaximumSize(maxWidth, maxHeight);

    if (width() > maxWidth || height() > maxHeight) {
        resize(qMin(width(), maxWidth), qMin(height(), maxHeight));
    }
}

void BatchImportDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    auto* step1Group = new QGroupBox(QStringLiteral("步骤1: 选择文件"), this);
    auto* step1Layout = new QVBoxLayout(step1Group);

    auto* fileLayout = new QHBoxLayout();
    m_btnSelectFile = new QPushButton(QStringLiteral("📂 选择文件"), this);
    m_filePathLabel = new QLabel(
        QStringLiteral("未选择文件（支持 .csv / .xlsx / .xls）"), this);
    m_filePathLabel->setWordWrap(true);
    fileLayout->addWidget(m_btnSelectFile);
    fileLayout->addWidget(m_filePathLabel, 1);
    step1Layout->addLayout(fileLayout);

    auto* optionLayout = new QHBoxLayout();
    optionLayout->addWidget(new QLabel(QStringLiteral("目标表:"), this));
    m_targetTableCombo = new QComboBox(this);
    for (const QString& tableName : FieldMappingHelper::tableNames()) {
        m_targetTableCombo->addItem(FieldMappingHelper::tableDisplayName(tableName), tableName);
    }
    optionLayout->addWidget(m_targetTableCombo);
    optionLayout->addStretch();
    step1Layout->addLayout(optionLayout);
    mainLayout->addWidget(step1Group);

    auto* step2Group = new QGroupBox(QStringLiteral("步骤2: 字段映射（系统已自动推荐，可手动修改）"), this);
    auto* step2Layout = new QVBoxLayout(step2Group);

    m_importStatusLabel = new QLabel(this);
    m_importStatusLabel->setWordWrap(true);
    step2Layout->addWidget(m_importStatusLabel);

    m_mappingTable = new QTableWidget(this);
    m_mappingTable->setColumnCount(2);
    m_mappingTable->setHorizontalHeaderLabels({
        QStringLiteral("文件列名"),
        QStringLiteral("系统字段")
    });
    m_mappingTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_mappingTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_mappingTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_mappingTable->setContextMenuPolicy(Qt::NoContextMenu);
    m_mappingTable->verticalHeader()->setVisible(false);
    m_mappingTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_mappingTable->verticalHeader()->setDefaultSectionSize(28);
    m_mappingTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    step2Layout->addWidget(m_mappingTable);

    m_gradeElementGroup = new QGroupBox(QStringLiteral("元素名称映射（品位列 → 元素）"), this);
    m_gradeElementGroup->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    auto* gradeElementLayout = new QVBoxLayout(m_gradeElementGroup);
    gradeElementLayout->setContentsMargins(8, 8, 8, 6);
    gradeElementLayout->setSpacing(4);

    m_gradeElementTable = new QTableWidget(this);
    m_gradeElementTable->setColumnCount(2);
    m_gradeElementTable->setHorizontalHeaderLabels({
        QStringLiteral("文件列名（品位列）"),
        QStringLiteral("元素名称")
    });
    m_gradeElementTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_gradeElementTable->verticalHeader()->setVisible(false);
    m_gradeElementTable->verticalHeader()->setSectionResizeMode(QHeaderView::Fixed);
    m_gradeElementTable->verticalHeader()->setDefaultSectionSize(30);
    m_gradeElementTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_gradeElementTable->setSelectionMode(QAbstractItemView::NoSelection);
    m_gradeElementTable->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_gradeElementTable->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    gradeElementLayout->addWidget(m_gradeElementTable);

    auto* gradeHint = new QLabel(
        QStringLiteral("💡 品位列映射到 grade_value 后，在此指定元素名（Cu/Zn/S 等），导入时自动列转行。"),
        this);
    gradeHint->setWordWrap(true);
    gradeHint->setStyleSheet(QStringLiteral("color: #666666; font-size: 11px;"));
    gradeElementLayout->addWidget(gradeHint);
    m_gradeElementGroup->setVisible(false);
    step2Layout->addWidget(m_gradeElementGroup, 0);

    auto* mappingHint = new QLabel(
        QStringLiteral("💡 标有“关键”的为必填字段，缺少则无法导入。"
                       "选择「未映射 (→ EXTRA_DATA)」的列会完整保存在 EXTRA_DATA 中。"
                       "若检测到主键冲突，将弹出对话框供您选择："
                       "跳过（保留旧数据）、覆盖（完全替换，空值也清空）、"
                       "合并（仅非空字段覆盖，其余保留）。"),
        this);
    mappingHint->setWordWrap(true);
    mappingHint->setStyleSheet(QStringLiteral("color: #666666;"));
    step2Layout->addWidget(mappingHint);

    auto* mappingBtnLayout = new QHBoxLayout();
    auto* btnSaveMapping = new QPushButton(QStringLiteral("💾 保存映射配置"), this);
    auto* btnLoadMapping = new QPushButton(QStringLiteral("📂 加载已有配置"), this);
    mappingBtnLayout->addWidget(btnSaveMapping);
    mappingBtnLayout->addWidget(btnLoadMapping);
    mappingBtnLayout->addStretch();
    step2Layout->addLayout(mappingBtnLayout);

    auto* step2Scroll = new QScrollArea(this);
    step2Scroll->setWidget(step2Group);
    step2Scroll->setWidgetResizable(true);
    step2Scroll->setFrameShape(QFrame::NoFrame);
    step2Scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    mainLayout->addWidget(step2Scroll, 1);

    auto* step3Group = new QGroupBox(QStringLiteral("步骤3: 执行导入"), this);
    auto* step3Layout = new QVBoxLayout(step3Group);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 100);
    m_progressBar->setValue(0);
    step3Layout->addWidget(m_progressBar);

    m_resultLabel = new QLabel(QStringLiteral("等待导入..."), this);
    step3Layout->addWidget(m_resultLabel);

    auto* logBtnLayout = new QHBoxLayout();
    m_btnDownloadLog = new QPushButton(QStringLiteral("📥 下载错误日志"), this);
    m_btnCleanLogs = new QPushButton(QStringLiteral("🗑 清理历史日志"), this);
    m_btnDownloadLog->setVisible(false);
    logBtnLayout->addWidget(m_btnDownloadLog);
    logBtnLayout->addWidget(m_btnCleanLogs);
    logBtnLayout->addStretch();
    step3Layout->addLayout(logBtnLayout);

    mainLayout->addWidget(step3Group);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_btnStartImport = new QPushButton(QStringLiteral("开始导入"), this);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    buttonLayout->addWidget(m_btnStartImport);
    buttonLayout->addWidget(btnCancel);
    mainLayout->addLayout(buttonLayout);

    connect(m_btnSelectFile, &QPushButton::clicked, this, &BatchImportDialog::onSelectFileClicked);
    connect(m_targetTableCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &BatchImportDialog::onTargetTableChanged);
    connect(btnSaveMapping, &QPushButton::clicked, this, &BatchImportDialog::onSaveMappingClicked);
    connect(btnLoadMapping, &QPushButton::clicked, this, &BatchImportDialog::onLoadMappingClicked);
    connect(m_btnStartImport, &QPushButton::clicked, this, &BatchImportDialog::onStartImportClicked);
    connect(m_btnDownloadLog, &QPushButton::clicked, this, &BatchImportDialog::onDownloadLogClicked);
    connect(m_btnCleanLogs, &QPushButton::clicked, this, &BatchImportDialog::onCleanLogsClicked);
    connect(btnCancel, &QPushButton::clicked, this, &BatchImportDialog::onCancelClicked);

    m_btnStartImport->setEnabled(false);
    updateLogActionButtons();
    updateCriticalFieldStatus();
}

QStringList BatchImportDialog::columnsMappedTo(const QString& systemField) const
{
    QStringList columns;
    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        QTableWidgetItem* fileItem = m_mappingTable->item(row, 0);
        QComboBox* combo = mappingComboBox(row);
        if (!fileItem || !combo) {
            continue;
        }
        if (combo->currentData().toString() == systemField) {
            columns.append(fileItem->text());
        }
    }
    return columns;
}

QHash<QString, QString> BatchImportDialog::currentGradeElementMapping() const
{
    QHash<QString, QString> mapping;
    if (!m_gradeElementGroup->isVisible()) {
        return mapping;
    }

    for (int row = 0; row < m_gradeElementTable->rowCount(); ++row) {
        QTableWidgetItem* fileItem = m_gradeElementTable->item(row, 0);
        auto* combo = qobject_cast<QComboBox*>(m_gradeElementTable->cellWidget(row, 1));
        if (!fileItem || !combo) {
            continue;
        }
        const QString elementName = combo->currentData().toString().trimmed().isEmpty()
            ? combo->currentText().trimmed()
            : combo->currentData().toString().trimmed();
        if (!elementName.isEmpty()) {
            mapping.insert(fileItem->text(), elementName);
        }
    }
    return mapping;
}

void BatchImportDialog::rebuildGradeElementTable()
{
    const bool isGradeTable = currentTargetTable() == QStringLiteral("GradeInfo");
    const QStringList gradeColumns = columnsMappedTo(QStringLiteral("grade_value"));
    m_gradeElementGroup->setVisible(isGradeTable && !gradeColumns.isEmpty());

    if (!isGradeTable || gradeColumns.isEmpty()) {
        m_gradeElementTable->setRowCount(0);
        return;
    }

    const QHash<QString, QString> existing = currentGradeElementMapping();
    m_gradeElementTable->setRowCount(gradeColumns.size());

    for (int row = 0; row < gradeColumns.size(); ++row) {
        const QString fileColumn = gradeColumns.at(row);
        m_gradeElementTable->setItem(row, 0, new QTableWidgetItem(fileColumn));

        auto* combo = new QComboBox(m_gradeElementTable);
        const QString suggested = existing.value(fileColumn, FieldMappingHelper::suggestElementName(fileColumn));
        populateElementCombo(combo, suggested);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) { updateCriticalFieldStatus(); });
        connect(combo, &QComboBox::editTextChanged, this, [this](const QString&) {
            updateCriticalFieldStatus();
        });
        m_gradeElementTable->setCellWidget(row, 1, combo);
    }

    adjustGradeElementTableHeight();
}

void BatchImportDialog::adjustGradeElementTableHeight()
{
    const int rowCount = m_gradeElementTable->rowCount();
    if (rowCount == 0) {
        m_gradeElementTable->setMaximumHeight(0);
        return;
    }

    constexpr int kMaxVisibleRows = 4;
    const int visibleRows = qMin(rowCount, kMaxVisibleRows);
    const int headerHeight = m_gradeElementTable->horizontalHeader()->height();
    const int rowHeight = m_gradeElementTable->verticalHeader()->defaultSectionSize();
    const int frame = m_gradeElementTable->frameWidth() * 2 + 4;
    const int tableHeight = headerHeight + rowHeight * visibleRows + frame;

    m_gradeElementTable->setMaximumHeight(tableHeight);
    m_gradeElementTable->setMinimumHeight(tableHeight);
}

void BatchImportDialog::updateCriticalFieldStatus()
{
    const QString tableName = FieldMappingHelper::tableDisplayName(currentTargetTable());
    const QStringList keyFields = FieldMappingHelper::keyFieldsForTable(currentTargetTable());

    if (m_mappingTable->rowCount() == 0) {
        m_matchDegreePercent = 0;
        QStringList keyLabels;
        for (const QString& field : keyFields) {
            keyLabels.append(FieldMappingHelper::fieldDisplayName(field, currentTargetTable()));
        }
        m_importStatusLabel->setText(
            QStringLiteral("请选择文件。目标表「%1」须映射关键字段：%2")
                .arg(tableName, keyLabels.join(QStringLiteral(" + "))));
        m_importStatusLabel->setStyleSheet(QStringLiteral("color: #996600;"));
        updateImportButtonState();
        return;
    }

    const FieldMatchDegreeResult matchResult = currentFieldMatchDegreeResult();
    m_matchDegreePercent = matchResult.percent;

    QStringList missingKeyLabels;
    const bool allKeyMapped = areKeyFieldsMapped(&missingKeyLabels);

    QString statusText;
    QString styleSheet = QStringLiteral("color: #996600; font-weight: bold;");

    if (matchResult.percent < 30) {
        statusText = QStringLiteral(
            "❌ 字段匹配度 %1%（%2/%3 个核心字段）— 匹配度过低，无法导入")
            .arg(matchResult.percent)
            .arg(matchResult.matchedCount)
            .arg(matchResult.totalCount);
        styleSheet = QStringLiteral("color: #cc0000; font-weight: bold;");
    } else if (matchResult.percent < 60) {
        statusText = QStringLiteral(
            "⚠️ 字段匹配度 %1%（%2/%3 个核心字段）— 匹配度偏低")
            .arg(matchResult.percent)
            .arg(matchResult.matchedCount)
            .arg(matchResult.totalCount);
    } else {
        statusText = QStringLiteral(
            "✅ 字段匹配度 %1%（%2/%3 个核心字段）— 匹配度合格，可以导入")
            .arg(matchResult.percent)
            .arg(matchResult.matchedCount)
            .arg(matchResult.totalCount);
        styleSheet = QStringLiteral("color: #22863a; font-weight: bold;");
    }

    if (!missingKeyLabels.isEmpty()) {
        statusText += QStringLiteral("\n⚠️ 还需映射关键字段：%1")
                          .arg(missingKeyLabels.join(QStringLiteral(" + ")));
        styleSheet = QStringLiteral("color: #996600; font-weight: bold;");
    } else if (allKeyMapped && !statusText.isEmpty()) {
        statusText += QStringLiteral("；关键字段已映射");
    }

    m_importStatusLabel->setText(statusText);
    m_importStatusLabel->setStyleSheet(styleSheet);
    updateImportButtonState();
}

bool BatchImportDialog::areKeyFieldsMapped(QStringList* missingKeyLabels) const
{
    const QStringList keyFields = FieldMappingHelper::keyFieldsForTable(currentTargetTable());

    QSet<QString> mappedFields;
    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        QComboBox* combo = mappingComboBox(row);
        if (!combo) {
            continue;
        }
        const QString systemField = combo->currentData().toString();
        if (systemField != kIgnoreValue) {
            mappedFields.insert(systemField);
        }
    }

    bool allMapped = true;
    for (const QString& keyField : keyFields) {
        if (mappedFields.contains(keyField)) {
            continue;
        }

        if (currentTargetTable() == QStringLiteral("GradeInfo")
            && keyField == QStringLiteral("element_name")) {
            const QStringList gradeColumns = columnsMappedTo(QStringLiteral("grade_value"));
            if (!gradeColumns.isEmpty()) {
                const QHash<QString, QString> elementMapping = currentGradeElementMapping();
                bool allElementsMapped = true;
                for (const QString& column : gradeColumns) {
                    if (!elementMapping.contains(column)) {
                        allElementsMapped = false;
                        break;
                    }
                }
                if (allElementsMapped) {
                    continue;
                }
            }
        }

        allMapped = false;
        if (missingKeyLabels) {
            missingKeyLabels->append(FieldMappingHelper::fieldDisplayName(keyField, currentTargetTable()));
        }
    }

    return allMapped;
}

void BatchImportDialog::resetProgress()
{
    m_progressBar->setValue(0);
    m_resultLabel->setText(QStringLiteral("等待导入..."));
    m_lastLogPath.clear();
    updateLogActionButtons();
}

void BatchImportDialog::updateLogActionButtons()
{
    m_btnDownloadLog->setVisible(!m_lastLogPath.isEmpty() && QFileInfo::exists(m_lastLogPath));
}

bool BatchImportDialog::copyLogToUserPath(const QString& defaultFileName) const
{
    if (m_lastLogPath.isEmpty() || !QFileInfo::exists(m_lastLogPath)) {
        QMessageBox::warning(const_cast<BatchImportDialog*>(this),
                             QStringLiteral("提示"),
                             QStringLiteral("没有可下载的错误日志。"));
        return false;
    }

    const QString savePath = QFileDialog::getSaveFileName(
        const_cast<BatchImportDialog*>(this),
        QStringLiteral("保存错误日志"),
        QDir::homePath() + QStringLiteral("/") + defaultFileName,
        QStringLiteral("日志文件 (*.log);;文本文件 (*.txt);;所有文件 (*.*)"));

    if (savePath.isEmpty()) {
        return false;
    }

    if (QFile::exists(savePath)) {
        QFile::remove(savePath);
    }

    if (QFile::copy(m_lastLogPath, savePath)) {
        QMessageBox::information(const_cast<BatchImportDialog*>(this),
                                 QStringLiteral("成功"),
                                 QStringLiteral("错误日志已保存到:\n%1").arg(savePath));
        return true;
    }

    QMessageBox::warning(const_cast<BatchImportDialog*>(this),
                         QStringLiteral("失败"),
                         QStringLiteral("保存日志文件失败，请检查目录权限。"));
    return false;
}

int BatchImportDialog::cleanAllImportLogs() const
{
    QDir logsDir(AppConfig::logsDir());
    if (!logsDir.exists()) {
        return 0;
    }

    int removedCount = 0;
    const QStringList files = logsDir.entryList(
        QStringList{QStringLiteral("import_errors_*.log")},
        QDir::Files);
    for (const QString& fileName : files) {
        if (logsDir.remove(fileName)) {
            ++removedCount;
        }
    }
    return removedCount;
}

void BatchImportDialog::onDownloadLogClicked()
{
    copyLogToUserPath(QFileInfo(m_lastLogPath).fileName());
}

void BatchImportDialog::onCleanLogsClicked()
{
    const int removedCount = cleanAllImportLogs();
    if (m_lastLogPath.isEmpty() || !QFileInfo::exists(m_lastLogPath)) {
        m_lastLogPath.clear();
    }
    updateLogActionButtons();
    QMessageBox::information(
        this,
        QStringLiteral("清理完成"),
        removedCount > 0
            ? QStringLiteral("已清理 %1 个历史错误日志文件。").arg(removedCount)
            : QStringLiteral("当前没有可清理的错误日志文件。"));
}

void BatchImportDialog::updateResultLabel(int totalCount,
                                          int insertedCount,
                                          int overwrittenCount,
                                          int mergedCount,
                                          int skippedCount,
                                          int failedCount)
{
    const int successCount = insertedCount + overwrittenCount + mergedCount;
    if (failedCount > 0 || skippedCount > 0) {
        QString text = QStringLiteral("导入完成：总 %1 条，成功 %2 条")
                           .arg(totalCount)
                           .arg(successCount);
        if (skippedCount > 0) {
            text += QStringLiteral("，用户跳过 %1 条").arg(skippedCount);
        }
        if (failedCount > 0) {
            text += QStringLiteral("，校验失败 %1 条").arg(failedCount);
        }
        m_resultLabel->setText(text);
    } else {
        m_resultLabel->setText(
            QStringLiteral("导入完成：共 %1 条，全部成功入库").arg(totalCount));
    }
}

QComboBox* BatchImportDialog::mappingComboBox(int row) const
{
    return qobject_cast<QComboBox*>(m_mappingTable->cellWidget(row, 1));
}

QString BatchImportDialog::currentTargetTable() const
{
    return m_targetTableCombo->currentData().toString();
}

bool BatchImportDialog::readFileColumns(const QString& filePath, QStringList* columns, QString* errorMessage)
{
    QJsonObject result;
    QString runError;
    const bool ok = PythonRunner::runScript(
        QStringLiteral("read_csv_columns.py"),
        { QStringLiteral("--file_path"), filePath },
        &result,
        &runError);

    if (!ok) {
        if (errorMessage) {
            *errorMessage = result.value(QStringLiteral("message")).toString(runError);
        }
        return false;
    }

    columns->clear();
    const QJsonArray array = result.value(QStringLiteral("columns")).toArray();
    for (const QJsonValue& value : array) {
        columns->append(value.toString());
    }
    return true;
}

void BatchImportDialog::rebuildMappingTable(const QStringList& fileColumns)
{
    m_currentFileColumns = fileColumns;
    m_mappingTable->setRowCount(fileColumns.size());

    const QString targetTable = currentTargetTable();
    const QStringList fields = FieldMappingHelper::fieldsForTable(targetTable);
    const QStringList keyFields = FieldMappingHelper::keyFieldsForTable(targetTable);

    for (int row = 0; row < fileColumns.size(); ++row) {
        const QString fileColumn = fileColumns.at(row);
        auto* fileItem = new QTableWidgetItem(fileColumn);
        m_mappingTable->setItem(row, 0, fileItem);

        auto* combo = new QComboBox(m_mappingTable);
        const QString suggested = FieldMappingHelper::suggestField(fileColumn, fields);
        populateFieldCombo(combo, fields, keyFields, suggested, targetTable);
        connect(combo, QOverload<int>::of(&QComboBox::currentIndexChanged),
                this, [this](int) {
                    updateCriticalFieldStatus();
                    rebuildGradeElementTable();
                });
        m_mappingTable->setCellWidget(row, 1, combo);
    }

    rebuildGradeElementTable();
    adjustMappingTableHeight();
    updateCriticalFieldStatus();
}

FieldMatchDegreeResult BatchImportDialog::currentFieldMatchDegreeResult() const
{
    return FieldMappingHelper::computeFieldMatchDegreeFromMapping(
        currentTargetTable(),
        currentColumnMapping(),
        currentGradeElementMapping());
}

void BatchImportDialog::updateImportButtonState()
{
    const bool canImport = !m_selectedFilePath.isEmpty()
        && m_matchDegreePercent >= 30
        && m_mappingTable->rowCount() > 0;
    m_btnStartImport->setEnabled(canImport);
}

bool BatchImportDialog::applyFileColumnsWithMatchCheck(const QStringList& fileColumns,
                                                       bool isTargetTableChange)
{
    m_currentFileColumns = fileColumns;
    rebuildMappingTable(fileColumns);
    if (!isTargetTableChange) {
        tryAutoLoadMappingConfig();
    }
    updateCriticalFieldStatus();
    m_lastTargetTableIndex = m_targetTableCombo->currentIndex();
    return m_matchDegreePercent >= 30;
}

void BatchImportDialog::adjustMappingTableHeight()
{
    const int rowCount = m_mappingTable->rowCount();
    if (rowCount == 0) {
        m_mappingTable->setMinimumHeight(72);
        m_mappingTable->setMaximumHeight(120);
        return;
    }

    constexpr int kMaxVisibleRows = 5;
    const int visibleRows = qMin(rowCount, kMaxVisibleRows);
    const int headerHeight = m_mappingTable->horizontalHeader()->height();
    const int rowHeight = m_mappingTable->verticalHeader()->defaultSectionSize();
    const int frame = m_mappingTable->frameWidth() * 2 + 4;
    const int tableHeight = headerHeight + rowHeight * visibleRows + frame;

    m_mappingTable->setMinimumHeight(tableHeight);
    m_mappingTable->setMaximumHeight(tableHeight);
}

void BatchImportDialog::applySavedMapping(const QHash<QString, QString>& savedMapping)
{
    const QStringList fields = FieldMappingHelper::fieldsForTable(currentTargetTable());
    const QStringList keyFields = FieldMappingHelper::keyFieldsForTable(currentTargetTable());

    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        QTableWidgetItem* fileItem = m_mappingTable->item(row, 0);
        QComboBox* combo = mappingComboBox(row);
        if (!fileItem || !combo) {
            continue;
        }

        const QString mappedField = savedMapping.value(fileItem->text());
        populateFieldCombo(combo, fields, keyFields, mappedField, currentTargetTable());
    }

    rebuildGradeElementTable();
    updateCriticalFieldStatus();
}

void BatchImportDialog::applyLoadedMappingProfile(const MappingProfile& profile)
{
    if (!profile.targetTable.isEmpty()) {
        m_targetTableCombo->blockSignals(true);
        const int tableIndex = m_targetTableCombo->findData(profile.targetTable);
        if (tableIndex >= 0) {
            m_targetTableCombo->setCurrentIndex(tableIndex);
        }
        m_targetTableCombo->blockSignals(false);
    }

    if (!m_currentFileColumns.isEmpty()) {
        rebuildMappingTable(m_currentFileColumns);
    }

    applySavedMapping(profile.columnMapping);

    for (int row = 0; row < m_gradeElementTable->rowCount(); ++row) {
        QTableWidgetItem* fileItem = m_gradeElementTable->item(row, 0);
        auto* combo = qobject_cast<QComboBox*>(m_gradeElementTable->cellWidget(row, 1));
        if (!fileItem || !combo) {
            continue;
        }
        const QString elementName = profile.gradeElementMapping.value(fileItem->text());
        if (!elementName.isEmpty()) {
            populateElementCombo(combo, elementName);
        }
    }

    updateCriticalFieldStatus();
}

QHash<QString, QString> BatchImportDialog::currentColumnMapping() const
{
    QHash<QString, QString> mapping;

    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        QTableWidgetItem* fileItem = m_mappingTable->item(row, 0);
        QComboBox* combo = mappingComboBox(row);
        if (!fileItem || !combo) {
            continue;
        }

        const QString systemField = combo->currentData().toString();
        mapping.insert(fileItem->text(), systemField);
    }

    return mapping;
}

QStringList BatchImportDialog::currentFileColumns() const
{
    if (!m_currentFileColumns.isEmpty()) {
        return m_currentFileColumns;
    }

    QStringList columns;
    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_mappingTable->item(row, 0);
        if (item) {
            columns.append(item->text());
        }
    }
    return columns;
}

MappingProfile BatchImportDialog::currentMappingProfile() const
{
    MappingProfile profile = FieldMappingHelper::buildMappingProfile(
        QString(),
        currentTargetTable(),
        m_selectedFilePath,
        currentFileColumns(),
        currentColumnMapping());
    profile.gradeElementMapping = currentGradeElementMapping();
    return profile;
}

bool BatchImportDialog::validateCriticalMapping() const
{
    const FieldMatchDegreeResult matchResult = FieldMappingHelper::computeFieldMatchDegreeFromMapping(
        currentTargetTable(),
        currentColumnMapping(),
        currentGradeElementMapping());

    if (matchResult.percent < 30) {
        QMessageBox box(const_cast<BatchImportDialog*>(this));
        box.setWindowTitle(QStringLiteral("无法导入"));
        box.setIcon(QMessageBox::Warning);
        box.setText(FieldMappingHelper::buildFieldMatchDegreeBlockMessage(
            currentTargetTable(), matchResult));
        box.addButton(QStringLiteral("返回修改"), QMessageBox::AcceptRole);
        box.exec();
        return false;
    }

    if (matchResult.percent < 60) {
        QMessageBox box(const_cast<BatchImportDialog*>(this));
        box.setWindowTitle(QStringLiteral("字段匹配度偏低"));
        box.setIcon(QMessageBox::Warning);
        box.setText(FieldMappingHelper::buildFieldMatchDegreeImportWarnMessage(
            currentTargetTable(), matchResult));
        box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        box.setDefaultButton(QMessageBox::No);
        box.button(QMessageBox::Yes)->setText(QStringLiteral("继续导入"));
        box.button(QMessageBox::No)->setText(QStringLiteral("返回修改"));
        if (box.exec() != QMessageBox::Yes) {
            return false;
        }
    }

    const QString targetTable = currentTargetTable();
    const QString tableName = FieldMappingHelper::tableDisplayName(targetTable);
    const QStringList keyFields = FieldMappingHelper::keyFieldsForTable(targetTable);
    const QStringList allFields = FieldMappingHelper::fieldsForTable(targetTable);
    const QStringList fileColumns = currentFileColumns();

    QSet<QString> mappedFields;
    for (int row = 0; row < m_mappingTable->rowCount(); ++row) {
        QComboBox* combo = mappingComboBox(row);
        if (!combo) {
            continue;
        }
        const QString systemField = combo->currentData().toString();
        if (systemField != kIgnoreValue) {
            mappedFields.insert(systemField);
        }
    }

    QStringList missingMapped;
    QStringList missingInFile;
    QStringList missingElementNames;

    const bool isGradeTable = targetTable == QStringLiteral("GradeInfo");
    const QStringList gradeColumns = isGradeTable
        ? columnsMappedTo(QStringLiteral("grade_value"))
        : QStringList();
    const QHash<QString, QString> elementMapping = isGradeTable
        ? currentGradeElementMapping()
        : QHash<QString, QString>();

    for (const QString& keyField : keyFields) {
        if (mappedFields.contains(keyField)) {
            continue;
        }

        if (isGradeTable && keyField == QStringLiteral("element_name") && !gradeColumns.isEmpty()) {
            for (const QString& column : gradeColumns) {
                if (!elementMapping.contains(column)) {
                    missingElementNames.append(column);
                }
            }
            continue;
        }

        bool fileHasCandidate = false;
        for (const QString& fileColumn : fileColumns) {
            if (FieldMappingHelper::suggestField(fileColumn, allFields) == keyField) {
                fileHasCandidate = true;
                break;
            }
        }

        if (fileHasCandidate) {
            missingMapped.append(keyField);
        } else {
            missingInFile.append(keyField);
        }
    }

    if (missingMapped.isEmpty() && missingInFile.isEmpty() && missingElementNames.isEmpty()) {
        return true;
    }

    QString message;
    if (!missingMapped.isEmpty()) {
        message += QStringLiteral("目标表 [%1] 缺少关键字段映射：\n\n").arg(tableName);
        for (const QString& field : missingMapped) {
            message += QStringLiteral("❌ %1 —— 未映射\n")
                           .arg(FieldMappingHelper::fieldDisplayName(field, targetTable));
        }
        message += QStringLiteral("\n该字段是当前目标表的主键/联合主键组成部分，必须映射后才能导入。\n\n");
        message += QStringLiteral("建议：\n");
        const QString alternateHint = FieldMappingHelper::suggestAlternateTableHint(
            targetTable, missingMapped + missingInFile);
        if (!alternateHint.isEmpty()) {
            message += QStringLiteral("1. %1\n").arg(alternateHint);
        }
        message += QStringLiteral("2. 确认文件中是否包含对应列\n");
        message += QStringLiteral("3. 在字段映射中为该列选择正确的系统字段\n");
    }

    if (!missingElementNames.isEmpty()) {
        if (!message.isEmpty()) {
            message += QChar('\n');
        }
        message += QStringLiteral("目标表 [%1] 的品位列缺少元素名称映射：\n\n").arg(tableName);
        for (const QString& column : missingElementNames) {
            message += QStringLiteral("❌ %1 —— 未指定元素名称\n").arg(column);
        }
        message += QStringLiteral(
            "\n请在「元素名称映射」区域为每个品位列指定元素（如 Cu、Zn、S）。\n"
            "导入时将自动把多列品位数据转换为多行记录。");
    }

    if (!missingInFile.isEmpty()) {
        if (!message.isEmpty()) {
            message += QChar('\n');
        }
        message += QStringLiteral("文件缺少目标表 [%1] 的以下关键字段：\n\n").arg(tableName);
        for (const QString& field : missingInFile) {
            message += QStringLiteral("❌ %1 —— 文件中未找到该列\n")
                           .arg(FieldMappingHelper::fieldDisplayName(field, targetTable));
        }
        message += QStringLiteral("\n请确认您选择的文件是否正确。");
    }

    QMessageBox box(const_cast<BatchImportDialog*>(this));
    box.setWindowTitle(QStringLiteral("⚠️ 无法导入"));
    box.setIcon(QMessageBox::Warning);
    box.setText(message);
    box.addButton(QStringLiteral("返回修改"), QMessageBox::AcceptRole);
    box.exec();
    return false;
}

bool BatchImportDialog::confirmLoadMappingProfile(const MappingProfile& profile,
                                                  const QString& configName)
{
    const MappingLoadComparison comparison = FieldMappingHelper::compareForLoad(
        profile,
        currentTargetTable(),
        currentFileColumns());

    QMessageBox box(this);
    box.setWindowTitle(comparison.columnStructureMatch
        ? QStringLiteral("配置加载确认")
        : QStringLiteral("列名不匹配"));
    box.setIcon(comparison.columnStructureMatch && comparison.targetTableMatch
                    ? QMessageBox::Information
                    : QMessageBox::Warning);
    box.setText(FieldMappingHelper::buildLoadConfirmMessage(
        configName,
        currentTargetTable(),
        profile,
        comparison));

    QPushButton* btnYes = box.addButton(QStringLiteral("是，继续加载"), QMessageBox::AcceptRole);
    QPushButton* btnNo = box.addButton(QStringLiteral("否，取消"), QMessageBox::RejectRole);
    box.setDefaultButton(comparison.columnStructureMatch && comparison.targetTableMatch ? btnYes : btnNo);

    box.exec();
    return box.clickedButton() == btnYes;
}

bool BatchImportDialog::loadMappingConfigAtPath(const QString& filePath, bool interactiveConfirm)
{
    MappingProfile profile;
    if (!FieldMappingHelper::loadMappingProfile(filePath, &profile)) {
        QMessageBox::warning(this, QStringLiteral("加载失败"), QStringLiteral("无法读取映射配置文件。"));
        return false;
    }

    const QString configName = profile.configName.trimmed().isEmpty()
        ? QFileInfo(filePath).completeBaseName()
        : profile.configName.trimmed();

    if (interactiveConfirm && !confirmLoadMappingProfile(profile, configName)) {
        return false;
    }

    applyLoadedMappingProfile(profile);
    FieldMappingHelper::touchConfigLastUsed(filePath);
    updateCriticalFieldStatus();
    m_lastTargetTableIndex = m_targetTableCombo->currentIndex();
    return true;
}

void BatchImportDialog::tryAutoLoadMappingConfig()
{
    if (m_currentFileColumns.isEmpty()) {
        return;
    }

    const QString autoPath = FieldMappingHelper::findBestConfigForColumns(m_currentFileColumns);
    if (autoPath.isEmpty()) {
        return;
    }

    loadMappingConfigAtPath(autoPath, false);
}

bool BatchImportDialog::showLoadMappingDialog(SavedMappingConfigSummary* selectedSummary)
{
    if (!selectedSummary) {
        return false;
    }

    const QList<SavedMappingConfigSummary> summaries = FieldMappingHelper::listSavedConfigSummaries();

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("加载已有配置"));
    dialog.resize(860, 420);

    auto* layout = new QVBoxLayout(&dialog);

    auto* table = new QTableWidget(&dialog);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({
        QStringLiteral("配置名称"),
        QStringLiteral("目标表"),
        QStringLiteral("文件列数"),
        QStringLiteral("保存时间"),
        QStringLiteral("最后使用时间"),
    });
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);

    auto* emptyLabel = new QLabel(QStringLiteral("暂无保存的配置"), &dialog);
    emptyLabel->setAlignment(Qt::AlignCenter);
    emptyLabel->setStyleSheet(QStringLiteral("color: #888888; padding: 24px;"));

    if (summaries.isEmpty()) {
        layout->addWidget(emptyLabel, 1);
    } else {
        table->setRowCount(summaries.size());
        for (int row = 0; row < summaries.size(); ++row) {
            const SavedMappingConfigSummary& summary = summaries.at(row);
            auto* nameItem = new QTableWidgetItem(summary.configName);
            nameItem->setData(Qt::UserRole, summary.filePath);
            table->setItem(row, 0, nameItem);
            table->setItem(row, 1, new QTableWidgetItem(
                FieldMappingHelper::tableDisplayName(summary.targetTable)));
            table->setItem(row, 2, new QTableWidgetItem(QString::number(summary.columnCount)));
            table->setItem(row, 3, new QTableWidgetItem(formatMappingSummaryTime(summary.created)));
            table->setItem(row, 4, new QTableWidgetItem(formatMappingSummaryTime(summary.lastUsed)));
        }
        table->selectRow(0);
        layout->addWidget(table, 1);
    }

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* btnLoad = new QPushButton(QStringLiteral("加载"), &dialog);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), &dialog);
    btnLoad->setEnabled(!summaries.isEmpty());
    btnLoad->setDefault(true);
    buttonLayout->addWidget(btnLoad);
    buttonLayout->addWidget(btnCancel);
    layout->addLayout(buttonLayout);

    connect(btnCancel, &QPushButton::clicked, &dialog, &QDialog::reject);
    connect(btnLoad, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (dialog.exec() != QDialog::Accepted || summaries.isEmpty()) {
        return false;
    }

    const int row = table->currentRow();
    if (row < 0 || row >= summaries.size()) {
        return false;
    }

    *selectedSummary = summaries.at(row);
    return true;
}

QString BatchImportDialog::writeTemporaryMappingFile() const
{
    QDir().mkpath(AppConfig::logsDir());
    const QString tempPath = AppConfig::logsDir() + QStringLiteral("/import_temp_mapping.json");
    FieldMappingHelper::saveMappingProfile(tempPath, currentMappingProfile());
    return tempPath;
}

void BatchImportDialog::onSelectFileClicked()
{
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        QStringLiteral("选择钻孔数据文件"),
        AppConfig::projectRoot(),
        QStringLiteral("所有支持的文件 (*.csv *.xlsx *.xls);;CSV 文件 (*.csv);;Excel 2007+ (*.xlsx);;Excel 97-2003 (*.xls)"));

    if (filePath.isEmpty()) {
        return;
    }

    if (!isSupportedImportFile(filePath)) {
        QMessageBox::warning(
            this,
            QStringLiteral("格式不支持"),
            QStringLiteral("请选择 .csv、.xlsx 或 .xls 格式的文件。"));
        return;
    }

    QStringList columns;
    QString errorMessage;
    if (!readFileColumns(filePath, &columns, &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("读取失败"), errorMessage);
        return;
    }

    if (columns.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("读取失败"), QStringLiteral("文件中没有可用列名。"));
        return;
    }

    m_selectedFilePath = filePath;
    m_filePathLabel->setText(QStringLiteral("已选: %1").arg(filePath));
    applyFileColumnsWithMatchCheck(columns, false);
    resetProgress();
}

void BatchImportDialog::onTargetTableChanged(int index)
{
    Q_UNUSED(index);

    if (m_selectedFilePath.isEmpty()) {
        updateCriticalFieldStatus();
        return;
    }

    QStringList columns = m_currentFileColumns;
    if (columns.isEmpty()) {
        QString errorMessage;
        if (!readFileColumns(m_selectedFilePath, &columns, &errorMessage)) {
            return;
        }
    }

    applyFileColumnsWithMatchCheck(columns, true);
}

void BatchImportDialog::onSaveMappingClicked()
{
    if (m_mappingTable->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择文件后再保存映射配置。"));
        return;
    }

    const QString targetTable = currentTargetTable();
    const QString suggestedName = FieldMappingHelper::suggestedConfigName(targetTable);

    bool ok = false;
    const QString configName = QInputDialog::getText(
        this,
        QStringLiteral("保存映射配置"),
        QStringLiteral(
            "请输入配置名称（同一目标表内不可重复）。\n"
            "建议格式：[目标表][用途描述][日期]\n"
            "例如：钻孔概况表_甲方格式_20260626"),
        QLineEdit::Normal,
        suggestedName,
        &ok);

    if (!ok || configName.trimmed().isEmpty()) {
        return;
    }

    const QString trimmedName = configName.trimmed();
    QString existingPath;
    if (FieldMappingHelper::configExistsForTable(targetTable, trimmedName, &existingPath)) {
        const int answer = QMessageBox::question(
            this,
            QStringLiteral("配置名称已存在"),
            QStringLiteral("配置名称「%1」在目标表「%2」下已存在，是否覆盖？")
                .arg(trimmedName, FieldMappingHelper::tableDisplayName(targetTable)));
        if (answer != QMessageBox::Yes) {
            return;
        }
    }

    MappingProfile profile = currentMappingProfile();
    profile.configName = trimmedName;
    profile.lastUsed = QDateTime::currentDateTime().toString(Qt::ISODate);

    const QString savePath = existingPath.isEmpty()
        ? FieldMappingHelper::mappingPathForProfile(targetTable, trimmedName)
        : existingPath;

    if (FieldMappingHelper::saveMappingProfile(savePath, profile)) {
        QMessageBox::information(
            this,
            QStringLiteral("保存成功"),
            QStringLiteral(
                "映射配置「%1」已保存。\n\n"
                "绑定信息：\n"
                "• 目标表：%2\n"
                "• 文件列结构：%3 列\n"
                "• 配置文件：%4\n\n"
                "下次选择列名结构相同的文件时，将自动加载最近使用的匹配配置。")
                .arg(trimmedName,
                     FieldMappingHelper::tableDisplayName(profile.targetTable),
                     QString::number(profile.sourceColumns.size()),
                     QFileInfo(savePath).fileName()));
    } else {
        QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入映射配置文件。"));
    }
}

void BatchImportDialog::onLoadMappingClicked()
{
    if (m_mappingTable->rowCount() == 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选择文件后再加载映射配置。"));
        return;
    }

    SavedMappingConfigSummary selectedSummary;
    if (!showLoadMappingDialog(&selectedSummary)) {
        return;
    }

    loadMappingConfigAtPath(selectedSummary.filePath, true);
}

QString BatchImportDialog::writeConflictResolutionsFile(const QHash<QString, QString>& resolutions) const
{
    QDir().mkpath(AppConfig::logsDir());
    const QString filePath = AppConfig::logsDir() + QStringLiteral("/import_conflict_resolutions.json");

    QJsonObject root;
    for (auto it = resolutions.constBegin(); it != resolutions.constEnd(); ++it) {
        root.insert(it.key(), it.value());
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        return QString();
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();
    return filePath;
}

bool BatchImportDialog::resolveImportConflicts(const QString& mappingPath,
                                               QHash<QString, QString>* resolutions)
{
    if (!resolutions) {
        return false;
    }
    resolutions->clear();

    QJsonObject analyzeResult;
    QString analyzeError;
    const bool analyzeOk = PythonRunner::runScript(
        QStringLiteral("batch_import.py"),
        {
            QStringLiteral("--file_path"), m_selectedFilePath,
            QStringLiteral("--mapping_file"), mappingPath,
            QStringLiteral("--target_table"), currentTargetTable(),
            QStringLiteral("--analyze_conflicts")
        },
        &analyzeResult,
        &analyzeError);

    if (!analyzeOk) {
        const QString message = analyzeResult.value(QStringLiteral("message")).toString(analyzeError);
        QMessageBox::warning(this, QStringLiteral("冲突分析失败"), message);
        return false;
    }

    const QJsonArray conflicts = analyzeResult.value(QStringLiteral("conflicts")).toArray();
    if (conflicts.isEmpty()) {
        return true;
    }

    QString batchAction;
    for (const QJsonValue& value : conflicts) {
        const QJsonObject conflict = value.toObject();
        const QString recordKey = conflict.value(QStringLiteral("record_key")).toString();

        if (!batchAction.isEmpty()) {
            resolutions->insert(recordKey, batchAction);
            continue;
        }

        ImportConflictDialog conflictDialog(conflict, this);
        if (conflictDialog.exec() != QDialog::Accepted) {
            m_resultLabel->setText(QStringLiteral("导入已取消"));
            return false;
        }

        const QString action = ImportConflictDialog::actionToString(conflictDialog.selectedAction());
        resolutions->insert(recordKey, action);

        if (conflictDialog.applyToAllConflicts()) {
            batchAction = action;
        }
    }

    return true;
}

void BatchImportDialog::onStartImportClicked()
{
    if (m_selectedFilePath.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("提示"), QStringLiteral("请先选择要导入的文件。"));
        return;
    }

    if (!validateCriticalMapping()) {
        return;
    }

    const QString mappingPath = writeTemporaryMappingFile();

    m_btnStartImport->setEnabled(false);
    m_progressBar->setValue(10);
    m_resultLabel->setText(QStringLiteral("正在分析数据冲突..."));

    QHash<QString, QString> conflictResolutions;
    if (!resolveImportConflicts(mappingPath, &conflictResolutions)) {
        m_progressBar->setValue(0);
        updateImportButtonState();
        return;
    }

    QStringList importArgs = {
        QStringLiteral("--file_path"), m_selectedFilePath,
        QStringLiteral("--mapping_file"), mappingPath,
        QStringLiteral("--target_table"), currentTargetTable()
    };

    if (!conflictResolutions.isEmpty()) {
        const QString resolutionPath = writeConflictResolutionsFile(conflictResolutions);
        if (resolutionPath.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("导入失败"), QStringLiteral("无法写入冲突处理策略文件。"));
            m_progressBar->setValue(0);
            updateImportButtonState();
            return;
        }
        importArgs << QStringLiteral("--conflict_resolutions") << resolutionPath;
    }

    m_progressBar->setValue(40);
    m_resultLabel->setText(QStringLiteral("正在导入，请稍候..."));

    QJsonObject result;
    QString errorMessage;
    const bool ok = PythonRunner::runScript(
        QStringLiteral("batch_import.py"),
        importArgs,
        &result,
        &errorMessage);

    m_progressBar->setValue(100);
    updateImportButtonState();

    if (!ok) {
        const QString message = result.value(QStringLiteral("message")).toString(errorMessage);
        m_resultLabel->setText(QStringLiteral("导入失败"));
        QMessageBox::warning(this, QStringLiteral("批量导入失败"), message);
        return;
    }

    const int totalCount = result.value(QStringLiteral("total")).toInt();
    const int insertedCount = result.value(QStringLiteral("inserted")).toInt();
    const int overwrittenCount = result.value(QStringLiteral("overwritten")).toInt();
    const int mergedCount = result.value(QStringLiteral("merged")).toInt();
    const int skippedCount = result.value(QStringLiteral("skipped")).toInt();
    const int failedCount = result.value(QStringLiteral("failed")).toInt();
    m_lastLogPath = result.value(QStringLiteral("log_path")).toString();
    const QString targetTable = result.value(QStringLiteral("table")).toString(currentTargetTable());
    const QJsonArray errorRecords = result.value(QStringLiteral("error_records")).toArray();
    const QJsonObject sampleRenameInfo = result.value(QStringLiteral("sample_id_renames")).toObject();

    updateResultLabel(totalCount, insertedCount, overwrittenCount, mergedCount, skippedCount, failedCount);
    updateLogActionButtons();
    emit importCompleted(totalCount, insertedCount, overwrittenCount, mergedCount, skippedCount,
                         failedCount, targetTable, errorRecords, sampleRenameInfo, m_lastLogPath);
}

void BatchImportDialog::onCancelClicked()
{
    reject();
}
