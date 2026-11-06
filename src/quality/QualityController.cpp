#include "quality/QualityController.h"

#include <QDir>
#include <QDirIterator>
#include <QVariant>

#include "mpv/MpvEngine.h"
#include "settings/AppSettings.h"

QualityController::QualityController(MpvEngine *engine, AppSettings *settings,
                                     const QString &shadersDir,
                                     QObject *parent)
    : QObject(parent), m_engine(engine), m_settings(settings),
      m_shadersDir(shadersDir)
{
    // A new file resets the auto demotion: the user's persisted tier comes
    // back and gets a fresh chance to trigger.
    if (m_engine) {
        connect(m_engine, &MpvEngine::fileLoaded, this, [this]() {
            if (m_autoDowngraded) {
                m_autoDowngraded = false;
                emit autoDowngradeChanged();
                applyProfile();
            }
        });
    }
}

QString QualityController::contentMode() const
{
    return m_settings ? m_settings->qualityContentMode() : QString();
}

QString QualityController::qualityTier() const
{
    return m_settings ? m_settings->qualityTier() : QString();
}

QString QualityController::toneMappingMode() const
{
    return m_settings ? m_settings->qualityToneMapping() : QString();
}

QString QualityController::scaleOverride() const
{
    return m_settings ? m_settings->qualityScaleOverride() : QString();
}

QString QualityController::debandMode() const
{
    return m_settings ? m_settings->qualityDebandMode() : QString();
}

void QualityController::startup()
{
    if (!m_engine)
        return;

    m_manualShaders = m_settings ? m_settings->manualShaderList() : QStringList();
    const QVariantList mountedNow =
        m_engine->getProperty(QStringLiteral("glsl-shaders")).toList();

    if (m_manualShaders.isEmpty()) {
        // First run of the quality panel: adopt whatever the imported mpv
        // config already mounted (P4 passes glsl-shaders through) so that
        // the panel becomes the single source of truth.
        QStringList adopted;
        for (const QVariant &entry : mountedNow) {
            const QString path = entry.toString();
            if (!path.isEmpty() && !adopted.contains(path))
                adopted.append(path);
        }
        if (!adopted.isEmpty() && m_settings) {
            m_manualShaders = adopted;
            m_settings->setManualShaderList(adopted);
            m_settings->save();
        }
    } else if (mountedNow.isEmpty()) {
        // Returning session without an imported shader list: mount the
        // persisted manual list via the documented change-list commands.
        applyShaderList(m_manualShaders);
    }

    scanAvailableShaders();
    if (!m_manualShaders.isEmpty()) {
        m_shaderListTouched = true;
        m_lastFinalShaders = m_manualShaders;
    }
    emit shadersChanged();
}

void QualityController::setContentMode(const QString &mode)
{
    if (!m_settings || mode == m_settings->qualityContentMode())
        return;
    m_settings->setQualityContentMode(mode);
    m_settings->save();
    emit prefsChanged();
    applyProfile();
}

void QualityController::setQualityTier(const QString &tier)
{
    if (!m_settings || tier == m_settings->qualityTier())
        return;
    m_settings->setQualityTier(tier);
    m_settings->save();
    if (m_autoDowngraded) {
        // A manual choice lifts the automatic demotion.
        m_autoDowngraded = false;
        emit autoDowngradeChanged();
    }
    emit prefsChanged();
    applyProfile();
}

void QualityController::setToneMappingMode(const QString &mode)
{
    if (!m_settings || mode == m_settings->qualityToneMapping())
        return;
    m_settings->setQualityToneMapping(mode);
    m_settings->save();
    emit prefsChanged();
    applyProfile();
}

void QualityController::setScaleOverride(const QString &scale)
{
    if (!m_settings || scale == m_settings->qualityScaleOverride())
        return;
    m_settings->setQualityScaleOverride(scale);
    m_settings->save();
    emit prefsChanged();
    applyProfile();
}

void QualityController::setDebandMode(const QString &mode)
{
    if (!m_settings || mode == m_settings->qualityDebandMode())
        return;
    m_settings->setQualityDebandMode(mode);
    m_settings->save();
    emit prefsChanged();
    applyProfile();
}

void QualityController::setShaderMounted(const QString &relativePath,
                                         bool mounted)
{
    if (relativePath.isEmpty())
        return;
    const QString absolute = QDir(m_shadersDir).filePath(relativePath);
    QStringList next = m_manualShaders;
    if (mounted) {
        if (!next.contains(absolute))
            next.append(absolute);
    } else {
        next.removeAll(absolute);
    }
    if (next == m_manualShaders)
        return;
    m_manualShaders = next;
    if (m_settings) {
        m_settings->setManualShaderList(next);
        m_settings->save();
    }
    emit shadersChanged();
    applyProfile();
}

void QualityController::notifyDropFrameRate(double framesPerSecond)
{
    if (m_autoDowngraded || framesPerSecond < kAutoDowngradeDropRate)
        return;
    const auto tier = QualityProfileEngine::qualityTierFromString(qualityTier());
    if (tier == QualityProfileEngine::QualityTier::Off
        || tier == QualityProfileEngine::QualityTier::Low)
        return;
    // Demote the active tier only: the persisted user tier stays, so the
    // next file starts from the user's real preference again.
    m_autoDowngraded = true;
    emit autoDowngradeChanged();
    applyProfile();
}

void QualityController::applyProfile()
{
    if (!m_engine)
        return;

    QualityProfileEngine::UserPrefs prefs;
    prefs.contentMode =
        QualityProfileEngine::contentModeFromString(contentMode());
    const auto userTier =
        QualityProfileEngine::qualityTierFromString(qualityTier());
    // Active-tier demotion while auto-downgraded; "off" stays off.
    prefs.qualityTier =
        m_autoDowngraded && userTier != QualityProfileEngine::QualityTier::Off
            ? QualityProfileEngine::QualityTier::Low
            : userTier;
    prefs.toneMappingMode = toneMappingMode();
    prefs.scaleOverride = scaleOverride();
    prefs.debandMode = debandMode();

    const QualityProfileEngine::MediaInfo info = gatherMediaInfo();
    const QualityProfileEngine::Result result =
        QualityProfileEngine::compute(info, prefs);

    const bool engineActive =
        prefs.qualityTier != QualityProfileEngine::QualityTier::Off;

    // Final mount list = manual (user-owned, absolute) + engine selection
    // (relative under shadersDir) with manual entries first.
    const QDir shaderDir(m_shadersDir);
    QStringList finalShaders = m_manualShaders;
    for (const QString &relative : result.shaders) {
        const QString absolute = shaderDir.filePath(relative);
        if (!finalShaders.contains(absolute))
            finalShaders.append(absolute);
    }

    // Inert at "off" until the engine or the shader list was ever engaged:
    // never touch imported rendering options of a fresh session.
    if (!engineActive && !m_engineApplied && !m_shaderListTouched
        && finalShaders.isEmpty())
        return;

    if (engineActive || m_engineApplied)
        applyScalarOptions(result.options);
    m_engineApplied = engineActive;

    if ((engineActive || !finalShaders.isEmpty() || m_shaderListTouched)
        && finalShaders != m_lastFinalShaders)
        applyShaderList(finalShaders);
    m_shaderListTouched = engineActive || !finalShaders.isEmpty();
    m_lastFinalShaders = finalShaders;
}

QualityProfileEngine::MediaInfo QualityController::gatherMediaInfo() const
{
    QualityProfileEngine::MediaInfo info;
    if (!m_engine)
        return info;

    const QVariantMap params =
        m_engine->getProperty(QStringLiteral("video-params")).toMap();
    info.width = params.value(QStringLiteral("w")).toInt();
    info.height = params.value(QStringLiteral("h")).toInt();
    info.isHdr = QualityProfileEngine::isHdrVideoParams(params);
    info.fps = m_engine->getProperty(QStringLiteral("container-fps")).toDouble();

    // Animated content hint: file name first, fall back to the title
    // (streaming URLs usually have no usable path part).
    QString name = m_engine->getProperty(QStringLiteral("path")).toString();
    if (name.isEmpty())
        name = m_engine->getProperty(QStringLiteral("media-title")).toString();
    info.vfFormat = QualityProfileEngine::animatedHintFrom(name);

    for (const QVariant &entry : m_availableShaders)
        info.availableShaders.append(entry.toString());
    return info;
}

void QualityController::scanAvailableShaders()
{
    m_availableShaders.clear();
    QStringList names;
    const QDir dir(m_shadersDir);
    QDirIterator it(m_shadersDir, QStringList{QStringLiteral("*.glsl")},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        names.append(dir.relativeFilePath(it.next()));
    names.sort();
    for (const QString &name : names)
        m_availableShaders.append(name);
    emit shadersChanged();
}

void QualityController::applyScalarOptions(const QVariantMap &options)
{
    for (auto it = options.constBegin(); it != options.constEnd(); ++it) {
        if (it.key() == QLatin1String("glsl-shaders"))
            continue; // handled by applyShaderList (change-list commands)
        m_engine->setProperty(it.key(), it.value());
    }
}

void QualityController::applyShaderList(const QStringList &paths)
{
    // glsl-shaders is a path-list option (mpv options.rst): the documented
    // runtime mutation is the change-list command (input.rst change-list;
    // player/command.c cmd_change_list; actions add/append/clr/...).
    m_engine->command({QStringLiteral("change-list"),
                       QStringLiteral("glsl-shaders"),
                       QStringLiteral("clr"), QString()});
    for (const QString &path : paths)
        m_engine->command({QStringLiteral("change-list"),
                           QStringLiteral("glsl-shaders"),
                           QStringLiteral("append"), path});
}
