#include "RepoScanner.h"
#include "GitProcess.h"

#include <QDir>
#include <QFileInfo>

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

bool isGitRepo(const QString &path)
{
    return QFileInfo::exists(QDir(path).filePath(QStringLiteral(".git")));
}

void scanRecursive(const QString &path, QStringList &out)
{
    QDir dir(path);
    if (!dir.exists())
        return;
    if (dir.exists(QStringLiteral(".git"))) {
        out << QDir::toNativeSeparators(dir.absolutePath());
        return;
    }
    const auto entries = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::NoSymLinks);
    for (const QFileInfo &fi : entries) {
        scanRecursive(fi.absoluteFilePath(), out);
    }
}

void collectSubmodulePaths(const QString &repoPath, QStringList &out)
{
    if (!QFileInfo::exists(QDir(repoPath).filePath(QStringLiteral(".gitmodules"))))
        return;

    const auto result = GitProcess::run(
        repoPath, {QStringLiteral("config"), QStringLiteral("-f"), QStringLiteral(".gitmodules"), QStringLiteral("--get-regexp"), QStringLiteral("path")}, nullptr);

    const QStringList lines = result.output.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    for (const QString &line : lines) {
        const int spaceIdx = line.indexOf(QLatin1Char(' '));
        if (spaceIdx < 0)
            continue;
        const QString relPath = line.mid(spaceIdx + 1).trimmed();
        if (relPath.isEmpty())
            continue;

        const QString absPath = QDir::toNativeSeparators(QDir(repoPath).filePath(relPath));
        out << absPath;

        // Recurse for nested submodules; if this one isn't initialized yet, there's nothing on
        // disk to recurse into and collectSubmodulePaths simply finds no .gitmodules there.
        collectSubmodulePaths(absPath, out);
    }
}

} // namespace

namespace RepoScanner {

QStringList findRepositories(const QString &rootDir)
{
    QStringList topRepos;
    scanRecursive(rootDir, topRepos);
    topRepos.sort(Qt::CaseInsensitive);

    QStringList allPaths;
    for (const QString &repo : topRepos) {
        allPaths << repo;
        collectSubmodulePaths(repo, allPaths);
    }
    return allPaths;
}

QString currentBranch(const QString &path)
{
    if (!isGitRepo(path))
        return QStringLiteral("(not initialized)");

    const auto branchResult = GitProcess::run(path, {QStringLiteral("branch"), QStringLiteral("--show-current")}, nullptr);
    QString branch = lastMeaningfulLine(branchResult.output);
    if (branch.isEmpty()) {
        const auto shaResult = GitProcess::run(path, {QStringLiteral("rev-parse"), QStringLiteral("--short"), QStringLiteral("HEAD")}, nullptr);
        branch = QStringLiteral("(detached @ %1)").arg(lastMeaningfulLine(shaResult.output));
    }
    return branch;
}

bool ensureRepoReady(const QString &path, const std::function<void(const QString &)> &onLine)
{
    if (isGitRepo(path))
        return true;

    // Not a git repo yet - it may be an uninitialized submodule. Walk up to find the owning
    // repository (parent repo or another submodule) and initialize just this one on demand.
    QString parentRepoPath, relPath;
    if (!findOwningRepo(path, &parentRepoPath, &relPath))
        return false;

    GitProcess::run(parentRepoPath,
                     {QStringLiteral("submodule"), QStringLiteral("update"), QStringLiteral("--init"), QStringLiteral("--"), relPath},
                     onLine);
    return isGitRepo(path);
}

bool findOwningRepo(const QString &path, QString *parentRepoPath, QString *relPath)
{
    QDir parent(path);
    while (parent.cdUp()) {
        if (isGitRepo(parent.absolutePath())) {
            if (parentRepoPath)
                *parentRepoPath = parent.absolutePath();
            if (relPath)
                *relPath = parent.relativeFilePath(path);
            return true;
        }
        if (parent.isRoot())
            break;
    }
    return false;
}

} // namespace RepoScanner
