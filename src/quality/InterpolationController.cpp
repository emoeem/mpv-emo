#include "quality/InterpolationController.h"

#include <QFile>

#include "mpv/MpvEngine.h"
#include "quality/QualityProfileEngine.h"
#include "settings/AppSettings.h"

using InterpolationDecision::Block;
using InterpolationDecision::blockToString;

InterpolationController::InterpolationController(MpvEngine *engine,
                                                 AppSettings *settings,
                                                 const QString &scriptsDir,
                                                 QObject *parent)
    : QObject(parent), m_engine(engine), m_settings(settings),
      m_scriptsDir(scriptsDir)
{
    if (m_engine) {
        // Admission depends on the media (HDR, fps): re-evaluate whenever
        // the facts arrive or a new file starts, exactly like the v1
        // quality profile path.
        connect(m_engine, &MpvEngine::fileLoaded, this,
                [this]() { apply(); });
        connect(m_engine, &MpvEngine::videoParamsChanged, this,
                [this]() { apply(); });
        connect(m_engine, &MpvEngine::containerFpsChanged, this,
                [this]() { apply(); });
    }
}

bool InterpolationController::enabled() const
{
    return m_settings ? m_settings->qualityInterpolationEnabled() : false;
}

QString InterpolationController::preset() const
{
    return m_settings ? m_settings->qualityInterpolationPreset() : QString();
}

QString InterpolationController::expectedScriptPath() const
{
    const QString scriptName =
        InterpolationDecision::scriptNameForPreset(preset());
    if (scriptName.isEmpty())
        return QString();
    return m_scriptsDir + QLatin1Char('/') + scriptName;
}

void InterpolationController::startup()
{
    apply();
}

void InterpolationController::setEnabled(bool enabled)
{
    if (!m_settings || enabled == m_settings->qualityInterpolationEnabled())
        return;
    m_settings->setQualityInterpolationEnabled(enabled);
    m_settings->save();
    emit stateChanged();
    apply();
}

void InterpolationController::setPreset(const QString &presetId)
{
    if (!m_settings
        || !InterpolationDecision::isKnownPreset(presetId)
        || presetId == m_settings->qualityInterpolationPreset())
        return;
    m_settings->setQualityInterpolationPreset(presetId);
    m_settings->save();
    emit stateChanged();
    apply();
}

void InterpolationController::apply()
{
    if (!m_engine)
        return;

    // Corrupted settings.json must not enable anything: unknown preset ids
    // behave like the switch being off.
    const QString presetId = preset();
    const bool on = enabled() && InterpolationDecision::isKnownPreset(presetId);

    if (!on) {
        if (m_entryMounted)
            removeEntry();
        m_entryMounted = false;
        setBlockReason(blockToString(Block::Disabled));
        return;
    }

    // The session probe already proved this libmpv cannot host the filter:
    // re-enabling (or a later preset switch) falls straight back off.
    if (m_support == Support::No) {
        fallbackUnsupported();
        return;
    }

    // Stand down while a vapoursynth entry we do not own is in the chain
    // (imported mpv.conf territory). Checked before probing: no reason to
    // touch the chain at all in that case.
    if (InterpolationDecision::vfHasForeignVapoursynth(vfChain())) {
        if (m_entryMounted)
            removeEntry();
        m_entryMounted = false;
        setBlockReason(blockToString(Block::ForeignVapoursynth));
        return;
    }

    // Prove once per session that libmpv accepted the filter before any
    // admission decision matters. A failed probe already reverted the
    // switch to off, so the next apply() takes the disabled path.
    if (m_support == Support::Unknown && !probeSupport())
        return;

    InterpolationDecision::MediaFacts facts = gatherFacts();
    const Block block = InterpolationDecision::admission(facts, true);
    if (block != Block::None) {
        if (m_entryMounted)
            removeEntry();
        m_entryMounted = false;
        setBlockReason(blockToString(block));
        return;
    }

    setBlockReason(QString());
    appendEntry(expectedScriptPath());
    m_entryMounted = true;
}

InterpolationDecision::MediaFacts InterpolationController::gatherFacts() const
{
    InterpolationDecision::MediaFacts facts;
    if (!m_engine)
        return facts;

    facts.fps = m_engine->getProperty(QStringLiteral("container-fps"))
                    .toDouble();
    // Same HDR criteria as the v1 quality profile (BT.2020/PQ/HLG).
    facts.isHdr = QualityProfileEngine::isHdrVideoParams(
        m_engine->getProperty(QStringLiteral("video-params")).toMap());
    facts.scriptAvailable =
        QFile::exists(expectedScriptPath());
    facts.foreignVapoursynth =
        InterpolationDecision::vfHasForeignVapoursynth(vfChain());
    return facts;
}

QVariantList InterpolationController::vfChain() const
{
    if (!m_engine)
        return {};
    return m_engine->getProperty(QStringLiteral("vf")).toList();
}

void InterpolationController::appendEntry(const QString &scriptPath)
{
    // vf is an object settings list; the documented runtime mutation is the
    // change-list command (mpv input.rst change-list; player/command.c
    // cmd_change_list). Appending an entry that carries our label replaces
    // an earlier one in place, so preset switches stay idempotent.
    m_engine->command({QStringLiteral("change-list"),
                       QStringLiteral("vf"),
                       QStringLiteral("append"),
                       InterpolationDecision::vfEntry(scriptPath)});
}

void InterpolationController::removeEntry()
{
    // "remove @label" marks our entry for deletion by label
    // (options/m_option.c parse_obj_settings_del); a missing entry only
    // warns inside mpv, which is exactly what we want on cleanup paths.
    m_engine->command({QStringLiteral("change-list"),
                       QStringLiteral("vf"),
                       QStringLiteral("remove"),
                       QLatin1Char('@') + InterpolationDecision::entryLabel()});
}

bool InterpolationController::probeSupport()
{
    // The append either lands in the vf list (filter compiled in) or fails
    // outright on a libmpv built without vapoursynth ("X doesn't exist",
    // options/m_option.c parse_obj_settings) — reading the chain back
    // distinguishes the two without trusting the command's void return.
    appendEntry(expectedScriptPath());
    const bool accepted = InterpolationDecision::vfHasLabeledEntry(vfChain());
    removeEntry();
    m_entryMounted = false;

    if (accepted) {
        m_support = Support::Yes;
        return true;
    }

    m_support = Support::No;
    fallbackUnsupported();
    return false;
}

void InterpolationController::fallbackUnsupported()
{
    // Honest fallback: without engine support the switch cannot stay on.
    // Persisting "off" avoids probing again on every startup; the panel
    // keeps showing the hint for the rest of the session (the next session
    // probes again — a libmpv with vapoursynth may have appeared).
    m_vsUnavailable = true;
    if (m_settings && m_settings->qualityInterpolationEnabled()) {
        m_settings->setQualityInterpolationEnabled(false);
        m_settings->save();
    }
    if (m_entryMounted) {
        removeEntry();
        m_entryMounted = false;
    }
    emit stateChanged();
    emit errorOccurred(QStringLiteral(
        "vapoursynth filter unavailable: libmpv rejected vf change-list "
        "append (system vapoursynth support required)"));
}

void InterpolationController::setBlockReason(const QString &reason)
{
    if (m_blockReason == reason)
        return;
    m_blockReason = reason;
    emit stateChanged();
}
