#include <QCommandLineParser>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QTimer>
#include <QUrl>
#include <QtQml/QtQml>

#include "history/HistoryStore.h"
#include "mpv/MpvEngine.h"
#include "mpv/MpvItem.h"

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
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
    cli.process(app);

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    // Destruction order matters: `view` is declared after `mpvEngine`, so it
    // is destroyed first; QML items (and their mpv render contexts) are
    // released before the mpv core goes away. See MpvItem / MpvEngine docs.
    MpvEngine mpvEngine;
    if (!mpvEngine.initialize()) {
        qCritical("Failed to initialize mpv: %s", qPrintable(mpvEngine.lastError()));
        return 1;
    }

    // Declared after the engine so it is destroyed first (its connections to
    // the engine must not outlive the engine's mpv handle).
    HistoryStore historyStore(&mpvEngine, HistoryStore::defaultFilePath(),
                              &mpvEngine);

    QQmlApplicationEngine view;
    view.rootContext()->setContextProperty(QStringLiteral("mpvEngine"), &mpvEngine);
    view.rootContext()->setContextProperty(QStringLiteral("historyStore"),
                                           &historyStore);
    qmlRegisterType<MpvItem>("EmoPlay.Mpv", 1, 0, "MpvItem");
    view.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (view.rootObjects().isEmpty())
        return 1;

    const QStringList files = cli.positionalArguments();
    if (!files.isEmpty())
        mpvEngine.loadFile(files.first());

    if (cli.isSet(smokeTestOption))
        QTimer::singleShot(3000, &app, &QCoreApplication::quit);

    return app.exec();
}
