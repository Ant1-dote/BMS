#include "loglistmodel.h"

LogListModel::LogListModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

int LogListModel::rowCount(const QModelIndex &parent) const
{
    if (parent.isValid()) {
        return 0;
    }
    return m_entries.size();
}

QVariant LogListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_entries.size()) {
        return {};
    }

    const LogEntry &entry = m_entries.at(index.row());
    switch (role) {
    case TimestampRole:
        return entry.timestamp;
    case LevelRole:
        return entry.level;
    case MessageRole:
        return entry.message;
    case DisplayRole:
    case Qt::DisplayRole:
        return QStringLiteral("[%1] %2").arg(entry.timestamp, entry.message);
    default:
        return {};
    }
}

QHash<int, QByteArray> LogListModel::roleNames() const
{
    return {
        { TimestampRole, "timestamp" },
        { LevelRole, "level" },
        { MessageRole, "message" },
        { DisplayRole, "display" },
    };
}

void LogListModel::appendEntry(const LogEntry &entry)
{
    if (m_entries.size() >= m_maxRows) {
        beginRemoveRows({}, 0, 0);
        m_entries.removeAt(0);
        endRemoveRows();
    }

    const int row = m_entries.size();
    beginInsertRows({}, row, row);
    m_entries.push_back(entry);
    endInsertRows();
}

void LogListModel::appendEntries(const QVector<LogEntry> &entries)
{
    for (const LogEntry &entry : entries) {
        appendEntry(entry);
    }
}

void LogListModel::clearEntries()
{
    if (m_entries.isEmpty()) {
        return;
    }

    beginResetModel();
    m_entries.clear();
    endResetModel();
}
