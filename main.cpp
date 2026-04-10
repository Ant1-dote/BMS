#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QIcon>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QStandardPaths>

#include "ads1256controller.h"
#include "crashlogger.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QString root = QStandardPaths::writableLocation(QStandardPaths::DesktopLocation);
    if (root.isEmpty()) {
        root = QDir::currentPath();
    }
    CrashLogger::initialize(QDir(root).filePath(QStringLiteral("BMS_Data")));

    // 优先使用圆角 ico 图标；如果缺失再回退到原始图片。
    QIcon appIcon(QStringLiteral(":/icons/bms_app.ico"));
    if (appIcon.isNull()) {
        const QString diskIconPath = QCoreApplication::applicationDirPath()
            + QStringLiteral("/bms_app.ico");
        if (QFileInfo::exists(diskIconPath)) {
            appIcon = QIcon(diskIconPath);
        }
    }
    if (appIcon.isNull()) {
        appIcon = QIcon(QStringLiteral(":/icons/source_icon.jpg"));
    }
    if (appIcon.isNull()) {
        const QString diskImagePath = QCoreApplication::applicationDirPath()
            + QStringLiteral("/source_icon.jpg");
        if (QFileInfo::exists(diskImagePath)) {
            appIcon = QIcon(diskImagePath);
        }
    }
    if (!appIcon.isNull()) {
        app.setWindowIcon(appIcon);
    }

    ADS1256Controller controller;

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("controller", &controller);
    QObject::connect(
        &engine,
        &QQmlApplicationEngine::objectCreationFailed,
        &app,
        []() { QCoreApplication::exit(-1); },
        Qt::QueuedConnection);
    engine.loadFromModule("BMS", "Main");

    return QCoreApplication::exec();
}
