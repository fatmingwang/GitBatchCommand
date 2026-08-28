#pragma once
#include <QWidget>
#include <QVector>
#include <QStringList>

class QTableWidget;
class QTableWidgetItem;
class QListWidget;
class QLineEdit;
class QCheckBox;
class QPushButton;
class QProgressBar;
class QThread;
class CommitWorker;
class LogPanel;

struct CommitRepoRow {
    QString path;
    QString status;
    QString message;
};

class CommitPage : public QWidget {
    Q_OBJECT
public:
    explicit CommitPage(LogPanel *logPanel, QWidget *parent = nullptr);
    ~CommitPage() override;

private slots:
    void onBrowseRoot();
    void onScan();
    void onSelectAll();
    void onSelectNone();
    void onCheckSelectedFromList();
    void onCommit();
    void onPush();
    void onSwitchBranch();
    void onMergeBranch();
    void onInitSwitchPull();
    void onRemoveSubmodule();
    void onAddSubmodule();

    void onScanFinished(QStringList repoPaths);
    void onRepoBranchInfo(QString path, QString branchInfo);
    void onProgress(int done, int total);
    void onRepoResult(QString path, QString status, QString message);
    void onCommitFinished();
    void onPushFinished();
    void onSwitchFinished();
    void onMergeFinished();
    void onInitSwitchPullFinished();
    void onRemoveSubmoduleFinished();
    void onAddSubmoduleFinished();
    void onTableItemChanged(QTableWidgetItem *item);

private:
    void setBusy(bool busy);
    void populateTable(const QStringList &paths);
    int rowForPath(const QString &path) const;
    void beginOperation(const QStringList &selectedPaths);
    void showProblemsSummary(const QString &title, const QString &okMessage);
    void updateActionButtons();
    QStringList selectedPaths() const;
    QStringList selectedSubmodulePaths() const;
    QStringList selectedMainRepoPaths() const;
    void setRowChecked(int row, bool checked);

    LogPanel *m_logPanel;

    QLineEdit *m_rootEdit;
    QPushButton *m_browseBtn;
    QPushButton *m_scanBtn;
    QTableWidget *m_table;
    QListWidget *m_pickList;
    QPushButton *m_checkSelectedBtn;
    QPushButton *m_selectAllBtn;
    QPushButton *m_selectNoneBtn;

    QLineEdit *m_commitMessageEdit;
    QCheckBox *m_stageAllCheck;
    QPushButton *m_commitBtn;
    QPushButton *m_pushBtn;

    QLineEdit *m_switchBranchEdit;
    QPushButton *m_switchBtn;

    QLineEdit *m_mergeBranchEdit;
    QPushButton *m_mergeBtn;

    QPushButton *m_initSwitchPullBtn;

    QPushButton *m_removeSubmoduleBtn;

    QLineEdit *m_addSubmoduleUrlEdit;
    QLineEdit *m_addSubmodulePathEdit;
    QPushButton *m_addSubmoduleBtn;

    QProgressBar *m_progressBar;

    QThread *m_thread;
    CommitWorker *m_worker;

    bool m_busy = false;
    QStringList m_repoPaths;
    QVector<CommitRepoRow> m_results;
};
