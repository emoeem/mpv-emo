#pragma once

#include <QObject>
#include <QString>

class AppSettings;
class MpvEngine;

// ICC display configuration (P5 v2): an enable switch plus a display
// profile file picked in the quality panel, applied to the live engine as
// the mpv "icc-profile" gpu-renderer option (mpv v0.37.0
// video/out/gpu/lcms.c mp_icc_conf; runtime changes are picked up by
// gl_video_update_options -> gl_lcms_update_options, video.c, so no reload
// is needed).
//
// The 3DLUT cache location always follows the P4 redirect table: the
// controller sets icc-cache-dir to MpvImportRules::redirectTarget(
// "icc-cache-dir", appDataDir) — <appData>/icc_cache — whenever it applies
// a profile, so enabling ICC from EmoPlay never moves a write path into
// the user's mpv directory, imported session or not.
//
// Honest failure handling: the profile file must exist before anything is
// written; the property set is verified by reading icc-profile back; and
// any failure reverts the persisted switch to off and raises failureHint
// for the panel. Disabling restores the icc-profile value captured at
// startup(), so a profile imported through P4 survives the round trip
// (inert-at-off, same rule as QualityController).
class IccController : public QObject
{
    Q_OBJECT
    // Persisted switch and profile path (absolute, from the panel's file
    // dialog).
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(QString profilePath READ profilePath NOTIFY stateChanged)
    // True after the last apply attempt failed (hint below is shown).
    Q_PROPERTY(bool failed READ failed NOTIFY stateChanged)
    Q_PROPERTY(QString failureHint READ failureHint NOTIFY stateChanged)
    // Redirected cache directory (P4 table), shown in the panel.
    Q_PROPERTY(QString cacheDir READ cacheDir CONSTANT)

public:
    // |settings| must outlive this controller; |engine| may be null in
    // tests (then nothing reaches mpv). |appDataDir| receives the cache
    // redirect (icc_cache sub-directory per the P4 table).
    explicit IccController(MpvEngine *engine, AppSettings *settings,
                           const QString &appDataDir, QObject *parent = nullptr);

    bool enabled() const;
    QString profilePath() const;
    bool failed() const { return m_failed; }
    const QString &failureHint() const { return m_failureHint; }
    QString cacheDir() const;

    // Snapshots the engine's current icc-profile (an imported value must
    // survive disable/enable cycles) and applies the persisted preferences.
    void startup();

public slots:
    // Switch from the panel: persisted, then applied; enabling without a
    // usable profile falls back to off with a hint.
    void setEnabled(bool enabled);
    // Absolute profile path from the panel's file dialog (file:// URLs are
    // converted). Persisted, then applied.
    void setProfilePath(const QString &path);

signals:
    void stateChanged();
    void errorOccurred(const QString &message);

private:
    void apply();
    void restoreProfile();
    // Reverts the switch to off (persisted), restores any applied profile
    // and surfaces |hint| in the panel.
    void fail(const QString &hint);

    MpvEngine *m_engine = nullptr;
    AppSettings *m_settings = nullptr;
    QString m_appDataDir;
    // icc-profile value seen at startup(); restored on disable/failure.
    QString m_startupProfile;
    bool m_snapshotTaken = false;
    bool m_applied = false;
    bool m_failed = false;
    QString m_failureHint;
};
