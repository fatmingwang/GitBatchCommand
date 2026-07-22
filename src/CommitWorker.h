#pragma once
#include <QObject>
#include <QStringList>

class CommitWorker : public QObject {
    Q_OBJECT
public:
    explicit CommitWorker(QObject *parent = nullptr);

public slots:
    void scan(QString rootDir);
    void commitAll(QStringList repoPaths, QString message, bool stageAll);
    void pushAll(QStringList repoPaths);
    void switchBranch(QStringList repoPaths, QString branchName);
    void mergeFromBranch(QStringList repoPaths, QString branchName);

signals:
    void logLine(QString line);
    void logInfo(QString line);
    void logWarning(QString line);
    void logError(QString line);
    void scanFinished(QStringList repoPaths);
    void repoBranchInfo(QString path, QString branchInfo);
    void progress(int done, int total);
    void repoResult(QString path, QString status, QString message);
    void commitFinished();
    void pushFinished();
    void switchFinished();
    void mergeFinished();
};
