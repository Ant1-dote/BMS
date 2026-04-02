#pragma once

#include <QAbstractListModel>
#include <QString>
#include <QVector>

struct LogEntry {
    QString timestamp;
    QString level;
    QString message;
};

class LogListModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TimestampRole = Qt::UserRole + 1,
        LevelRole,
        MessageRole,
        DisplayRole,
    };

    explicit LogListModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void appendEntry(const LogEntry &entry);
    void appendEntries(const QVector<LogEntry> &entries);
    void clearEntries();

private:
    QVector<LogEntry> m_entries;
    int m_maxRows = 3000;
};
