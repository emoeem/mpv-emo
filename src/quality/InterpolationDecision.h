#pragma once

#include <QString>
#include <QVariantList>

// P5 v2 frame interpolation: the pure decision table for the honest
// external-dependency pattern (docs/nativeization-plan.md §2.1, vs/ row).
// No mpv, no GUI, no filesystem access — the controller gathers the facts
// and applies the result, so the whole table is unit-testable.
//
// mpv v0.37.0 facts this table builds on (verified against the upstream
// sources; CI links libmpv 0.37):
//   - The vapoursynth video filter takes "file" (string, M_OPT_FILE),
//     "buffered-frames" (default 4) and "concurrent-frames" (default auto)
//     (video/filter/vf_vapoursynth.c vf_opts_fields); the filter name is
//     "vapoursynth". All interpolation logic lives in the .vpy script, so
//     the assembled entry carries the script path, nothing else.
//   - Filter chain entries accept a "@label:" prefix (DOCS/man/vf.rst;
//     labels restricted to [A-Za-z0-9_-], options/m_option.c NAMECH), and
//     "change-list vf append/remove" mutates the list at runtime
//     (m_option_type_obj_settings_list actions; player/command.c
//     cmd_change_list). Appending an entry with an existing label replaces
//     it in place; "remove @label" deletes by label (parse_obj_settings_del).
//   - Sub-param values may be quoted as "%len%<len bytes>" (read_subparam),
//     which is byte-exact: it protects Windows drive letters ("C:/..."),
//     commas and other metacharacters inside the script path.
//   - "vf" read back as MPV_FORMAT_NODE is an array of maps with
//     name/label/enabled/params (get_obj_settings_list) — the probe reads
//     that back to verify mpv actually accepted our entry.
namespace InterpolationDecision {

// The vf entry label EmoPlay owns. The controller only ever touches the
// chain entry carrying this label; anything else is user/imported config.
QString entryLabel();

// The mpv filter name the entry uses ("vapoursynth").
QString vapoursynthFilterName();

// vs/ preset semantics from docs/nativeization-plan.md §2.1 (mpv-Emo
// 补帧 presets): only the naming/admission semantics are adopted — the
// .vpy script files themselves stay external user assets (nativeization
// plan §0: no asset copying), expected in the controller's scripts dir
// under the documented preset file names.
struct PresetInfo
{
    const char *id;         // persisted id, e.g. "MEMC_RIFE_STD"
    const char *scriptName; // expected script file, e.g. "MEMC_RIFE_STD.vpy"
    const char *note;       // one-line backend semantics for the code reader
};

const QList<PresetInfo> &presets();
bool isKnownPreset(const QString &id);
// Script file name for a known preset id, "" otherwise.
QString scriptNameForPreset(const QString &id);

// Facts the controller gathers from the live engine and filesystem.
struct MediaFacts
{
    double fps = 0.0;   // container-fps; 0 = not known yet
    bool isHdr = false; // video-params BT.2020 primaries / PQ / HLG
    // The preset's script file exists in the scripts directory.
    bool scriptAvailable = false;
    // The vf chain already carries a vapoursynth entry EmoPlay does not own
    // (imported mpv.conf or a script): we stand down rather than fight it.
    bool foreignVapoursynth = false;
};

// Why interpolation is not active for the current file. Not all of these
// are errors: Disabled/FpsUnknown are the ordinary idle/no-media states.
enum class Block
{
    None,
    Disabled,            // switch off (or preset id unknown)
    FpsUnknown,          // media params not decoded yet — wait, don't guess
    HighFps,             // fps at/above the threshold: nothing to interpolate
    Hdr,                 // HDR sources excluded (safety fallback semantics)
    ScriptMissing,       // preset .vpy not found in the scripts directory
    ForeignVapoursynth,  // a vapoursynth entry we do not own is already in vf
};

// Admission decision for the current facts. |enabled| is the persisted
// switch; a disabled controller is Blocked::Disabled, not an error.
Block admission(const MediaFacts &facts, bool enabled);

// Stable id strings for the UI hint mapping (panel shows a qsTr'd text).
QString blockToString(Block block);

// Frame rates at or above this value are not interpolated (HFR safety
// fallback; same threshold as the v1 shader HFR avoidance).
double highFpsThreshold();

// The labeled vf entry the controller appends for |scriptPath|, with the
// file value in mpv's %len% quoting so paths with ':' (Windows drives),
// ',' or '[' survive the sub-param parser verbatim.
QString vfEntry(const QString &scriptPath);

// True when the vf readback contains OUR entry (our label AND the
// vapoursynth filter name). The label, not just the name, is what
// distinguishes our entry from a user-configured one.
bool vfHasLabeledEntry(const QVariantList &vfChain);

// True when the vf readback contains a vapoursynth entry that is NOT ours.
bool vfHasForeignVapoursynth(const QVariantList &vfChain);

} // namespace InterpolationDecision
