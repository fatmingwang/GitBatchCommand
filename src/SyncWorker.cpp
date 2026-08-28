#include "SyncWorker.h"
#include "GitProcess.h"
#include "RepoScanner.h"

namespace {

QString lastMeaningfulLine(const QString &output)
{
    const QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (auto it = lines.rbegin(); it != lines.rend(); ++it) {
        if (!it->trimmed().isEmpty())
            return it->trimmed();
    }
    return output.trimmed();
}

bool looksLikeConflict(const QString &output)
{
    return output.contains(QStringLiteral("CONFLICT"), Qt::CaseInsensitive)
        || output.contains(QStringLiteral("Automatic merge failed"), Qt::CaseInsensitive);
}

} // namespace

SyncWorker::SyncWorker(QObject *parent) : QObject(parent) {}

void SyncWorker::scan(QString rootDir)
{
    emit logInfo(QStringLiteral("Scanning ") + rootDir + QStringLiteral(" for git repositories..."));
    const QStringList allPaths = RepoScanner::findRepositories(rootDir);

    emit logInfo(QStringLiteral("Found %1 repositories (including submodules).").arg(allPaths.size()));
    emit scanFinished(allPaths);

    for (const QString &path : allPaths)
        emit repoBranchInfo(path, RepoScanner::currentBranch(path));
}

void SyncWorker::revertClean(QStringList repoPaths)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Revert & Clean %1 ====").arg(path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("OK"), QStringLiteral("Not initialized; nothing to revert"));
            ++done;
            emit progress(done, total);
            continue;
        }

        const auto resetResult = GitProcess::run(path, {QStringLiteral("reset"), QStringLiteral("--hard"), QStringLiteral("HEAD")}, onLine);
        if (!resetResult.success) {
            const QString reason = lastMeaningfulLine(resetResult.output);
            emit logError(QStringLiteral("Reset failed for ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("git reset --hard failed: ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        const auto cleanResult = GitProcess::run(path, {QStringLiteral("clean"), QStringLiteral("-fd")}, onLine);
        if (!cleanResult.success) {
            const QString reason = lastMeaningfulLine(cleanResult.output);
            emit logError(QStringLiteral("Clean failed for ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("git clean -fd failed: ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        emit logInfo(QStringLiteral("Reverted and cleaned: ") + path);
        emit repoResult(path, QStringLiteral("OK"), QStringLiteral("Reverted and cleaned"));
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch revert & clean finished."));
    emit revertCleanFinished();
}

void SyncWorker::pullAll(QStringList repoPaths)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Pulling %1 ====").arg(path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Not a git repository and could not be initialized as a submodule"));
            ++done;
            emit progress(done, total);
            continue;
        }

        const auto pullResult = GitProcess::run(path, {QStringLiteral("pull")}, onLine);
        if (!pullResult.success || looksLikeConflict(pullResult.output)) {
            GitProcess::run(path, {QStringLiteral("merge"), QStringLiteral("--abort")}, onLine);
            const QString reason = lastMeaningfulLine(pullResult.output);
            emit logError(QStringLiteral("Pull conflict/failure for ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Conflict"), QStringLiteral("git pull failed or produced a conflict (reverted): ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        emit logInfo(QStringLiteral("Up to date: ") + path);
        emit repoResult(path, QStringLiteral("OK"), lastMeaningfulLine(pullResult.output));
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch pull finished."));
    emit pullFinished();
}

void SyncWorker::switchBranch(QStringList repoPaths, QString branchName)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Switching %1 to '%2' ====").arg(path, branchName));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Not a git repository and could not be initialized as a submodule"));
            ++done;
            emit progress(done, total);
            continue;
        }

        GitProcess::run(path, {QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune")}, onLine);

        QString resultMessage;

        const auto checkoutResult = GitProcess::run(path, {QStringLiteral("checkout"), branchName}, onLine);
        if (checkoutResult.success) {
            resultMessage = QStringLiteral("Now on branch '") + branchName + QStringLiteral("'");
        } else {
            const auto trackResult = GitProcess::run(
                path, {QStringLiteral("checkout"), QStringLiteral("-b"), branchName, QStringLiteral("--track"), QStringLiteral("origin/") + branchName}, onLine);
            if (trackResult.success) {
                resultMessage = QStringLiteral("Created local branch '") + branchName + QStringLiteral("' tracking origin/") + branchName;
            } else {
                emit logWarning(QStringLiteral("Branch '") + branchName + QStringLiteral("' does not exist locally or on origin for ") + path + QStringLiteral("; creating a new branch."));
                const auto createResult = GitProcess::run(path, {QStringLiteral("checkout"), QStringLiteral("-b"), branchName}, onLine);
                if (createResult.success) {
                    resultMessage = QStringLiteral("Branch '") + branchName + QStringLiteral("' did not exist; created it from the current HEAD");
                } else {
                    const QString reason = lastMeaningfulLine(createResult.output);
                    emit logError(QStringLiteral("Branch switch failed for ") + path + QStringLiteral(": ") + reason);
                    emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Could not switch or create branch: ") + reason);
                    ++done;
                    emit progress(done, total);
                    continue;
                }
            }
        }

        emit repoBranchInfo(path, RepoScanner::currentBranch(path));
        emit logInfo(QStringLiteral("Switched: ") + path + QStringLiteral(" (") + resultMessage + QStringLiteral(")"));
        emit repoResult(path, QStringLiteral("OK"), resultMessage);
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch branch switch finished."));
    emit switchFinished();
}
