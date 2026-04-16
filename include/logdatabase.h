#pragma once

#include "loglistmodel.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QMetaType>
#include <QtGlobal>
#include <QVector>

enum class SampleMode : qint8 {
    Single = 0,
    Scan8 = 1,
};

struct SampleEntry {
    qint64 elapsedNs = 0;
    SampleMode mode = SampleMode::Single;
    quint8 channel = 0;
    float voltage = 0.0f;
    qint32 adc = 0;
};

Q_DECLARE_METATYPE(SampleEntry)
Q_DECLARE_METATYPE(QVector<SampleEntry>)

class LogDatabase : public QObject {
    Q_OBJECT

public:
    explicit LogDatabase(QObject *parent = nullptr);
    ~LogDatabase() override;

    bool initialize(const QString &dbPath, QString *errorMessage = nullptr);
    bool isReady() const;

    static QVector<LogEntry> readRecentLogsFromFile(
        const QString &dbPath,
        int limit,
        QString *errorMessage = nullptr);
    static QVector<SampleEntry> readRecentSamplesFromFile(
        const QString &dbPath,
        int limit,
        QString *errorMessage = nullptr);

    void insertLog(const QString &level, const QString &message);
    void insertSample(
        qint64 elapsedNs,
        const QString &mode,
        int channel,
        double voltage,
        int adc,
        const QString &hexValue);

    QVector<LogEntry> recentLogs(int limit) const;
    bool insertSamples(const QVector<SampleEntry> &samples);
    void clearLogs();

private:
    QString m_connectionName;
    QSqlDatabase m_db;
    int m_logInsertCounter = 0;
};
