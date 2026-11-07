#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "music/LrcParser.h"

class MpvEngine;

// Music-mode service (P7 v1). Patterned after HistoryStore/QualityController:
// a passive QObject driven by engine signals plus on-demand property reads
// (docs/architecture.md decision 7 — no cached play state). Owns no mpv
// state of its own: it decides when the player is effectively a music
// player, resolves/parses the .lrc next to the audio file and mirrors the
// current lyric line from time-pos updates.
//
// Responsibilities (this commit): audio detection + lyric tracking.
//
//   - audio detection: the current media is "music mode" material when the
//     track-list has no video track AND the media filename has an audio
//     extension. Pure rules in isAudioMedia()/isAudioExtension() so the
//     tests run without an engine. Cover art attached to audio files shows
//     up as a video track with the "albumart" flag (mpv 0.37 input.rst
//     track-list/N/albumart, MPV_FORMAT_FLAG) — those tracks do NOT
//     disqualify, or every mp3/flac with an embedded cover would be
//     classified as video.
//   - lyrics: resolves the same-name .lrc next to the audio file
//     (LrcParser::lrcCandidates), parses it and exposes the lines as a
//     QVariantList plus the current line index, recomputed on each
//     positionChanged from the engine.
class MusicController : public QObject
{
    Q_OBJECT
    // True while the current media is pure audio; the QML loader shows the
    // music page when this is set.
    Q_PROPERTY(bool audioMode READ audioMode NOTIFY audioModeChanged)
    // Parsed lyrics as [{time: double seconds, text: QString}]; empty when
    // none loaded (no .lrc next to the file, or not audio).
    Q_PROPERTY(QVariantList lyrics READ lyrics NOTIFY lyricsChanged)
    // Index of the line at the current time-pos; -1 before the first line.
    Q_PROPERTY(int lyricIndex READ lyricIndex NOTIFY lyricIndexChanged)
    // [ti]/[ar]/[al] values of the loaded .lrc (missing keys absent).
    Q_PROPERTY(QVariantMap lrcMetadata READ lrcMetadata NOTIFY lyricsChanged)
    // Relay of mpv's "metadata" property for the music page (fallback
    // artist/album display when the .lrc has no tags).
    Q_PROPERTY(QVariantMap mediaMetadata READ mediaMetadata
                   NOTIFY mediaMetadataChanged)

public:
    // |engine| may be null in tests (then nothing is observed/read).
    explicit MusicController(MpvEngine *engine, QObject *parent = nullptr);

    bool audioMode() const { return m_audioMode; }
    QVariantList lyrics() const { return m_lyrics; }
    int lyricIndex() const { return m_lyricIndex; }
    QVariantMap lrcMetadata() const { return m_lrcMetadata; }
    QVariantMap mediaMetadata() const { return m_mediaMetadata; }

    // Pure decision rules (static so unit tests run without an engine):
    // media counts as pure audio when no track-list entry is a (non-cover)
    // video track and the filename has a known audio extension.
    static bool isAudioExtension(const QString &filename);
    static bool isAudioMedia(const QVariantList &tracks, const QString &filename);
    // Index of the line playing at |seconds|: the last line whose time is
    // <= seconds; -1 before the first line (and for empty input).
    static int lyricIndexAt(const QVector<LrcParser::LrcLine> &lines,
                            double seconds);

public slots:
    // Re-runs detection + lyric resolution for the current media (also
    // connected to fileLoaded / idleActiveChanged).
    void reevaluate();

signals:
    void audioModeChanged(bool audioMode);
    void lyricsChanged();
    void lyricIndexChanged(int index);
    void mediaMetadataChanged(const QVariantMap &metadata);

private slots:
    void onPositionChanged(double seconds);
    void onMetadataChanged(const QVariantMap &metadata);

private:
    void setAudioMode(bool mode);
    void loadLyrics(const QString &path);
    void clearLyrics();
    void setLyricIndex(int index);

    MpvEngine *m_engine = nullptr;
    bool m_audioMode = false;
    QVariantList m_lyrics;
    QVector<LrcParser::LrcLine> m_lines;
    int m_lyricIndex = -1;
    QVariantMap m_lrcMetadata;
    QVariantMap m_mediaMetadata;
};
