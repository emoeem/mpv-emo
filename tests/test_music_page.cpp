#include <QStandardPaths>
#include <QtTest>

#include <QDebug>
#include <QQmlError>
#include <QQuickItem>
#include <functional>
#include <memory>

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
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

    // The equalizer repeater builds one slider per band. Counted in the
    // VISUAL tree: Repeater delegates get their parentItem set but keep no
    // QObject parent (the Repeater owns them in C++), so a findChildren
    // walk never reaches them even though everything instantiated fine.
    const std::function<int(QQuickItem *)> countSliders =
        [&](QQuickItem *item) -> int {
        int count = item->inherits("QQuickSlider") ? 1 : 0;
        for (QQuickItem *child : item->childItems())
            count += countSliders(child);
        return count;
    };
    const int sliderCount =
        countSliders(qobject_cast<QQuickItem *>(page.get()));
    if (sliderCount != controller.bandCount()) {
        // Failure context: the QObject-parent walk (declarative children
        // only) plus the repeater's own view of the world.
        QStringList childTypes;
        for (const QObject *child : page->findChildren<QObject *>())
            childTypes.append(
                QString::fromLatin1(child->metaObject()->className()));
        qWarning().nospace().noquote()
            << "object-parent walk (" << childTypes.size()
            << " objects): " << childTypes.join(u", "_s);
        for (const QObject *child : page->findChildren<QObject *>())
            if (child->inherits("QQuickRepeater"))
                qWarning() << "repeater model:" << child->property("model")
                           << "count:" << child->property("count");
        // Bisect with the page's exact delegate shape; counted both ways
        // to show the QObject-parent vs visual-parent split.
        QQmlComponent probeComponent(&engine);
        probeComponent.setData(
            "import QtQuick\nimport QtQuick.Controls\nimport QtQuick.Layouts\n"
            "Item { Repeater { model: 3; delegate: ColumnLayout { Slider { } } } }",
            QUrl(QStringLiteral("qrc:/probe.qml")));
        std::unique_ptr<QObject> probe(probeComponent.create());
        int probeCount = -1;
        int probeSlidersObject = 0;
        int probeSlidersVisual = 0;
        if (probe) {
            for (const QObject *child : probe->findChildren<QObject *>()) {
                if (child->inherits("QQuickRepeater"))
                    probeCount = child->property("count").toInt();
                if (child->inherits("QQuickSlider"))
                    ++probeSlidersObject;
            }
            if (QQuickItem *probeItem = qobject_cast<QQuickItem *>(probe.get()))
                probeSlidersVisual = countSliders(probeItem);
        }
        qWarning() << "shape probe: created =" << (probe != nullptr)
                   << "count =" << probeCount
                   << "sliders(object walk) =" << probeSlidersObject
                   << "sliders(visual walk) =" << probeSlidersVisual
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
