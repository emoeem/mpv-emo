#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>
#include <functional>

#include "plugins/PluginManager.h"
#include "plugins/PluginRemoteFetcher.h"
#include "plugins/RemoteManifestSource.h"

using namespace Qt::Literals::StringLiterals;

// Headless RemoteManifestSource tests. All fetches go through a synchronous
// fake (PluginRemoteFetcher seam) — no network, no real HTTP. The PluginManager
// interaction uses the PluginGitRunner fake from the P8 v1 tests — no git.
// Paths live in temporary directories; the real HOME is never touched.
//
// Covered here: the default source URL, remote JSON parsing of the fixture
// (a manager.json-style document), the pure local+remote merge, cross-source
// name dedupe (first source wins), the AppData/plugin-sources cache round
// trip (offline instances serve the cached list), graceful degradation on
// fetch failure and malformed bodies, custom-source persistence, and the
// PluginManager integration (remote manifests install/update/enable like
// builtin ones and land in appendScriptOptions).
namespace {

class FakeFetcher : public PluginRemoteFetcher
{
public:
    QVector<QUrl> requested;
    QQueue<QUrl> pending;

    void fetch(const QUrl &url) override
    {
        requested.append(url);
        pending.append(url);
    }

    // Test drives completion: answers the oldest outstanding fetch.
    void completeNext(bool success, const QByteArray &body = {},
                      const QString &error = {})
    {
        const QUrl url = pending.takeFirst();
        PluginRemoteFetchResult result;
        result.success = success;
        result.body = body;
        result.error = error;
        result.httpStatus = success ? 200 : 0;
        emit finished(url, result);
    }
};

class FakeGitRunner : public PluginGitRunner
{
public:
    std::function<PluginGitResult(const QStringList &, const QString &)>
        handler;

    void run(const QStringList &arguments,
             const QString &workingDirectory) override
    {
        const PluginGitResult result =
            handler ? handler(arguments, workingDirectory)
                    : PluginGitResult{0, {}, {}, true};
        emit finished(arguments, workingDirectory, result);
    }
};

// Escaped literals instead of raw strings: Qt 6.4's moc chokes on raw
// strings containing quotes and then reports "No relevant classes found"
// for this file's Q_OBJECT test class (same trap as test_plugin_manager.cpp).
static const char kLocalCatalogJson[] =
    "{\"sources\": ["
    "{\"name\": \"uosc\","
    " \"url\": \"https://git.example/uosc.git\","
    " \"branch\": \"main\", \"source\": \".\","
    " \"destination\": \"scripts/uosc\","
    " \"mode\": \"check\", \"baseline\": \"5.13.0\"}"
    "]}";

static const char kSecondSourceJson[] =
    "{\"sources\": ["
    "{\"name\": \"mpv360\","
    " \"url\": \"https://other.example/mpv360.git\","
    " \"branch\": \"main\", \"source\": \".\","
    " \"destination\": \"scripts/mpv360\", \"mode\": \"check\"},"
    "{\"name\": \"extra\","
    " \"url\": \"https://other.example/extra.git\","
    " \"branch\": \"main\", \"source\": \".\","
    " \"destination\": \"scripts/extra\", \"mode\": \"check\"}"
    "]}";

QByteArray fixtureBytes()
{
    QFile file(QStringLiteral(EMOPLAY_SOURCE_DIR)
               + QStringLiteral("/tests/fixtures/remote_manager.json"));
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(content);
    return true;
}

QVariantMap sourceRowNamed(const QVariantList &rows, const QString &url)
{
    for (const QVariant &value : rows) {
        const QVariantMap row = value.toMap();
        if (row.value(u"url"_s).toString() == url)
            return row;
    }
    return {};
}

QVariantMap pluginEntry(const PluginManager &manager, const QString &name)
{
    for (const QVariant &value : manager.plugins()) {
        const QVariantMap entry = value.toMap();
        if (entry.value(u"name"_s).toString() == name)
            return entry;
    }
    return {};
}

} // namespace

class TestRemoteManifestSource : public QObject
{
    Q_OBJECT

private slots:
    void defaultSourceIsTheUpstreamManagerJson();
    void parsesRemoteFixtureIntoEntries();
    void mergeCatalogCopiesLocalState();
    void mergeCatalogDedupesByNameFirstWins();
    void mergeUsesLivePluginManagerState();
    void fetchFailureDegradesToCachedEntries();
    void malformedRemoteBodyKeepsThePreviousCache();
    void cachePersistsAcrossInstances();
    void customSourcesAddRemovePersist();
    void crossSourceNameCollisionFirstSourceWins();

private:
    static void captureFinish(PluginManager &manager, bool *ok,
                              QString *message);
    static void captureError(RemoteManifestSource &source, QString *error);
};

void TestRemoteManifestSource::captureFinish(PluginManager &manager, bool *ok,
                                             QString *message)
{
    // Capture the caller-owned pointers BY VALUE (see test_plugin_manager.cpp
    // for the dangling-reference reasoning).
    QObject::connect(
        &manager, &PluginManager::operationFinished,
        [ok, message](const QString &, const QString &, bool success,
                      const QString &finishedMessage) {
            if (ok)
                *ok = success;
            if (message)
                *message = finishedMessage;
        });
}

void TestRemoteManifestSource::captureError(RemoteManifestSource &source,
                                            QString *error)
{
    QObject::connect(&source, &RemoteManifestSource::errorOccurred,
                     [error](const QString &message) { *error = message; });
}

void TestRemoteManifestSource::defaultSourceIsTheUpstreamManagerJson()
{
    QCOMPARE(RemoteManifestSource::defaultSourceUrl(),
             QStringLiteral(
                 "https://raw.githubusercontent.com/emoeem/mpv/main/manager.json"));
}

void TestRemoteManifestSource::parsesRemoteFixtureIntoEntries()
{
    QTemporaryDir dir;
    FakeFetcher fetcher;
    RemoteManifestSource source(dir.path(), nullptr, &fetcher);

    // Exactly one source out of the box: the default upstream manifest.
    QCOMPARE(source.sources().size(), 1);
    const QVariantMap defaultRow = source.sources().first().toMap();
    QCOMPARE(defaultRow.value(u"isDefault"_s).toBool(), true);
    QCOMPARE(defaultRow.value(u"url"_s).toString(),
             RemoteManifestSource::defaultSourceUrl());
    QCOMPARE(defaultRow.value(u"lastRefreshMs"_s).toLongLong(), 0LL);
    QVERIFY(!source.refreshing());

    QVERIFY(!fixtureBytes().isEmpty());
    source.refreshAll();
    QVERIFY(source.refreshing());
    QCOMPARE(fetcher.requested.size(), 1);
    QCOMPARE(fetcher.requested.first(),
             QUrl(RemoteManifestSource::defaultSourceUrl()));
    fetcher.completeNext(true, fixtureBytes());

    QVERIFY(!source.refreshing());
    QCOMPARE(source.entries().size(), 3);

    const QVariantMap first = source.entries().first().toMap();
    QCOMPARE(first.value(u"name"_s).toString(), u"uosc"_s);
    QCOMPARE(first.value(u"url"_s).toString(),
             u"https://git.example/uosc_danmaku.git"_s);
    QCOMPARE(first.value(u"branch"_s).toString(), u"main"_s);
    QCOMPARE(first.value(u"destination"_s).toString(),
             u"scripts/uosc_danmaku"_s);
    QCOMPARE(first.value(u"mode"_s).toString(), u"check"_s);
    QCOMPARE(first.value(u"baseline"_s).toString(), u"d8c1cd8"_s);
    QCOMPARE(first.value(u"isScript"_s).toBool(), true);
    // Merged against no local state: everything shows "not installed".
    QCOMPARE(first.value(u"installed"_s).toBool(), false);
    QCOMPARE(first.value(u"enabled"_s).toBool(), false);

    const QVariantMap shader = source.entries().at(2).toMap();
    QCOMPARE(shader.value(u"name"_s).toString(), u"remote_shader_pack"_s);
    QCOMPARE(shader.value(u"isScript"_s).toBool(), false);
    QCOMPARE(shader.value(u"mode"_s).toString(), u"replace"_s);

    const QVariantMap row = source.sources().first().toMap();
    QVERIFY(row.value(u"lastRefreshMs"_s).toLongLong() > 0);
}

void TestRemoteManifestSource::mergeCatalogCopiesLocalState()
{
    const QVector<PluginManifest> manifests =
        PluginManifest::parseList(fixtureBytes());
    QCOMPARE(manifests.size(), 3);

    QVariantList local;
    QVariantMap row;
    row.insert(u"name"_s, u"uosc"_s);
    row.insert(u"installed"_s, true);
    row.insert(u"enabled"_s, true);
    row.insert(u"head"_s, u"deadbeef0001"_s);
    row.insert(u"updateAvailable"_s, true);
    row.insert(u"locallyModified"_s, false);
    local.append(row);

    const QVariantList merged = RemoteManifestSource::mergeCatalog(manifests, local);
    QCOMPARE(merged.size(), 3);

    // Local state copied onto the remote entry; the manifest fields keep
    // their REMOTE values (the drawer shows where an update would come from).
    const QVariantMap mergedUosc = merged.at(0).toMap();
    QCOMPARE(mergedUosc.value(u"installed"_s).toBool(), true);
    QCOMPARE(mergedUosc.value(u"enabled"_s).toBool(), true);
    QCOMPARE(mergedUosc.value(u"head"_s).toString(), u"deadbeef0001"_s);
    QCOMPARE(mergedUosc.value(u"updateAvailable"_s).toBool(), true);
    QCOMPARE(mergedUosc.value(u"url"_s).toString(),
             u"https://git.example/uosc_danmaku.git"_s);

    const QVariantMap shader = merged.at(2).toMap();
    QCOMPARE(shader.value(u"installed"_s).toBool(), false);
    QCOMPARE(shader.value(u"enabled"_s).toBool(), false);
    QCOMPARE(shader.value(u"head"_s).toString(), QString());
}

void TestRemoteManifestSource::mergeCatalogDedupesByNameFirstWins()
{
    QVector<PluginManifest> manifests = PluginManifest::parseList(fixtureBytes());
    // A second occurrence of the same name (as two sources can carry one):
    // the first occurrence must win.
    PluginManifest duplicate = manifests.first();
    duplicate.url = u"https://other.example/uosc.git"_s;
    manifests.append(duplicate);

    const QVariantList merged =
        RemoteManifestSource::mergeCatalog(manifests, {});
    QCOMPARE(merged.size(), 3);
    QCOMPARE(merged.at(0).toMap().value(u"url"_s).toString(),
             u"https://git.example/uosc_danmaku.git"_s);
}

void TestRemoteManifestSource::mergeUsesLivePluginManagerState()
{
    QTemporaryDir dir;
    const QString catalogPath = dir.filePath(u"catalog.json"_s);
    writeFile(catalogPath, QByteArray(kLocalCatalogJson));

    FakeGitRunner git;
    git.handler = [](const QStringList &arguments, const QString &) {
        if (arguments.value(0) == QLatin1String("clone")) {
            // Create the target directory with an entry marker, like a real
            // successful clone of a script plugin.
            const QString target = arguments.last();
            QDir().mkpath(target);
            QFile marker(target + u"/main.lua"_s);
            marker.open(QIODevice::WriteOnly);
            marker.write("fake\n");
        }
        if (arguments.value(0) == QLatin1String("rev-parse")) {
            return PluginGitResult{
                0, QStringLiteral("deadbeef00000000000000000000000000000001"),
                {}, true};
        }
        return PluginGitResult{0, {}, {}, true};
    };
    PluginManager manager(dir.path(), catalogPath, &git);
    bool ok = false;
    QString message;
    captureFinish(manager, &ok, &message);

    // A builtin install before any fetch: the merge must see it live.
    manager.install(u"uosc"_s);
    QVERIFY(ok);

    FakeFetcher fetcher;
    RemoteManifestSource source(dir.path(), &manager, &fetcher);
    source.refreshAll();
    fetcher.completeNext(true, fixtureBytes());

    // plugins() now shows the builtin AND the remote catalog, builtin first;
    // the "uosc" name collision resolves to the builtin entry.
    QCOMPARE(manager.plugins().size(), 3);
    QCOMPARE(pluginEntry(manager, u"uosc"_s).value(u"url"_s).toString(),
             u"https://git.example/uosc.git"_s);
    QVERIFY(!pluginEntry(manager, u"mpv360"_s).isEmpty());
    QVERIFY(!pluginEntry(manager, u"remote_shader_pack"_s).isEmpty());

    // The merged remote rows mirror the live local state.
    const QVariantMap remoteUosc = source.entries().at(0).toMap();
    QCOMPARE(remoteUosc.value(u"installed"_s).toBool(), true);
    QCOMPARE(remoteUosc.value(u"enabled"_s).toBool(), true);
    QCOMPARE(remoteUosc.value(u"head"_s).toString(),
             u"deadbeef00000000000000000000000000000001"_s);

    // A remote manifest is plannable and installable through the manager.
    const QVariantMap plan =
        manager.planOperation(u"remote_shader_pack"_s, u"install"_s);
    QVERIFY(!plan.contains(u"error"_s));
    ok = false;
    manager.install(u"mpv360"_s);
    QVERIFY(ok);
    QCOMPARE(source.entries().at(1).toMap().value(u"installed"_s).toBool(),
             true);

    // Enabled+installed remote script plugins join the startup scripts
    // option (append-only behind the user's own value).
    QVariantMap options;
    options.insert(u"scripts"_s, QStringLiteral("/user/own.lua"));
    manager.appendScriptOptions(options);
    const QString scripts = options.value(u"scripts"_s).toString();
    QVERIFY(scripts.startsWith(u"/user/own.lua"_s));
    QVERIFY(scripts.contains(u"plugins/uosc"_s));
    QVERIFY(scripts.contains(u"plugins/mpv360/mpv360.lua"_s));
}

void TestRemoteManifestSource::fetchFailureDegradesToCachedEntries()
{
    QTemporaryDir dir;
    FakeFetcher fetcher;
    RemoteManifestSource source(dir.path(), nullptr, &fetcher);
    QString error;
    captureError(source, &error);

    source.refreshAll();
    fetcher.completeNext(true, fixtureBytes());
    QCOMPARE(source.entries().size(), 3);
    const auto lastRefresh =
        source.sources().first().toMap().value(u"lastRefreshMs"_s).toLongLong();

    source.refreshAll();
    fetcher.completeNext(false, {}, QStringLiteral("network unreachable"));

    QVERIFY(!error.isEmpty());
    QVERIFY(error.contains(u"network unreachable"_s));
    // Graceful degradation: the cached list stays, the refresh time does not
    // move, and the cycle finishes cleanly.
    QCOMPARE(source.entries().size(), 3);
    QCOMPARE(source.sources().first().toMap().value(u"lastRefreshMs"_s)
                 .toLongLong(),
             lastRefresh);
    QVERIFY(!source.refreshing());
}

void TestRemoteManifestSource::malformedRemoteBodyKeepsThePreviousCache()
{
    QTemporaryDir dir;
    FakeFetcher fetcher;
    RemoteManifestSource source(dir.path(), nullptr, &fetcher);
    QString error;
    captureError(source, &error);

    source.refreshAll();
    fetcher.completeNext(true, fixtureBytes());
    QCOMPARE(source.entries().size(), 3);

    // A malformed refresh must not destroy the last good copy (fail closed).
    source.refreshAll();
    fetcher.completeNext(true, QByteArrayLiteral("{ this is not json"));
    QVERIFY(!error.isEmpty());
    QCOMPARE(source.entries().size(), 3);

    // The previous good cache survives on disk: a fresh instance serves it
    // without any fetch.
    FakeFetcher secondFetcher;
    RemoteManifestSource second(dir.path(), nullptr, &secondFetcher);
    QCOMPARE(secondFetcher.requested.size(), 0);
    QCOMPARE(second.entries().size(), 3);
}

void TestRemoteManifestSource::cachePersistsAcrossInstances()
{
    QTemporaryDir dir;
    FakeFetcher firstFetcher;
    {
        RemoteManifestSource first(dir.path(), nullptr, &firstFetcher);
        first.refreshAll();
        firstFetcher.completeNext(true, fixtureBytes());
    }

    // The cache directory holds exactly the per-source cache file (the
    // source list itself is only written when a custom source is added).
    const QDir cacheDir(dir.path() + u"/plugin-sources"_s);
    QCOMPARE(cacheDir.entryList(QDir::Files).size(), 1);

    // A new instance (e.g. next start, or offline) serves the cached list
    // without any network activity.
    FakeFetcher secondFetcher;
    RemoteManifestSource second(dir.path(), nullptr, &secondFetcher);
    QCOMPARE(secondFetcher.requested.size(), 0);
    QCOMPARE(second.entries().size(), 3);
    QVERIFY(second.sources().first().toMap().value(u"lastRefreshMs"_s)
                .toLongLong()
            > 0);
}

void TestRemoteManifestSource::customSourcesAddRemovePersist()
{
    QTemporaryDir dir;
    FakeFetcher fetcher;
    RemoteManifestSource source(dir.path(), nullptr, &fetcher);

    QVERIFY(source.addSource(QUrl(u"https://example.org/manager.json"_s)));
    QCOMPARE(source.sources().size(), 2);

    // Duplicates, bad schemes and the default itself are rejected.
    QVERIFY(!source.addSource(QUrl(u"https://example.org/manager.json"_s)));
    QVERIFY(!source.addSource(QUrl(u"ftp://example.org/manager.json"_s)));
    QVERIFY(!source.addSource(
        QUrl(u"https://raw.githubusercontent.com/emoeem/mpv/main/manager.json"_s)));
    QCOMPARE(source.sources().size(), 2);

    // The default cannot be removed; a custom one can.
    QVERIFY(!source.removeSource(QUrl(RemoteManifestSource::defaultSourceUrl())));
    QVERIFY(source.removeSource(QUrl(u"https://example.org/manager.json"_s)));
    QCOMPARE(source.sources().size(), 1);
    QCOMPARE(source.sources().first().toMap().value(u"isDefault"_s).toBool(),
             true);

    // Custom sources persist across instances (default first, then in
    // insertion order).
    QVERIFY(source.addSource(QUrl(u"https://a.example/one.json"_s)));
    QVERIFY(source.addSource(QUrl(u"https://b.example/two.json"_s)));
    FakeFetcher secondFetcher;
    RemoteManifestSource second(dir.path(), nullptr, &secondFetcher);
    QCOMPARE(second.sources().size(), 3);
    QVERIFY(second.sources().at(0).toMap().value(u"isDefault"_s).toBool());
    QCOMPARE(second.sources().at(1).toMap().value(u"url"_s).toString(),
             u"https://a.example/one.json"_s);
    QCOMPARE(second.sources().at(2).toMap().value(u"url"_s).toString(),
             u"https://b.example/two.json"_s);
}

void TestRemoteManifestSource::crossSourceNameCollisionFirstSourceWins()
{
    QTemporaryDir dir;
    FakeFetcher fetcher;
    RemoteManifestSource source(dir.path(), nullptr, &fetcher);
    QString error;
    captureError(source, &error);

    source.refreshAll();
    fetcher.completeNext(true, fixtureBytes());
    QCOMPARE(source.entries().size(), 3);

    // A second source re-declaring "mpv360": the first source (default) wins.
    QVERIFY(source.addSource(QUrl(u"https://example.org/other.json"_s)));
    fetcher.completeNext(true, QByteArray(kSecondSourceJson));
    QVERIFY(error.isEmpty());
    QCOMPARE(source.entries().size(), 4);

    QVariantMap mpv360;
    for (const QVariant &value : source.entries()) {
        if (value.toMap().value(u"name"_s).toString() == u"mpv360"_s)
            mpv360 = value.toMap();
    }
    QCOMPARE(mpv360.value(u"url"_s).toString(),
             u"https://git.example/mpv360.git"_s);
    QVERIFY(!sourceRowNamed(source.sources(),
                            u"https://example.org/other.json"_s)
                 .isEmpty());
    // The new source contributed its unique entry.
    bool hasExtra = false;
    for (const QVariant &value : source.entries())
        hasExtra |= value.toMap().value(u"name"_s).toString() == u"extra"_s;
    QVERIFY(hasExtra);
}

QTEST_GUILESS_MAIN(TestRemoteManifestSource)
#include "test_remote_manifest_source.moc"
