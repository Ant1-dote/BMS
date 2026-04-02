#pragma once

#include "loglistmodel.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

class LogDatabase : public QObject {
    Q_OBJECT

public:
    explicit LogDatabase(QObject *parent = nullptr);
    ~LogDatabase() override;

    bool initialize(const QString &dbPath, QString *errorMessage = nullptr);
    bool isReady() const;

    void insertLog(const QString &level, const QString &message);
    void insertSample(
        qint64 elapsedNs,
        const QString &mode,
        int channel,
        double voltage,
        int adc,
        const QString &hexValue);

    QVector<LogEntry> recentLogs(int limit) const;
    void clearLogs();

private:
    QString m_connectionName;
    QSqlDatabase m_db;
};
