#include "samplesqlwriter.h"

#include <QDir>

namespace {

constexpr int kWriterBatchSize = 512;
constexpr int kWriterMaxQueue = 250000;

} // namespace

SampleSqlWriter::SampleSqlWriter(QObject *parent)
    : QObject(parent)
{
}

bool SampleSqlWriter::configureDatabaseSync(const QString &dbPath)
{
    const QString normalized = QDir::cleanPath(dbPath.trimmed());
    if (normalized.isEmpty()) {
        emit writerWarning(QStringLiteral("异步写入器数据库路径为空"));
        m_dbPath.clear();
        m_ready = false;
        return false;
    }

    m_dbPath = normalized;
    m_ready = false;
    m_failureStreak = 0;

    QString error;
    if (!m_logDatabase.initialize(m_dbPath, &error)) {
        emit writerWarning(QStringLiteral("异步写入器初始化失败: %1").arg(error));
        return false;
    }

    m_ready = true;
    return true;
}

void SampleSqlWriter::enqueueSamples(const QVector<SampleEntry> &samples)
{
    if (samples.isEmpty()) {
        return;
    }

    for (const SampleEntry &sample : samples) {
        m_queue.push_back(sample);
    }

    if (static_cast<int>(m_queue.size()) > kWriterMaxQueue) {
        const int dropCount = static_cast<int>(m_queue.size()) - kWriterMaxQueue;
        for (int i = 0; i < dropCount; ++i) {
            m_queue.pop_front();
        }
        emit writerWarning(QStringLiteral("异步写入队列超限，已丢弃最旧样本 %1 条").arg(dropCount));
    }

    processBatches(false);
}

bool SampleSqlWriter::flushPendingSync()
{
    processBatches(true);
    return m_queue.empty();
}

int SampleSqlWriter::discardPendingSync()
{
    const int dropped = static_cast<int>(m_queue.size());
    m_queue.clear();
    m_failureStreak = 0;
    return dropped;
}

int SampleSqlWriter::pendingCount() const
{
    return static_cast<int>(m_queue.size());
}

bool SampleSqlWriter::ensureDatabaseReady()
{
    if (m_ready && m_logDatabase.isReady()) {
        return true;
    }

    if (m_dbPath.isEmpty()) {
        return false;
    }

    QString error;
    if (!m_logDatabase.initialize(m_dbPath, &error)) {
        emit writerWarning(QStringLiteral("异步写入器重连数据库失败: %1").arg(error));
        return false;
    }

    m_ready = true;
    return true;
}

bool SampleSqlWriter::insertFrontBatch(int batchSize)
{
    if (batchSize <= 0 || m_queue.empty()) {
        return true;
    }

    if (!ensureDatabaseReady()) {
        return false;
    }

    QVector<SampleEntry> batch;
    batch.reserve(batchSize);
    for (int i = 0; i < batchSize; ++i) {
        batch.push_back(m_queue.at(i));
    }

    if (!m_logDatabase.insertSamples(batch)) {
        ++m_failureStreak;
        if (m_failureStreak == 1 || (m_failureStreak % 20) == 0) {
            emit writerWarning(QStringLiteral("异步批量写入 SQL 失败，待写队列=%1").arg(m_queue.size()));
        }
        return false;
    }

    for (int i = 0; i < batchSize; ++i) {
        m_queue.pop_front();
    }

    m_failureStreak = 0;
    return true;
}

void SampleSqlWriter::processBatches(bool forceAll)
{
    if (m_queue.empty()) {
        return;
    }

    while (!m_queue.empty()) {
        const int pending = static_cast<int>(m_queue.size());
        if (!forceAll && pending < kWriterBatchSize) {
            break;
        }

        const int batchSize = forceAll ? qMin(kWriterBatchSize, pending) : kWriterBatchSize;
        if (!insertFrontBatch(batchSize)) {
            break;
        }
    }
}
