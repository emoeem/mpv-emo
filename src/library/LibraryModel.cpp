#include "library/LibraryModel.h"

#include "library/ThumbnailService.h"

LibraryModel::LibraryModel(LibraryStore *store, ThumbnailService *thumbnails,
                           QObject *parent)
    : QAbstractListModel(parent), m_store(store), m_thumbnails(thumbnails)
{
    Q_ASSERT(m_store);
    connect(m_store, &LibraryStore::entriesChanged, this,
            &LibraryModel::rebuild);
    if (m_thumbnails) {
        connect(m_thumbnails, &ThumbnailService::thumbnailReady, this,
                &LibraryModel::onThumbnailReady);
        // Requests made while ffmpeg was still probing (or while it was
        // missing) are dropped by the service; when availability changes we
        // ask again so late-probed / late-installed ffmpeg still fills the
        // visible grid. Unavailable services drop them again (degradation).
        connect(m_thumbnails, &ThumbnailService::availabilityChanged, this,
                &LibraryModel::requestVisibleThumbnails);
    }
    rebuild();
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_visible.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0
        || index.row() >= m_visible.size())
        return {};

    const LibraryEntry &entry = m_visible.at(index.row());
    switch (role) {
    case Qt::DisplayRole:
        return entry.title;
    case PathRole:
        return entry.path;
    case TitleRole:
        return entry.title;
    case TypeRole:
        return entry.type;
    case SizeRole:
        return entry.size;
    case LastPlayedRole:
        return entry.lastPlayed;
    case ThumbnailRole:
        return m_thumbnails ? m_thumbnails->cachedThumbnailPath(entry.path)
                            : QString();
    default:
        return {};
    }
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    auto roles = QAbstractListModel::roleNames();
    roles.insert(PathRole, "path");
    roles.insert(TitleRole, "title");
    roles.insert(TypeRole, "type");
    roles.insert(SizeRole, "size");
    roles.insert(LastPlayedRole, "lastPlayed");
    roles.insert(ThumbnailRole, "thumbnail");
    return roles;
}

void LibraryModel::setFilter(const QString &filter)
{
    if (m_filter == filter)
        return;
    m_filter = filter;
    emit filterChanged();
    rebuild();
}

void LibraryModel::rebuild()
{
    beginResetModel();

    const int previousCount = m_visible.size();
    m_visible.clear();
    const QString needle = m_filter.trimmed();
    for (const LibraryEntry &entry : m_store->entryList()) {
        if (!needle.isEmpty()
            && !entry.title.contains(needle, Qt::CaseInsensitive)
            && !entry.path.contains(needle, Qt::CaseInsensitive))
            continue;
        m_visible.append(entry);
    }

    endResetModel();
    if (m_visible.size() != previousCount)
        emit countChanged();

    // On-demand thumbnails for the freshly (re)built visible list. The
    // service dedupes, serves cache hits synchronously (thumbnailReady ->
    // onThumbnailReady fires within this loop) and queues the rest.
    requestVisibleThumbnails();
}

void LibraryModel::onThumbnailReady(const QString &path,
                                    const QString &cacheFile)
{
    Q_UNUSED(cacheFile);
    for (int row = 0; row < m_visible.size(); ++row) {
        if (m_visible.at(row).path != path)
            continue;
        const QModelIndex rowIndex = index(row, 0);
        emit dataChanged(rowIndex, rowIndex, {ThumbnailRole});
    }
}

void LibraryModel::requestVisibleThumbnails()
{
    if (!m_thumbnails)
        return;
    for (const LibraryEntry &entry : m_visible)
        m_thumbnails->requestThumbnail(entry.path);
}
