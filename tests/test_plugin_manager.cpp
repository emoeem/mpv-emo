#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QtTest>
#include <functional>

#include "plugins/PluginManager.h"

using namespace Qt::Literals::StringLiterals;

// Headless PluginManager tests. All git invocations go through a synchronous
// fake (PluginGitRunner seam) — no network, no real git binary. Paths live in
// temporary directories; the real HOME is never touched.
//
// Covered here: path assembly, the scripts-merge logic (append-only, the P4
// compat layer's assembled value is preserved), the install/update/uninstall
// state machine including the replace-mode protective skip, check-mode
// semantics (comparison only), and state persistence across instances.
namespace {

class FakeGitRunner : public PluginGitRunner
{
public:
    std::function<PluginGitResult(const QStringList &, const QString &)>
        handler;
    QVector<QStringList> calls;

    void run(const QStringList &arguments,
             const QString &workingDirectory) override
    {
        calls.append(arguments);
        const PluginGitResult result =
            handler ? handler(arguments, workingDirectory)
                    : PluginGitResult{0, {}, {}, true};
        emit finished(arguments, workingDirectory, result);
    }

    bool sawCommand(const QByteArray &command) const
    {
        for (const QStringList &call : calls) {
            if (!call.isEmpty() && call.first() == QLatin1String(command))
                return true;
        }
        return false;
    }
};

// check mode with baseline / single-file destination / replace-mode
// shader pack: the three shapes the manager must tell apart. Escaped
// literals instead of a raw string: Qt 6.4's moc chokes on raw strings
// containing quotes and then reports "No relevant classes found" for this
// file's Q_OBJECT test class (undefined vtable at link time).
static const char kCatalogJson[] =
    "{\"sources\": ["
    "{\"name\": \"uosc_danmaku\","
    " \"url\": \"https://git.example/uosc_danmaku.git\","
    " \"branch\": \"main\", \"source\": \".\","
    " \"destination\": \"scripts/uosc_danmaku\","
    " \"mode\": \"check\", \"baseline\": \"old1\"},"
    "{\"name\": \"mpv360\","
    " \"url\": \"https://git.example/mpv360.git\","
    " \"branch\": \"master\", \"source\": \".\","
    " \"destination\": \"scripts/mpv360.lua\", \"mode\": \"check\"},"
    "{\"name\": \"anime4k\","
    " \"url\": \"https://git.example/anime4k.git\","
    " \"branch\": \"master\", \"source\": \"glsl\","
    " \"destination\": \"shaders/Anime4K/glsl\", \"mode\": \"replace\"}"
    "]}";

QByteArray catalogJson()
{
    return QByteArray(kCatalogJson);
}

bool writeFile(const QString &path, const QByteArray &content)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(content);
    return true;
}

QVariantMap pluginEntry(const PluginManager &manager, const QString &name)
{
    const QVariantList plugins = manager.plugins();
    for (const QVariant &value : plugins) {
        const QVariantMap entry = value.toMap();
        if (entry.value(u"name"_s).toString() == name)
            return entry;
    }
    return {};
}

// Creates the target directory of a clone with an entry marker file, like a
// real successful clone of a script plugin.
PluginGitResult fakeClone(const QStringList &arguments)
{
    if (arguments.value(0) == QLatin1String("clone")) {
        const QString target = arguments.last();
        if (!QDir().mkpath(target)) {
            PluginGitResult failure;
            failure.standardError = QStringLiteral("mkpath failed");
            return failure;
        }
        QFile marker(target + QStringLiteral("/main.lua"));
        if (!marker.open(QIODevice::WriteOnly)) {
            PluginGitResult failure;
            failure.standardError = QStringLiteral("marker failed");
            return failure;
        }
        marker.write("fake\n");
    }
    return PluginGitResult{0, {}, {}, true};
}

struct Fixture
{
    QTemporaryDir dir;
    FakeGitRunner git;
    QString catalogPath;
    QString pluginsRoot;

    Fixture()
    {
        catalogPath = dir.filePath(u"catalog.json"_s);
        writeFile(catalogPath, catalogJson());
        pluginsRoot = dir.path() + u"/plugins"_s;
    }
};

} // namespace

class TestPluginManager : public QObject
{
    Q_OBJECT

private slots:
    void scriptSeparatorMatchesMpvPathList();
    void scriptEntryPathsCoverDestinationShapes();
    void scriptEntryPathsIgnoreShaderPacks();
    void mergeScriptOptionsCreatesAppendsAndProtects();
    void installClonesAndRecordsHead();
    void installRefusedWhenAlreadyInstalled();
    void statePersistsAcrossInstances();
    void appendScriptOptionsCoexistsWithUserScripts();
    void replaceUpdateSwapsAtomicallyWithBackup();
    void replaceUpdateRefusedOnLocalModifications();
    void checkModeUpdateComparesBaselineWithoutTouchingFiles();
    void uninstallRemovesDirectoryAndState();
    void planOperationListsChanges();
    void checkAllRunsForEveryEntry();

private:
    static QStringList captureFinish(PluginManager &manager, bool *ok,
                                     QString *message, QString *action);
};

QStringList TestPluginManager::captureFinish(PluginManager &manager, bool *ok,
                                             QString *message, QString *action)
{
    QStringList names;
    QObject::connect(
        &manager, &PluginManager::operationFinished,
        [&](const QString &name, const QString &finishedAction, bool success,
            const QString &finishedMessage) {
            names.append(name);
            if (action)
                *action = finishedAction;
            if (ok)
                *ok = success;
            if (message)
                *message = finishedMessage;
        });
    return names;
}

void TestPluginManager::scriptSeparatorMatchesMpvPathList()
{
    // mpv's --scripts is an OPT_PATHLIST (':' Unix / ';' Windows), never ','.
    const QChar separator = PluginManager::scriptSeparator();
#if defined(Q_OS_WIN)
    QCOMPARE(separator, QChar(';'));
#else
    QCOMPARE(separator, QChar(':'));
#endif
}

void TestPluginManager::scriptEntryPathsCoverDestinationShapes()
{
    const QString root = QStringLiteral("/appdata/plugins");

    // Directory destination: the content root itself — mpv loads a directory
    // entry via its "main.lua" (player/scripting.c mp_load_script).
    QJsonObject dirEntry;
    dirEntry.insert(u"name"_s, u"uosc_danmaku"_s);
    dirEntry.insert(u"url"_s, u"https://git.example/u.git"_s);
    dirEntry.insert(u"destination"_s, u"scripts/uosc_danmaku"_s);
    const PluginManifest dirManifest = PluginManifest::parse(dirEntry);
    QCOMPARE(PluginManager::scriptEntryPaths(dirManifest, root),
             QStringList{root + u"/uosc_danmaku"_s});

    // Source subtree: the entry is <clone>/<source>.
    QJsonObject subtree = dirEntry;
    subtree.insert(u"name"_s, u"uosc"_s);
    subtree.insert(u"destination"_s, u"scripts/uosc"_s);
    subtree.insert(u"source"_s, u"src/uosc"_s);
    QCOMPARE(PluginManager::scriptEntryPaths(PluginManifest::parse(subtree), root),
             QStringList{root + u"/uosc/src/uosc"_s});

    // Single-file destination (manager.json also lists these): the file
    // inside the content root.
    QJsonObject fileEntry = dirEntry;
    fileEntry.insert(u"name"_s, u"mpv360"_s);
    fileEntry.insert(u"destination"_s, u"scripts/mpv360.lua"_s);
    QCOMPARE(PluginManager::scriptEntryPaths(PluginManifest::parse(fileEntry), root),
             QStringList{root + u"/mpv360/mpv360.lua"_s});
}

void TestPluginManager::scriptEntryPathsIgnoreShaderPacks()
{
    QJsonObject shaderEntry;
    shaderEntry.insert(u"name"_s, u"anime4k"_s);
    shaderEntry.insert(u"url"_s, u"https://git.example/a.git"_s);
    shaderEntry.insert(u"destination"_s, u"shaders/Anime4K/glsl"_s);
    QVERIFY(PluginManager::scriptEntryPaths(PluginManifest::parse(shaderEntry),
                                            QStringLiteral("/p"))
                .isEmpty());
}

void TestPluginManager::mergeScriptOptionsCreatesAppendsAndProtects()
{
    const QStringList entries{QStringLiteral("/plugins/uosc_danmaku")};

    // Absent key: created.
    QVariantMap options;
    PluginManager::mergeScriptOptions(options, entries);
    QCOMPARE(options.value(u"scripts"_s).toString(), entries.first());

    // Existing user value (assembled by the P4 compat layer): append only,
    // user paths keep their position.
    QVariantMap withUser{{u"scripts"_s, QStringLiteral("/user/a.lua:/user/b.lua")}};
    PluginManager::mergeScriptOptions(withUser, entries);
    QCOMPARE(withUser.value(u"scripts"_s).toString(),
             u"/user/a.lua:/user/b.lua:/plugins/uosc_danmaku"_s);

    // Nothing to append: the map is left completely untouched (no empty
    // "scripts" value is ever created).
    QVariantMap untouched{{u"other"_s, QStringLiteral("x")}};
    PluginManager::mergeScriptOptions(untouched, {});
    QCOMPARE(untouched.size(), 1);
    QVERIFY(!untouched.contains(u"scripts"_s));

    // A value we cannot interpret is never overwritten.
    QVariantMap opaque{{u"scripts"_s, 42}};
    PluginManager::mergeScriptOptions(opaque, entries);
    QCOMPARE(opaque.value(u"scripts"_s).toInt(), 42);
}

void TestPluginManager::installClonesAndRecordsHead()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);

    f.git.handler = [&](const QStringList &arguments,
                        const QString &workingDirectory) {
        // rev-parse runs inside the new clone after a successful clone.
        if (arguments.value(0) == QLatin1String("rev-parse")) {
            PluginGitResult result{0, QStringLiteral("head0123456789\n"), {}, true};
            return result;
        }
        return fakeClone(arguments);
    };

    bool ok = false;
    QString message;
    QString action;
    captureFinish(manager, &ok, &message, &action);
    manager.install(u"uosc_danmaku"_s);

    QVERIFY(ok);
    QCOMPARE(action, u"install"_s);
    QVERIFY(message.contains(u"head0123456789"_s));

    // Exactly one clone with depth 1, pinned branch, upstream URL.
    const QStringList *clone = nullptr;
    for (const QStringList &call : f.git.calls) {
        if (call.value(0) == QLatin1String("clone"))
            clone = &call;
    }
    QVERIFY(clone);
    QVERIFY(clone->contains(u"--depth"_s));
    QVERIFY(clone->contains(u"1"_s));
    QVERIFY(clone->contains(u"--branch"_s));
    QVERIFY(clone->contains(u"main"_s));
    QVERIFY(clone->contains(u"https://git.example/uosc_danmaku.git"_s));
    QCOMPARE(clone->last(), f.pluginsRoot + u"/uosc_danmaku"_s);

    const QVariantMap entry = pluginEntry(manager, u"uosc_danmaku"_s);
    QVERIFY(entry.value(u"installed"_s).toBool());
    QVERIFY(entry.value(u"enabled"_s).toBool());
    QCOMPARE(entry.value(u"head"_s).toString(), u"head0123456789"_s);
    // Installing a script plugin assembles an entry path under the plugin root.
    QCOMPARE(entry.value(u"entryPath"_s).toString(),
             f.pluginsRoot + u"/uosc_danmaku"_s);
}

void TestPluginManager::installRefusedWhenAlreadyInstalled()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
    f.git.handler = [](const QStringList &arguments, const QString &) {
        return fakeClone(arguments);
    };
    bool ok = true;
    captureFinish(manager, &ok, nullptr, nullptr);

    manager.install(u"uosc_danmaku"_s);
    QVERIFY(ok);
    const int callsAfterInstall = f.git.calls.size();

    manager.install(u"uosc_danmaku"_s);
    QVERIFY(!ok);
    QCOMPARE(f.git.calls.size(), callsAfterInstall); // no second clone
}

void TestPluginManager::statePersistsAcrossInstances()
{
    Fixture f;
    {
        PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
        f.git.handler = [&](const QStringList &arguments, const QString &) {
            if (arguments.value(0) == QLatin1String("rev-parse"))
                return PluginGitResult{0, QStringLiteral("abc123"), {}, true};
            return fakeClone(arguments);
        };
        bool ok = false;
        captureFinish(manager, &ok, nullptr, nullptr);
        manager.install(u"uosc_danmaku"_s);
        QVERIFY(ok);
        manager.setEnabled(u"uosc_danmaku"_s, false);
    }

    // A fresh instance reloads state.json from disk.
    PluginManager reloaded(f.dir.path(), f.catalogPath, &f.git);
    const QVariantMap entry = pluginEntry(reloaded, u"uosc_danmaku"_s);
    QVERIFY(entry.value(u"installed"_s).toBool()); // directory still there
    QCOMPARE(entry.value(u"enabled"_s).toBool(), false);
    QCOMPARE(entry.value(u"head"_s).toString(), u"abc123"_s);

    QVariantMap options;
    reloaded.appendScriptOptions(options);
    QVERIFY(!options.contains(u"scripts"_s)); // disabled: nothing appended
}

void TestPluginManager::appendScriptOptionsCoexistsWithUserScripts()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
    f.git.handler = [&](const QStringList &arguments, const QString &) {
        if (arguments.value(0) == QLatin1String("rev-parse"))
            return PluginGitResult{0, QStringLiteral("abc123"), {}, true};
        return fakeClone(arguments);
    };
    bool ok = false;
    captureFinish(manager, &ok, nullptr, nullptr);
    manager.install(u"uosc_danmaku"_s);
    QVERIFY(ok);

    // The compat layer already assembled the user's own scripts list.
    QVariantMap options{{u"scripts"_s, QStringLiteral("/user/config/a.lua")}};
    manager.appendScriptOptions(options);

    const QString merged = options.value(u"scripts"_s).toString();
    QVERIFY(merged.startsWith(u"/user/config/a.lua"_s));
    QVERIFY(merged.contains(PluginManager::scriptSeparator()));
    QVERIFY(merged.contains(f.pluginsRoot + u"/uosc_danmaku"_s));

    // A shader-pack plugin (anime4k) never feeds the scripts option.
    PluginManager::mergeScriptOptions(options, {});
    QCOMPARE(options.value(u"scripts"_s).toString(), merged);
}

void TestPluginManager::replaceUpdateSwapsAtomicallyWithBackup()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
    f.git.handler = [&](const QStringList &arguments,
                        const QString &workingDirectory) {
        if (arguments.value(0) == QLatin1String("rev-parse")) {
            const QString head = workingDirectory.contains(u".staging-"_s)
                                     ? QStringLiteral("newhead")
                                     : QStringLiteral("oldhead");
            return PluginGitResult{0, head + QStringLiteral("\n"), {}, true};
        }
        // status --porcelain: clean tree.
        if (arguments.value(0) == QLatin1String("status"))
            return PluginGitResult{0, {}, {}, true};
        return fakeClone(arguments);
    };
    bool ok = false;
    QString message;
    captureFinish(manager, &ok, &message, nullptr);
    manager.install(u"anime4k"_s);
    QVERIFY(ok);

    manager.update(u"anime4k"_s);
    QVERIFY(ok);
    QVERIFY(message.contains(u"oldhead"_s));
    QVERIFY(message.contains(u"newhead"_s));

    // The staging clone was swapped into place; the old install became the
    // backup (备份先行).
    QVERIFY(QFileInfo::exists(f.pluginsRoot + u"/anime4k/main.lua"_s));
    QVERIFY(QFileInfo::exists(f.pluginsRoot + u"/.backup-anime4k/main.lua"_s));
    QVERIFY(!QFileInfo::exists(f.pluginsRoot + u"/.staging-anime4k"_s));
    QCOMPARE(pluginEntry(manager, u"anime4k"_s).value(u"head"_s).toString(),
             u"newhead"_s);
}

void TestPluginManager::replaceUpdateRefusedOnLocalModifications()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
    f.git.handler = [&](const QStringList &arguments,
                        const QString &workingDirectory) {
        if (arguments.value(0) == QLatin1String("rev-parse"))
            return PluginGitResult{0, QStringLiteral("oldhead\n"), {}, true};
        if (arguments.value(0) == QLatin1String("status")) {
            // Local customization: an untracked file in the plugin dir.
            const QString marker =
                workingDirectory + u"/my-custom-shader.glsl"_s;
            if (QFileInfo::exists(marker))
                return PluginGitResult{0, QStringLiteral("?? my-custom-shader.glsl\n"),
                                       {}, true};
            return PluginGitResult{0, {}, {}, true};
        }
        return fakeClone(arguments);
    };
    bool ok = false;
    QString message;
    captureFinish(manager, &ok, &message, nullptr);
    manager.install(u"anime4k"_s);
    QVERIFY(ok);
    QVERIFY(QFile::copy(f.catalogPath,
                        f.pluginsRoot + u"/anime4k/my-custom-shader.glsl"_s));

    const int callsBefore = f.git.calls.size();
    manager.update(u"anime4k"_s);
    QVERIFY(!ok);
    // Protective skip: local customizations win (本地定制永不覆盖).
    QVERIFY(message.contains(u"local modifications"_s));
    QVERIFY(QFileInfo::exists(f.pluginsRoot + u"/anime4k/my-custom-shader.glsl"_s));
    QVERIFY(!QFileInfo::exists(f.pluginsRoot + u"/.backup-anime4k"_s));
    QVERIFY(!QFileInfo::exists(f.pluginsRoot + u"/.staging-anime4k"_s));
    // No clone was attempted after the guard.
    QCOMPARE(f.git.calls.size(), callsBefore + 1); // only the status call
}

void TestPluginManager::checkModeUpdateComparesBaselineWithoutTouchingFiles()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
    f.git.handler = [&](const QStringList &arguments,
                        const QString &workingDirectory) {
        if (arguments.value(0) == QLatin1String("rev-parse"))
            return PluginGitResult{0, QStringLiteral("installedhead\n"), {}, true};
        if (arguments.value(0) == QLatin1String("ls-remote"))
            return PluginGitResult{0, QStringLiteral("remotehead\trefs/heads/main\n"),
                                   {}, true};
        if (arguments.value(0) == QLatin1String("status"))
            return PluginGitResult{0, {}, {}, true};
        return fakeClone(arguments);
    };
    bool ok = false;
    QString message;
    captureFinish(manager, &ok, &message, nullptr);
    manager.install(u"uosc_danmaku"_s);
    QVERIFY(ok);

    manager.checkForUpdate(u"uosc_danmaku"_s);
    QVERIFY(ok);
    // The reference is the manifest baseline ("old1"), not the install HEAD.
    QVERIFY(message.contains(u"update available"_s));
    QVERIFY(message.contains(u"old1"_s));
    QVERIFY(message.contains(u"remotehead"_s));
    QCOMPARE(pluginEntry(manager, u"uosc_danmaku"_s)
                 .value(u"updateAvailable"_s)
                 .toBool(),
             true);

    // check mode: the "update" action is the comparison itself — no clone,
    // no file changes.
    const int callsBefore = f.git.calls.size();
    manager.update(u"uosc_danmaku"_s);
    QVERIFY(ok);
    QCOMPARE(f.git.calls.size(), callsBefore + 2); // ls-remote + status only
    int clonesSince = 0;
    for (int i = callsBefore; i < f.git.calls.size(); ++i)
        if (f.git.calls.at(i).value(0) == QLatin1String("clone"))
            ++clonesSince;
    QCOMPARE(clonesSince, 0);
    QVERIFY(QFileInfo::exists(f.pluginsRoot + u"/uosc_danmaku"_s));
}

void TestPluginManager::uninstallRemovesDirectoryAndState()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
    f.git.handler = [&](const QStringList &arguments, const QString &) {
        if (arguments.value(0) == QLatin1String("rev-parse"))
            return PluginGitResult{0, QStringLiteral("abc123"), {}, true};
        return fakeClone(arguments);
    };
    bool ok = false;
    captureFinish(manager, &ok, nullptr, nullptr);
    manager.install(u"uosc_danmaku"_s);
    QVERIFY(ok);
    QVERIFY(QFileInfo::exists(f.pluginsRoot + u"/uosc_danmaku"_s));

    manager.uninstall(u"uosc_danmaku"_s);
    QVERIFY(ok);
    QVERIFY(!QFileInfo::exists(f.pluginsRoot + u"/uosc_danmaku"_s));

    PluginManager reloaded(f.dir.path(), f.catalogPath, &f.git);
    QVERIFY(!pluginEntry(reloaded, u"uosc_danmaku"_s)
                 .value(u"installed"_s)
                 .toBool());

    // Uninstalling a not-installed plugin is refused without side effects.
    ok = true;
    manager.uninstall(u"uosc_danmaku"_s);
    QVERIFY(!ok);
}

void TestPluginManager::planOperationListsChanges()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);

    const QVariantMap install =
        manager.planOperation(u"uosc_danmaku"_s, u"install"_s);
    QVERIFY(!install.contains(u"error"_s));
    const QVariantList details = install.value(u"details"_s).toList();
    QVERIFY(!details.isEmpty());
    bool mentionsClone = false;
    for (const QVariant &line : details)
        if (line.toString().contains(u"Clone"_s))
            mentionsClone = true;
    QVERIFY(mentionsClone);

    // check-mode update plans state the comparison-only semantics.
    const QVariantMap update = manager.planOperation(u"uosc_danmaku"_s, u"update"_s);
    bool mentionsComparisonOnly = false;
    for (const QVariant &line : update.value(u"details"_s).toList())
        if (line.toString().contains(u"No local file is touched"_s))
            mentionsComparisonOnly = true;
    QVERIFY(mentionsComparisonOnly);

    QVERIFY(manager.planOperation(u"uosc_danmaku"_s, u"nuke"_s)
                .contains(u"error"_s));
    QVERIFY(manager.planOperation(u"nope"_s, u"install"_s).contains(u"error"_s));
}

void TestPluginManager::checkAllRunsForEveryEntry()
{
    Fixture f;
    PluginManager manager(f.dir.path(), f.catalogPath, &f.git);
    f.git.handler = [](const QStringList &arguments, const QString &) {
        if (arguments.value(0) == QLatin1String("rev-parse"))
            return PluginGitResult{0, QStringLiteral("installedhead\n"), {}, true};
        if (arguments.value(0) == QLatin1String("ls-remote"))
            return PluginGitResult{0, QStringLiteral("remotehead\trefs/heads/main\n"),
                                   {}, true};
        return PluginGitResult{0, {}, {}, true};
    };
    bool ok = false;
    int finishes = 0;
    QStringList checked;
    QObject::connect(&manager, &PluginManager::operationFinished,
                     [&](const QString &name, const QString &action,
                         bool success, const QString &) {
                         ++finishes;
                         ok = ok || success;
                         if (action == QLatin1String("check"))
                             checked.append(name);
                     });

    manager.checkAll();
    // One check per catalog entry (all three), each reporting a result.
    QCOMPARE(finishes, 3);
    QVERIFY(ok);
    QCOMPARE(checked.size(), 3);
    for (const QVariant &value : manager.plugins())
        QVERIFY(value.toMap().value(u"remoteHead"_s).toString().isEmpty()
                == false);
    QVERIFY(!manager.busy());
}

QTEST_GUILESS_MAIN(TestPluginManager)
#include "test_plugin_manager.moc"
