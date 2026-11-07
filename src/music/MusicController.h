#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

#include "music/LrcParser.h"

class AppSettings;
class MpvEngine;

// Music-mode service (P7 v1). Patterned after HistoryStore/QualityController:
// a passive QObject driven by engine signals plus on-demand property reads
// (docs/architecture.md decision 7 — no cached play state). Owns no mpv
// state of its own: it decides when the player is effectively a music
// player, resolves/parses the .lrc next to the audio file, mirrors the
// current lyric line from time-pos updates, and manages the 10-band
// equalizer (mpv af chain).
//
// Responsibilities:
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
//   - equalizer v1: 10 fixed bands -> mpv's af option as a chain of peaking
//     "equalizer" filters; built-in presets plus user values persisted in
//     AppSettings' "music" section (equalizerEnabled/equalizerPreset/
//     equalizerGains).
//
// mpv 0.37 verification (P5 convention — written down where it decides
// code): v0.37.0 has no builtin superequalizer/equalizer audio filter —
// filters/user_filters.c registers only af_lavfi and af_lavfi_bridge, and
// every other name routes through the libavfilter bridge
// (check_unknown_entry -> mp_lavfi_is_usable), so filter availability
// follows the linked libavfilter. FFmpeg n6.1's "superequalizer" (the name
// behind the equalizer-gui upstream) has 18 FIXED bands (65.4 Hz..20 kHz,
// options "1b".."18b") with gains clamped to 0..20 — no negative gain, no
// "bands=" parameter, and a fixed 18-band grid, a poor fit for a 10-band
// ±12 dB UI. The FFmpeg "equalizer" filter (libavfilter/af_biquads.c)
// instead takes f (frequency, double), t (width_type, default q), w (width,
// double, default 1.0) and g (gain, double, -900..900), which maps 1:1 to a
// UI band. Hence the chain form used here:
//     equalizer=f=31.25:t=q:w=1.4:g=2,equalizer=f=62.5:...
// w=1.4 ≈ √2 is the Q-factor for one-octave-spaced bands.
//
// Known v1 limitations: writing af REPLACES the whole chain, so an af
// imported from the user's mpv.conf (P4) is clobbered while the EQ is
// enabled (sole-manager semantics, same as glsl-shaders in P5); each
// integer-dB slider step rebuilds the chain (no debounce); bitstream
// passthrough interlock (nativeization-plan.md) is not in v1 — no
// passthrough UI exists yet.
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

    // Equalizer v1 (see the class comment for the af mapping). All three
    // change together on any EQ mutation; QML syncs imperatively.
    Q_PROPERTY(bool equalizerEnabled READ equalizerEnabled
                   NOTIFY equalizerChanged)
    // "flat"/"bass"/"vocal"/"live", or "custom" once the user moved a band.
    Q_PROPERTY(QString equalizerPreset READ equalizerPreset
                   NOTIFY equalizerChanged)
    // 10 per-band gains in dB, clamped to ±12.
    Q_PROPERTY(QVariantList equalizerGains READ equalizerGains
                   NOTIFY equalizerChanged)
    // Constants for the QML page (QML cannot read C++ static constexprs).
    Q_PROPERTY(int bandCount READ bandCount CONSTANT)
    Q_PROPERTY(double maxGain READ maxGain CONSTANT)

public:
    // |settings| must outlive this controller; |engine| may be null in
    // tests (then nothing is observed/read/applied).
    explicit MusicController(MpvEngine *engine, AppSettings *settings,
                             QObject *parent = nullptr);

    bool audioMode() const { return m_audioMode; }
    QVariantList lyrics() const { return m_lyrics; }
    int lyricIndex() const { return m_lyricIndex; }
    QVariantMap lrcMetadata() const { return m_lrcMetadata; }
    QVariantMap mediaMetadata() const { return m_mediaMetadata; }
    bool equalizerEnabled() const { return m_equalizerEnabled; }
    QString equalizerPreset() const { return m_equalizerPreset; }
    QVariantList equalizerGains() const;
    int bandCount() const { return kBandCount; }
    double maxGain() const { return kMaxGain; }

    // Pure decision rules (static so unit tests run without an engine):
    // media counts as pure audio when no track-list entry is a (non-cover)
    // video track and the filename has a known audio extension.
    static bool isAudioExtension(const QString &filename);
    static bool isAudioMedia(const QVariantList &tracks, const QString &filename);
    // Index of the line playing at |seconds|: the last line whose time is
    // <= seconds; -1 before the first line (and for empty input).
    static int lyricIndexAt(const QVector<LrcLine> &lines,
                            double seconds);

    // Equalizer pure helpers (unit tested without an engine).
    // The 10 fixed band frequencies (ISO one-octave grid), Hz.
    static const QVector<double> &bandFrequencies();
    // mpv af chain for |gains| (one "equalizer" filter per band); empty
    // string when every gain is zero (no filter mounted). Gains are
    // clamped to ±12 dB. Anything not 10 bands yields "".
    static QString equalizerChain(const QVector<double> &gains);
    // Built-in preset gains; unknown names fall back to "flat".
    static QVector<double> presetGains(const QString &preset);
    static const QStringList &presetNames();

    // Clamped slider range / band count, shared with the QML page.
    static constexpr int kBandCount = 10;
    static constexpr double kMaxGain = 12.0;

public slots:
    // Re-runs detection + lyric resolution for the current media (also
    // connected to fileLoaded / idleActiveChanged).
    void reevaluate();
    // Startup seeding: mount the persisted EQ chain when it was enabled
    // (never touches af when disabled — imported options stay untouched).
    void startup();
    void setEqualizerEnabled(bool enabled);
    // User moved one band slider (0..9, dB value): switches to "custom",
    // persists and re-applies when enabled.
    void setEqualizerBand(int band, double gain);
    // Apply a built-in preset by name (unknown names are ignored).
    void applyEqualizerPreset(const QString &preset);

signals:
    void audioModeChanged(bool audioMode);
    void lyricsChanged();
    void lyricIndexChanged(int index);
    void mediaMetadataChanged(const QVariantMap &metadata);
    void equalizerChanged();

private slots:
    void onPositionChanged(double seconds);
    void onMetadataChanged(const QVariantMap &metadata);

private:
    void setAudioMode(bool mode);
    void loadLyrics(const QString &path);
    void clearLyrics();
    void setLyricIndex(int index);
    void loadPersistedEqualizer();
    void persistEqualizer();
    void applyEqualizerToEngine();

    MpvEngine *m_engine = nullptr;
    AppSettings *m_settings = nullptr;
    bool m_audioMode = false;
    QVariantList m_lyrics;
    QVector<LrcLine> m_lines;
    int m_lyricIndex = -1;
    QVariantMap m_lrcMetadata;
    // The .lrc currently parsed and exposed (first existing candidate of
    // the playing file); repeated reevaluate() runs skip re-parsing it.
    QString m_lrcPath;
    QVariantMap m_mediaMetadata;
    bool m_equalizerEnabled = false;
    QString m_equalizerPreset = QStringLiteral("flat");
    QVector<double> m_gains = QVector<double>(kBandCount, 0.0);
};
