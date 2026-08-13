#include "SingleEntryDialog.h"
#include "ImportConflictDialog.h"
#include "PythonRunner.h"
#include "AppConfig.h"
#include "NumericInputField.h"
#include "DatabaseManager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QHeaderView>
#include <QMessageBox>
#include <QAbstractItemView>
#include <QLabel>
#include <QScrollArea>
#include <QGroupBox>
#include <QFrame>
#include <QPair>
#include <QColor>
#include <QBrush>
#include <QJsonDocument>
#include <QJsonObject>
#include <QFile>
#include <QDir>
#include <QSignalBlocker>
#include <QLineEdit>
#include <QtGlobal>

namespace {

constexpr int SampleRowTypeRole = Qt::UserRole;
constexpr int SampleLinkedIdRole = Qt::UserRole + 1;

bool isSampleElementRow(QTableWidget* table, int row)
{
    if (!table || row < 0 || row >= table->rowCount()) {
        return false;
    }
    QTableWidgetItem* item = table->item(row, 0);
    return item && item->data(SampleRowTypeRole).toString() == QLatin1String("element");
}

QColor sampleElementRowBackground()
{
    return QColor(0xf3, 0xf6, 0xfa);
}

QColor sampleElementMutedTextColor()
{
    return QColor(0x77, 0x77, 0x77);
}

QTableWidgetItem* createCenteredItem(const QString& text)
{
    auto* item = new QTableWidgetItem(text);
    item->setTextAlignment(Qt::AlignCenter);
    return item;
}

void configureSampleHeadRowItems(QTableWidget* table, int row)
{
    static const QBrush normalBrush;
    for (int col = 0; col < 7; ++col) {
        auto* item = createCenteredItem(QString());
        item->setData(SampleRowTypeRole, QStringLiteral("head"));
        item->setBackground(normalBrush);
        table->setItem(row, col, item);
    }
}

void configureSampleElementRowItems(QTableWidget* table, int row, const QString& sampleId)
{
    const QBrush rowBrush(sampleElementRowBackground());
    const QBrush mutedBrush(sampleElementMutedTextColor());

    auto* idItem = createCenteredItem(QStringLiteral("↳ %1").arg(sampleId));
    idItem->setData(SampleRowTypeRole, QStringLiteral("element"));
    idItem->setData(SampleLinkedIdRole, sampleId);
    idItem->setFlags(idItem->flags() & ~Qt::ItemIsEditable);
    idItem->setForeground(mutedBrush);
    idItem->setBackground(rowBrush);
    table->setItem(row, 0, idItem);

    for (int col = 1; col <= 4; ++col) {
        auto* item = createCenteredItem(QString());
        item->setData(SampleRowTypeRole, QStringLiteral("element"));
        item->setFlags(item->flags() & ~Qt::ItemIsEditable);
        item->setBackground(rowBrush);
        table->setItem(row, col, item);
    }

    for (int col = 5; col <= 6; ++col) {
        auto* item = createCenteredItem(QString());
        item->setData(SampleRowTypeRole, QStringLiteral("element"));
        item->setBackground(rowBrush);
        table->setItem(row, col, item);
    }
}

void refreshSampleElementRowLabels(QTableWidget* table, int headRow)
{
    if (!table || headRow < 0 || headRow >= table->rowCount()) {
        return;
    }

    QTableWidgetItem* headItem = table->item(headRow, 0);
    if (!headItem || isSampleElementRow(table, headRow)) {
        return;
    }

    const QString sampleId = headItem->text().trimmed();
    const QBrush mutedBrush(sampleElementMutedTextColor());

    for (int row = headRow + 1; row < table->rowCount(); ++row) {
        if (!isSampleElementRow(table, row)) {
            QTableWidgetItem* nextHead = table->item(row, 0);
            if (nextHead && !nextHead->text().trimmed().isEmpty()) {
                break;
            }
            continue;
        }

        QTableWidgetItem* idItem = table->item(row, 0);
        if (!idItem) {
            continue;
        }

        QSignalBlocker blocker(table);
        idItem->setText(sampleId.isEmpty()
            ? QStringLiteral("↳ …")
            : QStringLiteral("↳ %1").arg(sampleId));
        idItem->setData(SampleLinkedIdRole, sampleId);
        idItem->setForeground(mutedBrush);
    }
}

NumericInputField* createCoordField(QWidget* parent)
{
    return new NumericInputField(-99999999.0, 99999999.0, 2, parent);
}

NumericInputField* createBasicNumericField(QWidget* parent,
                                           double minimum,
                                           double maximum,
                                           int decimals)
{
    return new NumericInputField(minimum, maximum, decimals, parent);
}

void setupEditableTable(QTableWidget* table)
{
    table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setContextMenuPolicy(Qt::NoContextMenu);
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->verticalHeader()->setVisible(false);
}

void applyLightBlueTableHighlight(QTableWidget* table)
{
    table->setStyleSheet(QStringLiteral(
        "QTableWidget {"
        "  selection-background-color: #e6f2ff;"
        "  selection-color: #000000;"
        "  outline: none;"
        "}"
        "QTableWidget::item:selected {"
        "  background-color: #e6f2ff;"
        "  color: #000000;"
        "}"
        "QTableWidget::item:focus {"
        "  background-color: #cce5ff;"
        "  color: #000000;"
        "}"
        "QTableWidget QLineEdit {"
        "  background-color: #e6f2ff;"
        "  selection-background-color: #0066cc;"
        "  selection-color: #ffffff;"
        "  border: 1px solid #99c2ff;"
        "  padding: 2px;"
        "}"));
}

QWidget* createTableHeaderBar(QWidget* parent, const QList<QPair<QString, bool>>& columns)
{
    auto* bar = new QWidget(parent);
    auto* layout = new QHBoxLayout(bar);
    layout->setContentsMargins(0, 0, 0, 4);
    layout->setSpacing(0);

    for (const auto& column : columns) {
        QString html = column.first;
        if (column.second) {
            html += QStringLiteral(" <font color='red'>*</font>");
        }
        auto* label = new QLabel(html, bar);
        label->setTextFormat(Qt::RichText);
        label->setAlignment(Qt::AlignCenter);
        layout->addWidget(label, 1);
    }

    return bar;
}

void setupDataTableColumns(QTableWidget* table, int columnCount)
{
    table->setColumnCount(columnCount);
    table->horizontalHeader()->setVisible(false);
    setupEditableTable(table);
}

bool tableRowHasContent(QTableWidget* table, int row)
{
    for (int col = 0; col < table->columnCount(); ++col) {
        QTableWidgetItem* item = table->item(row, col);
        if (item && !item->text().trimmed().isEmpty()) {
            return true;
        }
    }
    return false;
}

QWidget* wrapScrollPage(QWidget* page)
{
    auto* scroll = new QScrollArea();
    scroll->setWidget(page);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    return scroll;
}

} // namespace

SingleEntryDialog::SingleEntryDialog(QWidget* parent)
    : QDialog(parent)
{
    m_stepSaved[0] = false;
    m_stepSaved[1] = false;
    m_stepSaved[2] = false;
    m_stepSaved[3] = false;

    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setWindowTitle(QStringLiteral("单孔录入"));
    setModal(true);
    resize(860, 640);

    setupUI();
    updateStepIndicator();
    updateNavigationButtons();
    updateSaveButtonState();
    updateLinkedBoreholeLabels();
}

SingleEntryDialog::~SingleEntryDialog() = default;

void SingleEntryDialog::setupUI()
{
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(8);

    auto* indicatorLayout = new QHBoxLayout();
    indicatorLayout->addStretch();

    static const QStringList stepNames = {
        QStringLiteral("钻孔概况"),
        QStringLiteral("测斜数据"),
        QStringLiteral("地层分层"),
        QStringLiteral("样品数据")
    };

    for (int i = 0; i < 4; ++i) {
        m_stepTabs[i] = new QPushButton(this);
        m_stepTabs[i]->setFlat(true);
        m_stepTabs[i]->setCursor(Qt::PointingHandCursor);
        m_stepTabs[i]->setProperty("stepIndex", i);
        connect(m_stepTabs[i], &QPushButton::clicked, this, &SingleEntryDialog::onStepTabClicked);
        indicatorLayout->addWidget(m_stepTabs[i]);
        if (i < 3) {
            indicatorLayout->addSpacing(16);
        }
    }

    indicatorLayout->addStretch();
    mainLayout->addLayout(indicatorLayout);

    m_stackedWidget = new QStackedWidget(this);
    m_stackedWidget->addWidget(wrapScrollPage(createBasicInfoPage()));
    m_stackedWidget->addWidget(wrapScrollPage(createInclinePage()));
    m_stackedWidget->addWidget(wrapScrollPage(createStrataPage()));
    m_stackedWidget->addWidget(wrapScrollPage(createSamplePage()));
    mainLayout->addWidget(m_stackedWidget, 1);

    connect(m_stackedWidget, &QStackedWidget::currentChanged,
            this, &SingleEntryDialog::onStepChanged);

    auto* buttonLayout = new QHBoxLayout();
    m_btnPrevious = new QPushButton(QStringLiteral("上一步"), this);
    m_btnNext = new QPushButton(QStringLiteral("下一步"), this);
    m_btnSave = new QPushButton(QStringLiteral("保存"), this);
    m_btnSave->setAttribute(Qt::WA_AlwaysShowToolTips);
    m_btnCancel = new QPushButton(QStringLiteral("取消"), this);

    buttonLayout->addWidget(m_btnPrevious);
    buttonLayout->addWidget(m_btnNext);
    buttonLayout->addStretch();
    buttonLayout->addWidget(m_btnSave);
    buttonLayout->addWidget(m_btnCancel);
    mainLayout->addLayout(buttonLayout);

    connect(m_btnPrevious, &QPushButton::clicked, this, &SingleEntryDialog::onPreviousClicked);
    connect(m_btnNext, &QPushButton::clicked, this, &SingleEntryDialog::onNextClicked);
    connect(m_btnSave, &QPushButton::clicked, this, &SingleEntryDialog::onSaveClicked);
    connect(m_btnCancel, &QPushButton::clicked, this, &SingleEntryDialog::onCancelClicked);

    connect(m_editBoreholeId, &QLineEdit::textChanged, this, &SingleEntryDialog::onFormChanged);
    connect(m_editAreaId, &QLineEdit::textChanged, this, &SingleEntryDialog::onFormChanged);
}

void SingleEntryDialog::appendExtensionFieldsSection(QVBoxLayout* layout,
                                                     QTableWidget** tableOut,
                                                     const QString& hintText,
                                                     const QString& groupTitle)
{
    const QString title = groupTitle.isEmpty()
        ? QStringLiteral("扩展字段（非标准列，入库为 JSON）")
        : groupTitle;
    auto* group = new QGroupBox(title, layout->parentWidget());
    auto* groupLayout = new QVBoxLayout(group);

    auto* table = new QTableWidget(group);
    table->setColumnCount(2);
    table->setHorizontalHeaderLabels({
        QStringLiteral("字段名"),
        QStringLiteral("值")
    });
    setupEditableTable(table);
    applyLightBlueTableHighlight(table);
    table->setMaximumHeight(160);
    groupLayout->addWidget(table);

    auto* btnLayout = new QHBoxLayout();
    auto* btnAdd = new QPushButton(QStringLiteral("+ 添加字段"), group);
    auto* btnDelete = new QPushButton(QStringLiteral("删除选中字段"), group);
    connect(btnAdd, &QPushButton::clicked, this, &SingleEntryDialog::onAddExtensionField);
    connect(btnDelete, &QPushButton::clicked, this, &SingleEntryDialog::onDeleteExtensionField);
    btnLayout->addWidget(btnAdd);
    btnLayout->addWidget(btnDelete);
    btnLayout->addStretch();
    groupLayout->addLayout(btnLayout);

    auto* hint = new QLabel(hintText, group);
    hint->setWordWrap(true);
    hint->setTextFormat(Qt::RichText);
    hint->setStyleSheet(QStringLiteral("color: #666666; font-size: 11px;"));
    groupLayout->addWidget(hint);

    layout->addWidget(group);
    *tableOut = table;
}

QWidget* SingleEntryDialog::createBasicInfoPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    auto* coreGroup = new QGroupBox(QStringLiteral("核心字段"), page);
    auto* grid = new QGridLayout(coreGroup);
    grid->setColumnStretch(1, 1);
    grid->setVerticalSpacing(10);

    m_editBoreholeId = new QLineEdit(coreGroup);
    m_editBoreholeId->setAlignment(Qt::AlignCenter);

    m_editAreaId = new QLineEdit(coreGroup);
    m_editAreaId->setAlignment(Qt::AlignCenter);

    m_numX = createCoordField(coreGroup);
    m_numY = createCoordField(coreGroup);
    m_numZ = createCoordField(coreGroup);

    m_numTotalDepth = createBasicNumericField(coreGroup, 0.0, 5000.0, 2);

    m_numDipAngle = createBasicNumericField(coreGroup, 0.0, 90.0, 2);

    m_numAzimuth = createBasicNumericField(coreGroup, 0.0, 360.0, 2);

    const auto connectNumericField = [this](NumericInputField* field) {
        field->setChangedCallback([this]() { onFormChanged(); });
    };
    connectNumericField(m_numX);
    connectNumericField(m_numY);
    connectNumericField(m_numZ);
    connectNumericField(m_numTotalDepth);
    connectNumericField(m_numDipAngle);
    connectNumericField(m_numAzimuth);

    struct FieldSpec {
        QString label;
        QWidget* widget;
        bool required;
    };

    const QList<FieldSpec> fields = {
        { QStringLiteral("勘探区编号"), m_editAreaId, false },
        { QStringLiteral("钻孔编号"), m_editBoreholeId, true },
        { QStringLiteral("X坐标"), m_numX, true },
        { QStringLiteral("Y坐标"), m_numY, true },
        { QStringLiteral("Z坐标"), m_numZ, true },
        { QStringLiteral("终孔深度"), m_numTotalDepth, true },
        { QStringLiteral("钻孔倾角"), m_numDipAngle, false },
        { QStringLiteral("钻孔方位角"), m_numAzimuth, false }
    };

    for (int row = 0; row < fields.size(); ++row) {
        QString labelText = fields[row].label;
        if (fields[row].required) {
            labelText += QStringLiteral(" <font color='red'>*</font>");
        }
        auto* label = new QLabel(labelText, coreGroup);
        label->setTextFormat(Qt::RichText);
        grid->addWidget(label, row, 0);
        grid->addWidget(fields[row].widget, row, 1);
    }

    layout->addWidget(coreGroup);
    appendExtensionFieldsSection(
        layout,
        &m_basicExtraTable,
        QStringLiteral("红色 * 为必填项（新增钻孔时须全部填写；已存在钻孔合并时可留空保留旧值）；"
                       "选填项留空表示不提交；点击 × 可清空为未填写；"
                       "输入 0 视为有效值，失焦后格式化为两位小数；"
                       "勘探区编号在测斜/地层/样品保存时自动带入；扩展字段打包为 JSON 存入 EXTRA_DATA。"));
    layout->addStretch();
    return page;
}

QWidget* SingleEntryDialog::createInclinePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_inclineBoreholeLabel = new QLabel(page);
    m_inclineBoreholeLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_inclineBoreholeLabel);

    auto* btnLayout = new QHBoxLayout();
    auto* btnAddRow = new QPushButton(QStringLiteral("+ 新增行"), page);
    auto* btnDeleteRow = new QPushButton(QStringLiteral("🗑 删除选中行"), page);
    connect(btnAddRow, &QPushButton::clicked, this, &SingleEntryDialog::onAddInclineRow);
    connect(btnDeleteRow, &QPushButton::clicked, this, &SingleEntryDialog::onDeleteSelectedRow);
    btnLayout->addWidget(btnAddRow);
    btnLayout->addWidget(btnDeleteRow);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    static const QList<QPair<QString, bool>> inclineColumns = {
        { QStringLiteral("测点号"), true },
        { QStringLiteral("测点深度"), true },
        { QStringLiteral("偏斜角采用值"), false },
        { QStringLiteral("方位角采用值"), false },
    };
    layout->addWidget(createTableHeaderBar(page, inclineColumns));

    m_inclineTable = new QTableWidget(page);
    setupDataTableColumns(m_inclineTable, inclineColumns.size());
    m_inclineTable->setMaximumHeight(220);
    layout->addWidget(m_inclineTable);

    appendExtensionFieldsSection(
        layout,
        &m_inclineExtraTable,
        QStringLiteral("标红 * 为必填项；测点深度必须递增，且不超过终孔深度。"));

    return page;
}

QWidget* SingleEntryDialog::createStrataPage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_strataBoreholeLabel = new QLabel(page);
    m_strataBoreholeLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_strataBoreholeLabel);

    auto* btnLayout = new QHBoxLayout();
    auto* btnAddRow = new QPushButton(QStringLiteral("+ 新增行"), page);
    auto* btnDeleteRow = new QPushButton(QStringLiteral("🗑 删除选中行"), page);
    connect(btnAddRow, &QPushButton::clicked, this, &SingleEntryDialog::onAddStrataRow);
    connect(btnDeleteRow, &QPushButton::clicked, this, &SingleEntryDialog::onDeleteSelectedRow);
    btnLayout->addWidget(btnAddRow);
    btnLayout->addWidget(btnDeleteRow);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    static const QList<QPair<QString, bool>> strataColumns = {
        { QStringLiteral("岩层序号"), true },
        { QStringLiteral("分层号"), false },
        { QStringLiteral("岩石分层孔深（底深）"), true },
        { QStringLiteral("岩石全名"), false },
        { QStringLiteral("岩层倾角"), false },
    };
    layout->addWidget(createTableHeaderBar(page, strataColumns));

    m_strataTable = new QTableWidget(page);
    setupDataTableColumns(m_strataTable, strataColumns.size());
    m_strataTable->setMaximumHeight(220);
    layout->addWidget(m_strataTable);

    appendExtensionFieldsSection(
        layout,
        &m_strataExtraTable,
        QStringLiteral("标红 * 为必填项；岩石分层孔深（底深）必须递增，相邻层必须连续，且不超过终孔深度。"));

    return page;
}

QWidget* SingleEntryDialog::createSamplePage()
{
    auto* page = new QWidget(this);
    auto* layout = new QVBoxLayout(page);

    m_sampleBoreholeLabel = new QLabel(page);
    m_sampleBoreholeLabel->setStyleSheet(QStringLiteral("font-weight: bold;"));
    layout->addWidget(m_sampleBoreholeLabel);

    auto* btnLayout = new QHBoxLayout();
    auto* btnAddSample = new QPushButton(QStringLiteral("+ 新增样品"), page);
    auto* btnAddElement = new QPushButton(QStringLiteral("+ 为选中样品新增元素"), page);
    btnAddElement->setToolTip(QStringLiteral("在已选样品下追加一条元素名称/元素品位值记录（同一样品可有多条）"));
    auto* btnDeleteRow = new QPushButton(QStringLiteral("🗑 删除选中行"), page);
    connect(btnAddSample, &QPushButton::clicked, this, &SingleEntryDialog::onAddSampleRow);
    connect(btnAddElement, &QPushButton::clicked, this, &SingleEntryDialog::onAddElementRow);
    connect(btnDeleteRow, &QPushButton::clicked, this, &SingleEntryDialog::onDeleteSelectedRow);
    btnLayout->addWidget(btnAddSample);
    btnLayout->addWidget(btnAddElement);
    btnLayout->addWidget(btnDeleteRow);
    btnLayout->addStretch();
    layout->addLayout(btnLayout);

    static const QList<QPair<QString, bool>> sampleColumns = {
        { QStringLiteral("样品编号"), true },
        { QStringLiteral("采样起始孔深"), true },
        { QStringLiteral("采样终止孔深"), true },
        { QStringLiteral("样长"), false },
        { QStringLiteral("样品类型"), false },
        { QStringLiteral("元素名称"), true },
        { QStringLiteral("元素品位值"), true },
    };
    layout->addWidget(createTableHeaderBar(page, sampleColumns));

    m_sampleTable = new QTableWidget(page);
    setupDataTableColumns(m_sampleTable, sampleColumns.size());
    m_sampleTable->setMaximumHeight(260);
    connect(m_sampleTable, &QTableWidget::itemChanged, this, [this](QTableWidgetItem* item) {
        if (!item || item->column() != 0 || !m_sampleTable) {
            return;
        }
        if (isSampleElementRow(m_sampleTable, item->row())) {
            return;
        }
        refreshSampleElementRowLabels(m_sampleTable, item->row());
        if (m_sampleTable->currentRow() == item->row()) {
            syncSampleExtraEditorForRow(item->row());
        }
    });
    connect(m_sampleTable->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, [this](const QModelIndex& current, const QModelIndex& previous) {
                Q_UNUSED(previous);
                if (!current.isValid()) {
                    syncSampleExtraEditorForRow(-1);
                    return;
                }
                syncSampleExtraEditorForRow(current.row());
            });
    layout->addWidget(m_sampleTable);

    m_sampleExtraScopeLabel = new QLabel(
        QStringLiteral("请先选中样品行，再编辑该样品的扩展字段（写入 SampleRecord.extra_data）"),
        page);
    m_sampleExtraScopeLabel->setWordWrap(true);
    m_sampleExtraScopeLabel->setStyleSheet(QStringLiteral("color: #666666; font-size: 11px;"));
    layout->addWidget(m_sampleExtraScopeLabel);

    appendExtensionFieldsSection(
        layout,
        &m_sampleExtraTable,
        QStringLiteral(
            "带红色 <font color='red'>*</font> 的列为必填。<br>"
            "① 点「+ 新增样品」录入一个样品：若该样品只有<b>一种</b>元素，可在同一行直接填写「元素名称」和「元素品位值」。<br>"
            "② 若同一段样品有多种元素（如 Cu、Zn、S）：先选中该样品行，再点「+ 为选中样品新增元素」。"
            "下方会出现灰色子行（↳ 样品号），<b>只需填写元素名称和元素品位值</b>，孔深沿用上方样品，不必重复输入。<br>"
            "③ 扩展字段仅描述<b>样品物理信息</b>，保存到 <b>SampleRecord.extra_data</b>；"
            "元素/品位属于 <b>GradeInfo</b>，不在此区录入。各样品扩展字段独立，请选中对应样品行后填写。"),
        QStringLiteral("样品扩展字段（SampleRecord.extra_data）"));

    return page;
}

void SingleEntryDialog::switchToStep(int index)
{
    if (index >= 0 && index < 4) {
        m_stackedWidget->setCurrentIndex(index);
    }
}

void SingleEntryDialog::updateStepIndicator()
{
    static const QStringList stepNames = {
        QStringLiteral("钻孔概况"),
        QStringLiteral("测斜数据"),
        QStringLiteral("地层分层"),
        QStringLiteral("样品数据")
    };

    const int current = m_stackedWidget->currentIndex();

    for (int i = 0; i < 4; ++i) {
        const bool isCurrent = (i == current);
        QString dot;
        QString textColor;
        if (isCurrent) {
            dot = QStringLiteral("◉");
            textColor = QStringLiteral("#0066cc");
        } else if (m_stepSaved[i]) {
            dot = QStringLiteral("●");
            textColor = QStringLiteral("#22863a");
        } else {
            dot = QStringLiteral("○");
            textColor = QStringLiteral("#888888");
        }

        m_stepTabs[i]->setText(QStringLiteral("%1 %2").arg(dot, stepNames[i]));
        m_stepTabs[i]->setStyleSheet(
            QStringLiteral(
                "QPushButton { color: %1; background-color: %2; font-size: 14px; "
                "padding: 4px 8px; border: none; border-radius: 4px; } "
                "QPushButton:hover { color: #0066cc; background-color: #f0f7ff; }")
                .arg(textColor, isCurrent ? QStringLiteral("#e6f2ff") : QStringLiteral("transparent")));
    }
}

void SingleEntryDialog::updateNavigationButtons()
{
    const int current = m_stackedWidget->currentIndex();
    m_btnPrevious->setEnabled(current > 0);
    m_btnNext->setEnabled(current < 3);
}

void SingleEntryDialog::updateLinkedBoreholeLabels()
{
    const QString boreholeId = m_editBoreholeId->text().trimmed();
    QString areaId = m_editAreaId->text().trimmed();
    bool areaFromDrillHole = false;

    if (areaId.isEmpty() && !boreholeId.isEmpty()) {
        areaId = DatabaseManager::instance().fetchAreaIdForBorehole(boreholeId);
        areaFromDrillHole = !areaId.isEmpty();
    }

    QString text;
    if (boreholeId.isEmpty()) {
        text = QStringLiteral("钻孔编号：（请先在钻孔概况中填写）");
    } else {
        text = QStringLiteral("钻孔编号：%1（自动关联）").arg(boreholeId);
        if (!areaId.isEmpty()) {
            const QString areaLine = areaFromDrillHole
                ? QStringLiteral("勘探区编号：%1（来自钻孔概况）").arg(areaId)
                : QStringLiteral("勘探区编号：%1").arg(areaId);
            text = QStringLiteral("%1  |  %2").arg(areaLine, text);
        } else {
            text += QStringLiteral("\n勘探区编号：（未填写）");
        }
    }

    m_inclineBoreholeLabel->setText(text);
    m_strataBoreholeLabel->setText(text);
    m_sampleBoreholeLabel->setText(text);
}

void SingleEntryDialog::appendAreaIdArg(QStringList* args) const
{
    if (!args) {
        return;
    }

    QString areaId = m_editAreaId->text().trimmed();
    if (areaId.isEmpty()) {
        areaId = DatabaseManager::instance().fetchAreaIdForBorehole(
            m_editBoreholeId->text().trimmed());
    }

    args->append(QStringLiteral("--area_id"));
    args->append(areaId);
}

void SingleEntryDialog::appendOptionalNumericArg(QStringList* args,
                                                 const QString& flag,
                                                 const NumericInputField* field) const
{
    if (!args || flag.isEmpty()) {
        return;
    }

    if (!field || !field->isFilled()) {
        args->append(flag);
        return;
    }

    bool ok = false;
    const double value = field->toDouble(&ok);
    if (!ok) {
        args->append(flag);
        return;
    }

    args->append(flag);
    args->append(QString::number(value, 'f', 2));
}

void SingleEntryDialog::appendNumericArgIfFilled(QStringList* args,
                                                 const QString& flag,
                                                 const NumericInputField* field) const
{
    if (!args || !field || !field->isFilled()) {
        return;
    }

    bool ok = false;
    const double value = field->toDouble(&ok);
    if (!ok) {
        return;
    }

    args->append(flag);
    args->append(QString::number(value, 'f', 2));
}

QString SingleEntryDialog::basicInfoMissingReason() const
{
    QStringList missing;

    if (m_editBoreholeId->text().trimmed().isEmpty()) {
        missing.append(QStringLiteral("钻孔编号"));
    }
    if (!m_numX->isFilled()) {
        missing.append(QStringLiteral("X坐标"));
    }
    if (!m_numY->isFilled()) {
        missing.append(QStringLiteral("Y坐标"));
    }
    if (!m_numZ->isFilled()) {
        missing.append(QStringLiteral("Z坐标"));
    }
    if (!m_numTotalDepth->isFilled()) {
        missing.append(QStringLiteral("终孔深度"));
    }

    if (missing.isEmpty()) {
        return QString();
    }

    return QStringLiteral("请填写必填项：%1").arg(missing.join(QStringLiteral("、")));
}

QString SingleEntryDialog::basicInfoInvalidNumericReason() const
{
    struct FieldCheck {
        NumericInputField* field;
        QString label;
    };

    const QList<FieldCheck> checks = {
        { m_numX, QStringLiteral("X坐标") },
        { m_numY, QStringLiteral("Y坐标") },
        { m_numZ, QStringLiteral("Z坐标") },
        { m_numTotalDepth, QStringLiteral("终孔深度") },
        { m_numDipAngle, QStringLiteral("钻孔倾角") },
        { m_numAzimuth, QStringLiteral("钻孔方位角") },
    };

    QStringList invalid;
    for (const FieldCheck& check : checks) {
        if (check.field->isFilled() && !check.field->isAcceptable()) {
            invalid.append(check.label);
        }
    }

    if (invalid.isEmpty()) {
        return QString();
    }

    return QStringLiteral("以下字段数值无效：%1").arg(invalid.join(QStringLiteral("、")));
}

QString SingleEntryDialog::basicInfoAllZeroCoordsReason() const
{
    if (!m_numX->isFilled()
        || !m_numY->isFilled()
        || !m_numZ->isFilled()) {
        return QString();
    }

    bool okX = false;
    bool okY = false;
    bool okZ = false;
    const double x = m_numX->toDouble(&okX);
    const double y = m_numY->toDouble(&okY);
    const double z = m_numZ->toDouble(&okZ);
    if (!okX || !okY || !okZ) {
        return QString();
    }

    if (qFuzzyIsNull(x)
        && qFuzzyIsNull(y)
        && qFuzzyIsNull(z)) {
        return QStringLiteral("钻孔坐标不能全为 0，请检查录入数据");
    }

    return QString();
}

void SingleEntryDialog::updateSaveButtonState()
{
    const int step = m_stackedWidget->currentIndex();
    QString reason;

    if (step == 0) {
        if (m_editBoreholeId->text().trimmed().isEmpty()) {
            reason = QStringLiteral("请填写钻孔编号");
        } else {
            reason = basicInfoInvalidNumericReason();
            if (reason.isEmpty()) {
                reason = basicInfoAllZeroCoordsReason();
            }
        }
    }

    m_btnSave->setEnabled(reason.isEmpty());
    m_btnSave->setToolTip(reason.isEmpty() ? QString() : reason);
}

void SingleEntryDialog::markStepSaved(int stepIndex)
{
    if (stepIndex >= 0 && stepIndex < 4) {
        m_stepSaved[stepIndex] = true;
        updateStepIndicator();
    }
}

void SingleEntryDialog::onFormChanged()
{
    updateLinkedBoreholeLabels();
    updateSaveButtonState();
}

void SingleEntryDialog::onStepTabClicked()
{
    auto* tab = qobject_cast<QPushButton*>(sender());
    if (tab) {
        switchToStep(tab->property("stepIndex").toInt());
    }
}

void SingleEntryDialog::onStepChanged(int index)
{
    Q_UNUSED(index);
    updateStepIndicator();
    updateNavigationButtons();
    updateSaveButtonState();
    updateLinkedBoreholeLabels();
}

void SingleEntryDialog::onPreviousClicked()
{
    switchToStep(m_stackedWidget->currentIndex() - 1);
}

void SingleEntryDialog::onNextClicked()
{
    switchToStep(m_stackedWidget->currentIndex() + 1);
}

QTableWidget* SingleEntryDialog::extensionTableForStep(int stepIndex) const
{
    switch (stepIndex) {
    case 0: return m_basicExtraTable;
    case 1: return m_inclineExtraTable;
    case 2: return m_strataExtraTable;
    case 3: return m_sampleExtraTable;
    default: return nullptr;
    }
}

QJsonObject SingleEntryDialog::buildExtraDataObject(QTableWidget* table) const
{
    QJsonObject extra;
    if (!table) {
        return extra;
    }

    for (int row = 0; row < table->rowCount(); ++row) {
        const QString fieldName = tableCellText(table, row, 0);
        const QString fieldValue = tableCellText(table, row, 1);
        if (!fieldName.isEmpty() && !fieldValue.isEmpty()) {
            extra.insert(fieldName, fieldValue);
        }
    }
    return extra;
}

QString SingleEntryDialog::extraDataJsonForTable(QTableWidget* table) const
{
    const QJsonObject extra = buildExtraDataObject(table);
    if (extra.isEmpty()) {
        return QString();
    }
    return QString::fromUtf8(QJsonDocument(extra).toJson(QJsonDocument::Compact));
}

void SingleEntryDialog::onAddExtensionField()
{
    QTableWidget* table = extensionTableForStep(m_stackedWidget->currentIndex());
    if (!table) {
        return;
    }

    const int row = table->rowCount();
    table->insertRow(row);
    table->setItem(row, 0, createCenteredItem(QString()));
    table->setItem(row, 1, createCenteredItem(QString()));
    table->selectRow(row);
}

void SingleEntryDialog::onDeleteExtensionField()
{
    QTableWidget* table = extensionTableForStep(m_stackedWidget->currentIndex());
    if (!table) {
        return;
    }

    const int row = table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中要删除的扩展字段行。"));
        return;
    }
    table->removeRow(row);
}

bool SingleEntryDialog::validateBasicInfoForSave() const
{
    const QString invalidReason = basicInfoInvalidNumericReason();
    if (!invalidReason.isEmpty()) {
        QMessageBox::warning(
            const_cast<SingleEntryDialog*>(this),
            QStringLiteral("校验失败"),
            invalidReason);
        return false;
    }

    const QString missingReason = basicInfoMissingReason();
    if (!missingReason.isEmpty()) {
        QMessageBox::warning(
            const_cast<SingleEntryDialog*>(this),
            QStringLiteral("校验失败"),
            missingReason);
        return false;
    }

    const QString allZeroReason = basicInfoAllZeroCoordsReason();
    if (!allZeroReason.isEmpty()) {
        QMessageBox::warning(
            const_cast<SingleEntryDialog*>(this),
            QStringLiteral("校验失败"),
            allZeroReason);
        return false;
    }

    return true;
}

bool SingleEntryDialog::validateBasicInfoPartialForSave() const
{
    const QString invalidReason = basicInfoInvalidNumericReason();
    if (!invalidReason.isEmpty()) {
        QMessageBox::warning(
            const_cast<SingleEntryDialog*>(this),
            QStringLiteral("校验失败"),
            invalidReason);
        return false;
    }

    const QString allZeroReason = basicInfoAllZeroCoordsReason();
    if (!allZeroReason.isEmpty()) {
        QMessageBox::warning(
            const_cast<SingleEntryDialog*>(this),
            QStringLiteral("校验失败"),
            allZeroReason);
        return false;
    }

    return true;
}

QStringList SingleEntryDialog::buildBasicInfoArgs() const
{
    QStringList args;
    args << QStringLiteral("--borehole_id") << m_editBoreholeId->text().trimmed();
    appendAreaIdArg(&args);
    appendNumericArgIfFilled(&args, QStringLiteral("--x"), m_numX);
    appendNumericArgIfFilled(&args, QStringLiteral("--y"), m_numY);
    appendNumericArgIfFilled(&args, QStringLiteral("--z"), m_numZ);
    appendNumericArgIfFilled(&args, QStringLiteral("--total_depth"), m_numTotalDepth);
    appendOptionalNumericArg(&args, QStringLiteral("--azimuth"), m_numAzimuth);
    appendOptionalNumericArg(&args, QStringLiteral("--dip_angle"), m_numDipAngle);

    const QString extraJson = extraDataJsonForTable(m_basicExtraTable);
    if (!extraJson.isEmpty()) {
        args << QStringLiteral("--extra_data_json") << extraJson;
    }

    return args;
}

QString SingleEntryDialog::tableCellText(QTableWidget* table, int row, int col) const
{
    QTableWidgetItem* item = table->item(row, col);
    return item ? item->text().trimmed() : QString();
}

bool SingleEntryDialog::ensureBoreholeIdForLinkedSave() const
{
    if (m_editBoreholeId->text().trimmed().isEmpty()) {
        QMessageBox::warning(
            const_cast<SingleEntryDialog*>(this),
            QStringLiteral("校验失败"),
            QStringLiteral("请先在「钻孔概况」步骤填写钻孔编号。"));
        return false;
    }
    return true;
}

bool SingleEntryDialog::runPythonAnalyze(const QString& scriptFileName,
                                         const QStringList& args,
                                         QJsonArray* conflicts,
                                         QString* errorMessage,
                                         QJsonObject* analyzeMetadata)
{
    QJsonObject result;
    QStringList analyzeArgs = args;
    analyzeArgs << QStringLiteral("--analyze");

    if (!PythonRunner::runScript(scriptFileName, analyzeArgs, &result, errorMessage)) {
        if (errorMessage && errorMessage->isEmpty()) {
            *errorMessage = result.value(QStringLiteral("message")).toString();
        }
        return false;
    }

    if (conflicts) {
        *conflicts = result.value(QStringLiteral("conflicts")).toArray();
    }
    if (analyzeMetadata) {
        *analyzeMetadata = result;
    }
    return true;
}

QString SingleEntryDialog::writeConflictResolutionsFile(const QHash<QString, QString>& resolutions) const
{
    QDir().mkpath(AppConfig::logsDir());
    const QString filePath = AppConfig::logsDir() + QStringLiteral("/single_entry_conflicts.json");

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

bool SingleEntryDialog::runPythonSave(const QString& scriptFileName,
                                       const QStringList& args,
                                       const QString& fallbackMessage,
                                       int stepIndex)
{
    QJsonObject result;
    QString errorMessage;

    if (!PythonRunner::runScript(scriptFileName, args, &result, &errorMessage)) {
        const QString message = result.value(QStringLiteral("message")).toString(errorMessage);
        QMessageBox::warning(this, QStringLiteral("保存失败"), message);
        return false;
    }

    const QString message = result.value(QStringLiteral("message")).toString(fallbackMessage);
    QMessageBox::information(this, QStringLiteral("保存成功"), message);
    markStepSaved(stepIndex);
    // 仅钻孔概况写入 DrillHoleInfo 后刷新主界面钻孔列表
    if (stepIndex == 0) {
        emit dataSaved();
    }
    return true;
}

bool SingleEntryDialog::resolveConflictsAndSave(const QString& scriptFileName,
                                                const QStringList& baseArgs,
                                                int stepIndex,
                                                const QString& successFallback,
                                                const QJsonArray* prefetchedConflicts)
{
    QJsonArray conflicts;
    QString errorMessage;
    if (prefetchedConflicts) {
        conflicts = *prefetchedConflicts;
    } else if (!runPythonAnalyze(scriptFileName, baseArgs, &conflicts, &errorMessage)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), errorMessage);
        return false;
    }

    QHash<QString, QString> resolutions;
    QString batchAction;

    for (const QJsonValue& value : conflicts) {
        const QJsonObject conflict = value.toObject();
        const QString recordKey = conflict.value(QStringLiteral("record_key")).toString();

        if (!batchAction.isEmpty()) {
            resolutions.insert(recordKey, batchAction);
            continue;
        }

        ImportConflictDialog conflictDialog(conflict, this);
        if (conflictDialog.exec() != QDialog::Accepted) {
            return false;
        }

        const QString action = ImportConflictDialog::actionToString(conflictDialog.selectedAction());
        resolutions.insert(recordKey, action);

        if (conflictDialog.applyToAllConflicts()) {
            batchAction = action;
        }
    }

    QStringList saveArgs = baseArgs;
    if (!resolutions.isEmpty()) {
        const QString resolutionPath = writeConflictResolutionsFile(resolutions);
        if (resolutionPath.isEmpty()) {
            QMessageBox::warning(this, QStringLiteral("保存失败"), QStringLiteral("无法写入冲突处理策略文件。"));
            return false;
        }
        saveArgs << QStringLiteral("--conflict_resolutions") << resolutionPath;
    }

    return runPythonSave(scriptFileName, saveArgs, successFallback, stepIndex);
}

QJsonArray SingleEntryDialog::buildInclineJson() const
{
    QJsonArray rows;
    for (int row = 0; row < m_inclineTable->rowCount(); ++row) {
        if (!tableRowHasContent(m_inclineTable, row)) {
            continue;
        }
        QJsonObject item;
        item.insert(QStringLiteral("point_id"), tableCellText(m_inclineTable, row, 0).toInt());
        item.insert(QStringLiteral("point_depth"), tableCellText(m_inclineTable, row, 1).toDouble());
        item.insert(QStringLiteral("deviation_angle"), tableCellText(m_inclineTable, row, 2));
        item.insert(QStringLiteral("azimuth"), tableCellText(m_inclineTable, row, 3));
        rows.append(item);
    }
    return rows;
}

QJsonArray SingleEntryDialog::buildStrataJson() const
{
    QJsonArray rows;
    for (int row = 0; row < m_strataTable->rowCount(); ++row) {
        if (!tableRowHasContent(m_strataTable, row)) {
            continue;
        }
        QJsonObject item;
        item.insert(QStringLiteral("layer_order"), tableCellText(m_strataTable, row, 0).toInt());
        item.insert(QStringLiteral("layer_no"), tableCellText(m_strataTable, row, 1));
        item.insert(QStringLiteral("bottom_depth"), tableCellText(m_strataTable, row, 2).toDouble());
        item.insert(QStringLiteral("rock_name"), tableCellText(m_strataTable, row, 3));
        item.insert(QStringLiteral("dip_angle"), tableCellText(m_strataTable, row, 4));
        rows.append(item);
    }
    return rows;
}

QJsonObject SingleEntryDialog::buildSamplePayload() const
{
    const_cast<SingleEntryDialog*>(this)->stashActiveSampleExtraFields();

    QJsonArray samples;
    QJsonArray grades;

    for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
        if (!tableRowHasContent(m_sampleTable, row)) {
            continue;
        }

        const QString sampleId = tableCellText(m_sampleTable, row, 0);
        if (!sampleId.isEmpty() && !isSampleElementRow(m_sampleTable, row)) {
            QJsonObject sample;
            sample.insert(QStringLiteral("sample_id"), sampleId);
            sample.insert(QStringLiteral("start_depth"), tableCellText(m_sampleTable, row, 1));
            sample.insert(QStringLiteral("end_depth"), tableCellText(m_sampleTable, row, 2));
            sample.insert(QStringLiteral("sample_length"), tableCellText(m_sampleTable, row, 3));
            sample.insert(QStringLiteral("sample_type"), tableCellText(m_sampleTable, row, 4));
            const QJsonObject extra = m_sampleExtraById.value(sampleId);
            if (!extra.isEmpty()) {
                sample.insert(QStringLiteral("extra_data"), extra);
            }
            samples.append(sample);
        }

        const QString element = tableCellText(m_sampleTable, row, 5);
        const QString grade = tableCellText(m_sampleTable, row, 6);
        if (!element.isEmpty()) {
            const int headRow = findSampleHeadRow(row);
            const QString linkedSampleId = tableCellText(m_sampleTable, headRow, 0);
            if (!linkedSampleId.isEmpty()) {
                QJsonObject gradeItem;
                gradeItem.insert(QStringLiteral("sample_id"), linkedSampleId);
                gradeItem.insert(QStringLiteral("element_name"), element);
                gradeItem.insert(QStringLiteral("grade_value"), grade.toDouble());
                grades.append(gradeItem);
            }
        }
    }

    QJsonObject payload;
    payload.insert(QStringLiteral("samples"), samples);
    payload.insert(QStringLiteral("grades"), grades);
    return payload;
}

bool SingleEntryDialog::hasInclineData() const
{
    for (int row = 0; row < m_inclineTable->rowCount(); ++row) {
        if (tableRowHasContent(m_inclineTable, row)) {
            return true;
        }
    }
    return false;
}

bool SingleEntryDialog::hasStrataData() const
{
    for (int row = 0; row < m_strataTable->rowCount(); ++row) {
        if (tableRowHasContent(m_strataTable, row)) {
            return true;
        }
    }
    return false;
}

bool SingleEntryDialog::hasSampleData() const
{
    for (int row = 0; row < m_sampleTable->rowCount(); ++row) {
        if (tableRowHasContent(m_sampleTable, row)) {
            return true;
        }
    }
    return false;
}

void SingleEntryDialog::saveBasicInfo()
{
    if (m_editBoreholeId->text().trimmed().isEmpty()) {
        QMessageBox::warning(
            this,
            QStringLiteral("校验失败"),
            QStringLiteral("请填写钻孔编号"));
        return;
    }

    const QStringList args = buildBasicInfoArgs();

    QJsonObject analyzeMetadata;
    QJsonArray conflicts;
    QString errorMessage;
    if (!runPythonAnalyze(
            QStringLiteral("save_drill_hole.py"),
            args,
            &conflicts,
            &errorMessage,
            &analyzeMetadata)) {
        QMessageBox::warning(this, QStringLiteral("保存失败"), errorMessage);
        return;
    }

    const bool recordExists = analyzeMetadata.value(QStringLiteral("record_exists")).toBool(false);
    if (!recordExists) {
        if (!validateBasicInfoForSave()) {
            return;
        }
    } else if (!validateBasicInfoPartialForSave()) {
        return;
    }

    resolveConflictsAndSave(
        QStringLiteral("save_drill_hole.py"),
        args,
        0,
        QStringLiteral("钻孔概况保存成功"),
        &conflicts);
}

void SingleEntryDialog::saveInclineData()
{
    if (!hasInclineData()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先保存钻孔概况"));
        return;
    }
    if (!ensureBoreholeIdForLinkedSave()) {
        return;
    }

    const QJsonArray rows = buildInclineJson();
    QStringList args;
    args << QStringLiteral("--borehole_id") << m_editBoreholeId->text().trimmed();
    appendAreaIdArg(&args);
    args << QStringLiteral("--data_json")
         << QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact));

    const QString extraJson = extraDataJsonForTable(m_inclineExtraTable);
    if (!extraJson.isEmpty()) {
        args << QStringLiteral("--extra_data_json") << extraJson;
    }

    resolveConflictsAndSave(
        QStringLiteral("save_incline.py"),
        args,
        1,
        QStringLiteral("测斜数据保存成功"));
}

void SingleEntryDialog::saveStrataData()
{
    if (!hasStrataData()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先保存钻孔概况"));
        return;
    }
    if (!ensureBoreholeIdForLinkedSave()) {
        return;
    }

    const QJsonArray rows = buildStrataJson();
    QStringList args;
    args << QStringLiteral("--borehole_id") << m_editBoreholeId->text().trimmed();
    appendAreaIdArg(&args);
    args << QStringLiteral("--data_json")
         << QString::fromUtf8(QJsonDocument(rows).toJson(QJsonDocument::Compact));

    const QString extraJson = extraDataJsonForTable(m_strataExtraTable);
    if (!extraJson.isEmpty()) {
        args << QStringLiteral("--extra_data_json") << extraJson;
    }

    resolveConflictsAndSave(
        QStringLiteral("save_strata.py"),
        args,
        2,
        QStringLiteral("地层分层保存成功"));
}

void SingleEntryDialog::saveSampleData()
{
    if (!hasSampleData()) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先保存钻孔概况"));
        return;
    }
    if (!ensureBoreholeIdForLinkedSave()) {
        return;
    }

    const QJsonObject payload = buildSamplePayload();
    QStringList args;
    args << QStringLiteral("--borehole_id") << m_editBoreholeId->text().trimmed();
    appendAreaIdArg(&args);
    args << QStringLiteral("--data_json")
         << QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact));

    resolveConflictsAndSave(
        QStringLiteral("save_sample.py"),
        args,
        3,
        QStringLiteral("样品数据保存成功"));
}

void SingleEntryDialog::onSaveClicked()
{
    switch (m_stackedWidget->currentIndex()) {
    case 0: saveBasicInfo(); break;
    case 1: saveInclineData(); break;
    case 2: saveStrataData(); break;
    case 3: saveSampleData(); break;
    default: break;
    }
}

void SingleEntryDialog::onCancelClicked()
{
    reject();
}

void SingleEntryDialog::onAddInclineRow()
{
    const int row = m_inclineTable->rowCount();
    m_inclineTable->insertRow(row);
    m_inclineTable->setItem(row, 0, createCenteredItem(QString::number(row + 1)));
    m_inclineTable->setItem(row, 1, createCenteredItem(QStringLiteral("0.00")));
    m_inclineTable->setItem(row, 2, createCenteredItem(QString()));
    m_inclineTable->setItem(row, 3, createCenteredItem(QString()));
}

void SingleEntryDialog::onAddStrataRow()
{
    const int row = m_strataTable->rowCount();
    m_strataTable->insertRow(row);
    m_strataTable->setItem(row, 0, createCenteredItem(QString::number(row + 1)));
    m_strataTable->setItem(row, 1, createCenteredItem(QString()));
    m_strataTable->setItem(row, 2, createCenteredItem(QStringLiteral("0.00")));
    m_strataTable->setItem(row, 3, createCenteredItem(QString()));
    m_strataTable->setItem(row, 4, createCenteredItem(QString()));
}

void SingleEntryDialog::onAddSampleRow()
{
    const int row = m_sampleTable->rowCount();
    m_sampleTable->insertRow(row);
    configureSampleHeadRowItems(m_sampleTable, row);
    m_sampleTable->selectRow(row);
    m_sampleTable->setCurrentCell(row, 0);
}

void SingleEntryDialog::onDeleteSelectedRow()
{
    QTableWidget* table = nullptr;
    switch (m_stackedWidget->currentIndex()) {
    case 1: table = m_inclineTable; break;
    case 2: table = m_strataTable; break;
    case 3: table = m_sampleTable; break;
    default: break;
    }

    if (!table) {
        return;
    }

    const int row = table->currentRow();
    if (row < 0) {
        QMessageBox::information(this, QStringLiteral("提示"), QStringLiteral("请先选中要删除的行。"));
        return;
    }

    if (!tableRowHasContent(table, row)) {
        table->removeRow(row);
        return;
    }

    if (QMessageBox::question(
            this,
            QStringLiteral("确认删除"),
            QStringLiteral("确定删除选中的临时记录吗？")) != QMessageBox::Yes) {
        return;
    }

    if (table == m_sampleTable) {
        const int headRow = findSampleHeadRow(row);
        if (headRow >= 0) {
            const QString sampleId = tableCellText(m_sampleTable, headRow, 0);
            if (sampleId == m_activeSampleExtraId) {
                m_activeSampleExtraId.clear();
            }
            m_sampleExtraById.remove(sampleId);
        }
    }

    table->removeRow(row);

    if (table == m_sampleTable) {
        syncSampleExtraEditorForRow(m_sampleTable->currentRow());
    }
}

int SingleEntryDialog::findSampleHeadRow(int row) const
{
    if (row < 0 || row >= m_sampleTable->rowCount()) {
        return -1;
    }

    int headRow = row;
    while (headRow >= 0) {
        if (!isSampleElementRow(m_sampleTable, headRow)) {
            QTableWidgetItem* item = m_sampleTable->item(headRow, 0);
            if (item && !item->text().trimmed().isEmpty()) {
                return headRow;
            }
        }
        --headRow;
    }

    return -1;
}

void SingleEntryDialog::loadExtraFieldsToTable(QTableWidget* table, const QJsonObject& extra) const
{
    if (!table) {
        return;
    }

    table->setRowCount(0);
    for (auto it = extra.begin(); it != extra.end(); ++it) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, createCenteredItem(it.key()));
        table->setItem(row, 1, createCenteredItem(it.value().toVariant().toString()));
    }
}

void SingleEntryDialog::stashActiveSampleExtraFields()
{
    if (!m_sampleExtraTable || m_activeSampleExtraId.isEmpty()) {
        return;
    }

    const QJsonObject extra = buildExtraDataObject(m_sampleExtraTable);
    if (extra.isEmpty()) {
        m_sampleExtraById.remove(m_activeSampleExtraId);
    } else {
        m_sampleExtraById.insert(m_activeSampleExtraId, extra);
    }
}

void SingleEntryDialog::syncSampleExtraEditorForRow(int row)
{
    stashActiveSampleExtraFields();

    if (!m_sampleExtraTable || !m_sampleExtraScopeLabel) {
        return;
    }

    const int headRow = findSampleHeadRow(row);
    if (headRow < 0) {
        m_activeSampleExtraId.clear();
        loadExtraFieldsToTable(m_sampleExtraTable, QJsonObject());
        m_sampleExtraScopeLabel->setText(
            QStringLiteral("请先选中样品行，再编辑该样品的扩展字段（写入 SampleRecord.extra_data）"));
        return;
    }

    const QString sampleId = tableCellText(m_sampleTable, headRow, 0);
    m_activeSampleExtraId = sampleId;
    loadExtraFieldsToTable(m_sampleExtraTable, m_sampleExtraById.value(sampleId));
    m_sampleExtraScopeLabel->setText(
        QStringLiteral("当前样品 %1 → SampleRecord.extra_data（元素/品位属于 GradeInfo，不在此区录入）")
            .arg(sampleId));
}

int SingleEntryDialog::findSampleGroupEndRow(int headRow) const
{
    if (headRow < 0 || headRow >= m_sampleTable->rowCount()) {
        return headRow;
    }

    int endRow = headRow;
    for (int row = headRow + 1; row < m_sampleTable->rowCount(); ++row) {
        if (isSampleElementRow(m_sampleTable, row)) {
            endRow = row;
            continue;
        }

        QTableWidgetItem* item = m_sampleTable->item(row, 0);
        if (item && !item->text().trimmed().isEmpty()) {
            break;
        }
        endRow = row;
    }
    return endRow;
}

void SingleEntryDialog::onAddElementRow()
{
    const int currentRow = m_sampleTable->currentRow();
    if (currentRow < 0) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请先选中一个样品行（或该样品下的元素行），再新增元素。"));
        return;
    }

    const int headRow = findSampleHeadRow(currentRow);
    if (headRow < 0) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请先填写样品编号，再为该样品新增元素。"));
        return;
    }

    QTableWidgetItem* sampleIdItem = m_sampleTable->item(headRow, 0);
    const QString sampleId = sampleIdItem ? sampleIdItem->text().trimmed() : QString();
    if (sampleId.isEmpty()) {
        QMessageBox::information(
            this,
            QStringLiteral("提示"),
            QStringLiteral("请先填写样品编号，再为该样品新增元素。"));
        return;
    }

    const int insertRow = findSampleGroupEndRow(headRow) + 1;
    m_sampleTable->insertRow(insertRow);
    configureSampleElementRowItems(m_sampleTable, insertRow, sampleId);
    m_sampleTable->selectRow(insertRow);
    m_sampleTable->setCurrentCell(insertRow, 5);
}
