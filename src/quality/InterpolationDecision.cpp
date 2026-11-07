#include "quality/InterpolationDecision.h"

namespace InterpolationDecision {

namespace {

// The preset table mirrors the mpv-Emo vs/ interpolation preset semantics
// documented in docs/nativeization-plan.md §2.1 (MEMC_* names). The .vpy
// files are NOT shipped: the user points EmoPlay at a scripts directory
// holding their own scripts (e.g. an existing mpv-Emo vs/ checkout), so
// the app never fakes a capability it does not carry.
const QList<PresetInfo> kPresets = {
    {"MEMC_RIFE_NV",  "MEMC_RIFE_NV.vpy",  "RIFE motion interpolation, NVIDIA-preferred backend"},
    {"MEMC_RIFE_STD", "MEMC_RIFE_STD.vpy", "RIFE motion interpolation, standard backend"},
    {"MEMC_RIFE_DML", "MEMC_RIFE_DML.vpy", "RIFE motion interpolation, DirectML backend"},
    {"MEMC_SVP_PRO",  "MEMC_SVP_PRO.vpy",  "SVP 4 Pro motion interpolation"},
    {"MEMC_MVT_LQ",   "MEMC_MVT_LQ.vpy",   "MVtools motion interpolation, low quality/low cost"},
    {"MEMC_DRBA_NV",  "MEMC_DRBA_NV.vpy",  "DRBA (depth-aware rate adjustment) NVIDIA variant"},
};

} // namespace

QString entryLabel()
{
    return QStringLiteral("emoplay-interp");
}

QString vapoursynthFilterName()
{
    return QStringLiteral("vapoursynth");
}

const QList<PresetInfo> &presets()
{
    return kPresets;
}

bool isKnownPreset(const QString &id)
{
    for (const PresetInfo &preset : kPresets)
        if (id == QLatin1String(preset.id))
            return true;
    return false;
}

QString scriptNameForPreset(const QString &id)
{
    for (const PresetInfo &preset : kPresets)
        if (id == QLatin1String(preset.id))
            return QLatin1String(preset.scriptName);
    return QString();
}

double highFpsThreshold()
{
    return 50.0;
}

Block admission(const MediaFacts &facts, bool enabled)
{
    if (!enabled)
        return Block::Disabled;
    // A vapoursynth entry we do not own wins: the user's imported config
    // (or a script) manages interpolation already — never fight it.
    if (facts.foreignVapoursynth)
        return Block::ForeignVapoursynth;
    // Media parameters unknown (idle or not yet decoded): never guess.
    // The controller re-runs admission when video-params/fps arrive.
    if (facts.fps <= 0.0)
        return Block::FpsUnknown;
    if (facts.fps >= highFpsThreshold())
        return Block::HighFps;
    if (facts.isHdr)
        return Block::Hdr;
    if (!facts.scriptAvailable)
        return Block::ScriptMissing;
    return Block::None;
}

QString blockToString(Block block)
{
    switch (block) {
    case Block::None:
        return QString();
    case Block::Disabled:
        return QStringLiteral("disabled");
    case Block::FpsUnknown:
        return QStringLiteral("fps-unknown");
    case Block::HighFps:
        return QStringLiteral("high-fps");
    case Block::Hdr:
        return QStringLiteral("hdr");
    case Block::ScriptMissing:
        return QStringLiteral("script-missing");
    case Block::ForeignVapoursynth:
        return QStringLiteral("foreign-vapoursynth");
    }
    return QString();
}

QString vfEntry(const QString &scriptPath)
{
    // "%len%" quoting (options/m_option.c read_subparam): byte-exact, so
    // the script path survives the ':'-separated sub-param parser on every
    // platform ("C:/Users/..." would otherwise split into extra params).
    const QByteArray utf8 = scriptPath.toUtf8();
    return QLatin1Char('@') + entryLabel() + QLatin1Char(':')
           + vapoursynthFilterName() + QLatin1String("=file=%")
           + QString::number(utf8.size()) + QLatin1Char('%')
           + QString::fromUtf8(utf8);
}

bool vfHasLabeledEntry(const QVariantList &vfChain)
{
    for (const QVariant &entry : vfChain) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("name")).toString()
                == vapoursynthFilterName()
            && map.value(QStringLiteral("label")).toString() == entryLabel())
            return true;
    }
    return false;
}

bool vfHasForeignVapoursynth(const QVariantList &vfChain)
{
    for (const QVariant &entry : vfChain) {
        const QVariantMap map = entry.toMap();
        if (map.value(QStringLiteral("name")).toString()
                != vapoursynthFilterName())
            continue;
        if (map.value(QStringLiteral("label")).toString() != entryLabel())
            return true;
    }
    return false;
}

} // namespace InterpolationDecision
