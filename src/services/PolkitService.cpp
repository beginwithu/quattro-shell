#include "PolkitService.h"

#include <QFile>
#include <QProcess>
#include <QTextStream>
#include <unistd.h>

#pragma push_macro("signals")
#undef signals
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkitagent/polkitagent.h>
#pragma pop_macro("signals")

namespace {

struct _QuattroPolkitListener {
    PolkitAgentListener parent_instance;
    quattro::PolkitService *service;
};

struct _QuattroPolkitListenerClass {
    PolkitAgentListenerClass parent_class;
};

typedef struct _QuattroPolkitListener QuattroPolkitListener;
typedef struct _QuattroPolkitListenerClass QuattroPolkitListenerClass;

G_DEFINE_TYPE(QuattroPolkitListener, quattro_polkit_listener, POLKIT_AGENT_TYPE_LISTENER)

static void quattro_polkit_listener_initiate_authentication(
    PolkitAgentListener *listener,
    const gchar *action_id,
    const gchar *message,
    const gchar *icon_name,
    PolkitDetails * /*details*/,
    const gchar *cookie,
    GList *identities,
    GCancellable *cancellable,
    GAsyncReadyCallback callback,
    gpointer user_data)
{
    auto *self = reinterpret_cast<QuattroPolkitListener *>(listener);
    GTask *task = g_task_new(listener, cancellable, callback, user_data);

    PolkitIdentity *chosen = nullptr;
    const uid_t myUid = getuid();
    for (GList *l = identities; l != nullptr; l = l->next) {
        auto *id = POLKIT_IDENTITY(l->data);
        if (POLKIT_IS_UNIX_USER(id) && polkit_unix_user_get_uid(POLKIT_UNIX_USER(id)) == static_cast<gint>(myUid)) {
            chosen = id;
            break;
        }
    }
    if (!chosen && identities)
        chosen = POLKIT_IDENTITY(identities->data);

    if (!self->service || !chosen) {
        g_task_return_new_error(task, POLKIT_ERROR, POLKIT_ERROR_FAILED,
                                "No usable Polkit identity available");
        g_object_unref(task);
        return;
    }

    g_object_ref(chosen);

    const QString act = QString::fromUtf8(action_id ? action_id : "");
    const QString msg = QString::fromUtf8(message ? message : "");
    const QString ico = QString::fromUtf8(icon_name ? icon_name : "");
    const QString cke = QString::fromUtf8(cookie ? cookie : "");

    QMetaObject::invokeMethod(self->service, [svc = self->service, act, msg, ico, cke, chosen, task, cancellable]() {
        svc->handleInitiate(act, msg, ico, cke, chosen, task, cancellable);
    }, Qt::QueuedConnection);
}

static gboolean quattro_polkit_listener_initiate_authentication_finish(
    PolkitAgentListener * /*listener*/,
    GAsyncResult *res,
    GError **error)
{
    return g_task_propagate_boolean(G_TASK(res), error);
}

static void quattro_polkit_listener_class_init(QuattroPolkitListenerClass *klass) {
    PolkitAgentListenerClass *listener_class = POLKIT_AGENT_LISTENER_CLASS(klass);
    listener_class->initiate_authentication = quattro_polkit_listener_initiate_authentication;
    listener_class->initiate_authentication_finish = quattro_polkit_listener_initiate_authentication_finish;
}

static void quattro_polkit_listener_init(QuattroPolkitListener *self) {
    self->service = nullptr;
}

static void on_session_request(PolkitAgentSession * /*session*/, const gchar *request, gboolean echo_on, gpointer user_data) {
    auto *svc = static_cast<quattro::PolkitService *>(user_data);
    const QString req = QString::fromUtf8(request ? request : "");
    const bool echo = (echo_on != FALSE);
    QMetaObject::invokeMethod(svc, [svc, req, echo]() {
        svc->handleSessionRequest(req, echo);
    }, Qt::QueuedConnection);
}

static void on_session_show_error(PolkitAgentSession * /*session*/, const gchar *text, gpointer user_data) {
    auto *svc = static_cast<quattro::PolkitService *>(user_data);
    const QString msg = QString::fromUtf8(text ? text : "");
    QMetaObject::invokeMethod(svc, [svc, msg]() {
        svc->handleSessionShowError(msg);
    }, Qt::QueuedConnection);
}

static void on_session_show_info(PolkitAgentSession * /*session*/, const gchar *text, gpointer user_data) {
    auto *svc = static_cast<quattro::PolkitService *>(user_data);
    const QString msg = QString::fromUtf8(text ? text : "");
    QMetaObject::invokeMethod(svc, [svc, msg]() {
        svc->handleSessionShowInfo(msg);
    }, Qt::QueuedConnection);
}

static void on_session_completed(PolkitAgentSession * /*session*/, gboolean gained, gpointer user_data) {
    auto *svc = static_cast<quattro::PolkitService *>(user_data);
    const bool ok = (gained != FALSE);
    QMetaObject::invokeMethod(svc, [svc, ok]() {
        svc->handleSessionCompleted(ok);
    }, Qt::QueuedConnection);
}

static void on_cancellable_cancelled(GCancellable * /*cancellable*/, gpointer user_data) {
    auto *svc = static_cast<quattro::PolkitService *>(user_data);
    QMetaObject::invokeMethod(svc, [svc]() {
        svc->cancel();
    }, Qt::QueuedConnection);
}

} // namespace

namespace quattro {

PolkitService::PolkitService(QObject *parent)
    : QObject(parent)
{
    m_glibPumpTimer.setInterval(40);
    connect(&m_glibPumpTimer, &QTimer::timeout, this, []() {
        while (g_main_context_pending(nullptr))
            g_main_context_iteration(nullptr, FALSE);
    });
}

PolkitService::~PolkitService() {
    stop();
}

bool PolkitService::start() {
    if (m_registered)
        return true;

    GError *err = nullptr;
    PolkitSubject *subject = polkit_unix_session_new_for_process_sync(getpid(), nullptr, &err);
    if (!subject) {
        if (err) {
            qWarning("PolkitService: session lookup failed: %s", err->message);
            g_error_free(err);
        }
        return false;
    }

    auto *listener = static_cast<QuattroPolkitListener *>(
        g_object_new(quattro_polkit_listener_get_type(), nullptr));
    listener->service = this;

    gpointer reg = polkit_agent_listener_register(
        POLKIT_AGENT_LISTENER(listener),
        POLKIT_AGENT_REGISTER_FLAGS_NONE,
        subject,
        "/org/quattro/PolkitAgent",
        nullptr,
        &err);

    g_object_unref(subject);

    if (!reg) {
        if (err) {
            qWarning("PolkitService: register listener failed: %s", err->message);
            g_error_free(err);
        }
        g_object_unref(listener);
        return false;
    }

    m_listener = listener;
    m_regHandle = reg;
    m_registered = true;
    m_glibPumpTimer.start();
    return true;
}

void PolkitService::stop() {
    m_glibPumpTimer.stop();
    cancel();
    if (m_regHandle) {
        polkit_agent_listener_unregister(m_regHandle);
        m_regHandle = nullptr;
    }
    if (m_listener) {
        static_cast<QuattroPolkitListener *>(m_listener)->service = nullptr;
        g_object_unref(m_listener);
        m_listener = nullptr;
    }
    m_registered = false;
}

bool PolkitService::isPolkitFingerprintConfigured() {
    const QStringList candidates = {
        QStringLiteral("/etc/pam.d/quattro-lock"),
        QStringLiteral("/etc/pam.d/polkit-1")
    };
    for (const QString &path : candidates) {
        QFile f(path);
        if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            const QString content = QString::fromUtf8(f.readAll());
            if (content.contains(QStringLiteral("pam_fprintd.so")))
                return true;
        }
    }
    return false;
}

void PolkitService::handleInitiate(const QString &actionId,
                                   const QString &message,
                                   const QString &iconName,
                                   const QString &cookie,
                                   void *identityPtr,
                                   void *taskPtr,
                                   void *cancellablePtr) {
    // Cancel any stale active task before starting new one
    if (m_activeTask)
        cancel();

    m_activeIdentity = identityPtr; // already reffed by caller
    m_activeTask = taskPtr;
    m_cancellable = cancellablePtr;
    m_completedHandled = false;
    m_waitingForInput = false;
    m_authenticating = false;
    m_pendingResponse.clear();
    m_lastError.clear();

    QString idName;
    if (auto *id = POLKIT_IDENTITY(identityPtr)) {
        gchar *str = polkit_identity_to_string(id);
        if (str) {
            idName = QString::fromUtf8(str);
            g_free(str);
        }
        if (idName.startsWith(QStringLiteral("unix-user:")))
            idName = idName.mid(10);
    }

    m_current = PolkitRequestInfo{
        true,
        actionId,
        message,
        iconName,
        cookie,
        idName,
        QStringLiteral("Password"),
        false,
        isPolkitFingerprintConfigured()
    };

    if (m_cancellable) {
        auto *c = G_CANCELLABLE(m_cancellable);
        g_object_ref(c);
        m_cancelHandlerId = g_signal_connect(c, "cancelled", G_CALLBACK(on_cancellable_cancelled), this);
    }

    emit requestOpened(m_current);
    checkLidClosedAsync();
    beginSessionAttempt();
}

void PolkitService::checkLidClosedAsync() {
    auto *proc = new QProcess(this);
    connect(proc, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, [this, proc](int exitCode, QProcess::ExitStatus) {
        proc->deleteLater();
        if (m_current.active && exitCode == 0 && m_current.fingerprintAvailable) {
            m_current.fingerprintAvailable = false;
            emit requestOpened(m_current);
        }
    });
    proc->start(QStringLiteral("quattro-hw-laptop-closed"));
}

void PolkitService::beginSessionAttempt() {
    if (!m_activeIdentity || m_current.cookie.isEmpty())
        return;

    if (m_activeSession) {
        g_signal_handlers_disconnect_by_data(m_activeSession, this);
        g_object_unref(m_activeSession);
        m_activeSession = nullptr;
    }

    m_waitingForInput = false;
    const QByteArray cke = m_current.cookie.toUtf8();
    PolkitAgentSession *session = polkit_agent_session_new(POLKIT_IDENTITY(m_activeIdentity), cke.constData());
    m_activeSession = session;

    g_signal_connect(session, "request", G_CALLBACK(on_session_request), this);
    g_signal_connect(session, "show-error", G_CALLBACK(on_session_show_error), this);
    g_signal_connect(session, "show-info", G_CALLBACK(on_session_show_info), this);
    g_signal_connect(session, "completed", G_CALLBACK(on_session_completed), this);

    polkit_agent_session_initiate(session);
}

void PolkitService::handleSessionRequest(const QString &request, bool echoOn) {
    m_waitingForInput = true;
    m_authenticating = false;
    m_current.promptLabel = request.trimmed();
    if (m_current.promptLabel.endsWith(QLatin1Char(':')))
        m_current.promptLabel.chop(1);
    if (m_current.promptLabel.isEmpty())
        m_current.promptLabel = QStringLiteral("Password");
    m_current.echoOn = echoOn;

    emit promptUpdated(m_current.promptLabel, echoOn);

    if (!m_pendingResponse.isNull()) {
        const QString resp = m_pendingResponse;
        m_pendingResponse.clear();
        submitResponse(resp);
    }
}

void PolkitService::handleSessionShowError(const QString &text) {
    m_lastError = text.trimmed();
    emit statusMessage(m_lastError, true);
}

void PolkitService::handleSessionShowInfo(const QString &text) {
    emit statusMessage(text.trimmed(), false);
}

void PolkitService::handleSessionCompleted(bool gainedAuthorization) {
    m_authenticating = false;
    m_waitingForInput = false;

    if (!m_activeTask)
        return;

    if (gainedAuthorization) {
        auto *task = G_TASK(m_activeTask);
        m_activeTask = nullptr;
        g_task_return_boolean(task, TRUE);
        g_object_unref(task);
        cleanupActiveAttempt();
        m_current = {};
        emit requestClosed(true);
        return;
    }

    if (m_completedHandled)
        return;

    const QString err = m_lastError.isEmpty()
                            ? QStringLiteral("Incorrect password, try again")
                            : m_lastError;
    m_lastError.clear();
    emit authenticationFailed(err);

    // Re-initiate session for the same cookie so user can retry seamlessly
    beginSessionAttempt();
}

void PolkitService::submitResponse(const QString &response) {
    if (!m_activeSession || !m_current.active)
        return;
    if (!m_waitingForInput) {
        m_pendingResponse = response;
        m_authenticating = true;
        return;
    }
    m_waitingForInput = false;
    m_authenticating = true;
    const QByteArray utf8 = response.toUtf8();
    polkit_agent_session_response(POLKIT_AGENT_SESSION(m_activeSession), utf8.constData());
}

void PolkitService::cancel() {
    if (!m_activeTask && !m_activeSession && !m_current.active)
        return;

    m_completedHandled = true;

    if (m_activeSession) {
        g_signal_handlers_disconnect_by_data(m_activeSession, this);
        polkit_agent_session_cancel(POLKIT_AGENT_SESSION(m_activeSession));
        g_object_unref(m_activeSession);
        m_activeSession = nullptr;
    }

    if (m_activeTask) {
        auto *task = G_TASK(m_activeTask);
        m_activeTask = nullptr;
        g_task_return_boolean(task, FALSE);
        g_object_unref(task);
    }

    cleanupActiveAttempt();
    m_current = {};
    emit requestClosed(false);
}

void PolkitService::cleanupActiveAttempt() {
    m_waitingForInput = false;
    m_authenticating = false;
    m_pendingResponse.clear();
    m_lastError.clear();

    if (m_activeSession) {
        g_signal_handlers_disconnect_by_data(m_activeSession, this);
        g_object_unref(m_activeSession);
        m_activeSession = nullptr;
    }
    if (m_activeIdentity) {
        g_object_unref(m_activeIdentity);
        m_activeIdentity = nullptr;
    }
    if (m_cancellable) {
        auto *c = G_CANCELLABLE(m_cancellable);
        if (m_cancelHandlerId) {
            g_signal_handler_disconnect(c, m_cancelHandlerId);
            m_cancelHandlerId = 0;
        }
        g_object_unref(c);
        m_cancellable = nullptr;
    }
}

} // namespace quattro
