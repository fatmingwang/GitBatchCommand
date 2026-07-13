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

    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels({QString(), QStringLiteral("Repository"), QStringLiteral("Path"), QStringLiteral("Status")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    m_selectAllBtn = new QPushButton(QStringLiteral("Select All"), this);
    m_selectNoneBtn = new QPushButton(QStringLiteral("Select None"), this);

    m_revertCleanBtn = new QPushButton(QStringLiteral("Revert && Clean Selected"), this);
    m_revertCleanBtn->setMinimumHeight(32);

    m_pullBtn = new QPushButton(QStringLiteral("Pull Selected"), this);
    m_pullBtn->setMinimumHeight(32);

    m_branchEdit = new QLineEdit(this);
    m_branchEdit->setPlaceholderText(QStringLiteral("target branch name, e.g. develop"));
    m_switchBtn = new QPushButton(QStringLiteral("Switch Selected To Branch"), this);

    m_revertCleanSubBtn = new QPushButton(QStringLiteral("Revert && Clean Submodules"), this);
    m_revertCleanSubBtn->setMinimumHeight(32);

    m_switchSubBtn = new QPushButton(QStringLiteral("Switch Submodules To Branch && Pull"), this);
    m_switchSubBtn->setMinimumHeight(32);

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

    auto *selectionRow = new QHBoxLayout;
    selectionRow->addWidget(m_selectAllBtn);
    selectionRow->addWidget(m_selectNoneBtn);
    selectionRow->addStretch();

    auto *actionRow = new QHBoxLayout;
    actionRow->addWidget(m_revertCleanBtn);
    actionRow->addWidget(m_pullBtn);

    auto *branchRow = new QHBoxLayout;
    branchRow->addWidget(m_branchEdit);
    branchRow->addWidget(m_switchBtn);

    auto *submoduleRow = new QHBoxLayout;
    submoduleRow->addWidget(m_revertCleanSubBtn);
    submoduleRow->addWidget(m_switchSubBtn);

    auto *submoduleGroup = new QGroupBox(QStringLiteral("Submodules"), this);
    auto *submoduleLayout = new QVBoxLayout(submoduleGroup);
    submoduleLayout->addLayout(submoduleRow);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(rootGroup);
    mainLayout->addLayout(selectionRow);
    mainLayout->addWidget(m_table);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addLayout(actionRow);
    mainLayout->addLayout(branchRow);
    mainLayout->addWidget(submoduleGroup);

    connect(m_browseBtn, &QPushButton::clicked, this, &SyncPage::onBrowseRoot);
    connect(m_scanBtn, &QPushButton::clicked, this, &SyncPage::onScan);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &SyncPage::onSelectAll);
    connect(m_selectNoneBtn, &QPushButton::clicked, this, &SyncPage::onSelectNone);
    connect(m_table, &QTableWidget::itemChanged, this, &SyncPage::onTableItemChanged);
    connect(m_revertCleanBtn, &QPushButton::clicked, this, &SyncPage::onRevertClean);
    connect(m_pullBtn, &QPushButton::clicked, this, &SyncPage::onPull);
    connect(m_switchBtn, &QPushButton::clicked, this, &SyncPage::onSwitchBranch);
    connect(m_revertCleanSubBtn, &QPushButton::clicked, this, &SyncPage::onRevertCleanSubmodules);
    connect(m_switchSubBtn, &QPushButton::clicked, this, &SyncPage::onSwitchSubmodulesBranch);

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
    connect(m_worker, &SyncWorker::revertCleanSubmodulesFinished, this, &SyncPage::onRevertCleanSubmodulesFinished);
    connect(m_worker, &SyncWorker::switchSubmodulesFinished, this, &SyncPage::onSwitchSubmodulesFinished);

    m_thread->start();

    m_selectAllBtn->setEnabled(false);
    m_selectNoneBtn->setEnabled(false);
    m_revertCleanBtn->setEnabled(false);
    m_pullBtn->setEnabled(false);
    m_switchBtn->setEnabled(false);
    m_revertCleanSubBtn->setEnabled(false);
    m_switchSubBtn->setEnabled(false);
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
    m_busy = busy;
    m_browseBtn->setEnabled(!busy);
    m_scanBtn->setEnabled(!busy);
    m_rootEdit->setEnabled(!busy);
    m_branchEdit->setEnabled(!busy);
    m_selectAllBtn->setEnabled(!busy && !m_repoPaths.isEmpty());
    m_selectNoneBtn->setEnabled(!busy && !m_repoPaths.isEmpty());
    updateActionButtons();
}

void SyncPage::updateActionButtons()
{
    const bool hasSelection = !m_busy && !selectedPaths().isEmpty();
    m_revertCleanBtn->setEnabled(hasSelection);
    m_pullBtn->setEnabled(hasSelection);
    m_switchBtn->setEnabled(hasSelection);
    m_revertCleanSubBtn->setEnabled(hasSelection);
    m_switchSubBtn->setEnabled(hasSelection);
}

void SyncPage::onScan()
{
    const QString root = m_rootEdit->text().trimmed();
    if (root.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Scan"), QStringLiteral("Please choose a root directory first."));
        return;
    }
    setBusy(true);
    m_table->setRowCount(0);
    m_repoPaths.clear();
    QMetaObject::invokeMethod(m_worker, "scan", Qt::QueuedConnection, Q_ARG(QString, root));
}

void SyncPage::onSelectAll()
{
    for (int r = 0; r < m_table->rowCount(); ++r)
        setRowChecked(r, true);
}

void SyncPage::onSelectNone()
{
    for (int r = 0; r < m_table->rowCount(); ++r)
        setRowChecked(r, false);
}

void SyncPage::setRowChecked(int row, bool checked)
{
    if (auto *item = m_table->item(row, 0))
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

QStringList SyncPage::selectedPaths() const
{
    QStringList paths;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 0)->checkState() == Qt::Checked)
            paths << m_table->item(r, 2)->text();
    }
    return paths;
}

void SyncPage::onTableItemChanged(QTableWidgetItem *item)
{
    if (item && item->column() == 0)
        updateActionButtons();
}

int SyncPage::rowForPath(const QString &path) const
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 2)->text() == path)
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

        auto *checkItem = new QTableWidgetItem;
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        checkItem->setCheckState(Qt::Checked);
        m_table->setItem(r, 0, checkItem);

        m_table->setItem(r, 1, new QTableWidgetItem(name));
        m_table->setItem(r, 2, new QTableWidgetItem(path));
        m_table->setItem(r, 3, new QTableWidgetItem(QStringLiteral("Pending")));
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
        m_table->setItem(row, 3, item);
    }
}

void SyncPage::beginOperation(const QStringList &selected)
{
    m_results.clear();
    setBusy(true);
    m_progressBar->setRange(0, selected.size());
    m_progressBar->setValue(0);
    for (int r = 0; r < m_table->rowCount(); ++r) {
        const bool isSelected = m_table->item(r, 0)->checkState() == Qt::Checked;
        m_table->item(r, 3)->setText(isSelected ? QStringLiteral("Working...") : QStringLiteral("Skipped"));
    }
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
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Revert && Clean"), QStringLiteral("Please select at least one repository first."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Confirm Revert && Clean"));
    box.setText(QStringLiteral("This will discard ALL local changes (git reset --hard + git clean -fd) "
                               "in %1 selected repositories.\n\n"
                               "Uncommitted work will be LOST. Continue?").arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Starting revert & clean for %1 selected repositories...").arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "revertClean", Qt::QueuedConnection, Q_ARG(QStringList, selected));
}

void SyncPage::onRevertCleanFinished()
{
    showProblemsSummary(QStringLiteral("Revert && Clean Finished"),
                         QStringLiteral("All repositories were reverted and cleaned successfully."));
}

void SyncPage::onPull()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Pull"), QStringLiteral("Please select at least one repository first."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Pull"));
    box.setText(QStringLiteral("Pull the latest changes in %1 selected repositories?").arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Starting pull for %1 selected repositories...").arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "pullAll", Qt::QueuedConnection, Q_ARG(QStringList, selected));
}

void SyncPage::onPullFinished()
{
    showProblemsSummary(QStringLiteral("Pull Finished"),
                         QStringLiteral("All repositories were pulled successfully. No conflicts."));
}

void SyncPage::onSwitchBranch()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Switch Branch"), QStringLiteral("Please select at least one repository first."));
        return;
    }
    const QString branch = m_branchEdit->text().trimmed();
    if (branch.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Switch Branch"), QStringLiteral("Please enter a target branch name."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Branch Switch"));
    box.setText(QStringLiteral("Switch %1 selected repositories to branch '%2'?").arg(selected.size()).arg(branch));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Switching %1 selected repositories to branch '%2'...").arg(selected.size()).arg(branch));
    QMetaObject::invokeMethod(m_worker, "switchBranch", Qt::QueuedConnection,
                               Q_ARG(QStringList, selected), Q_ARG(QString, branch));
}

void SyncPage::onSwitchFinished()
{
    showProblemsSummary(QStringLiteral("Branch Switch Finished"),
                         QStringLiteral("All repositories were switched successfully."));
}

void SyncPage::onRevertCleanSubmodules()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Revert && Clean Submodules"), QStringLiteral("Please select at least one repository first."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Confirm Submodule Revert && Clean"));
    box.setText(QStringLiteral("This will discard ALL local changes (git reset --hard + git clean -fd) "
                               "in every submodule of %1 selected repositories.\n\n"
                               "Uncommitted work in those submodules will be LOST. Continue?").arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Starting submodule revert & clean for %1 selected repositories...").arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "revertCleanSubmodules", Qt::QueuedConnection, Q_ARG(QStringList, selected));
}

void SyncPage::onRevertCleanSubmodulesFinished()
{
    showProblemsSummary(QStringLiteral("Submodule Revert && Clean Finished"),
                         QStringLiteral("All submodules were reverted and cleaned successfully."));
}

void SyncPage::onSwitchSubmodulesBranch()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Switch Submodules Branch"), QStringLiteral("Please select at least one repository first."));
        return;
    }
    const QString branch = m_branchEdit->text().trimmed();
    if (branch.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Switch Submodules Branch"), QStringLiteral("Please enter a target branch name."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Submodule Branch Switch"));
    box.setText(QStringLiteral("Switch every submodule of %1 selected repositories to branch '%2' and pull latest?").arg(selected.size()).arg(branch));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Switching submodules of %1 selected repositories to branch '%2' and pulling...").arg(selected.size()).arg(branch));
    QMetaObject::invokeMethod(m_worker, "switchSubmodulesAndPull", Qt::QueuedConnection,
                               Q_ARG(QStringList, selected), Q_ARG(QString, branch));
}

void SyncPage::onSwitchSubmodulesFinished()
{
    showProblemsSummary(QStringLiteral("Submodule Branch Switch Finished"),
                         QStringLiteral("All submodules were switched and pulled successfully."));
}
