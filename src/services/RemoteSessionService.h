#pragma once

#include "../core/HyprlandClient.h"
#include <QObject>
#include <QStringList>
#include <QTimer>

namespace quattro {

class RemoteSessionService : public QObject {
    Q_OBJECT
public:
    explicit RemoteSessionService(HyprlandClient *hypr, QObject *parent = nullptr);
    ~RemoteSessionService() override;

    bool active() const { return m_active; }
    QStringList peers() const { return m_peers; }
    void probe();

signals:
    void changed();

private:
    void applyScreenShareRule(bool enable);

    HyprlandClient *m_hypr;
    bool m_active = false;
    bool m_ruleApplied = false;
    QStringList m_peers;
    QString m_captureOwner;
    QTimer m_pollTimer;
    QTimer m_debounceTimer;
};

} // namespace quattro
