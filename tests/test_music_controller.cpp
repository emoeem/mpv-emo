#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

#include <algorithm>

#include "mpv/MpvEngine.h"
#include "music/MusicController.h"
#include "settings/AppSettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Minimal track-list entry for the isAudioMedia() cases: {type: "audio"|
// "video"|"sub"}, plus the optional albumart flag (mpv exposes embedded
// cover art as a video track with albumart=yes).
QVariantMap track(const QString &type, bool albumart = false)
{
    QVariantMap entry;
    entry.insert(u"type"_s, type);
    if (albumart)
        entry.insert(u"albumart"_s, true);
    return entry;
}

} // namespace

// Pure-logic tests for the music-mode decision rules (no engine) plus the
// equalizer mapping. The EQ "live engine" cases drive a real idle libmpv
// core (headless), so they double as the runtime verification that the
// written af chain is accepted by mpv 0.37 / the linked libavfilter.
class TestMusicController : public QObject
{
    Q_OBJECT

private slots:
    void audioExtensionRules();
    void audioMediaDetection();
    void coverArtTrackDoesNotDisqualify();
    void lyricIndexBasics();
    void lyricIndexExactAndPastEnd();
    void nullEngineStaysIdle();
    // Equalizer pure helpers.
    void equalizerChainAllZeroIsEmpty();
    void equalizerChainFormat();
    void equalizerChainClampsGains();
    void presetTables();
    // Equalizer with a real engine + persistence.
    void initTestCase();
    void equalizerAppliesToEngine();
    void equalizerDisableClearsChain();
    void equalizerPersistedAcrossInstances();

private:
    QTemporaryDir m_dir;
};

void TestMusicController::audioExtensionRules()
{
    QVERIFY(MusicController::isAudioExtension(u"song.mp3"_s));
    QVERIFY(MusicController::isAudioExtension(u"SONG.MP3"_s));
    QVERIFY(MusicController::isAudioExtension(u"/music/a/song.FLAC"_s));
    QVERIFY(MusicController::isAudioExtension(u"song.opus"_s));
    QVERIFY(!MusicController::isAudioExtension(u"movie.mkv"_s));
    QVERIFY(!MusicController::isAudioExtension(u"song.mp4"_s));
    QVERIFY(!MusicController::isAudioExtension(u"noextension"_s));
    QVERIFY(!MusicController::isAudioExtension(QString()));
}

void TestMusicController::audioMediaDetection()
{
    // Audio track list + audio extension -> music mode.
    QVERIFY(MusicController::isAudioMedia(
        {track(u"audio"_s)}, u"song.mp3"_s));
    // A real video track disqualifies, even with an audio extension.
    QVERIFY(!MusicController::isAudioMedia(
        {track(u"audio"_s), track(u"video"_s)}, u"song.mp3"_s));
    QVERIFY(!MusicController::isAudioMedia(
        {track(u"video"_s)}, u"movie.mkv"_s));
    // Sub/lyric tracks neither qualify nor disqualify.
    QVERIFY(MusicController::isAudioMedia(
        {track(u"audio"_s), track(u"sub"_s)}, u"song.flac"_s));
    // Audio extension but empty track list (file still probing): the rule
    // is "no video AND audio extension", so this qualifies.
    QVERIFY(MusicController::isAudioMedia({}, u"song.mp3"_s));
    // Video container without tracks: no video track, but the extension
    // gate keeps it out of music mode.
    QVERIFY(!MusicController::isAudioMedia({}, u"movie.mkv"_s));
}

void TestMusicController::coverArtTrackDoesNotDisqualify()
{
    // mpv exposes an embedded cover as a video track with albumart=yes
    // (input.rst track-list/N/albumart); common on mp3/flac downloads.
    QVERIFY(MusicController::isAudioMedia(
        {track(u"audio"_s), track(u"video", true)}, u"song.mp3"_s));
    // A real video next to the cover still disqualifies.
    QVERIFY(!MusicController::isAudioMedia(
        {track(u"video", true), track(u"video", false)}, u"song.mp3"_s));
}

void TestMusicController::lyricIndexBasics()
{
    const QVector<LrcLine> lines = {
        {10.0, u"one"_s}, {20.0, u"two"_s}, {30.0, u"three"_s},
    };
    QCOMPARE(MusicController::lyricIndexAt(lines, 0.0), -1);
    QCOMPARE(MusicController::lyricIndexAt(lines, 9.999), -1);
    QCOMPARE(MusicController::lyricIndexAt(lines, 10.0), 0);
    QCOMPARE(MusicController::lyricIndexAt(lines, 19.0), 0);
    QCOMPARE(MusicController::lyricIndexAt(lines, 20.0), 1);
    QCOMPARE(MusicController::lyricIndexAt(lines, 999.0), 2);
    QCOMPARE(MusicController::lyricIndexAt({}, 10.0), -1);
}

void TestMusicController::lyricIndexExactAndPastEnd()
{
    // Exact hits count (epsilon guard against double rounding), and an
    // empty line at a timestamp is still an indexable line.
    const QVector<LrcLine> lines = {{5.0, QString()}, {5.0, u"repeat"_s}};
    QCOMPARE(MusicController::lyricIndexAt(lines, 4.999999), -1);
    QCOMPARE(MusicController::lyricIndexAt(lines, 5.0), 1);
}

void TestMusicController::nullEngineStaysIdle()
{
    // Controller without an engine: nothing may crash and audioMode stays
    // false after a reevaluate().
    MusicController controller(nullptr, nullptr);
    controller.reevaluate();
    QCOMPARE(controller.audioMode(), false);
    QVERIFY(controller.lyrics().isEmpty());
    QCOMPARE(controller.lyricIndex(), -1);
}

void TestMusicController::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QVERIFY(m_dir.isValid());
}

void TestMusicController::equalizerChainAllZeroIsEmpty()
{
    // All-zero gains mount nothing (inert, like the P5 tier=off rule).
    QCOMPARE(MusicController::equalizerChain(
                 QVector<double>(10, 0.0)),
             QString());
    // Wrong band count is refused outright.
    QCOMPARE(MusicController::equalizerChain(QVector<double>(3, 1.0)),
             QString());
}

void TestMusicController::equalizerChainFormat()
{
    // One peaking "equalizer" filter per band, Q-factor width 1.4
    // (one octave), comma-joined in band order. Verified against
    // FFmpeg n6.1 libavfilter/af_biquads.c (f/t/w/g options).
    QVector<double> gains(10, 0.0);
    gains[0] = 7.0;
    gains[9] = -2.0;
    QCOMPARE(MusicController::equalizerChain(gains),
             u"equalizer=f=31.25:t=q:w=1.4:g=7,"
             u"equalizer=f=62.5:t=q:w=1.4:g=0,"
             u"equalizer=f=125:t=q:w=1.4:g=0,"
             u"equalizer=f=250:t=q:w=1.4:g=0,"
             u"equalizer=f=500:t=q:w=1.4:g=0,"
             u"equalizer=f=1000:t=q:w=1.4:g=0,"
             u"equalizer=f=2000:t=q:w=1.4:g=0,"
             u"equalizer=f=4000:t=q:w=1.4:g=0,"
             u"equalizer=f=8000:t=q:w=1.4:g=0,"
             u"equalizer=f=16000:t=q:w=1.4:g=-2"_s);
    QCOMPARE(MusicController::bandFrequencies().size(), 10);
    QCOMPARE(MusicController::bandFrequencies().first(), 31.25);
    QCOMPARE(MusicController::bandFrequencies().last(), 16000.0);
}

void TestMusicController::equalizerChainClampsGains()
{
    // Out-of-range gains clamp to the slider range (±12 dB).
    QVector<double> gains(10, 0.0);
    gains[0] = 30.0;
    gains[1] = -30.0;
    const QString chain = MusicController::equalizerChain(gains);
    QVERIFY(chain.contains(u"equalizer=f=31.25:t=q:w=1.4:g=12,"_s));
    QVERIFY(chain.contains(u"equalizer=f=62.5:t=q:w=1.4:g=-12,"_s));
}

void TestMusicController::presetTables()
{
    QCOMPARE(MusicController::presetNames(),
             (QStringList{u"flat"_s, u"bass"_s, u"vocal"_s, u"live"_s}));

    const QVector<double> flat = MusicController::presetGains(u"flat"_s);
    QCOMPARE(flat.size(), 10);
    QVERIFY(std::all_of(flat.cbegin(), flat.cend(),
                        [](double g) { return g == 0.0; }));

    const QVector<double> bass = MusicController::presetGains(u"bass"_s);
    QCOMPARE(bass, (QVector<double>{7, 6, 5, 3, 1, 0, 0, 0, 0, 0}));
    const QVector<double> vocal = MusicController::presetGains(u"vocal"_s);
    QCOMPARE(vocal, (QVector<double>{-1, 0, 1, 3, 4, 4, 3, 1, 0, -1}));
    const QVector<double> live = MusicController::presetGains(u"live"_s);
    QCOMPARE(live, (QVector<double>{5, 4, 2, 0, -1, 0, 2, 4, 5, 5}));

    // Unknown preset names fall back to flat instead of crashing.
    QCOMPARE(MusicController::presetGains(u"nope"_s), flat);
}

void TestMusicController::equalizerAppliesToEngine()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"eq-on.json"_s));
    MusicController controller(&engine, &settings);
    // Startup with disabled EQ must stay inert (nothing touches af).
    controller.startup();
    QVERIFY(engine.getProperty(u"af"_s).toList().isEmpty());

    controller.setEqualizerEnabled(true);
    // Flat gains: enabled but no audible band -> still no filter mounted.
    QVERIFY(engine.getProperty(u"af"_s).toList().isEmpty());

    controller.applyEqualizerPreset(u"bass"_s);
    QCOMPARE(controller.equalizerPreset(), u"bass"_s);

    // The chain is accepted by mpv 0.37: reading the af property back
    // yields one {name, params} entry per band (m_option.c serializes
    // OPT_SETTINGSLIST as name/label/enabled/params maps). This is the
    // runtime proof that the "equalizer" filter name and its f/t/w/g
    // parameter format parse on the linked libavfilter.
    const QVariantList af = engine.getProperty(u"af"_s).toList();
    QCOMPARE(af.size(), 10);
    QCOMPARE(af.first().toMap().value(u"name"_s).toString(), u"equalizer"_s);
    const QVariantMap params = af.first().toMap().value(u"params"_s).toMap();
    QCOMPARE(params.value(u"f"_s).toString(), u"31.25"_s);
    QCOMPARE(params.value(u"g"_s).toString(), u"7"_s);

    // Single-band user adjustment: value lands in the engine and flips the
    // preset to custom.
    controller.setEqualizerBand(3, -5);
    QCOMPARE(controller.equalizerPreset(), u"custom"_s);
    const QVariantList afAfterBand =
        engine.getProperty(u"af"_s).toList();
    QCOMPARE(afAfterBand.size(), 10);
    QCOMPARE(afAfterBand[3].toMap().value(u"params"_s).toMap()
                 .value(u"g"_s).toString(),
             u"-5"_s);

    // Out-of-range band writes are clamped, unknown bands ignored.
    controller.setEqualizerBand(0, 99);
    QCOMPARE(controller.equalizerGains().at(0).toDouble(), 12.0);
    controller.setEqualizerBand(-1, 5);
    controller.setEqualizerBand(10, 5);
    QCOMPARE(controller.equalizerGains().size(), 10);
}

void TestMusicController::equalizerDisableClearsChain()
{
    MpvEngine engine;
    QVERIFY(engine.initialize());

    AppSettings settings(m_dir.filePath(u"eq-off.json"_s));
    MusicController controller(&engine, &settings);
    controller.setEqualizerEnabled(true);
    controller.applyEqualizerPreset(u"vocal"_s);
    QCOMPARE(engine.getProperty(u"af"_s).toList().size(), 10);

    controller.setEqualizerEnabled(false);
    QVERIFY(engine.getProperty(u"af"_s).toList().isEmpty());

    // Re-enabling re-mounts the last (custom-persisted) gains.
    controller.setEqualizerEnabled(true);
    QCOMPARE(engine.getProperty(u"af"_s).toList().size(), 10);
}

void TestMusicController::equalizerPersistedAcrossInstances()
{
    const QString file = m_dir.filePath(u"eq-persist.json"_s);
    {
        MpvEngine engine;
        QVERIFY(engine.initialize());
        AppSettings settings(file);
        MusicController controller(&engine, &settings);
        controller.setEqualizerEnabled(true);
        controller.applyEqualizerPreset(u"bass"_s);
        controller.setEqualizerBand(2, 4);
        QCOMPARE(engine.getProperty(u"af"_s).toList().size(), 10);
    }
    // A fresh instance restores the persisted state and remounts it on
    // startup() (enabled -> chain present without any further call).
    MpvEngine engine;
    QVERIFY(engine.initialize());
    AppSettings settings(file);
    MusicController controller(&engine, &settings);
    QCOMPARE(controller.equalizerEnabled(), true);
    QCOMPARE(controller.equalizerPreset(), u"custom"_s);
    QCOMPARE(controller.equalizerGains().at(2).toDouble(), 4.0);
    controller.startup();
    const QVariantList af = engine.getProperty(u"af"_s).toList();
    QCOMPARE(af.size(), 10);
    QCOMPARE(af[2].toMap().value(u"params"_s).toMap().value(u"g"_s).toString(),
             u"4"_s);

    // Disabled state persists as inert: a fresh session never touches af.
    {
        controller.setEqualizerEnabled(false);
    }
    MpvEngine engine2;
    QVERIFY(engine2.initialize());
    AppSettings settings2(file);
    MusicController controller2(&engine2, &settings2);
    QCOMPARE(controller2.equalizerEnabled(), false);
    controller2.startup();
    QVERIFY(engine2.getProperty(u"af"_s).toList().isEmpty());
}

QTEST_MAIN(TestMusicController)
#include "test_music_controller.moc"
