#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

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

private:
    QTemporaryDir m_dir;
};

void TestMusicPage::initTestCase()
{
    QStandardPaths::setTestModeEnabled(true);
    QQuickStyle::setStyle(QStringLiteral("Basic"));
}

void TestMusicPage::pageInstantiates()
{
    QQmlEngine engine;
    MusicController controller(nullptr, nullptr);
    engine.rootContext()->setContextProperty(
        QStringLiteral("musicController"), &controller);

    QQmlComponent component(
        &engine, QUrl::fromLocalFile(
                     QStringLiteral(EMOPLAY_SOURCE_DIR "/qml/MusicPage.qml")));
    std::unique_ptr<QObject> page(component.create());
    QVERIFY2(page, qPrintable(component.errorString()));

    // The equalizer repeater built one slider per band (counted by type,
    // not by reading the var property back — var-to-QVariant conversion is
    // lossy for object references).
    int sliderCount = 0;
    const QList<QObject *> children = page->findChildren<QObject *>();
    for (const QObject *child : children) {
        if (child->inherits("QQuickSlider"))
            ++sliderCount;
    }
    QCOMPARE(sliderCount, controller.bandCount());

    // Collapsed by default; exposing it again must not error out.
    QCOMPARE(page->property("eqPanelVisible").toBool(), false);
    QVERIFY(page->setProperty("eqPanelVisible", true));
    QCOMPARE(page->property("eqPanelVisible").toBool(), true);
}

QTEST_MAIN(TestMusicPage)
#include "test_music_page.moc"
