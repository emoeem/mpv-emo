#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

#include "quality/QualityProfileEngine.h"

class AppSettings;
class MpvEngine;

// Applies QualityProfileEngine decisions to the live engine and owns the
// user-facing quality preferences (P5 v1). Patterned after HistoryStore:
// a passive QObject service driven by engine signals plus on-demand
// property reads (docs/architecture.md decision 7 — no cached play state).
//
// Responsibilities:
//   - persisted preferences live in AppSettings' "quality" section and are
//     exposed to QML as read-only properties (imperative sync convention)
//   - startup(): adopts shaders mounted by the imported mpv config (P4)
//     into the panel's manual list on first run, and mounts the persisted
//     manual list on later runs. The profile decision table itself is NOT
//     run here — no media is loaded yet
//   - applyProfile(): gathers MediaInfo from observed engine properties,
//     runs the decision table and applies the result — scalar options via
//     MpvEngine::setProperty, the shader list via "change-list" commands
//   - notifyDropFrameRate(): fed from MpvItem's 2s sampler through the QML
//     bridge; sustained heavy dropping demotes the ACTIVE tier to low for
//     the current file (autoDowngraded flag drives the panel hint). The
//     persisted user tier is untouched and returns on the next file
//
// Inert-at-off rule: a session with tier "off" that never had anything
// applied never writes to the engine, so imported mpv.conf rendering
// options are untouched until the user engages the quality engine.
// When the engine disengages (tier switched to off), the managed options
// are reset to mpv defaults per the decision table (known v1 limitation:
// this does not restore a previously imported value).
class QualityController : public QObject
{
    Q_OBJECT
    // Persisted preferences ("auto"/"anime"/"live", "off"/"low"/"medium"/
    // "high", tone-mapping "auto" or an explicit supported value, scale
    // override "" or an explicit --scale value).
    Q_PROPERTY(QString contentMode READ contentMode NOTIFY prefsChanged)
    Q_PROPERTY(QString qualityTier READ qualityTier NOTIFY prefsChanged)
    Q_PROPERTY(QString toneMappingMode READ toneMappingMode NOTIFY prefsChanged)
    Q_PROPERTY(QString scaleOverride READ scaleOverride NOTIFY prefsChanged)
    // Scanned "*.glsl" files (relative paths, may include subdirectories)
    // for the panel's shader management list.
    Q_PROPERTY(QVariantList availableShaders READ availableShaders
                   NOTIFY shadersChanged)
    // Absolute shader paths currently checked in the panel (persisted).
    Q_PROPERTY(QStringList manualShaders READ manualShaders NOTIFY shadersChanged)
    // True while the current file ran with an automatic tier demotion.
    Q_PROPERTY(bool autoDowngraded READ autoDowngraded NOTIFY autoDowngradeChanged)
    Q_PROPERTY(QString shadersDir READ shadersDir CONSTANT)

public:
    // |settings| must outlive this controller; |engine| may be null in
    // tests (then nothing reaches mpv).
    explicit QualityController(MpvEngine *engine, AppSettings *settings,
                               const QString &shadersDir,
                               QObject *parent = nullptr);

    QString contentMode() const;
    QString qualityTier() const;
    QString toneMappingMode() const;
    QString scaleOverride() const;
    QVariantList availableShaders() const { return m_availableShaders; }
    const QStringList &manualShaders() const { return m_manualShaders; }
    bool autoDowngraded() const { return m_autoDowngraded; }
    const QString &shadersDir() const { return m_shadersDir; }

    // Startup seeding; see the class comment. Emits shadersChanged().
    void startup();

public slots:
    // Preference changes from the quality panel: persist, then re-apply.
    void setContentMode(const QString &mode);
    void setQualityTier(const QString &tier);
    void setToneMappingMode(const QString &mode);
    void setScaleOverride(const QString &scale);
    // Panel shader list: mount/unmount a relative shader path.
    void setShaderMounted(const QString &relativePath, bool mounted);
    // Dropped-frames-per-second over MpvItem's 2s window. A sustained rate
    // at or above kAutoDowngradeDropRate demotes the active tier to low.
    void notifyDropFrameRate(double framesPerSecond);
    // Recompute and apply; also connected to the engine's fileLoaded.
    void applyProfile();

signals:
    void prefsChanged();
    void shadersChanged();
    void autoDowngradeChanged();
    void errorOccurred(const QString &message);

private:
    QualityProfileEngine::MediaInfo gatherMediaInfo() const;
    void scanAvailableShaders();
    // Scalar options only (skips the "glsl-shaders" entry).
    void applyScalarOptions(const QVariantMap &options);
    // clr + append of the desired final shader list (absolute paths).
    void applyShaderList(const QStringList &paths);

    MpvEngine *m_engine = nullptr;
    AppSettings *m_settings = nullptr;
    QString m_shadersDir;
    QVariantList m_availableShaders;
    QStringList m_manualShaders;
    bool m_autoDowngraded = false;
    bool m_engineApplied = false;    // engine-owned options are in effect
    bool m_shaderListTouched = false; // we manage the glsl-shaders list
    QStringList m_lastFinalShaders;

    // Dropped frames per second (over MpvItem's 2s window) that triggers
    // the automatic tier demotion.
    static constexpr double kAutoDowngradeDropRate = 2.0;
};
