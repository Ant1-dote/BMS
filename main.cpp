#include <QApplication>
#include <QQmlContext>
#include <QQmlApplicationEngine>

#include "ads1256controller.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

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
