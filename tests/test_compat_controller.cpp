#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>

#include "compat/CompatController.h"

using namespace Qt::Literals::StringLiterals;

// Headless tests for the compat decision flow. All paths are injected: the
// scanned config dir is the committed fixture, settings and app data live in
// temporary directories. The real HOME is never touched.
class TestCompatController : public QObject
{
    Q_OBJECT

private slots:
    void undecidedShowsPromptAndImportsNothing();
    void fullImportBuildsOptionMap();
    void fullImportForcesEmbeddingOptions();
    void selectiveImportHonorsFileList();
    void selectiveImportOfNothingIsEmpty();
    void standaloneUsesOwnInputConf();
    void skipSuppressesPrompt();
    void decisionPersistsAcrossInstances();
    void missingConfigDirIsUndetected();
    void requestWizardTogglesProperty();

private:
    // Returns the fixture dir; caller wires it in as the user config dir.
    static QString fixtureDir()
    {
        return QFileInfo(QFINDTESTDATA("fixtures/mpv-conf/mpv.conf"))
            .absolutePath();
    }
};

void TestCompatController::undecidedShowsPromptAndImportsNothing()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    QCOMPARE(controller.startupOptions(), QVariantMap{});
    QVERIFY(controller.configDetected());
    QVERIFY(controller.firstRunPromptNeeded());
    QCOMPARE(controller.sourceDir(), fixtureDir());
    QVERIFY(!controller.importFiles().isEmpty());
}

void TestCompatController::fullImportBuildsOptionMap()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    controller.startupOptions();
    controller.chooseFullImport();

    const QVariantMap options = controller.startupOptions();
    // Embedding-critical values.
    QCOMPARE(options.value(u"vo"_s).toString(), u"libmpv"_s);
    QCOMPARE(options.value(u"osc"_s).toString(), u"no"_s);
    QCOMPARE(options.value(u"terminal"_s).toString(), u"no"_s);
    // User option passes through (fixture mpv.conf: volume=80).
    QCOMPARE(options.value(u"volume"_s).toString(), u"80"_s);
    // input.conf is loaded from the user's directory, not copied.
    QCOMPARE(options.value(u"input-conf"_s).toString(),
             fixtureDir() + u"/input.conf"_s);
    // config-dir points at the user's dir for ~~ resolution.
    QCOMPARE(options.value(u"config-dir"_s).toString(), fixtureDir());
    // Both fixture scripts are listed (comma-separated, absolute).
    const QString scripts = options.value(u"scripts"_s).toString();
    QVERIFY(scripts.contains(fixtureDir() + u"/scripts/a.lua"_s));
    QVERIFY(scripts.contains(fixtureDir() + u"/scripts/lib/b.js"_s));
    QVERIFY(scripts.contains(u","_s));
    // fonts-dir points at the user's fonts directory.
    QCOMPARE(options.value(u"fonts-dir"_s).toString(),
             fixtureDir() + u"/fonts"_s);
    // Write paths are redirected into EmoPlay's app data.
    QCOMPARE(options.value(u"watch-later-dir"_s).toString(),
             appData.path() + u"/watch_later"_s);
    QCOMPARE(options.value(u"screenshot-directory"_s).toString(),
             appData.path() + u"/screenshots"_s);
    // Profile section [hq] from the fixture is not applied.
    QVERIFY(!options.contains(u"scale"_s));
}

void TestCompatController::fullImportForcesEmbeddingOptions()
{
    // The fixture mpv.conf contains vo=gpu-next: the forced value must win.
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    controller.startupOptions();
    controller.chooseFullImport();

    QCOMPARE(controller.startupOptions().value(u"vo"_s).toString(),
             u"libmpv"_s);
}

void TestCompatController::selectiveImportHonorsFileList()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    controller.startupOptions();
    controller.chooseSelectiveImport(QStringList{u"mpv.conf"_s});

    const QVariantMap options = controller.startupOptions();
    QCOMPARE(options.value(u"volume"_s).toString(), u"80"_s);
    QVERIFY(!options.contains(u"input-conf"_s));
    QVERIFY(!options.contains(u"scripts"_s));
    QVERIFY(!options.contains(u"fonts-dir"_s));
    // Forced values still apply — the embedding constraints are not optional.
    QCOMPARE(options.value(u"vo"_s).toString(), u"libmpv"_s);
    // config-dir is still set so ~~ values resolve somewhere defined.
    QCOMPARE(options.value(u"config-dir"_s).toString(), fixtureDir());

    // Selecting only input.conf: no user options, but bindings load.
    controller.chooseSelectiveImport(QStringList{u"input.conf"_s});
    const QVariantMap inputOnly = controller.startupOptions();
    QVERIFY(!inputOnly.contains(u"volume"_s));
    QCOMPARE(inputOnly.value(u"input-conf"_s).toString(),
             fixtureDir() + u"/input.conf"_s);
}

void TestCompatController::selectiveImportOfNothingIsEmpty()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    controller.startupOptions();
    controller.chooseSelectiveImport(QStringList{});
    QCOMPARE(controller.startupOptions(), QVariantMap{});
    QVERIFY(!controller.firstRunPromptNeeded());
}

void TestCompatController::standaloneUsesOwnInputConf()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    controller.startupOptions();
    controller.chooseStandalone();

    const QVariantMap options = controller.startupOptions();
    QCOMPARE(options.value(u"input-conf"_s).toString(),
             appData.path() + u"/input.conf"_s);
    // The empty input.conf is created in EmoPlay's own app data.
    QVERIFY(QFileInfo::exists(appData.path() + u"/input.conf"_s));
    // No user config leak: no config-dir, no scripts, no user options.
    QCOMPARE(options.size(), 1);
}

void TestCompatController::skipSuppressesPrompt()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    controller.startupOptions();
    controller.skipAndDontAskAgain();

    QVERIFY(!controller.firstRunPromptNeeded());
    QVERIFY(controller.configDetected());
    QCOMPARE(controller.startupOptions(), QVariantMap{});
}

void TestCompatController::decisionPersistsAcrossInstances()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());
    const QString settingsFile = appData.filePath(u"settings.json"_s);

    {
        CompatController controller(settingsFile, appData.path(),
                                    fixtureDir());
        controller.startupOptions();
        controller.chooseFullImport();
    }
    CompatController controller(settingsFile, appData.path(), fixtureDir());
    const QVariantMap options = controller.startupOptions();
    QVERIFY(!options.isEmpty());
    QVERIFY(!controller.firstRunPromptNeeded());
    QCOMPARE(options.value(u"vo"_s).toString(), u"libmpv"_s);
}

void TestCompatController::missingConfigDirIsUndetected()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(),
                                u"/definitely/not/a/real/mpv/dir"_s);
    controller.startupOptions();
    QVERIFY(!controller.configDetected());
    QVERIFY(!controller.firstRunPromptNeeded());
}

void TestCompatController::requestWizardTogglesProperty()
{
    QTemporaryDir appData;
    QVERIFY(appData.isValid());

    CompatController controller(appData.filePath(u"settings.json"_s),
                                appData.path(), fixtureDir());
    QVERIFY(!controller.wizardRequested());
    controller.requestWizard();
    QVERIFY(controller.wizardRequested());
    controller.chooseStandalone();
    QVERIFY(!controller.wizardRequested());
}

QTEST_MAIN(TestCompatController)
#include "test_compat_controller.moc"
