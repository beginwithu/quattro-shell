#include "IpcServer.h"

#include <QCryptographicHash>
#include <QFile>
#include <QLocalSocket>

namespace quattro {

IpcServer::IpcServer(QObject *parent)
    : QObject(parent)
{
    connect(&m_server, &QLocalServer::newConnection, this, &IpcServer::onNewConnection);
}

IpcServer::~IpcServer() {
    stop();
}

QString IpcServer::computeSocketPath(const QString &shellPath, const QString &waylandDisplay, const QString &runtimeDir) {
    const QByteArray payload = shellPath.toUtf8() + '\n' + waylandDisplay.toUtf8();
    const QByteArray hex = QCryptographicHash::hash(payload, QCryptographicHash::Md5).toHex();
    return runtimeDir + QStringLiteral("/quattro-shell-") + QString::fromLatin1(hex.left(16)) + QStringLiteral(".sock");
}

bool IpcServer::start(const QString &shellPath, const QString &waylandDisplay) {
    stop();
    const QString runtimeDir = qEnvironmentVariable("XDG_RUNTIME_DIR",
                                                    QStringLiteral("/run/user/%1").arg(qEnvironmentVariable("UID")));
    m_socketPath = computeSocketPath(shellPath, waylandDisplay, runtimeDir);
    QFile::remove(m_socketPath);
    return m_server.listen(m_socketPath);
}

void IpcServer::stop() {
    if (m_server.isListening())
        m_server.close();
    if (!m_socketPath.isEmpty())
        QFile::remove(m_socketPath);
}

void IpcServer::registerTarget(const QString &target, IpcHandlerFn handler) {
    m_handlers.insert(target, std::move(handler));
}

IpcResult IpcServer::dispatch(const QString &target, const QString &method, const QStringList &args) const {
    auto it = m_handlers.constFind(target);
    if (it == m_handlers.cend() || !it.value())
        return {false, {}};
    return it.value()(method, args);
}

QByteArray IpcServer::encodeReply(const IpcResult &res) {
    if (!res.handled)
        return QByteArray("SKIP\x1e");
    QByteArray out("OK\x1f");
    out.append(res.output.toUtf8());
    out.append('\x1e');
    return out;
}

void IpcServer::onNewConnection() {
    while (QLocalSocket *sock = m_server.nextPendingConnection()) {
        auto *buf = new QByteArray();
        connect(sock, &QLocalSocket::readyRead, this, [this, sock, buf]() {
            buf->append(sock->readAll());
            const int rs = buf->indexOf('\x1e');
            if (rs < 0)
                return;

            const QByteArray frame = buf->left(rs);
            buf->remove(0, rs + 1);
            const QList<QByteArray> fields = frame.split('\x1f');

            IpcResult res{false, {}};
            if (fields.size() >= 2) {
                const QString target = QString::fromUtf8(fields.at(0));
                const QString method = QString::fromUtf8(fields.at(1));
                QStringList args;
                for (int i = 2; i < fields.size(); ++i)
                    args.push_back(QString::fromUtf8(fields.at(i)));
                res = dispatch(target, method, args);
            }
            sock->write(encodeReply(res));
            sock->flush();
            sock->disconnectFromServer();
        });
        connect(sock, &QLocalSocket::disconnected, sock, [sock, buf]() {
            delete buf;
            sock->deleteLater();
        });
    }
}

} // namespace quattro
