#include "quality/QualityProfileEngine.h"

#include <QVariant>

namespace QualityProfileEngine {

namespace {

// Verified against mpv v0.37.0: "lanczos" is the default --scale
// (video/out/gpu/video.c gl_video_opts_def), "ewa_lanczossharp" a
// documented choice (DOCS/man/options.rst --scale).
constexpr auto kDefaultScale = QLatin1String("lanczos");
constexpr auto kQualityScale = QLatin1String("ewa_lanczossharp");

// Tone-mapping picks by tier. "spline"/"bt.2446a" would be nicer but are
// gpu-next only in v0.37 (see the header comment); "bt.2390" is the
// perceptual EETF available under the classic gpu renderer.
constexpr auto kToneMappingDefault = QLatin1String("auto");
constexpr auto kToneMappingQuality = QLatin1String("bt.2390");
constexpr auto kToneMappingLight = QLatin1String("mobius");

// Keyword list for the "animated" content hint — a v1 simplification of the
// mpv-Emo release-group dictionary (docs/nativeization-plan.md §2.1).
// Deliberately conservative to avoid flagging live-action titles.
const QStringList &animeHintTokens()
{
    static const QStringList tokens = {
        QStringLiteral("anime"),      QStringLiteral("at-x"),
        QStringLiteral("bs11"),       QStringLiteral("baha"),
        QStringLiteral("b-global"),   QStringLiteral("bglobal"),
        QStringLiteral("cr.webrip"),  QStringLiteral("cr-webrip"),
        QStringLiteral("vcb-studio"), QStringLiteral("leopard-raws"),
        QStringLiteral("ma10p"),
    };
    return tokens;
}

// Tone-mapping values valid under vo=libmpv (classic gpu renderer) in mpv
// v0.37.0 — see the header comment for the verification trail.
const QStringList &supportedToneMappings()
{
    static const QStringList values = {
        QStringLiteral("auto"),   QStringLiteral("clip"),
        QStringLiteral("mobius"), QStringLiteral("reinhard"),
        QStringLiteral("hable"),  QStringLiteral("gamma"),
        QStringLiteral("linear"), QStringLiteral("bt.2390"),
    };
    return values;
}

// --scale choices documented in the mpv v0.37.0 manual (options.rst
// --scale). The manual notes there are more undocumented filters; only the
// documented set is accepted as a user override.
const QStringList &supportedScales()
{
    static const QStringList values = {
        QStringLiteral("bilinear"),             QStringLiteral("lanczos"),
        QStringLiteral("ewa_lanczos"),          QStringLiteral("ewa_lanczossharp"),
        QStringLiteral("ewa_lanczos4sharpest"), QStringLiteral("mitchell"),
        QStringLiteral("hermite"),              QStringLiteral("catmull_rom"),
        QStringLiteral("oversample"),           QStringLiteral("linear"),
    };
    return values;
}

// Anime4K v4 group chains, file names verified against upstream
// bloc97/Anime4K v4.0.1 GLSL_Instructions.md (the "Fast" variants chosen so
// lower-end GPUs survive; the engine mounts only files that actually exist
// in the user's shaders directory, so partial installs degrade gracefully).
//   Mode A   = Clamp_Highlights -> Restore_CNN_M -> Upscale_CNN_x2_M ->
//              AutoDownscalePre_x2 -> AutoDownscalePre_x4 -> Upscale_CNN_x2_S
//   Mode A+A = Mode A followed by a second Restore_CNN_S -> Upscale_CNN_x2_S
//              pass (upstream CTRL+4 "Mode A+A (Fast)")
//   Mode C   = Clamp_Highlights -> Upscale_Denoise_CNN_x2_M ->
//              AutoDownscalePre_x2 -> AutoDownscalePre_x4 -> Upscale_CNN_x2_S
const QStringList &anime4kGroupA()
{
    static const QStringList group = {
        QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
        QStringLiteral("Anime4K_Restore_CNN_M.glsl"),
        QStringLiteral("Anime4K_Upscale_CNN_x2_M.glsl"),
        QStringLiteral("Anime4K_AutoDownscalePre_x2.glsl"),
        QStringLiteral("Anime4K_AutoDownscalePre_x4.glsl"),
        QStringLiteral("Anime4K_Upscale_CNN_x2_S.glsl"),
    };
    return group;
}

const QStringList &anime4kGroupAPlusA()
{
    static const QStringList group = {
        QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
        QStringLiteral("Anime4K_Restore_CNN_M.glsl"),
        QStringLiteral("Anime4K_Upscale_CNN_x2_M.glsl"),
        QStringLiteral("Anime4K_AutoDownscalePre_x2.glsl"),
        QStringLiteral("Anime4K_AutoDownscalePre_x4.glsl"),
        QStringLiteral("Anime4K_Restore_CNN_S.glsl"),
        QStringLiteral("Anime4K_Upscale_CNN_x2_S.glsl"),
    };
    return group;
}

const QStringList &anime4kGroupC()
{
    static const QStringList group = {
        QStringLiteral("Anime4K_Clamp_Highlights.glsl"),
        QStringLiteral("Anime4K_Upscale_Denoise_CNN_x2_M.glsl"),
        QStringLiteral("Anime4K_AutoDownscalePre_x2.glsl"),
        QStringLiteral("Anime4K_AutoDownscalePre_x4.glsl"),
        QStringLiteral("Anime4K_Upscale_CNN_x2_S.glsl"),
    };
    return group;
}

const QStringList &ssimSuperResGroup()
{
    // Live-action high tier: optional SSimSuperRes group (agyild's shader;
    // mounted only when present among the available files).
    static const QStringList group = { QStringLiteral("SSimSuperRes.glsl") };
    return group;
}

// Resolution -> Anime4K group (v1 constant table). Bigger sources need less
// upscaling work, so the heaviest chain is reserved for SD sources.
QString anime4kGroupForHeight(int height)
{
    if (height >= 1440)
        return QStringLiteral("C");
    if (height >= 900)
        return QStringLiteral("A");
    return QStringLiteral("A+A");
}

const QStringList &shaderGroupByName(const QString &group)
{
    if (group == QLatin1String("A"))
        return anime4kGroupA();
    if (group == QLatin1String("A+A"))
        return anime4kGroupAPlusA();
    if (group == QLatin1String("C"))
        return anime4kGroupC();
    return ssimSuperResGroup();
}

// Match a wanted group file against the available shader list: an entry
// matches when it equals the file name or sits in a subdirectory carrying
// it (e.g. "Anime4K/Anime4K_Restore_CNN_M.glsl").
QStringList resolveShaderGroup(const QString &group,
                               const QStringList &available)
{
    const QStringList &wanted = shaderGroupByName(group);
    QStringList resolved;
    for (const QString &name : wanted) {
        for (const QString &candidate : available) {
            if (candidate == name || candidate.endsWith(QLatin1Char('/') + name)) {
                if (!resolved.contains(candidate))
                    resolved.append(candidate);
                break;
            }
        }
    }
    return resolved;
}

} // namespace

ContentMode contentModeFromString(const QString &value)
{
    if (value == QLatin1String("anime"))
        return ContentMode::Anime;
    if (value == QLatin1String("live"))
        return ContentMode::Live;
    return ContentMode::Auto;
}

QString contentModeToString(ContentMode mode)
{
    switch (mode) {
    case ContentMode::Anime: return QLatin1String("anime");
    case ContentMode::Live: return QLatin1String("live");
    case ContentMode::Auto: break;
    }
    return QLatin1String("auto");
}

QualityTier qualityTierFromString(const QString &value)
{
    if (value == QLatin1String("low"))
        return QualityTier::Low;
    if (value == QLatin1String("medium"))
        return QualityTier::Medium;
    if (value == QLatin1String("high"))
        return QualityTier::High;
    return QualityTier::Off;
}

QString qualityTierToString(QualityTier tier)
{
    switch (tier) {
    case QualityTier::Low: return QLatin1String("low");
    case QualityTier::Medium: return QLatin1String("medium");
    case QualityTier::High: return QLatin1String("high");
    case QualityTier::Off: break;
    }
    return QLatin1String("off");
}

bool isSupportedToneMapping(const QString &value)
{
    return supportedToneMappings().contains(value);
}

bool isSupportedScale(const QString &value)
{
    return supportedScales().contains(value);
}

bool isHdrVideoParams(const QVariantMap &videoParams)
{
    // mpv v0.37 property values (input.rst video-params/*): "primaries"
    // e.g. "bt.709"/"bt.2020"; "gamma" (transfer) e.g. "bt.1886"/"pq"/"hlg".
    const QString primaries = videoParams.value(QStringLiteral("primaries")).toString();
    const QString gamma = videoParams.value(QStringLiteral("gamma")).toString();
    return primaries == QLatin1String("bt.2020") || gamma == QLatin1String("pq")
           || gamma == QLatin1String("hlg");
}

QString animatedHintFrom(const QString &pathOrTitle)
{
    const QString lower = pathOrTitle.toLower();
    for (const QString &token : animeHintTokens()) {
        if (lower.contains(token))
            return QStringLiteral("animated");
    }
    return QString();
}

Result compute(const MediaInfo &info, const UserPrefs &prefs)
{
    Result result;

    if (prefs.qualityTier == QualityTier::Off) {
        // Decision table row "off": clear everything back to mpv defaults
        // (verified defaults; see the header comment).
        result.options.insert(QStringLiteral("scale"), kDefaultScale);
        result.options.insert(QStringLiteral("deband"), false);
        result.options.insert(QStringLiteral("tone-mapping"), kToneMappingDefault);
        result.options.insert(QStringLiteral("target-colorspace-hint"), false);
        result.options.insert(QStringLiteral("glsl-shaders"), QStringList());
        return result;
    }

    // Content resolution: explicit mode wins, "auto" follows the hint.
    const bool anime = prefs.contentMode == ContentMode::Anime
                       || (prefs.contentMode == ContentMode::Auto
                           && info.vfFormat == QLatin1String("animated"));

    // High-frame-rate sources (>= 50 fps): heavy shader groups cannot keep
    // up, so they are dropped (downgrade rule keeps only the scale filter).
    const bool highFps = info.fps >= 50.0;

    // Shader groups: high tier only, never on HDR sources (HDR 避让) and
    // never on high-frame-rate sources. With unknown video parameters
    // (height<=0, e.g. media info not decoded yet or audio-only files) no
    // group is mounted at all: the resolution table and the HDR/HFR checks
    // cannot be evaluated — the caller re-runs the table when
    // video-params/container-fps arrive.
    QStringList shaders;
    const bool paramsKnown = info.height > 0;
    if (paramsKnown && prefs.qualityTier == QualityTier::High && !info.isHdr
        && !highFps) {
        const QString group = anime
                                  ? anime4kGroupForHeight(info.height)
                                  : QStringLiteral("SSimSuperRes");
        shaders = resolveShaderGroup(group, info.availableShaders);
    }

    // Scale: low tier stays at the mpv default, medium/high sharpen; an
    // explicit user override wins whenever it names a documented --scale
    // value (corrupted settings.json values fall back to the tier pick).
    QString scale = prefs.qualityTier == QualityTier::Low
                        ? kDefaultScale.toString()
                        : kQualityScale.toString();
    if (!prefs.scaleOverride.isEmpty() && isSupportedScale(prefs.scaleOverride))
        scale = prefs.scaleOverride;

    result.options.insert(QStringLiteral("scale"), scale);
    // Deband: the mode is normalized first — "yes" forces it on, "no"
    // forces it off, and any other value (corrupted settings.json values
    // included) means "auto", where the table turns deband on for every
    // active tier (mpv's own default is "no").
    result.options.insert(QStringLiteral("deband"),
                          prefs.debandMode != QLatin1String("no"));

    // Tone-mapping series: managed on every active apply so that values
    // left by a previous HDR file cannot leak into SDR playback (and vice
    // versa). While the engine is active it owns the managed option set
    // (scale/deband behave the same way); the inert-at-off rule keeps
    // imported values untouched as long as the engine is never engaged.
    if (info.isHdr) {
        // HDR sources: the explicit user mode wins when it is usable under
        // vo=libmpv; otherwise the engine picks by tier.
        QString tone = prefs.toneMappingMode;
        if (!isSupportedToneMapping(tone) || tone == kToneMappingDefault) {
            tone = prefs.qualityTier == QualityTier::Low
                       ? kToneMappingLight.toString()
                       : kToneMappingQuality.toString();
        }
        result.options.insert(QStringLiteral("tone-mapping"), tone);
        // gpu-next only in v0.37 (no-op here); set for pipeline parity.
        result.options.insert(QStringLiteral("target-colorspace-hint"), true);
    } else {
        result.options.insert(QStringLiteral("tone-mapping"), kToneMappingDefault);
        result.options.insert(QStringLiteral("target-colorspace-hint"), false);
    }

    result.options.insert(QStringLiteral("glsl-shaders"), shaders);
    result.shaders = shaders;
    return result;
}

} // namespace QualityProfileEngine
