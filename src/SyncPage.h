#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>

class QTableWidget;
class QTableWidgetItem;
class QLineEdit;
class QPushButton;
class QProgressBar;
class QThread;
class SyncWorker;
class LogPanel;

struct SyncRepoRow {
    QString path;
    QString status;
    QString message;
};

class SyncPage : public QWidget {
    Q_OBJECT
public:
    explicit SyncPage(LogPanel *logPanel, QWidget *parent = nullptr);
    ~SyncPage() override;

private slots:
    void onBrowseRoot();
    void onScan();
    void onSelectAll();
    void onSelectNone();
    void onRevertClean();
    void onPull();
    void onSwitchBranch();
    void onRevertCleanSubmodules();
    void onSwitchSubmodulesBranch();

    void onScanFinished(QStringList repoPaths);
    void onProgress(int done, int total);
    void onRepoResult(QString path, QString status, QString message);
    void onRevertCleanFinished();
    void onPullFinished();
    void onSwitchFinished();
    void onRevertCleanSubmodulesFinished();
    void onSwitchSubmodulesFinished();
    void onTableItemChanged(QTableWidgetItem *item);

private:
    void setBusy(bool busy);
    void populateTable(const QStringList &paths);
    int rowForPath(const QString &path) const;
    void beginOperation(const QStringList &selectedPaths);
    void showProblemsSummary(const QString &title, const QString &okMessage);
    void updateActionButtons();
    QStringList selectedPaths() const;
    void setRowChecked(int row, bool checked);

    LogPanel *m_logPanel;

    QLineEdit *m_rootEdit;
    QPushButton *m_browseBtn;
    QPushButton *m_scanBtn;
    QTableWidget *m_table;
    QPushButton *m_selectAllBtn;
    QPushButton *m_selectNoneBtn;
    QPushButton *m_revertCleanBtn;
    QPushButton *m_pullBtn;
    QLineEdit *m_branchEdit;
    QPushButton *m_switchBtn;
    QPushButton *m_revertCleanSubBtn;
    QPushButton *m_switchSubBtn;
    QProgressBar *m_progressBar;

    QThread *m_thread;
    SyncWorker *m_worker;

    bool m_busy = false;
    QStringList m_repoPaths;
    QVector<SyncRepoRow> m_results;
};
