#include <QTemporaryDir>
#include <QtTest>

#include "mpv/MpvEngine.h"
#include "quality/InterpolationController.h"
#include "settings/AppSettings.h"

using namespace Qt::Literals::StringLiterals;

// Headless tests for the P5 v2 interpolation application path: a real
// (idle) libmpv core receives whatever the controller assembles, and the
// vf chain is read back through the engine itself. Idle media means fps is
// unknown, so admission holds at fps-unknown — the mounted-entry states for
// admitted media are covered by the pure decision table
// (test_interpolation_decision), while everything the controller does on
// an idle core is asserted here against the live vf property.
//
// Whether the CI libmpv was built with vapoursynth or not, the controller
// must behave honestly: either the probe accepts the filter, or it falls
// back off with vsUnavailable set.
class TestInterpolationController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void disabledTouchesNothing();
    void enabledWithMissingScriptBlocksButStaysOn();
    void enabledWithScriptIsHonestAboutSupport();
    void unsupportedSessionFallsBackOnEveryEnable();
    void unknownPresetIsIgnoredAndCorruptionBehavesLikeOff();

private:
    bool writeScript(const QString &path) const;
    bool vfHasOurEntry(MpvEngine &engine) const;

    QTemporaryDir m_dir;
};

void TestInterpolationController::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

bool TestInterpolationController::writeScript(const QString &path) const
{
    const QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    // A stub: existence is all the controller checks; the script itself
    // stays an external user asset.
    file.write("# user-provided VapourSynth preset script\n");
    return true;
}

bool TestInterpolationController::vfHasOurEntry(MpvEngine &engine) const
{
    return InterpolationDecision::vfHasLabeledEntry(
        engine.getProperty(u"vf"_s).toList());
}

void TestInterpolationController::disabledTouchesNothing()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"off.json"_s));
    InterpolationController controller(&engine, &settings,
                                       m_dir.filePath(u"vs"_s), &engine);
    controller.startup();

    QCOMPARE(controller.enabled(), false);
    QCOMPARE(controller.blockReason(), u"disabled"_s);
    QVERIFY(engine.getProperty(u"vf"_s).toList().isEmpty());
}

void TestInterpolationController::enabledWithMissingScriptBlocksButStaysOn()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"missing.json"_s));
    InterpolationController controller(&engine, &settings,
                                       m_dir.filePath(u"vs"_s), &engine);
    controller.startup();

    // No script file anywhere: enabling blocks with the script hint but is
    // NOT a support failure — the user can drop the script in later.
    controller.setEnabled(true);
    if (controller.vsUnavailable()) {
        // This libmpv build rejected the filter before admission even
        // mattered: the honest fallback dominates the missing script.
        QCOMPARE(controller.enabled(), false);
        QVERIFY(!vfHasOurEntry(engine));
        return;
    }
    QCOMPARE(controller.enabled(), true);
    QCOMPARE(controller.blockReason(), u"script-missing"_s);
    QVERIFY(!vfHasOurEntry(engine));
    QCOMPARE(controller.expectedScriptPath(),
             m_dir.filePath(u"vs"_s) + u"/MEMC_RIFE_STD.vpy"_s);

    // The enabled state is persisted.
    AppSettings reloaded(m_dir.filePath(u"missing.json"_s));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.qualityInterpolationEnabled(), true);
}

void TestInterpolationController::enabledWithScriptIsHonestAboutSupport()
{
    const QString scriptsDir = m_dir.filePath(u"vs2"_s);
    QVERIFY(writeScript(scriptsDir + u"/MEMC_RIFE_STD.vpy"_s));

    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"probe.json"_s));
    InterpolationController controller(&engine, &settings, scriptsDir,
                                       &engine);
    controller.startup();
    controller.setEnabled(true);

    if (controller.vsUnavailable()) {
        // This libmpv build rejected the filter: the honest fallback
        // reverted the switch and nothing stays mounted.
        QCOMPARE(controller.enabled(), false);
        QVERIFY(!vfHasOurEntry(engine));
        AppSettings reloaded(m_dir.filePath(u"probe.json"_s));
        QVERIFY(reloaded.load());
        QCOMPARE(reloaded.qualityInterpolationEnabled(), false);
        return;
    }

    // Support proven. Idle media: admission holds at fps-unknown, so
    // nothing is mounted yet and the switch stays on.
    QCOMPARE(controller.enabled(), true);
    QCOMPARE(controller.blockReason(), u"fps-unknown"_s);
    QVERIFY(!vfHasOurEntry(engine));

    AppSettings reloaded(m_dir.filePath(u"probe.json"_s));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.qualityInterpolationEnabled(), true);
}

void TestInterpolationController::unsupportedSessionFallsBackOnEveryEnable()
{
    // Only meaningful on a libmpv without vapoursynth; on a capable one
    // the probe succeeds and this scenario cannot occur.
    {
        MpvEngine engine;
        QVERIFY(engine.initialize());
        AppSettings settings(m_dir.filePath(u"fallback.json"_s));
        InterpolationController controller(&engine, &settings,
                                           m_dir.filePath(u"vs3"_s), &engine);
        QVERIFY(writeScript(m_dir.filePath(u"vs3/MEMC_RIFE_STD.vpy"_s)));
        controller.startup();
        controller.setEnabled(true);
        if (!controller.vsUnavailable()) {
            QSKIP("this libmpv build has a vapoursynth filter");
            return;
        }
        QCOMPARE(controller.enabled(), false);

        // An external writer flipping the persisted switch back on must
        // still not mount anything: the session probe result wins.
        settings.setQualityInterpolationEnabled(true);
        settings.save();
        emit engine.fileLoaded();
        QVERIFY(!vfHasOurEntry(engine));
        QCOMPARE(controller.vsUnavailable(), true);
        {
            AppSettings rechecked(m_dir.filePath(u"fallback.json"_s));
            QVERIFY(rechecked.load());
            QCOMPARE(rechecked.qualityInterpolationEnabled(), false);
        }
    }

    // Re-enabling in the same session (new controller over the same
    // settings is equivalent: a new session probe) must fall back again
    // instead of staying on unmounted and silent.
    MpvEngine engine;
    QVERIFY(engine.initialize());
    AppSettings settings(m_dir.filePath(u"fallback.json"_s));
    InterpolationController controller(&engine, &settings,
                                       m_dir.filePath(u"vs3"_s), &engine);
    controller.startup();
    controller.setEnabled(true);
    QCOMPARE(controller.vsUnavailable(), true);
    QCOMPARE(controller.enabled(), false);
    QVERIFY(!vfHasOurEntry(engine));
    AppSettings reloaded(m_dir.filePath(u"fallback.json"_s));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.qualityInterpolationEnabled(), false);
}

void TestInterpolationController::unknownPresetIsIgnoredAndCorruptionBehavesLikeOff()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"preset.json"_s));
    InterpolationController controller(&engine, &settings,
                                       m_dir.filePath(u"vs4"_s), &engine);
    controller.startup();

    // Setting an unknown id through the panel is ignored. On a libmpv
    // without vapoursynth the enable already fell back off (probe), which
    // changes nothing about the preset validation below.
    controller.setEnabled(true);
    QVERIFY(controller.enabled() || controller.vsUnavailable());
    controller.setPreset(u"MEMC_MAGICAL"_s);
    QCOMPARE(controller.preset(), u"MEMC_RIFE_STD"_s);

    // A corrupted settings.json (unknown preset with the switch on) must
    // not enable anything. On a libmpv without vapoursynth the fallback
    // above already persisted "off", so turn the persisted switch back on
    // explicitly to build the corrupted state.
    settings.setQualityInterpolationEnabled(true);
    settings.setQualityInterpolationPreset(u"MEMC_MAGICAL"_s);
    settings.save();
    InterpolationController corrupted(&engine, &settings,
                                      m_dir.filePath(u"vs4"_s), &engine);
    corrupted.startup();
    QCOMPARE(corrupted.enabled(), true); // raw persisted value
    QCOMPARE(corrupted.blockReason(), u"disabled"_s);
    QVERIFY(!vfHasOurEntry(engine));

    // A known id round-trips and moves the expected script path.
    controller.setPreset(u"MEMC_SVP_PRO"_s);
    QCOMPARE(controller.preset(), u"MEMC_SVP_PRO"_s);
    QCOMPARE(controller.expectedScriptPath(),
             m_dir.filePath(u"vs4"_s) + u"/MEMC_SVP_PRO.vpy"_s);
}

QTEST_GUILESS_MAIN(TestInterpolationController)
#include "test_interpolation_controller.moc"
