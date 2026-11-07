#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

#include "plugins/PluginManifest.h"

using namespace Qt::Literals::StringLiterals;

// Pure-logic tests for the manager.json-compatible manifest parser
// (docs/nativeization-plan.md §4.1): valid entries, missing/typed fields,
// invalid modes, path-traversal rejection and the shipped builtin catalog.
class TestPluginManifest : public QObject
{
    Q_OBJECT

private slots:
    void validEntryParses();
    void defaultsFillOptionalFields();
    void missingRequiredFieldsRejected();
    void invalidModeRejected();
    void typeConfusionRejected();
    void pluginNameCharsetEnforced();
    void unsafePathsRejected();
    void managerJsonFormAccepted();
    void bareArrayFormAccepted();
    void duplicateNamesRejected();
    void malformedJsonRejected();
    void scriptPluginDetection();
    void contentRootHonorsSourceSubtree();
    void builtinCatalogParsesWithSixUpstreamSources();

private:
    static QJsonObject entryWithDefaults();
};

QJsonObject TestPluginManifest::entryWithDefaults()
{
    QJsonObject entry;
    entry.insert(u"name"_s, u"uosc_danmaku"_s);
    entry.insert(u"url"_s, u"https://github.com/Tony15246/uosc_danmaku.git"_s);
    entry.insert(u"destination"_s, u"scripts/uosc_danmaku"_s);
    return entry;
}

void TestPluginManifest::validEntryParses()
{
    QJsonObject entry = entryWithDefaults();
    entry.insert(u"branch"_s, u"main"_s);
    entry.insert(u"source"_s, u"src/uosc"_s);
    entry.insert(u"mode"_s, u"check"_s);
    entry.insert(u"baseline"_s, u"5.13.0"_s);

    const PluginManifest manifest = PluginManifest::parse(entry);
    QVERIFY(manifest.valid);
    QCOMPARE(manifest.name, u"uosc_danmaku"_s);
    QCOMPARE(manifest.url, u"https://github.com/Tony15246/uosc_danmaku.git"_s);
    QCOMPARE(manifest.branch, u"main"_s);
    QCOMPARE(manifest.source, u"src/uosc"_s);
    QCOMPARE(manifest.destination, u"scripts/uosc_danmaku"_s);
    QCOMPARE(manifest.mode, u"check"_s);
    QCOMPARE(manifest.baseline, u"5.13.0"_s);
    QVERIFY(!manifest.isReplace());
}

void TestPluginManifest::defaultsFillOptionalFields()
{
    const PluginManifest manifest = PluginManifest::parse(entryWithDefaults());
    QVERIFY(manifest.valid);
    // Defaults per nativeization-plan §4.1/§4.2: branch main, source "."
    // (whole repo), mode check (never destructive by default), no baseline.
    QCOMPARE(manifest.branch, u"main"_s);
    QCOMPARE(manifest.source, u"."_s);
    QCOMPARE(manifest.mode, u"check"_s);
    QCOMPARE(manifest.baseline, QString());
}

void TestPluginManifest::missingRequiredFieldsRejected()
{
    QJsonObject noName = entryWithDefaults();
    noName.remove(u"name"_s);
    QCOMPARE(PluginManifest::parse(noName).valid, false);

    QJsonObject noUrl = entryWithDefaults();
    noUrl.remove(u"url"_s);
    QVERIFY(!PluginManifest::parse(noUrl).valid);

    QJsonObject noDestination = entryWithDefaults();
    noDestination.remove(u"destination"_s);
    QVERIFY(!PluginManifest::parse(noDestination).valid);
}

void TestPluginManifest::invalidModeRejected()
{
    // A typo in mode must never silently pick the destructive semantics.
    QJsonObject entry = entryWithDefaults();
    entry.insert(u"mode"_s, u"repalce"_s);
    const PluginManifest manifest = PluginManifest::parse(entry);
    QVERIFY(!manifest.valid);
    QVERIFY(manifest.error.contains(u"mode"_s));
}

void TestPluginManifest::typeConfusionRejected()
{
    QJsonObject entry = entryWithDefaults();
    entry.insert(u"branch"_s, 3);
    QVERIFY(!PluginManifest::parse(entry).valid);

    QJsonObject nameIsNumber = entryWithDefaults();
    nameIsNumber.insert(u"name"_s, 42);
    QVERIFY(!PluginManifest::parse(nameIsNumber).valid);
}

void TestPluginManifest::pluginNameCharsetEnforced()
{
    // The name becomes a directory name under <AppData>/plugins/ — no
    // separators, no dots, no emptiness.
    QJsonObject slash = entryWithDefaults();
    slash.insert(u"name"_s, u"../evil"_s);
    QVERIFY(!PluginManifest::parse(slash).valid);

    QJsonObject hidden = entryWithDefaults();
    hidden.insert(u"name"_s, u".git"_s);
    QVERIFY(!PluginManifest::parse(hidden).valid);

    QJsonObject uppercase = entryWithDefaults();
    uppercase.insert(u"name"_s, u"Anime4K_Pack"_s);
    QVERIFY(PluginManifest::parse(uppercase).valid); // charset allows A-Z
}

void TestPluginManifest::unsafePathsRejected()
{
    QJsonObject traversal = entryWithDefaults();
    traversal.insert(u"destination"_s, u"shaders/../../etc"_s);
    QVERIFY(!PluginManifest::parse(traversal).valid);

    QJsonObject absolute = entryWithDefaults();
    absolute.insert(u"source"_s, u"/etc/mpv"_s);
    QVERIFY(!PluginManifest::parse(absolute).valid);

    QJsonObject dotdotSource = entryWithDefaults();
    dotdotSource.insert(u"source"_s, u".."_s);
    QVERIFY(!PluginManifest::parse(dotdotSource).valid);
}

void TestPluginManifest::managerJsonFormAccepted()
{
    // mpv-Emo manager.json wraps the entries in a "sources" array.
    QJsonArray sources;
    sources.append(entryWithDefaults());
    QJsonObject root;
    root.insert(u"sources"_s, sources);
    QString error;
    const QVector<PluginManifest> manifests =
        PluginManifest::parseList(QJsonDocument(root).toJson(), &error);
    QCOMPARE(error, QString());
    QCOMPARE(manifests.size(), 1);
    QVERIFY(manifests.first().valid);
}

void TestPluginManifest::bareArrayFormAccepted()
{
    QJsonArray sources;
    sources.append(entryWithDefaults());
    QString error;
    const QVector<PluginManifest> manifests =
        PluginManifest::parseList(QJsonDocument(sources).toJson(), &error);
    QCOMPARE(error, QString());
    QCOMPARE(manifests.size(), 1);
}

void TestPluginManifest::duplicateNamesRejected()
{
    // Plugin ids are unique: the second entry must not shadow the first.
    QJsonArray sources;
    sources.append(entryWithDefaults());
    sources.append(entryWithDefaults());
    QString error;
    const QVector<PluginManifest> manifests =
        PluginManifest::parseList(QJsonDocument(sources).toJson(), &error);
    QVERIFY(manifests.isEmpty());
    QVERIFY(error.contains(u"duplicate"_s));
}

void TestPluginManifest::malformedJsonRejected()
{
    QString error;
    QVERIFY(PluginManifest::parseList(QByteArray("{not json"), &error).isEmpty());
    QVERIFY(!error.isEmpty());

    error.clear();
    QVERIFY(PluginManifest::parseList(QByteArray("42"), &error).isEmpty());
    QVERIFY(!error.isEmpty());
}

void TestPluginManifest::scriptPluginDetection()
{
    // scripts/ destinations feed mpv's --scripts option; shaders/ do not.
    QJsonObject script = entryWithDefaults();
    script.insert(u"destination"_s, u"scripts/uosc_danmaku"_s);
    QVERIFY(PluginManifest::parse(script).isScriptPlugin());

    QJsonObject singleFile = entryWithDefaults();
    singleFile.insert(u"destination"_s, u"scripts/mpv360.lua"_s);
    QVERIFY(PluginManifest::parse(singleFile).isScriptPlugin());

    QJsonObject shader = entryWithDefaults();
    shader.insert(u"destination"_s, u"shaders/Anime4K/glsl"_s);
    QVERIFY(!PluginManifest::parse(shader).isScriptPlugin());
}

void TestPluginManifest::contentRootHonorsSourceSubtree()
{
    const PluginManifest whole = PluginManifest::parse(entryWithDefaults());
    QVERIFY(whole.valid);
    QCOMPARE(whole.contentRoot(u"/plugins/x"_s), u"/plugins/x"_s);

    QJsonObject subtree = entryWithDefaults();
    subtree.insert(u"source"_s, u"src/uosc"_s);
    const PluginManifest manifest = PluginManifest::parse(subtree);
    QVERIFY(manifest.valid);
    QCOMPARE(manifest.contentRoot(u"/plugins/uosc"_s),
             u"/plugins/uosc/src/uosc"_s);
}

void TestPluginManifest::builtinCatalogParsesWithSixUpstreamSources()
{
    // The catalog shipped with the app is transcribed from the user
    // repository emoeem/mpv manager.json (commit 70d6eb4). It must always
    // parse and keep exactly these six upstream sources.
    QFile file(QStringLiteral(EMOPLAY_SOURCE_BUILTIN_CATALOG));
    QVERIFY2(file.open(QIODevice::ReadOnly),
             "builtin catalog not found at compile-time path");
    QString error;
    const QVector<PluginManifest> manifests =
        PluginManifest::parseList(file.readAll(), &error);
    file.close();
    if (!error.isEmpty())
        QFAIL(qPrintable(error));
    QCOMPARE(manifests.size(), 6);

    QStringList names;
    for (const PluginManifest &manifest : manifests)
        names.append(manifest.name);
    names.sort();
    QCOMPARE(names.join(QLatin1Char(',')),
             u"anime4k,file_browser,hdr-toys,trakt_scrobble,uosc,uosc_danmaku"_s);

    // Spot-check the transcribed fields against manager.json (70d6eb4).
    const auto find = [&manifests](const QString &name) {
        for (const PluginManifest &manifest : manifests)
            if (manifest.name == name)
                return manifest;
        return PluginManifest();
    };
    const PluginManifest uosc = find(u"uosc"_s);
    QVERIFY(uosc.valid);
    QCOMPARE(uosc.url, u"https://github.com/tomasklaen/uosc.git"_s);
    QCOMPARE(uosc.branch, u"main"_s);
    QCOMPARE(uosc.source, u"src/uosc"_s);
    QCOMPARE(uosc.destination, u"scripts/uosc"_s);
    QCOMPARE(uosc.mode, u"check"_s);
    QCOMPARE(uosc.baseline, u"5.13.0"_s);

    const PluginManifest danmaku = find(u"uosc_danmaku"_s);
    QVERIFY(danmaku.valid);
    QCOMPARE(danmaku.mode, u"check"_s);
    QCOMPARE(danmaku.baseline,
             u"d8c1cd8b4786c286a19079caef3f9ac9fa64933d"_s);

    const PluginManifest anime4k = find(u"anime4k"_s);
    QVERIFY(anime4k.valid);
    QCOMPARE(anime4k.mode, u"replace"_s);
    QCOMPARE(anime4k.branch, u"master"_s);
    QCOMPARE(anime4k.source, u"glsl"_s);
    QCOMPARE(anime4k.destination, u"shaders/Anime4K/glsl"_s);
    QVERIFY(!anime4k.isScriptPlugin());

    const PluginManifest fileBrowser = find(u"file_browser"_s);
    QVERIFY(fileBrowser.valid);
    QCOMPARE(fileBrowser.mode, u"replace"_s);
    QCOMPARE(fileBrowser.destination, u"scripts/file-browser"_s);
    QVERIFY(fileBrowser.isScriptPlugin());

    const PluginManifest hdrToys = find(u"hdr-toys"_s);
    QVERIFY(hdrToys.valid);
    QCOMPARE(hdrToys.mode, u"check"_s);
    QCOMPARE(hdrToys.baseline,
             u"b43e4575bdd655f4c7536f704219798b07d9a300"_s);
    QVERIFY(!hdrToys.isScriptPlugin());

    const PluginManifest trakt = find(u"trakt_scrobble"_s);
    QVERIFY(trakt.valid);
    QCOMPARE(trakt.mode, u"check"_s);
    QVERIFY(trakt.baseline.isEmpty());
}

QTEST_GUILESS_MAIN(TestPluginManifest)
#include "test_plugin_manifest.moc"
