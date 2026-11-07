#include <QTemporaryDir>
#include <QtTest>

#include "music/LrcParser.h"

using namespace Qt::Literals::StringLiterals;

// Pure-logic tests for the .lrc parser: no mpv, no GUI, no real HOME.
class TestLrcParser : public QObject
{
    Q_OBJECT

private slots:
    void basicTimestamps();
    void multiTimestampLine();
    void metadataTags();
    void bomStripped();
    void crlfLineEndings();
    void fractionDigitVariants();
    void unsortedInputSortedByTime();
    void unknownTagsAndPlainLinesIgnored();
    void emptyTextLinesKept();
    void parseFileMissingReturnsEmpty();
    void lrcCandidates();

private:
    static LrcParser::LrcData parse(const QString &content)
    {
        return LrcParser::parse(content);
    }
};

void TestLrcParser::basicTimestamps()
{
    const LrcParser::LrcData data = parse(
        u"[00:12.00]first\n[01:05]second\n"_s);
    QCOMPARE(data.lines.size(), 2);
    QCOMPARE(data.lines[0].time, 12.0);
    QCOMPARE(data.lines[0].text, u"first"_s);
    QCOMPARE(data.lines[1].time, 65.0);
    QCOMPARE(data.lines[1].text, u"second"_s);
}

void TestLrcParser::multiTimestampLine()
{
    const LrcParser::LrcData data = parse(u"[00:12.00][00:45.50]chorus\n"_s);
    QCOMPARE(data.lines.size(), 2);
    QCOMPARE(data.lines[0].time, 12.0);
    QCOMPARE(data.lines[1].time, 45.5);
    QCOMPARE(data.lines[0].text, u"chorus"_s);
    QCOMPARE(data.lines[1].text, u"chorus"_s);
}

void TestLrcParser::metadataTags()
{
    const LrcParser::LrcData data = parse(
        u"[ti:Song Title]\n[ar:The Artist]\n[al:The Album]\n[00:01.00]line\n"_s);
    QCOMPARE(data.title, u"Song Title"_s);
    QCOMPARE(data.artist, u"The Artist"_s);
    QCOMPARE(data.album, u"The Album"_s);
    QCOMPARE(data.lines.size(), 1);
}

void TestLrcParser::bomStripped()
{
    // UTF-8 BOM decoded to U+FEFF before the first tag.
    const LrcParser::LrcData data = parse(
        u"\uFEFF[ti:T]\n[00:01.00]line\n"_s);
    QCOMPARE(data.title, u"T"_s);
    QCOMPARE(data.lines.size(), 1);
    QCOMPARE(data.lines[0].text, u"line"_s);
}

void TestLrcParser::crlfLineEndings()
{
    const LrcParser::LrcData data =
        parse(u"[ti:T]\r\n[00:01.00]one\r\n[00:02.00]two\r\n"_s);
    QCOMPARE(data.title, u"T"_s);
    QCOMPARE(data.lines.size(), 2);
    // The \r must not leak into the text.
    QCOMPARE(data.lines[0].text, u"one"_s);
    QCOMPARE(data.lines[1].text, u"two"_s);
}

void TestLrcParser::fractionDigitVariants()
{
    const LrcParser::LrcData data = parse(
        u"[00:01.5]a\n[00:02.25]b\n[00:03.250]c\n[00:04]d\n"_s);
    QCOMPARE(data.lines.size(), 4);
    QCOMPARE(data.lines[0].time, 1.5);
    QCOMPARE(data.lines[1].time, 2.25);
    QCOMPARE(data.lines[2].time, 3.25);
    QCOMPARE(data.lines[3].time, 4.0);
}

void TestLrcParser::unsortedInputSortedByTime()
{
    const LrcParser::LrcData data =
        parse(u"[00:20.00]later\n[00:10.00]earlier\n"_s);
    QCOMPARE(data.lines.size(), 2);
    QCOMPARE(data.lines[0].text, u"earlier"_s);
    QCOMPARE(data.lines[1].text, u"later"_s);
}

void TestLrcParser::unknownTagsAndPlainLinesIgnored()
{
    const LrcParser::LrcData data = parse(
        u"[by:Tool]\n[offset:+500]\njust some text\n\n[00:01.00]real\n"_s);
    QCOMPARE(data.lines.size(), 1);
    QCOMPARE(data.lines[0].text, u"real"_s);
    QVERIFY(data.title.isEmpty());
}

void TestLrcParser::emptyTextLinesKept()
{
    // Timestamp-only lines are common interlude markers; they stay.
    const LrcParser::LrcData data = parse(u"[00:05.00]\n[00:10.00]after\n"_s);
    QCOMPARE(data.lines.size(), 2);
    QCOMPARE(data.lines[0].text, QString());
    QCOMPARE(data.lines[0].time, 5.0);
    QCOMPARE(data.lines[1].text, u"after"_s);
}

void TestLrcParser::parseFileMissingReturnsEmpty()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const LrcParser::LrcData data =
        LrcParser::parseFile(dir.filePath(u"missing.lrc"_s));
    QVERIFY(data.lines.isEmpty());
    QVERIFY(data.title.isEmpty());
}

void TestLrcParser::lrcCandidates()
{
    const QStringList candidates = LrcParser::lrcCandidates(
        u"/music/artist/song.mp3"_s);
    QCOMPARE(candidates.size(), 2);
    QCOMPARE(candidates[0], u"/music/artist/song.lrc"_s);
    QCOMPARE(candidates[1], u"/music/artist/song.LRC"_s);
    // Multi-part extensions: only the last one is replaced.
    QCOMPARE(LrcParser::lrcCandidates(u"/m/a.b.flac"_s).at(0),
             u"/m/a.b.lrc"_s);
    QVERIFY(LrcParser::lrcCandidates(QString()).isEmpty());
}

// Pure-logic test: QTEST_GUILESS_MAIN (a QGuiApplication would need a
// display or an offscreen platform; see execution-log batch 5).
QTEST_GUILESS_MAIN(TestLrcParser)
#include "test_lrc_parser.moc"
