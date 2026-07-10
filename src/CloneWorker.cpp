#include "CloneWorker.h"
#include "GitProcess.h"

#include <QDir>
#include <QFileInfo>
#include <algorithm>

namespace {

QString repoNameFromUrl(const QString &url)
{
    QString u = url.trimmed();
    while (u.endsWith('/'))
        u.chop(1);
    if (u.endsWith(QStringLiteral(".git"), Qt::CaseInsensitive))
        u.chop(4);
    const int slashIdx = std::max(u.lastIndexOf('/'), u.lastIndexOf(':'));
    QString name = slashIdx >= 0 ? u.mid(slashIdx + 1) : u;
    if (name.isEmpty())
        name = QStringLiteral("repo");
    return name;
}

QString lastMeaningfulLine(const QString &output)
{
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (!it->trimmed().isEmpty())
            return it->trimmed();
    }
    return output.trimmed();
}

} // namespace

CloneWorker::CloneWorker(QObject *parent) : QObject(parent) {}

void CloneWorker::run(QStringList urls, QString destDir, bool updateSubmodules)
{
    QDir dir(destDir);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));

    const int total = urls.size();
    int done = 0;

    for (const QString &url : urls) {
        emit logInfo(QStringLiteral("==== Cloning %1 ====").arg(url));

        auto onLine = [this](const QString &line) { emit logLine(line); };

        const QString name = repoNameFromUrl(url);
        const QString targetPath = QDir(destDir).filePath(name);

        QDir targetDir(targetPath);
        if (targetDir.exists() && !targetDir.entryList(QDir::NoDotAndDotDot | QDir::AllEntries).isEmpty()) {
            emit logWarning(QStringLiteral("Skipping %1: target folder already exists and is not empty (%2)").arg(url, targetPath));
            emit itemDone(url, false, QStringLiteral("Target directory already exists and is not empty: ") + targetPath);
            ++done;
            emit progress(done, total);
            continue;
        }

        const auto cloneResult = GitProcess::run(destDir, {QStringLiteral("clone"), url, name}, onLine);
        if (!cloneResult.success) {
            const QString reason = lastMeaningfulLine(cloneResult.output);
            emit logError(QStringLiteral("Clone failed for %1: %2").arg(url, reason));
            emit itemDone(url, false, QStringLiteral("Clone failed: ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        QString branchUsed;
        bool switched = false;
        for (const QString &branch : {QStringLiteral("main"), QStringLiteral("master")}) {
            const auto coResult = GitProcess::run(targetPath, {QStringLiteral("checkout"), branch}, onLine);
            if (coResult.success) {
                branchUsed = branch;
                switched = true;
                break;
            }
        }
        if (!switched) {
            emit logWarning(QStringLiteral("No 'main' or 'master' branch found for %1; leaving default checkout.").arg(url));
        }

        if (updateSubmodules) {
            emit logInfo(QStringLiteral("Updating submodules for ") + name + QStringLiteral("..."));
            const auto subResult = GitProcess::run(targetPath, {QStringLiteral("submodule"), QStringLiteral("update"), QStringLiteral("--init"), QStringLiteral("--recursive")}, onLine);
            if (!subResult.success) {
                emit logWarning(QStringLiteral("Submodule update failed for ") + url + QStringLiteral(": ") + lastMeaningfulLine(subResult.output));
            } else {
                GitProcess::run(targetPath,
                                 {QStringLiteral("submodule"), QStringLiteral("foreach"), QStringLiteral("--recursive"),
                                  QStringLiteral("git checkout main 2>/dev/null || git checkout master 2>/dev/null || true")},
                                 onLine);
            }
        }

        const QString message = switched
            ? QStringLiteral("Cloned successfully, on branch '%1'").arg(branchUsed)
            : QStringLiteral("Cloned successfully (no main/master branch found)");
        emit itemDone(url, true, message);
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch clone finished."));
    emit finished();
}
