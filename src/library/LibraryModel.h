#pragma once

#include <QAbstractListModel>

#include "library/LibraryStore.h"

class ThumbnailService;

// QAbstractListModel over the LibraryStore's entries, exposed to QML as the
// context property |libraryModel|. Adds two things on top of the raw list:
// a case-insensitive substring filter (the drawer's search box) applied to
// title and path, and the P6 v2 "thumbnail" role (cache path of the entry's
// ffmpeg-generated JPEG, empty when none — the drawer falls back to the
// type icon placeholder).
//
// Thumbnails are requested on demand: whenever the visible list is rebuilt,
// every visible path is handed to the ThumbnailService (which dedupes,
// serves cache hits and serializes generation in the background). Ready
// thumbnails are mapped back to their rows via dataChanged so QML delegates
// reload without a full reset.
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
        ThumbnailRole,
    };

    // |thumbnails| is optional: null keeps the thumbnail role permanently
    // empty (and is what older call sites / minimal setups pass).
    explicit LibraryModel(LibraryStore *store,
                          ThumbnailService *thumbnails = nullptr,
                          QObject *parent = nullptr);

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

private slots:
    void onThumbnailReady(const QString &path, const QString &cacheFile);
    void requestVisibleThumbnails();

private:
    void rebuild();

    LibraryStore *m_store = nullptr;
    ThumbnailService *m_thumbnails = nullptr;
    QList<LibraryEntry> m_visible;
    QString m_filter;
};
