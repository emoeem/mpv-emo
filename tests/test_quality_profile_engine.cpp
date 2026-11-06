#include <QtTest>

#include "quality/QualityProfileEngine.h"

using namespace Qt::Literals::StringLiterals;
namespace QPE = QualityProfileEngine; // namespace alias, not a type alias

namespace {

QPE::MediaInfo makeInfo(int height = 1080, double fps = 23.976,
                        const QString &vfFormat = QString(), bool hdr = false,
                        const QStringList &shaders = QStringList())
{
    QPE::MediaInfo info;
    info.width = height * 16 / 9;
    info.height = height;
    info.fps = fps;
    info.vfFormat = vfFormat;
    info.isHdr = hdr;
    info.availableShaders = shaders;
    return info;
}

// The Anime4K "Fast" files a full upstream install provides, nested in a
// subdirectory to exercise the group resolver's path matching.
QStringList fullAnime4kSet()
{
    return QStringList{
        u"Anime4K/Anime4K_Clamp_Highlights.glsl"_s,
        u"Anime4K/Anime4K_Restore_CNN_M.glsl"_s,
        u"Anime4K/Anime4K_Restore_CNN_S.glsl"_s,
        u"Anime4K/Anime4K_Upscale_Denoise_CNN_x2_M.glsl"_s,
        u"Anime4K/Anime4K_Upscale_CNN_x2_M.glsl"_s,
        u"Anime4K/Anime4K_Upscale_CNN_x2_S.glsl"_s,
        u"Anime4K/Anime4K_AutoDownscalePre_x2.glsl"_s,
        u"Anime4K/Anime4K_AutoDownscalePre_x4.glsl"_s,
    };
}

QPE::UserPrefs makePrefs(QPE::ContentMode mode, QPE::QualityTier tier,
                         const QString &tone = u"auto"_s,
                         const QString &scaleOverride = QString())
{
    QPE::UserPrefs prefs;
    prefs.contentMode = mode;
    prefs.qualityTier = tier;
    prefs.toneMappingMode = tone;
    prefs.scaleOverride = scaleOverride;
    return prefs;
}

} // namespace

// Headless decision-table tests for the P5 v1 quality engine. Key paths of
// the anime/live/off x tier x fps x HDR matrix (mpv.md P5 acceptance).
class TestQualityProfileEngine : public QObject
{
    Q_OBJECT

private slots:
    void offTierResetsEverythingToDefaults();
    void animeHighFhdMountsGroupA();
    void animeHighSdMountsHeavierGroup();
    void animeHighUhdMountsLightestGroup();
    void liveHighMountsSsimSuperResWhenAvailable();
    void missingShaderFilesAreSkipped();
    void mediumAndLowTiersNeverMountShaders();
    void highFpsStripsShaderGroupsButKeepsScale();
    void hdrAvoidsShadersAndSetsToneMappingByTier();
    void hdrExplicitToneMappingWinsWhenSupported();
    void hdrUnsupportedToneMappingFallsBack();
    void nonHdrDoesNotTouchToneMappingOptions();
    void autoContentModeFollowsAnimatedHint();
    void explicitContentModeOverridesHint();
    void scaleOverrideWinsWhileActive();
    void debandModeOverride();
    void hdrVideoParamsDetection();
    void animatedHintHeuristic();
    void enumStringRoundTrips();
};

void TestQualityProfileEngine::offTierResetsEverythingToDefaults()
{
    // "off" clears everything back to the verified mpv defaults, whatever
    // the media is (anime or not, HDR or not).
    const auto info = makeInfo(2160, 23.976, u"animated"_s, true);
    const QPE::Result result = QPE::compute(info, makePrefs(QPE::ContentMode::Anime, QPE::QualityTier::Off));
    QCOMPARE(result.options.value(u"scale"_s).toString(), u"lanczos"_s);
    QCOMPARE(result.options.value(u"deband"_s).toBool(), false);
    QCOMPARE(result.options.value(u"tone-mapping"_s).toString(), u"auto"_s);
    QCOMPARE(result.options.value(u"target-colorspace-hint"_s).toBool(), false);
    QVERIFY(result.shaders.isEmpty());
    QVERIFY(result.options.value(u"glsl-shaders"_s).toStringList().isEmpty());
}

void TestQualityProfileEngine::animeHighFhdMountsGroupA()
{
    const auto info = makeInfo(1080, 23.976, u"animated"_s, false, fullAnime4kSet());
    const QPE::Result result = QPE::compute(info, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QCOMPARE(result.options.value(u"scale"_s).toString(), u"ewa_lanczossharp"_s);
    QCOMPARE(result.options.value(u"deband"_s).toBool(), true);
    // FHD anime resolves to group A (Fast): 6 files in upstream order.
    QCOMPARE(result.shaders,
             (QStringList{
                 u"Anime4K/Anime4K_Clamp_Highlights.glsl"_s,
                 u"Anime4K/Anime4K_Restore_CNN_M.glsl"_s,
                 u"Anime4K/Anime4K_Upscale_CNN_x2_M.glsl"_s,
                 u"Anime4K/Anime4K_AutoDownscalePre_x2.glsl"_s,
                 u"Anime4K/Anime4K_AutoDownscalePre_x4.glsl"_s,
                 u"Anime4K/Anime4K_Upscale_CNN_x2_S.glsl"_s,
             }));
}

void TestQualityProfileEngine::animeHighSdMountsHeavierGroup()
{
    // SD sources get the double-pass A+A chain (extra Restore_CNN_S +
    // Upscale_CNN_x2_S pass per upstream CTRL+4).
    const auto info = makeInfo(720, 23.976, u"animated"_s, false, fullAnime4kSet());
    const QPE::Result result = QPE::compute(info, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QCOMPARE(result.shaders.size(), 7);
    QCOMPARE(result.shaders.at(1), u"Anime4K/Anime4K_Restore_CNN_M.glsl"_s);
    QCOMPARE(result.shaders.at(5), u"Anime4K/Anime4K_Restore_CNN_S.glsl"_s);
}

void TestQualityProfileEngine::animeHighUhdMountsLightestGroup()
{
    // UHD sources only need the lightest chain (mode C).
    const auto info = makeInfo(2160, 23.976, u"animated"_s, false, fullAnime4kSet());
    const QPE::Result result = QPE::compute(info, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QCOMPARE(result.shaders,
             (QStringList{
                 u"Anime4K/Anime4K_Clamp_Highlights.glsl"_s,
                 u"Anime4K/Anime4K_Upscale_Denoise_CNN_x2_M.glsl"_s,
                 u"Anime4K/Anime4K_AutoDownscalePre_x2.glsl"_s,
                 u"Anime4K/Anime4K_AutoDownscalePre_x4.glsl"_s,
                 u"Anime4K/Anime4K_Upscale_CNN_x2_S.glsl"_s,
             }));
}

void TestQualityProfileEngine::liveHighMountsSsimSuperResWhenAvailable()
{
    const auto info = makeInfo(1080, 23.976, QString(), false,
                               QStringList{u"SSimSuperRes.glsl"_s});
    const QPE::Result result = QPE::compute(info, makePrefs(QPE::ContentMode::Live, QPE::QualityTier::High));
    QCOMPARE(result.shaders, (QStringList{u"SSimSuperRes.glsl"_s}));
}

void TestQualityProfileEngine::missingShaderFilesAreSkipped()
{
    // Without any available shaders nothing is mounted, but the scale
    // decision still applies ("若存在" semantics).
    const auto info = makeInfo(1080, 23.976, u"animated"_s);
    const QPE::Result result = QPE::compute(info, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QVERIFY(result.shaders.isEmpty());
    QCOMPARE(result.options.value(u"scale"_s).toString(), u"ewa_lanczossharp"_s);

    // A partial install mounts only the files that exist.
    const auto partial = makeInfo(1080, 23.976, u"animated"_s, false,
                                  QStringList{u"Anime4K/Anime4K_Clamp_Highlights.glsl"_s});
    const QPE::Result partialResult = QPE::compute(partial, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QCOMPARE(partialResult.shaders, (QStringList{u"Anime4K/Anime4K_Clamp_Highlights.glsl"_s}));
}

void TestQualityProfileEngine::mediumAndLowTiersNeverMountShaders()
{
    const auto full = makeInfo(720, 23.976, u"animated"_s, false, fullAnime4kSet());

    const QPE::Result medium = QPE::compute(full, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::Medium));
    QVERIFY(medium.shaders.isEmpty());
    QCOMPARE(medium.options.value(u"scale"_s).toString(), u"ewa_lanczossharp"_s);
    QCOMPARE(medium.options.value(u"deband"_s).toBool(), true);

    const QPE::Result low = QPE::compute(full, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::Low));
    QVERIFY(low.shaders.isEmpty());
    QCOMPARE(low.options.value(u"scale"_s).toString(), u"lanczos"_s);
    QCOMPARE(low.options.value(u"deband"_s).toBool(), true);
}

void TestQualityProfileEngine::highFpsStripsShaderGroupsButKeepsScale()
{
    // Downgrade rule: fps >= 50 keeps the scale filter but drops the
    // shader groups even at the high tier.
    const auto hfrAnime = makeInfo(1080, 59.94, u"animated"_s, false, fullAnime4kSet());
    const QPE::Result result = QPE::compute(hfrAnime, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QVERIFY(result.shaders.isEmpty());
    QCOMPARE(result.options.value(u"scale"_s).toString(), u"ewa_lanczossharp"_s);

    // Exactly 50 fps counts as high-frame-rate; 49.94 does not.
    const auto edge = makeInfo(1080, 50.0, u"animated"_s, false, fullAnime4kSet());
    QVERIFY(QPE::compute(edge, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High)).shaders.isEmpty());
    const auto below = makeInfo(1080, 49.94, u"animated"_s, false, fullAnime4kSet());
    QCOMPARE(QPE::compute(below, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High)).shaders.size(), 6);
}

void TestQualityProfileEngine::hdrAvoidsShadersAndSetsToneMappingByTier()
{
    const auto hdr = makeInfo(2160, 23.976, u"animated"_s, true, fullAnime4kSet());

    const QPE::Result high = QPE::compute(hdr, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QVERIFY(high.shaders.isEmpty());
    QCOMPARE(high.options.value(u"tone-mapping"_s).toString(), u"bt.2390"_s);
    QCOMPARE(high.options.value(u"target-colorspace-hint"_s).toBool(), true);

    const QPE::Result medium = QPE::compute(hdr, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::Medium));
    QCOMPARE(medium.options.value(u"tone-mapping"_s).toString(), u"bt.2390"_s);

    const QPE::Result low = QPE::compute(hdr, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::Low));
    QCOMPARE(low.options.value(u"tone-mapping"_s).toString(), u"mobius"_s);
}

void TestQualityProfileEngine::hdrExplicitToneMappingWinsWhenSupported()
{
    const auto hdr = makeInfo(2160, 23.976, QString(), true);
    const QPE::Result result = QPE::compute(hdr, makePrefs(QPE::ContentMode::Live, QPE::QualityTier::High, u"hable"_s));
    QCOMPARE(result.options.value(u"tone-mapping"_s).toString(), u"hable"_s);
}

void TestQualityProfileEngine::hdrUnsupportedToneMappingFallsBack()
{
    // "spline"/"bt.2446a" are gpu-next only in mpv 0.37 (options.rst
    // --tone-mapping): the engine must not emit them under vo=libmpv.
    const auto hdr = makeInfo(2160, 23.976, QString(), true);
    for (const auto requested : {u"spline"_s, u"bt.2446a"_s, u"st2094-40"_s, u"garbage"_s}) {
        const QPE::Result result = QPE::compute(hdr, makePrefs(QPE::ContentMode::Live, QPE::QualityTier::High, requested));
        QCOMPARE(result.options.value(u"tone-mapping"_s).toString(), u"bt.2390"_s);
    }
}

void TestQualityProfileEngine::nonHdrDoesNotTouchToneMappingOptions()
{
    // SDR sources leave the tone-mapping options alone (the user's imported
    // mpv.conf values keep working).
    const auto sdr = makeInfo(1080, 23.976, QString(), false);
    const QPE::Result result = QPE::compute(sdr, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QVERIFY(!result.options.contains(u"tone-mapping"_s));
    QVERIFY(!result.options.contains(u"target-colorspace-hint"_s));
}

void TestQualityProfileEngine::autoContentModeFollowsAnimatedHint()
{
    // Auto + animated hint resolves to an Anime4K group.
    const auto animeInfo = makeInfo(1080, 23.976, u"animated"_s, false, fullAnime4kSet());
    const QPE::Result anime = QPE::compute(animeInfo, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QVERIFY(anime.shaders.first().contains(u"Anime4K"_s));

    // Auto without the hint falls back to the live path.
    const auto liveInfo = makeInfo(1080, 23.976, QString(), false, QStringList{u"SSimSuperRes.glsl"_s});
    const QPE::Result live = QPE::compute(liveInfo, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High));
    QCOMPARE(live.shaders, (QStringList{u"SSimSuperRes.glsl"_s}));
}

void TestQualityProfileEngine::explicitContentModeOverridesHint()
{
    const QStringList shaders{u"SSimSuperRes.glsl"_s, u"Anime4K/Anime4K_Clamp_Highlights.glsl"_s};
    // Animated hint present, but the user forces live content mode.
    const auto info = makeInfo(1080, 23.976, u"animated"_s, false, shaders);
    const QPE::Result live = QPE::compute(info, makePrefs(QPE::ContentMode::Live, QPE::QualityTier::High));
    QCOMPARE(live.shaders, (QStringList{u"SSimSuperRes.glsl"_s}));
    // No hint, but the user forces anime content mode.
    const auto plain = makeInfo(1080, 23.976, QString(), false, shaders);
    const QPE::Result anime = QPE::compute(plain, makePrefs(QPE::ContentMode::Anime, QPE::QualityTier::High));
    QCOMPARE(anime.shaders, (QStringList{u"Anime4K/Anime4K_Clamp_Highlights.glsl"_s}));
}

void TestQualityProfileEngine::scaleOverrideWinsWhileActive()
{
    const auto info = makeInfo(1080, 23.976, u"animated"_s);
    const QPE::Result result = QPE::compute(info, makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::Medium, u"auto"_s, u"bilinear"_s));
    QCOMPARE(result.options.value(u"scale"_s).toString(), u"bilinear"_s);
}

void TestQualityProfileEngine::debandModeOverride()
{
    // "auto" and "yes" keep the table's deband-on while active; "no" wins.
    const auto info = makeInfo(1080, 23.976, u"animated"_s);
    const auto prefs = makePrefs(QPE::ContentMode::Auto, QPE::QualityTier::High);
    QCOMPARE(QPE::compute(info, prefs).options.value(u"deband"_s).toBool(), true);

    auto forcedOn = prefs;
    forcedOn.debandMode = u"yes"_s;
    QCOMPARE(QPE::compute(info, forcedOn).options.value(u"deband"_s).toBool(), true);

    auto forcedOff = prefs;
    forcedOff.debandMode = u"no"_s;
    QCOMPARE(QPE::compute(info, forcedOff).options.value(u"deband"_s).toBool(), false);

    // "off" tier always clears deband, whatever the override says.
    forcedOff.qualityTier = QPE::QualityTier::Off;
    QCOMPARE(QPE::compute(info, forcedOff).options.value(u"deband"_s).toBool(), false);
}

void TestQualityProfileEngine::hdrVideoParamsDetection()
{
    QVERIFY(!QPE::isHdrVideoParams(QVariantMap{}));
    QVERIFY(!QPE::isHdrVideoParams(QVariantMap{{u"w"_s, 1920}, {u"h"_s, 1080}}));
    // SDR BT.709.
    QVERIFY(!QPE::isHdrVideoParams(QVariantMap{{u"primaries"_s, u"bt.709"_s},
                                               {u"gamma"_s, u"bt.1886"_s}}));
    // HDR10: BT.2020 primaries with PQ transfer.
    QVERIFY(QPE::isHdrVideoParams(QVariantMap{{u"primaries"_s, u"bt.2020"_s},
                                              {u"gamma"_s, u"pq"_s}}));
    // HLG without BT.2020 primaries is still HDR.
    QVERIFY(QPE::isHdrVideoParams(QVariantMap{{u"gamma"_s, u"hlg"_s}}));
}

void TestQualityProfileEngine::animatedHintHeuristic()
{
    QCOMPARE(QPE::animatedHintFrom(u"[VCB-Studio] Kimi no Na wa [Ma10p_1080p]"_s),
             u"animated"_s);
    QCOMPARE(QPE::animatedHintFrom(u"Movie.2023.1080p.BluRay.x264-GRP"_s), QString());
    QCOMPARE(QPE::animatedHintFrom(QString()), QString());
}

void TestQualityProfileEngine::enumStringRoundTrips()
{
    QCOMPARE(QPE::contentModeToString(QPE::contentModeFromString(u"auto"_s)), u"auto"_s);
    QCOMPARE(QPE::contentModeToString(QPE::contentModeFromString(u"anime"_s)), u"anime"_s);
    QCOMPARE(QPE::contentModeToString(QPE::contentModeFromString(u"live"_s)), u"live"_s);
    QCOMPARE(QPE::qualityTierToString(QPE::qualityTierFromString(u"off"_s)), u"off"_s);
    QCOMPARE(QPE::qualityTierToString(QPE::qualityTierFromString(u"low"_s)), u"low"_s);
    QCOMPARE(QPE::qualityTierToString(QPE::qualityTierFromString(u"medium"_s)), u"medium"_s);
    QCOMPARE(QPE::qualityTierToString(QPE::qualityTierFromString(u"high"_s)), u"high"_s);
    // Unknown strings fall back to safe defaults.
    QCOMPARE(QPE::contentModeToString(QPE::contentModeFromString(u"???"_s)), u"auto"_s);
    QCOMPARE(QPE::qualityTierToString(QPE::qualityTierFromString(u"???"_s)), u"off"_s);
    QVERIFY(QPE::isSupportedToneMapping(u"bt.2390"_s));
    QVERIFY(!QPE::isSupportedToneMapping(u"spline"_s));
}

QTEST_GUILESS_MAIN(TestQualityProfileEngine)
#include "test_quality_profile_engine.moc"
