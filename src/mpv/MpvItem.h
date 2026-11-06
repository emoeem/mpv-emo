#pragma once

#include <QQuickFramebufferObject>
#include <QTimer>
#include <QVariant>

class MpvEngine;

// QQuickItem that renders mpv video into the Qt Quick scene graph.
//
// Built on QQuickFramebufferObject: a dedicated renderer (render thread) owns
// the mpv render context and blits the current frame into the item's
// framebuffer object. Playback state is mirrored from the shared MpvEngine
// through its observed-property signals, so QML can bind to position,
// duration, pause state, volume, track list, video geometry, fps, drop
// counters, etc.
class MpvItem : public QQuickFramebufferObject
{
    Q_OBJECT
    Q_PROPERTY(double position READ position NOTIFY positionChanged)
    Q_PROPERTY(double duration READ duration NOTIFY durationChanged)
    Q_PROPERTY(bool paused READ paused WRITE setPaused NOTIFY pausedChanged)
    Q_PROPERTY(bool idle READ idle NOTIFY idleChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool muted READ muted WRITE setMuted NOTIFY mutedChanged)
    Q_PROPERTY(QString mediaTitle READ mediaTitle NOTIFY mediaTitleChanged)
    Q_PROPERTY(QVariantList trackList READ trackList NOTIFY trackListChanged)
    Q_PROPERTY(QVariantList playlist READ playlist NOTIFY playlistChanged)
    // P5 quality inputs mirrored from the engine (video-params /
    // container-fps / frame-drop-count).
    Q_PROPERTY(int videoWidth READ videoWidth NOTIFY videoSizeChanged)
    Q_PROPERTY(int videoHeight READ videoHeight NOTIFY videoSizeChanged)
    Q_PROPERTY(double fps READ fps NOTIFY fpsChanged)
    Q_PROPERTY(qlonglong droppedFrames READ droppedFrames NOTIFY droppedFramesChanged)
    Q_PROPERTY(bool hdrActive READ hdrActive NOTIFY hdrActiveChanged)
    // Dropped frames per second over the last 2s sampling window.
    Q_PROPERTY(double droppedFrameRate READ droppedFrameRate
                   NOTIFY droppedFrameRateChanged)

public:
    explicit MpvItem(QQuickItem *parent = nullptr);
    ~MpvItem() override;

    Renderer *createRenderer() const override;

    double position() const { return m_position; }
    double duration() const { return m_duration; }
    bool paused() const { return m_paused; }
    bool idle() const { return m_idle; }
    double volume() const { return m_volume; }
    bool muted() const { return m_muted; }
    QString mediaTitle() const { return m_mediaTitle; }
    QVariantList trackList() const { return m_trackList; }
    QVariantList playlist() const { return m_playlist; }
    int videoWidth() const { return m_videoWidth; }
    int videoHeight() const { return m_videoHeight; }
    double fps() const { return m_fps; }
    qlonglong droppedFrames() const { return m_droppedFrames; }
    bool hdrActive() const { return m_hdrActive; }
    double droppedFrameRate() const { return m_droppedFrameRate; }

    // Writers only forward the request to the engine; the authoritative state
    // comes back through the engine's observed-property signals.
    void setPaused(bool paused);
    void setVolume(double volume);
    void setMuted(bool muted);

public slots:
    void togglePlayPause();
    void play();
    void pause();
    void stop();
    void seek(double seconds);   // absolute position in seconds
    void seekBy(double offset);  // relative offset in seconds

    // Track selection: |id| is a track id from trackList (number) or the
    // string "no" to turn the track off. Requests only — the resulting
    // selection comes back through trackListChanged.
    void selectAudioTrack(const QVariant &id);
    void selectSubtitleTrack(const QVariant &id);
    // Loads an external subtitle file (local path or file:// URL) and
    // selects it immediately (sub-add ... select).
    void loadExternalSubtitle(const QString &path);

    // Playlist commands. appendToPlaylist queues a file without interrupting
    // playback (loadfile ... append-play; mpv starts it when idle). The
    // authoritative list state comes back through playlistChanged.
    void appendToPlaylist(const QString &path);
    void playlistNext();
    void playlistPrev();
    void playlistPlayIndex(int index);
    void playlistClear();

signals:
    void positionChanged(double position);
    void durationChanged(double duration);
    void pausedChanged(bool paused);
    void idleChanged(bool idle);
    void volumeChanged(double volume);
    void mutedChanged(bool muted);
    void mediaTitleChanged(const QString &title);
    void trackListChanged(const QVariantList &tracks);
    void playlistChanged(const QVariantList &playlist);
    void videoSizeChanged();
    void fpsChanged(double fps);
    void droppedFramesChanged(qlonglong dropped);
    void hdrActiveChanged(bool hdr);
    void droppedFrameRateChanged(double framesPerSecond);

private slots:
    // 2s sampling window over the mirrored drop counter; emits the drop
    // rate for the auto-downgrade path (P5).
    void sampleDroppedFrames();

private:
    MpvEngine *m_engine = nullptr;

    double m_position = 0.0;
    double m_duration = 0.0;
    double m_volume = 100.0;
    bool m_paused = true;
    bool m_idle = true;
    bool m_muted = false;
    QString m_mediaTitle;
    QVariantList m_trackList;
    QVariantList m_playlist;
    int m_videoWidth = 0;
    int m_videoHeight = 0;
    double m_fps = 0.0;
    qlonglong m_droppedFrames = 0;
    bool m_hdrActive = false;
    double m_droppedFrameRate = 0.0;
    qlonglong m_lastDroppedSample = 0;
    QTimer m_dropSampleTimer;
};
