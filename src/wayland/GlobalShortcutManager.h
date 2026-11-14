#pragma once

#include <QObject>
#include <QString>
#include <QVector>
#include <QtWaylandClient/QWaylandClientExtensionTemplate>

#include "qwayland-hyprland-global-shortcuts-v1.h"

namespace quattro {

class GlobalShortcutBinding : public QObject, public QtWayland::hyprland_global_shortcut_v1 {
    Q_OBJECT
public:
    GlobalShortcutBinding(struct ::hyprland_global_shortcut_v1 *obj,
                          QString kind,
                          QString target,
                          QObject *parent = nullptr);
    ~GlobalShortcutBinding() override;

signals:
    void activated(const QString &kind, const QString &target);

protected:
    void hyprland_global_shortcut_v1_pressed(uint32_t tv_sec_hi, uint32_t tv_sec_lo, uint32_t tv_nsec) override;

private:
    QString m_kind;
    QString m_target;
};

class GlobalShortcutManager : public QWaylandClientExtensionTemplate<GlobalShortcutManager>,
                              public QtWayland::hyprland_global_shortcuts_manager_v1 {
    Q_OBJECT
public:
    explicit GlobalShortcutManager(const QString &quattroPath, QObject *parent = nullptr);
    ~GlobalShortcutManager() override;

    void start();

signals:
    void shortcutTriggered(const QString &kind, const QString &target);

private:
    void registerFromFile();

    QString m_quattroPath;
    QVector<GlobalShortcutBinding *> m_bindings;
    bool m_registered = false;
};

} // namespace quattro
