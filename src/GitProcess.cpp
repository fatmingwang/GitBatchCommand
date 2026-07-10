#include "GitProcess.h"

#include <QProcess>
#include <QProcessEnvironment>
#include <QDeadlineTimer>

GitCommandResult GitProcess::run(const QString &workingDir,
                                  const QStringList &args,
                                  const std::function<void(const QString &)> &onLine,
                                  int timeoutMs)
{
    GitCommandResult result;

    QProcess proc;
    proc.setWorkingDirectory(workingDir);
    proc.setProcessChannelMode(QProcess::MergedChannels);

    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("GIT_TERMINAL_PROMPT", "0");
    proc.setProcessEnvironment(env);

    proc.start(QStringLiteral("git"), args);
    if (!proc.waitForStarted(10000)) {
        result.success = false;
        result.exitCode = -1;
        result.output = QStringLiteral("Failed to start 'git'. Make sure git is installed and on PATH.");
        return result;
    }

    QDeadlineTimer deadline(timeoutMs);
    QString pending;
    QString fullOutput;

    while (proc.state() != QProcess::NotRunning) {
        if (deadline.hasExpired()) {
            proc.kill();
            proc.waitForFinished(3000);
            const QString msg = QStringLiteral("[TIMEOUT] git command exceeded time limit and was terminated.");
            fullOutput += "\n" + msg + "\n";
            if (onLine)
                onLine(msg);
            result.success = false;
            result.exitCode = -2;
            result.output = fullOutput;
            return result;
        }
        proc.waitForReadyRead(200);
        const QByteArray chunk = proc.readAll();
        if (!chunk.isEmpty()) {
            pending += QString::fromLocal8Bit(chunk);
            int idx;
            while ((idx = pending.indexOf('\n')) != -1) {
                QString line = pending.left(idx);
                pending.remove(0, idx + 1);
                if (line.endsWith('\r'))
                    line.chop(1);
                fullOutput += line + "\n";
                if (onLine)
                    onLine(line);
            }
        }
    }

    const QByteArray rest = proc.readAll();
    if (!rest.isEmpty())
        pending += QString::fromLocal8Bit(rest);
    if (!pending.isEmpty()) {
        fullOutput += pending;
        if (onLine)
            onLine(pending);
    }

    proc.waitForFinished(3000);
    result.exitCode = proc.exitCode();
    result.success = (proc.exitStatus() == QProcess::NormalExit && result.exitCode == 0);
    result.output = fullOutput;
    return result;
}
