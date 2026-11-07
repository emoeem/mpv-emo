#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "library/LibraryModel.h"
#include "library/LibraryStore.h"

using namespace Qt::Literals::StringLiterals;

// Headless unit tests for the P6 media library. The storage file path is
// injected per test (QTemporaryDir), the scan runs against the committed
// fixtures/library tree (located via QFINDTESTDATA) or small trees built in
// a QTemporaryDir, and QStandardPaths test mode keeps defaultFilePath()
// away from the user's real app data. requestScan() is exercised through
// its real worker thread (QTRY_* pumps the queued results).
namespace {

// Resolved fixtures/library directory.
QString fixturePath(const QString &relative = {})
{
    const QString base = QFINDTESTDATA("fixtures/library");
    return relative.isEmpty() ? base : base + QLatin1Char('/') + relative;
}

LibraryEntry findEntry(const QList<LibraryEntry> &entries, const QString &title)
{
    for (const LibraryEntry &entry : entries) {
        if (entry.title == title)
            return entry;
    }
    return {};
}

bool makeFile(const QString &path, const char *content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(content) > 0;
}

QJsonObject entryJson(const QString &path, const QString &title,
                      const QString &type, qint64 size,
                      const QString &lastPlayed)
{
    QJsonObject object;
    object.insert(u"path"_s, path);
    object.insert(u"title"_s, title);
    object.insert(u"type"_s, type);
    object.insert(u"size"_s, size);
    object.insert(u"lastPlayed"_s, lastPlayed);
    return object;
}

} // namespace

class TestLibraryStore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();

    // Pure scan logic
    void mediaTypeClassification();
    void scanClassifiesSkipsAndSorts();
    void scanDepthAndHiddenOptions();
    void scanDedupesAcrossDirectories();
    void scanProgressAndCancellation();

    // Store: folder list
    void defaultPathIsLibraryJsonInAppData();
    void addRemoveDirectoryValidatesAndPersists();
    void addDirectoryAcceptsFileUrl();
    void removeDirectoryDropsItsEntries();
    void removeDirectoryPrefixDoesNotCatchSiblings();

    // Store: scans
    void autoScanAfterAddFindsEntries();
    void chainedScanCoversLateFolders();
    void scanUpdatesScanningAndCountProperties();
    void requestScanWithoutDirectoriesKeepsEmpty();
    void rescanPreservesLastPlayed();

    // Store: lastPlayed + persistence
    void notePlayedUpdatesAndPersists();
    void notePlayedUnknownPathIsIgnored();
    void corruptFileRecoversToEmpty();

    // LibraryModel
    void modelMirrorsStoreEntries();
    void modelFilterNarrowsAndResets();
    void modelFollowsStoreUpdates();
    void modelRolesExposeEntryFields();
};

void TestLibraryStore::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
}

// ---------------------------------------------------------------- pure scan

void TestLibraryStore::mediaTypeClassification()
{
    QCOMPARE(LibraryStore::mediaTypeForSuffix(u"mkv"_s), u"video"_s);
    QCOMPARE(LibraryStore::mediaTypeForSuffix(u"webm"_s), u"video"_s);
    QCOMPARE(LibraryStore::mediaTypeForSuffix(u"flac"_s), u"audio"_s);
    QCOMPARE(LibraryStore::mediaTypeForSuffix(u"opus"_s), u"audio"_s);
    QCOMPARE(LibraryStore::mediaTypeForSuffix(u"txt"_s), QString());
    QCOMPARE(LibraryStore::mediaTypeForSuffix(u"exe"_s), QString());
    QCOMPARE(LibraryStore::mediaTypeForSuffix(QString()), QString());
}

// Movies fixture (default options: maxDepth 2, skipHidden):
//   movie.mkv / trailer.mp4 / BIG.MKV (depth 0, video)
//   Season 1/ep1.mkv (video), Season 1/theme.wav (audio)   — depth 1
//   Season 1/Deep/nested.mp4 (video)                       — depth 2
//   Season 1/Deep/Deepest/too-deep.mp4                     — depth 3, skipped
//   readme.txt (no media extension) and .hidden.mkv (hidden), skipped
// Result must be sorted by title case-insensitively (BIG before ep1).
void TestLibraryStore::scanClassifiesSkipsAndSorts()
{
    const QList<LibraryEntry> entries = LibraryStore::scanDirectories(
        {fixturePath(u"Movies"_s)}, LibraryStore::ScanOptions{});

    QCOMPARE(entries.size(), 6);
    QCOMPARE(entries.at(0).title, u"BIG"_s);
    QCOMPARE(entries.at(1).title, u"ep1"_s);
    QCOMPARE(entries.at(2).title, u"movie"_s);
    QCOMPARE(entries.at(3).title, u"nested"_s);
    QCOMPARE(entries.at(4).title, u"theme"_s);
    QCOMPARE(entries.at(5).title, u"trailer"_s);

    // Uppercase extension classifies like its lower-case form.
    QCOMPARE(entries.at(0).type, u"video"_s);
    QCOMPARE(findEntry(entries, u"theme"_s).type, u"audio"_s);
    QCOMPARE(findEntry(entries, u"ep1"_s).type, u"video"_s);

    // Title is the file name without extensions; size is the file size.
    const LibraryEntry movie = findEntry(entries, u"movie"_s);
    QVERIFY(!movie.path.isEmpty());
    QCOMPARE(movie.path, fixturePath(u"Movies/movie.mkv"_s));
    QCOMPARE(movie.size, QFileInfo(fixturePath(u"Movies/movie.mkv"_s)).size());

    // Excluded entries.
    QVERIFY(findEntry(entries, u"readme"_s).path.isEmpty());
    QVERIFY(findEntry(entries, u"hidden"_s).path.isEmpty());
    QVERIFY(findEntry(entries, u"too-deep"_s).path.isEmpty());
}

void TestLibraryStore::scanDepthAndHiddenOptions()
{
    const QStringList dirs = {fixturePath(u"Movies"_s)};

    LibraryStore::ScanOptions options;
    options.maxDepth = 1;
    QCOMPARE(LibraryStore::scanDirectories(dirs, options).size(), 5);

    // maxDepth <= 0 means unlimited: too-deep.mp4 (depth 3) is included.
    options.maxDepth = 0;
    QCOMPARE(LibraryStore::scanDirectories(dirs, options).size(), 7);

    // Turning the hidden filter off adds .hidden.mkv.
    options.maxDepth = 2;
    options.skipHidden = false;
    QCOMPARE(LibraryStore::scanDirectories(dirs, options).size(), 7);
}

void TestLibraryStore::scanDedupesAcrossDirectories()
{
    const QStringList dirs = {fixturePath(u"Movies"_s),
                              fixturePath(u"Movies/Season 1"_s)};
    // Movies alone yields 6 (depth <= 2); the Season 1 walk re-finds
    // ep1/theme/nested but reaches too-deep.mp4 at its own depth 2. The
    // union is 7 unique files and the overlapping ones appear once each.
    const QList<LibraryEntry> entries =
        LibraryStore::scanDirectories(dirs, LibraryStore::ScanOptions{});
    QCOMPARE(entries.size(), 7);
    for (const char *title : {"ep1", "theme", "nested"}) {
        int occurrences = 0;
        for (const LibraryEntry &entry : entries) {
            if (entry.title == QLatin1String(title))
                ++occurrences;
        }
        QCOMPARE(occurrences, 1);
    }
    QVERIFY(!findEntry(entries, u"too-deep"_s).path.isEmpty());

    // Scanning the same directory twice neither duplicates nor doubles.
    QCOMPARE(LibraryStore::scanDirectories({fixturePath(u"Movies"_s),
                                            fixturePath(u"Movies"_s)},
                                           LibraryStore::ScanOptions{})
                 .size(),
             6);
}

void TestLibraryStore::scanProgressAndCancellation()
{
    int last = 0;
    int calls = 0;
    const QList<LibraryEntry> entries = LibraryStore::scanDirectories(
        {fixturePath(u"Movies"_s)}, LibraryStore::ScanOptions{},
        [&last, &calls](int found) {
            last = found;
            ++calls;
        });
    QCOMPARE(entries.size(), 6);
    // The final call reports every file found; with 6 files there is no
    // intermediate (every-32) call.
    QCOMPARE(last, 6);
    QCOMPARE(calls, 1);

    // A cancelled walk stops early and returns what it has.
    int checks = 0;
    const QList<LibraryEntry> partial = LibraryStore::scanDirectories(
        {fixturePath(u"Movies"_s)}, LibraryStore::ScanOptions{}, {},
        [&checks]() { return ++checks > 3; });
    QVERIFY(partial.size() < 6);
}

// --------------------------------------------------------- folder list

void TestLibraryStore::defaultPathIsLibraryJsonInAppData()
{
    const QString path = LibraryStore::defaultFilePath();
    QVERIFY(QFileInfo(path).isAbsolute());
    QVERIFY(path.endsWith(u"library.json"_s));
    // Test mode redirects AppDataLocation away from the user's real data.
    QVERIFY(path.contains(u".qttest"_s));
}

void TestLibraryStore::addRemoveDirectoryValidatesAndPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(u"library.json"_s);

    LibraryStore store(file);
    QSignalSpy spy(&store, &LibraryStore::directoriesChanged);
    QVERIFY(spy.isValid());

    // Non-directories are rejected.
    QCOMPARE(store.addDirectory(u"/nonexistent/nope"_s), false);
    QCOMPARE(store.addDirectory(fixturePath(u"Movies/movie.mkv"_s)), false);
    QCOMPARE(spy.count(), 0);

    // Adding a directory succeeds; the trailing-slash form dedupes.
    QCOMPARE(store.addDirectory(fixturePath(u"Movies"_s)), true);
    QCOMPARE(store.addDirectory(fixturePath(u"Movies/"_s)), false);
    QCOMPARE(store.directories().size(), 1);
    QCOMPARE(spy.count(), 1);

    // The folder list survives a reload.
    QTRY_VERIFY(!store.isScanning());
    LibraryStore reloaded(file);
    QCOMPARE(reloaded.directories(), store.directories());
}

void TestLibraryStore::addDirectoryAcceptsFileUrl()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    // The QML dialogs hand over file:// URLs (like MpvEngine::loadFile).
    const QString url = QUrl::fromLocalFile(fixturePath(u"Music"_s)).toString();
    QCOMPARE(store.addDirectory(url), true);
    QCOMPARE(store.directories().size(), 1);
    QCOMPARE(store.directories().first(), fixturePath(u"Music"_s));
}

void TestLibraryStore::removeDirectoryDropsItsEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(u"library.json"_s);

    LibraryStore store(file);
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_VERIFY(!store.isScanning());
    QCOMPARE(store.entryList().size(), 6);

    QSignalSpy dirSpy(&store, &LibraryStore::directoriesChanged);
    QSignalSpy entrySpy(&store, &LibraryStore::entriesChanged);
    store.removeDirectory(fixturePath(u"Movies"_s));
    QCOMPARE(dirSpy.count(), 1);
    QCOMPARE(entrySpy.count(), 1);
    QCOMPARE(store.directories().size(), 0);
    QCOMPARE(store.entryList().size(), 0);

    // Unknown folders are a silent no-op.
    store.removeDirectory(u"/unknown/dir"_s);
    QCOMPARE(dirSpy.count(), 1);

    LibraryStore reloaded(file);
    QCOMPARE(reloaded.directories().size(), 0);
    QCOMPARE(reloaded.entryList().size(), 0);
}

// The removed folder's path prefix must not swallow sibling folders whose
// names merely start with the same string ("A" vs "AB").
void TestLibraryStore::removeDirectoryPrefixDoesNotCatchSiblings()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(u"library.json"_s);
    QVERIFY(QDir(dir.path()).mkpath(u"A"_s));
    QVERIFY(QDir(dir.path()).mkpath(u"AB"_s));
    QVERIFY(makeFile(dir.filePath(u"A/x.mkv"_s), "x"));
    QVERIFY(makeFile(dir.filePath(u"AB/y.mkv"_s), "y"));

    LibraryStore store(file);
    QVERIFY(store.addDirectory(dir.filePath(u"A"_s)));
    QVERIFY(store.addDirectory(dir.filePath(u"AB"_s)));
    QTRY_VERIFY(!store.isScanning());
    QCOMPARE(store.entryList().size(), 2);

    store.removeDirectory(dir.filePath(u"A"_s));
    QCOMPARE(store.entryList().size(), 1);
    QCOMPARE(store.entryList().first().path, dir.filePath(u"AB/y.mkv"_s));
}

// ------------------------------------------------------------------ scans

void TestLibraryStore::autoScanAfterAddFindsEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(u"library.json"_s);

    LibraryStore store(file);
    QSignalSpy spy(&store, &LibraryStore::entriesChanged);

    // addDirectory kicks off a scan on the worker thread.
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_COMPARE(store.entryList().size(), 6);
    QVERIFY(spy.count() >= 1);
    QVERIFY(!store.isScanning());
    QCOMPARE(store.scanFileCount(), 6);

    // The scan results are persisted.
    LibraryStore reloaded(file);
    QCOMPARE(reloaded.entryList().size(), 6);
    QCOMPARE(findEntry(reloaded.entryList(), u"movie"_s).type, u"video"_s);
}

void TestLibraryStore::chainedScanCoversLateFolders()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    // The second add lands while the first scan is likely still running;
    // the request must be chained (or restarted), never dropped.
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QVERIFY(store.addDirectory(fixturePath(u"Music"_s)));

    QTRY_COMPARE(store.entryList().size(), 8); // 6 Movies + 2 Music
    QVERIFY(!store.isScanning());
    QVERIFY(findEntry(store.entryList(), u"track"_s).type == u"audio"_s);
    QVERIFY(findEntry(store.entryList(), u"song"_s).type == u"audio"_s);
}

void TestLibraryStore::scanUpdatesScanningAndCountProperties()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_VERIFY(!store.isScanning());

    QSignalSpy scanningSpy(&store, &LibraryStore::scanningChanged);
    QSignalSpy countSpy(&store, &LibraryStore::scanFileCountChanged);

    store.requestScan();
    QCOMPARE(scanningSpy.count(), 1); // scanning = true (synchronous)
    QVERIFY(store.isScanning());
    QTRY_VERIFY(!store.isScanning());
    QCOMPARE(scanningSpy.count(), 2); // + scanning = false
    QVERIFY(countSpy.count() >= 1);
    QCOMPARE(store.scanFileCount(), 6);
}

void TestLibraryStore::requestScanWithoutDirectoriesKeepsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    store.requestScan();
    QTRY_VERIFY(!store.isScanning());
    QCOMPARE(store.entryList().size(), 0);
}

void TestLibraryStore::rescanPreservesLastPlayed()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_VERIFY(!store.isScanning());

    const QString moviePath = fixturePath(u"Movies/movie.mkv"_s);
    store.notePlayed(moviePath);
    const QString stamp = findEntry(store.entryList(), u"movie"_s).lastPlayed;
    QVERIFY(!stamp.isEmpty());

    store.requestScan();
    QTRY_VERIFY(!store.isScanning());
    QCOMPARE(store.entryList().size(), 6);
    QCOMPARE(findEntry(store.entryList(), u"movie"_s).lastPlayed, stamp);
}

// ------------------------------------------------------------ lastPlayed

void TestLibraryStore::notePlayedUpdatesAndPersists()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_VERIFY(!store.isScanning());

    QSignalSpy spy(&store, &LibraryStore::entriesChanged);
    store.notePlayed(fixturePath(u"Movies/Season 1/ep1.mkv"_s));
    QCOMPARE(spy.count(), 1);
    const QString stamp =
        findEntry(store.entryList(), u"ep1"_s).lastPlayed;
    QVERIFY(QDateTime::fromString(stamp, Qt::ISODate).isValid());

    LibraryStore reloaded(dir.filePath(u"library.json"_s));
    QCOMPARE(findEntry(reloaded.entryList(), u"ep1"_s).lastPlayed, stamp);
}

void TestLibraryStore::notePlayedUnknownPathIsIgnored()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_VERIFY(!store.isScanning());

    QSignalSpy spy(&store, &LibraryStore::entriesChanged);
    store.notePlayed(u"/unknown/file.mkv"_s);
    store.notePlayed(QString());
    QCOMPARE(spy.count(), 0);
    QCOMPARE(store.entryList().size(), 6);
}

void TestLibraryStore::corruptFileRecoversToEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString file = dir.filePath(u"library.json"_s);

    QFile garbage(file);
    QVERIFY(garbage.open(QIODevice::WriteOnly));
    garbage.write("this is not json {{{");
    garbage.close();

    LibraryStore store(file);
    QCOMPARE(store.directories().size(), 0);
    QCOMPARE(store.entryList().size(), 0);

    // Adding a directory must overwrite the broken file with valid JSON.
    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_VERIFY(!store.isScanning());
    LibraryStore reloaded(file);
    QCOMPARE(reloaded.directories().size(), 1);
    QCOMPARE(reloaded.entryList().size(), 6);
}

// ----------------------------------------------------------- LibraryModel

// A pre-written library.json gives the model a synchronous starting state.
void TestLibraryStore::modelMirrorsStoreEntries()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonArray entries;
    entries.append(entryJson(u"/media/A/alpha.mkv"_s, u"alpha"_s, u"video"_s,
                             10, QString()));
    entries.append(entryJson(u"/media/B/Zeta.flac"_s, u"Zeta"_s, u"audio"_s,
                             20, QString()));
    QJsonObject root;
    root.insert(u"version"_s, 1);
    root.insert(u"directories"_s, QJsonArray());
    root.insert(u"entries"_s, entries);
    QFile file(dir.filePath(u"library.json"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();

    LibraryStore store(file);
    QCOMPARE(store.entryList().size(), 2);

    LibraryModel model(&store);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.rowCount(model.index(0, 0)), 0); // no children

    const QVariant path =
        model.data(model.index(0, 0), LibraryModel::PathRole);
    QCOMPARE(path.toString(), u"/media/A/alpha.mkv"_s);
    QCOMPARE(model.data(model.index(1, 0), LibraryModel::TitleRole)
                 .toString(),
             u"Zeta"_s);
    QCOMPARE(model.data(model.index(1, 0), LibraryModel::TypeRole).toString(),
             u"audio"_s);
    QCOMPARE(model.data(model.index(1, 0), LibraryModel::SizeRole).toInt(),
             20);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::LastPlayedRole)
                 .toString(),
             QString());
    QCOMPARE(model.data(model.index(0, 0), Qt::DisplayRole).toString(),
             u"alpha"_s);
    QCOMPARE(model.data(QModelIndex(), LibraryModel::PathRole), QVariant());
    QCOMPARE(model.data(model.index(5, 0), LibraryModel::PathRole),
             QVariant());
}

void TestLibraryStore::modelFilterNarrowsAndResets()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    QJsonArray entries;
    entries.append(entryJson(u"/media/A/alpha.mkv"_s, u"alpha"_s, u"video"_s,
                             10, QString()));
    entries.append(entryJson(u"/media/B/Zeta.flac"_s, u"Zeta"_s, u"audio"_s,
                             20, QString()));
    QJsonObject root;
    root.insert(u"version"_s, 1);
    root.insert(u"directories"_s, QJsonArray());
    root.insert(u"entries"_s, entries);
    QFile file(dir.filePath(u"library.json"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();

    LibraryStore store(file);
    LibraryModel model(&store);
    QCOMPARE(model.count(), 2);
    QCOMPARE(model.filter(), QString());

    QSignalSpy countSpy(&model, &LibraryModel::countChanged);
    QSignalSpy filterSpy(&model, &LibraryModel::filterChanged);

    // Title match, case-insensitive.
    model.setFilter(u"ALP"_s);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::TitleRole)
                 .toString(),
             u"alpha"_s);
    QCOMPARE(countSpy.count(), 1);
    QCOMPARE(filterSpy.count(), 1);

    // Path match, case-insensitive; title of the other row does not match.
    // (count stays 1, so countChanged must not fire again.)
    model.setFilter(u"/media/b"_s);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::TitleRole)
                 .toString(),
             u"Zeta"_s);
    QCOMPARE(countSpy.count(), 1);

    // No match.
    model.setFilter(u"nothing-matches"_s);
    QCOMPARE(model.count(), 0);

    // Resetting the filter restores everything; same value is a no-op.
    model.setFilter(QString());
    QCOMPARE(model.count(), 2);
    model.setFilter(QString());
    QCOMPARE(countSpy.count(), 3);
    QCOMPARE(filterSpy.count(), 4);
}

void TestLibraryStore::modelFollowsStoreUpdates()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    // Start with one fake entry; adding a real folder replaces the store's
    // list, and the model must follow without any manual refresh.
    QJsonArray entries;
    entries.append(entryJson(u"/gone/fake.mkv"_s, u"fake"_s, u"video"_s, 1,
                             QString()));
    QJsonObject root;
    root.insert(u"version"_s, 1);
    root.insert(u"directories"_s, QJsonArray());
    root.insert(u"entries"_s, entries);
    QFile file(dir.filePath(u"library.json"_s));
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
    file.close();

    LibraryStore store(file);
    LibraryModel model(&store);
    QCOMPARE(model.count(), 1);

    QVERIFY(store.addDirectory(fixturePath(u"Movies"_s)));
    QTRY_COMPARE(model.count(), 6);
    QVERIFY(findEntry(store.entryList(), u"fake"_s).path.isEmpty());
    // The filtered-out view follows too.
    model.setFilter(u"movie"_s);
    QCOMPARE(model.count(), 1);
    QCOMPARE(model.data(model.index(0, 0), LibraryModel::PathRole).toString(),
             fixturePath(u"Movies/movie.mkv"_s));
}

void TestLibraryStore::modelRolesExposeEntryFields()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    LibraryStore store(dir.filePath(u"library.json"_s));
    LibraryModel model(&store);

    const QHash<int, QByteArray> roles = model.roleNames();
    QVERIFY(roles.contains(LibraryModel::PathRole));
    QVERIFY(roles.contains(LibraryModel::TitleRole));
    QVERIFY(roles.contains(LibraryModel::TypeRole));
    QVERIFY(roles.contains(LibraryModel::SizeRole));
    QVERIFY(roles.contains(LibraryModel::LastPlayedRole));
    QCOMPARE(roles.value(LibraryModel::PathRole), QByteArray("path"));
    QCOMPARE(roles.value(LibraryModel::TypeRole), QByteArray("type"));
}

QTEST_GUILESS_MAIN(TestLibraryStore)
#include "test_library_store.moc"
