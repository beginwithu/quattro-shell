#include "GlobalShortcutManager.h"

#include <QFile>
#include <QRegularExpression>

namespace quattro {

GlobalShortcutBinding::GlobalShortcutBinding(struct ::hyprland_global_shortcut_v1 *obj,
                                             QString kind,
                                             QString target,
                                             QObject *parent)
    : QObject(parent)
    , QtWayland::hyprland_global_shortcut_v1(obj)
    , m_kind(std::move(kind))
    , m_target(std::move(target))
{
}

GlobalShortcutBinding::~GlobalShortcutBinding() {
    if (isInitialized())
        destroy();
}

void GlobalShortcutBinding::hyprland_global_shortcut_v1_pressed(uint32_t, uint32_t, uint32_t) {
    emit activated(m_kind, m_target);
}

GlobalShortcutManager::GlobalShortcutManager(const QString &quattroPath, QObject *parent)
    : QWaylandClientExtensionTemplate<GlobalShortcutManager>(1)
    , m_quattroPath(quattroPath)
{
    setParent(parent);
    connect(this, &QWaylandClientExtension::activeChanged, this, [this]() {
        if (isActive())
            registerFromFile();
    });
}

GlobalShortcutManager::~GlobalShortcutManager() {
    qDeleteAll(m_bindings);
    m_bindings.clear();
    if (isInitialized())
        destroy();
}

void GlobalShortcutManager::start() {
    initialize();
    if (isActive())
        registerFromFile();
}

void GlobalShortcutManager::registerFromFile() {
    if (m_registered || !isActive())
        return;
    m_registered = true;

    const QString path = m_quattroPath + QStringLiteral("/default/quattro/shortcuts");
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return;

    static const QRegularExpression lineRe(QStringLiteral("^([A-Za-z]+)\\s+(\\S+)\\s*$"));
    while (!f.atEnd()) {
        const QString line = QString::fromUtf8(f.readLine()).trimmed();
        if (line.isEmpty() || line.startsWith(QLatin1Char('#')))
            continue;
        const auto m = lineRe.match(line);
        if (!m.hasMatch())
            continue;
        const QString kind = m.captured(1);
        const QString target = m.captured(2);
        const QString id = kind + QLatin1Char('.') + target;

        struct ::hyprland_global_shortcut_v1 *obj =
            register_shortcut(id, QStringLiteral("quattro"), id, id);
        if (obj) {
            auto *binding = new GlobalShortcutBinding(obj, kind, target, this);
            connect(binding, &GlobalShortcutBinding::activated,
                    this, &GlobalShortcutManager::shortcutTriggered);
            m_bindings.push_back(binding);
        }
    }
}

} // namespace quattro
