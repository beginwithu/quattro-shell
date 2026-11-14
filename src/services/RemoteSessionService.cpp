#include "RemoteSessionService.h"

#include <QDir>
#include <QFile>
#include <QRandomGenerator>
#include <QSet>
#include <unistd.h>

namespace quattro {

RemoteSessionService::RemoteSessionService(HyprlandClient *hypr, QObject *parent)
    : QObject(parent)
    , m_hypr(hypr)
    , m_captureOwner(QString::number(QRandomGenerator::global()->generate64(), 16))
{
    m_pollTimer.setInterval(2000);
    connect(&m_pollTimer, &QTimer::timeout, this, &RemoteSessionService::probe);

    m_debounceTimer.setSingleShot(true);
    m_debounceTimer.setInterval(150);
    connect(&m_debounceTimer, &QTimer::timeout, this, &RemoteSessionService::probe);

    if (m_hypr) {
        connect(m_hypr, &HyprlandClient::configReloaded, this, [this]() {
            m_ruleApplied = false;
            probe();
        });
        connect(m_hypr, &HyprlandClient::screencastChanged, this, [this]() {
            m_debounceTimer.start();
        });
    }
    probe();
}

RemoteSessionService::~RemoteSessionService() {
    if (m_active && m_hypr)
        applyScreenShareRule(false);
}

void RemoteSessionService::probe() {
    const uid_t myUid = getuid();
    QDir procDir(QStringLiteral("/proc"));
    const QStringList entries = procDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    bool found = false;
    QStringList peers;
    QSet<QString> seenPeers;

    for (const QString &pidStr : entries) {
        bool ok = false;
        pidStr.toUInt(&ok);
        if (!ok) continue;

        QFile commFile(QStringLiteral("/proc/") + pidStr + QStringLiteral("/comm"));
        if (!commFile.open(QIODevice::ReadOnly)) continue;
        const QByteArray comm = commFile.readAll().trimmed();
        if (comm != "gliff-server") continue;

        QFile statusFile(QStringLiteral("/proc/") + pidStr + QStringLiteral("/status"));
        if (statusFile.open(QIODevice::ReadOnly)) {
            const QByteArray st = statusFile.readAll();
            const int idx = st.indexOf("Uid:");
            if (idx >= 0) {
                const QList<QByteArray> parts = st.mid(idx + 4, 48).split('\t');
                if (!parts.isEmpty() && parts.first().trimmed().toUInt() != myUid)
                    continue;
            }
        }

        found = true;
        QFile envFile(QStringLiteral("/proc/") + pidStr + QStringLiteral("/environ"));
        if (envFile.open(QIODevice::ReadOnly)) {
            const QList<QByteArray> vars = envFile.readAll().split('\0');
            for (const QByteArray &kv : vars) {
                if (kv.startsWith("SSH_CONNECTION=")) {
                    const QString peer = QString::fromUtf8(kv.mid(15)).section(QLatin1Char(' '), 0, 0).trimmed();
                    if (!peer.isEmpty() && !seenPeers.contains(peer)) {
                        seenPeers.insert(peer);
                        peers.push_back(peer);
                    }
                }
            }
        }
    }

    if (found && !m_pollTimer.isActive()) m_pollTimer.start();
    else if (!found && m_pollTimer.isActive()) m_pollTimer.stop();

    const bool changedNow = (found != m_active || peers != m_peers);
    m_active = found;
    m_peers = peers;

    if (!m_ruleApplied || changedNow) {
        m_ruleApplied = true;
        applyScreenShareRule(m_active);
    }

    if (changedNow)
        emit changed();
}

void RemoteSessionService::applyScreenShareRule(bool enable) {
    if (!m_hypr || !m_hypr->isAvailable())
        return;
    const QString lua = QStringLiteral(
        "hl.window_rule({ name = \"quattro-remote-screen-share\", match = { class = \".*\" }, "
        "no_screen_share = false, enabled = %1 })").arg(enable ? QStringLiteral("true") : QStringLiteral("false"));
    m_hypr->eval(lua);
}

} // namespace quattro
