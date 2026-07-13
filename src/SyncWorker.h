#pragma once
#include <QObject>
#include <QStringList>
#include <functional>

#include "GitProcess.h"

class SyncWorker : public QObject {
    Q_OBJECT
public:
    explicit SyncWorker(QObject *parent = nullptr);

public slots:
    void scan(QString rootDir);
    void revertClean(QStringList repoPaths);
    void pullAll(QStringList repoPaths);
    void switchBranch(QStringList repoPaths, QString branchName);
    void revertCleanSubmodules(QStringList repoPaths);
    void switchSubmodulesAndPull(QStringList repoPaths, QString branchName);

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
    void revertCleanSubmodulesFinished();
    void switchSubmodulesFinished();

private:
    void scanRecursive(const QString &path, QStringList &out);
    GitCommandResult forceUpdateSubmodules(const QString &path, const std::function<void(const QString &)> &onLine);
};
