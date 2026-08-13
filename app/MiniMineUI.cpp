#include "MiniMineUI.h"
#include "SingleEntryDialog.h"
#include "BatchImportDialog.h"
#include "ExportDataDialog.h"
#include "DrillDetailDialog.h"
#include "DatabaseManager.h"
#include "FieldMappingHelper.h"
#include "AppConfig.h"

#include <QMessageBox>
#include <QDateTime>
#include <QFileDialog>
#include <QFile>
#include <QTableWidgetItem>
#include <QFileInfo>
#include <QDir>
#include <QDialog>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QPushButton>
#include <QJsonArray>
#include <QJsonObject>

namespace {

QString formatImportTimeDisplay(const QString& dbTime)
{
    const QString trimmed = dbTime.trimmed();
    if (trimmed.isEmpty()) {
        return QStringLiteral("—");
    }

    QDateTime dateTime = QDateTime::fromString(trimmed, QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (!dateTime.isValid()) {
        dateTime = QDateTime::fromString(trimmed, QStringLiteral("yyyy-MM-dd HH:mm"));
    }
    if (!dateTime.isValid()) {
        return trimmed;
    }
    return dateTime.toString(QStringLiteral("yyyy-MM-dd hh:mm"));
}

} // namespace

MiniMineUI::MiniMineUI(QWidget* parent)
    : QMainWindow(parent)
{
    setupUI();
    loadSampleData();
    updateStatusBar();
}

MiniMineUI::~MiniMineUI()
{
    DatabaseManager::instance().close();
}

void MiniMineUI::setupUI()
{
    setWindowTitle("MiniMine 数字矿山系统 - 钻孔数据管理");
    resize(1000, 600);

    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);

    QHBoxLayout* buttonLayout = new QHBoxLayout();

    m_btnSingleEntry = new QPushButton("📝 单孔录入", this);
    m_btnBatchImport = new QPushButton("📂 批量导入", this);
    m_btnExportData = new QPushButton("💾 导出数据", this);
    m_btnRefresh3D = new QPushButton("🔄 刷新三维", this);

    QSize btnSize(120, 35);
    m_btnSingleEntry->setFixedSize(btnSize);
    m_btnBatchImport->setFixedSize(btnSize);
    m_btnExportData->setFixedSize(btnSize);
    m_btnRefresh3D->setFixedSize(btnSize);

    buttonLayout->addWidget(m_btnSingleEntry);
    buttonLayout->addWidget(m_btnBatchImport);
    buttonLayout->addWidget(m_btnExportData);
    buttonLayout->addWidget(m_btnRefresh3D);
    buttonLayout->addStretch();

    mainLayout->addLayout(buttonLayout);

    m_tableWidget = new QTableWidget(this);
    m_tableWidget->setColumnCount(6);
    QStringList headers = { "钻孔编号", "X坐标", "Y坐标", "Z坐标", "终孔深度", "导入时间" };
    m_tableWidget->setHorizontalHeaderLabels(headers);
    m_tableWidget->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tableWidget->setContextMenuPolicy(Qt::NoContextMenu);

    connect(m_tableWidget, &QTableWidget::cellDoubleClicked,
            this, &MiniMineUI::onTableRowDoubleClicked);

    mainLayout->addWidget(m_tableWidget);

    m_statusLabel = new QLabel(this);
    statusBar()->addWidget(m_statusLabel);

    QLabel* readOnlyHint = new QLabel("🔒 只读模式：仅支持查看与新增", this);
    readOnlyHint->setStyleSheet("color: #888888;");
    statusBar()->addPermanentWidget(readOnlyHint);

    connect(m_btnSingleEntry, &QPushButton::clicked, this, &MiniMineUI::onSingleEntryClicked);
    connect(m_btnBatchImport, &QPushButton::clicked, this, &MiniMineUI::onBatchImportClicked);
    connect(m_btnExportData, &QPushButton::clicked, this, &MiniMineUI::onExportDataClicked);
    connect(m_btnRefresh3D, &QPushButton::clicked, this, &MiniMineUI::onRefresh3DClicked);
}

void MiniMineUI::showTableMessage(const QString& message)
{
    m_isPlaceholderTable = true;
    m_tableWidget->clearSpans();
    m_tableWidget->setRowCount(1);

    auto* item = new QTableWidgetItem(message);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(item->flags() & ~Qt::ItemIsSelectable & ~Qt::ItemIsEditable);
    m_tableWidget->setItem(0, 0, item);
    m_tableWidget->setSpan(0, 0, 1, 6);
}

void MiniMineUI::loadSampleData()
{
    m_tableWidget->clearSpans();
    m_isPlaceholderTable = false;

    DatabaseManager& db = DatabaseManager::instance();
    if (!db.open(AppConfig::dbPath())) {
        showTableMessage(QStringLiteral("数据库连接失败：%1").arg(db.lastError()));
        updateStatusBar();
        return;
    }

    const QVector<DrillHoleRecord> records = db.fetchAllDrillHoles();
    if (records.isEmpty()) {
        showTableMessage(QStringLiteral("暂无数据，请导入钻孔"));
        updateStatusBar();
        return;
    }

    m_tableWidget->setRowCount(records.size());

    for (int row = 0; row < records.size(); ++row) {
        const DrillHoleRecord& record = records[row];
        m_tableWidget->setItem(row, 0, new QTableWidgetItem(record.boreholeId));
        m_tableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(record.xCoord, 'f', 2)));
        m_tableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(record.yCoord, 'f', 2)));
        m_tableWidget->setItem(row, 3, new QTableWidgetItem(QString::number(record.zCoord, 'f', 2)));
        m_tableWidget->setItem(row, 4, new QTableWidgetItem(QString::number(record.totalDepth, 'f', 2)));
        m_tableWidget->setItem(row, 5, new QTableWidgetItem(formatImportTimeDisplay(record.importTime)));
    }

    updateStatusBar();
}

void MiniMineUI::reloadDrillHoleTable()
{
    loadSampleData();
}

void MiniMineUI::updateStatusBar()
{
    const int rowCount = m_isPlaceholderTable ? 0 : m_tableWidget->rowCount();
    const QString currentTime = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm");
    m_statusLabel->setText(QString("共 %1 条记录 | 最后刷新: %2").arg(rowCount).arg(currentTime));
}

void MiniMineUI::onSingleEntryClicked()
{
    SingleEntryDialog dialog(this);
    connect(&dialog, &SingleEntryDialog::dataSaved, this, &MiniMineUI::reloadDrillHoleTable);
    dialog.exec();
}

void MiniMineUI::showImportResult(int totalCount,
                                  int insertedCount,
                                  int overwrittenCount,
                                  int mergedCount,
                                  int skippedCount,
                                  int failedCount,
                                  const QString& targetTable,
                                  const QJsonArray& errorRecords,
                                  const QJsonObject& sampleRenameInfo,
                                  const QString& logPath)
{
    const int successCount = insertedCount + overwrittenCount + mergedCount;

    QDialog dialog(this);
    dialog.setWindowTitle(QStringLiteral("📊 导入完成"));
    dialog.resize(680, failedCount > 0 ? 580 : 380);

    auto* layout = new QVBoxLayout(&dialog);

    const QString tableName = FieldMappingHelper::tableDisplayName(targetTable);
    QString summaryText = QStringLiteral("目标表：%1\n\n"
                                         "总记录数：%2 条\n"
                                         "✅ 成功入库：%3 条\n"
                                         "      ├─ 新插入：%4 条\n"
                                         "      ├─ 覆盖：%5 条（冲突弹窗选择「覆盖」，完全替换旧数据）\n"
                                         "      └─ 合并：%6 条（冲突弹窗选择「合并」，仅非空字段覆盖）\n"
                                         "⏭️ 用户跳过：%7 条（冲突弹窗选择「跳过」，保留旧数据）\n"
                                         "❌ 校验失败：%8 条（格式错误/外键不存在）")
                              .arg(tableName)
                              .arg(totalCount)
                              .arg(successCount)
                              .arg(insertedCount)
                              .arg(overwrittenCount)
                              .arg(mergedCount)
                              .arg(skippedCount)
                              .arg(failedCount);

    const int duplicateSampleIds = sampleRenameInfo.value(QStringLiteral("duplicate_original_ids")).toInt();
    const bool gradeSynced = sampleRenameInfo.value(QStringLiteral("grade_synced_from_db")).toBool(false);
    const int gradeResolved = sampleRenameInfo.value(QStringLiteral("resolved_records")).toInt();

    if (duplicateSampleIds > 0) {
        summaryText += QStringLiteral("\n\n📊 样品编号处理（样品记录表）：\n"
                                      "   - 总样品数：%1 个\n"
                                      "   - 其中 %2 个样品编号因跨钻孔重复，已自动添加钻孔后缀区分")
                           .arg(sampleRenameInfo.value(QStringLiteral("total_samples")).toInt())
                           .arg(duplicateSampleIds);

        const QJsonArray examples = sampleRenameInfo.value(QStringLiteral("examples")).toArray();
        for (const QJsonValue& value : examples) {
            const QJsonObject example = value.toObject();
            if (example.contains(QStringLiteral("resolved_sample_id"))) {
                continue;
            }
            const QString originalId = example.value(QStringLiteral("original_sample_id")).toString();
            const QJsonArray records = example.value(QStringLiteral("records")).toArray();
            QStringList parts;
            for (const QJsonValue& recordValue : records) {
                const QJsonObject record = recordValue.toObject();
                parts.append(QStringLiteral("%1 (%2)")
                                 .arg(record.value(QStringLiteral("sample_id")).toString(),
                                      record.value(QStringLiteral("borehole_id")).toString()));
            }
            if (!originalId.isEmpty() && !parts.isEmpty()) {
                summaryText += QStringLiteral("\n   - 示例：%1 → %2")
                                   .arg(originalId, parts.join(QStringLiteral(" 和 ")));
            }
        }
    }

    if (gradeSynced && gradeResolved > 0) {
        summaryText += QStringLiteral("\n\n📊 样品编号同步（品位信息表）：\n"
                                      "   - 已从样品记录表查询匹配 %1 条 sample_id")
                           .arg(gradeResolved);
        const QJsonArray gradeExamples = sampleRenameInfo.value(QStringLiteral("examples")).toArray();
        for (const QJsonValue& value : gradeExamples) {
            const QJsonObject example = value.toObject();
            if (!example.contains(QStringLiteral("resolved_sample_id"))) {
                continue;
            }
            summaryText += QStringLiteral("\n   - 示例：%1 + %2 → %3")
                               .arg(example.value(QStringLiteral("original_sample_id")).toString(),
                                    example.value(QStringLiteral("borehole_id")).toString(),
                                    example.value(QStringLiteral("resolved_sample_id")).toString());
        }
    }

    auto* summaryLabel = new QLabel(summaryText, &dialog);
    summaryLabel->setWordWrap(true);
    layout->addWidget(summaryLabel);

    if (failedCount > 0 && !errorRecords.isEmpty()) {
        auto* previewLabel = new QLabel(QStringLiteral("校验失败明细："), &dialog);
        layout->addWidget(previewLabel);

        auto* table = new QTableWidget(&dialog);
        table->setColumnCount(4);
        table->setHorizontalHeaderLabels({
            QStringLiteral("行号"),
            QStringLiteral("字段名"),
            QStringLiteral("原始值"),
            QStringLiteral("错误原因")
        });
        table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->verticalHeader()->setVisible(false);

        const int previewCount = qMin(errorRecords.size(), 10);
        table->setRowCount(previewCount);
        for (int i = 0; i < previewCount; ++i) {
            const QJsonObject record = errorRecords.at(i).toObject();
            table->setItem(i, 0, new QTableWidgetItem(QString::number(record.value(QStringLiteral("row_num")).toInt())));
            table->setItem(i, 1, new QTableWidgetItem(record.value(QStringLiteral("field_name")).toString()));
            table->setItem(i, 2, new QTableWidgetItem(record.value(QStringLiteral("original_value")).toString()));
            table->setItem(i, 3, new QTableWidgetItem(record.value(QStringLiteral("reason")).toString()));
        }
        layout->addWidget(table, 1);

        if (failedCount > previewCount) {
            auto* moreLabel = new QLabel(
                QStringLiteral("... 还有 %1 条校验失败记录，请下载错误日志查看完整明细")
                    .arg(failedCount - previewCount),
                &dialog);
            moreLabel->setStyleSheet(QStringLiteral("color: #666666;"));
            layout->addWidget(moreLabel);
        }
    }

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();

    QPushButton* downloadBtn = nullptr;
    if (!logPath.isEmpty()) {
        downloadBtn = new QPushButton(QStringLiteral("📥 下载错误日志"), &dialog);
        buttonLayout->addWidget(downloadBtn);
    }

    auto* okBtn = new QPushButton(QStringLiteral("确定"), &dialog);
    okBtn->setDefault(true);
    buttonLayout->addWidget(okBtn);
    layout->addLayout(buttonLayout);

    connect(okBtn, &QPushButton::clicked, &dialog, &QDialog::accept);

    if (downloadBtn) {
        connect(downloadBtn, &QPushButton::clicked, &dialog, [&dialog, this, logPath]() {
            const QString defaultName = QFileInfo(logPath).fileName();
            const QString savePath = QFileDialog::getSaveFileName(
                &dialog,
                QStringLiteral("保存错误日志"),
                QDir::homePath() + QStringLiteral("/") + defaultName,
                QStringLiteral("日志文件 (*.log);;文本文件 (*.txt);;所有文件 (*.*)"));

            if (savePath.isEmpty()) {
                return;
            }

            if (QFile::exists(savePath)) {
                QFile::remove(savePath);
            }

            if (QFile::copy(logPath, savePath)) {
                QMessageBox::information(&dialog, QStringLiteral("成功"),
                                         QStringLiteral("错误日志已保存到:\n%1").arg(savePath));
            } else {
                QMessageBox::warning(&dialog, QStringLiteral("失败"),
                                     QStringLiteral("保存日志文件失败，请检查目录权限。"));
            }
        });
    }

    dialog.exec();
}

void MiniMineUI::onBatchImportClicked()
{
    BatchImportDialog dialog(this);
    connect(&dialog, &BatchImportDialog::importCompleted, this,
            [this](int totalCount, int insertedCount, int overwrittenCount, int mergedCount,
                   int skippedCount, int failedCount, const QString& targetTable,
                   const QJsonArray& errorRecords, const QJsonObject& sampleRenameInfo,
                   const QString& logPath) {
                reloadDrillHoleTable();
                showImportResult(totalCount, insertedCount, overwrittenCount, mergedCount,
                                 skippedCount, failedCount, targetTable, errorRecords,
                                 sampleRenameInfo, logPath);
            });
    dialog.exec();
}

void MiniMineUI::onExportDataClicked()
{
    ExportDataDialog dialog(this);
    dialog.exec();
}

void MiniMineUI::onRefresh3DClicked()
{
    QMessageBox::information(
        this,
        QStringLiteral("三维场景"),
        QStringLiteral(
            "数据已保存在 SQLite 数据库中，可供下游三维建模模块读取。\n\n"
            "本版本暂未与 QuantyView3D 三维引擎做自动联动刷新。"
            "请在 QuantyView3D 中打开对应数据源查看最新钻孔模型。"));
}

void MiniMineUI::onTableRowDoubleClicked(int row, int column)
{
    Q_UNUSED(column);

    if (row < 0 || m_isPlaceholderTable) {
        return;
    }

    QTableWidgetItem* item = m_tableWidget->item(row, 0);
    if (!item) {
        return;
    }

    const QString drillNo = item->text();
    DrillDetailDialog dialog(drillNo, this);
    dialog.exec();
}
