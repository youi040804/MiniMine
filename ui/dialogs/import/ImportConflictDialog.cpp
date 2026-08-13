#include "ImportConflictDialog.h"
#include "FieldMappingHelper.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QHeaderView>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QRadioButton>
#include <QCheckBox>
#include <QPushButton>
#include <QButtonGroup>
#include <QJsonArray>
#include <QColor>

ImportConflictDialog::ImportConflictDialog(const QJsonObject& conflict, QWidget* parent)
    : QDialog(parent)
{
    setWindowFlags(windowFlags() | Qt::WindowMinimizeButtonHint);
    setWindowTitle(QStringLiteral("⚠️ 检测到冲突"));
    setModal(true);
    resize(820, 520);
    setupUI(conflict);
}

void ImportConflictDialog::setupUI(const QJsonObject& conflict)
{
    const QString conflictDescription = conflict.value(QStringLiteral("conflict_description")).toString(
        conflict.value(QStringLiteral("record_label")).toString());

    auto* mainLayout = new QVBoxLayout(this);

    auto* titleLabel = new QLabel(
        QStringLiteral("%1\n以下字段新旧数据不一致，请选择处理方式：").arg(conflictDescription),
        this);
    titleLabel->setWordWrap(true);
    mainLayout->addWidget(titleLabel);

    auto* diffTable = new QTableWidget(this);
    diffTable->setColumnCount(5);
    diffTable->setHorizontalHeaderLabels({
        QStringLiteral("字段"),
        QStringLiteral("数据库当前值"),
        QStringLiteral("新提交的值"),
        QStringLiteral("覆盖影响"),
        QStringLiteral("合并影响")
    });
    diffTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    diffTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    diffTable->setSelectionMode(QAbstractItemView::NoSelection);
    diffTable->verticalHeader()->setVisible(false);

    const QJsonArray differences = conflict.value(QStringLiteral("differences")).toArray();
    diffTable->setRowCount(differences.size());
    for (int i = 0; i < differences.size(); ++i) {
        const QJsonObject diff = differences.at(i).toObject();
        const QString fieldName = diff.value(QStringLiteral("field_name")).toString();
        const QString fieldLabel = diff.value(QStringLiteral("field_label")).toString(
            FieldMappingHelper::fieldDisplayName(fieldName));

        diffTable->setItem(i, 0, new QTableWidgetItem(fieldLabel));
        diffTable->setItem(i, 1, new QTableWidgetItem(diff.value(QStringLiteral("existing_value")).toString()));
        diffTable->setItem(i, 2, new QTableWidgetItem(diff.value(QStringLiteral("new_value")).toString()));

        auto* overwriteItem = new QTableWidgetItem(
            diff.value(QStringLiteral("overwrite_effect")).toString());
        auto* mergeItem = new QTableWidgetItem(
            diff.value(QStringLiteral("merge_effect")).toString());
        overwriteItem->setForeground(QColor(0xcc0000));
        mergeItem->setForeground(QColor(0x0066cc));
        diffTable->setItem(i, 3, overwriteItem);
        diffTable->setItem(i, 4, mergeItem);
    }
    mainLayout->addWidget(diffTable);

    auto* scopeHint = new QLabel(
        QStringLiteral("提示：主键不存在时自动新增，不会弹出此对话框。"
                       "以下三种选择仅作用于当前目标表，请对照上表「覆盖影响 / 合并影响」列后决定。"),
        this);
    scopeHint->setWordWrap(true);
    scopeHint->setStyleSheet(QStringLiteral("color: #666666;"));
    mainLayout->addWidget(scopeHint);

    auto* actionGroup = new QGroupBox(QStringLiteral("请选择处理方式："), this);
    auto* actionLayout = new QGridLayout(actionGroup);

    m_skipRadio = new QRadioButton(QStringLiteral("跳过"), actionGroup);
    m_overwriteRadio = new QRadioButton(QStringLiteral("覆盖"), actionGroup);
    m_mergeRadio = new QRadioButton(QStringLiteral("合并"), actionGroup);
    m_mergeRadio->setChecked(true);

    const QString skipHint = QStringLiteral(
        "放弃本次操作，保留数据库现有数据\n");
    const QString overwriteHint = QStringLiteral(
        "新数据中有值的字段覆盖旧数据；标红必填字段留空时将保留数据库旧值，不会被清空"
        "（仅非必填的可选字段留空才会在覆盖时写入 NULL；扩展字段 EXTRA_DATA 整段替换）");
    const QString mergeHint = QStringLiteral(
        "新数据中有值的字段覆盖旧数据；为空或缺失的字段保留旧值"
        "（扩展字段 EXTRA_DATA 按键合并：新 JSON 有的键覆盖，没有的键保留）。\n"
        "在合并模式下，留空的字段（包括必填项）将保留数据库中的旧值。");

    m_skipRadio->setToolTip(skipHint);
    m_overwriteRadio->setToolTip(overwriteHint);
    m_mergeRadio->setToolTip(mergeHint);

    auto* buttonGroup = new QButtonGroup(actionGroup);
    buttonGroup->addButton(m_skipRadio);
    buttonGroup->addButton(m_overwriteRadio);
    buttonGroup->addButton(m_mergeRadio);

    actionLayout->addWidget(m_skipRadio, 0, 0);
    auto* skipLabel = new QLabel(skipHint, actionGroup);
    skipLabel->setWordWrap(true);
    actionLayout->addWidget(skipLabel, 0, 1);

    actionLayout->addWidget(m_overwriteRadio, 1, 0);
    auto* overwriteLabel = new QLabel(overwriteHint, actionGroup);
    overwriteLabel->setWordWrap(true);
    actionLayout->addWidget(overwriteLabel, 1, 1);

    actionLayout->addWidget(m_mergeRadio, 2, 0);
    auto* mergeLabel = new QLabel(mergeHint, actionGroup);
    mergeLabel->setWordWrap(true);
    actionLayout->addWidget(mergeLabel, 2, 1);
    mainLayout->addWidget(actionGroup);

    m_applyAllCheckBox = new QCheckBox(
        QStringLiteral("本次所有冲突均按此处理（不再重复询问）"),
        this);
    mainLayout->addWidget(m_applyAllCheckBox);

    auto* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    m_btnConfirm = new QPushButton(QStringLiteral("确认"), this);
    m_btnConfirm->setDefault(true);
    buttonLayout->addWidget(m_btnConfirm);
    buttonLayout->addStretch();
    mainLayout->addLayout(buttonLayout);

    connect(m_btnConfirm, &QPushButton::clicked, this, &QDialog::accept);
}

ImportConflictDialog::Action ImportConflictDialog::selectedAction() const
{
    if (m_skipRadio->isChecked()) {
        return Action::Skip;
    }
    if (m_overwriteRadio->isChecked()) {
        return Action::Overwrite;
    }
    return Action::Merge;
}

bool ImportConflictDialog::applyToAllConflicts() const
{
    return m_applyAllCheckBox->isChecked();
}

QString ImportConflictDialog::actionToString(Action action)
{
    switch (action) {
    case Action::Skip:
        return QStringLiteral("skip");
    case Action::Overwrite:
        return QStringLiteral("overwrite");
    case Action::Merge:
        return QStringLiteral("merge");
    }
    return QStringLiteral("skip");
}
