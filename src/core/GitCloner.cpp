#include "GitCloner.h"
#include <QRegularExpression>
#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>

GitCloner::GitCloner(QObject* parent) : QObject(parent) {}

void GitCloner::start(const QString& repoUrl, const QString& destDir, const QString& ref) {
    m_cancelled = false;
    m_phasePct.clear();
    m_stderrBuf.clear();

    // destDir deve essere vuota/non esistente: git clone crea la cartella.
    QDir().mkpath(QFileInfo(destDir).absolutePath());
    if (QDir(destDir).exists()) {
        // cartella residua di un tentativo precedente: ripartiamo puliti
        QDir(destDir).removeRecursively();
    }

    QString gitExe = QStandardPaths::findExecutable("git");
    if (gitExe.isEmpty()) {
        emit finished(false, "git non trovato nel PATH. Installa Git for Windows e riprova.");
        return;
    }

    QStringList args;
    args << "clone" << "--progress";
    if (!ref.isEmpty()) args << "--branch" << ref;
    args << "--" << repoUrl << destDir;

    m_process = new QProcess(this);
    // git scrive il progresso su stderr: lo leggiamo separatamente dallo stdout
    m_process->setProcessChannelMode(QProcess::SeparateChannels);

    connect(m_process, &QProcess::readyReadStandardError, this, &GitCloner::onReadyReadStderr);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &GitCloner::onProcessFinished);
    connect(m_process, &QProcess::errorOccurred, this, &GitCloner::onProcessErrorOccurred);

    m_process->start(gitExe, args);
}

void GitCloner::cancel() {
    m_cancelled = true;
    if (m_process && m_process->state() != QProcess::NotRunning) {
        m_process->kill();
    }
}

void GitCloner::onReadyReadStderr() {
    m_stderrBuf += m_process->readAllStandardError();

    // git aggiorna la stessa riga con \r finché la fase non è completa,
    // poi stampa \n. Trattiamo sia \r che \n come separatori di "riga".
    int pos;
    while ((pos = m_stderrBuf.indexOf('\r')) != -1 || (pos = m_stderrBuf.indexOf('\n')) != -1) {
        QByteArray chunk = m_stderrBuf.left(pos);
        m_stderrBuf.remove(0, pos + 1);
        QString line = QString::fromUtf8(chunk).trimmed();
        if (!line.isEmpty()) handleLine(line);
    }
}

void GitCloner::handleLine(const QString& line) {
    static const QRegularExpression reEnumerating(R"(Enumerating objects:\s*(\d+))");
    static const QRegularExpression reCounting(R"(Counting objects:\s*(\d+)%)");
    static const QRegularExpression reCompressing(R"(Compressing objects:\s*(\d+)%)");
    static const QRegularExpression reReceiving(R"(Receiving objects:\s*(\d+)%)");
    static const QRegularExpression reResolving(R"(Resolving deltas:\s*(\d+)%)");
    static const QRegularExpression reCheckout(R"(Updating files:\s*(\d+)%)");

    QRegularExpressionMatch m;
    if ((m = reReceiving.match(line)).hasMatch()) {
        int pct = m.captured(1).toInt();
        m_phasePct["receiving"] = pct;
        emit phaseProgress("receiving", pct, line);
    } else if ((m = reCheckout.match(line)).hasMatch()) {
        int pct = m.captured(1).toInt();
        m_phasePct["checkout"] = pct;
        emit phaseProgress("checkout", pct, line);
    } else if ((m = reResolving.match(line)).hasMatch()) {
        int pct = m.captured(1).toInt();
        m_phasePct["resolving"] = pct;
        emit phaseProgress("resolving", pct, line);
    } else if ((m = reCompressing.match(line)).hasMatch()) {
        int pct = m.captured(1).toInt();
        m_phasePct["compressing"] = pct;
        emit phaseProgress("compressing", pct, line);
    } else if ((m = reCounting.match(line)).hasMatch()) {
        int pct = m.captured(1).toInt();
        m_phasePct["counting"] = pct;
        emit phaseProgress("counting", pct, line);
    } else if ((m = reEnumerating.match(line)).hasMatch()) {
        emit phaseProgress("enumerating", -1, line);
    } else if (line.contains("fatal:", Qt::CaseInsensitive) || line.contains("error:", Qt::CaseInsensitive)) {
        m_lastError = line;
    }

    recomputeOverall();
}

void GitCloner::recomputeOverall() {
    double overall =
        W_COUNTING    * m_phasePct.value("counting", 0) +
        W_COMPRESSING * m_phasePct.value("compressing", 0) +
        W_RECEIVING   * m_phasePct.value("receiving", 0) +
        W_RESOLVING   * m_phasePct.value("resolving", 0) +
        W_CHECKOUT    * m_phasePct.value("checkout", 0);
    emit overallProgress(qBound(0, static_cast<int>(overall), 100));
}

void GitCloner::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    if (m_cancelled) {
        emit finished(false, "Installazione annullata dall'utente.");
        return;
    }
    if (status == QProcess::NormalExit && exitCode == 0) {
        emit overallProgress(100);
        emit finished(true, QString());
    } else {
        QString err = m_lastError.isEmpty()
            ? QString("git clone terminato con codice %1").arg(exitCode)
            : m_lastError;
        emit finished(false, err);
    }
}

void GitCloner::onProcessErrorOccurred(QProcess::ProcessError) {
    if (m_cancelled) return;
    emit finished(false, m_process ? m_process->errorString() : "Errore sconosciuto nel processo git.");
}
