#pragma once

#include <QObject>
#include <QString>
#include <QVariant>
#include <QVariantList>
#include <QVariantMap>

#include <mpv/client.h>

// Owns the libmpv client handle and bridges mpv events to Qt signals.
//
// Threading model: libmpv's client API is thread-safe. Events are pumped on
// the GUI thread through mpv_set_wakeup_callback + queued invocation, so all
// signals emitted by this class arrive on the thread that owns the object.
//
// Lifetime: MpvEngine::instance() is set in the constructor and cleared in
// the destructor. In main() the engine must be declared *before* the
// QmlApplicationEngine so that QML items (and their mpv render contexts) are
// destroyed first. See MpvItem for the render side of that contract.
class MpvEngine : public QObject
{
    Q_OBJECT

public:
    explicit MpvEngine(QObject *parent = nullptr);
    ~MpvEngine() override;

    MpvEngine(const MpvEngine &) = delete;
    MpvEngine &operator=(const MpvEngine &) = delete;

    // Process-wide last-initialized engine, used by QML items to reach the
    // handle. Null when no engine has been constructed.
    static MpvEngine *instance() { return s_instance; }

    // Creates and initializes the mpv core. |options| are applied as raw
    // option strings (mpv_set_option_string) before mpv_initialize and
    // override the built-in defaults. Returns false on failure; lastError()
    // explains why.
    bool initialize(const QVariantMap &options = {});

    mpv_handle *handle() const { return m_mpv; }
    const QString &lastError() const { return m_lastError; }

    // Run an mpv command, e.g. {"loadfile", "/path"}, {"seek", "10"}.
    Q_INVOKABLE void command(const QVariantList &args);
    Q_INVOKABLE void setProperty(const QString &name, const QVariant &value);
    Q_INVOKABLE QVariant getProperty(const QString &name) const;
    // Accepts local paths, file:// URLs and network URLs (http(s)://...).
    Q_INVOKABLE void loadFile(const QString &pathOrUrl);

    // Feed a key event to mpv's input layer ("keypress" command), so bindings
    // from an imported input.conf and script-registered bindings fire in the
    // embedded UI. |key| is an mpv key name ("shift+a", "PGUP", "ctrl+s");
    // unknown names fail harmlessly inside mpv. QML side: mpvKeyName().
    Q_INVOKABLE void keyPress(const QString &key);
    // Qt key event -> mpv key name ("" when unmappable). Delegates to
    // MpvKeyName::fromQt; exposed for the QML key-forwarding path.
    Q_INVOKABLE QString mpvKeyName(int key, int modifiers,
                                   const QString &text) const;

    // Resolves user input to something mpv accepts: local file paths stay
    // as-is, file:// URLs become local paths, everything else (http(s),
    // ytdl) passes through. Shared by loadFile and the sub-add/loadfile
    // commands issued for external subtitles and playlist entries.
    static QString toMediaPath(const QString &pathOrUrl);

signals:
    void errorOccurred(const QString &message);
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pausedChanged(bool paused);
    void volumeChanged(double volume);
    void mutedChanged(bool muted);
    void mediaTitleChanged(const QString &title);
    void trackListChanged(const QVariantList &tracks);
    // Mirrors mpv's "playlist" property: a list of entries with filename,
    // current/playing flags, optional title.
    void playlistChanged(const QVariantList &playlist);
    void idleActiveChanged(bool idle);
    void fileLoaded();
    void playbackRestarted();
    // reason follows mpv_event_end_file.reason (0 = EOF, 2 = quit, 3 = error).
    void endFile(int reason);

    // P5 quality engine inputs (observed NODE/DOUBLE/STRING properties).
    // video-params as a QVariantMap (w/h/primaries/gamma/...); empty map
    // when the values are unavailable. frame-drop-count is observed as
    // MPV_FORMAT_NODE because the counter is an int64 (mpv input.rst).
    void videoParamsChanged(const QVariantMap &params);
    void containerFpsChanged(double fps);
    void frameDropCountChanged(qlonglong count);
    void currentVoChanged(const QString &vo);
    void videoFormatChanged(const QString &format);

private slots:
    // Entry point for the wakeup callback; see onWakeup.
    void handleEvents();

private:
    static void onWakeup(void *ctx);

    void observeDefaults();
    void emitError(int mpvErr, const char *what);

    mpv_handle *m_mpv = nullptr;
    QString m_lastError;
    inline static MpvEngine *s_instance = nullptr;
};
