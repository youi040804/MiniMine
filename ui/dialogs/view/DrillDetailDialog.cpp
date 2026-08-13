#include "DrillDetailDialog.h"
#include "DatabaseManager.h"
#include "AppConfig.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QTabWidget>
#include <QTableWidget>
#include <QLabel>
#include <QPushButton>
#include <QHeaderView>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMessageBox>
#include <QTableWidgetItem>
#include <QItemSelectionModel>

namespace {

QString formatDisplayValue(const QVariant& value, int decimals = 2)
{
    if (!value.isValid() || value.isNull()) {
        return QStringLiteral("—");
    }

    if (value.metaType().id() == QMetaType::Double
        || value.metaType().id() == QMetaType::Float) {
        return QString::number(value.toDouble(), 'f', decimals);
    }

    const QString text = value.toString().trimmed();
    if (text.isEmpty()) {
        return QStringLiteral("—");
    }

    bool ok = false;
    const double numeric = text.toDouble(&ok);
    if (ok && text.contains(QLatin1Char('.'))) {
        return QString::number(numeric, 'f', decimals);
    }

    return text;
}

QTableWidgetItem* createReadOnlyItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    item->setFlags(item->flags() & ~Qt::ItemIsEditable);
    return item;
}

void addFormRow(QGridLayout* grid, int row, const QString& label, const QString& value)
{
    auto* labelWidget = new QLabel(label);
    labelWidget->setStyleSheet(QStringLiteral("color: #555555;"));
    auto* valueWidget = new QLabel(value.isEmpty() ? QStringLiteral("—") : value);
    valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
    valueWidget->setWordWrap(true);
    grid->addWidget(labelWidget, row, 0, Qt::AlignTop);
    grid->addWidget(valueWidget, row, 1);
}

void clearGridLayout(QGridLayout* grid)
{
    if (!grid) {
        return;
    }

    while (QLayoutItem* item = grid->takeAt(0)) {
        if (QWidget* widget = item->widget()) {
            widget->deleteLater();
        }
        delete item;
    }
}

void populateExtraDataGrid(QGridLayout* grid, const QString& extraDataJson)
{
    clearGridLayout(grid);
    if (!grid) {
        return;
    }

    const QString trimmed = extraDataJson.trimmed();
    if (trimmed.isEmpty()) {
        auto* emptyLabel = new QLabel(QStringLiteral("（无扩展字段）"));
        emptyLabel->setStyleSheet(QStringLiteral("color: #888888;"));
        grid->addWidget(emptyLabel, 0, 0, 1, 2);
        return;
    }

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        addFormRow(grid, 0, QStringLiteral("原始 JSON"), trimmed);
        return;
    }

    const QJsonObject extra = doc.object();
    if (extra.isEmpty()) {
        auto* emptyLabel = new QLabel(QStringLiteral("（无扩展字段）"));
        emptyLabel->setStyleSheet(QStringLiteral("color: #888888;"));
        grid->addWidget(emptyLabel, 0, 0, 1, 2);
        return;
    }

    int row = 0;
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        addFormRow(grid, row++, it.key(), it.value().toVariant().toString());
    }
}

QGroupBox* createExtraDataGroup(QWidget* parent, QGridLayout** gridOut)
{
    auto* group = new QGroupBox(QStringLiteral("扩展字段（EXTRA_DATA）"), parent);
    auto* grid = new QGridLayout(group);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(8);
    if (gridOut) {
        *gridOut = grid;
    }
    return group;
}

} // namespace

DrillDetailDialog::DrillDetailDialog(const QString& boreholeId, QWidget* parent)
    : QDialog(parent)
    , m_boreholeId(boreholeId.trimmed())
{
    setWindowTitle(QStringLiteral("钻孔详情 - %1").arg(m_boreholeId));
    setModal(true);
    resize(920, 640);

    setupUI();

    if (!loadData()) {
        return;
    }
}

void DrillDetailDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(12, 12, 12, 12);
    mainLayout->setSpacing(10);

    m_tabWidget = new QTabWidget(this);
    mainLayout->addWidget(m_tabWidget, 1);

    auto* readOnlyHint = new QLabel(
        QStringLiteral("🔒 只读模式：此界面仅用于查看，不支持修改或删除"),
        this);
    readOnlyHint->setStyleSheet(QStringLiteral("color: #888888; padding: 4px 0;"));
    mainLayout->addWidget(readOnlyHint);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    auto* btnClose = new QPushButton(QStringLiteral("关闭"), this);
    btnClose->setDefault(true);
    connect(btnClose, &QPushButton::clicked, this, &QDialog::accept);
    buttonLayout->addWidget(btnClose);
    mainLayout->addLayout(buttonLayout);
}

bool DrillDetailDialog::loadData()
{
    DatabaseManager& db = DatabaseManager::instance();
    if (!db.open(AppConfig::dbPath())) {
        QMessageBox::warning(
            this,
            QStringLiteral("加载失败"),
            QStringLiteral("数据库连接失败：%1").arg(db.lastError()));
        reject();
        return false;
    }

    DrillHoleFullDetail detail;
    if (!db.fetchDrillHoleDetail(m_boreholeId, &detail)) {
        QMessageBox::warning(
            this,
            QStringLiteral("加载失败"),
            db.lastError());
        reject();
        return false;
    }

    m_gradesBySampleId = detail.gradesBySampleId;

    m_tabWidget->addTab(createBasicInfoTab(detail.basic), QStringLiteral("钻孔概况"));
    m_tabWidget->addTab(createInclineTab(detail.inclines), QStringLiteral("测斜数据"));
    m_tabWidget->addTab(createStrataTab(detail.strata), QStringLiteral("地层分层"));
    m_tabWidget->addTab(createSampleTab(detail.samples, detail.gradesBySampleId),
                        QStringLiteral("样品数据"));
    return true;
}

QWidget* DrillDetailDialog::createEmptyStateWidget(const QString& message)
{
    auto* widget = new QWidget(this);
    auto* layout = new QVBoxLayout(widget);
    auto* label = new QLabel(message, widget);
    label->setAlignment(Qt::AlignCenter);
    label->setStyleSheet(QStringLiteral("color: #888888; padding: 24px;"));
    layout->addStretch();
    layout->addWidget(label);
    layout->addStretch();
    return widget;
}

QWidget* DrillDetailDialog::createBasicInfoTab(const DrillHoleDetailRecord& basic)
{
    auto* page = new QWidget(this);
    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);

    auto* group = new QGroupBox(QStringLiteral("钻孔基本信息"), content);
    auto* grid = new QGridLayout(group);
    grid->setColumnStretch(1, 1);
    grid->setHorizontalSpacing(16);
    grid->setVerticalSpacing(10);

    int row = 0;
    addFormRow(grid, row++, QStringLiteral("钻孔编号"), basic.boreholeId);
    addFormRow(grid, row++, QStringLiteral("勘探区编号"), basic.areaId);
    addFormRow(grid, row++, QStringLiteral("X坐标"), formatDisplayValue(basic.xCoord));
    addFormRow(grid, row++, QStringLiteral("Y坐标"), formatDisplayValue(basic.yCoord));
    addFormRow(grid, row++, QStringLiteral("Z坐标"), formatDisplayValue(basic.zCoord));
    addFormRow(grid, row++, QStringLiteral("终孔深度"), formatDisplayValue(basic.totalDepth));
    addFormRow(grid, row++, QStringLiteral("钻孔倾角"), formatDisplayValue(basic.dipAngle));
    addFormRow(grid, row++, QStringLiteral("钻孔方位角"), formatDisplayValue(basic.azimuth));

    layout->addWidget(group);

    QGridLayout* extraGrid = nullptr;
    auto* extraGroup = createExtraDataGroup(content, &extraGrid);
    populateExtraDataGrid(extraGrid, basic.extraDataJson);
    layout->addWidget(extraGroup);
    layout->addStretch();

    scroll->setWidget(content);

    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);
    pageLayout->addWidget(scroll);
    return page;
}

void DrillDetailDialog::setupReadOnlyTable(QTableWidget* table)
{
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
    table->setStyleSheet(QStringLiteral(
        "QTableWidget {"
        "  selection-background-color: #f0f7ff;"
        "  selection-color: #000000;"
        "  outline: none;"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: #f0f7ff;"
        "  color: #000000;"
        "}"
        "QTableWidget::item:focus {"
        "  background-color: #e6f2ff;"
        "  color: #000000;"
        "}"));
}

QWidget* DrillDetailDialog::createInclineTab(const QVector<InclineDetailRecord>& records)
{
    if (records.isEmpty()) {
        return createEmptyStateWidget(QStringLiteral("该钻孔暂无测斜数据"));
    }

    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);

    auto* table = new QTableWidget(content);
    table->setColumnCount(4);
    table->setHorizontalHeaderLabels({
        QStringLiteral("测点号"),
        QStringLiteral("测点深度"),
        QStringLiteral("偏斜角采用值"),
        QStringLiteral("方位角采用值"),
    });
    setupReadOnlyTable(table);
    table->setRowCount(records.size());

    for (int row = 0; row < records.size(); ++row) {
        const InclineDetailRecord& record = records[row];
        table->setItem(row, 0, createReadOnlyItem(QString::number(record.pointId)));
        table->setItem(row, 1, createReadOnlyItem(formatDisplayValue(record.pointDepth)));
        table->setItem(row, 2, createReadOnlyItem(formatDisplayValue(record.deviationAngle)));
        table->setItem(row, 3, createReadOnlyItem(formatDisplayValue(record.azimuth)));
    }

    layout->addWidget(table);

    QGridLayout* extraGrid = nullptr;
    auto* extraGroup = createExtraDataGroup(content, &extraGrid);
    layout->addWidget(extraGroup);
    layout->addStretch();

    scroll->setWidget(content);
    pageLayout->addWidget(scroll);

    connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            table, [records, extraGrid](const QModelIndex& current, const QModelIndex& previous) {
                Q_UNUSED(previous);
                if (!current.isValid() || current.row() < 0 || current.row() >= records.size()) {
                    populateExtraDataGrid(extraGrid, QString());
                    return;
                }
                populateExtraDataGrid(extraGrid, records.at(current.row()).extraDataJson);
            });

    table->selectRow(0);
    return page;
}

QWidget* DrillDetailDialog::createStrataTab(const QVector<StrataDetailRecord>& records)
{
    if (records.isEmpty()) {
        return createEmptyStateWidget(QStringLiteral("该钻孔暂无地层分层数据"));
    }

    auto* page = new QWidget(this);
    auto* pageLayout = new QVBoxLayout(page);
    pageLayout->setContentsMargins(0, 0, 0, 0);

    auto* scroll = new QScrollArea(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget(scroll);
    auto* layout = new QVBoxLayout(content);

    auto* table = new QTableWidget(content);
    table->setColumnCount(5);
    table->setHorizontalHeaderLabels({
        QStringLiteral("岩层序号"),
        QStringLiteral("分层号"),
        QStringLiteral("岩石分层孔深（底深）"),
        QStringLiteral("岩石全名"),
        QStringLiteral("岩层倾角"),
    });
    setupReadOnlyTable(table);
    table->setRowCount(records.size());

    for (int row = 0; row < records.size(); ++row) {
        const StrataDetailRecord& record = records[row];
        table->setItem(row, 0, createReadOnlyItem(QString::number(record.layerOrder)));
        table->setItem(row, 1, createReadOnlyItem(record.layerNo.isEmpty()
            ? QStringLiteral("—")
            : record.layerNo));
        table->setItem(row, 2, createReadOnlyItem(formatDisplayValue(record.bottomDepth)));
        table->setItem(row, 3, createReadOnlyItem(record.rockName.isEmpty()
            ? QStringLiteral("—")
            : record.rockName));
        table->setItem(row, 4, createReadOnlyItem(formatDisplayValue(record.dipAngle)));
    }

    layout->addWidget(table);

    QGridLayout* extraGrid = nullptr;
    auto* extraGroup = createExtraDataGroup(content, &extraGrid);
    layout->addWidget(extraGroup);
    layout->addStretch();

    scroll->setWidget(content);
    pageLayout->addWidget(scroll);

    connect(table->selectionModel(), &QItemSelectionModel::currentRowChanged,
            table, [records, extraGrid](const QModelIndex& current, const QModelIndex& previous) {
                Q_UNUSED(previous);
                if (!current.isValid() || current.row() < 0 || current.row() >= records.size()) {
                    populateExtraDataGrid(extraGrid, QString());
                    return;
                }
                populateExtraDataGrid(extraGrid, records.at(current.row()).extraDataJson);
            });

    table->selectRow(0);
    return page;
}

QWidget* DrillDetailDialog::createSampleTab(const QVector<SampleDetailRecord>& samples,
                                            const QHash<QString, QVector<GradeDetailRecord>>& gradesBySampleId)
{
    Q_UNUSED(gradesBySampleId);

    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);
    layout->setSpacing(12);

    if (samples.isEmpty()) {
        layout->addWidget(createEmptyStateWidget(QStringLiteral("该钻孔暂无样品数据")));
        return page;
    }

    m_samples = samples;

    auto* sampleTitle = new QLabel(QStringLiteral("【样品列表】"), page);
    sampleTitle->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(sampleTitle);

    m_sampleTable = new QTableWidget(page);
    m_sampleTable->setColumnCount(5);
    m_sampleTable->setHorizontalHeaderLabels({
        QStringLiteral("样品编号"),
        QStringLiteral("采样起始孔深"),
        QStringLiteral("采样终止孔深"),
        QStringLiteral("样长"),
        QStringLiteral("样品类型"),
    });
    setupReadOnlyTable(m_sampleTable);
    m_sampleTable->setMaximumHeight(220);
    m_sampleTable->setRowCount(samples.size());

    for (int row = 0; row < samples.size(); ++row) {
        const SampleDetailRecord& record = samples[row];
        auto* idItem = createReadOnlyItem(record.sampleId);
        idItem->setData(Qt::UserRole, record.sampleId);
        m_sampleTable->setItem(row, 0, idItem);
        m_sampleTable->setItem(row, 1, createReadOnlyItem(formatDisplayValue(record.startDepth)));
        m_sampleTable->setItem(row, 2, createReadOnlyItem(formatDisplayValue(record.endDepth)));
        m_sampleTable->setItem(row, 3, createReadOnlyItem(formatDisplayValue(record.sampleLength)));
        m_sampleTable->setItem(row, 4, createReadOnlyItem(formatDisplayValue(record.sampleType, 0)));
    }

    connect(m_sampleTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& current, const QModelIndex& previous) {
                Q_UNUSED(previous);
                if (!current.isValid() || !m_sampleTable) {
                    return;
                }
                populateSampleExtraData(current.row());
                QTableWidgetItem* idItem = m_sampleTable->item(current.row(), 0);
                if (!idItem) {
                    return;
                }
                populateGradeTable(idItem->data(Qt::UserRole).toString());
            });

    layout->addWidget(m_sampleTable);

    auto* sampleExtraGroup = createExtraDataGroup(page, &m_sampleExtraGrid);
    layout->addWidget(sampleExtraGroup);

    m_gradeSectionLabel = new QLabel(QStringLiteral("【品位详情】（点击样品后显示）"), page);
    m_gradeSectionLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_gradeSectionLabel);

    m_gradeContainer = new QWidget(page);
    auto* gradeLayout = new QVBoxLayout(m_gradeContainer);
    gradeLayout->setContentsMargins(0, 0, 0, 0);

    m_gradeTable = new QTableWidget(m_gradeContainer);
    m_gradeTable->setColumnCount(2);
    m_gradeTable->setHorizontalHeaderLabels({
        QStringLiteral("元素名称"),
        QStringLiteral("元素品位值"),
    });
    setupReadOnlyTable(m_gradeTable);
    gradeLayout->addWidget(m_gradeTable);

    auto* gradeExtraGroup = createExtraDataGroup(m_gradeContainer, &m_gradeExtraGrid);
    gradeLayout->addWidget(gradeExtraGroup);

    connect(m_gradeTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& current, const QModelIndex& previous) {
                Q_UNUSED(previous);
                if (!current.isValid()) {
                    populateExtraDataGrid(m_gradeExtraGrid, QString());
                    return;
                }
                populateGradeExtraData(current.row());
            });

    m_gradeEmptyLabel = new QLabel(QStringLiteral("该样品暂无品位数据"), m_gradeContainer);
    m_gradeEmptyLabel->setAlignment(Qt::AlignCenter);
    m_gradeEmptyLabel->setStyleSheet(QStringLiteral("color: #888888; padding: 16px;"));
    m_gradeEmptyLabel->hide();
    gradeLayout->addWidget(m_gradeEmptyLabel);

    layout->addWidget(m_gradeContainer, 1);

    m_sampleTable->selectRow(0);
    return page;
}

void DrillDetailDialog::populateSampleExtraData(int sampleRow)
{
    if (!m_sampleExtraGrid || sampleRow < 0 || sampleRow >= m_samples.size()) {
        populateExtraDataGrid(m_sampleExtraGrid, QString());
        return;
    }
    populateExtraDataGrid(m_sampleExtraGrid, m_samples.at(sampleRow).extraDataJson);
}

void DrillDetailDialog::populateGradeExtraData(int gradeRow)
{
    if (!m_gradeExtraGrid || gradeRow < 0 || gradeRow >= m_currentGrades.size()) {
        populateExtraDataGrid(m_gradeExtraGrid, QString());
        return;
    }
    populateExtraDataGrid(m_gradeExtraGrid, m_currentGrades.at(gradeRow).extraDataJson);
}

void DrillDetailDialog::populateGradeTable(const QString& sampleId)
{
    if (!m_gradeTable || !m_gradeEmptyLabel) {
        return;
    }

    m_currentGrades = m_gradesBySampleId.value(sampleId);
    populateExtraDataGrid(m_gradeExtraGrid, QString());

    if (m_currentGrades.isEmpty()) {
        m_gradeTable->hide();
        m_gradeEmptyLabel->setText(QStringLiteral("该样品暂无品位数据"));
        m_gradeEmptyLabel->show();
        if (m_gradeSectionLabel) {
            m_gradeSectionLabel->setText(
                QStringLiteral("【品位详情】— %1").arg(sampleId));
        }
        return;
    }

    m_gradeEmptyLabel->hide();
    m_gradeTable->show();
    m_gradeTable->setRowCount(m_currentGrades.size());

    for (int row = 0; row < m_currentGrades.size(); ++row) {
        const GradeDetailRecord& record = m_currentGrades[row];
        m_gradeTable->setItem(row, 0, createReadOnlyItem(record.elementName));
        m_gradeTable->setItem(row, 1, createReadOnlyItem(formatDisplayValue(record.gradeValue)));
    }

    if (m_gradeSectionLabel) {
        m_gradeSectionLabel->setText(
            QStringLiteral("【品位详情】— %1").arg(sampleId));
    }

    if (m_currentGrades.size() > 0) {
        m_gradeTable->selectRow(0);
    }
}
