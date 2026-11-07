#include <QStandardPaths>
#include <QtTest>

#include <QDebug>
#include <QQmlError>
#include <memory>

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQmlExpression>
#include <QQuickStyle>

#include "music/MusicController.h"

using namespace Qt::Literals::StringLiterals;

// Headless instantiation test for qml/MusicPage.qml (P7 music page).
//
// The CI smoke test only boots Main.qml — the MusicPage loader stays
// inactive there while nothing is playing, so a broken MusicPage would
// pass CI unnoticed. This test mounts the real page offscreen with a
// null-engine controller: creation must succeed without QML errors and
// the band sliders must instantiate. Pure UI logic beyond creation is
// verified on a real machine (see docs/architecture.md known limits).
class TestMusicPage : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void pageInstantiates();
};

void TestMusicPage::initTestCase()
{
    // Keep defaultFilePath() calls (if any code touches it) off the real
    // user data; the page itself is instantiated with a null-engine
    // controller and a null settings pointer.
    QStandardPaths::setTestModeEnabled(true);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void TestMusicPage::pageInstantiates()
{
    QQmlEngine engine;
    MusicController controller(nullptr, nullptr);
    engine.rootContext()->setContextProperty(
        QStringLiteral("musicController"), &controller);

    // Record QML engine warnings: a half-created page (the root survives
    // while a child failed) comes back non-null with the errors only in
    // the engine's warning channel — and CI is the only compiler, so its
    // log is the only diagnostic there is.
    QStringList qmlMessages;
    QObject::connect(&engine, &QQmlEngine::warnings,
                     [&](const QList<QQmlError> &warnings) {
                         for (const QQmlError &warning : warnings)
                             qmlMessages.append(warning.toString());
                     });

    QQmlComponent component(
        &engine, QUrl::fromLocalFile(
                     QStringLiteral(EMOPLAY_SOURCE_DIR "/qml/MusicPage.qml")));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));
    // A non-null create() can still carry errors; surface them either way.
    QVERIFY2(component.errors().isEmpty(),
             qPrintable(component.errorString()));
    QVERIFY2(qmlMessages.isEmpty(),
             qPrintable(qmlMessages.join(u"\n"_s)));

    // The equalizer repeater built one slider per band (counted by type,
    // not by reading the var property back — var-to-QVariant conversion is
    // lossy for object references).
    int sliderCount = 0;
    QStringList childTypes;
    const QObject *repeater = nullptr;
    const QList<QObject *> children = page->findChildren<QObject *>();
    for (const QObject *child : children) {
        childTypes.append(QString::fromLatin1(child->metaObject()->className()));
        if (!repeater && child->inherits("QQuickRepeater"))
            repeater = child;
        if (child->inherits("QQuickSlider"))
            ++sliderCount;
    }
    if (sliderCount != controller.bandCount()) {
        qWarning().nospace().noquote()
            << "instantiated item tree (" << childTypes.size()
            << " children): " << childTypes.join(u", "_s);
        if (repeater)
            qWarning() << "repeater model:" << repeater->property("model")
                       << "count:" << repeater->property("count");
        // What QML itself sees for the context property and its reads.
        for (const QString &expression :
             {u"typeof musicController"_s, u"musicController"_s,
              u"musicController.bandCount"_s,
              u"musicController.equalizerGains.length"_s}) {
            bool undefinedValue = false;
            QQmlExpression eval(engine.rootContext(), page.get(), expression);
            const QVariant value = eval.evaluate(&undefinedValue);
            const QString error =
                eval.error().isValid() ? eval.error().toString() : QString();
            qWarning().nospace().noquote()
                << "qml expr '" << expression << "' ->" << value
                << "undefined:" << undefinedValue
                << (error.isEmpty() ? QString()
                                    : u" [error: "_s + error + u"]"_s);
        }
        // Bisect: does a minimal Repeater create items on this runner?
        QQmlComponent probeComponent(&engine);
        probeComponent.setData(
            "import QtQuick\nimport QtQuick.Controls\n"
            "Item { Repeater { model: 3; delegate: Item { objectName: "
            "\"probe\" + index } } }",
            QUrl(QStringLiteral("qrc:/probe.qml")));
        std::unique_ptr<QObject> probe(probeComponent.create());
        int probeCount = -1;
        if (probe) {
            for (const QObject *child : probe->findChildren<QObject *>())
                if (child->inherits("QQuickRepeater"))
                    probeCount = child->property("count").toInt();
        }
        qWarning() << "minimal repeater probe: created ="
                   << (probe != nullptr) << "count =" << probeCount
                   << "errors:" << probeComponent.errorString();
    }
    QCOMPARE(sliderCount, controller.bandCount());

    // Collapsed by default; exposing it again must not error out.
    QCOMPARE(page->property("eqPanelVisible").toBool(), false);
    QVERIFY(page->setProperty("eqPanelVisible", true));
    QCOMPARE(page->property("eqPanelVisible").toBool(), true);
}

QTEST_MAIN(TestMusicPage)
#include "test_music_page.moc"
