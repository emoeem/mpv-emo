#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

#include <atomic>
#include <functional>

class QThread;
class LibraryScanWorker;

// One discovered media file in the library. This is the P6 v1 "entry model":
// no metadata lookup, no thumbnails — just what a directory walk can tell us
// (plus an optional last-played timestamp kept across re-scans).
struct LibraryEntry
{
    QString path;       // absolute local path
    QString title;      // display name (file name without extensions)
    QString type;       // "video" or "audio"
    qint64 size = 0;    // bytes
    QString lastPlayed; // ISO 8601, empty when never played in the library
};

Q_DECLARE_METATYPE(LibraryEntry)

// Media library state: the user's media folders and the media files found in
// them, persisted to a JSON file (AppDataLocation/library.json by default,
// QSaveFile atomic writes — same pattern as HistoryStore / AppSettings).
//
// Semantics (P6 v1, native simplification of the file-browser script family,
// see docs/nativeization-plan.md §2.2):
//  - Directory list: addDirectory() / removeDirectory(), deduped by
//    normalized absolute path, persisted.
//  - Scanning: QDirIterator walk per directory; files classified video/audio
//    by (lower-cased) extension; unknown extensions, hidden entries and
//    directories deeper than ScanOptions::maxDepth are skipped. QDirIterator
//    (Qt 6.4) already skips hidden files during filtering and does not even
//    descend into hidden directories unless QDir::Hidden/AllDirs is set
//    (qtbase src/corelib/io/qdiriterator.cpp matchesFilters / checkAndPushDirectory),
//    and symlinked directories are not followed unless FollowSymlinks is set —
//    so plain QDir::Files + Subdirectories gives the wanted behavior for free.
//  - Entries are deduped by absolute path (nested folders would otherwise
//    report the same file once per covering directory) and sorted by title.
//  - Re-scans replace the entry list but preserve the lastPlayed timestamp of
//    files that are still present.
//
// The scan itself is pure logic and re-entrant (scanDirectories is static).
// requestScan() moves it to a worker QThread so the GUI never blocks, with
// periodic progress and a cancellation flag checked per file. The store only
// applies worker results on its own thread (queued connection).
//
// The store never touches mpv; callers may notePlayed() when a file starts
// playing. Tests inject a storage path and drive the synchronous scan
// directly or the async requestScan() through QTRY_* waits.
class LibraryStore : public QObject
{
    Q_OBJECT
    // Normalized absolute paths of the user's media folders.
    Q_PROPERTY(QStringList directories READ directories NOTIFY directoriesChanged)
    // True while a scan runs on the worker thread.
    Q_PROPERTY(bool scanning READ isScanning NOTIFY scanningChanged)
    // Media files found so far by the running scan (progress hint).
    Q_PROPERTY(int scanFileCount READ scanFileCount NOTIFY scanFileCountChanged)

public:
    struct ScanOptions
    {
        int maxDepth = 2;       // subdirectory levels below a root folder; <= 0 = unlimited
        bool skipHidden = true; // hidden entries (QDirIterator filtering, see class docs)
    };

    // |filePath| is the storage JSON; injected so tests can use a
    // temporary location.
    explicit LibraryStore(const QString &filePath, QObject *parent = nullptr);
    ~LibraryStore() override;

    // Default storage location; requires the application name to be set
    // (QGuiApplication does that before this is called in main()).
    static QString defaultFilePath();

    QStringList directories() const { return m_directories; }
    bool isScanning() const { return m_scanning; }
    int scanFileCount() const { return m_scanFileCount; }

    // Full entry list (the LibraryModel wraps this for QML).
    const QList<LibraryEntry> &entryList() const { return m_entries; }

    // Add |pathOrUrl| (local path or file:// URL, matching the QML dialog
    // convention used by MpvEngine::loadFile) to the folder list. Returns
    // false when the path is not an existing directory or is already in the
    // list. Persisting aside, this kicks off a scan so the UI reflects the
    // new folder without a second click (a no-op while a scan runs).
    Q_INVOKABLE bool addDirectory(const QString &pathOrUrl);
    // Remove the folder and every entry under it. Unknown folders are a
    // no-op.
    Q_INVOKABLE void removeDirectory(const QString &pathOrUrl);

    // Scan the folder list on the worker thread. Ignored while a scan is
    // already running. Results arrive through entriesChanged().
    Q_INVOKABLE void requestScan();

    // Stamp the entry for |path| as played now and persist. Unknown paths
    // are ignored (v1 does not add arbitrary files to the library).
    Q_INVOKABLE void notePlayed(const QString &path);

    void setScanOptions(const ScanOptions &options) { m_scanOptions = options; }

    // Pure scan logic: walk every directory in |dirs|, classify media files
    // by extension, dedupe by absolute path, sort by (case-insensitive)
    // title. |progress| receives the number of files found so far (called
    // every few dozen files and once at the end); |cancelled| is polled per
    // file and stops the walk when it returns true. Both may be null.
    static QList<LibraryEntry> scanDirectories(
        const QStringList &dirs, const ScanOptions &options,
        const std::function<void(int)> &progress = {},
        const std::function<bool()> &cancelled = {});

    // "video" / "audio" / empty (not media) for a lower-cased extension.
    static QString mediaTypeForSuffix(const QString &suffix);

signals:
    void directoriesChanged();
    void entriesChanged();
    void scanningChanged();
    void scanFileCountChanged();

private slots:
    // Queued receiver of the worker's finished() signal; also used to apply
    // a scan's results. Must run on the store's thread.
    void applyScanResults(const QList<LibraryEntry> &entries);
    // Queued receiver of the worker's progress() signal.
    void onScanProgress(int fileCount);

private:
    void load();
    void save();

    QString m_filePath;
    QStringList m_directories;
    QList<LibraryEntry> m_entries;
    ScanOptions m_scanOptions;

    bool m_scanning = false;
    int m_scanFileCount = 0;

    // Scan worker plumbing: one persistent worker thread for the store's
    // lifetime (created in the constructor, shut down in the destructor).
    // The worker lives in that thread; all cross-thread contact is via
    // queued signal/slot or queued invokeMethod, so no locking is needed.
    QThread *m_scanThread = nullptr;
    LibraryScanWorker *m_scanWorker = nullptr;
    std::atomic_bool m_scanCancelled{false};
};
