#ifndef IMPORTCONFLICTDIALOG_H
#define IMPORTCONFLICTDIALOG_H

#include <QDialog>
#include <QJsonObject>

class QTableWidget;
class QRadioButton;
class QCheckBox;
class QPushButton;

class ImportConflictDialog : public QDialog
{
    Q_OBJECT

public:
    enum class Action {
        Skip,
        Overwrite,
        Merge
    };

    explicit ImportConflictDialog(const QJsonObject& conflict, QWidget* parent = nullptr);

    Action selectedAction() const;
    bool applyToAllConflicts() const;

    static QString actionToString(Action action);

private:
    void setupUI(const QJsonObject& conflict);

    QRadioButton* m_skipRadio;
    QRadioButton* m_overwriteRadio;
    QRadioButton* m_mergeRadio;
    QCheckBox* m_applyAllCheckBox;
    QPushButton* m_btnConfirm;
};

#endif // IMPORTCONFLICTDIALOG_H
