#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <functional>

namespace quattro {

struct IpcResult {
    bool handled = false;
    QString output;
};

using IpcHandlerFn = std::function<IpcResult(const QString &method, const QStringList &args)>;

class IpcServer : public QObject {
    Q_OBJECT
public:
    explicit IpcServer(QObject *parent = nullptr);
    ~IpcServer() override;

    bool start(const QString &shellPath, const QString &waylandDisplay);
    void stop();

    void registerTarget(const QString &target, IpcHandlerFn handler);
    IpcResult dispatch(const QString &target, const QString &method, const QStringList &args) const;

    QString socketPath() const { return m_socketPath; }

    static QString computeSocketPath(const QString &shellPath, const QString &waylandDisplay, const QString &runtimeDir);
    static QByteArray encodeReply(const IpcResult &res);

private slots:
    void onNewConnection();

private:
    QString m_socketPath;
    QLocalServer m_server;
    QHash<QString, IpcHandlerFn> m_handlers;
};

} // namespace quattro
