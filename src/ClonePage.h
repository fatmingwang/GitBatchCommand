#pragma once
#include <QWidget>
#include <QVector>
#include <QPair>
#include <QStringList>

class QListWidget;
class QLineEdit;
class QPushButton;
class QCheckBox;
class QProgressBar;
class QThread;
class CloneWorker;
class LogPanel;

class ClonePage : public QWidget {
    Q_OBJECT
public:
    explicit ClonePage(LogPanel *logPanel, QWidget *parent = nullptr);
    ~ClonePage() override;

private slots:
    void onLoadJson();
    void onAddUrl();
    void onRemoveSelected();
    void onClearAll();
    void onBrowseDest();
    void onBatchClone();

    void onItemDone(QString url, bool success, QString message);
    void onProgress(int done, int total);
    void onFinished();

private:
    void setBusy(bool busy);
    QStringList collectUrls() const;

    LogPanel *m_logPanel;

    QListWidget *m_listWidget;
    QLineEdit *m_urlEdit;
    QLineEdit *m_destEdit;
    QCheckBox *m_submodulesCheck;
    QProgressBar *m_progressBar;

    QPushButton *m_loadJsonBtn;
    QPushButton *m_addBtn;
    QPushButton *m_removeBtn;
    QPushButton *m_clearBtn;
    QPushButton *m_browseBtn;
    QPushButton *m_cloneBtn;

    QThread *m_thread;
    CloneWorker *m_worker;

    QVector<QPair<QString, QString>> m_failures;
    int m_successCount = 0;
    int m_totalCount = 0;
};
