#include "library/LibraryModel.h"

LibraryModel::LibraryModel(LibraryStore *store, QObject *parent)
    : QAbstractListModel(parent), m_store(store)
{
    Q_ASSERT(m_store);
    connect(m_store, &LibraryStore::entriesChanged, this,
            &LibraryModel::rebuild);
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
}
