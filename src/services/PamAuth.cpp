#include "PamAuth.h"

#include <QFileInfo>
#include <QMetaObject>
#include <QThread>
#include <pwd.h>
#include <security/pam_appl.h>
#include <cstdlib>
#include <cstring>
#include <unistd.h>

namespace quattro {

namespace {

struct PamPayload {
    QByteArray password;
    QString errorMessage;
};

int pamConv(int num_msg, const struct pam_message **msg,
            struct pam_response **resp, void *appdata_ptr) {
    if (num_msg <= 0 || !appdata_ptr)
        return PAM_CONV_ERR;
    auto *payload = static_cast<PamPayload *>(appdata_ptr);
    auto *reply = static_cast<struct pam_response *>(calloc(static_cast<size_t>(num_msg), sizeof(struct pam_response)));
    if (!reply)
        return PAM_BUF_ERR;

    for (int i = 0; i < num_msg; ++i) {
        switch (msg[i]->msg_style) {
        case PAM_PROMPT_ECHO_OFF:
        case PAM_PROMPT_ECHO_ON:
            reply[i].resp = strdup(payload->password.constData());
            reply[i].resp_retcode = 0;
            break;
        case PAM_ERROR_MSG:
            payload->errorMessage = QString::fromLocal8Bit(msg[i]->msg);
            break;
        default:
            break;
        }
    }
    *resp = reply;
    return PAM_SUCCESS;
}

} // namespace

PamAuth::PamAuth(QObject *parent)
    : QObject(parent)
{
}

bool PamAuth::isPamConfigured() {
    return QFileInfo::exists(QStringLiteral("/etc/pam.d/quattro-lock-password"));
}

QString PamAuth::currentUsername() {
    if (const struct passwd *pw = getpwuid(getuid()))
        return QString::fromLocal8Bit(pw->pw_name);
    return qEnvironmentVariable("USER");
}

void PamAuth::authenticateAsync(const QString &password) {
    const QString service = isPamConfigured() ? QStringLiteral("quattro-lock-password")
                                              : QStringLiteral("system-auth");
    authenticateWithServiceAsync(service, password);
}

void PamAuth::authenticateWithServiceAsync(const QString &serviceName, const QString &password) {
    if (m_authenticating)
        return;
    m_authenticating = true;

    const QByteArray user = currentUsername().toLocal8Bit();
    const QByteArray pass = password.toUtf8();
    const QByteArray service = serviceName.toLocal8Bit();

    QThread *thread = QThread::create([this, user, pass, service]() mutable {
        PamPayload payload{pass, {}};
        struct pam_conv conv{pamConv, &payload};
        pam_handle_t *pamh = nullptr;

        int rc = pam_start(service.constData(), user.constData(), &conv, &pamh);
        if (rc == PAM_SUCCESS)
            rc = pam_authenticate(pamh, PAM_SILENT);
        if (rc == PAM_SUCCESS)
            rc = pam_acct_mgmt(pamh, PAM_SILENT);

        const bool ok = (rc == PAM_SUCCESS);
        QString errMsg = payload.errorMessage;
        if (!ok && errMsg.isEmpty())
            errMsg = pamh ? QString::fromLocal8Bit(pam_strerror(pamh, rc)) : QStringLiteral("Authentication failed");

        if (pamh)
            pam_end(pamh, rc);

        // Zero out sensitive buffer
        if (!payload.password.isEmpty())
            explicit_bzero(payload.password.data(), static_cast<size_t>(payload.password.size()));

        QMetaObject::invokeMethod(this, [this, ok, errMsg]() {
            m_authenticating = false;
            emit authenticationFinished(ok, errMsg);
        }, Qt::QueuedConnection);
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

} // namespace quattro
