#include "crashlogger.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

#include <array>
#include <atomic>
#include <csignal>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <exception>

namespace {

QMutex s_mutex;
QString s_logPath;
QtMessageHandler s_prevMessageHandler = nullptr;
std::terminate_handler s_prevTerminateHandler = nullptr;
std::atomic<bool> s_handlersInstalled = false;
std::array<char, 1024> s_signalLogPath {};
thread_local bool s_inQtMessageHandler = false;

QString timePrefix()
{
    return QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"));
}

void updateSignalLogPathLocked(const QString &path)
{
    const QByteArray utf8 = QFileInfo(path).absoluteFilePath().toLocal8Bit();
    const int copyLen = qMin(static_cast<int>(s_signalLogPath.size()) - 1, utf8.size());
    std::fill(s_signalLogPath.begin(), s_signalLogPath.end(), '\0');
    if (copyLen > 0) {
        std::memcpy(s_signalLogPath.data(), utf8.constData(), copyLen);
        s_signalLogPath[copyLen] = '\0';
    }
}

void appendLineLocked(const QString &line)
{
    if (s_logPath.isEmpty()) {
        return;
    }

    QFile file(s_logPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }

    QTextStream out(&file);
    out << line << Qt::endl;
}

void appendLine(const QString &line)
{
    QMutexLocker locker(&s_mutex);
    appendLineLocked(line);
}

void signalHandler(int signalCode)
{
    const char *signalName = "UNKNOWN";
    switch (signalCode) {
    case SIGABRT:
        signalName = "SIGABRT";
        break;
    case SIGSEGV:
        signalName = "SIGSEGV";
        break;
    case SIGILL:
        signalName = "SIGILL";
        break;
    case SIGFPE:
        signalName = "SIGFPE";
        break;
    case SIGTERM:
        signalName = "SIGTERM";
        break;
    default:
        break;
    }

    // Best-effort signal logging. Avoid Qt APIs here to reduce secondary crash risk.
    if (s_signalLogPath[0] != '\0') {
        if (FILE *fp = std::fopen(s_signalLogPath.data(), "a")) {
            std::fprintf(fp, "[CRASH] Signal received: %s (%d)\n", signalName, signalCode);
            std::fclose(fp);
        }
    }

    std::signal(signalCode, SIG_DFL);
    std::raise(signalCode);
}

void terminateHandler()
{
    QString reason = QStringLiteral("std::terminate called");
    if (const std::exception_ptr exPtr = std::current_exception()) {
        try {
            std::rethrow_exception(exPtr);
        } catch (const std::exception &ex) {
            reason += QStringLiteral(": %1").arg(QString::fromLocal8Bit(ex.what()));
        } catch (...) {
            reason += QStringLiteral(": unknown exception");
        }
    }

    appendLine(timePrefix() + QStringLiteral(" [CRASH] ") + reason);

    if (s_prevTerminateHandler && s_prevTerminateHandler != terminateHandler) {
        s_prevTerminateHandler();
    }
    std::_Exit(EXIT_FAILURE);
}

void qtMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    if (s_inQtMessageHandler) {
        if (s_prevMessageHandler) {
            s_prevMessageHandler(type, context, msg);
        }
        if (type == QtFatalMsg) {
            std::abort();
        }
        return;
    }

    s_inQtMessageHandler = true;

    const QString level = [&]() {
        switch (type) {
        case QtDebugMsg:
            return QStringLiteral("DEBUG");
        case QtInfoMsg:
            return QStringLiteral("INFO");
        case QtWarningMsg:
            return QStringLiteral("WARN");
        case QtCriticalMsg:
            return QStringLiteral("CRITICAL");
        case QtFatalMsg:
            return QStringLiteral("FATAL");
        }
        return QStringLiteral("UNKNOWN");
    }();

    QString detail = msg;
    if (context.file && context.line > 0) {
        detail += QStringLiteral(" | %1:%2").arg(QString::fromLocal8Bit(context.file)).arg(context.line);
    }
    if (context.function) {
        detail += QStringLiteral(" | %1").arg(QString::fromLocal8Bit(context.function));
    }

    if (type == QtCriticalMsg || type == QtFatalMsg) {
        appendLine(timePrefix() + QStringLiteral(" [") + level + QStringLiteral("] ") + detail);
    }

    if (s_prevMessageHandler) {
        s_prevMessageHandler(type, context, msg);
    }

    s_inQtMessageHandler = false;

    if (type == QtFatalMsg) {
        std::abort();
    }
}

void installHandlers()
{
    bool expected = false;
    if (!s_handlersInstalled.compare_exchange_strong(expected, true)) {
        return;
    }

    s_prevMessageHandler = qInstallMessageHandler(qtMessageHandler);
    s_prevTerminateHandler = std::set_terminate(terminateHandler);

    std::signal(SIGABRT, signalHandler);
    std::signal(SIGSEGV, signalHandler);
    std::signal(SIGILL, signalHandler);
    std::signal(SIGFPE, signalHandler);
    std::signal(SIGTERM, signalHandler);
}

} // namespace

namespace CrashLogger {

void initialize(const QString &defaultDirectory)
{
    setLogDirectory(defaultDirectory);
    installHandlers();
}

void setLogDirectory(const QString &directory)
{
    QString dirPath = QDir::cleanPath(directory.trimmed());
    if (dirPath.isEmpty()) {
        dirPath = QDir::currentPath();
    }

    QDir dir(dirPath);
    if (!dir.exists()) {
        dir.mkpath(QStringLiteral("."));
    }

    const QString path = dir.filePath(QStringLiteral("error.log"));

    QMutexLocker locker(&s_mutex);
    s_logPath = path;
    updateSignalLogPathLocked(path);
}

QString logFilePath()
{
    QMutexLocker locker(&s_mutex);
    return s_logPath;
}

void log(const QString &message)
{
    appendLine(timePrefix() + QStringLiteral(" [MANUAL] ") + message);
}

} // namespace CrashLogger
