#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include "settings/AppSettings.h"

using namespace Qt::Literals::StringLiterals;

// Headless tests for the persisted import decision (settings.json). The file
// path is injected per test; QStandardPaths test mode keeps defaultFilePath()
// away from the user's real app data.
class TestAppSettings : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void defaultPathIsSettingsJsonInAppData();
    void startsUndecided();
    void decisionRoundTripsAcrossInstances();
    void selectiveFileListRoundTrips();
    void corruptFileResetsToDefaults();
    void missingDirectoryIsCreatedOnSave();
    void importDecisionTimestamped();
    // P5 quality section.
    void qualityDefaultsWhenMissing();
    void qualitySectionRoundTrips();
    void qualityCoexistsWithImportDecision();
    // P7 music section.
    void musicDefaultsWhenMissing();
    void musicSectionRoundTrips();

private:
    QTemporaryDir m_dir;
};

void TestAppSettings::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void TestAppSettings::defaultPathIsSettingsJsonInAppData()
{
    const QString path = AppSettings::defaultFilePath();
    QVERIFY(QFileInfo(path).isAbsolute());
    QVERIFY(path.endsWith(u"settings.json"_s));
    // Test mode redirects AppDataLocation away from the user's real data.
    QVERIFY(path.contains(u".qttest"_s));
}

void TestAppSettings::startsUndecided()
{
    const QString file = m_dir.filePath(u"undecided.json"_s);
    AppSettings settings(file);
    QVERIFY(!settings.load()); // nothing persisted yet
    QCOMPARE(settings.importDecision(), QString());
    QVERIFY(!settings.dontAskAgain());
    QVERIFY(settings.selectedFiles().isEmpty());
}

void TestAppSettings::decisionRoundTripsAcrossInstances()
{
    const QString file = m_dir.filePath(u"decision.json"_s);
    {
        AppSettings settings(file);
        settings.setImportDecision(u"full"_s);
        QVERIFY(settings.save());
    }
    {
        AppSettings settings(file);
        QVERIFY(settings.load());
        QCOMPARE(settings.importDecision(), u"full"_s);
        QVERIFY(!settings.importedAt().isEmpty());
    }
    {
        // A second decision overwrites the first.
        AppSettings settings(file);
        QVERIFY(settings.load());
        settings.setImportDecision(u"standalone"_s);
        settings.setDontAskAgain(true);
        QVERIFY(settings.save());
    }
    AppSettings settings(file);
    QVERIFY(settings.load());
    QCOMPARE(settings.importDecision(), u"standalone"_s);
    QVERIFY(settings.dontAskAgain());
}

void TestAppSettings::selectiveFileListRoundTrips()
{
    const QString file = m_dir.filePath(u"selective.json"_s);
    QVariantMap summary;
    summary.insert(u"sourceDir"_s, u"/home/user/.config/mpv"_s);
    summary.insert(u"scriptCount"_s, 2);
    {
        AppSettings settings(file);
        settings.setImportDecision(u"selective"_s);
        settings.setSelectedFiles(
            QStringList{u"mpv.conf"_s, u"scripts/a.lua"_s});
        settings.setLastImportSummary(summary);
        QVERIFY(settings.save());
    }
    AppSettings settings(file);
    QVERIFY(settings.load());
    QCOMPARE(settings.importDecision(), u"selective"_s);
    // Extra parentheses: the comma inside the braced list would otherwise
    // split the QCOMPARE macro arguments.
    QCOMPARE(settings.selectedFiles(),
             (QStringList{u"mpv.conf"_s, u"scripts/a.lua"_s}));
    QCOMPARE(settings.lastImportSummary().value(u"scriptCount"_s).toInt(), 2);
    QCOMPARE(settings.lastImportSummary().value(u"sourceDir"_s).toString(),
             u"/home/user/.config/mpv"_s);
}

void TestAppSettings::corruptFileResetsToDefaults()
{
    const QString file = m_dir.filePath(u"corrupt.json"_s);
    {
        AppSettings settings(file);
        settings.setImportDecision(u"full"_s);
        QVERIFY(settings.save());
    }
    QVERIFY(QFile(file).size() > 0);
    QFile corruptFile(file);
    QVERIFY(corruptFile.open(QIODevice::WriteOnly | QIODevice::Truncate));
    corruptFile.write("{ not valid json");
    corruptFile.close();

    AppSettings settings(file);
    QVERIFY(!settings.load());
    QCOMPARE(settings.importDecision(), QString());
    // Saving after corruption rewrites a valid document.
    settings.setImportDecision(u"skip"_s);
    QVERIFY(settings.save());
    AppSettings reloaded(file);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.importDecision(), u"skip"_s);
}

void TestAppSettings::missingDirectoryIsCreatedOnSave()
{
    const QString file = m_dir.filePath(u"nested/deeper/settings.json"_s);
    AppSettings settings(file);
    settings.setImportDecision(u"full"_s);
    QVERIFY(settings.save());
    QVERIFY(QFileInfo::exists(file));
}

void TestAppSettings::importDecisionTimestamped()
{
    const QString file = m_dir.filePath(u"timestamp.json"_s);
    AppSettings settings(file);
    QVERIFY(settings.importedAt().isEmpty());
    settings.setImportDecision(u"skip"_s);
    QVERIFY(!settings.importedAt().isEmpty());
    QDateTime parsed = QDateTime::fromString(settings.importedAt(), Qt::ISODate);
    QVERIFY(parsed.isValid());
    // Same decision again does not refresh the timestamp.
    const QString first = settings.importedAt();
    settings.setImportDecision(u"skip"_s);
    QCOMPARE(settings.importedAt(), first);
}

void TestAppSettings::qualityDefaultsWhenMissing()
{
    // A file written before P5 (no quality section) loads with defaults.
    const QString file = m_dir.filePath(u"pre-quality.json"_s);
    {
        AppSettings settings(file);
        settings.setImportDecision(u"full"_s);
        QVERIFY(settings.save());
    }
    AppSettings settings(file);
    QVERIFY(settings.load());
    QCOMPARE(settings.qualityContentMode(), u"auto"_s);
    QCOMPARE(settings.qualityTier(), u"off"_s);
    QCOMPARE(settings.qualityToneMapping(), u"auto"_s);
    QCOMPARE(settings.qualityScaleOverride(), QString());
    QCOMPARE(settings.qualityDebandMode(), u"auto"_s);
    QVERIFY(settings.manualShaderList().isEmpty());
}

void TestAppSettings::qualitySectionRoundTrips()
{
    const QString file = m_dir.filePath(u"quality.json"_s);
    {
        AppSettings settings(file);
        settings.setQualityContentMode(u"anime"_s);
        settings.setQualityTier(u"high"_s);
        settings.setQualityToneMapping(u"bt.2390"_s);
        settings.setQualityScaleOverride(u"bilinear"_s);
        settings.setQualityDebandMode(u"no"_s);
        settings.setManualShaderList(
            QStringList{u"/mpv/shaders/Anime4K/Anime4K_Restore_CNN_M.glsl"_s});
        QVERIFY(settings.save());
    }
    AppSettings settings(file);
    QVERIFY(settings.load());
    QCOMPARE(settings.qualityContentMode(), u"anime"_s);
    QCOMPARE(settings.qualityTier(), u"high"_s);
    QCOMPARE(settings.qualityToneMapping(), u"bt.2390"_s);
    QCOMPARE(settings.qualityScaleOverride(), u"bilinear"_s);
    QCOMPARE(settings.qualityDebandMode(), u"no"_s);
    QCOMPARE(settings.manualShaderList(),
             (QStringList{u"/mpv/shaders/Anime4K/Anime4K_Restore_CNN_M.glsl"_s}));

    // Change a value and verify it round-trips over the previous state.
    settings.setQualityTier(u"off"_s);
    QVERIFY(settings.save());
    AppSettings reloaded(file);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.qualityTier(), u"off"_s);
    QCOMPARE(reloaded.qualityContentMode(), u"anime"_s);
}

void TestAppSettings::qualityCoexistsWithImportDecision()
{
    // The P4 import decision and the P5 quality section live in one
    // document; saving one must not lose the other.
    const QString file = m_dir.filePath(u"combined.json"_s);
    {
        AppSettings settings(file);
        settings.setImportDecision(u"selective"_s);
        QVERIFY(settings.save());
    }
    {
        AppSettings settings(file);
        QVERIFY(settings.load());
        settings.setQualityTier(u"medium"_s);
        QVERIFY(settings.save());
    }
    AppSettings settings(file);
    QVERIFY(settings.load());
    QCOMPARE(settings.importDecision(), u"selective"_s);
    QCOMPARE(settings.qualityTier(), u"medium"_s);
    QVERIFY(!settings.importedAt().isEmpty());
}

void TestAppSettings::musicDefaultsWhenMissing()
{
    // A file written before P7 (no music section) loads with defaults.
    const QString file = m_dir.filePath(u"pre-music.json"_s);
    {
        AppSettings settings(file);
        settings.setImportDecision(u"full"_s);
        QVERIFY(settings.save());
    }
    AppSettings settings(file);
    QVERIFY(settings.load());
    QVERIFY(!settings.musicEqualizerEnabled());
    QCOMPARE(settings.musicEqualizerPreset(), u"flat"_s);
    QVERIFY(settings.musicEqualizerGains().isEmpty());
    QCOMPARE(settings.importDecision(), u"full"_s);
}

void TestAppSettings::musicSectionRoundTrips()
{
    const QString file = m_dir.filePath(u"music.json"_s);
    {
        AppSettings settings(file);
        settings.setMusicEqualizerEnabled(true);
        settings.setMusicEqualizerPreset(u"custom"_s);
        settings.setMusicEqualizerGains(
            QList<double>{1, 2, 3, 0, 0, -1, -2, 0, 4, 5});
        QVERIFY(settings.save());
    }
    AppSettings settings(file);
    QVERIFY(settings.load());
    QCOMPARE(settings.musicEqualizerEnabled(), true);
    QCOMPARE(settings.musicEqualizerPreset(), u"custom"_s);
    QCOMPARE(settings.musicEqualizerGains(),
             (QList<double>{1, 2, 3, 0, 0, -1, -2, 0, 4, 5}));

    // The music section coexists with the quality/import sections: change
    // one, verify the others survive the save/reload cycle.
    settings.setQualityTier(u"high"_s);
    QVERIFY(settings.save());
    AppSettings reloaded(file);
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.qualityTier(), u"high"_s);
    QCOMPARE(reloaded.musicEqualizerPreset(), u"custom"_s);
    QCOMPARE(reloaded.musicEqualizerEnabled(), true);
}

QTEST_GUILESS_MAIN(TestAppSettings)
#include "test_app_settings.moc"
