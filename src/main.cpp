#include <QApplication>
#include <QDir>

#include "ElaApplication.h"
#include "ui/dialogs/LoginDialog.h"
#include "ui/mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    eApp->init();

    const QString dataDir = QDir::current().filePath(QStringLiteral("data"));
    const QString outputDir = QDir::current().filePath(QStringLiteral("out"));

    LoginDialog loginDialog(dataDir, outputDir);
    if (loginDialog.exec() != QDialog::Accepted || !loginDialog.isAuthenticated())
        return 0;

    MainWindow w(loginDialog.authenticatedUser(), loginDialog.dataDir(), loginDialog.outputDir());
    w.moveToCenter();
    w.show();

    return app.exec();
}
