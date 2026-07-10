#include "MainWindow.h"
#include "GeneratePage.h"
#include "ClonePage.h"
#include "SyncPage.h"
#include "LogPanel.h"

#include <QTabWidget>
#include <QSplitter>
#include <QLabel>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Batch Git Command"));
    resize(1000, 750);

    auto *logPanel = new LogPanel(this);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(new GeneratePage(logPanel, tabs), QStringLiteral("Generate Clone JSON"));
    tabs->addTab(new ClonePage(logPanel, tabs), QStringLiteral("Batch Clone"));
    tabs->addTab(new SyncPage(logPanel, tabs), QStringLiteral("Batch Revert / Pull / Switch"));

    auto *splitter = new QSplitter(Qt::Vertical, this);
    splitter->addWidget(tabs);

    auto *logContainer = new QWidget(this);
    auto *logLayout = new QVBoxLayout(logContainer);
    logLayout->setContentsMargins(4, 4, 4, 4);
    logLayout->addWidget(new QLabel(QStringLiteral("Log"), logContainer));
    logLayout->addWidget(logPanel);
    splitter->addWidget(logContainer);

    splitter->setStretchFactor(0, 3);
    splitter->setStretchFactor(1, 2);

    setCentralWidget(splitter);
}
