#include <QtTest>

#include "music/MusicController.h"

using namespace Qt::Literals::StringLiterals;

// Pure-logic tests for the music-mode decision rules. These run without an
// engine (the controller itself tolerates a null engine); the equalizer's
// live-engine coverage is in the P7 equalizer commit.
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

private:
    static QVariantMap track(const QString &type, bool albumart = false)
    {
        QVariantMap track;
        track.insert(u"type"_s, type);
        if (albumart)
            track.insert(u"albumart"_s, true);
        return track;
    }
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
    MusicController controller(nullptr);
    controller.reevaluate();
    QCOMPARE(controller.audioMode(), false);
    QVERIFY(controller.lyrics().isEmpty());
    QCOMPARE(controller.lyricIndex(), -1);
}

QTEST_MAIN(TestMusicController)
#include "test_music_controller.moc"
