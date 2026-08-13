#ifndef EXPORTDATADIALOG_H
#define EXPORTDATADIALOG_H

#include <QDialog>
#include <QComboBox>

class ExportDataDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ExportDataDialog(QWidget* parent = nullptr);

private slots:
    void onExportClicked();

private:
    QString currentTableName() const;
    QString currentFormat() const;
    QString suggestedFileName() const;
    QString ensureOutputExtension(const QString& filePath) const;

    QComboBox* m_tableCombo = nullptr;
    QComboBox* m_formatCombo = nullptr;
};

#endif // EXPORTDATADIALOG_H
