#include <QFile>
#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

#include "mpv/MpvEngine.h"
#include "quality/IccController.h"
#include "settings/AppSettings.h"

using namespace Qt::Literals::StringLiterals;

// Headless tests for the P5 v2 ICC application path: a real (idle) libmpv
// core receives icc-profile / icc-cache-dir, and both options are read
// back through the engine itself. The cache must always land in the P4
// redirect location, and any failure must fall back off with a hint.
class TestIccController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void disabledStartLeavesImportedProfileAlone();
    void missingProfileFallsBackOffWithHint();
    void validProfileAppliesWithRedirectedCache();
    void disableRestoresStartupSnapshot();
    void fileUrlIsNormalizedToLocalPath();
    void startupAppliesPersistedPreferences();

private:
    QString makeProfile(const QString &name);

    QTemporaryDir m_dir;
};

void TestIccController::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

QString TestIccController::makeProfile(const QString &name)
{
    // Content does not matter at the property level: mpv validates the
    // profile when the renderer loads it, the controller checks
    // existence/readability and the option round trip.
    const QString path = m_dir.filePath(name);
    QFile file(path);
    QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
    file.write("stub icc bytes");
    return path;
}

void TestIccController::disabledStartLeavesImportedProfileAlone()
{
    // An imported mpv.conf profile (P4 passes icc-profile through) must
    // survive a controller that never got enabled.
    const QString imported = makeProfile(u"imported.icc"_s);
    MpvEngine engine;
    QVERIFY(engine.initialize(
        QVariantMap{{u"icc-profile"_s, imported}}));

    AppSettings settings(m_dir.filePath(u"off.json"_s));
    IccController controller(&engine, &settings, m_dir.path(), &engine);
    controller.startup();

    QCOMPARE(controller.enabled(), false);
    QCOMPARE(engine.getProperty(u"icc-profile"_s).toString(), imported);
    QVERIFY(engine.getProperty(u"icc-cache-dir"_s).toString().isEmpty());
    QVERIFY(!controller.failed());
}

void TestIccController::missingProfileFallsBackOffWithHint()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"missing.json"_s));
    IccController controller(&engine, &settings, m_dir.path(), &engine);
    controller.startup();

    controller.setProfilePath(m_dir.filePath(u"does-not-exist.icc"_s));
    controller.setEnabled(true);

    // Honest fallback: the switch is back off (persisted), the hint is
    // up, and the engine was never touched.
    QCOMPARE(controller.failed(), true);
    QVERIFY(!controller.failureHint().isEmpty());
    QCOMPARE(controller.enabled(), false);
    QVERIFY(engine.getProperty(u"icc-profile"_s).toString().isEmpty());
    QVERIFY(engine.getProperty(u"icc-cache-dir"_s).toString().isEmpty());

    AppSettings reloaded(m_dir.filePath(u"missing.json"_s));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.qualityIccEnabled(), false);
    QCOMPARE(reloaded.qualityIccProfilePath(),
             m_dir.filePath(u"does-not-exist.icc"_s));
}

void TestIccController::validProfileAppliesWithRedirectedCache()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"apply.json"_s));
    IccController controller(&engine, &settings, m_dir.path(), &engine);
    controller.startup();

    const QString profile = makeProfile(u"display.icc"_s);
    controller.setProfilePath(profile);
    controller.setEnabled(true);

    QCOMPARE(controller.failed(), false);
    QVERIFY(controller.failureHint().isEmpty());
    QCOMPARE(engine.getProperty(u"icc-profile"_s).toString(), profile);
    // The 3DLUT cache follows the P4 redirect table: <appData>/icc_cache.
    QCOMPARE(engine.getProperty(u"icc-cache-dir"_s).toString(),
             m_dir.path() + u"/icc_cache"_s);
    QCOMPARE(controller.cacheDir(), m_dir.path() + u"/icc_cache"_s);
}

void TestIccController::disableRestoresStartupSnapshot()
{
    // An imported profile must come back when EmoPlay's ICC is disabled.
    const QString imported = makeProfile(u"imported2.icc"_s);
    MpvEngine engine;
    QVERIFY(engine.initialize(
        QVariantMap{{u"icc-profile"_s, imported}}));

    AppSettings settings(m_dir.filePath(u"restore.json"_s));
    IccController controller(&engine, &settings, m_dir.path(), &engine);
    controller.startup();

    const QString ours = makeProfile(u"ours.icc"_s);
    controller.setProfilePath(ours);
    controller.setEnabled(true);
    QCOMPARE(engine.getProperty(u"icc-profile"_s).toString(), ours);

    controller.setEnabled(false);
    QCOMPARE(engine.getProperty(u"icc-profile"_s).toString(), imported);
    QVERIFY(!controller.failed());

    // Re-enabling works after a disable round trip.
    controller.setEnabled(true);
    QCOMPARE(engine.getProperty(u"icc-profile"_s).toString(), ours);
}

void TestIccController::fileUrlIsNormalizedToLocalPath()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"url.json"_s));
    IccController controller(&engine, &settings, m_dir.path(), &engine);
    controller.startup();

    const QString profile = makeProfile(u"url.icc"_s);
    controller.setProfilePath(QUrl::fromLocalFile(profile).toString());
    QCOMPARE(controller.profilePath(), profile);
    QCOMPARE(engine.getProperty(u"icc-profile"_s).toString(), profile);
}

void TestIccController::startupAppliesPersistedPreferences()
{
    const QString profile = makeProfile(u"startup.icc"_s);
    const QString file = m_dir.filePath(u"startup.json"_s);
    {
        AppSettings settings(file);
        settings.setQualityIccEnabled(true);
        settings.setQualityIccProfilePath(profile);
        settings.save();
    }

    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(file);
    QVERIFY(settings.load());
    IccController controller(&engine, &settings, m_dir.path(), &engine);
    controller.startup();

    QCOMPARE(engine.getProperty(u"icc-profile"_s).toString(), profile);
    QCOMPARE(engine.getProperty(u"icc-cache-dir"_s).toString(),
             m_dir.path() + u"/icc_cache"_s);
    QCOMPARE(controller.enabled(), true);
}

QTEST_GUILESS_MAIN(TestIccController)
#include "test_icc_controller.moc"
