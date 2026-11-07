#include "quality/IccController.h"

#include <QFileInfo>

#include "compat/MpvImportRules.h"
#include "mpv/MpvEngine.h"
#include "settings/AppSettings.h"

IccController::IccController(MpvEngine *engine, AppSettings *settings,
                             const QString &appDataDir, QObject *parent)
    : QObject(parent), m_engine(engine), m_settings(settings),
      m_appDataDir(appDataDir)
{
}

bool IccController::enabled() const
{
    return m_settings ? m_settings->qualityIccEnabled() : false;
}

QString IccController::profilePath() const
{
    return m_settings ? m_settings->qualityIccProfilePath() : QString();
}

QString IccController::cacheDir() const
{
    // Same table the P4 startup redirects use — one source of truth.
    return MpvImportRules::redirectTarget(QStringLiteral("icc-cache-dir"),
                                          m_appDataDir);
}

void IccController::startup()
{
    if (!m_engine)
        return;

    // Snapshot before touching anything: with an imported mpv.conf the
    // engine may already run a user-configured profile that must survive
    // our disable path untouched.
    m_startupProfile =
        m_engine->getProperty(QStringLiteral("icc-profile")).toString();
    m_snapshotTaken = true;

    apply();
}

void IccController::setEnabled(bool enabled)
{
    if (!m_settings || enabled == m_settings->qualityIccEnabled())
        return;
    m_settings->setQualityIccEnabled(enabled);
    m_settings->save();
    emit stateChanged();
    apply();
}

void IccController::setProfilePath(const QString &path)
{
    // File dialogs hand out file:// URLs; mpv's M_OPT_FILE wants a plain
    // filesystem path.
    const QString normalized = MpvEngine::toMediaPath(path);
    if (!m_settings || normalized == m_settings->qualityIccProfilePath())
        return;
    m_settings->setQualityIccProfilePath(normalized);
    m_settings->save();
    emit stateChanged();
    apply();
}

void IccController::apply()
{
    if (!m_engine)
        return;

    const bool on = enabled() && !profilePath().isEmpty();
    if (!on) {
        if (m_applied) {
            restoreProfile();
            m_applied = false;
        }
        m_failed = false;
        m_failureHint.clear();
        emit stateChanged();
        return;
    }

    // Honest pre-check: a missing/unreadable profile would only surface
    // deep inside mpv's renderer; state it where the user can act on it.
    const QFileInfo info(profilePath());
    if (!info.isFile() || !info.isReadable()) {
        fail(QStringLiteral("ICC profile file not found or unreadable: %1")
                 .arg(profilePath()));
        return;
    }

    // Cache redirect first so the freshly enabled profile's 3DLUT cache
    // never lands in the user's mpv directory (P4 redirect table).
    m_engine->setProperty(QStringLiteral("icc-cache-dir"), cacheDir());
    m_engine->setProperty(QStringLiteral("icc-profile"), profilePath());

    // Verify the option actually took (a libmpv without the gpu renderer
    // option would silently ignore it otherwise).
    const QString readBack =
        m_engine->getProperty(QStringLiteral("icc-profile")).toString();
    if (readBack != profilePath()) {
        fail(QStringLiteral("mpv rejected icc-profile (read back '%1')")
                 .arg(readBack));
        return;
    }

    m_applied = true;
    m_failed = false;
    m_failureHint.clear();
    emit stateChanged();
}

void IccController::restoreProfile()
{
    if (!m_engine)
        return;
    // An empty string restores mpv's default (no profile).
    m_engine->setProperty(QStringLiteral("icc-profile"), m_startupProfile);
}

void IccController::fail(const QString &hint)
{
    if (m_applied) {
        restoreProfile();
        m_applied = false;
    }
    // Same fallback contract as the interpolation controller: without a
    // usable profile the switch cannot stay on.
    if (m_settings && m_settings->qualityIccEnabled()) {
        m_settings->setQualityIccEnabled(false);
        m_settings->save();
    }
    m_failed = true;
    m_failureHint = hint;
    emit stateChanged();
    emit errorOccurred(hint);
}
