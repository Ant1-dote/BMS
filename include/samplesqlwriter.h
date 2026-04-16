#pragma once

#include "logdatabase.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <deque>

class SampleSqlWriter : public QObject {
    Q_OBJECT

public:
    explicit SampleSqlWriter(QObject *parent = nullptr);

    Q_INVOKABLE bool configureDatabaseSync(const QString &dbPath);
    Q_INVOKABLE void enqueueSamples(const QVector<SampleEntry> &samples);
    Q_INVOKABLE bool flushPendingSync();
    Q_INVOKABLE int discardPendingSync();
    Q_INVOKABLE int pendingCount() const;

signals:
    void writerWarning(const QString &message);

private:
    bool ensureDatabaseReady();
    bool insertFrontBatch(int batchSize);
    void processBatches(bool forceAll);

private:
    LogDatabase m_logDatabase;
    QString m_dbPath;
    std::deque<SampleEntry> m_queue;
    int m_failureStreak = 0;
    bool m_ready = false;
};
