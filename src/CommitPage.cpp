#include "CommitPage.h"
#include "CommitWorker.h"
#include "LogPanel.h"

#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QListWidget>
#include <QLineEdit>
#include <QLabel>
#include <QCheckBox>
#include <QPushButton>
#include <QProgressBar>
#include <QThread>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QSplitter>
#include <QScrollArea>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QMetaObject>
#include <QDir>
#include <QColor>
#include <QSettings>

namespace {
const auto kLastRootDirKey = QStringLiteral("lastRootDirectory");

// Qt::UserRole "kind" tags for Quick Select list items.
constexpr int kPickAllMain = 1;      // checks every top-level (non-submodule) row
constexpr int kPickAllWithName = 2;  // Qt::UserRole+1 holds a submodule folder name
}

CommitPage::CommitPage(LogPanel *logPanel, QWidget *parent)
    : QWidget(parent), m_logPanel(logPanel)
{
    m_rootEdit = new QLineEdit(this);
    m_rootEdit->setText(QSettings().value(kLastRootDirKey).toString());
    m_browseBtn = new QPushButton(QStringLiteral("Browse..."), this);
    m_scanBtn = new QPushButton(QStringLiteral("Scan"), this);

    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels({QString(), QStringLiteral("Repository"), QStringLiteral("Path"), QStringLiteral("Branch"), QStringLiteral("Status")});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->setColumnWidth(0, 28);
    m_table->setColumnWidth(1, 200);
    m_table->setColumnWidth(2, 320);
    m_table->setColumnWidth(3, 160);
    m_table->setColumnWidth(4, 90);
    m_table->setWordWrap(true);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::NoSelection);

    m_pickList = new QListWidget(this);
    m_pickList->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_checkSelectedBtn = new QPushButton(QStringLiteral("Check Selected"), this);

    m_selectAllBtn = new QPushButton(QStringLiteral("Select All"), this);
    m_selectNoneBtn = new QPushButton(QStringLiteral("Select None"), this);

    m_commitMessageEdit = new QLineEdit(this);
    m_commitMessageEdit->setPlaceholderText(QStringLiteral("commit message"));
    m_stageAllCheck = new QCheckBox(QStringLiteral("Stage all changes (git add -A) before commit"), this);
    m_stageAllCheck->setChecked(true);
    m_commitBtn = new QPushButton(QStringLiteral("Commit Selected"), this);
    m_commitBtn->setMinimumHeight(32);

    m_pushBtn = new QPushButton(QStringLiteral("Push Selected"), this);
    m_pushBtn->setMinimumHeight(32);

    m_switchBranchEdit = new QLineEdit(this);
    m_switchBranchEdit->setPlaceholderText(QStringLiteral("target branch name, e.g. develop"));
    m_switchBtn = new QPushButton(QStringLiteral("Switch Selected To Branch"), this);
    m_switchBtn->setMinimumHeight(32);

    m_mergeBranchEdit = new QLineEdit(this);
    m_mergeBranchEdit->setPlaceholderText(QStringLiteral("source branch name, e.g. develop"));
    m_mergeBtn = new QPushButton(QStringLiteral("Merge Selected From Branch"), this);
    m_mergeBtn->setMinimumHeight(32);

    m_initSwitchPullBtn = new QPushButton(QStringLiteral("Init Submodule (if needed) + Switch to master/main + Pull"), this);
    m_initSwitchPullBtn->setMinimumHeight(32);

    m_removeSubmoduleBtn = new QPushButton(QStringLiteral("Remove Checked Submodule(s)"), this);
    m_removeSubmoduleBtn->setMinimumHeight(32);

    m_addSubmoduleUrlEdit = new QLineEdit(this);
    m_addSubmoduleUrlEdit->setPlaceholderText(QStringLiteral("submodule git URL, e.g. https://github.com/user/repo.git"));
    m_addSubmodulePathEdit = new QLineEdit(this);
    m_addSubmodulePathEdit->setPlaceholderText(QStringLiteral("optional path, e.g. libs/repo (defaults to the repository name at the root)"));
    m_addSubmoduleBtn = new QPushButton(QStringLiteral("Add Submodule To Checked Main Repositories"), this);
    m_addSubmoduleBtn->setMinimumHeight(32);

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

    auto *pickGroup = new QGroupBox(QStringLiteral("Quick Select"), this);
    auto *pickLayout = new QVBoxLayout(pickGroup);
    pickLayout->addWidget(m_pickList, 1);
    pickLayout->addWidget(m_checkSelectedBtn);
    pickGroup->setMinimumWidth(220);

    auto *commitButtonRow = new QHBoxLayout;
    commitButtonRow->addWidget(m_commitBtn);
    commitButtonRow->addWidget(m_pushBtn);

    auto *commitGroup = new QGroupBox(QStringLiteral("Commit / Push"), this);
    auto *commitLayout = new QVBoxLayout(commitGroup);
    commitLayout->addWidget(m_commitMessageEdit);
    commitLayout->addWidget(m_stageAllCheck);
    commitLayout->addLayout(commitButtonRow);

    auto *switchRow = new QHBoxLayout;
    switchRow->addWidget(m_switchBranchEdit);
    switchRow->addWidget(m_switchBtn);

    auto *switchGroup = new QGroupBox(QStringLiteral("Switch Branch"), this);
    auto *switchLayout = new QVBoxLayout(switchGroup);
    switchLayout->addLayout(switchRow);

    auto *mergeRow = new QHBoxLayout;
    mergeRow->addWidget(m_mergeBranchEdit);
    mergeRow->addWidget(m_mergeBtn);

    auto *mergeGroup = new QGroupBox(QStringLiteral("Merge From Branch"), this);
    auto *mergeLayout = new QVBoxLayout(mergeGroup);
    mergeLayout->addLayout(mergeRow);

    auto *initSwitchPullGroup = new QGroupBox(QStringLiteral("Submodule Init && Sync"), this);
    auto *initSwitchPullLayout = new QVBoxLayout(initSwitchPullGroup);
    auto *initSwitchPullLabel = new QLabel(QStringLiteral("For each selected repository/submodule: initialize it if not yet checked out, switch to 'master' (falling back to 'main'), then pull."), initSwitchPullGroup);
    initSwitchPullLabel->setWordWrap(true);
    initSwitchPullLayout->addWidget(initSwitchPullLabel);
    initSwitchPullLayout->addWidget(m_initSwitchPullBtn);

    auto *removeSubmoduleGroup = new QGroupBox(QStringLiteral("Remove Submodule"), this);
    auto *removeSubmoduleLayout = new QVBoxLayout(removeSubmoduleGroup);
    auto *removeSubmoduleLabel = new QLabel(QStringLiteral("Deinitializes and removes each checked submodule from disk and from its parent repository's index (staged; commit the parent to finalize)."), removeSubmoduleGroup);
    removeSubmoduleLabel->setWordWrap(true);
    removeSubmoduleLayout->addWidget(removeSubmoduleLabel);
    removeSubmoduleLayout->addWidget(m_removeSubmoduleBtn);

    auto *addSubmoduleUrlRow = new QHBoxLayout;
    addSubmoduleUrlRow->addWidget(new QLabel(QStringLiteral("URL:"), this));
    addSubmoduleUrlRow->addWidget(m_addSubmoduleUrlEdit);

    auto *addSubmodulePathRow = new QHBoxLayout;
    addSubmodulePathRow->addWidget(new QLabel(QStringLiteral("Path:"), this));
    addSubmodulePathRow->addWidget(m_addSubmodulePathEdit);

    auto *addSubmoduleGroup = new QGroupBox(QStringLiteral("Add Submodule"), this);
    auto *addSubmoduleLayout = new QVBoxLayout(addSubmoduleGroup);
    auto *addSubmoduleLabel = new QLabel(QStringLiteral("Adds a new submodule from the URL below (optionally at a specific path, instead of the repository root) to each checked main (non-submodule) repository, updates it, and switches it to 'master' (falling back to 'main')."), addSubmoduleGroup);
    addSubmoduleLabel->setWordWrap(true);
    addSubmoduleLayout->addWidget(addSubmoduleLabel);
    addSubmoduleLayout->addLayout(addSubmoduleUrlRow);
    addSubmoduleLayout->addLayout(addSubmodulePathRow);
    addSubmoduleLayout->addWidget(m_addSubmoduleBtn);

    auto *tableColumn = new QWidget(this);
    auto *tableColumnLayout = new QVBoxLayout(tableColumn);
    tableColumnLayout->setContentsMargins(0, 0, 0, 0);
    tableColumnLayout->addLayout(selectionRow);
    tableColumnLayout->addWidget(m_table, 1);
    tableColumnLayout->addWidget(m_progressBar);

    auto *actionsColumn = new QWidget(this);
    auto *actionsColumnLayout = new QVBoxLayout(actionsColumn);
    actionsColumnLayout->setContentsMargins(0, 0, 0, 0);
    actionsColumnLayout->addWidget(commitGroup);
    actionsColumnLayout->addWidget(switchGroup);
    actionsColumnLayout->addWidget(mergeGroup);
    actionsColumnLayout->addWidget(initSwitchPullGroup);
    actionsColumnLayout->addWidget(removeSubmoduleGroup);
    actionsColumnLayout->addWidget(addSubmoduleGroup);
    actionsColumnLayout->addStretch();

    auto *actionsScrollArea = new QScrollArea(this);
    actionsScrollArea->setWidget(actionsColumn);
    actionsScrollArea->setWidgetResizable(true);
    actionsScrollArea->setFrameShape(QFrame::NoFrame);
    actionsScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    actionsScrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

    auto *contentSplitter = new QSplitter(Qt::Horizontal, this);
    contentSplitter->addWidget(tableColumn);
    contentSplitter->addWidget(actionsScrollArea);
    contentSplitter->setStretchFactor(0, 3);
    contentSplitter->setStretchFactor(1, 2);
    contentSplitter->setSizes({620, 380});

    auto *leftContainer = new QWidget(this);
    auto *leftLayout = new QVBoxLayout(leftContainer);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->addWidget(rootGroup);
    leftLayout->addWidget(contentSplitter, 1);

    auto *bodySplitter = new QSplitter(Qt::Horizontal, this);
    bodySplitter->addWidget(leftContainer);
    bodySplitter->addWidget(pickGroup);
    bodySplitter->setStretchFactor(0, 4);
    bodySplitter->setStretchFactor(1, 1);
    bodySplitter->setSizes({1020, 260});

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(bodySplitter);

    connect(m_browseBtn, &QPushButton::clicked, this, &CommitPage::onBrowseRoot);
    connect(m_scanBtn, &QPushButton::clicked, this, &CommitPage::onScan);
    connect(m_selectAllBtn, &QPushButton::clicked, this, &CommitPage::onSelectAll);
    connect(m_selectNoneBtn, &QPushButton::clicked, this, &CommitPage::onSelectNone);
    connect(m_checkSelectedBtn, &QPushButton::clicked, this, &CommitPage::onCheckSelectedFromList);
    connect(m_table, &QTableWidget::itemChanged, this, &CommitPage::onTableItemChanged);
    connect(m_commitBtn, &QPushButton::clicked, this, &CommitPage::onCommit);
    connect(m_pushBtn, &QPushButton::clicked, this, &CommitPage::onPush);
    connect(m_switchBtn, &QPushButton::clicked, this, &CommitPage::onSwitchBranch);
    connect(m_mergeBtn, &QPushButton::clicked, this, &CommitPage::onMergeBranch);
    connect(m_initSwitchPullBtn, &QPushButton::clicked, this, &CommitPage::onInitSwitchPull);
    connect(m_removeSubmoduleBtn, &QPushButton::clicked, this, &CommitPage::onRemoveSubmodule);
    connect(m_addSubmoduleBtn, &QPushButton::clicked, this, &CommitPage::onAddSubmodule);

    m_thread = new QThread(this);
    m_worker = new CommitWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &CommitWorker::logLine, m_logPanel, &LogPanel::appendRaw);
    connect(m_worker, &CommitWorker::logInfo, m_logPanel, &LogPanel::appendInfo);
    connect(m_worker, &CommitWorker::logWarning, m_logPanel, &LogPanel::appendWarning);
    connect(m_worker, &CommitWorker::logError, m_logPanel, &LogPanel::appendError);
    connect(m_worker, &CommitWorker::scanFinished, this, &CommitPage::onScanFinished);
    connect(m_worker, &CommitWorker::repoBranchInfo, this, &CommitPage::onRepoBranchInfo);
    connect(m_worker, &CommitWorker::progress, this, &CommitPage::onProgress);
    connect(m_worker, &CommitWorker::repoResult, this, &CommitPage::onRepoResult);
    connect(m_worker, &CommitWorker::commitFinished, this, &CommitPage::onCommitFinished);
    connect(m_worker, &CommitWorker::pushFinished, this, &CommitPage::onPushFinished);
    connect(m_worker, &CommitWorker::switchFinished, this, &CommitPage::onSwitchFinished);
    connect(m_worker, &CommitWorker::mergeFinished, this, &CommitPage::onMergeFinished);
    connect(m_worker, &CommitWorker::initSwitchPullFinished, this, &CommitPage::onInitSwitchPullFinished);
    connect(m_worker, &CommitWorker::removeSubmodulesFinished, this, &CommitPage::onRemoveSubmoduleFinished);
    connect(m_worker, &CommitWorker::addSubmoduleFinished, this, &CommitPage::onAddSubmoduleFinished);

    m_thread->start();

    m_selectAllBtn->setEnabled(false);
    m_selectNoneBtn->setEnabled(false);
    m_checkSelectedBtn->setEnabled(false);
    m_commitBtn->setEnabled(false);
    m_pushBtn->setEnabled(false);
    m_switchBtn->setEnabled(false);
    m_mergeBtn->setEnabled(false);
    m_initSwitchPullBtn->setEnabled(false);
    m_removeSubmoduleBtn->setEnabled(false);
    m_addSubmoduleBtn->setEnabled(false);
}

CommitPage::~CommitPage()
{
    m_thread->quit();
    m_thread->wait();
}

void CommitPage::onBrowseRoot()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Root Directory"));
    if (!dir.isEmpty()) {
        m_rootEdit->setText(dir);
        QSettings().setValue(kLastRootDirKey, dir);
    }
}

void CommitPage::setBusy(bool busy)
{
    m_busy = busy;
    m_browseBtn->setEnabled(!busy);
    m_scanBtn->setEnabled(!busy);
    m_rootEdit->setEnabled(!busy);
    m_commitMessageEdit->setEnabled(!busy);
    m_stageAllCheck->setEnabled(!busy);
    m_switchBranchEdit->setEnabled(!busy);
    m_mergeBranchEdit->setEnabled(!busy);
    m_addSubmoduleUrlEdit->setEnabled(!busy);
    m_addSubmodulePathEdit->setEnabled(!busy);
    m_selectAllBtn->setEnabled(!busy && !m_repoPaths.isEmpty());
    m_selectNoneBtn->setEnabled(!busy && !m_repoPaths.isEmpty());
    m_checkSelectedBtn->setEnabled(!busy && !m_repoPaths.isEmpty());
    m_pickList->setEnabled(!busy);
    updateActionButtons();
}

void CommitPage::updateActionButtons()
{
    const bool hasSelection = !m_busy && !selectedPaths().isEmpty();
    m_commitBtn->setEnabled(hasSelection);
    m_pushBtn->setEnabled(hasSelection);
    m_switchBtn->setEnabled(hasSelection);
    m_mergeBtn->setEnabled(hasSelection);
    m_initSwitchPullBtn->setEnabled(hasSelection);
    m_removeSubmoduleBtn->setEnabled(hasSelection);
    m_addSubmoduleBtn->setEnabled(hasSelection);
}

void CommitPage::onScan()
{
    const QString root = m_rootEdit->text().trimmed();
    if (root.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Scan"), QStringLiteral("Please choose a root directory first."));
        return;
    }
    setBusy(true);
    m_table->setRowCount(0);
    m_pickList->clear();
    m_repoPaths.clear();
    QMetaObject::invokeMethod(m_worker, "scan", Qt::QueuedConnection, Q_ARG(QString, root));
}

void CommitPage::onSelectAll()
{
    for (int r = 0; r < m_table->rowCount(); ++r)
        setRowChecked(r, true);
}

void CommitPage::onSelectNone()
{
    for (int r = 0; r < m_table->rowCount(); ++r)
        setRowChecked(r, false);
}

void CommitPage::onCheckSelectedFromList()
{
    const auto selectedItems = m_pickList->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Check Selected"), QStringLiteral("Select one or more items in the Quick Select list first (click, then Ctrl+Click or Shift+Click to add more)."));
        return;
    }
    for (QListWidgetItem *item : selectedItems) {
        const int kind = item->data(Qt::UserRole).toInt();
        if (kind == kPickAllMain) {
            for (int r = 0; r < m_table->rowCount(); ++r) {
                if (!m_table->item(r, 0)->data(Qt::UserRole).toBool())
                    setRowChecked(r, true);
            }
        } else {
            const QString subName = item->data(Qt::UserRole + 1).toString();
            for (int r = 0; r < m_table->rowCount(); ++r) {
                const bool isSubmodule = m_table->item(r, 0)->data(Qt::UserRole).toBool();
                if (isSubmodule && QDir(m_table->item(r, 2)->text()).dirName() == subName)
                    setRowChecked(r, true);
            }
        }
    }
}

void CommitPage::setRowChecked(int row, bool checked)
{
    if (auto *item = m_table->item(row, 0))
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
}

QStringList CommitPage::selectedPaths() const
{
    QStringList paths;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 0)->checkState() == Qt::Checked)
            paths << m_table->item(r, 2)->text();
    }
    return paths;
}

QStringList CommitPage::selectedSubmodulePaths() const
{
    QStringList paths;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 0)->checkState() == Qt::Checked && m_table->item(r, 0)->data(Qt::UserRole).toBool())
            paths << m_table->item(r, 2)->text();
    }
    return paths;
}

QStringList CommitPage::selectedMainRepoPaths() const
{
    QStringList paths;
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 0)->checkState() == Qt::Checked && !m_table->item(r, 0)->data(Qt::UserRole).toBool())
            paths << m_table->item(r, 2)->text();
    }
    return paths;
}

void CommitPage::onTableItemChanged(QTableWidgetItem *item)
{
    if (item && item->column() == 0)
        updateActionButtons();
}

int CommitPage::rowForPath(const QString &path) const
{
    for (int r = 0; r < m_table->rowCount(); ++r) {
        if (m_table->item(r, 2)->text() == path)
            return r;
    }
    return -1;
}

void CommitPage::populateTable(const QStringList &paths)
{
    m_table->setRowCount(paths.size());
    m_pickList->clear();

    auto *allMainItem = new QListWidgetItem(QStringLiteral("★ All Main Repositories"));
    allMainItem->setData(Qt::UserRole, kPickAllMain);
    m_pickList->addItem(allMainItem);

    QStringList submoduleNamesSeen;

    for (int r = 0; r < paths.size(); ++r) {
        const QString &path = paths[r];
        const QString name = QDir(path).dirName();

        // The nearest earlier path that is a proper ancestor directory is this row's immediate
        // parent; scanning backwards finds it first because of the depth-first scan order.
        int parentRow = -1;
        for (int p = r - 1; p >= 0; --p) {
            if (path.startsWith(paths[p] + QDir::separator())) {
                parentRow = p;
                break;
            }
        }
        const bool isSubmodule = parentRow >= 0;
        const QString displayName = isSubmodule ? (QStringLiteral("    ↳ ") + name) : name;

        auto *checkItem = new QTableWidgetItem;
        checkItem->setFlags(Qt::ItemIsUserCheckable | Qt::ItemIsEnabled);
        checkItem->setCheckState(Qt::Checked);
        checkItem->setData(Qt::UserRole, isSubmodule);
        m_table->setItem(r, 0, checkItem);

        m_table->setItem(r, 1, new QTableWidgetItem(displayName));
        m_table->setItem(r, 2, new QTableWidgetItem(path));
        m_table->setItem(r, 3, new QTableWidgetItem(QStringLiteral("...")));
        m_table->setItem(r, 4, new QTableWidgetItem(QStringLiteral("Pending")));

        if (isSubmodule && !submoduleNamesSeen.contains(name))
            submoduleNamesSeen << name;
    }

    // One Quick Select entry per unique submodule name, regardless of how many repos have it;
    // picking it checks every row across every repo whose submodule folder matches that name.
    for (const QString &subName : submoduleNamesSeen) {
        auto *pickItem = new QListWidgetItem(QStringLiteral("    ↳ ") + subName);
        pickItem->setData(Qt::UserRole, kPickAllWithName);
        pickItem->setData(Qt::UserRole + 1, subName);
        m_pickList->addItem(pickItem);
    }
}

void CommitPage::onScanFinished(QStringList repoPaths)
{
    m_repoPaths = repoPaths;
    populateTable(repoPaths);
    setBusy(false);

    if (repoPaths.isEmpty())
        QMessageBox::information(this, QStringLiteral("Scan Complete"), QStringLiteral("No git repositories were found under this directory."));
}

void CommitPage::onRepoBranchInfo(QString path, QString branchInfo)
{
    const int row = rowForPath(path);
    if (row < 0)
        return;
    m_table->item(row, 3)->setText(branchInfo);
    m_table->resizeRowToContents(row);
}

void CommitPage::onProgress(int done, int total)
{
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(done);
}

void CommitPage::onRepoResult(QString path, QString status, QString message)
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
        m_table->setItem(row, 4, item);
    }
}

void CommitPage::beginOperation(const QStringList &selected)
{
    m_results.clear();
    setBusy(true);
    m_progressBar->setRange(0, selected.size());
    m_progressBar->setValue(0);
    for (int r = 0; r < m_table->rowCount(); ++r)
        m_table->item(r, 4)->setText(QStringLiteral("Skipped"));
    for (const QString &path : selected) {
        const int row = rowForPath(path);
        if (row >= 0)
            m_table->item(row, 4)->setText(QStringLiteral("Working..."));
    }
}

void CommitPage::showProblemsSummary(const QString &title, const QString &okMessage)
{
    setBusy(false);

    QVector<CommitRepoRow> problems;
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
    box.setText(QStringLiteral("%1 of %2 selected items had conflicts or failures.").arg(problems.size()).arg(m_results.size()));
    box.setDetailedText(details);
    box.exec();
}

void CommitPage::onCommit()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Commit"), QStringLiteral("Please select at least one repository or submodule first."));
        return;
    }
    const QString message = m_commitMessageEdit->text().trimmed();
    if (message.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Commit"), QStringLiteral("Please enter a commit message."));
        return;
    }
    const bool stageAll = m_stageAllCheck->isChecked();

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Commit"));
    box.setText(QStringLiteral("Commit changes in %1 selected repositories with message:\n\"%2\"?").arg(selected.size()).arg(message));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Starting commit for %1 selected repositories...").arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "commitAll", Qt::QueuedConnection,
                               Q_ARG(QStringList, selected), Q_ARG(QString, message), Q_ARG(bool, stageAll));
}

void CommitPage::onCommitFinished()
{
    showProblemsSummary(QStringLiteral("Commit Finished"),
                         QStringLiteral("All repositories were committed successfully."));
}

void CommitPage::onPush()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Push"), QStringLiteral("Please select at least one repository or submodule first."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Push"));
    box.setText(QStringLiteral("Push %1 selected repositories to their remotes?").arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Starting push for %1 selected repositories...").arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "pushAll", Qt::QueuedConnection, Q_ARG(QStringList, selected));
}

void CommitPage::onPushFinished()
{
    showProblemsSummary(QStringLiteral("Push Finished"),
                         QStringLiteral("All repositories were pushed successfully."));
}

void CommitPage::onSwitchBranch()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Switch Branch"), QStringLiteral("Please select at least one repository or submodule first."));
        return;
    }
    const QString branch = m_switchBranchEdit->text().trimmed();
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

void CommitPage::onSwitchFinished()
{
    showProblemsSummary(QStringLiteral("Branch Switch Finished"),
                         QStringLiteral("All repositories were switched successfully."));
}

void CommitPage::onMergeBranch()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Merge From Branch"), QStringLiteral("Please select at least one repository or submodule first."));
        return;
    }
    const QString branch = m_mergeBranchEdit->text().trimmed();
    if (branch.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Merge From Branch"), QStringLiteral("Please enter a source branch name."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Merge"));
    box.setText(QStringLiteral("Merge branch '%1' into the current branch of %2 selected repositories?").arg(branch).arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Merging '%1' into %2 selected repositories...").arg(branch).arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "mergeFromBranch", Qt::QueuedConnection,
                               Q_ARG(QStringList, selected), Q_ARG(QString, branch));
}

void CommitPage::onMergeFinished()
{
    showProblemsSummary(QStringLiteral("Merge Finished"),
                         QStringLiteral("All repositories were merged successfully. No conflicts."));
}

void CommitPage::onInitSwitchPull()
{
    const QStringList selected = selectedPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Init Submodule + Switch + Pull"), QStringLiteral("Please select at least one repository or submodule first."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Init + Switch + Pull"));
    box.setText(QStringLiteral("For %1 selected repositories: initialize if needed, switch to 'master' (or 'main'), and pull?").arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Starting init/switch/pull for %1 selected repositories...").arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "initSwitchPull", Qt::QueuedConnection, Q_ARG(QStringList, selected));
}

void CommitPage::onInitSwitchPullFinished()
{
    showProblemsSummary(QStringLiteral("Init + Switch + Pull Finished"),
                         QStringLiteral("All repositories were initialized, switched, and pulled successfully."));
}

void CommitPage::onRemoveSubmodule()
{
    const QStringList selected = selectedSubmodulePaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Remove Submodule"), QStringLiteral("Please check at least one submodule (not a main repository) first."));
        return;
    }

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Confirm Submodule Removal"));
    box.setText(QStringLiteral("Permanently remove %1 checked submodule(s) from disk and deinitialize them?\n"
                                "The removal from the parent repository's index will be staged; commit the parent repository to finalize it.\n"
                                "This cannot be undone by this tool.").arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Removing %1 checked submodules...").arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "removeSubmodules", Qt::QueuedConnection, Q_ARG(QStringList, selected));
}

void CommitPage::onRemoveSubmoduleFinished()
{
    showProblemsSummary(QStringLiteral("Submodule Removal Finished"),
                         QStringLiteral("All checked submodules were removed successfully. Scan again to refresh the list, and commit each parent repository to finalize."));
}

void CommitPage::onAddSubmodule()
{
    const QStringList selected = selectedMainRepoPaths();
    if (selected.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Add Submodule"), QStringLiteral("Please check at least one main repository (not a submodule) first."));
        return;
    }
    const QString url = m_addSubmoduleUrlEdit->text().trimmed();
    if (url.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Add Submodule"), QStringLiteral("Please enter the submodule's git URL."));
        return;
    }
    const QString relPath = m_addSubmodulePathEdit->text().trimmed();

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Add Submodule"));
    box.setText(relPath.isEmpty()
                    ? QStringLiteral("Add submodule '%1' to %2 checked main repositories?").arg(url).arg(selected.size())
                    : QStringLiteral("Add submodule '%1' at path '%2' to %3 checked main repositories?").arg(url, relPath).arg(selected.size()));
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    beginOperation(selected);
    m_logPanel->appendInfo(QStringLiteral("Adding submodule '%1' to %2 checked main repositories...").arg(url).arg(selected.size()));
    QMetaObject::invokeMethod(m_worker, "addSubmodule", Qt::QueuedConnection,
                               Q_ARG(QStringList, selected), Q_ARG(QString, url), Q_ARG(QString, relPath));
}

void CommitPage::onAddSubmoduleFinished()
{
    showProblemsSummary(QStringLiteral("Add Submodule Finished"),
                         QStringLiteral("The submodule was added to all checked main repositories successfully. Scan again to refresh the list."));
}
