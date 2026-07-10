#include "ClonePage.h"
#include "CloneWorker.h"
#include "LogPanel.h"

#include <QListWidget>
#include <QLineEdit>
#include <QPushButton>
#include <QCheckBox>
#include <QProgressBar>
#include <QThread>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QMetaObject>

namespace {

QStringList parseRepoJson(const QByteArray &data, QString *error)
{
    QStringList urls;
    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(data, &parseError);
    if (doc.isNull()) {
        if (error)
            *error = parseError.errorString();
        return urls;
    }

    QJsonArray arr;
    if (doc.isArray()) {
        arr = doc.array();
    } else if (doc.isObject() && doc.object().contains(QStringLiteral("repos")) && doc.object().value(QStringLiteral("repos")).isArray()) {
        arr = doc.object().value(QStringLiteral("repos")).toArray();
    } else {
        if (error)
            *error = QStringLiteral("Expected a JSON array of URLs, or an object with a \"repos\" array.");
        return urls;
    }

    for (const QJsonValue &val : arr) {
        if (val.isString()) {
            urls << val.toString();
        } else if (val.isObject() && val.toObject().contains(QStringLiteral("url"))) {
            urls << val.toObject().value(QStringLiteral("url")).toString();
        }
    }
    return urls;
}

} // namespace

ClonePage::ClonePage(LogPanel *logPanel, QWidget *parent)
    : QWidget(parent), m_logPanel(logPanel)
{
    m_listWidget = new QListWidget(this);

    m_urlEdit = new QLineEdit(this);
    m_urlEdit->setPlaceholderText(QStringLiteral("https://github.com/user/repo.git"));
    m_addBtn = new QPushButton(QStringLiteral("Add"), this);
    m_loadJsonBtn = new QPushButton(QStringLiteral("Load from JSON..."), this);
    m_removeBtn = new QPushButton(QStringLiteral("Remove Selected"), this);
    m_clearBtn = new QPushButton(QStringLiteral("Clear All"), this);

    m_destEdit = new QLineEdit(this);
    m_browseBtn = new QPushButton(QStringLiteral("Browse..."), this);

    m_submodulesCheck = new QCheckBox(QStringLiteral("Update submodules and switch them to main/master too"), this);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setRange(0, 1);
    m_progressBar->setValue(0);
    m_progressBar->setTextVisible(true);

    m_cloneBtn = new QPushButton(QStringLiteral("Batch Clone"), this);
    m_cloneBtn->setMinimumHeight(32);

    auto *addRow = new QHBoxLayout;
    addRow->addWidget(m_urlEdit);
    addRow->addWidget(m_addBtn);
    addRow->addWidget(m_loadJsonBtn);

    auto *listButtonsRow = new QHBoxLayout;
    listButtonsRow->addWidget(m_removeBtn);
    listButtonsRow->addWidget(m_clearBtn);
    listButtonsRow->addStretch();

    auto *listGroup = new QGroupBox(QStringLiteral("Repository URLs"), this);
    auto *listLayout = new QVBoxLayout(listGroup);
    listLayout->addLayout(addRow);
    listLayout->addWidget(m_listWidget);
    listLayout->addLayout(listButtonsRow);

    auto *destRow = new QHBoxLayout;
    destRow->addWidget(m_destEdit);
    destRow->addWidget(m_browseBtn);

    auto *destGroup = new QGroupBox(QStringLiteral("Destination Directory"), this);
    auto *destLayout = new QVBoxLayout(destGroup);
    destLayout->addLayout(destRow);

    auto *mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(listGroup);
    mainLayout->addWidget(destGroup);
    mainLayout->addWidget(m_submodulesCheck);
    mainLayout->addWidget(m_progressBar);
    mainLayout->addWidget(m_cloneBtn);

    connect(m_addBtn, &QPushButton::clicked, this, &ClonePage::onAddUrl);
    connect(m_urlEdit, &QLineEdit::returnPressed, this, &ClonePage::onAddUrl);
    connect(m_loadJsonBtn, &QPushButton::clicked, this, &ClonePage::onLoadJson);
    connect(m_removeBtn, &QPushButton::clicked, this, &ClonePage::onRemoveSelected);
    connect(m_clearBtn, &QPushButton::clicked, this, &ClonePage::onClearAll);
    connect(m_browseBtn, &QPushButton::clicked, this, &ClonePage::onBrowseDest);
    connect(m_cloneBtn, &QPushButton::clicked, this, &ClonePage::onBatchClone);

    m_thread = new QThread(this);
    m_worker = new CloneWorker();
    m_worker->moveToThread(m_thread);
    connect(m_thread, &QThread::finished, m_worker, &QObject::deleteLater);

    connect(m_worker, &CloneWorker::logLine, m_logPanel, &LogPanel::appendRaw);
    connect(m_worker, &CloneWorker::logInfo, m_logPanel, &LogPanel::appendInfo);
    connect(m_worker, &CloneWorker::logWarning, m_logPanel, &LogPanel::appendWarning);
    connect(m_worker, &CloneWorker::logError, m_logPanel, &LogPanel::appendError);
    connect(m_worker, &CloneWorker::itemDone, this, &ClonePage::onItemDone);
    connect(m_worker, &CloneWorker::progress, this, &ClonePage::onProgress);
    connect(m_worker, &CloneWorker::finished, this, &ClonePage::onFinished);

    m_thread->start();
}

ClonePage::~ClonePage()
{
    m_thread->quit();
    m_thread->wait();
}

QStringList ClonePage::collectUrls() const
{
    QStringList urls;
    for (int i = 0; i < m_listWidget->count(); ++i)
        urls << m_listWidget->item(i)->text();
    return urls;
}

void ClonePage::onLoadJson()
{
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Select repository list (JSON)"), QString(), QStringLiteral("JSON Files (*.json);;All Files (*)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, QStringLiteral("Load JSON"), QStringLiteral("Could not open file: ") + path);
        return;
    }
    const QByteArray data = file.readAll();
    QString error;
    const QStringList urls = parseRepoJson(data, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Load JSON"), QStringLiteral("Failed to parse JSON: ") + error);
        return;
    }
    if (urls.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Load JSON"), QStringLiteral("No repository URLs were found in this file."));
        return;
    }

    int added = 0;
    QStringList existing = collectUrls();
    for (const QString &url : urls) {
        const QString trimmed = url.trimmed();
        if (trimmed.isEmpty() || existing.contains(trimmed))
            continue;
        m_listWidget->addItem(trimmed);
        existing << trimmed;
        ++added;
    }
    m_logPanel->appendInfo(QStringLiteral("Loaded %1 new repository URL(s) from %2").arg(added).arg(path));
}

void ClonePage::onAddUrl()
{
    const QString url = m_urlEdit->text().trimmed();
    if (url.isEmpty())
        return;
    if (collectUrls().contains(url)) {
        QMessageBox::information(this, QStringLiteral("Add Repository"), QStringLiteral("This URL is already in the list."));
        return;
    }
    m_listWidget->addItem(url);
    m_urlEdit->clear();
}

void ClonePage::onRemoveSelected()
{
    for (QListWidgetItem *item : m_listWidget->selectedItems())
        delete m_listWidget->takeItem(m_listWidget->row(item));
}

void ClonePage::onClearAll()
{
    m_listWidget->clear();
}

void ClonePage::onBrowseDest()
{
    const QString dir = QFileDialog::getExistingDirectory(this, QStringLiteral("Select Destination Directory"));
    if (!dir.isEmpty())
        m_destEdit->setText(dir);
}

void ClonePage::setBusy(bool busy)
{
    m_loadJsonBtn->setEnabled(!busy);
    m_addBtn->setEnabled(!busy);
    m_removeBtn->setEnabled(!busy);
    m_clearBtn->setEnabled(!busy);
    m_browseBtn->setEnabled(!busy);
    m_cloneBtn->setEnabled(!busy);
    m_destEdit->setEnabled(!busy);
    m_urlEdit->setEnabled(!busy);
    m_listWidget->setEnabled(!busy);
    m_submodulesCheck->setEnabled(!busy);
}

void ClonePage::onBatchClone()
{
    const QStringList urls = collectUrls();
    if (urls.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Batch Clone"), QStringLiteral("Add at least one repository URL first."));
        return;
    }
    const QString destDir = m_destEdit->text().trimmed();
    if (destDir.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Batch Clone"), QStringLiteral("Please choose a destination directory first."));
        return;
    }

    const bool updateSubmodules = m_submodulesCheck->isChecked();

    QString preview;
    const int previewCount = qMin(urls.size(), 15);
    for (int i = 0; i < previewCount; ++i)
        preview += urls[i] + QStringLiteral("\n");
    if (urls.size() > previewCount)
        preview += QStringLiteral("...and %1 more").arg(urls.size() - previewCount);

    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(QStringLiteral("Confirm Batch Clone"));
    box.setText(QStringLiteral("This will clone %1 repositories into:\n%2\n\nContinue?").arg(urls.size()).arg(destDir));
    box.setDetailedText(preview);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    if (box.exec() != QMessageBox::Yes)
        return;

    m_failures.clear();
    m_successCount = 0;
    m_totalCount = urls.size();
    m_progressBar->setRange(0, m_totalCount);
    m_progressBar->setValue(0);

    setBusy(true);
    m_logPanel->appendInfo(QStringLiteral("Starting batch clone of %1 repositories...").arg(urls.size()));

    QMetaObject::invokeMethod(m_worker, "run", Qt::QueuedConnection,
                               Q_ARG(QStringList, urls), Q_ARG(QString, destDir), Q_ARG(bool, updateSubmodules));
}

void ClonePage::onItemDone(QString url, bool success, QString message)
{
    if (success)
        ++m_successCount;
    else
        m_failures.append(qMakePair(url, message));
}

void ClonePage::onProgress(int done, int total)
{
    m_progressBar->setRange(0, total);
    m_progressBar->setValue(done);
}

void ClonePage::onFinished()
{
    setBusy(false);

    if (m_failures.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Batch Clone Complete"),
                                  QStringLiteral("All %1 repositories were cloned successfully.").arg(m_successCount));
        return;
    }

    QString details;
    for (const auto &pair : m_failures)
        details += pair.first + QStringLiteral("\n    ") + pair.second + QStringLiteral("\n");

    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(QStringLiteral("Batch Clone Finished with Errors"));
    box.setText(QStringLiteral("Cloned %1 of %2 repositories successfully.\n%3 failed.")
                    .arg(m_successCount).arg(m_totalCount).arg(m_failures.size()));
    box.setDetailedText(details);
    box.setStandardButtons(QMessageBox::Ok);
    box.exec();
}
