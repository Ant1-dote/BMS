#include "logdatabase.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {

QSqlDatabase openReadOnlyDatabase(const QString &dbPath, QString *connectionName, QString *errorMessage)
{
    *connectionName = QStringLiteral("bms-read-%1")
                          .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), *connectionName);
    db.setDatabaseName(dbPath);
    if (!db.open() && errorMessage) {
        *errorMessage = db.lastError().text();
    }
    return db;
}

void closeReadOnlyDatabase(QSqlDatabase &db, const QString &connectionName)
{
    if (db.isValid() && db.isOpen()) {
        db.close();
    }
    db = {};
    QSqlDatabase::removeDatabase(connectionName);
}

} // namespace

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
    if (m_db.isValid()) {
        if (m_db.isOpen()) {
            m_db.close();
        }
        m_db = {};
        QSqlDatabase::removeDatabase(m_connectionName);
    }

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
    query.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    query.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    query.exec(QStringLiteral("PRAGMA temp_store=MEMORY"));
    query.exec(QStringLiteral("PRAGMA wal_autocheckpoint=1000"));

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

QVector<LogEntry> LogDatabase::readRecentLogsFromFile(
    const QString &dbPath,
    int limit,
    QString *errorMessage)
{
    QVector<LogEntry> result;
    if (errorMessage) {
        errorMessage->clear();
    }

    if (limit <= 0) {
        return result;
    }

    const QFileInfo info(dbPath);
    if (!info.exists() || !info.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("文件不存在或不可读: %1").arg(dbPath);
        }
        return result;
    }

    QString connectionName;
    QSqlDatabase db = openReadOnlyDatabase(dbPath, &connectionName, errorMessage);
    if (!db.isOpen()) {
        closeReadOnlyDatabase(db, connectionName);
        return result;
    }

    bool queryOk = true;
    {
        QSqlQuery query(db);
        query.prepare(QStringLiteral(
            "SELECT timestamp, level, message FROM logs ORDER BY id DESC LIMIT ?"));
        query.addBindValue(limit);
        if (!query.exec()) {
            queryOk = false;
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
        }

        if (queryOk) {
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
        }
    }

    closeReadOnlyDatabase(db, connectionName);
    if (!queryOk) {
        result.clear();
    }
    return result;
}

QVector<SampleEntry> LogDatabase::readRecentSamplesFromFile(
    const QString &dbPath,
    int limit,
    QString *errorMessage)
{
    QVector<SampleEntry> result;
    if (errorMessage) {
        errorMessage->clear();
    }

    const QFileInfo info(dbPath);
    if (!info.exists() || !info.isFile()) {
        if (errorMessage) {
            *errorMessage = QStringLiteral("文件不存在或不可读: %1").arg(dbPath);
        }
        return result;
    }

    QString connectionName;
    QSqlDatabase db = openReadOnlyDatabase(dbPath, &connectionName, errorMessage);
    if (!db.isOpen()) {
        closeReadOnlyDatabase(db, connectionName);
        return result;
    }

    bool queryOk = true;
    {
        QSqlQuery query(db);
        if (limit > 0) {
            query.prepare(QStringLiteral(
                "SELECT elapsed_ns, mode, channel, voltage, adc FROM samples ORDER BY id DESC LIMIT ?"));
            query.addBindValue(limit);
        } else {
            query.prepare(QStringLiteral(
                "SELECT elapsed_ns, mode, channel, voltage, adc FROM samples ORDER BY id DESC"));
        }
        if (!query.exec()) {
            queryOk = false;
            if (errorMessage) {
                *errorMessage = query.lastError().text();
            }
        }

        if (queryOk) {
            QVector<SampleEntry> reversed;
            while (query.next()) {
                const QString modeText = query.value(1).toString().trimmed().toUpper();
                const int channel = qBound(0, query.value(2).toInt(), 7);

                reversed.push_back(SampleEntry {
                    query.value(0).toLongLong(),
                    (modeText == QStringLiteral("SCAN8")) ? SampleMode::Scan8 : SampleMode::Single,
                    static_cast<quint8>(channel),
                    static_cast<float>(query.value(3).toDouble()),
                    static_cast<qint32>(query.value(4).toInt()),
                });
            }

            result.reserve(reversed.size());
            for (int i = reversed.size() - 1; i >= 0; --i) {
                result.push_back(reversed.at(i));
            }
        }
    }

    closeReadOnlyDatabase(db, connectionName);
    if (!queryOk) {
        result.clear();
    }
    return result;
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

    ++m_logInsertCounter;
    if (m_logInsertCounter % 100 == 0) {
        QSqlQuery pruneQuery(m_db);
        pruneQuery.prepare(QStringLiteral(
            "DELETE FROM logs "
            "WHERE id <= (SELECT COALESCE(MAX(id) - ?, 0) FROM logs)"));
        pruneQuery.addBindValue(5000);
        pruneQuery.exec();
    }
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

bool LogDatabase::insertSamples(const QVector<SampleEntry> &samples)
{
    if (!isReady()) {
        return false;
    }

    if (samples.isEmpty()) {
        return true;
    }

    if (!m_db.transaction()) {
        return false;
    }

    QSqlQuery query(m_db);
    query.prepare(QStringLiteral(
        "INSERT INTO samples(elapsed_ns, mode, channel, voltage, adc, hex) VALUES(?, ?, ?, ?, ?, ?)"));

    bool ok = true;
    for (const SampleEntry &sample : samples) {
        const QString modeText = (sample.mode == SampleMode::Scan8)
            ? QStringLiteral("SCAN8")
            : QStringLiteral("SINGLE");
        const QString hexValue = QStringLiteral("0x%1")
                                     .arg(sample.adc & 0x00FFFFFF, 6, 16, QLatin1Char('0'))
                                     .toUpper();

        query.addBindValue(sample.elapsedNs);
        query.addBindValue(modeText);
        query.addBindValue(static_cast<int>(sample.channel));
        query.addBindValue(static_cast<double>(sample.voltage));
        query.addBindValue(sample.adc);
        query.addBindValue(hexValue);
        if (!query.exec()) {
            ok = false;
            break;
        }
    }

    if (!ok) {
        m_db.rollback();
        return false;
    }

    return m_db.commit();
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
