#include <QVariantList>
#include <QVariantMap>
#include <QtTest>

#include "quality/InterpolationDecision.h"

using namespace Qt::Literals::StringLiterals;
namespace Dec = InterpolationDecision;

// Pure-logic tests for the P5 v2 interpolation decision table: preset
// registry, vf entry assembly (mpv %len% quoting), admission blocks and
// the vf readback classification. No mpv, no GUI, no filesystem.
class TestInterpolationDecision : public QObject
{
    Q_OBJECT

private slots:
    void presetTableMatchesDocumentedVsPresets();
    void unknownPresetRejected();
    void admissionDisabledWhenSwitchOff();
    void admissionBlocksUnknownAndHighFps();
    void admissionBlocksHdr();
    void admissionBlocksMissingScriptAndForeignFilter();
    void admissionPassesSdrLowFpsWithScript();
    void vfEntryUsesLenQuoting();
    void vfEntrySurvivesMetacharacters();
    void vfReadbackClassification();
};

void TestInterpolationDecision::presetTableMatchesDocumentedVsPresets()
{
    // The interpolation presets of docs/nativeization-plan.md §2.1
    // (mpv-Emo vs/ MEMC_* semantics); script names follow the preset ids.
    const QList<Dec::PresetInfo> &all = Dec::presets();
    QCOMPARE(all.size(), 6);
    for (const Dec::PresetInfo &preset : all) {
        QVERIFY(Dec::isKnownPreset(QLatin1String(preset.id)));
        QCOMPARE(QString::fromLatin1(preset.scriptName),
                 QLatin1String(preset.id) + u".vpy"_s);
    }
    QVERIFY(Dec::isKnownPreset(u"MEMC_RIFE_STD"_s));
    QVERIFY(Dec::isKnownPreset(u"MEMC_SVP_PRO"_s));
    QCOMPARE(Dec::scriptNameForPreset(u"MEMC_RIFE_STD"_s),
             u"MEMC_RIFE_STD.vpy"_s);
}

void TestInterpolationDecision::unknownPresetRejected()
{
    QVERIFY(!Dec::isKnownPreset(u"MEMC_MAGICAL"_s));
    QVERIFY(!Dec::isKnownPreset(QString()));
    QVERIFY(Dec::scriptNameForPreset(u"MEMC_MAGICAL"_s).isEmpty());
}

void TestInterpolationDecision::admissionDisabledWhenSwitchOff()
{
    Dec::MediaFacts facts;
    facts.fps = 24.0;
    facts.scriptAvailable = true;
    QCOMPARE(Dec::admission(facts, false), Dec::Block::Disabled);
}

void TestInterpolationDecision::admissionBlocksUnknownAndHighFps()
{
    Dec::MediaFacts facts;
    facts.scriptAvailable = true;

    // Unknown fps (idle or not yet decoded) never guesses.
    facts.fps = 0.0;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::FpsUnknown);

    // HFR sources have nothing to interpolate; the threshold matches the
    // v1 shader HFR avoidance (50 fps).
    facts.fps = 23.976;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::None);
    facts.fps = 49.9;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::None);
    facts.fps = 50.0;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::HighFps);
    facts.fps = 60.0;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::HighFps);
    QCOMPARE(Dec::highFpsThreshold(), 50.0);
}

void TestInterpolationDecision::admissionBlocksHdr()
{
    // HDR sources are excluded (safety fallback semantics of the vs/
    // presets, docs/nativeization-plan.md §2.1).
    Dec::MediaFacts facts;
    facts.fps = 24.0;
    facts.scriptAvailable = true;
    facts.isHdr = true;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::Hdr);
}

void TestInterpolationDecision::admissionBlocksMissingScriptAndForeignFilter()
{
    Dec::MediaFacts facts;
    facts.fps = 24.0;

    facts.scriptAvailable = false;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::ScriptMissing);

    facts.scriptAvailable = true;
    facts.foreignVapoursynth = true;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::ForeignVapoursynth);

    // A foreign filter wins even over every other condition.
    facts.fps = 0.0;
    facts.isHdr = true;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::ForeignVapoursynth);
}

void TestInterpolationDecision::admissionPassesSdrLowFpsWithScript()
{
    Dec::MediaFacts facts;
    facts.fps = 24.0;
    facts.scriptAvailable = true;
    facts.isHdr = false;
    facts.foreignVapoursynth = false;
    QCOMPARE(Dec::admission(facts, true), Dec::Block::None);
    QCOMPARE(Dec::blockToString(Dec::Block::None), QString());
    QCOMPARE(Dec::blockToString(Dec::Block::Hdr), u"hdr"_s);
    QCOMPARE(Dec::blockToString(Dec::Block::ScriptMissing),
             u"script-missing"_s);
}

void TestInterpolationDecision::vfEntryUsesLenQuoting()
{
    // mpv sub-param quoting: %len%<len bytes> — byte-exact, so the entry
    // parser can never split inside the path.
    const QString path = u"/tmp/a.vpy"_s;
    QCOMPARE(Dec::vfEntry(path),
             u"@emoplay-interp:vapoursynth=file=%10%/tmp/a.vpy"_s);
    QVERIFY(Dec::vfEntry(path).startsWith(
        u"@emoplay-interp:vapoursynth=file=%"_s));
    QCOMPARE(Dec::entryLabel(), u"emoplay-interp"_s);
    QCOMPARE(Dec::vapoursynthFilterName(), u"vapoursynth"_s);
}

void TestInterpolationDecision::vfEntrySurvivesMetacharacters()
{
    // Windows drive letters and separators must survive the ':'-separated
    // sub-param parser; the %len% prefix must carry the UTF-8 byte count.
    const QString path = u"C:/Users/tester/AppData/vs/MEMC_RIFE_STD.vpy"_s;
    const QString entry = Dec::vfEntry(path);
    const QString expected =
        u"@emoplay-interp:vapoursynth=file=%"_s
        + QString::number(path.toUtf8().size()) + u"%"_s + path;
    QCOMPARE(entry, expected);

    // Commas (the vf list separator) and brackets are protected the same
    // way: the whole path stays one value.
    const QString nasty = u"/tmp/a,b[c].vpy"_s;
    const QString nastyEntry = Dec::vfEntry(nasty);
    QVERIFY(nastyEntry.contains(u"=file=%"_s));
    QVERIFY(nastyEntry.endsWith(nasty));
}

namespace {

QVariantList vfChain(std::initializer_list<QVariantMap> entries)
{
    QVariantList list;
    for (const QVariantMap &entry : entries)
        list.append(entry);
    return list;
}

QVariantMap vfEntryMap(const QString &name, const QString &label)
{
    QVariantMap map;
    map.insert(u"name"_s, name);
    if (!label.isEmpty())
        map.insert(u"label"_s, label);
    return map;
}

} // namespace

void TestInterpolationDecision::vfReadbackClassification()
{
    // The vf property NODE readback: entries with name/label (get_obj_
    // settings_list). Only our exact label counts as ours.
    const QVariantList ours =
        vfChain({vfEntryMap(u"scale", QString()),
                 vfEntryMap(u"vapoursynth", u"emoplay-interp"_s)});
    QVERIFY(Dec::vfHasLabeledEntry(ours));
    QVERIFY(!Dec::vfHasForeignVapoursynth(ours));

    // No entry at all.
    const QVariantList empty;
    QVERIFY(!Dec::vfHasLabeledEntry(empty));
    QVERIFY(!Dec::vfHasForeignVapoursynth(empty));

    // An unlabeled vapoursynth entry is foreign (user config territory).
    const QVariantList unlabeled =
        vfChain({vfEntryMap(u"vapoursynth", QString())});
    QVERIFY(!Dec::vfHasLabeledEntry(unlabeled));
    QVERIFY(Dec::vfHasForeignVapoursynth(unlabeled));

    // A differently-labeled vapoursynth entry is foreign, too.
    const QVariantList labeledOther =
        vfChain({vfEntryMap(u"vapoursynth", u"my-own-memc"_s)});
    QVERIFY(!Dec::vfHasLabeledEntry(labeledOther));
    QVERIFY(Dec::vfHasForeignVapoursynth(labeledOther));

    // Same-name mismatch: a non-vapoursynth entry with our label is not
    // ours (and not foreign).
    const QVariantList wrongName =
        vfChain({vfEntryMap(u"crop", u"emoplay-interp"_s)});
    QVERIFY(!Dec::vfHasLabeledEntry(wrongName));
    QVERIFY(!Dec::vfHasForeignVapoursynth(wrongName));
}

QTEST_GUILESS_MAIN(TestInterpolationDecision)
#include "test_interpolation_decision.moc"
