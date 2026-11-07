#include "library/LibraryStore.h"

#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QSaveFile>
#include <QSet>
#include <QStandardPaths>
#include <QUrl>

#include <algorithm>

namespace {

// Fixed classification table for P6 v1 (no content sniffing, no MIME lookup):
// a file is media when its lower-cased extension is listed here. The list is
// the Main.qml FileDialog filter set plus a few common extras.
const QStringList &videoSuffixes()
{
    static const QStringList suffixes = {
        QStringLiteral("mkv"), QStringLiteral("mp4"),
        QStringLiteral("m4v"), QStringLiteral("avi"),
        QStringLiteral("mov"), QStringLiteral("webm"),
        QStringLiteral("ts"),  QStringLiteral("m2ts"),
        QStringLiteral("mpg"), QStringLiteral("mpeg"),
        QStringLiteral("wmv"), QStringLiteral("ogv"),
        QStringLiteral("flv"), QStringLiteral("vob")};
    return suffixes;
}

const QStringList &audioSuffixes()
{
    static const QStringList suffixes = {
        QStringLiteral("mp3"),  QStringLiteral("flac"),
        QStringLiteral("m4a"),  QStringLiteral("ogg"),
        QStringLiteral("opus"), QStringLiteral("wav"),
        QStringLiteral("aac"),  QStringLiteral("mka"),
        QStringLiteral("ape"),  QStringLiteral("wma"),
        QStringLiteral("aiff")};
    return suffixes;
}

// Resolve a path or file:// URL (what the QML dialogs hand over) to a
// cleaned absolute local path.
QString toLocalPath(const QString &pathOrUrl)
{
    const QUrl url(pathOrUrl);
    const QString local = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    if (local.isEmpty())
        return {};
    return QDir::cleanPath(QFileInfo(local).absoluteFilePath());
}

} // namespace

LibraryStore::LibraryStore(const QString &filePath, QObject *parent)
    : QObject(parent), m_filePath(filePath)
{
    load();
}

LibraryStore::~LibraryStore() = default;

QString LibraryStore::defaultFilePath()
{
    return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
           + QStringLiteral("/library.json");
}

bool LibraryStore::addDirectory(const QString &pathOrUrl)
{
    const QString path = toLocalPath(pathOrUrl);
    if (path.isEmpty() || !QFileInfo(path).isDir())
        return false;
    if (m_directories.contains(path))
        return false;

    m_directories.append(path);
    emit directoriesChanged();
    save();

    // Reflect the new folder immediately; a no-op while a scan runs.
    requestScan();
    return true;
}

void LibraryStore::removeDirectory(const QString &pathOrUrl)
{
    const QString path = toLocalPath(pathOrUrl);
    if (!m_directories.removeOne(path))
        return;

    // Drop the entries that lived under the removed folder (prefix match on
    // the normalized paths; adding '/' keeps "/a/b" from matching "/a/bc").
    const QString prefix = path + QLatin1Char('/');
    for (int i = m_entries.size() - 1; i >= 0; --i) {
        if (m_entries.at(i).path == path
            || m_entries.at(i).path.startsWith(prefix))
            m_entries.removeAt(i);
    }

    emit directoriesChanged();
    emit entriesChanged();
    save();
}

void LibraryStore::requestScan()
{
    if (m_scanning)
        return;

    m_scanning = true;
    emit scanningChanged();

    // v1 (this commit): the scan runs on the caller's thread; the next
    // commit moves it onto a worker QThread with progress + cancellation.
    m_scanFileCount = 0;
    emit scanFileCountChanged();
    applyScanResults(
        scanDirectories(m_directories, m_scanOptions,
                        [this](int count) {
                            m_scanFileCount = count;
                            emit scanFileCountChanged();
                        }));
}

void LibraryStore::notePlayed(const QString &path)
{
    if (path.isEmpty())
        return;

    for (int i = 0; i < m_entries.size(); ++i) {
        if (m_entries.at(i).path != path)
            continue;
        LibraryEntry entry = m_entries.at(i);
        entry.lastPlayed =
            QDateTime::currentDateTime().toString(Qt::ISODate);
        m_entries.replace(i, entry);
        emit entriesChanged();
        save();
        return;
    }
}

QString LibraryStore::mediaTypeForSuffix(const QString &suffix)
{
    if (videoSuffixes().contains(suffix))
        return QStringLiteral("video");
    if (audioSuffixes().contains(suffix))
        return QStringLiteral("audio");
    return {};
}

QList<LibraryEntry> LibraryStore::scanDirectories(
    const QStringList &dirs, const ScanOptions &options,
    const std::function<void(int)> &progress,
    const std::function<bool()> &cancelled)
{
    QList<LibraryEntry> entries;
    QSet<QString> seen;
    int found = 0;

    const auto stop = [&cancelled]() { return cancelled && cancelled(); };

    for (const QString &dirPath : dirs) {
        if (stop())
            break;

        const QDir dir(dirPath);
        if (!dir.exists())
            continue;

        // QDir::Files only: directories are never reported, but they are
        // still traversed (checkAndPushDirectory runs before matchesFilters
        // in qdiriterator.cpp). No QDir::Hidden while skipHidden is set, so
        // hidden files are filtered out and hidden directories are not even
        // descended into; FollowSymlinks stays unset, so directory symlinks
        // are not followed (no cycles).
        QDir::Filters filters = QDir::Files | QDir::NoDotAndDotDot;
        if (!options.skipHidden)
            filters |= QDir::Hidden;

        QDirIterator it(dir.absolutePath(), filters,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) {
            it.next();
            if (stop())
                return entries;

            const QFileInfo info = it.fileInfo();

            // Depth of the file relative to this root: number of directories
            // between the root and the file ("file.mkv" = 0, "a/b.mkv" = 1).
            if (options.maxDepth > 0) {
                const QString relative =
                    dir.relativeFilePath(info.absoluteFilePath());
                if (relative.count(QLatin1Char('/')) > options.maxDepth)
                    continue;
            }

            const QString type = mediaTypeForSuffix(info.suffix().toLower());
            if (type.isEmpty())
                continue;

            const QString path = info.absoluteFilePath();
            if (seen.contains(path))
                continue; // nested folders can cover the same file twice
            seen.insert(path);

            LibraryEntry entry;
            entry.path = path;
            entry.title = info.completeBaseName();
            entry.type = type;
            entry.size = info.size();
            entries.append(entry);
            ++found;

            // Throttled progress: per-file signals would flood the GUI for
            // big libraries.
            if (progress && (found % 32) == 0)
                progress(found);
        }
    }

    if (progress && found > 0)
        progress(found);

    std::sort(entries.begin(), entries.end(),
              [](const LibraryEntry &a, const LibraryEntry &b) {
                  const int byTitle = a.title.compare(b.title,
                                                      Qt::CaseInsensitive);
                  if (byTitle != 0)
                      return byTitle < 0;
                  return a.path.compare(b.path, Qt::CaseInsensitive) < 0;
              });
    return entries;
}

void LibraryStore::applyScanResults(const QList<LibraryEntry> &entries)
{
    // A re-scan is the authority on what exists, but a file that survived
    // keeps its last-played stamp (the walk cannot know it).
    QHash<QString, QString> lastPlayed;
    lastPlayed.reserve(m_entries.size());
    for (const LibraryEntry &entry : m_entries)
        lastPlayed.insert(entry.path, entry.lastPlayed);

    m_entries = entries;
    for (LibraryEntry &entry : m_entries) {
        const QString played = lastPlayed.value(entry.path);
        if (!played.isEmpty())
            entry.lastPlayed = played;
    }

    m_scanning = false;
    emit scanningChanged();
    emit entriesChanged();
    save();
}

void LibraryStore::load()
{
    m_directories.clear();
    m_entries.clear();

    QFile file(m_filePath);
    if (!file.open(QIODevice::ReadOnly))
        return; // no library yet

    QJsonParseError parseError;
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
        return; // corrupt file: start with an empty library

    if (!doc.isObject())
        return;

    const QJsonObject root = doc.object();
    const QJsonArray dirs = root.value(QStringLiteral("directories")).toArray();
    for (const QJsonValue &value : dirs) {
        const QString path = value.toString();
        if (!path.isEmpty() && !m_directories.contains(path))
            m_directories.append(path);
    }

    const QJsonArray array = root.value(QStringLiteral("entries")).toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject object = value.toObject();
        LibraryEntry entry;
        entry.path = object.value(QStringLiteral("path")).toString();
        if (entry.path.isEmpty())
            continue;
        entry.title = object.value(QStringLiteral("title")).toString();
        entry.type = object.value(QStringLiteral("type")).toString();
        entry.size = object.value(QStringLiteral("size")).toInteger();
        entry.lastPlayed =
            object.value(QStringLiteral("lastPlayed")).toString();
        m_entries.append(entry);
    }
}

void LibraryStore::save()
{
    QDir().mkpath(QFileInfo(m_filePath).absolutePath());

    QSaveFile file(m_filePath);
    if (!file.open(QIODevice::WriteOnly))
        return;

    QJsonArray dirs;
    for (const QString &path : m_directories)
        dirs.append(path);

    QJsonArray array;
    for (const LibraryEntry &entry : m_entries) {
        QJsonObject object;
        object.insert(QStringLiteral("path"), entry.path);
        object.insert(QStringLiteral("title"), entry.title);
        object.insert(QStringLiteral("type"), entry.type);
        object.insert(QStringLiteral("size"), entry.size);
        object.insert(QStringLiteral("lastPlayed"), entry.lastPlayed);
        array.append(object);
    }

    QJsonObject root;
    root.insert(QStringLiteral("version"), 1);
    root.insert(QStringLiteral("directories"), dirs);
    root.insert(QStringLiteral("entries"), array);
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.commit(); // atomic replace; a crash mid-write keeps the old file
}
