#include <QTemporaryDir>
#include <QtTest>

#include "mpv/MpvEngine.h"
#include "quality/QualityController.h"
#include "settings/AppSettings.h"

using namespace Qt::Literals::StringLiterals;

// Headless tests for the quality application path: a real (idle) libmpv
// core receives whatever the controller applies, so option values and the
// mounted glsl-shaders list are read back through the engine itself.
class TestQualityController : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void inertAtOffFreshSession();
    void highTierAppliesQualityOptions();
    void offAfterActiveResetsToDefaults();
    void manualShaderMountingPersists();
    void startupAdoptsImportedShaders();
    void autoDowngradeDemotesActiveTierOnly();
    void fileLoadedRestoresUserTier();

private:
    QString shaderA() const { return m_dir.filePath(u"shaders/a.glsl"_s); }
    bool writeShader(const QString &path) const;

    QTemporaryDir m_dir;
};

bool TestQualityController::writeShader(const QString &path) const
{
    const QDir dir = QFileInfo(path).absoluteDir();
    if (!dir.exists() && !dir.mkpath(QStringLiteral(".")))
        return false;
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return false;
    file.write("//!VARIABLE\n// test stub shader\n");
    return true;
}

void TestQualityController::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
    QVERIFY(writeShader(shaderA()));
    QVERIFY(writeShader(m_dir.filePath(u"shaders/sub/b.glsl"_s)));
}

void TestQualityController::inertAtOffFreshSession()
{
    // A fresh session with tier "off" must not touch anything: the
    // imported option value survives untouched.
    MpvEngine engine;
    QVERIFY(engine.initialize(QVariantMap{{u"scale"_s, u"spline36"_s}}));

    AppSettings settings(m_dir.filePath(u"inert.json"_s));
    QualityController controller(&engine, &settings,
                                 m_dir.filePath(u"shaders"_s), &engine);
    controller.startup();
    controller.applyProfile();

    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"spline36"_s);
    QVERIFY(engine.getProperty(u"glsl-shaders"_s).toList().isEmpty());
    QCOMPARE(controller.autoDowngraded(), false);
}

void TestQualityController::highTierAppliesQualityOptions()
{
    MpvEngine engine;
    QVERIFY(engine.initialize(QVariantMap{{u"scale"_s, u"spline36"_s}}));

    AppSettings settings(m_dir.filePath(u"high.json"_s));
    QualityController controller(&engine, &settings,
                                 m_dir.filePath(u"shaders"_s), &engine);
    controller.startup();
    controller.setQualityTier(u"high"_s);

    // No video loaded: the decision table still fixes scale/deband; no
    // shader group matches without media info, so none is mounted.
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"ewa_lanczossharp"_s);
    QCOMPARE(engine.getProperty(u"deband"_s).toBool(), true);
    QVERIFY(engine.getProperty(u"glsl-shaders"_s).toList().isEmpty());
}

void TestQualityController::offAfterActiveResetsToDefaults()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"off.json"_s));
    QualityController controller(&engine, &settings,
                                 m_dir.filePath(u"shaders"_s), &engine);
    controller.startup();
    controller.setQualityTier(u"high"_s);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"ewa_lanczossharp"_s);

    controller.setQualityTier(u"off"_s);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"lanczos"_s);
    QCOMPARE(engine.getProperty(u"deband"_s).toBool(), false);
    QCOMPARE(engine.getProperty(u"tone-mapping"_s).toString(), u"auto"_s);
    QCOMPARE(engine.getProperty(u"target-colorspace-hint"_s).toBool(), false);
}

void TestQualityController::manualShaderMountingPersists()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"manual.json"_s));
    QualityController controller(&engine, &settings,
                                 m_dir.filePath(u"shaders"_s), &engine);
    controller.startup();

    // Mounting works even with tier "off" (user-owned list), but the
    // engine's scalar options are left untouched (still inert).
    controller.setShaderMounted(u"a.glsl"_s, true);
    const QVariantList mounted =
        engine.getProperty(u"glsl-shaders"_s).toList();
    QCOMPARE(mounted.size(), 1);
    QCOMPARE(mounted.first().toString(), shaderA());
    QVERIFY(engine.getProperty(u"scale"_s).toString().isEmpty()
            || engine.getProperty(u"scale"_s).toString() == u"lanczos"_s);

    // The manual list is persisted.
    AppSettings reloaded(m_dir.filePath(u"manual.json"_s));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.manualShaderList(), (QStringList{shaderA()}));

    // Unmounting empties the list again.
    controller.setShaderMounted(u"a.glsl"_s, false);
    QVERIFY(engine.getProperty(u"glsl-shaders"_s).toList().isEmpty());
    AppSettings reloaded2(m_dir.filePath(u"manual.json"_s));
    QVERIFY(reloaded2.load());
    QVERIFY(reloaded2.manualShaderList().isEmpty());
}

void TestQualityController::startupAdoptsImportedShaders()
{
    // An imported glsl-shaders option (P4) is adopted into the manual list
    // on the first startup so the panel becomes the source of truth.
    MpvEngine engine;
    QVERIFY(engine.initialize(QVariantMap{
        {u"glsl-shaders"_s, shaderA()}}));

    AppSettings settings(m_dir.filePath(u"adopt.json"_s));
    QualityController controller(&engine, &settings,
                                 m_dir.filePath(u"shaders"_s), &engine);
    controller.startup();

    QCOMPARE(controller.manualShaders(), (QStringList{shaderA()}));
    AppSettings reloaded(m_dir.filePath(u"adopt.json"_s));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.manualShaderList(), (QStringList{shaderA()}));
    // Still mounted on the engine, untouched.
    QCOMPARE(engine.getProperty(u"glsl-shaders"_s).toList().size(), 1);
}

void TestQualityController::autoDowngradeDemotesActiveTierOnly()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"downgrade.json"_s));
    QualityController controller(&engine, &settings,
                                 m_dir.filePath(u"shaders"_s), &engine);
    controller.startup();
    controller.setQualityTier(u"high"_s);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"ewa_lanczossharp"_s);

    // Below the threshold nothing happens.
    controller.notifyDropFrameRate(0.5);
    QCOMPARE(controller.autoDowngraded(), false);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"ewa_lanczossharp"_s);

    // Heavy dropping demotes the ACTIVE tier to low, without touching the
    // persisted user tier.
    controller.notifyDropFrameRate(3.0);
    QCOMPARE(controller.autoDowngraded(), true);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"lanczos"_s);

    AppSettings reloaded(m_dir.filePath(u"downgrade.json"_s));
    QVERIFY(reloaded.load());
    QCOMPARE(reloaded.qualityTier(), u"high"_s);

    // Already demoted: further bursts are ignored (no flapping).
    controller.notifyDropFrameRate(9.0);
    QCOMPARE(controller.autoDowngraded(), true);

    // Off and low tiers never demote.
    AppSettings settings2(m_dir.filePath(u"downgrade2.json"_s));
    settings2.setQualityTier(u"low"_s);
    QualityController controller2(&engine, &settings2,
                                  m_dir.filePath(u"shaders"_s), &engine);
    controller2.setQualityTier(u"low"_s);
    controller2.notifyDropFrameRate(9.0);
    QCOMPARE(controller2.autoDowngraded(), false);
}

void TestQualityController::fileLoadedRestoresUserTier()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"restore.json"_s));
    QualityController controller(&engine, &settings,
                                 m_dir.filePath(u"shaders"_s), &engine);
    controller.startup();
    controller.setQualityTier(u"high"_s);
    controller.notifyDropFrameRate(3.0);
    QCOMPARE(controller.autoDowngraded(), true);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"lanczos"_s);

    // Signals are public methods in Qt; emit the engine's file-loaded
    // event directly to simulate the next file starting.
    emit engine.fileLoaded();

    QCOMPARE(controller.autoDowngraded(), false);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"ewa_lanczossharp"_s);
}

QTEST_GUILESS_MAIN(TestQualityController)
#include "test_quality_controller.moc"
