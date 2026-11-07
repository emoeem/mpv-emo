#include "music/MusicController.h"

#include <QFile>
#include <QUrl>

#include <algorithm>
#include <cmath>

#include "mpv/MpvEngine.h"
#include "settings/AppSettings.h"

using namespace Qt::Literals::StringLiterals;

namespace {

// Extensions treated as audio-only media (music mode candidates). Case is
// ignored; the list mirrors the player's common audio formats plus the
// mainstream lossless/DSD ones. Video containers (mkv/mp4/...) are
// deliberately absent: a video file must never enter music mode, whatever
// the extension game is.
const QStringList &audioExtensions()
{
    static const QStringList extensions {
        u"mp3"_s,  u"flac"_s, u"m4a"_s, u"aac"_s,  u"ogg"_s, u"oga"_s,
        u"opus"_s, u"wav"_s,  u"wma"_s, u"ape"_s,  u"mka"_s, u"dsf"_s,
        u"dff"_s,  u"aiff"_s, u"aif"_s, u"mpc"_s,  u"tak"_s, u"tta"_s,
        u"alac"_s, u"au"_s,   u"ra"_s,  u"amr"_s,
    };
    return extensions;
}

QString localMediaPath(const QVariant &property)
{
    // getProperty("path") returns a local path for files and a URL string
    // for network streams; normalize file:// the same way as loadFile().
    return MpvEngine::toMediaPath(property.toString());
}

// Equalizer preset tables, one gain per band (dB), ordered like
// bandFrequencies(): 31.25 Hz .. 16 kHz. Integer dB keeps the persisted
// values identical to what the sliders show.
const QVector<double> &flatGains()
{
    static const QVector<double> gains(MusicController::kBandCount, 0.0);
    return gains;
}

const QVector<double> &bassGains()
{
    static const QVector<double> gains {7, 6, 5, 3, 1, 0, 0, 0, 0, 0};
    return gains;
}

const QVector<double> &vocalGains()
{
    static const QVector<double> gains {-1, 0, 1, 3, 4, 4, 3, 1, 0, -1};
    return gains;
}

const QVector<double> &liveGains()
{
    static const QVector<double> gains {5, 4, 2, 0, -1, 0, 2, 4, 5, 5};
    return gains;
}

} // namespace

MusicController::MusicController(MpvEngine *engine, AppSettings *settings,
                                 QObject *parent)
    : QObject(parent), m_engine(engine), m_settings(settings)
{
    if (m_engine) {
        connect(m_engine, &MpvEngine::fileLoaded, this,
                &MusicController::reevaluate);
        connect(m_engine, &MpvEngine::idleActiveChanged, this,
                &MusicController::reevaluate);
        connect(m_engine, &MpvEngine::trackListChanged, this,
                &MusicController::reevaluate);
        connect(m_engine, &MpvEngine::positionChanged, this,
                &MusicController::onPositionChanged);
        connect(m_engine, &MpvEngine::metadataChanged, this,
                &MusicController::onMetadataChanged);
    }
    loadPersistedEqualizer();
}

bool MusicController::isAudioExtension(const QString &filename)
{
    const int dot = filename.lastIndexOf(QLatin1Char('.'));
    if (dot < 0)
        return false;
    return audioExtensions().contains(filename.mid(dot + 1).toLower());
}

bool MusicController::isAudioMedia(const QVariantList &tracks,
                                   const QString &filename)
{
    for (const QVariant &entry : tracks) {
        const QVariantMap track = entry.toMap();
        if (track.value(u"type"_s).toString() != QLatin1String("video"))
            continue;
        // Attached cover art is exposed as a video track with albumart=yes
        // (mpv 0.37 input.rst track-list/N/albumart). Audio files with an
        // embedded cover are still pure audio for music-mode purposes.
        if (track.value(u"albumart"_s).toBool())
            continue;
        return false;
    }
    return isAudioExtension(filename);
}

int MusicController::lyricIndexAt(const QVector<LrcLine> &lines,
                                  double seconds)
{
    int index = -1;
    for (int i = 0; i < lines.size(); ++i) {
        // Tiny epsilon so exact hits still count after double rounding
        // (a parsed ".33" and mpv's time-pos can differ in the last bits).
        // It must stay far below the 1 ms .lrc granularity: 4.999999 s
        // (one microsecond before a 5 s line) is not "at" the line.
        if (lines[i].time <= seconds + 0.000000001)
            index = i;
        else
            break; // sorted by time; nothing later can match either
    }
    return index;
}

void MusicController::reevaluate()
{
    if (!m_engine) {
        setAudioMode(false);
        clearLyrics();
        return;
    }

    const QVariantList tracks = m_engine->getProperty(u"track-list"_s).toList();
    const QString path = localMediaPath(m_engine->getProperty(u"path"_s));
    const bool audio = isAudioMedia(tracks, path);
    setAudioMode(audio);

    if (audio)
        loadLyrics(path);
    else
        clearLyrics();
}

void MusicController::setAudioMode(bool mode)
{
    if (m_audioMode == mode)
        return;
    m_audioMode = mode;
    emit audioModeChanged(m_audioMode);
}

void MusicController::loadLyrics(const QString &path)
{
    for (const QString &candidate : LrcParser::lrcCandidates(path)) {
        if (!QFile::exists(candidate))
            continue;
        const LrcParser::LrcData data = LrcParser::parseFile(candidate);
        QVariantMap metadata;
        if (!data.title.isEmpty())
            metadata.insert(u"title"_s, data.title);
        if (!data.artist.isEmpty())
            metadata.insert(u"artist"_s, data.artist);
        if (!data.album.isEmpty())
            metadata.insert(u"album"_s, data.album);

        QVariantList lines;
        lines.reserve(data.lines.size());
        for (const LrcLine &line : data.lines) {
            lines.append(QVariantMap{
                {u"time"_s, line.time},
                {u"text"_s, line.text},
            });
        }

        m_lines = data.lines;
        m_lyrics = lines;
        m_lrcMetadata = metadata;
        // Playback position did not change, but the line set may differ.
        setLyricIndex(lyricIndexAt(m_lines,
                                   m_engine ? m_engine->getProperty(u"time-pos"_s).toDouble()
                                            : 0.0));
        emit lyricsChanged();
        return; // first existing candidate wins
    }
    clearLyrics();
}

void MusicController::clearLyrics()
{
    if (m_lyrics.isEmpty() && m_lines.isEmpty() && m_lrcMetadata.isEmpty())
        return;
    m_lines.clear();
    m_lyrics.clear();
    m_lrcMetadata.clear();
    setLyricIndex(-1);
    emit lyricsChanged();
}

void MusicController::setLyricIndex(int index)
{
    if (m_lyricIndex == index)
        return;
    m_lyricIndex = index;
    emit lyricIndexChanged(m_lyricIndex);
}

void MusicController::onPositionChanged(double seconds)
{
    if (m_lines.isEmpty())
        return;
    setLyricIndex(lyricIndexAt(m_lines, seconds));
}

void MusicController::onMetadataChanged(const QVariantMap &metadata)
{
    if (m_mediaMetadata == metadata)
        return;
    m_mediaMetadata = metadata;
    emit mediaMetadataChanged(m_mediaMetadata);
}

// ------------------------------------------------------------------
// Equalizer v1
// ------------------------------------------------------------------

const QVector<double> &MusicController::bandFrequencies()
{
    // ISO one-octave center frequencies; Q≈√2 (w=1.4) matches the spacing.
    static const QVector<double> frequencies {
        31.25, 62.5, 125, 250, 500, 1000, 2000, 4000, 8000, 16000,
    };
    return frequencies;
}

const QStringList &MusicController::presetNames()
{
    static const QStringList names {
        u"flat"_s, u"bass"_s, u"vocal"_s, u"live"_s,
    };
    return names;
}

QVector<double> MusicController::presetGains(const QString &preset)
{
    if (preset == QLatin1String("bass"))
        return bassGains();
    if (preset == QLatin1String("vocal"))
        return vocalGains();
    if (preset == QLatin1String("live"))
        return liveGains();
    return flatGains(); // "flat" and anything unknown
}

QString MusicController::equalizerChain(const QVector<double> &gains)
{
    if (gains.size() != kBandCount)
        return {};
    bool audible = false;
    for (const double gain : gains) {
        if (std::abs(gain) >= 0.001) {
            audible = true;
            break;
        }
    }
    if (!audible)
        return {}; // all-zero: mount nothing (inert)

    QStringList parts;
    parts.reserve(kBandCount);
    const QVector<double> &frequencies = bandFrequencies();
    for (int i = 0; i < kBandCount; ++i) {
        parts.append(QStringLiteral("equalizer=f=%1:t=q:w=1.4:g=%2")
                         .arg(QString::number(frequencies[i]),
                              QString::number(
                                  std::clamp(gains[i], -kMaxGain, kMaxGain))));
    }
    return parts.join(QLatin1Char(','));
}

QVariantList MusicController::equalizerGains() const
{
    QVariantList gains;
    gains.reserve(m_gains.size());
    for (const double gain : m_gains)
        gains.append(gain);
    return gains;
}

void MusicController::loadPersistedEqualizer()
{
    if (!m_settings)
        return;
    m_equalizerEnabled = m_settings->musicEqualizerEnabled();
    m_equalizerPreset = m_settings->musicEqualizerPreset();
    const QList<double> persisted = m_settings->musicEqualizerGains();
    m_gains = QVector<double>(kBandCount, 0.0);
    // Sanitize persisted data: wrong length resets to flat; each value is
    // clamped to the same range the sliders enforce.
    if (persisted.size() == kBandCount) {
        for (int i = 0; i < kBandCount; ++i)
            m_gains[i] = std::clamp(persisted[i], -kMaxGain, kMaxGain);
    } else {
        m_equalizerPreset = QStringLiteral("flat");
    }
    if (m_equalizerPreset != QLatin1String("custom")
        && !presetNames().contains(m_equalizerPreset))
        m_equalizerPreset = QStringLiteral("flat");
}

void MusicController::startup()
{
    // Inert-when-disabled rule (mirrors P5): never write af on a fresh run
    // unless the user has the EQ enabled, so imported mpv.conf options
    // survive untouched.
    if (m_equalizerEnabled)
        applyEqualizerToEngine();
}

void MusicController::setEqualizerEnabled(bool enabled)
{
    if (m_equalizerEnabled == enabled)
        return;
    m_equalizerEnabled = enabled;
    persistEqualizer();
    applyEqualizerToEngine();
    emit equalizerChanged();
}

void MusicController::setEqualizerBand(int band, double gain)
{
    if (band < 0 || band >= kBandCount)
        return;
    const double clamped = std::clamp(gain, -kMaxGain, kMaxGain);
    if (m_gains[band] == clamped)
        return;
    m_gains[band] = clamped;
    if (m_equalizerPreset != QLatin1String("custom")) {
        m_equalizerPreset = QStringLiteral("custom");
        m_settings->setMusicEqualizerPreset(m_equalizerPreset);
    }
    persistEqualizer();
    applyEqualizerToEngine();
    emit equalizerChanged();
}

void MusicController::applyEqualizerPreset(const QString &preset)
{
    if (!presetNames().contains(preset))
        return;
    m_equalizerPreset = preset;
    m_gains = presetGains(preset);
    persistEqualizer();
    applyEqualizerToEngine();
    emit equalizerChanged();
}

void MusicController::persistEqualizer()
{
    if (!m_settings)
        return;
    m_settings->setMusicEqualizerEnabled(m_equalizerEnabled);
    m_settings->setMusicEqualizerPreset(m_equalizerPreset);
    m_settings->setMusicEqualizerGains(
        QList<double>(m_gains.constBegin(), m_gains.constEnd()));
    m_settings->save();
}

void MusicController::applyEqualizerToEngine()
{
    if (!m_engine)
        return;
    // Whole-chain write: enabled -> the equalizer chain, disabled -> clear
    // (mpv resets the af list on an empty string). Replaces whatever was
    // mounted before — documented sole-manager limitation.
    m_engine->setProperty(u"af"_s, m_equalizerEnabled
                                       ? equalizerChain(m_gains)
                                       : QString());
}
