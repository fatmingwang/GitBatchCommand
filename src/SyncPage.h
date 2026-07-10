#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>

class QTableWidget;
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
    void onRevertClean();
    void onPull();
    void onSwitchBranch();

    void onScanFinished(QStringList repoPaths);
    void onProgress(int done, int total);
    void onRepoResult(QString path, QString status, QString message);
    void onRevertCleanFinished();
    void onPullFinished();
    void onSwitchFinished();

private:
    void setBusy(bool busy);
    void populateTable(const QStringList &paths);
    int rowForPath(const QString &path) const;
    void beginOperation(int repoCount);
    void showProblemsSummary(const QString &title, const QString &okMessage);

    LogPanel *m_logPanel;

    QLineEdit *m_rootEdit;
    QPushButton *m_browseBtn;
    QPushButton *m_scanBtn;
    QTableWidget *m_table;
    QPushButton *m_revertCleanBtn;
    QPushButton *m_pullBtn;
    QLineEdit *m_branchEdit;
    QPushButton *m_switchBtn;
    QProgressBar *m_progressBar;

    QThread *m_thread;
    SyncWorker *m_worker;

    QStringList m_repoPaths;
    QVector<SyncRepoRow> m_results;
};
