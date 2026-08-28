#include "CommitWorker.h"
#include "GitProcess.h"
#include "RepoScanner.h"

#include <QDir>

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

// "git submodule add <url>" places the submodule at the URL's basename (minus ".git") by
// default. Used to locate the submodule directory right after adding it, without a rescan.
QString submoduleNameFromUrl(const QString &url)
{
    QString name = url.trimmed();
    while (name.endsWith(QLatin1Char('/')))
        name.chop(1);
    int cut = name.lastIndexOf(QLatin1Char('/'));
    if (cut < 0)
        cut = name.lastIndexOf(QLatin1Char(':'));
    if (cut >= 0)
        name = name.mid(cut + 1);
    if (name.endsWith(QStringLiteral(".git")))
        name.chop(4);
    return name;
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

        emit repoBranchInfo(path, RepoScanner::currentBranch(path));
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

void CommitWorker::initSwitchPull(QStringList repoPaths)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Init/Switch/Pull %1 ====").arg(path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Not a git repository and could not be initialized as a submodule"));
            ++done;
            emit progress(done, total);
            continue;
        }

        GitProcess::run(path, {QStringLiteral("fetch"), QStringLiteral("--all"), QStringLiteral("--prune")}, onLine);

        QString resultMessage;
        bool switched = false;
        for (const QString &candidate : {QStringLiteral("master"), QStringLiteral("main")}) {
            const auto checkoutResult = GitProcess::run(path, {QStringLiteral("checkout"), candidate}, onLine);
            if (checkoutResult.success) {
                resultMessage = QStringLiteral("Now on branch '") + candidate + QStringLiteral("'");
                switched = true;
                break;
            }
            const auto trackResult = GitProcess::run(
                path, {QStringLiteral("checkout"), QStringLiteral("-b"), candidate, QStringLiteral("--track"), QStringLiteral("origin/") + candidate}, onLine);
            if (trackResult.success) {
                resultMessage = QStringLiteral("Created local branch '") + candidate + QStringLiteral("' tracking origin/") + candidate;
                switched = true;
                break;
            }
        }

        if (!switched) {
            emit logError(QStringLiteral("Neither 'master' nor 'main' branch exists for ") + path);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Neither 'master' nor 'main' branch exists locally or on origin"));
            ++done;
            emit progress(done, total);
            continue;
        }

        const auto pullResult = GitProcess::run(path, {QStringLiteral("pull")}, onLine);
        if (!pullResult.success || looksLikeConflict(pullResult.output)) {
            GitProcess::run(path, {QStringLiteral("merge"), QStringLiteral("--abort")}, onLine);
            const QString reason = lastMeaningfulLine(pullResult.output);
            emit logError(QStringLiteral("Pull conflict/failure for ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Conflict"), QStringLiteral("%1; git pull failed or produced a conflict (reverted): %2").arg(resultMessage, reason));
            ++done;
            emit progress(done, total);
            continue;
        }

        emit repoBranchInfo(path, RepoScanner::currentBranch(path));
        emit logInfo(QStringLiteral("Ready: ") + path + QStringLiteral(" (") + resultMessage + QStringLiteral(")"));
        emit repoResult(path, QStringLiteral("OK"), resultMessage + QStringLiteral("; ") + lastMeaningfulLine(pullResult.output));
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch init/switch/pull finished."));
    emit initSwitchPullFinished();
}

void CommitWorker::removeSubmodules(QStringList repoPaths)
{
    const int total = repoPaths.size();
    int done = 0;

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Removing submodule %1 ====").arg(path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        QString parentRepoPath, relPath;
        if (!RepoScanner::findOwningRepo(path, &parentRepoPath, &relPath)) {
            emit logError(QStringLiteral("Could not find the parent repository that owns ") + path);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Could not find the parent repository that owns this submodule"));
            ++done;
            emit progress(done, total);
            continue;
        }

        GitProcess::run(parentRepoPath, {QStringLiteral("submodule"), QStringLiteral("deinit"), QStringLiteral("-f"), QStringLiteral("--"), relPath}, onLine);

        const auto rmResult = GitProcess::run(parentRepoPath, {QStringLiteral("rm"), QStringLiteral("-f"), QStringLiteral("--"), relPath}, onLine);
        if (!rmResult.success) {
            const QString reason = lastMeaningfulLine(rmResult.output);
            emit logError(QStringLiteral("Failed to remove ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("git rm failed: ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        QDir modulesDir(QDir(parentRepoPath).filePath(QStringLiteral(".git/modules/") + relPath));
        if (modulesDir.exists())
            modulesDir.removeRecursively();

        emit logInfo(QStringLiteral("Removed submodule: ") + path);
        emit repoResult(path, QStringLiteral("OK"), QStringLiteral("Submodule removed (staged in parent repository; commit to finalize)"));
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch submodule removal finished."));
    emit removeSubmodulesFinished();
}

void CommitWorker::addSubmodule(QStringList repoPaths, QString url, QString relPath)
{
    const int total = repoPaths.size();
    int done = 0;
    relPath = relPath.trimmed();

    for (const QString &path : repoPaths) {
        emit logInfo(QStringLiteral("==== Adding submodule '%1' to %2 ====").arg(url, path));
        auto onLine = [this](const QString &line) { emit logLine(line); };

        if (!RepoScanner::ensureRepoReady(path, onLine)) {
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("Not a git repository and could not be initialized as a submodule"));
            ++done;
            emit progress(done, total);
            continue;
        }

        QStringList addArgs = {QStringLiteral("submodule"), QStringLiteral("add"), url};
        if (!relPath.isEmpty())
            addArgs << relPath;

        const auto addResult = GitProcess::run(path, addArgs, onLine);
        if (!addResult.success) {
            const QString reason = lastMeaningfulLine(addResult.output);
            emit logError(QStringLiteral("Failed to add submodule to ") + path + QStringLiteral(": ") + reason);
            emit repoResult(path, QStringLiteral("Failed"), QStringLiteral("git submodule add failed: ") + reason);
            ++done;
            emit progress(done, total);
            continue;
        }

        QString resultMessage = QStringLiteral("Added submodule '%1' (staged; commit to finalize)").arg(url);

        const QString submoduleName = relPath.isEmpty() ? submoduleNameFromUrl(url) : relPath;
        if (submoduleName.isEmpty()) {
            emit logWarning(QStringLiteral("Could not determine the new submodule's directory name from '") + url + QStringLiteral("'; skipping update/switch."));
        } else {
            const QString submodulePath = QDir(path).filePath(submoduleName);

            // "submodule add" already clones the repo, but explicitly update it too so the
            // checked-out data is guaranteed to be fully brought in.
            GitProcess::run(path, {QStringLiteral("submodule"), QStringLiteral("update"), QStringLiteral("--init"), QStringLiteral("--"), submoduleName}, onLine);

            bool switched = false;
            for (const QString &candidate : {QStringLiteral("master"), QStringLiteral("main")}) {
                const auto checkoutResult = GitProcess::run(submodulePath, {QStringLiteral("checkout"), candidate}, onLine);
                if (checkoutResult.success) {
                    resultMessage += QStringLiteral("; switched to '%1'").arg(candidate);
                    switched = true;
                    break;
                }
                const auto trackResult = GitProcess::run(
                    submodulePath, {QStringLiteral("checkout"), QStringLiteral("-b"), candidate, QStringLiteral("--track"), QStringLiteral("origin/") + candidate}, onLine);
                if (trackResult.success) {
                    resultMessage += QStringLiteral("; created local branch '%1' tracking origin/%1").arg(candidate);
                    switched = true;
                    break;
                }
            }
            if (!switched)
                emit logWarning(QStringLiteral("Neither 'master' nor 'main' branch exists in the new submodule at ") + submodulePath);
        }

        emit logInfo(QStringLiteral("Added submodule '") + url + QStringLiteral("' to: ") + path);
        emit repoResult(path, QStringLiteral("OK"), resultMessage);
        ++done;
        emit progress(done, total);
    }

    emit logInfo(QStringLiteral("Batch submodule add finished."));
    emit addSubmoduleFinished();
}
