#include "SyncPage.h"
#include "SyncWorker.h"
#include "LogPanel.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QLineEdit>
#include <QPushButton>
#include <QProgressBar>
#include <QThread>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaObject>
#include <QDir>
#include <QColor>

SyncPage::SyncPage(LogPanel *logPanel, QWidget *parent)
    : QWidget(parent), m_logPanel(logPanel)
{
    m_rootEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(QStringLiteral("Browse..."), this);
    m_scanBtn = new QPushButton(QStringLiteral("Scan"), this);

    m_table = new QTableWidget(0, 3, this);
    m_table->setHorizontalHeaderLabels({QStringLiteral("Repository"), QStringLiteral("Path"), QStringLiteral("Status")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    m_revertCleanBtn = new QPushButton(QStringLiteral("Revert && Clean All"), this);
    m_revertCleanBtn->setMinimumHeight(32);

    m_pullBtn = new QPushButton(QStringLiteral("Pull All"), this);
    m_pullBtn->setMinimumHeight(32);

    m_branchEdit = new QLineEdit(this);
    m_branchEdit->setPlaceholderText(QStringLiteral("target branch name, e.g. develop"));
    m_switchBtn = new QPushButton(QStringLiteral("Switch All To Branch"), this);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);

    auto *rootRow = new QHBoxLayout;
    rootRow->addWidget(m_rootEdit);
    rootRow->addWidget(m_browseBtn);
    rootRow->addWidget(m_scanBtn);

    auto *rootGroup = new QGroupBox(QStringLiteral("Root Directory"), this);
    auto *rootLayout = new QVBoxLayout(rootGroup);
    rootLayout->addLayout(rootRow);

    auto *actionRow = new QHBoxLayout;
    actionRow->addWidget(m_revertCleanBtn);
    actionRow->addWidget(m_pullBtn);

    auto *branchRow = new QHBoxLayout;
    branchRow->addWidget(m_branchEdit);
    branchRow->addWidget(m_switchBtn);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(rootGroup);
    mainLayout->addWidget(m_table);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addLayout(actionRow);
    mainLayout->addLayout(branchRow);

    connect(m_browseBtn, &QPushButton::clicked, this, &SyncPage::onBrowseRoot);
    connect(m_scanBtn, &QPushButton::clicked, this, &SyncPage::onScan);
    connect(m_revertCleanBtn, &QPushButton::clicked, this, &SyncPage::onRevertClean);
    connect(m_pullBtn, &QPushButton::clicked, this, &SyncPage::onPull);
    connect(m_switchBtn, &QPushButton::clicked, this, &SyncPage::onSwitchBranch);

    m_thread = new QThread(this);
    m_worker = new SyncWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &SyncWorker::logLine, m_logPanel, &LogPanel::appendRaw);
    connect(m_worker, &SyncWorker::logInfo, m_logPanel, &LogPanel::appendInfo);
    connect(m_worker, &SyncWorker::logWarning, m_logPanel, &LogPanel::appendWarning);
    connect(m_worker, &SyncWorker::logError, m_logPanel, &LogPanel::appendError);
    connect(m_worker, &SyncWorker::scanFinished, this, &SyncPage::onScanFinished);
    connect(m_worker, &SyncWorker::progress, this, &SyncPage::onProgress);
    connect(m_worker, &SyncWorker::repoResult, this, &SyncPage::onRepoResult);
    connect(m_worker, &SyncWorker::revertCleanFinished, this, &SyncPage::onRevertCleanFinished);
    connect(m_worker, &SyncWorker::pullFinished, this, &SyncPage::onPullFinished);
    connect(m_worker, &SyncWorker::switchFinished, this, &SyncPage::onSwitchFinished);

    m_thread->start();

    m_revertCleanBtn->setEnabled(false);
    m_pullBtn->setEnabled(false);
    m_switchBtn->setEnabled(false);
}

SyncPage::~SyncPage()
{
    m_thread->quit();
    m_thread->wait();
}

void SyncPage::onBrowseRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Root Directory"));
    if (!dir.isEmpty())
        m_rootEdit->setText(dir);
}

void SyncPage::setBusy(bool busy)
{
    m_browseBtn->setEnabled(!busy);
    m_scanBtn->setEnabled(!busy);
    m_rootEdit->setEnabled(!busy);
    m_branchEdit->setEnabled(!busy);
    const bool hasRepos = !m_repoPaths.isEmpty();
    m_revertCleanBtn->setEnabled(!busy && hasRepos);
    m_pullBtn->setEnabled(!busy && hasRepos);
    m_switchBtn->setEnabled(!busy && hasRepos);
}

void SyncPage::onScan()
{
    const QString root = m_rootEdit->text().trimmed();
    if (root.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Scan"), QStringLiteral("Please choose a root directory first."));
        return;
    }
    setBusy(true);
    m_revertCleanBtn->setEnabled(false);
    m_pullBtn->setEnabled(false);
    m_switchBtn->setEnabled(false);
    m_table->setRowCount(0);
    m_repoPaths.clear();
    QMetaObject::invokeMethod(m_worker, "scan", Qt::QueuedConnection, Q_ARG(QString, root));
}

int SyncPage::rowForPath(const QString &path) const
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 1)->text() == path)
            return r;
    }
    return -1;
}

void SyncPage::populateTable(const QStringList &paths)
{
    m_table->setRowCount(paths.size());
    for (int r = 0; r < paths.size(); ++r) {
        const QString &path = paths[r];
        const QString name = QDir(path).dirName();
        m_table->setItem(r, 0, new QTableWidgetItem(name));
        m_table->setItem(r, 1, new QTableWidgetItem(path));
        m_table->setItem(r, 2, new QTableWidgetItem(QStringLiteral("Pending")));
    }
}

void SyncPage::onScanFinished(QStringList repoPaths)
{
    m_repoPaths = repoPaths;
    populateTable(repoPaths);
    setBusy(false);

    if (repoPaths.isEmpty())
        QMessageBox::information(this, QStringLiteral("Scan Complete"), QStringLiteral("No git repositories were found under this directory."));
}

void SyncPage::onProgress(int done, int total)
{
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(done);
}

void SyncPage::onRepoResult(QString path, QString status, QString message)
{
    m_results.append({path, status, message});
    const int row = rowForPath(path);
    if (row >= 0) {
        auto *item = new QTableWidgetItem(status);
        if (status == QStringLiteral("OK"))
            item->setForeground(QColor(QStringLiteral("#1f8f3c")));
        else if (status == QStringLiteral("Conflict"))
            item->setForeground(QColor(QStringLiteral("#c98a00")));
        else
            item->setForeground(QColor(QStringLiteral("#d32f2f")));
        item->setToolTip(message);
        m_table->setItem(row, 2, item);
    }
}

void SyncPage::beginOperation(int repoCount)
{
    m_results.clear();
    setBusy(true);
    m_progressBar->setRange(0, repoCount);
    m_progressBar->setValue(0);
    for (int r = 0; r < m_table->rowCount(); ++r)
        m_table->item(r, 2)->setText(QStringLiteral("Working..."));
}

void SyncPage::showProblemsSummary(const QString &title, const QString &okMessage)
{
    setBusy(false);

    QVector<SyncRepoRow> problems;
    for (const auto &r : m_results)
        if (r.status != QStringLiteral("OK"))
            problems.append(r);

    if (problems.isEmpty()) {
        QMessageBox::information(this, title, okMessage);
        return;
    }

    QString details;
    for (const auto &r : problems)
        details += QStringLiteral("[%1] %2\n    %3\n").arg(r.status, r.path, r.message);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(QStringLiteral("%1 of %2 repositories had conflicts or failures.").arg(problems.size()).arg(m_results.size()));
    box.setDetailedText(details);
    box.exec();
}

void SyncPage::onRevertClean()
{
    if (m_repoPaths.isEmpty())
        return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Confirm Revert && Clean"));
    box.setText(QStringLiteral("This will discard ALL local changes (git reset --hard + git clean -fd) "
                               "in %1 repositories.\n\n"
                               "Uncommitted work will be LOST. Continue?").arg(m_repoPaths.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(m_repoPaths.size());
    m_logPanel->appendInfo(QStringLiteral("Starting revert & clean for %1 repositories...").arg(m_repoPaths.size()));
    QMetaObject::invokeMethod(m_worker, "revertClean", Qt::QueuedConnection, Q_ARG(QStringList, m_repoPaths));
}

void SyncPage::onRevertCleanFinished()
{
    showProblemsSummary(QStringLiteral("Revert && Clean Finished"),
                         QStringLiteral("All repositories were reverted and cleaned successfully."));
}

void SyncPage::onPull()
{
    if (m_repoPaths.isEmpty())
        return;

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Pull"));
    box.setText(QStringLiteral("Pull the latest changes in %1 repositories?").arg(m_repoPaths.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(m_repoPaths.size());
    m_logPanel->appendInfo(QStringLiteral("Starting pull for %1 repositories...").arg(m_repoPaths.size()));
    QMetaObject::invokeMethod(m_worker, "pullAll", Qt::QueuedConnection, Q_ARG(QStringList, m_repoPaths));
}

void SyncPage::onPullFinished()
{
    showProblemsSummary(QStringLiteral("Pull Finished"),
                         QStringLiteral("All repositories were pulled successfully. No conflicts."));
}

void SyncPage::onSwitchBranch()
{
    if (m_repoPaths.isEmpty())
        return;
    const QString branch = m_branchEdit->text().trimmed();
    if (branch.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Switch Branch"), QStringLiteral("Please enter a target branch name."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Branch Switch"));
    box.setText(QStringLiteral("Switch %1 repositories to branch '%2'?").arg(m_repoPaths.size()).arg(branch));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(m_repoPaths.size());
    m_logPanel->appendInfo(QStringLiteral("Switching %1 repositories to branch '%2'...").arg(m_repoPaths.size()).arg(branch));
    QMetaObject::invokeMethod(m_worker, "switchBranch", Qt::QueuedConnection,
                               Q_ARG(QStringList, m_repoPaths), Q_ARG(QString, branch));
}

void SyncPage::onSwitchFinished()
{
    showProblemsSummary(QStringLiteral("Branch Switch Finished"),
                         QStringLiteral("All repositories were switched successfully."));
}
