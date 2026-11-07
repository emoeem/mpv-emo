#include <clocale>

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QtQml/QtQml>

#include "compat/CompatController.h"
#include "history/HistoryStore.h"
#include "library/LibraryModel.h"
#include "library/LibraryStore.h"
#include "library/ThumbnailService.h"
#include "mpv/MpvEngine.h"
#include "mpv/MpvItem.h"
#include "music/MusicController.h"
#include "plugins/PluginManager.h"
#include "plugins/RemoteManifestSource.h"
#include "quality/IccController.h"
#include "quality/InterpolationController.h"
#include "quality/QualityController.h"
#include "update/UpdateChecker.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    // libmpv refuses to initialize unless LC_NUMERIC is "C" (it parses
    // numbers locale-independently); Qt switches the locale from the
    // environment during QGuiApplication construction, so this must happen
    // after the app object exists.
    std::setlocale(LC_ALL, "");
    std::setlocale(LC_NUMERIC, "C");
    QGuiApplication::setOrganizationName(QStringLiteral("EmoPlay"));
    QGuiApplication::setApplicationName(QStringLiteral("EmoPlay"));
    QGuiApplication::setApplicationVersion(QString::fromLatin1(EMOPLAY_VERSION));

    QCommandLineParser cli;
    cli.setApplicationDescription(
        QStringLiteral("EmoPlay - media player built on libmpv and Qt6/QML"));
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption smokeTestOption(
        QStringLiteral("smoke-test"),
        QStringLiteral("Start up, render for a few seconds, then exit (for CI)."));
    cli.addOption(smokeTestOption);
    QCommandLineOption screenshotOption(
        QStringLiteral("screenshot"),
        QStringLiteral("Save a window capture to <path> after startup, then "
                       "exit (UI verification)."),
        QStringLiteral("screenshot"));
    cli.addOption(screenshotOption);
    cli.process(app);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // P4 compat startup order: read settings.json -> scan/apply the recorded
    // import decision -> initialize(options) -> (QML) first-run wizard when a
    // config is detected but undecided. Declared before the engine: the
    // option map must exist before initialize() runs.
    CompatController compatController;
    QObject::connect(&compatController, &CompatController::errorOccurred,
                     [](const QString &message) {
                         qCritical("compat: %s", qPrintable(message));
                     });
    // Non-const: the P8 plugin manager appends enabled script plugin paths
    // to the same map below before initialize() applies it.
    QVariantMap compatOptions = compatController.startupOptions();
    if (!compatOptions.isEmpty())
        qInfo("Applying %lld imported mpv option(s)",
              static_cast<long long>(compatOptions.size()));

    // P8 plugin store: enabled script plugins are APPENDED to the "scripts"
    // option the compat layer assembled (the user's imported script list is
    // never replaced); applies the same initialize() option map below.
    PluginManager pluginManager(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        PluginManager::defaultCatalogPath());
    QObject::connect(&pluginManager, &PluginManager::errorOccurred,
                     [](const QString &message) {
                         qWarning("plugins: %s", qPrintable(message));
                     });

    // P8 v2 online manifest sources: fetches manager.json-style catalogs
    // (the default upstream source + user-added ones) over
    // QNetworkAccessManager, caches them under AppData/plugin-sources/ and
    // pushes the fetched manifests into the plugin manager so online entries
    // install like builtin ones. Network failures degrade to the last cached
    // list — never fatal.
    RemoteManifestSource remoteSources(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation),
        &pluginManager);
    QObject::connect(&remoteSources, &RemoteManifestSource::errorOccurred,
                     [](const QString &message) {
                         qWarning("plugin-sources: %s", qPrintable(message));
                     });

    // P8 plugin store: enabled script plugins are APPENDED to the "scripts"
    // option the compat layer assembled (the user's imported script list is
    // never replaced); applies the same initialize() option map below. Runs
    // after RemoteManifestSource so plugins installed from online sources in
    // earlier sessions are appended too (RemoteManifestSource pushes its
    // cached manifests into the manager at construction).
    pluginManager.appendScriptOptions(compatOptions);

    // Destruction order matters: `view` is declared after `mpvEngine`, so it
    // is destroyed first; QML items (and their mpv render contexts) are
    // released before the mpv core goes away. See MpvItem / MpvEngine docs.
    MpvEngine mpvEngine;
    if (!mpvEngine.initialize(compatOptions)) {
        qCritical("Failed to initialize mpv: %s", qPrintable(mpvEngine.lastError()));
        return 1;
    }

    // Declared after the engine so it is destroyed first (its connections to
    // the engine must not outlive the engine's mpv handle).
    HistoryStore historyStore(&mpvEngine, HistoryStore::defaultFilePath(),
                              &mpvEngine);

    // P6 media library: persisted folder list + scanned entries; the model
    // wraps the store for QML. A file-loaded event stamps the matching
    // entry's last-played timestamp (unknown files are ignored).
    LibraryStore libraryStore(LibraryStore::defaultFilePath(), &mpvEngine);
    // P6 v2 thumbnails: on-demand ffmpeg-generated JPEGs cached under
    // AppData/thumbnails/<sha1(path+mtime)>.jpg. The availability probe is
    // asynchronous; while ffmpeg is missing the model simply keeps empty
    // thumbnails and the drawer degrades to type icons.
    ThumbnailService thumbnailService(
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/thumbnails"));
    thumbnailService.probe();
    LibraryModel libraryModel(&libraryStore, &thumbnailService);
    QObject::connect(&mpvEngine, &MpvEngine::fileLoaded, &libraryStore,
                     [&mpvEngine, &libraryStore] {
                         libraryStore.notePlayed(
                             mpvEngine.getProperty(QStringLiteral("path"))
                                 .toString());
                     });

    // P5 quality engine: shader sources come from the imported user config
    // directory when that config was actually imported (consent-driven,
    // read-only); otherwise from EmoPlay's own app-data shaders directory.
    const QString importDecision =
        compatController.settings().importDecision();
    QString qualityShadersDir;
    if ((importDecision == QLatin1String("full")
         || importDecision == QLatin1String("selective"))
        && !compatController.sourceDir().isEmpty()) {
        qualityShadersDir =
            compatController.sourceDir() + QStringLiteral("/shaders");
    } else {
        qualityShadersDir =
            QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)
            + QStringLiteral("/shaders");
    }

    // Declared after the engine (same destruction contract as above). The
    // persisted preferences take effect through startup() (manual shader
    // list) and on each file-loaded event (profile decision table).
    QualityController qualityController(&mpvEngine,
                                        &compatController.settings(),
                                        qualityShadersDir, &mpvEngine);
    QObject::connect(&qualityController, &QualityController::errorOccurred,
                     [](const QString &message) {
                         qCritical("quality: %s", qPrintable(message));
                     });
    qualityController.startup();

    // P5 v2: VapourSynth frame interpolation and ICC display configuration.
    // Both follow the honest external-dependency pattern — the scripts,
    // runtimes and profile files stay external, and anything missing
    // stands down visibly instead of pretending to work. The interpolation
    // scripts directory is EmoPlay's own app-data vs/ directory; the ICC
    // cache follows the P4 redirect table (AppData/icc_cache).
    const QString appDataDir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    InterpolationController interpolationController(
        &mpvEngine, &compatController.settings(),
        appDataDir + QStringLiteral("/vs"), &mpvEngine);
    QObject::connect(&interpolationController,
                     &InterpolationController::errorOccurred,
                     [](const QString &message) {
                         qCritical("interpolation: %s", qPrintable(message));
                     });
    interpolationController.startup();

    IccController iccController(&mpvEngine, &compatController.settings(),
                                appDataDir, &mpvEngine);
    QObject::connect(&iccController, &IccController::errorOccurred,
                     [](const QString &message) {
                         qCritical("icc: %s", qPrintable(message));
                     });
    iccController.startup();

    // P7 music mode: lyrics + equalizer service. Declared after the engine
    // (same destruction contract as above). Shares the compat layer's
    // AppSettings, so the "music" section lives in the same settings.json.
    MusicController musicController(&mpvEngine, &compatController.settings(),
                                    &mpvEngine);
    musicController.startup();

    // P9 update check: one background GET of the GitHub releases API per
    // launch (delayed so it never competes with startup). The result is
    // only ever reported — the checker never downloads or installs
    // anything, and every failure stays silent (the badge just stays
    // hidden). Declared before `view` per the destruction-order contract.
    UpdateChecker updateChecker(QString::fromLatin1(EMOPLAY_VERSION));
    QTimer::singleShot(3000, &updateChecker, &UpdateChecker::check);

    QQmlApplicationEngine view;
    view.rootContext()->setContextProperty(QStringLiteral("mpvEngine"), &mpvEngine);
    view.rootContext()->setContextProperty(QStringLiteral("historyStore"),
                                           &historyStore);
    view.rootContext()->setContextProperty(QStringLiteral("compatController"),
                                           &compatController);
    view.rootContext()->setContextProperty(QStringLiteral("qualityController"),
                                           &qualityController);
    view.rootContext()->setContextProperty(
        QStringLiteral("interpolationController"), &interpolationController);
    view.rootContext()->setContextProperty(QStringLiteral("iccController"),
                                           &iccController);
    view.rootContext()->setContextProperty(QStringLiteral("musicController"),
                                           &musicController);
    view.rootContext()->setContextProperty(QStringLiteral("libraryStore"),
                                           &libraryStore);
    view.rootContext()->setContextProperty(QStringLiteral("libraryModel"),
                                           &libraryModel);
    view.rootContext()->setContextProperty(QStringLiteral("pluginManager"),
                                           &pluginManager);
    view.rootContext()->setContextProperty(QStringLiteral("remotePluginSources"),
                                           &remoteSources);
    view.rootContext()->setContextProperty(QStringLiteral("updateChecker"),
                                           &updateChecker);
    qmlRegisterType<MpvItem>("EmoPlay.Mpv", 1, 0, "MpvItem");
    view.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (view.rootObjects().isEmpty())
        return 1;

    const QStringList files = cli.positionalArguments();
    if (!files.isEmpty()) {
        // The mpv render context is created when the scene graph renders its
        // first frame. A loadfile issued before that hits vo=libmpv with no
        // render context: vo init fails and playback aborts to idle. Defer
        // the load until the first frame has been swapped.
        if (auto *window = qobject_cast<QQuickWindow *>(view.rootObjects().value(0))) {
            QMetaObject::Connection firstFrame = QObject::connect(
                window, &QQuickWindow::frameSwapped, &app,
                [&mpvEngine, files, window]() mutable {
                    window->disconnect();
                    QTimer::singleShot(0, &mpvEngine,
                                       [&mpvEngine, files]() {
                                           mpvEngine.loadFile(files.first());
                                       });
                });
        }
    }

    if (cli.isSet(smokeTestOption)) {
        // Capture list must be exactly [&]: clang and MSVC reject a redundant
        // explicit capture after a by-reference default (GCC only warns).
        QTimer::singleShot(3000, &app, [&]() {
            qInfo("smoke: idle-active=%s filename=%s",
                  mpvEngine.getProperty(QStringLiteral("idle-active")).toString().toUtf8().constData(),
                  mpvEngine.getProperty(QStringLiteral("filename")).toString().toUtf8().constData());
            QCoreApplication::quit();
        });
    }

    // UI verification: let the scene render, grab every top-level window to
    // the given path, then exit. Works under Xvfb (offscreen GL included).
    if (cli.isSet(screenshotOption)) {
        const QString path = cli.value(screenshotOption);
        QTimer::singleShot(3800, &app, [path]() {
            const QList<QWindow *> windows = QGuiApplication::topLevelWindows();
            for (QWindow *window : windows) {
                if (auto *quick = qobject_cast<QQuickWindow *>(window)) {
                    const QImage shot = quick->grabWindow();
                    if (!shot.save(path))
                        qWarning("screenshot: failed to save %s",
                                 qPrintable(path));
                }
            }
            QCoreApplication::quit();
        });
    }

    return app.exec();
}
