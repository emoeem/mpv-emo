#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>

// P5 (v1 subset) quality engine: a pure decision table without mpv, GUI or
// filesystem access, so the whole table is unit-testable.
//
// Input: detected media facts + user preferences. Output: the mpv options
// EmoPlay should apply. Every mpv option name and value used here was
// verified against the mpv v0.37.0 docs and sources (CI builds against
// libmpv 0.37):
//   - "scale": default is "lanczos"; "ewa_lanczossharp" is a documented
//     choice (DOCS/man/options.rst --scale; video/out/gpu/video.c
//     gl_video_opts_def).
//   - "deband": flag, default no (video.c gl_video_opts_def has no value).
//   - "tone-mapping": choice set auto/clip/mobius/reinhard/hable/gamma/
//     linear/spline/bt.2390/bt.2446a/st2094-40/st2094-10 (options.rst
//     --tone-mapping). BUT under vo=libmpv only the classic gpu renderer
//     runs (video/out/vo_libmpv.c render_backends[] = { render_backend_gpu,
//     render_backend_sw }); "spline"/"bt.2446a"/"st2094-40" are gpu-next
//     only, so this engine never emits them. Supported here: auto, clip,
//     mobius, reinhard, hable, gamma, linear, bt.2390.
//   - "target-colorspace-hint": requires gpu-next per options.rst, i.e. a
//     no-op in this pipeline. v1 still sets it for HDR sources (harmless
//     and correct once a gpu-next-capable pipeline arrives).
//   - "glsl-shaders": a path-list option (options.rst --glsl-shaders). The
//     applier mutates it with the documented "change-list" command
//     (input.rst change-list; player/command.c cmd_change_list; the list
//     option actions are add/append/clr/pre/set/remove, options/m_option.c).
//
// v1 simplifications versus the mpv-Emo adaptive-quality semantics
// (docs/nativeization-plan.md §2.1): no GPU-tier detection, no release-group
// shader dictionaries (a fixed keyword list instead), and no snapshot of
// user-set values — tier "off" resets the managed options to mpv defaults.
namespace QualityProfileEngine {

// Content type the engine optimizes for. "Auto" resolves through the
// animated content hint of MediaInfo.
enum class ContentMode { Auto, Anime, Live };

// User-facing quality tiers.
enum class QualityTier { Off, Low, Medium, High };

ContentMode contentModeFromString(const QString &value);
QString contentModeToString(ContentMode mode);
QualityTier qualityTierFromString(const QString &value);
QString qualityTierToString(QualityTier tier);

// Facts about the current media, gathered by the caller from observed mpv
// properties and file heuristics (QualityController does this).
struct MediaInfo
{
    int width = 0;    // video-params/w; 0 = unknown
    int height = 0;   // video-params/h
    double fps = 0.0; // container-fps; 0 = unknown
    // "animated" when the track-tag/filename heuristics say anime, else "".
    QString vfFormat;
    // video-params primaries/gamma hint (see isHdrVideoParams()).
    bool isHdr = false;
    // Shader files (relative paths, "*.glsl") present in the user's shaders
    // directory. The engine only ever mounts files from this list.
    QStringList availableShaders;
};

// User preferences shown in the quality panel and persisted in the
// settings.json "quality" section.
struct UserPrefs
{
    ContentMode contentMode = ContentMode::Auto;
    QualityTier qualityTier = QualityTier::Off;
    // "auto" (engine picks by tier) or an explicit tone-mapping value for
    // HDR sources. Values outside the vo=libmpv-supported set fall back to
    // the tier pick.
    QString toneMappingMode = QStringLiteral("auto");
    // "" (engine picks by tier) or an explicit --scale value used whenever
    // the engine is active.
    QString scaleOverride;
    // "auto" (decision table: deband on while the engine is active), "yes"
    // or "no" — user override from the panel's deband control.
    QString debandMode = QStringLiteral("auto");
};

// Engine decision. |options| carries scalar mpv options for
// MpvEngine::setProperty; the special key "glsl-shaders" holds a QStringList
// of desired shader paths, which the applier sends as "change-list"
// commands instead of setProperty. |shaders| mirrors that list.
struct Result
{
    QVariantMap options;
    QStringList shaders;
};

// Run the decision table (table documented in QualityProfileEngine.cpp).
Result compute(const MediaInfo &info, const UserPrefs &prefs);

// True when video-params (the map obtained by observing the "video-params"
// NODE property) indicates HDR: BT.2020 primaries or PQ/HLG transfer.
bool isHdrVideoParams(const QVariantMap &videoParams);

// Filename/title heuristic (v1 keyword list, see the .cpp): returns
// "animated" when the name carries one of the anime release tokens,
// otherwise "".
QString animatedHintFrom(const QString &pathOrTitle);

// Tone-mapping values that work under vo=libmpv (verified against mpv
// v0.37.0, see file comment). Used to validate the user preference.
bool isSupportedToneMapping(const QString &value);

} // namespace QualityProfileEngine
