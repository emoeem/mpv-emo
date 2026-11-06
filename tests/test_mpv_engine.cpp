#include <QSignalSpy>
#include <QtTest>

#include "mpv/MpvEngine.h"

using namespace Qt::Literals::StringLiterals;

// Exercises the real libmpv core headless (vo=libmpv, no file playback):
// initialization, property round-trips, observed-property signal delivery
// through the wakeup callback, and command error reporting.
class TestMpvEngine : public QObject
{
    Q_OBJECT

private slots:
    void initializeSmoke();
    void scriptAutoloadDisabledByDefault();
    void doubleInitializeRejected();
    void propertyRoundTrip();
    void observedPropertySignals();
    void unavailablePropertiesEmitClearedValues();
    void commandErrorEmitsErrorSignal();
};

void TestMpvEngine::initializeSmoke()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());
    QVERIFY(engine.handle() != nullptr);
}

void TestMpvEngine::scriptAutoloadDisabledByDefault()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());
    // load-scripts defaults to yes in mpv (options.c) and libmpv's builtin
    // profile does not disable it, so it must be forced off here: scripts
    // enter only via the compat controller's explicit "scripts" list.
    QCOMPARE(engine.getProperty(u"load-scripts"_s).toBool(), false);
}

void TestMpvEngine::doubleInitializeRejected()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());
    QVERIFY(!engine.initialize());
    QCOMPARE(engine.lastError(), u"Engine already initialized"_s);
}

void TestMpvEngine::propertyRoundTrip()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    engine.setProperty(u"volume"_s, 42.0);
    QCOMPARE(engine.getProperty(u"volume"_s).toDouble(), 42.0);

    engine.setProperty(u"pause"_s, true);
    QCOMPARE(engine.getProperty(u"pause"_s).toBool(), true);

    engine.setProperty(u"speed"_s, 1.5);
    QCOMPARE(engine.getProperty(u"speed"_s).toDouble(), 1.5);

    // String-valued property sets go through the MPV_FORMAT_STRING path,
    // which expects the address of a char* (regression: passing the char*
    // itself crashed inside mpv when the quality engine applied options).
    engine.setProperty(u"tone-mapping"_s, u"bt.2390"_s);
    QCOMPARE(engine.getProperty(u"tone-mapping"_s).toString(), u"bt.2390"_s);
    engine.setProperty(u"scale"_s, u"ewa_lanczossharp"_s);
    QCOMPARE(engine.getProperty(u"scale"_s).toString(), u"ewa_lanczossharp"_s);
}

void TestMpvEngine::observedPropertySignals()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    QSignalSpy pausedSpy(&engine, &MpvEngine::pausedChanged);
    QSignalSpy volumeSpy(&engine, &MpvEngine::volumeChanged);
    QVERIFY(pausedSpy.isValid());
    QVERIFY(volumeSpy.isValid());

    // Let the initial observed-property events flush before we assert on
    // events caused by our own property writes.
    QTest::qWait(200);
    pausedSpy.clear();
    volumeSpy.clear();

    engine.setProperty(u"pause"_s, true);
    QTRY_VERIFY(pausedSpy.count() >= 1);
    QCOMPARE(pausedSpy.last().at(0).toBool(), true);

    engine.setProperty(u"volume"_s, 42.0);
    QTRY_VERIFY(volumeSpy.count() >= 1);
    QCOMPARE(volumeSpy.last().at(0).toDouble(), 42.0);
}

void TestMpvEngine::commandErrorEmitsErrorSignal()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    QSignalSpy errorSpy(&engine, &MpvEngine::errorOccurred);
    QVERIFY(errorSpy.isValid());

    engine.command({u"this-command-does-not-exist"_s});
    QTRY_COMPARE(errorSpy.count(), 1);
    QVERIFY(errorSpy.at(0).at(0).toString().contains(u"failed"_s));
}

void TestMpvEngine::unavailablePropertiesEmitClearedValues()
{
    // mpv always sends an initial change notification per observed
    // property; on an idle engine the P5 properties are unavailable and
    // arrive as MPV_FORMAT_NONE with an invalid data pointer. The engine
    // must turn those into cleared values instead of dropping them, or
    // stale mirrors survive across files.
    MpvEngine engine;
    QVERIFY(engine.initialize());

    QSignalSpy paramsSpy(&engine, &MpvEngine::videoParamsChanged);
    QSignalSpy fpsSpy(&engine, &MpvEngine::containerFpsChanged);
    QSignalSpy dropsSpy(&engine, &MpvEngine::frameDropCountChanged);
    QSignalSpy voSpy(&engine, &MpvEngine::currentVoChanged);
    QSignalSpy formatSpy(&engine, &MpvEngine::videoFormatChanged);
    QVERIFY(paramsSpy.isValid() && fpsSpy.isValid() && dropsSpy.isValid()
            && voSpy.isValid() && formatSpy.isValid());

    // The notifications arrive through the wakeup pump; QTRY_* spins the
    // event loop so the queued handleEvents() runs.
    QTRY_VERIFY(!paramsSpy.isEmpty());
    QCOMPARE(paramsSpy.last().first().toMap(), QVariantMap{});
    QTRY_VERIFY(!fpsSpy.isEmpty());
    QCOMPARE(fpsSpy.last().first().toDouble(), 0.0);
    QTRY_VERIFY(!dropsSpy.isEmpty());
    QCOMPARE(dropsSpy.last().first().toLongLong(), 0);
    QTRY_VERIFY(!voSpy.isEmpty());
    QCOMPARE(voSpy.last().first().toString(), QString());
    QTRY_VERIFY(!formatSpy.isEmpty());
    QCOMPARE(formatSpy.last().first().toString(), QString());
}

QTEST_GUILESS_MAIN(TestMpvEngine)
#include "test_mpv_engine.moc"
