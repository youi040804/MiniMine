#include "ExportDataDialog.h"
#include "FieldMappingHelper.h"
#include "PythonRunner.h"
#include "AppConfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QFileDialog>
#include <QMessageBox>
#include <QJsonObject>
#include <QDateTime>
#include <QFileInfo>

ExportDataDialog::ExportDataDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("导出数据"));
    setMinimumWidth(440);

    auto* mainLayout = new QVBoxLayout(this);

    auto* tableRow = new QHBoxLayout();
    tableRow->addWidget(new QLabel(QStringLiteral("目标表："), this));
    m_tableCombo = new QComboBox(this);
    for (const QString& tableName : FieldMappingHelper::tableNames()) {
        m_tableCombo->addItem(
            QStringLiteral("%1（%2）")
                .arg(FieldMappingHelper::tableDisplayName(tableName), tableName),
            tableName);
    }
    tableRow->addWidget(m_tableCombo, 1);
    mainLayout->addLayout(tableRow);

    auto* formatRow = new QHBoxLayout();
    formatRow->addWidget(new QLabel(QStringLiteral("导出格式："), this));
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem(QStringLiteral("CSV"), QStringLiteral("csv"));
    m_formatCombo->addItem(QStringLiteral("Excel (.xlsx)"), QStringLiteral("xlsx"));
    formatRow->addWidget(m_formatCombo, 1);
    mainLayout->addLayout(formatRow);

    mainLayout->addSpacing(8);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* btnExport = new QPushButton(QStringLiteral("导出"), this);
    auto* btnCancel = new QPushButton(QStringLiteral("取消"), this);
    btnExport->setDefault(true);
    buttonLayout->addWidget(btnExport);
    buttonLayout->addWidget(btnCancel);
    mainLayout->addLayout(buttonLayout);

    connect(btnExport, &QPushButton::clicked, this, &ExportDataDialog::onExportClicked);
    connect(btnCancel, &QPushButton::clicked, this, &QDialog::reject);
}

QString ExportDataDialog::currentTableName() const
{
    return m_tableCombo->currentData().toString();
}

QString ExportDataDialog::currentFormat() const
{
    return m_formatCombo->currentData().toString();
}

QString ExportDataDialog::suggestedFileName() const
{
    const QString timestamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"));
    const QString extension = currentFormat() == QStringLiteral("xlsx")
        ? QStringLiteral("xlsx")
        : QStringLiteral("csv");
    return QStringLiteral("%1_%2.%3").arg(currentTableName(), timestamp, extension);
}

QString ExportDataDialog::ensureOutputExtension(const QString& filePath) const
{
    const QString expectedExt = currentFormat() == QStringLiteral("xlsx")
        ? QStringLiteral("xlsx")
        : QStringLiteral("csv");
    const QString actualExt = QFileInfo(filePath).suffix().toLower();
    if (actualExt == expectedExt) {
        return filePath;
    }
    return filePath + QStringLiteral(".") + expectedExt;
}

void ExportDataDialog::onExportClicked()
{
    const QString tableName = currentTableName();
    const QString format = currentFormat();
    const QString filter = format == QStringLiteral("xlsx")
        ? QStringLiteral("Excel 文件 (*.xlsx)")
        : QStringLiteral("CSV 文件 (*.csv)");

    const QString selectedPath = QFileDialog::getSaveFileName(
        this,
        QStringLiteral("保存导出文件"),
        AppConfig::projectRoot() + QStringLiteral("/") + suggestedFileName(),
        filter);

    if (selectedPath.isEmpty()) {
        return;
    }

    const QString outputPath = ensureOutputExtension(selectedPath);

    QJsonObject result;
    QString errorMessage;
    const bool ok = PythonRunner::runScript(
        QStringLiteral("export_data.py"),
        {
            QStringLiteral("--target_table"), tableName,
            QStringLiteral("--format"), format,
            QStringLiteral("--output_path"), outputPath,
        },
        &result,
        &errorMessage);

    if (!ok) {
        QMessageBox::warning(
            this,
            QStringLiteral("导出失败"),
            errorMessage.isEmpty() ? QStringLiteral("导出过程中发生未知错误。") : errorMessage);
        return;
    }

    const int rowCount = result.value(QStringLiteral("row_count")).toInt(0);
    const QString savedPath = result.value(QStringLiteral("output_path")).toString(outputPath);
    QMessageBox::information(
        this,
        QStringLiteral("导出成功"),
        QStringLiteral("已成功导出 %1 条记录到：\n%2")
            .arg(rowCount)
            .arg(savedPath));
    accept();
}
