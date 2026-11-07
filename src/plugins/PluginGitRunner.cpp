#include "plugins/PluginGitRunner.h"

#include <QProcess>

void QProcessPluginGitRunner::run(const QStringList &arguments,
                                  const QString &workingDirectory)
{
    auto *process = new QProcess(this);
    // Queueing the result through the runner's own signals keeps the
    // PluginManager state machine on the GUI thread without blocking it.
    connect(process, &QProcess::finished, this,
            [this, process](int exitCode, QProcess::ExitStatus) {
                PluginGitResult result;
                result.exitCode = exitCode;
                result.standardOutput =
                    QString::fromUtf8(process->readAllStandardOutput());
                result.standardError =
                    QString::fromUtf8(process->readAllStandardError());
                result.started = true;
                emit finished(process->arguments(), process->workingDirectory(),
                              result);
                process->deleteLater();
            });
    connect(process, &QProcess::errorOccurred, this,
            [this, process](QProcess::ProcessError error) {
                if (error != QProcess::FailedToStart)
                    return; // finished() will report the real exit state
                PluginGitResult result;
                result.started = false;
                result.standardError = process->errorString();
                emit finished(process->arguments(), process->workingDirectory(),
                              result);
                process->deleteLater();
            });
    process->setWorkingDirectory(workingDirectory);
    process->start(QStringLiteral("git"), arguments);
}
