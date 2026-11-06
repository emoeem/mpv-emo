#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include <numeric>

#include "compat/MpvProfileScanner.h"

using namespace Qt::Literals::StringLiterals;

// Headless unit tests for the P4 compat scanner. The fixture tree lives in
// fixtures/mpv-conf/ (located via QFINDTESTDATA); location probing is tested
// with injected base directories so the real HOME is never read or written.
class TestCompatScanner : public QObject
{
    Q_OBJECT

private slots:
    void scanFindsAllKindsAndCounts();
    void scanMissingDirectoryIsInvalid();
    void scanEmptyDirectoryIsValidButContentless();
    void entryCountsComeFromParser();
    void locatePrefersPlatformDefaultOverXdgFallback();
    void locateFallsBackToXdgConfigHome();
    void locateReturnsEmptyWhenNothingExists();
    void locateOnWindowsUsesAppData();
    void summaryVariantRoundTrip();

private:
    static QString fixtureDir();
};

QString TestCompatScanner::fixtureDir()
{
    // tests/fixtures/mpv-conf, resolved relative to this test's source dir.
    return QFileInfo(QFINDTESTDATA("fixtures/mpv-conf/mpv.conf"))
        .absolutePath();
}

void TestCompatScanner::scanFindsAllKindsAndCounts()
{
    const ImportSummary summary = MpvProfileScanner::scan(fixtureDir());

    QVERIFY(summary.isValid());
    QCOMPARE(summary.scriptCount, 2);   // scripts/a.lua + scripts/lib/b.js
    QCOMPARE(summary.scriptOptCount, 1); // script-opts/foo.conf
    QCOMPARE(summary.shaderCount, 1);   // shaders/x.glsl
    QCOMPARE(summary.fontCount, 1);     // fonts/Fake.ttf
    // 4 conf files + 1 script-opt + 3 files under scripts/ + 1 shader + 1 font.
    QCOMPARE(summary.files.size(), 10);

    bool sawDataBin = false;
    for (const ImportFileSummary &file : summary.files) {
        QVERIFY(QFileInfo(summary.sourceDir + QLatin1Char('/') + file.relativePath)
                    .isFile());
        QVERIFY(file.sizeBytes > 0);
        if (file.relativePath == u"scripts/data.bin"_s) {
            sawDataBin = true;
            QCOMPARE(file.kind, u"other"_s);
        }
    }
    QVERIFY(sawDataBin);
    QCOMPARE(summary.totalBytes,
             std::accumulate(summary.files.cbegin(), summary.files.cend(),
                             qint64(0), [](qint64 sum, const ImportFileSummary &f) {
                                 return sum + f.sizeBytes;
                             }));
}

void TestCompatScanner::scanMissingDirectoryIsInvalid()
{
    const ImportSummary summary =
        MpvProfileScanner::scan(u"/definitely/not/a/real/dir"_s);
    QVERIFY(!summary.isValid());
    QVERIFY(summary.files.isEmpty());
}

void TestCompatScanner::scanEmptyDirectoryIsValidButContentless()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const ImportSummary summary = MpvProfileScanner::scan(dir.path());

    QVERIFY(summary.isValid());
    QVERIFY(!summary.hasContent());
    QCOMPARE(summary.files.size(), 0);
}

void TestCompatScanner::entryCountsComeFromParser()
{
    const ImportSummary summary = MpvProfileScanner::scan(fixtureDir());

    const auto findFile = [&summary](const QString &relPath) {
        for (const ImportFileSummary &file : summary.files)
            if (file.relativePath == relPath)
                return file;
        return ImportFileSummary{};
    };

    // mpv.conf: 3 top-level options + 1 profile header + 2 profile options;
    // the profile section entries are counted too (they parse).
    QCOMPARE(findFile(u"mpv.conf"_s).kind, u"mpv-conf"_s);
    QCOMPARE(findFile(u"mpv.conf"_s).entryCount, 6);
    // input.conf: 3 bindings.
    QCOMPARE(findFile(u"input.conf"_s).kind, u"input-conf"_s);
    QCOMPARE(findFile(u"input.conf"_s).entryCount, 3);
    // profiles.conf: 2 headers + 3 option entries.
    QCOMPARE(findFile(u"profiles.conf"_s).kind, u"profiles-conf"_s);
    QCOMPARE(findFile(u"profiles.conf"_s).entryCount, 5);
    QCOMPARE(findFile(u"platform.conf"_s).entryCount, 1);
    QCOMPARE(findFile(u"script-opts/foo.conf"_s).kind, u"script-opt"_s);
    QCOMPARE(findFile(u"script-opts/foo.conf"_s).entryCount, 1);
    // Non-config files are counted but not parsed.
    QCOMPARE(findFile(u"scripts/a.lua"_s).kind, u"script"_s);
    QCOMPARE(findFile(u"scripts/a.lua"_s).entryCount, 0);
    QCOMPARE(findFile(u"fonts/Fake.ttf"_s).kind, u"font"_s);
}

void TestCompatScanner::locatePrefersPlatformDefaultOverXdgFallback()
{
    QTemporaryDir home;
    QTemporaryDir xdg;
    QVERIFY(home.isValid());
    QVERIFY(xdg.isValid());
    // Both candidates exist: ~/.config/mpv wins on Linux.
    QVERIFY(QDir(home.path()).mkpath(u".config/mpv"_s));
    QVERIFY(QDir(xdg.path()).mkpath(u"mpv"_s));

    QCOMPARE(MpvProfileScanner::locateConfigDirIn(home.path(), xdg.path(),
                                                  QString()),
             home.path() + u"/.config/mpv"_s);
}

void TestCompatScanner::locateFallsBackToXdgConfigHome()
{
    QTemporaryDir home;
    QTemporaryDir xdg;
    QVERIFY(home.isValid());
    QVERIFY(xdg.isValid());
    // Only the XDG candidate exists.
    QVERIFY(QDir(xdg.path()).mkpath(u"mpv"_s));

    QCOMPARE(MpvProfileScanner::locateConfigDirIn(home.path(), xdg.path(),
                                                  QString()),
             xdg.path() + u"/mpv"_s);
}

void TestCompatScanner::locateReturnsEmptyWhenNothingExists()
{
    QTemporaryDir home;
    QVERIFY(home.isValid());
    QCOMPARE(MpvProfileScanner::locateConfigDirIn(home.path(), QString(),
                                                  QString()),
             QString());
}

void TestCompatScanner::locateOnWindowsUsesAppData()
{
#ifdef Q_OS_WIN
    QTemporaryDir appData;
    QVERIFY(appData.isValid());
    QVERIFY(QDir(appData.path()).mkpath(u"mpv"_s));
    QCOMPARE(MpvProfileScanner::locateConfigDirIn(QString(), QString(),
                                                  appData.path()),
             appData.path() + u"/mpv"_s);
#else
    // The Windows branch is compiled out elsewhere; document the contract.
    QSKIP("Windows-only probe");
#endif
}

void TestCompatScanner::summaryVariantRoundTrip()
{
    const ImportSummary original = MpvProfileScanner::scan(fixtureDir());
    QVERIFY(original.isValid());

    const ImportSummary restored =
        ImportSummary::fromVariantMap(original.toVariantMap());

    QCOMPARE(restored.sourceDir, original.sourceDir);
    QCOMPARE(restored.scriptCount, original.scriptCount);
    QCOMPARE(restored.scriptOptCount, original.scriptOptCount);
    QCOMPARE(restored.shaderCount, original.shaderCount);
    QCOMPARE(restored.fontCount, original.fontCount);
    QCOMPARE(restored.totalBytes, original.totalBytes);
    QCOMPARE(restored.files.size(), original.files.size());
    for (int i = 0; i < original.files.size(); ++i) {
        QCOMPARE(restored.files.at(i).kind, original.files.at(i).kind);
        QCOMPARE(restored.files.at(i).relativePath,
                 original.files.at(i).relativePath);
        QCOMPARE(restored.files.at(i).sizeBytes,
                 original.files.at(i).sizeBytes);
        QCOMPARE(restored.files.at(i).entryCount,
                 original.files.at(i).entryCount);
    }
}

QTEST_GUILESS_MAIN(TestCompatScanner)
#include "test_compat_scanner.moc"
