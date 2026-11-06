#include <QtTest>

#include "compat/MpvKeyName.h"

using namespace Qt::Literals::StringLiterals;

// Headless tests for Qt key event -> mpv key name mapping, the piece that
// lets imported input.conf bindings fire in the embedded UI.
class TestMpvKeyName : public QObject
{
    Q_OBJECT

private slots:
    void printableTextBecomesKeyName();
    void namedKeys();
    void shiftedLetter();
    void shiftedPunctuationPrefersText();
    void ctrlCombination();
    void altAndMetaCombinations();
    void unmappableKeysReturnEmpty();
    void spaceIsNamedNotLiteral();
};

void TestMpvKeyName::printableTextBecomesKeyName()
{
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_A, Qt::NoModifier, u"a"_s), u"a"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_0, Qt::NoModifier, u"0"_s), u"0"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Period, Qt::NoModifier, u"."_s),
             u"."_s);
    // Text takes precedence over the code-based name for punctuation.
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Comma, Qt::NoModifier, u","_s),
             u","_s);
}

void TestMpvKeyName::namedKeys()
{
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Left, Qt::NoModifier, QString()),
             u"left"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Right, Qt::NoModifier, QString()),
             u"right"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_PageUp, Qt::NoModifier, QString()),
             u"PGUP"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_PageDown, Qt::NoModifier, QString()),
             u"PGDWN"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_F5, Qt::NoModifier, QString()),
             u"f5"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_F12, Qt::NoModifier, QString()),
             u"f12"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Home, Qt::NoModifier, QString()),
             u"HOME"_s);
}

void TestMpvKeyName::shiftedLetter()
{
    // Capital A is mpv's "shift+a" (same binding as input.conf "A").
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_A, Qt::ShiftModifier, u"A"_s),
             u"shift+a"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Left, Qt::ShiftModifier, QString()),
             u"shift+left"_s);
}

void TestMpvKeyName::shiftedPunctuationPrefersText()
{
    // Shifted punctuation uses the canonical modifier form: mpv parses
    // "shift+." exactly like the ">" key name in an input.conf binding.
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Period, Qt::ShiftModifier, u">"_s),
             u"shift+."_s);
}

void TestMpvKeyName::ctrlCombination()
{
    // Ctrl+letter: event text is a control character, so the name comes from
    // the key code.
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_S, Qt::ControlModifier, u"\x03"_s),
             u"ctrl+s"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_S,
                                Qt::ControlModifier | Qt::ShiftModifier,
                                QString()),
             u"ctrl+shift+s"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Left, Qt::ControlModifier, QString()),
             u"ctrl+left"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Comma, Qt::ControlModifier, QString()),
             u"ctrl+,"_s);
}

void TestMpvKeyName::altAndMetaCombinations()
{
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_H, Qt::AltModifier, QString()),
             u"alt+h"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_H,
                                Qt::ControlModifier | Qt::AltModifier,
                                QString()),
             u"ctrl+alt+h"_s);
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_A, Qt::MetaModifier, QString()),
             u"meta+a"_s);
}

void TestMpvKeyName::unmappableKeysReturnEmpty()
{
    // Qt::Key_MediaTogglePlayPause etc. have no mpv equivalent here.
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_MediaTogglePlayPause,
                                Qt::NoModifier, QString()),
             QString());
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_unknown, Qt::NoModifier, QString()),
             QString());
    // Modifier-only events have no text and no base key.
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Shift, Qt::ShiftModifier, QString()),
             QString());
}

void TestMpvKeyName::spaceIsNamedNotLiteral()
{
    // Space must map to mpv's named key, not a literal ' '.
    QCOMPARE(MpvKeyName::fromQt(Qt::Key_Space, Qt::NoModifier, u" "_s),
             u"SPACE"_s);
}

QTEST_GUILESS_MAIN(TestMpvKeyName)
#include "test_mpv_key_name.moc"
