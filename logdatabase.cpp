#include "logdatabase.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

LogDatabase::LogDatabase(QObject *parent)
    : QObject(parent)
    , m_connectionName(QStringLiteral("bms-log-%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces)))
{
}

LogDatabase::~LogDatabase()
{
    if (m_db.isValid()) {
        m_db.close();
    }
    m_db = {};
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool LogDatabase::initialize(const QString &dbPath, QString *errorMessage)
{
    QFileInfo info(dbPath);
    QDir dir = info.dir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("无法创建日志目录: %1").arg(dir.path());
        }
        return false;
    }

    m_db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), m_connectionName);
    m_db.setDatabaseName(dbPath);
    if (!m_db.open()) {
        if (errorMessage) {
            *errorMessage = m_db.lastError().text();
        }
        return false;
    }

    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS logs ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "timestamp TEXT NOT NULL,"
            "level TEXT NOT NULL,"
            "message TEXT NOT NULL)"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    if (!query.exec(QStringLiteral(
            "CREATE TABLE IF NOT EXISTS samples ("
            "id INTEGER PRIMARY KEY AUTOINCREMENT,"
            "elapsed_ns INTEGER NOT NULL,"
            "mode TEXT NOT NULL,"
            "channel INTEGER NOT NULL,"
            "voltage REAL NOT NULL,"
            "adc INTEGER NOT NULL,"
            "hex TEXT NOT NULL)"))) {
        if (errorMessage) {
            *errorMessage = query.lastError().text();
        }
        return false;
    }

    return true;
}

bool LogDatabase::isReady() const
{
    return m_db.isValid() && m_db.isOpen();
}

void LogDatabase::insertLog(const QString &level, const QString &message)
{
    if (!isReady()) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO logs(timestamp, level, message) VALUES(?, ?, ?)"));
    query.addBindValue(QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss")));
    query.addBindValue(level);
    query.addBindValue(message);
    query.exec();
}

void LogDatabase::insertSample(
    qint64 elapsedNs,
    const QString &mode,
    int channel,
    double voltage,
    int adc,
    const QString &hexValue)
{
    if (!isReady()) {
        return;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO samples(elapsed_ns, mode, channel, voltage, adc, hex) VALUES(?, ?, ?, ?, ?, ?)"));
    query.addBindValue(elapsedNs);
    query.addBindValue(mode);
    query.addBindValue(channel);
    query.addBindValue(voltage);
    query.addBindValue(adc);
    query.addBindValue(hexValue);
    query.exec();
}

QVector<LogEntry> LogDatabase::recentLogs(int limit) const
{
    QVector<LogEntry> result;
    if (!isReady() || limit <= 0) {
        return result;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "SELECT timestamp, level, message FROM logs ORDER BY id DESC LIMIT ?"));
    query.addBindValue(limit);
    if (!query.exec()) {
        return result;
    }

    QVector<LogEntry> reversed;
    while (query.next()) {
        reversed.push_back(LogEntry {
            query.value(0).toString(),
            query.value(1).toString(),
            query.value(2).toString(),
        });
    }

    result.reserve(reversed.size());
    for (int i = reversed.size() - 1; i >= 0; --i) {
        result.push_back(reversed.at(i));
    }
    return result;
}

void LogDatabase::clearLogs()
{
    if (!isReady()) {
        return;
    }

    QSqlQuery query(m_db);
    query.exec(QStringLiteral("DELETE FROM logs"));
}
