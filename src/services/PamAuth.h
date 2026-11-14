#pragma once

#include <QObject>
#include <QString>

namespace quattro {

class PamAuth : public QObject {
    Q_OBJECT
public:
    explicit PamAuth(QObject *parent = nullptr);

    static bool isPamConfigured();
    static QString currentUsername();

    void authenticateAsync(const QString &password);
    void authenticateWithServiceAsync(const QString &serviceName, const QString &password);
    bool isAuthenticating() const { return m_authenticating; }

signals:
    void authenticationFinished(bool success, const QString &message);

private:
    bool m_authenticating = false;
};

} // namespace quattro
