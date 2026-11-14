#include "FprintdService.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace quattro {

FprintdService::FprintdService(QObject *parent)
    : QObject(parent)
{
    m_retryTimer.setSingleShot(true);
    connect(&m_retryTimer, &QTimer::timeout, this, &FprintdService::startAuthAttempt);

    connect(&m_pam, &PamAuth::authenticationFinished, this, [this](bool ok, const QString &msg) {
        if (!m_active)
            return;
        if (ok) {
            m_streak = 0;
            m_unavailable = false;
            emit stateChanged();
            emit fingerprintUnlocked();
            return;
        }

        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        const qint64 elapsed = now - m_attemptStartMs;
        const bool fastErr = elapsed < kFastErrorMs;
        if (spannedSleep(elapsed, kIdleClearMs))
            m_resumeUntilMs = now + kResumeGraceMs;

        const bool inGrace = now < m_resumeUntilMs;
        m_streak = nextStreak(m_streak, !fastErr, inGrace);
        m_unavailable = isUnavailableStreak(m_streak);
        m_statusText = msg;
        emit stateChanged();
        if (!msg.isEmpty())
            emit fingerprintError(msg);

        if (!m_unavailable)
            scheduleRetry(fastErr ? errorBackoffMs(m_streak) : kMatchRetryMs);
    });
}

QString FprintdService::classifyProbeOutput(const QString &text) {
    static const QRegularExpression fingerLine(QStringLiteral("^[ \\t]*-[ \\t]*#[0-9]+:"), QRegularExpression::MultilineOption);
    if (fingerLine.match(text).hasMatch())
        return QStringLiteral("yes");
    if (text.contains(QLatin1String("has no fingers enrolled"), Qt::CaseInsensitive)
        || text.trimmed().compare(QLatin1String("no"), Qt::CaseInsensitive) == 0)
        return QStringLiteral("no");
    return QStringLiteral("unknown");
}

int FprintdService::errorBackoffMs(int streak) {
    const int s = std::max(1, streak);
    const qint64 ms = static_cast<qint64>(kErrorRetryBaseMs) << std::min(s - 1, 10);
    return static_cast<int>(std::min<qint64>(ms, kErrorRetryCapMs));
}

bool FprintdService::spannedSleep(qint64 elapsedMs, qint64 expectedMs) {
    return elapsedMs > expectedMs + kSleepGapMs;
}

int FprintdService::nextStreak(int streak, bool usable, bool inGrace) {
    if (usable) return 0;
    if (inGrace) return 1;
    return streak + 1;
}

void FprintdService::startLockSession() {
    m_active = true;
    m_streak = 0;
    m_unavailable = false;
    m_statusText.clear();
    probeEnrolled();
}

void FprintdService::stopLockSession() {
    m_active = false;
    m_retryTimer.stop();
}

void FprintdService::nudge() {
    if (!m_active || !m_enrolled)
        return;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - m_lastNudgeMs < kNudgeCooldownMs)
        return;
    m_lastNudgeMs = now;
    if (m_unavailable || m_retryTimer.isActive()) {
        m_unavailable = false;
        m_streak = 0;
        scheduleRetry(100);
    }
}

void FprintdService::probeEnrolled() {
    if (!QFileInfo::exists(QStringLiteral("/etc/pam.d/quattro-lock-fingerprint"))) {
        m_enrolled = false;
        emit stateChanged();
        return;
    }

    auto *proc = new QProcess(this);
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert(QStringLiteral("LC_ALL"), QStringLiteral("C"));
    proc->setProcessEnvironment(env);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int, QProcess::ExitStatus) {
        const QString out = QString::fromUtf8(proc->readAllStandardOutput() + proc->readAllStandardError());
        proc->deleteLater();
        m_enrolled = (classifyProbeOutput(out) == QLatin1String("yes"));
        emit stateChanged();
        if (m_active && m_enrolled)
            startAuthAttempt();
    });
    proc->start(QStringLiteral("fprintd-list"), {PamAuth::currentUsername()});
}

void FprintdService::startAuthAttempt() {
    if (!m_active || !m_enrolled || m_pam.isAuthenticating())
        return;
    m_attemptStartMs = QDateTime::currentMSecsSinceEpoch();
    m_pam.authenticateWithServiceAsync(QStringLiteral("quattro-lock-fingerprint"), QString());
}

void FprintdService::scheduleRetry(int ms) {
    if (!m_active)
        return;
    m_retryTimer.start(std::max(50, ms));
}

QString FprintdService::cachedPosterPathFor(const QString &sourcePath) {
    const QFileInfo fi(sourcePath);
    if (!fi.exists())
        return {};
    const QString cacheDir = qEnvironmentVariable("XDG_CACHE_HOME", QDir::homePath() + QStringLiteral("/.cache"))
                             + QStringLiteral("/quattro/lock-poster");
    const QString keyInput = sourcePath + QLatin1Char('\n')
                             + QString::number(fi.size()) + QLatin1Char(':')
                             + QString::number(fi.lastModified().toMSecsSinceEpoch());
    const QString key = QString::fromLatin1(QCryptographicHash::hash(keyInput.toUtf8(), QCryptographicHash::Sha256).toHex());
    return cacheDir + QStringLiteral("/poster-") + key + QStringLiteral(".jpg");
}

void FprintdService::generateLockPosterAsync(const QString &sourcePath, QObject *receiver,
                                             std::function<void(const QString &posterPath)> cb) {
    const QString poster = cachedPosterPathFor(sourcePath);
    if (poster.isEmpty()) {
        if (cb) cb({});
        return;
    }
    if (QFileInfo::exists(poster)) {
        if (cb) cb(poster);
        return;
    }
    const QFileInfo pi(poster);
    QDir().mkpath(pi.absolutePath());
    const QString tmp = poster + QStringLiteral(".tmp");
    auto *proc = new QProcess(receiver);
    QObject::connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
                     receiver, [proc, tmp, poster, cb](int code, QProcess::ExitStatus) {
        proc->deleteLater();
        if (code == 0 && QFileInfo::exists(tmp)) {
            QFile::remove(poster);
            QFile::rename(tmp, poster);
            if (cb) cb(poster);
        } else {
            QFile::remove(tmp);
            if (cb) cb({});
        }
    });
    proc->start(QStringLiteral("ffmpegthumbnailer"),
                {QStringLiteral("-i"), sourcePath, QStringLiteral("-o"), tmp,
                 QStringLiteral("-s"), QStringLiteral("1920"), QStringLiteral("-q"), QStringLiteral("8")});
}

} // namespace quattro
