#pragma once

#include "PamAuth.h"
#include <QObject>
#include <QString>
#include <QTimer>
#include <functional>

namespace quattro {

// Implements FingerprintModel.js backoff + fprintd-list probe + PamAuth("quattro-lock-fingerprint")
// plus video poster thumbnail generation (mirroring lock/poster.sh).
class FprintdService : public QObject {
    Q_OBJECT
public:
    static constexpr int kMatchRetryMs = 250;
    static constexpr int kFastErrorMs = 2000;
    static constexpr int kNudgeCooldownMs = 2000;
    static constexpr int kIdleClearMs = 32000;
    static constexpr int kErrorRetryBaseMs = 1000;
    static constexpr int kErrorRetryCapMs = 40000;
    static constexpr int kUnavailableAfter = 3;
    static constexpr int kSleepGapMs = 2000;
    static constexpr int kResumeGraceMs = 5000;

    explicit FprintdService(QObject *parent = nullptr);

    bool available() const { return m_enrolled && !m_unavailable; }
    bool enrolled() const { return m_enrolled; }
    bool unavailable() const { return m_unavailable; }
    QString statusText() const { return m_statusText; }

    void startLockSession();
    void stopLockSession();
    void nudge();

    // Pure helpers from FingerprintModel.js (tested in unit tests)
    static QString classifyProbeOutput(const QString &text);
    static int errorBackoffMs(int streak);
    static bool spannedSleep(qint64 elapsedMs, qint64 expectedMs);
    static int nextStreak(int streak, bool usable, bool inGrace);
    static bool isUnavailableStreak(int streak) { return streak >= kUnavailableAfter; }

    // Synchronous or async video frame extraction for lock poster (ffmpegthumbnailer)
    static QString cachedPosterPathFor(const QString &sourcePath);
    static void generateLockPosterAsync(const QString &sourcePath, QObject *receiver,
                                        std::function<void(const QString &posterPath)> cb);

signals:
    void fingerprintUnlocked();
    void fingerprintError(const QString &message);
    void stateChanged();

private:
    void probeEnrolled();
    void startAuthAttempt();
    void scheduleRetry(int ms);

    PamAuth m_pam;
    bool m_active = false;
    bool m_enrolled = false;
    bool m_unavailable = false;
    int m_streak = 0;
    qint64 m_lastNudgeMs = 0;
    qint64 m_resumeUntilMs = 0;
    qint64 m_attemptStartMs = 0;
    QString m_statusText;
    QTimer m_retryTimer;
};

} // namespace quattro
