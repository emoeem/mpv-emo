#pragma once

#include <QString>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

#include "config/MpvConfigParser.h"

// Result of applying the import rules to a parsed mpv.conf.
struct ImportRuleResult
{
    // Option map ready for MpvEngine::initialize(): surviving user options
    // plus the forced embedding options and redirected write paths.
    QVariantMap options;
    // Audit lines: "vo: user value 'gpu-next' replaced with 'libmpv'".
    QStringList forced;
    // Audit lines: "profile: 'Target' (user profiles are not applied in P4)".
    QStringList skipped;
    // Audit lines: "watch-later-dir: redirected to '<path>'".
    QStringList redirected;
    // Names of [profile] sections seen in the user's mpv.conf (not applied).
    QStringList profileNames;
};

// Applies EmoPlay's import policy to options parsed from a user's mpv.conf
// (P4 compat layer). The policy implements two user principles:
//
// 1. GUI embedding constraints win over user config: `vo`, `osc` and
//    `terminal` are forced to the embed-safe values no matter what the
//    imported file says (MpvEngine additionally re-asserts them after the
//    option map is applied, so no rules bug can break the embed).
// 2. EmoPlay never writes into the user's mpv directory: options that make
//    the core write state files (watch-later, screenshots, shader/ICC and
//    demuxer caches, stats/log dumps) are redirected into EmoPlay's own app
//    data directory, and `include=` / `profile=` are dropped entirely
//    (arbitrary file loads and profile definitions are not applied in P4).
//
// Everything else passes through: hwdec, volume, keep-open, scale, and the
// like are imported as-is. Values containing `~~` are kept — the engine side
// points mpv's config-dir at the user's directory so they resolve to the
// user's original files without copying anything.
//
// Pure logic, no I/O: unit-testable in isolation.
class MpvImportRules
{
public:
    // Options forced to embed-safe values, in force order.
    static const QStringList &forcedKeys();
    // Options dropped from user config (replaced or removed by the engine).
    static const QStringList &strippedKeys();

    // |mpvDir| is the user's mpv config directory (for audit messages),
    // |appDataDir| EmoPlay's writable app data directory receiving redirected
    // write paths. appDataDir must be non-empty.
    static ImportRuleResult apply(const QVector<MpvConfEntry> &entries,
                                  const QString &mpvDir,
                                  const QString &appDataDir);
};
