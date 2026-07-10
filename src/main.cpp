#include "MainWindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("BatchGitCommand"));
    app.setOrganizationName(QStringLiteral("BatchGitCommand"));

    MainWindow window;
    window.show();

    return app.exec();
}
