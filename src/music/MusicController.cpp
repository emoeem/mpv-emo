#include "music/MusicController.h"

#include <QFile>
#include <QUrl>

#include "mpv/MpvEngine.h"

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

} // namespace

MusicController::MusicController(MpvEngine *engine, QObject *parent)
    : QObject(parent), m_engine(engine)
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

int MusicController::lyricIndexAt(const QVector<LrcParser::LrcLine> &lines,
                                  double seconds)
{
    int index = -1;
    for (int i = 0; i < lines.size(); ++i) {
        // Epsilon: exact hits like lines[i].time == seconds must count.
        if (lines[i].time <= seconds + 0.000001)
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
