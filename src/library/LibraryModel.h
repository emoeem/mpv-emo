#pragma once

#include <QAbstractListModel>

#include "library/LibraryStore.h"

// QAbstractListModel over the LibraryStore's entries, exposed to QML as the
// context property |libraryModel|. Adds one thing on top of the raw list: a
// case-insensitive substring filter (the drawer's search box) applied to
// title and path.
//
// The model re-reads the whole filtered list whenever the store reports
// entriesChanged or the filter changes (beginResetModel/endResetModel) —
// library sizes make incremental diffing pointless in v1.
class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    // Rows matching the current filter.
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    // Case-insensitive substring filter over title and path; empty = all.
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
    enum Role
    {
        PathRole = Qt::UserRole + 1,
        TitleRole,
        TypeRole,
        SizeRole,
        LastPlayedRole,
    };

    explicit LibraryModel(LibraryStore *store, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index,
                  int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const { return m_visible.size(); }
    QString filter() const { return m_filter; }

public slots:
    void setFilter(const QString &filter);

signals:
    void countChanged();
    void filterChanged();

private:
    void rebuild();

    LibraryStore *m_store = nullptr;
    QList<LibraryEntry> m_visible;
    QString m_filter;
};
