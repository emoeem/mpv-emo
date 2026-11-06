#include "compat/MpvImportRules.h"

namespace {

// Embedding-critical options and the values that must win.
struct ForcedValue
{
    const char *key;
    const char *value;
    const char *reason;
};

const ForcedValue kForcedValues[] = {
    {"vo", "libmpv", "embedded render API"},
    {"osc", "no", "QML UI draws its own controls"},
    {"terminal", "no", "GUI embedding has no terminal"},
};

// Options removed from user config before applying. See class comment.
const char *kStrippedKeys[] = {
    "include",              // arbitrary file loading bypasses the audit
    "profile",              // profile refs; user profiles are not defined
    "scripts",              // owned by the compat controller
    "input-conf",           // owned by the compat controller
    "config-dir",           // owned by the compat controller
    "watch-later-dir",      // write path -> app data
    "screenshot-directory", // write path -> app data
    "gpu-shader-cache-dir", // write path -> app data
    "icc-cache-dir",        // write path -> app data
    "dump-stats",           // would write to a user-chosen path
    "log-file",             // would write to a user-chosen path
};

bool isStripped(const QString &key)
{
    for (const char *stripped : kStrippedKeys)
        if (key == QLatin1String(stripped))
            return true;
    return false;
}

const ForcedValue *findForced(const QString &key)
{
    for (const ForcedValue &forced : kForcedValues)
        if (key == QLatin1String(forced.key))
            return &forced;
    return nullptr;
}

} // namespace

const QStringList &MpvImportRules::forcedKeys()
{
    static const QStringList keys = [] {
        QStringList list;
        for (const ForcedValue &forced : kForcedValues)
            list.append(QLatin1String(forced.key));
        return list;
    }();
    return keys;
}

const QStringList &MpvImportRules::strippedKeys()
{
    static const QStringList keys = [] {
        QStringList list;
        for (const char *stripped : kStrippedKeys)
            list.append(QLatin1String(stripped));
        return list;
    }();
    return keys;
}

ImportRuleResult MpvImportRules::apply(const QVector<MpvConfEntry> &entries,
                                       const QString &mpvDir,
                                       const QString &appDataDir)
{
    ImportRuleResult result;

    // Pass 1: user options. Top-level options only — everything after a
    // [profile] header belongs to that profile and is not applied in P4
    // (profiles cannot be defined through mpv_set_option_string; they are
    // reported instead). Last occurrence of an option wins, matching mpv.
    QString currentProfile;
    for (const MpvConfEntry &entry : entries) {
        switch (entry.kind) {
        case MpvConfEntry::Kind::ProfileHeader:
            currentProfile = entry.profileName;
            result.profileNames.append(entry.profileName);
            result.skipped.append(
                QStringLiteral("profile [%1] in %2: section not applied in P4")
                    .arg(entry.profileName, mpvDir));
            break;
        case MpvConfEntry::Kind::Option: {
            if (!currentProfile.isEmpty())
                continue; // belongs to the profile above; already reported

            if (entry.key == QLatin1String("screenshot-template")
                && entry.value.contains(QLatin1String("~~"))) {
                // A config-dir-relative screenshot template would land inside
                // the user's mpv directory even with screenshot-directory
                // redirected, because ~~ expands to the user config dir.
                result.skipped.append(
                    QStringLiteral("screenshot-template: '%1' (~~ path would "
                                   "write into the user's mpv directory)")
                        .arg(entry.value));
                continue;
            }

            if (isStripped(entry.key)) {
                result.skipped.append(
                    QStringLiteral("%1: '%2' (%3)")
                        .arg(entry.key, entry.value,
                             entry.key == QLatin1String("profile")
                                 ? QStringLiteral("profiles are not applied "
                                                  "in P4")
                                 : QStringLiteral("managed by EmoPlay")));
                continue;
            }

            if (const ForcedValue *forced = findForced(entry.key)) {
                result.forced.append(
                    QStringLiteral("%1: user value '%2' replaced with '%3' "
                                   "(%4)")
                        .arg(entry.key, entry.value,
                             QLatin1String(forced->value),
                             QLatin1String(forced->reason)));
                continue; // the forced value is written in pass 2
            }

            result.options.insert(entry.key, entry.value);
            break;
        }
        case MpvConfEntry::Kind::Unknown:
            break;
        }
    }

    // Pass 2: forced embedding values win over everything from pass 1.
    for (const ForcedValue &forced : kForcedValues)
        result.options.insert(QLatin1String(forced.key),
                              QLatin1String(forced.value));

    // Pass 3: write paths land in EmoPlay's app data, never in the user's
    // mpv directory (which config-dir points at for ~~ resolution).
    struct Redirect
    {
        const char *key;
        const char *subdir;
    };
    const Redirect kRedirects[] = {
        {"watch-later-dir", "watch_later"},
        {"screenshot-directory", "screenshots"},
        {"gpu-shader-cache-dir", "shader_cache"},
        {"icc-cache-dir", "icc_cache"},
    };
    for (const Redirect &redirect : kRedirects) {
        const QString path = appDataDir + QLatin1Char('/')
                             + QLatin1String(redirect.subdir);
        result.options.insert(QLatin1String(redirect.key), path);
        result.redirected.append(QStringLiteral("%1 -> %2")
                                     .arg(QLatin1String(redirect.key), path));
    }

    return result;
}
