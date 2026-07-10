#pragma once
#include <QObject>
#include <QStringList>

class CloneWorker : public QObject {
    Q_OBJECT
public:
    explicit CloneWorker(QObject *parent = nullptr);

public slots:
    void run(QStringList urls, QString destDir, bool updateSubmodules);

signals:
    void logLine(QString line);
    void logInfo(QString line);
    void logWarning(QString line);
    void logError(QString line);
    void progress(int done, int total);
    void itemDone(QString url, bool success, QString message);
    void finished();
};
