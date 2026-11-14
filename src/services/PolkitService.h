#pragma once

#include <QObject>
#include <QString>
#include <QTimer>

namespace quattro {

struct PolkitRequestInfo {
    bool active = false;
    QString actionId;
    QString message;
    QString iconName;
    QString cookie;
    QString identityName;
    QString promptLabel;
    bool echoOn = false;
    bool fingerprintAvailable = false;
};

class PolkitService : public QObject {
    Q_OBJECT
public:
    explicit PolkitService(QObject *parent = nullptr);
    ~PolkitService() override;

    bool start();
    void stop();
    bool isRegistered() const { return m_registered; }

    const PolkitRequestInfo &currentRequest() const { return m_current; }
    bool isWaitingForInput() const { return m_waitingForInput; }
    bool isAuthenticating() const { return m_authenticating; }

    void submitResponse(const QString &response);
    void cancel();

    static bool isPolkitFingerprintConfigured();

    // Exposed internally for GLib GObject callbacks
    void handleInitiate(const QString &actionId,
                        const QString &message,
                        const QString &iconName,
                        const QString &cookie,
                        void *identityPtr,
                        void *taskPtr,
                        void *cancellablePtr);
    void handleSessionRequest(const QString &request, bool echoOn);
    void handleSessionShowError(const QString &text);
    void handleSessionShowInfo(const QString &text);
    void handleSessionCompleted(bool gainedAuthorization);

signals:
    void requestOpened(const PolkitRequestInfo &info);
    void promptUpdated(const QString &prompt, bool echoOn);
    void statusMessage(const QString &message, bool isError);
    void authenticationFailed(const QString &message);
    void requestClosed(bool gainedAuthorization);

private:
    void beginSessionAttempt();
    void cleanupActiveAttempt();
    void checkLidClosedAsync();

    void *m_listener = nullptr;
    void *m_regHandle = nullptr;
    void *m_activeSession = nullptr;
    void *m_activeIdentity = nullptr;
    void *m_activeTask = nullptr;
    void *m_cancellable = nullptr;
    unsigned long m_cancelHandlerId = 0;

    bool m_registered = false;
    bool m_waitingForInput = false;
    bool m_authenticating = false;
    bool m_completedHandled = false;
    QString m_pendingResponse;
    QString m_lastError;
    PolkitRequestInfo m_current;
    QTimer m_glibPumpTimer;
};

} // namespace quattro
