#pragma once

#include <QQuickFramebufferObject>
#include <QVariant>

class MpvEngine;

// QQuickItem that renders mpv video into the Qt Quick scene graph.
//
// Built on QQuickFramebufferObject: a dedicated renderer (render thread) owns
// the mpv render context and blits the current frame into the item's
// framebuffer object. Playback state is mirrored from the shared MpvEngine
// through its observed-property signals, so QML can bind to position,
// duration, pause state, volume, track list, etc.
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

signals:
    void positionChanged(double position);
    void durationChanged(double duration);
    void pausedChanged(bool paused);
    void idleChanged(bool idle);
    void volumeChanged(double volume);
    void mutedChanged(bool muted);
    void mediaTitleChanged(const QString &title);
    void trackListChanged(const QVariantList &tracks);

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
};
