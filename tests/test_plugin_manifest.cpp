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

QTEST_GUILESS_MAIN(TestPluginManifest)
#include "test_plugin_manifest.moc"
