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

signals:
    void errorOccurred(const QString &message);
    void positionChanged(double seconds);
    void durationChanged(double seconds);
    void pausedChanged(bool paused);
    void volumeChanged(double volume);
    void mutedChanged(bool muted);
    void mediaTitleChanged(const QString &title);
    void trackListChanged(const QVariantList &tracks);
    void idleActiveChanged(bool idle);
    void fileLoaded();
    void playbackRestarted();
    // reason follows mpv_event_end_file.reason (0 = EOF, 2 = quit, 3 = error).
    void endFile(int reason);

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
