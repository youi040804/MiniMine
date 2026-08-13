#ifndef MINIMINEUI_H
#define MINIMINEUI_H

#include <QMainWindow>
#include <QTableWidget>
#include <QPushButton>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QHeaderView>
#include <QLabel>
#include <QJsonArray>
#include <QJsonObject>

class MiniMineUI : public QMainWindow
{
    Q_OBJECT

public:
    MiniMineUI(QWidget* parent = nullptr);
    ~MiniMineUI();

public slots:
    void reloadDrillHoleTable();

private slots:
    void onSingleEntryClicked();
    void onBatchImportClicked();
    void onExportDataClicked();
    void onRefresh3DClicked();
    void onTableRowDoubleClicked(int row, int column);

private:
    void setupUI();
    void loadSampleData();
    void showTableMessage(const QString& message);
    void updateStatusBar();
    void showImportResult(int totalCount,
                          int insertedCount,
                          int overwrittenCount,
                          int mergedCount,
                          int skippedCount,
                          int failedCount,
                          const QString& targetTable,
                          const QJsonArray& errorRecords,
                          const QJsonObject& sampleRenameInfo,
                          const QString& logPath);

    QTableWidget* m_tableWidget;
    QPushButton* m_btnSingleEntry;
    QPushButton* m_btnBatchImport;
    QPushButton* m_btnExportData;
    QPushButton* m_btnRefresh3D;
    QLabel* m_statusLabel;
    bool m_isPlaceholderTable = false;
};

#endif // MINIMINEUI_H
