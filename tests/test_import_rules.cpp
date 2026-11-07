#include <QTemporaryDir>
#include <QtTest>

#include "compat/MpvImportRules.h"

using namespace Qt::Literals::StringLiterals;

// Headless tests for the P4 import policy: embedding-critical options are
// forced, write paths are redirected out of the user's mpv directory, and
// everything else passes through.
class TestImportRules : public QObject
{
    Q_OBJECT

private slots:
    void forcedKeysAlwaysWin();
    void userOptionsPassThrough();
    void lastOccurrenceWins();
    void profileSectionsSkippedAndReported();
    void profileReferencesSkipped();
    void includeAndDumpOptionsStripped();
    void writePathsRedirectedToAppData();
    void configRelativeScreenshotTemplateStripped();
    void plainScreenshotTemplateKept();
    void configRelativeShaderValuesKept();
    void forcedAndStrippedKeyLists();
    void redirectTargetSharesTheStartupTable();

private:
    static QVector<MpvConfEntry> parse(const QString &text)
    {
        return MpvConfigParser::parseMpvConf(text);
    }
};

void TestImportRules::forcedKeysAlwaysWin()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"vo=gpu-next\nosc=yes\nterminal=yes\nvolume=80\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QCOMPARE(result.options.value(u"vo"_s).toString(), u"libmpv"_s);
    QCOMPARE(result.options.value(u"osc"_s).toString(), u"no"_s);
    QCOMPARE(result.options.value(u"terminal"_s).toString(), u"no"_s);
    // Untouched option survives.
    QCOMPARE(result.options.value(u"volume"_s).toString(), u"80"_s);
    // All three replacements are audited.
    QCOMPARE(result.forced.size(), 3);
    QVERIFY(result.forced.first().startsWith(u"vo: user value 'gpu-next'"_s));
}

void TestImportRules::userOptionsPassThrough()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"hwdec=auto-copy\nkeep-open=yes\nvolume=65\n"
              u"slang=chs,eng\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QCOMPARE(result.options.value(u"hwdec"_s).toString(), u"auto-copy"_s);
    QCOMPARE(result.options.value(u"keep-open"_s).toString(), u"yes"_s);
    QCOMPARE(result.options.value(u"volume"_s).toString(), u"65"_s);
    QCOMPARE(result.options.value(u"slang"_s).toString(), u"chs,eng"_s);
    QVERIFY(result.forced.isEmpty());
    QVERIFY(result.profileNames.isEmpty());
}

void TestImportRules::lastOccurrenceWins()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"volume=80\nvolume=90\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QCOMPARE(result.options.value(u"volume"_s).toString(), u"90"_s);
}

void TestImportRules::profileSectionsSkippedAndReported()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"volume=50\n[hq]\nscale=ewa_lanczossharp\ndeband=yes\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    // Top-level option kept; profile options dropped, profile reported.
    QCOMPARE(result.options.value(u"volume"_s).toString(), u"50"_s);
    QVERIFY(!result.options.contains(u"scale"_s));
    QVERIFY(!result.options.contains(u"deband"_s));
    QCOMPARE(result.profileNames, QStringList{u"hq"_s});
    QVERIFY(result.skipped.first().contains(u"[hq]"_s));
}

void TestImportRules::profileReferencesSkipped()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"profile=Target\nprofile=Dither\nvolume=50\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QVERIFY(!result.options.contains(u"profile"_s));
    QCOMPARE(result.skipped.size(), 2);
    QVERIFY(result.skipped.first().startsWith(u"profile:"_s));
    // Options after a profile reference still apply (not inside a section).
    QCOMPARE(result.options.value(u"volume"_s).toString(), u"50"_s);
}

void TestImportRules::includeAndDumpOptionsStripped()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"include=extra.conf\nlog-file=~~/mpv.log\ndump-stats=stats.bin\n"
              u"volume=50\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QVERIFY(!result.options.contains(u"include"_s));
    QVERIFY(!result.options.contains(u"log-file"_s));
    QVERIFY(!result.options.contains(u"dump-stats"_s));
    QCOMPARE(result.skipped.size(), 3);
}

void TestImportRules::writePathsRedirectedToAppData()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"watch-later-dir=~~/cache/watch_later\nvolume=50\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    // The five write paths always point into EmoPlay's app data.
    QCOMPARE(result.options.value(u"watch-later-dir"_s).toString(),
             appData.path() + u"/watch_later"_s);
    QCOMPARE(result.options.value(u"screenshot-directory"_s).toString(),
             appData.path() + u"/screenshots"_s);
    QCOMPARE(result.options.value(u"gpu-shader-cache-dir"_s).toString(),
             appData.path() + u"/shader_cache"_s);
    QCOMPARE(result.options.value(u"icc-cache-dir"_s).toString(),
             appData.path() + u"/icc_cache"_s);
    QCOMPARE(result.options.value(u"demuxer-cache-dir"_s).toString(),
             appData.path() + u"/demuxer_cache"_s);
    QCOMPARE(result.redirected.size(), 5);
}

void TestImportRules::configRelativeScreenshotTemplateStripped()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"screenshot-template=~~/files/screen/%n\nvolume=50\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QVERIFY(!result.options.contains(u"screenshot-template"_s));
    QVERIFY(result.skipped.first().startsWith(u"screenshot-template:"_s));
}

void TestImportRules::plainScreenshotTemplateKept()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"screenshot-template=%F-%n\nvolume=50\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QCOMPARE(result.options.value(u"screenshot-template"_s).toString(),
             u"%F-%n"_s);
}

void TestImportRules::configRelativeShaderValuesKept()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    // ~~ values are read paths: they must survive so the engine side can
    // resolve them against the user's config dir (no file copying).
    const ImportRuleResult result = MpvImportRules::apply(
        parse(u"glsl-shaders=~~/shaders/Anime4K/glsl/x.glsl\nicc-profile=~~/icc/display.icc\n"_s),
        u"/home/user/.config/mpv"_s, appData.path());

    QCOMPARE(result.options.value(u"glsl-shaders"_s).toString(),
             u"~~/shaders/Anime4K/glsl/x.glsl"_s);
    QCOMPARE(result.options.value(u"icc-profile"_s).toString(),
             u"~~/icc/display.icc"_s);
}

void TestImportRules::forcedAndStrippedKeyLists()
{
    const QStringList &forced = MpvImportRules::forcedKeys();
    QCOMPARE(forced.size(), 3);
    QVERIFY(forced.contains(u"vo"_s));
    QVERIFY(forced.contains(u"osc"_s));
    QVERIFY(forced.contains(u"terminal"_s));

    const QStringList &stripped = MpvImportRules::strippedKeys();
    QVERIFY(stripped.contains(u"include"_s));
    QVERIFY(stripped.contains(u"profile"_s));
    QVERIFY(stripped.contains(u"watch-later-dir"_s));
    // Forced keys are not stripped keys: they are replaced, not removed.
    for (const QString &key : forced)
        QVERIFY(!stripped.contains(key));
}

void TestImportRules::redirectTargetSharesTheStartupTable()
{
    // The P5 runtime layers (IccController) reuse the same redirect table
    // as the P4 startup pass, so both always agree on sub-directory names.
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    // The ICC cache is a write path: redirected to app data.
    QCOMPARE(MpvImportRules::redirectTarget(u"icc-cache-dir"_s,
                                            appData.path()),
             appData.path() + u"/icc_cache"_s);
    // The other write paths agree with the startup redirects.
    QCOMPARE(MpvImportRules::redirectTarget(u"watch-later-dir"_s,
                                            appData.path()),
             appData.path() + u"/watch_later"_s);
    QCOMPARE(MpvImportRules::redirectTarget(u"demuxer-cache-dir"_s,
                                            appData.path()),
             appData.path() + u"/demuxer_cache"_s);

    // Read paths (icc-profile keeps its user value, see
    // configRelativeShaderValuesKept) and unknown keys are not targets.
    QVERIFY(MpvImportRules::redirectTarget(u"icc-profile"_s,
                                           appData.path())
                .isEmpty());
    QVERIFY(MpvImportRules::redirectTarget(u"volume"_s,
                                           appData.path())
                .isEmpty());
}

QTEST_GUILESS_MAIN(TestImportRules)
#include "test_import_rules.moc"
