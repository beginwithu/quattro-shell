#include "HyprlandClient.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace quattro {

HyprlandClient::HyprlandClient(QObject *parent)
    : QObject(parent)
{
    const QString sig = qEnvironmentVariable("HYPRLAND_INSTANCE_SIGNATURE");
    if (!sig.isEmpty()) {
        const QString runtime = qEnvironmentVariable("XDG_RUNTIME_DIR",
                                                     QStringLiteral("/run/user/%1").arg(qEnvironmentVariable("UID")));
        const QString base = runtime + QStringLiteral("/hypr/") + sig;
        if (QFileInfo::exists(base + QStringLiteral("/.socket.sock"))) {
            m_socketPath = base + QStringLiteral("/.socket.sock");
            m_eventSocketPath = base + QStringLiteral("/.socket2.sock");
        }
    }

    m_reconnectTimer.setInterval(2000);
    m_reconnectTimer.setSingleShot(true);
    connect(&m_reconnectTimer, &QTimer::timeout, this, &HyprlandClient::reconnectEventSocket);

    m_refreshDebounce.setInterval(15);
    m_refreshDebounce.setSingleShot(true);
    connect(&m_refreshDebounce, &QTimer::timeout, this, &HyprlandClient::refreshState);

    connect(&m_eventSocket, &QLocalSocket::readyRead, this, &HyprlandClient::onEventReadyRead);
    connect(&m_eventSocket, &QLocalSocket::disconnected, this, [this]() {
        if (!m_eventSocketPath.isEmpty())
            m_reconnectTimer.start();
    });

    if (isAvailable()) {
        refreshAll();
        reconnectEventSocket();
    }
}

QByteArray HyprlandClient::request(const QByteArray &cmd) const {
    if (m_socketPath.isEmpty())
        return {};
    QLocalSocket sock;
    sock.connectToServer(m_socketPath);
    if (!sock.waitForConnected(200))
        return {};
    sock.write(cmd);
    sock.flush();
    QByteArray out;
    while (sock.waitForReadyRead(200))
        out.append(sock.readAll());
    sock.disconnectFromServer();
    return out;
}

bool HyprlandClient::dispatch(const QString &expression) {
    const QByteArray resp = request("dispatch " + expression.toUtf8());
    return resp.trimmed() == "ok";
}

QByteArray HyprlandClient::eval(const QString &luaExpr) const {
    return request("eval " + luaExpr.toUtf8());
}

void HyprlandClient::refreshAll() {
    refreshState();
    refreshOptions();
}

void HyprlandClient::refreshState() {
    if (!isAvailable())
        return;

    bool changed = false;

    const QByteArray monRaw = request("j/monitors");
    if (!monRaw.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(monRaw);
        if (doc.isArray()) {
            QVector<HyprMonitor> nextMons;
            for (const QJsonValue &v : doc.array()) {
                const QJsonObject o = v.toObject();
                HyprMonitor m;
                m.id = o.value(QStringLiteral("id")).toInt(-1);
                m.name = o.value(QStringLiteral("name")).toString();
                m.activeWorkspaceId = o.value(QStringLiteral("activeWorkspace")).toObject().value(QStringLiteral("id")).toInt(-1);
                m.focused = o.value(QStringLiteral("focused")).toBool(false);
                nextMons.push_back(m);
            }
            m_monitors = nextMons;
            changed = true;
        }
    }

    const QByteArray wsRaw = request("j/workspaces");
    if (!wsRaw.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(wsRaw);
        if (doc.isArray()) {
            QVector<HyprWorkspace> nextWs;
            for (const QJsonValue &v : doc.array()) {
                const QJsonObject o = v.toObject();
                HyprWorkspace w;
                w.id = o.value(QStringLiteral("id")).toInt(-1);
                w.name = o.value(QStringLiteral("name")).toString();
                w.monitorName = o.value(QStringLiteral("monitor")).toString();
                w.windows = o.value(QStringLiteral("windows")).toInt(0);
                nextWs.push_back(w);
            }
            m_workspaces = nextWs;
            changed = true;
        }
    }

    const QByteArray awRaw = request("j/activewindow");
    if (!awRaw.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(awRaw);
        if (doc.isObject()) {
            const QJsonObject o = doc.object();
            const QString title = o.value(QStringLiteral("title")).toString();
            const QString cls = o.value(QStringLiteral("class")).toString();
            if (title != m_activeWindowTitle || cls != m_activeWindowClass) {
                m_activeWindowTitle = title;
                m_activeWindowClass = cls;
                emit activeWindowChanged();
            }
        }
    }

    if (m_keyboardLayout.isEmpty()) {
        const QByteArray devRaw = request("j/devices");
        const QJsonDocument doc = QJsonDocument::fromJson(devRaw);
        if (doc.isObject()) {
            const QJsonArray kbs = doc.object().value(QStringLiteral("keyboards")).toArray();
            for (const QJsonValue &v : kbs) {
                const QJsonObject kb = v.toObject();
                if (kb.value(QStringLiteral("main")).toBool(false) || m_keyboardLayout.isEmpty()) {
                    const QString keymap = kb.value(QStringLiteral("active_keymap")).toString();
                    if (!keymap.isEmpty()) {
                        m_keyboardLayout = keymap;
                        emit keyboardLayoutChanged(m_keyboardLayout);
                    }
                }
            }
        }
    }

    if (changed)
        emit stateChanged();
}

void HyprlandClient::refreshOptions() {
    if (!isAvailable())
        return;

    int nextRounding = m_rounding;
    int nextGapsOut = m_gapsOut;
    bool nextAnim = m_animationsEnabled;

    if (const QByteArray r = request("j/getoption decoration:rounding"); !r.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(r);
        if (doc.isObject() && doc.object().contains(QStringLiteral("int")))
            nextRounding = std::max(0, doc.object().value(QStringLiteral("int")).toInt(nextRounding));
    }

    if (const QByteArray g = request("j/getoption general:gaps_out"); !g.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(g);
        if (doc.isObject()) {
            const QJsonObject o = doc.object();
            const QString css = o.value(QStringLiteral("css")).toString();
            static const QRegularExpression numRe(QStringLiteral("-?\\d+(?:\\.\\d+)?"));
            const auto m = numRe.match(css);
            const double val = m.hasMatch() ? m.captured(0).toDouble() : o.value(QStringLiteral("int")).toDouble(10.0);
            nextGapsOut = std::max(0, static_cast<int>(std::lround(val / 2.0)));
        }
    }

    if (const QByteArray a = request("j/getoption animations:enabled"); !a.isEmpty()) {
        const QJsonDocument doc = QJsonDocument::fromJson(a);
        if (doc.isObject() && doc.object().contains(QStringLiteral("bool")))
            nextAnim = doc.object().value(QStringLiteral("bool")).toBool(true);
    }

    if (nextRounding != m_rounding || nextGapsOut != m_gapsOut || nextAnim != m_animationsEnabled) {
        m_rounding = nextRounding;
        m_gapsOut = nextGapsOut;
        m_animationsEnabled = nextAnim;
        emit optionsChanged(m_rounding, m_gapsOut, m_animationsEnabled);
    }
}

HyprMonitor HyprlandClient::monitorByName(const QString &name) const {
    for (const HyprMonitor &m : m_monitors) {
        if (m.name == name)
            return m;
    }
    return {};
}

QString HyprlandClient::focusedMonitorName() const {
    for (const HyprMonitor &m : m_monitors) {
        if (m.focused)
            return m.name;
    }
    return m_monitors.isEmpty() ? QString() : m_monitors.first().name;
}

QVector<HyprClientInfo> HyprlandClient::clients() const {
    QVector<HyprClientInfo> out;
    const QByteArray raw = request("j/clients");
    if (raw.isEmpty())
        return out;
    const QJsonDocument doc = QJsonDocument::fromJson(raw);
    if (!doc.isArray())
        return out;
    for (const QJsonValue &v : doc.array()) {
        const QJsonObject o = v.toObject();
        HyprClientInfo c;
        c.address = o.value(QStringLiteral("address")).toString();
        c.appClass = o.value(QStringLiteral("class")).toString();
        c.initialClass = o.value(QStringLiteral("initialClass")).toString();
        c.title = o.value(QStringLiteral("title")).toString();
        const QJsonObject ws = o.value(QStringLiteral("workspace")).toObject();
        c.workspaceId = ws.value(QStringLiteral("id")).toInt(-1);
        c.workspaceName = ws.value(QStringLiteral("name")).toString();
        c.monitorId = o.value(QStringLiteral("monitor")).toInt(-1);
        c.fullscreen = o.value(QStringLiteral("fullscreen")).toInt(0);
        c.floating = o.value(QStringLiteral("floating")).toBool(false);
        c.mapped = o.value(QStringLiteral("mapped")).toBool(true);
        c.hidden = o.value(QStringLiteral("hidden")).toBool(false);
        c.focusHistoryId = o.value(QStringLiteral("focusHistoryID")).toInt(0);
        out.push_back(c);
    }
    return out;
}

int HyprlandClient::activeWorkspaceForMonitor(const QString &name) const {
    for (const HyprMonitor &m : m_monitors) {
        if (m.name == name)
            return m.activeWorkspaceId;
    }
    return -1;
}

bool HyprlandClient::workspaceHasWindows(int id) const {
    for (const HyprWorkspace &w : m_workspaces) {
        if (w.id == id)
            return w.windows > 0;
    }
    return false;
}

void HyprlandClient::reconnectEventSocket() {
    if (m_eventSocketPath.isEmpty())
        return;
    if (m_eventSocket.state() == QLocalSocket::ConnectedState)
        return;
    m_eventSocket.connectToServer(m_eventSocketPath);
}

void HyprlandClient::onEventReadyRead() {
    m_eventBuffer.append(m_eventSocket.readAll());
    int nl = -1;
    while ((nl = m_eventBuffer.indexOf('\n')) != -1) {
        const QString line = QString::fromUtf8(m_eventBuffer.left(nl)).trimmed();
        m_eventBuffer.remove(0, nl + 1);
        if (!line.isEmpty())
            handleEventLine(line);
    }
}

void HyprlandClient::handleEventLine(const QString &line) {
    const int sep = line.indexOf(QStringLiteral(">>"));
    if (sep < 0)
        return;
    const QString ev = line.left(sep);
    const QString data = line.mid(sep + 2);

    if (ev == QLatin1String("workspacev2") || ev == QLatin1String("moveworkspacev2")
        || ev == QLatin1String("changeworkspaceid") || ev == QLatin1String("focusedmon")
        || ev == QLatin1String("focusedmonv2") || ev == QLatin1String("openwindow")
        || ev == QLatin1String("closewindow") || ev == QLatin1String("movewindow")
        || ev == QLatin1String("monitoradded") || ev == QLatin1String("monitorremoved")) {
        m_refreshDebounce.start();
    } else if (ev == QLatin1String("activewindow")) {
        const int comma = data.indexOf(QLatin1Char(','));
        if (comma >= 0) {
            m_activeWindowClass = data.left(comma);
            m_activeWindowTitle = data.mid(comma + 1);
            emit activeWindowChanged();
        }
    } else if (ev == QLatin1String("activelayout")) {
        const int comma = data.lastIndexOf(QLatin1Char(','));
        const QString layout = comma >= 0 ? data.mid(comma + 1).trimmed() : data.trimmed();
        if (!layout.isEmpty() && layout != m_keyboardLayout) {
            m_keyboardLayout = layout;
            emit keyboardLayoutChanged(m_keyboardLayout);
        }
    } else if (ev == QLatin1String("configreloaded")) {
        refreshOptions();
        m_refreshDebounce.start();
        emit configReloaded();
    } else if (ev == QLatin1String("fullscreen")) {
        m_refreshDebounce.start();
        emit fullscreenChanged();
    } else if (ev == QLatin1String("screencast")) {
        emit screencastChanged();
    }
}

} // namespace quattro
