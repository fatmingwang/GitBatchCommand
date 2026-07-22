#pragma once
#include <QString>
#include <QStringList>
#include <functional>

// Shared repository-discovery logic used by both SyncPage and CommitPage: finds git
// repositories under a root directory along with their submodules (recursively), and can
// lazily initialize an uninitialized submodule on demand.
namespace RepoScanner {

// Returns every git repository found under rootDir, followed immediately by each of its
// submodules (recursively). A submodule path is always nested under its parent's path.
QStringList findRepositories(const QString &rootDir);

// The current branch name at path, or "(detached @ <sha>)" / "(not initialized)".
QString currentBranch(const QString &path);

// Returns true if path is (or was successfully made into) a real git repository. If path
// isn't a git repo yet, walks up to find the owning repository and runs a scoped
// "git submodule update --init -- <relPath>" for just that submodule.
bool ensureRepoReady(const QString &path, const std::function<void(const QString &)> &onLine);

} // namespace RepoScanner
