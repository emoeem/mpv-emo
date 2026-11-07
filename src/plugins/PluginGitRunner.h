#pragma once

#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>

// Result of one git command invocation.
struct PluginGitResult
{
    int exitCode = -1;
    QString standardOutput;
    QString standardError;
    // false when the git program could not be started at all.
    bool started = false;
};

Q_DECLARE_METATYPE(PluginGitResult)

// Seam between PluginManager and the git binary: unit tests inject a fake
// that answers synchronously, production runs QProcess asynchronously so a
// slow network clone never blocks the GUI thread.
//
// One instance serializes whole PluginManager operations; PluginManager
// guarantees at most one run() call is in flight at any time.
class PluginGitRunner : public QObject
{
    Q_OBJECT

public:
    using QObject::QObject;
    ~PluginGitRunner() override = default;

    // Runs "git <arguments>" with workingDirectory as the cwd. Exactly one
    // finished() signal must be emitted per call (asynchronously in the
    // QProcess implementation; synchronously is fine for fakes).
    virtual void run(const QStringList &arguments,
                     const QString &workingDirectory) = 0;

signals:
    void finished(const QStringList &arguments, const QString &workingDirectory,
                  const PluginGitResult &result);
};

// Production implementation: one QProcess per call, parented to the runner so
// pending git children are killed when the runner (and with it the app) goes
// away — mirroring manager.lua's "abort subprocesses on mpv exit" behavior.
class QProcessPluginGitRunner : public PluginGitRunner
{
    Q_OBJECT

public:
    using PluginGitRunner::PluginGitRunner;

    void run(const QStringList &arguments,
             const QString &workingDirectory) override;
};
