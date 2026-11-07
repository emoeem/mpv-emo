#pragma once

#include <QObject>
#include <QString>
#include <QVariantList>

#include "quality/InterpolationDecision.h"

class AppSettings;
class MpvEngine;

// Frame interpolation v2 (P5): VapourSynth MEMC presets applied to the live
// engine as a labeled "vf" chain entry, behind an enable switch and a
// preset picker persisted in settings.json's quality section.
//
// Honest external-dependency pattern (docs/nativeization-plan.md §2.1 vs/
// row): EmoPlay only assembles and mounts the mpv side. The VapourSynth
// runtime, inference backends and the .vpy preset scripts stay external;
// whenever a piece is missing the feature stands down visibly instead of
// pretending to work:
//   - the preset's script file missing from the scripts directory blocks
//     the preset (panel hint, switch stays on, retry is free);
//   - a vapoursynth entry in the vf chain that EmoPlay does not own
//     (imported mpv.conf / a script) makes the controller stand down
//     completely — user config territory is never fought over;
//   - if libmpv cannot accept the vapoursynth filter at all (build without
//     vapoursynth: the "change-list vf append" command fails immediately),
//     the controller detects that by reading the vf property back, reverts
//     the persisted switch to off and raises the "needs system VapourSynth
//     support" hint (vsUnavailable). The probe runs once per session.
//
// Admission (HDR sources, high/unknown fps) lives in the pure
// InterpolationDecision table; this class gathers the facts (same
// signal-driven pattern as QualityController, docs/architecture.md
// decision 7: nothing is cached) and mounts/unmounts exactly one vf entry
// labeled "emoplay-interp", leaving the rest of the chain untouched.
class InterpolationController : public QObject
{
    Q_OBJECT
    // Persisted switch and preset id ("MEMC_RIFE_STD", ...).
    Q_PROPERTY(bool enabled READ enabled NOTIFY stateChanged)
    Q_PROPERTY(QString preset READ preset NOTIFY stateChanged)
    // True after the session probe proved libmpv cannot host vapoursynth;
    // the switch was already reverted to off.
    Q_PROPERTY(bool vsUnavailable READ vsUnavailable NOTIFY stateChanged)
    // Stable id for the panel hint of the current admission block ("",
    // "fps-unknown", "high-fps", "hdr", "script-missing",
    // "foreign-vapoursynth").
    Q_PROPERTY(QString blockReason READ blockReason NOTIFY stateChanged)
    Q_PROPERTY(QString scriptsDir READ scriptsDir CONSTANT)
    // Absolute script path the current preset expects (panel shows where
    // to drop the .vpy file).
    Q_PROPERTY(QString expectedScriptPath READ expectedScriptPath
                   NOTIFY stateChanged)

public:
    // |settings| must outlive this controller; |engine| may be null in
    // tests (then nothing reaches mpv). |scriptsDir| holds the user's
    // VapourSynth preset scripts (EmoPlay ships none).
    explicit InterpolationController(MpvEngine *engine, AppSettings *settings,
                                     const QString &scriptsDir,
                                     QObject *parent = nullptr);

    bool enabled() const;
    QString preset() const;
    bool vsUnavailable() const { return m_vsUnavailable; }
    const QString &blockReason() const { return m_blockReason; }
    const QString &scriptsDir() const { return m_scriptsDir; }
    QString expectedScriptPath() const;

    // Applies the persisted preferences to the (idle) engine. Admission for
    // unknown media blocks without touching the chain; the support probe
    // runs once while the switch is on.
    void startup();

public slots:
    // Switch from the panel: persisted, then (re)applied. Enabling with an
    // unsupported libmpv triggers the probe fallback immediately.
    void setEnabled(bool enabled);
    // Preset id from the panel; unknown ids are ignored (defensive against
    // corrupted settings.json).
    void setPreset(const QString &presetId);

signals:
    void stateChanged();
    void errorOccurred(const QString &message);

private:
    void apply();
    // Gathers fps/HDR facts from observed engine properties (no caching).
    InterpolationDecision::MediaFacts gatherFacts() const;
    // Appends/removes our labeled vf entry via change-list commands.
    void appendEntry(const QString &scriptPath);
    void removeEntry();
    QVariantList vfChain() const;
    // Appends the entry, reads the chain back, removes it again. Returns
    // true when mpv accepted the filter. On failure: persists the switch
    // off, sets vsUnavailable and clears any mounted entry.
    bool probeSupport();
    // The "needs system vapoursynth support" fallback: switch off
    // (persisted), chain cleaned, hint set.
    void fallbackUnsupported();
    void setBlockReason(const QString &reason);

    MpvEngine *m_engine = nullptr;
    AppSettings *m_settings = nullptr;
    QString m_scriptsDir;
    QString m_blockReason;
    bool m_vsUnavailable = false;
    bool m_entryMounted = false;
    // Session probe result: unknown until first probed, then sticky.
    enum class Support { Unknown, Yes, No };
    Support m_support = Support::Unknown;
};
