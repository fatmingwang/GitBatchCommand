#pragma once
#include <QString>
#include <QStringList>
#include <functional>

struct GitCommandResult {
    bool success = false;
    int exitCode = -1;
    QString output;
};

class GitProcess {
public:
    static GitCommandResult run(const QString &workingDir,
                                 const QStringList &args,
                                 const std::function<void(const QString &)> &onLine = nullptr,
                                 int timeoutMs = 10 * 60 * 1000);
};
