#pragma once
#include <QObject>
#include <QStringList>

class SyncWorker : public QObject {
    Q_OBJECT
public:
    explicit SyncWorker(QObject *parent = nullptr);

public slots:
    void scan(QString rootDir);
    void revertClean(QStringList repoPaths);
    void pullAll(QStringList repoPaths);
    void switchBranch(QStringList repoPaths, QString branchName);

signals:
    void logLine(QString line);
    void logInfo(QString line);
    void logWarning(QString line);
    void logError(QString line);
    void scanFinished(QStringList repoPaths);
    void progress(int done, int total);
    void repoResult(QString path, QString status, QString message);
    void revertCleanFinished();
    void pullFinished();
    void switchFinished();

private:
    void scanRecursive(const QString &path, QStringList &out);
};
