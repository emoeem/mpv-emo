#include <QtTest>

#include "config/MpvConfigParser.h"

using namespace Qt::Literals::StringLiterals;

class TestConfigParser : public QObject
{
    Q_OBJECT

private slots:
    void mpvConfBasicOptions();
    void mpvConfCommentsAndBlanks();
    void mpvConfProfiles();
    void mpvConfHashInsideValue();
    void mpvConfMissingEquals();
    void mpvConfCrlfLineEndings();
    void inputConfBasicBinding();
    void inputConfQuotedArgs();
    void inputConfInlineComment();
    void inputConfHashWithoutSpaceIsPartOfToken();
    void inputConfKeyOnlyLineIsSkipped();
    void tokenizeEdgeCases();
    void emptyInputYieldsNothing();
};

void TestConfigParser::mpvConfBasicOptions()
{
    const QVector<MpvConfEntry> entries =
        MpvConfigParser::parseMpvConf(u"vo=gpu-next\nhwdec=auto-safe\nfullscreen=yes\n"_s);

    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].kind, MpvConfEntry::Kind::Option);
    QCOMPARE(entries[0].key, u"vo"_s);
    QCOMPARE(entries[0].value, u"gpu-next"_s);
    QCOMPARE(entries[0].lineNumber, 1);
    QCOMPARE(entries[1].key, u"hwdec"_s);
    QCOMPARE(entries[1].value, u"auto-safe"_s);
    QCOMPARE(entries[2].key, u"fullscreen"_s);
    QCOMPARE(entries[2].value, u"yes"_s);
    QCOMPARE(entries[2].lineNumber, 3);
}

void TestConfigParser::mpvConfCommentsAndBlanks()
{
    const QString text = u"# leading comment\n\n   \n\tvolume=50\n  # indented comment\n"_s;
    const QVector<MpvConfEntry> entries = MpvConfigParser::parseMpvConf(text);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].key, u"volume"_s);
    QCOMPARE(entries[0].value, u"50"_s);
    QCOMPARE(entries[0].lineNumber, 4);
}

void TestConfigParser::mpvConfProfiles()
{
    const QString text = u"[fast]\nprofile-cond=get(\"video-bitrate\") > 10000\nvo=gpu-next\n"_s;
    const QVector<MpvConfEntry> entries = MpvConfigParser::parseMpvConf(text);

    QCOMPARE(entries.size(), 3);
    QCOMPARE(entries[0].kind, MpvConfEntry::Kind::ProfileHeader);
    QCOMPARE(entries[0].profileName, u"fast"_s);
    QCOMPARE(entries[1].kind, MpvConfEntry::Kind::Option);
}

void TestConfigParser::mpvConfHashInsideValue()
{
    // mpv.conf has no inline comments: '#' inside a value is preserved.
    const QVector<MpvConfEntry> entries =
        MpvConfigParser::parseMpvConf(u"background-color=#000000\n"_s);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].value, u"#000000"_s);
}

void TestConfigParser::mpvConfMissingEquals()
{
    const QVector<MpvConfEntry> entries =
        MpvConfigParser::parseMpvConf(u"not-an-option-line\n"_s);

    QCOMPARE(entries.size(), 1);
    QCOMPARE(entries[0].kind, MpvConfEntry::Kind::Unknown);
}

void TestConfigParser::mpvConfCrlfLineEndings()
{
    const QVector<MpvConfEntry> entries =
        MpvConfigParser::parseMpvConf(u"vo=gpu-next\r\nvolume=80\r\n"_s);

    QCOMPARE(entries.size(), 2);
    QCOMPARE(entries[0].value, u"gpu-next"_s);
    QCOMPARE(entries[1].value, u"80"_s);
}

void TestConfigParser::inputConfBasicBinding()
{
    const QVector<MpvInputBinding> bindings =
        MpvConfigParser::parseInputConf(u"Space cycle pause\n"_s);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings[0].key, u"Space"_s);
    QCOMPARE(bindings[0].command, QStringList{u"cycle"_s, u"pause"_s});
    QCOMPARE(bindings[0].comment, QString());
    QCOMPARE(bindings[0].lineNumber, 1);
}

void TestConfigParser::inputConfQuotedArgs()
{
    const QVector<MpvInputBinding> bindings =
        MpvConfigParser::parseInputConf(u"a loadfile \"my file.mkv\"\n"_s);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings[0].command, QStringList{u"loadfile"_s, u"my file.mkv"_s});
}

void TestConfigParser::inputConfInlineComment()
{
    // '#' starts a comment when preceded by whitespace.
    const QVector<MpvInputBinding> bindings =
        MpvConfigParser::parseInputConf(u"Ctrl+s screenshot # take a shot\n"_s);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings[0].key, u"Ctrl+s"_s);
    QCOMPARE(bindings[0].command, QStringList{u"screenshot"_s});
    QCOMPARE(bindings[0].comment, u"take a shot"_s);
}

void TestConfigParser::inputConfHashWithoutSpaceIsPartOfToken()
{
    const QVector<MpvInputBinding> bindings =
        MpvConfigParser::parseInputConf(u"a#b stop\n"_s);

    QCOMPARE(bindings.size(), 1);
    QCOMPARE(bindings[0].key, u"a#b"_s);
    QCOMPARE(bindings[0].command, QStringList{u"stop"_s});
}

void TestConfigParser::inputConfKeyOnlyLineIsSkipped()
{
    const QVector<MpvInputBinding> bindings = MpvConfigParser::parseInputConf(u"x\n"_s);
    QVERIFY(bindings.isEmpty());
}

void TestConfigParser::tokenizeEdgeCases()
{
    QVERIFY(MpvConfigParser::tokenizeCommand(QString()).isEmpty());
    QCOMPARE(MpvConfigParser::tokenizeCommand(u"one"_s), QStringList{u"one"_s});
    QCOMPARE(MpvConfigParser::tokenizeCommand(u"  spaced   out  "_s),
             QStringList{u"spaced"_s, u"out"_s});
    QCOMPARE(MpvConfigParser::tokenizeCommand(u"\"quoted arg\" tail"_s),
             QStringList{u"quoted arg"_s, u"tail"_s});
    // Unterminated quote extends to the end of the string.
    QCOMPARE(MpvConfigParser::tokenizeCommand(u"\"open arg"_s), QStringList{u"open arg"_s});
}

void TestConfigParser::emptyInputYieldsNothing()
{
    QVERIFY(MpvConfigParser::parseMpvConf(QString()).isEmpty());
    QVERIFY(MpvConfigParser::parseInputConf(QString()).isEmpty());
    QVERIFY(MpvConfigParser::parseMpvConf(u"\n\n# only comments\n"_s).isEmpty());
}

QTEST_GUILESS_MAIN(TestConfigParser)
#include "test_config_parser.moc"
