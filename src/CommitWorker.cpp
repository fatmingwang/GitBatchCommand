#include "CommitWorker.h"
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

CommitWorker::CommitWorker(QObject *parent) : QObject(parent) {}

void CommitWorker::scan(QString rootDir)
{
    emit logInfo(QStringLiteral("Scanning ") + rootDir + QStringLiteral(" for git repositories..."));
    const QStringList allPaths = RepoScanner::findRepositories(rootDir);

    emit logInfo(QStringLiteral("Found %1 repositories (including submodules).").arg(allPaths.size()));
    emit scanFinished(allPaths);

    for (const QString &path : allPaths)
        emit repoBranchInfo(path, RepoScanner::currentBranch(path));
}

void CommitWorker::commitAll(QStringList repoPaths, QString message, bool stageAll)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Committing %1 ====").arg(path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("OK"), QStringLiteral("Not initialized; nothing to commit"));
            ++done;
            emit progress(done, total);
            continue;
        }

        if (stageAll) {
            const auto addResult = GitProcess::run(path, {QStringLiteral("add"), QStringLiteral("-A")}, onLine);
            if (!addResult.success) {
                const QString reason = lastMeaningfulLine(addResult.output);
                emit logError(QStringLiteral("Stage failed for ") + path + QStringLiteral(": ") + reason);
                emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("git add -A failed: ") + reason);
                ++done;
                emit progress(done, total);
                continue;
            }
        }

        const auto commitResult = GitProcess::run(path, {QStringLiteral("commit"), QStringLiteral("-m"), message}, onLine);
        if (!commitResult.success) {
            if (commitResult.output.contains(QStringLiteral("nothing to commit"), Qt::CaseInsensitive)) {
                emit logInfo(QStringLiteral("Nothing to commit: ") + path);
                emit repoResult(path, QStringLiteral("OK"), QStringLiteral("Nothing to commit"));
                ++done;
                emit progress(done, total);
                continue;
            }
            const QString reason = lastMeaningfulLine(commitResult.output);
            emit logError(QStringLiteral("Commit failed for ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("git commit failed: ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        emit logInfo(QStringLiteral("Committed: ") + path);
        emit repoResult(path, QStringLiteral("OK"), lastMeaningfulLine(commitResult.output));
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch commit finished."));
    emit commitFinished();
}

void CommitWorker::pushAll(QStringList repoPaths)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Pushing %1 ====").arg(path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Not a git repository and could not be initialized as a submodule"));
            ++done;
            emit progress(done, total);
            continue;
        }

        auto pushResult = GitProcess::run(path, {QStringLiteral("push")}, onLine);
        if (!pushResult.success && pushResult.output.contains(QStringLiteral("has no upstream branch"), Qt::CaseInsensitive)) {
            const auto branchResult = GitProcess::run(path, {QStringLiteral("rev-parse"), QStringLiteral("--abbrev-ref"), QStringLiteral("HEAD")}, nullptr);
            const QString branch = lastMeaningfulLine(branchResult.output);
            if (branchResult.success && !branch.isEmpty()) {
                emit logWarning(QStringLiteral("No upstream for ") + path + QStringLiteral("; setting upstream to origin/") + branch);
                pushResult = GitProcess::run(path, {QStringLiteral("push"), QStringLiteral("--set-upstream"), QStringLiteral("origin"), branch}, onLine);
            }
        }

        if (!pushResult.success) {
            const QString reason = lastMeaningfulLine(pushResult.output);
            emit logError(QStringLiteral("Push failed for ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("git push failed: ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        emit logInfo(QStringLiteral("Pushed: ") + path);
        emit repoResult(path, QStringLiteral("OK"), lastMeaningfulLine(pushResult.output));
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch push finished."));
    emit pushFinished();
}

void CommitWorker::switchBranch(QStringList repoPaths, QString branchName)
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

        emit logInfo(QStringLiteral("Switched: ") + path + QStringLiteral(" (") + resultMessage + QStringLiteral(")"));
        emit repoResult(path, QStringLiteral("OK"), resultMessage);
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch branch switch finished."));
    emit switchFinished();
}

void CommitWorker::mergeFromBranch(QStringList repoPaths, QString branchName)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Merging '%1' into %2 ====").arg(branchName, path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Not a git repository and could not be initialized as a submodule"));
            ++done;
            emit progress(done, total);
            continue;
        }

        GitProcess::run(path, {QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune")}, onLine);

        auto mergeResult = GitProcess::run(path, {QStringLiteral("merge"), branchName}, onLine);
        QString sourceUsed = branchName;
        if (!mergeResult.success && !looksLikeConflict(mergeResult.output)) {
            sourceUsed = QStringLiteral("origin/") + branchName;
            mergeResult = GitProcess::run(path, {QStringLiteral("merge"), sourceUsed}, onLine);
        }

        if (!mergeResult.success || looksLikeConflict(mergeResult.output)) {
            GitProcess::run(path, {QStringLiteral("merge"), QStringLiteral("--abort")}, onLine);
            const QString reason = lastMeaningfulLine(mergeResult.output);
            emit logError(QStringLiteral("Merge conflict/failure for ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Conflict"), QStringLiteral("Merge from '%1' failed or conflicted (aborted): %2").arg(branchName, reason));
            ++done;
            emit progress(done, total);
            continue;
        }

        emit logInfo(QStringLiteral("Merged: ") + path);
        emit repoResult(path, QStringLiteral("OK"), QStringLiteral("Merged from '%1'").arg(sourceUsed));
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch merge finished."));
    emit mergeFinished();
}
