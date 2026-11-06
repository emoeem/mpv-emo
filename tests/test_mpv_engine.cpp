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
    void doubleInitializeRejected();
    void propertyRoundTrip();
    void observedPropertySignals();
    void commandErrorEmitsErrorSignal();
};

void TestMpvEngine::initializeSmoke()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());
    QVERIFY(engine.handle() != nullptr);
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

QTEST_GUILESS_MAIN(TestMpvEngine)
#include "test_mpv_engine.moc"
