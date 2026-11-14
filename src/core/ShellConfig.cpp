#include "ShellConfig.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace quattro {

namespace {

QByteArray readBytes(const QString &path) {
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return {};
    return f.readAll();
}

QString resolveQuattroPath(const QString &home) {
    const QString env = qEnvironmentVariable("QUATTRO_PATH");
    if (!env.isEmpty() && QFileInfo::exists(env))
        return env;
    const QString local = home + QStringLiteral("/.local/share/quattro");
    if (QFileInfo::exists(local))
        return local;
    if (QFileInfo::exists(QStringLiteral("/usr/share/quattro")))
        return QStringLiteral("/usr/share/quattro");
    return local;
}

} // namespace

ShellConfig::ShellConfig(QObject *parent)
    : QObject(parent)
    , m_home(QDir::homePath())
    , m_quattroPath(resolveQuattroPath(m_home))
    , m_userConfigPath(qEnvironmentVariable("XDG_CONFIG_HOME", m_home + QStringLiteral("/.config"))
                       + QStringLiteral("/quattro/shell.json"))
    , m_barOffFlagPath(qEnvironmentVariable("XDG_STATE_HOME", m_home + QStringLiteral("/.local/state"))
                       + QStringLiteral("/quattro/toggles/bar-off"))
    , m_defaults(builtinFallback())
    , m_effective(m_defaults)
{
    if (QFileInfo::exists(defaultsPath()))
        m_watcher.addPath(defaultsPath());
    if (QFileInfo::exists(m_userConfigPath))
        m_watcher.addPath(m_userConfigPath);
    const QString togglesDir = QFileInfo(m_barOffFlagPath).absolutePath();
    if (QFileInfo::exists(togglesDir))
        m_watcher.addPath(togglesDir);

    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this](const QString &path) {
        if (QFileInfo::exists(path) && !m_watcher.files().contains(path))
            m_watcher.addPath(path);
        if (path == defaultsPath() || path == m_userConfigPath)
            loadFromDisk();
    });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this](const QString &) {
        syncBarHidden();
    });

    loadFromDisk();
    syncBarHidden();
}

QJsonObject ShellConfig::builtinFallback() {
    QJsonObject layout;
    layout[QStringLiteral("left")] = QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("quattro.menu")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("quattro.workspaces")}}
    };
    layout[QStringLiteral("centerBegin")] = QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("quattro.indicators")}}
    };
    layout[QStringLiteral("center")] = QJsonArray{
        QJsonObject{
            {QStringLiteral("id"), QStringLiteral("quattro.clock")},
            {QStringLiteral("format"), QStringLiteral("dddd HH:mm")}
        }
    };
    layout[QStringLiteral("centerEnd")] = QJsonArray{};
    layout[QStringLiteral("right")] = QJsonArray{
        QJsonObject{{QStringLiteral("id"), QStringLiteral("quattro.audio")}},
        QJsonObject{{QStringLiteral("id"), QStringLiteral("quattro.power")}}
    };

    QJsonObject bar;
    bar[QStringLiteral("position")] = QStringLiteral("top");
    bar[QStringLiteral("background")] = QStringLiteral("opaque");
    bar[QStringLiteral("layout")] = layout;

    QJsonObject idle;
    idle[QStringLiteral("screensaver")] = 150;
    idle[QStringLiteral("lock")] = 300;

    QJsonObject dock;
    dock[QStringLiteral("position")] = QStringLiteral("bottom");
    dock[QStringLiteral("autohide")] = false;
    dock[QStringLiteral("pinned")] = QJsonArray{};

    QJsonObject root;
    root[QStringLiteral("version")] = 1;
    root[QStringLiteral("idle")] = idle;
    root[QStringLiteral("dock")] = dock;
    root[QStringLiteral("bar")] = bar;
    root[QStringLiteral("plugins")] = QJsonArray{};
    return root;
}

void ShellConfig::loadFromDisk() {
    applyJsonText(readBytes(defaultsPath()), readBytes(m_userConfigPath));
}

bool ShellConfig::reloadUserConfig() {
    return applyJsonText(readBytes(defaultsPath()), readBytes(m_userConfigPath));
}

bool ShellConfig::applyJsonText(const QByteArray &defaultsRaw, const QByteArray &userRaw) {
    QJsonObject nextDefaults = builtinFallback();
    if (!defaultsRaw.trimmed().isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(defaultsRaw, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()
            && doc.object().value(QStringLiteral("version")).toInt() == 1) {
            nextDefaults = doc.object();
        }
    }
    m_defaults = nextDefaults;

    QJsonObject nextUser;
    bool hasValidUser = false;
    const QByteArray trimmedUser = userRaw.trimmed();
    if (!trimmedUser.isEmpty()) {
        QJsonParseError err;
        const QJsonDocument doc = QJsonDocument::fromJson(trimmedUser, &err);
        if (err.error == QJsonParseError::NoError && doc.isObject()
            && doc.object().value(QStringLiteral("version")).toInt() == 1) {
            nextUser = doc.object();
            hasValidUser = true;
        }
    }

    m_userUnreadable = !trimmedUser.isEmpty() && !hasValidUser;
    const QJsonObject nextEffective = hasValidUser ? nextUser : m_defaults;
    if (nextEffective != m_effective) {
        m_effective = nextEffective;
        emit configChanged();
    }
    return !m_userUnreadable;
}

QString ShellConfig::barPosition() const {
    const QString pos = m_effective.value(QStringLiteral("bar")).toObject()
                            .value(QStringLiteral("position")).toString(QStringLiteral("top")).toLower();
    if (pos == QLatin1String("bottom") || pos == QLatin1String("left") || pos == QLatin1String("right"))
        return pos;
    return QStringLiteral("top");
}

QString ShellConfig::barBackground() const {
    const QString bg = m_effective.value(QStringLiteral("bar")).toObject()
                           .value(QStringLiteral("background")).toString(QStringLiteral("opaque")).toLower();
    if (bg == QLatin1String("glassy") || bg == QLatin1String("none") || bg == QLatin1String("transparent"))
        return bg == QLatin1String("transparent") ? QStringLiteral("none") : bg;
    return QStringLiteral("opaque");
}

QVector<BarWidgetEntry> ShellConfig::parseSection(const QJsonArray &arr) {
    QVector<BarWidgetEntry> out;
    out.reserve(arr.size());
    for (const QJsonValue &v : arr) {
        if (v.isObject()) {
            const QJsonObject obj = v.toObject();
            const QString id = obj.value(QStringLiteral("id")).toString().trimmed();
            if (!id.isEmpty())
                out.push_back({id, obj});
        } else if (v.isString()) {
            const QString id = v.toString().trimmed();
            if (!id.isEmpty())
                out.push_back({id, QJsonObject{{QStringLiteral("id"), id}}});
        }
    }
    return out;
}

BarLayout ShellConfig::barLayout() const {
    const QJsonObject layout = m_effective.value(QStringLiteral("bar")).toObject()
                                   .value(QStringLiteral("layout")).toObject();
    BarLayout out;
    out.left = parseSection(layout.value(QStringLiteral("left")).toArray());
    out.centerBegin = parseSection(layout.value(QStringLiteral("centerBegin")).toArray());
    out.center = parseSection(layout.value(QStringLiteral("center")).toArray());
    out.centerEnd = parseSection(layout.value(QStringLiteral("centerEnd")).toArray());
    out.tray = parseSection(layout.value(QStringLiteral("tray")).toArray());
    out.right = parseSection(layout.value(QStringLiteral("right")).toArray());
    return out;
}

int ShellConfig::idleScreensaverSec() const {
    return std::max(0, m_effective.value(QStringLiteral("idle")).toObject()
                           .value(QStringLiteral("screensaver")).toInt(150));
}

int ShellConfig::idleLockSec() const {
    return std::max(0, m_effective.value(QStringLiteral("idle")).toObject()
                           .value(QStringLiteral("lock")).toInt(300));
}

QString ShellConfig::dockPosition() const {
    const QJsonObject d = m_effective.contains(QStringLiteral("dock"))
                              ? m_effective.value(QStringLiteral("dock")).toObject()
                              : m_defaults.value(QStringLiteral("dock")).toObject();
    const QString pos = d.value(QStringLiteral("position")).toString(QStringLiteral("bottom")).toLower();
    if (pos == QLatin1String("top") || pos == QLatin1String("left") || pos == QLatin1String("right"))
        return pos;
    return QStringLiteral("bottom");
}

bool ShellConfig::dockAutohide() const {
    const QJsonObject d = m_effective.contains(QStringLiteral("dock"))
                              ? m_effective.value(QStringLiteral("dock")).toObject()
                              : m_defaults.value(QStringLiteral("dock")).toObject();
    return d.value(QStringLiteral("autohide")).toBool(false);
}

QStringList ShellConfig::dockPinned() const {
    const QJsonObject d = m_effective.contains(QStringLiteral("dock"))
                              ? m_effective.value(QStringLiteral("dock")).toObject()
                              : m_defaults.value(QStringLiteral("dock")).toObject();
    QStringList out;
    for (const QJsonValue &v : d.value(QStringLiteral("pinned")).toArray()) {
        QString s = v.toString().trimmed();
        if (s.endsWith(QLatin1String(".desktop"), Qt::CaseInsensitive))
            s.chop(8);
        if (!s.isEmpty() && !out.contains(s))
            out.push_back(s);
    }
    return out;
}

bool ShellConfig::setDockPosition(const QString &pos) {
    QJsonObject root = m_effective;
    QJsonObject d = root.value(QStringLiteral("dock")).toObject();
    d[QStringLiteral("position")] = pos;
    root[QStringLiteral("dock")] = d;
    return persistUserConfig(root);
}

bool ShellConfig::setDockAutohide(bool enabled) {
    QJsonObject root = m_effective;
    QJsonObject d = root.value(QStringLiteral("dock")).toObject();
    d[QStringLiteral("autohide")] = enabled;
    root[QStringLiteral("dock")] = d;
    return persistUserConfig(root);
}

bool ShellConfig::setDockPinned(const QStringList &pinned) {
    QJsonObject root = m_effective;
    QJsonObject d = root.value(QStringLiteral("dock")).toObject();
    QJsonArray arr;
    for (const QString &p : pinned)
        arr.append(p);
    d[QStringLiteral("pinned")] = arr;
    root[QStringLiteral("dock")] = d;
    return persistUserConfig(root);
}

QJsonObject ShellConfig::barWidgetEntry(const QString &id) const {
    const BarLayout lay = barLayout();
    const QVector<BarWidgetEntry> all = lay.left + lay.centerBegin + lay.center + lay.centerEnd + lay.tray + lay.right;
    for (const auto &e : all) {
        if (e.id == id)
            return e.raw;
    }
    return {};
}

QString ShellConfig::barWidgetIdAt(const QString &section, int index) const {
    if (index < 0)
        return {};
    const BarLayout lay = barLayout();
    const QVector<BarWidgetEntry> *vec = nullptr;
    if (section == QLatin1String("left")) vec = &lay.left;
    else if (section == QLatin1String("centerBegin")) vec = &lay.centerBegin;
    else if (section == QLatin1String("center")) vec = &lay.center;
    else if (section == QLatin1String("centerEnd")) vec = &lay.centerEnd;
    else if (section == QLatin1String("tray")) vec = &lay.tray;
    else if (section == QLatin1String("right")) vec = &lay.right;
    if (!vec || index >= vec->size())
        return {};
    return (*vec)[index].id;
}

bool ShellConfig::updateEntryInline(const QString &id, const QJsonObject &patch) {
    QJsonObject nextRoot = m_effective;
    QJsonObject bar = nextRoot.value(QStringLiteral("bar")).toObject();
    QJsonObject layout = bar.value(QStringLiteral("layout")).toObject();
    bool updated = false;

    const QStringList sections{
        QStringLiteral("left"), QStringLiteral("centerBegin"), QStringLiteral("center"),
        QStringLiteral("centerEnd"), QStringLiteral("tray"), QStringLiteral("right")
    };
    for (const QString &sec : sections) {
        QJsonArray arr = layout.value(sec).toArray();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject obj = arr.at(i).toObject();
            if (obj.value(QStringLiteral("id")).toString() == id) {
                for (auto it = patch.cbegin(); it != patch.cend(); ++it)
                    obj[it.key()] = it.value();
                arr[i] = obj;
                updated = true;
            }
        }
        layout[sec] = arr;
    }
    if (!updated)
        return false;
    bar[QStringLiteral("layout")] = layout;
    nextRoot[QStringLiteral("bar")] = bar;
    return persistUserConfig(nextRoot);
}

bool ShellConfig::persistUserConfig(const QJsonObject &nextObj) {
    QJsonObject payload = nextObj;
    payload[QStringLiteral("version")] = 1;
    m_effective = payload;
    emit configChanged();

    if (m_userUnreadable)
        return false;

    const QFileInfo fi(m_userConfigPath);
    QDir().mkpath(fi.absolutePath());
    QSaveFile file(m_userConfigPath);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    file.write(QJsonDocument(payload).toJson(QJsonDocument::Indented));
    return file.commit();
}

QString ShellConfig::cycleBarBackground() {
    const QString cur = barBackground();
    QString next = QStringLiteral("opaque");
    if (cur == QLatin1String("opaque")) next = QStringLiteral("glassy");
    else if (cur == QLatin1String("glassy")) next = QStringLiteral("none");
    else next = QStringLiteral("opaque");

    QJsonObject nextRoot = m_effective;
    QJsonObject bar = nextRoot.value(QStringLiteral("bar")).toObject();
    bar[QStringLiteral("background")] = next;
    nextRoot[QStringLiteral("bar")] = bar;
    persistUserConfig(nextRoot);
    return next;
}

bool ShellConfig::setBarWidgetSetting(const QString &id, const QString &key, const QJsonValue &value) {
    QJsonObject nextRoot = m_effective;
    QJsonObject bar = nextRoot.value(QStringLiteral("bar")).toObject();
    QJsonObject layout = bar.value(QStringLiteral("layout")).toObject();
    bool updated = false;

    const QStringList sections{
        QStringLiteral("left"), QStringLiteral("centerBegin"), QStringLiteral("center"),
        QStringLiteral("centerEnd"), QStringLiteral("tray"), QStringLiteral("right")
    };
    for (const QString &sec : sections) {
        QJsonArray arr = layout.value(sec).toArray();
        for (int i = 0; i < arr.size(); ++i) {
            QJsonObject obj = arr.at(i).toObject();
            if (obj.value(QStringLiteral("id")).toString() == id) {
                obj[key] = value;
                arr[i] = obj;
                updated = true;
            }
        }
        layout[sec] = arr;
    }

    if (!updated)
        return false;
    bar[QStringLiteral("layout")] = layout;
    nextRoot[QStringLiteral("bar")] = bar;
    return persistUserConfig(nextRoot);
}

namespace {

QStringList validBarSections() {
    return {QStringLiteral("left"), QStringLiteral("centerBegin"), QStringLiteral("center"),
            QStringLiteral("centerEnd"), QStringLiteral("tray"), QStringLiteral("right")};
}

int indexOfWidgetInSection(const QJsonArray &arr, const QString &id) {
    for (int i = 0; i < arr.size(); ++i) {
        if (arr.at(i).toObject().value(QStringLiteral("id")).toString() == id)
            return i;
    }
    return -1;
}

} // namespace

QString ShellConfig::putBarWidget(const QString &id, const QString &placementJson) {
    QJsonParseError err;
    const QJsonDocument pdoc = QJsonDocument::fromJson(placementJson.trimmed().isEmpty() ? QByteArray("{}") : placementJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !pdoc.isObject())
        return QStringLiteral("invalid placement: ") + err.errorString();

    const QJsonObject p = pdoc.object();
    QJsonObject nextRoot = m_effective;
    QJsonObject bar = nextRoot.value(QStringLiteral("bar")).toObject();
    QJsonObject layout = bar.value(QStringLiteral("layout")).toObject();

    // Check if already in bar (except spacer which allows multiple)
    if (id != QLatin1String("quattro.spacer")) {
        for (const QString &sec : validBarSections()) {
            if (indexOfWidgetInSection(layout.value(sec).toArray(), id) >= 0)
                return QStringLiteral("ok");
        }
    }

    QString targetSection = p.value(QStringLiteral("section")).toString(QStringLiteral("right"));
    if (!validBarSections().contains(targetSection))
        targetSection = QStringLiteral("right");
    if (id == QLatin1String("quattro.tray") && targetSection == QLatin1String("tray"))
        return QStringLiteral("the tray cannot go in its own drawer");

    QJsonArray arr = layout.value(targetSection).toArray();
    int insertIdx = arr.size();

    if (p.contains(QStringLiteral("before"))) {
        const int anchorIdx = indexOfWidgetInSection(arr, p.value(QStringLiteral("before")).toString());
        if (anchorIdx >= 0) insertIdx = anchorIdx;
    } else if (p.contains(QStringLiteral("after"))) {
        const int anchorIdx = indexOfWidgetInSection(arr, p.value(QStringLiteral("after")).toString());
        if (anchorIdx >= 0) insertIdx = anchorIdx + 1;
    } else if (p.contains(QStringLiteral("index"))) {
        insertIdx = std::clamp(p.value(QStringLiteral("index")).toInt(arr.size()), 0, static_cast<int>(arr.size()));
    }

    arr.insert(insertIdx, QJsonObject{{QStringLiteral("id"), id}});
    layout[targetSection] = arr;
    bar[QStringLiteral("layout")] = layout;
    nextRoot[QStringLiteral("bar")] = bar;
    return persistUserConfig(nextRoot) ? QStringLiteral("ok") : QStringLiteral("error");
}

QString ShellConfig::moveBarWidget(const QString &id, const QString &placementJson) {
    QJsonParseError err;
    const QJsonDocument pdoc = QJsonDocument::fromJson(placementJson.trimmed().isEmpty() ? QByteArray("{}") : placementJson.toUtf8(), &err);
    if (err.error != QJsonParseError::NoError || !pdoc.isObject())
        return QStringLiteral("invalid placement: ") + err.errorString();

    const QJsonObject p = pdoc.object();
    QJsonObject nextRoot = m_effective;
    QJsonObject bar = nextRoot.value(QStringLiteral("bar")).toObject();
    QJsonObject layout = bar.value(QStringLiteral("layout")).toObject();

    QString fromSection = p.value(QStringLiteral("fromSection")).toString();
    int fromIndex = p.contains(QStringLiteral("fromIndex")) ? p.value(QStringLiteral("fromIndex")).toInt(-1) : -1;

    if (fromSection.isEmpty() || fromIndex < 0) {
        for (const QString &sec : validBarSections()) {
            const int idx = indexOfWidgetInSection(layout.value(sec).toArray(), id);
            if (idx >= 0) {
                fromSection = sec;
                fromIndex = idx;
                break;
            }
        }
    }
    if (fromSection.isEmpty() || fromIndex < 0)
        return QStringLiteral("not in bar");

    QJsonArray srcArr = layout.value(fromSection).toArray();
    if (fromIndex >= srcArr.size())
        return QStringLiteral("invalid source index");

    const QJsonValue entry = srcArr.at(fromIndex);
    srcArr.removeAt(fromIndex);
    layout[fromSection] = srcArr;

    QString toSection = p.value(QStringLiteral("section")).toString(fromSection);
    if (!validBarSections().contains(toSection))
        toSection = fromSection;
    if (id == QLatin1String("quattro.tray") && toSection == QLatin1String("tray"))
        return QStringLiteral("the tray cannot go in its own drawer");

    QJsonArray dstArr = layout.value(toSection).toArray();
    int dstIdx = dstArr.size();
    if (p.contains(QStringLiteral("before"))) {
        const int a = indexOfWidgetInSection(dstArr, p.value(QStringLiteral("before")).toString());
        if (a >= 0) dstIdx = a;
    } else if (p.contains(QStringLiteral("after"))) {
        const int a = indexOfWidgetInSection(dstArr, p.value(QStringLiteral("after")).toString());
        if (a >= 0) dstIdx = a + 1;
    } else if (p.contains(QStringLiteral("index"))) {
        dstIdx = std::clamp(p.value(QStringLiteral("index")).toInt(dstArr.size()), 0, static_cast<int>(dstArr.size()));
    }

    dstArr.insert(dstIdx, entry);
    layout[toSection] = dstArr;
    bar[QStringLiteral("layout")] = layout;
    nextRoot[QStringLiteral("bar")] = bar;
    return persistUserConfig(nextRoot) ? QStringLiteral("ok") : QStringLiteral("error");
}

QString ShellConfig::setBarWidget(const QString &id, const QString &key, const QString &valueJson, const QString &selectorJson) {
    QJsonParseError err;
    const QByteArray wrapped = QByteArray("{\"v\":") + valueJson.toUtf8() + QByteArray("}");
    const QJsonDocument vdoc = QJsonDocument::fromJson(wrapped, &err);
    if (err.error != QJsonParseError::NoError)
        return QStringLiteral("invalid value");
    const QJsonValue val = vdoc.object().value(QStringLiteral("v"));

    const QJsonObject sel = QJsonDocument::fromJson(selectorJson.trimmed().isEmpty() ? QByteArray("{}") : selectorJson.toUtf8()).object();
    const QString selSec = sel.value(QStringLiteral("section")).toString();
    const int selIdx = sel.contains(QStringLiteral("index")) ? sel.value(QStringLiteral("index")).toInt(-1) : -1;

    if (selSec.isEmpty() && selIdx < 0)
        return setBarWidgetSetting(id, key, val) ? QStringLiteral("ok") : QStringLiteral("not in bar");

    QJsonObject nextRoot = m_effective;
    QJsonObject bar = nextRoot.value(QStringLiteral("bar")).toObject();
    QJsonObject layout = bar.value(QStringLiteral("layout")).toObject();
    QJsonArray arr = layout.value(selSec).toArray();
    if (selIdx < 0 || selIdx >= arr.size())
        return QStringLiteral("not in bar");
    QJsonObject obj = arr.at(selIdx).toObject();
    if (obj.value(QStringLiteral("id")).toString() != id)
        return QStringLiteral("not in bar");
    obj[key] = val;
    arr[selIdx] = obj;
    layout[selSec] = arr;
    bar[QStringLiteral("layout")] = layout;
    nextRoot[QStringLiteral("bar")] = bar;
    return persistUserConfig(nextRoot) ? QStringLiteral("ok") : QStringLiteral("error");
}

QString ShellConfig::setPluginEnabled(const QString &id, bool enabled, const QString &placementJson) {
    if (!enabled) {
        QJsonObject nextRoot = m_effective;
        QJsonObject bar = nextRoot.value(QStringLiteral("bar")).toObject();
        QJsonObject layout = bar.value(QStringLiteral("layout")).toObject();
        bool changed = false;
        for (const QString &sec : validBarSections()) {
            QJsonArray arr = layout.value(sec).toArray();
            for (int i = arr.size() - 1; i >= 0; --i) {
                if (arr.at(i).toObject().value(QStringLiteral("id")).toString() == id) {
                    arr.removeAt(i);
                    changed = true;
                }
            }
            layout[sec] = arr;
        }
        if (changed) {
            bar[QStringLiteral("layout")] = layout;
            nextRoot[QStringLiteral("bar")] = bar;
            persistUserConfig(nextRoot);
        }
        return QStringLiteral("ok");
    }
    return putBarWidget(id, placementJson);
}

QString ShellConfig::listPluginsJson() const {
    struct Meta { const char *id; const char *name; const char *kinds[3]; };
    static const Meta catalog[] = {
        {"quattro.active-window", "Active Window", {"bar-widget", nullptr, nullptr}},
        {"quattro.audio", "Audio", {"bar-widget", nullptr, nullptr}},
        {"quattro.background", "Background", {"service", nullptr, nullptr}},
        {"quattro.bar", "Bar", {"bar", nullptr, nullptr}},
        {"quattro.battery", "Battery", {"service", nullptr, nullptr}},
        {"quattro.bluetooth", "Bluetooth", {"bar-widget", nullptr, nullptr}},
        {"quattro.clipboard", "Clipboard", {"overlay", nullptr, nullptr}},
        {"quattro.clock", "Clock", {"bar-widget", nullptr, nullptr}},
        {"quattro.dev-gallery", "Dev gallery", {"panel", nullptr, nullptr}},
        {"quattro.disk-speedtest", "Disk Speed Test", {"panel", nullptr, nullptr}},
        {"quattro.dock", "Dock", {"service", nullptr, nullptr}},
        {"quattro.dropbox", "Dropbox", {"bar-widget", nullptr, nullptr}},
        {"quattro.elsewhen", "Elsewhen", {"bar-widget", nullptr, nullptr}},
        {"quattro.emojis", "Emojis", {"overlay", nullptr, nullptr}},
        {"quattro.idle", "Idle", {"service", nullptr, nullptr}},
        {"quattro.image-picker", "Image picker", {"overlay", nullptr, nullptr}},
        {"quattro.indicators", "Indicators", {"bar-widget", nullptr, nullptr}},
        {"quattro.keyboard-layout", "Keyboard Layout", {"bar-widget", nullptr, nullptr}},
        {"quattro.lock", "Lock Screen", {"service", nullptr, nullptr}},
        {"quattro.media", "Media", {"service", "bar-widget", nullptr}},
        {"quattro.menu", "Quattro menu", {"menu", "bar-widget", nullptr}},
        {"quattro.microphone", "Microphone", {"bar-widget", nullptr, nullptr}},
        {"quattro.monitor", "Monitor", {"bar-widget", nullptr, nullptr}},
        {"quattro.network", "Network", {"bar-widget", nullptr, nullptr}},
        {"quattro.nightlight", "Night Light", {"service", nullptr, nullptr}},
        {"quattro.notifications", "Notifications", {"service", nullptr, nullptr}},
        {"quattro.osd", "On-screen display", {"panel", nullptr, nullptr}},
        {"quattro.polkit", "Polkit Agent", {"service", nullptr, nullptr}},
        {"quattro.power", "Power", {"bar-widget", nullptr, nullptr}},
        {"quattro.reminders", "Reminders", {"overlay", nullptr, nullptr}},
        {"quattro.remote-session", "Remote Session", {"service", nullptr, nullptr}},
        {"quattro.spacer", "Spacer", {"bar-widget", nullptr, nullptr}},
        {"quattro.speedtest", "Speed Test", {"panel", nullptr, nullptr}},
        {"quattro.system-update", "System Update", {"bar-widget", nullptr, nullptr}},
        {"quattro.tailscale", "Tailscale", {"bar-widget", nullptr, nullptr}},
        {"quattro.tray", "Tray", {"bar-widget", nullptr, nullptr}},
        {"quattro.weather", "Weather", {"bar-widget", nullptr, nullptr}},
        {"quattro.wifiqr", "Wi-Fi QR", {"panel", nullptr, nullptr}},
        {"quattro.workspaces", "Workspaces", {"bar-widget", nullptr, nullptr}},
    };

    const BarLayout lay = barLayout();
    const QVector<BarWidgetEntry> allBar = lay.left + lay.centerBegin + lay.center + lay.centerEnd + lay.tray + lay.right;
    QSet<QString> inBar;
    for (const auto &e : allBar) inBar.insert(e.id);

    QJsonArray out;
    for (const Meta &m : catalog) {
        QJsonArray kinds;
        bool barWidgetOnly = true;
        for (const char *k : m.kinds) {
            if (k) {
                kinds.append(QString::fromLatin1(k));
                if (QString::fromLatin1(k) != QLatin1String("bar-widget"))
                    barWidgetOnly = false;
            }
        }
        const QString id = QString::fromLatin1(m.id);
        const bool enabled = barWidgetOnly ? inBar.contains(id) : true;
        out.append(QJsonObject{
            {QStringLiteral("id"), id},
            {QStringLiteral("name"), QString::fromLatin1(m.name)},
            {QStringLiteral("kinds"), kinds},
            {QStringLiteral("enabled"), enabled},
            {QStringLiteral("active"), enabled},
            {QStringLiteral("canDisable"), barWidgetOnly},
            {QStringLiteral("firstParty"), true},
            {QStringLiteral("clonedFrom"), QJsonValue::Null}
        });
    }
    return QString::fromUtf8(QJsonDocument(out).toJson(QJsonDocument::Compact));
}

void ShellConfig::syncBarHidden() {
    const bool next = QFileInfo::exists(m_barOffFlagPath);
    if (next != m_barHidden) {
        m_barHidden = next;
        emit barHiddenChanged(next);
    }
}

QByteArray ShellConfig::stripJsonc(const QByteArray &input) {
    QByteArray out;
    out.reserve(input.size());
    const int n = input.size();
    bool inString = false;
    bool escape = false;

    for (int i = 0; i < n; ++i) {
        const char c = input[i];
        if (inString) {
            out.append(c);
            if (escape) {
                escape = false;
            } else if (c == '\\') {
                escape = true;
            } else if (c == '"') {
                inString = false;
            }
            continue;
        }
        if (c == '"') {
            inString = true;
            out.append(c);
            continue;
        }
        if (c == '/' && i + 1 < n && input[i + 1] == '/') {
            i += 2;
            while (i < n && input[i] != '\n') ++i;
            if (i < n) out.append('\n');
            continue;
        }
        if (c == '/' && i + 1 < n && input[i + 1] == '*') {
            i += 2;
            while (i + 1 < n && !(input[i] == '*' && input[i + 1] == '/')) {
                if (input[i] == '\n') out.append('\n');
                ++i;
            }
            ++i;
            continue;
        }
        out.append(c);
    }

    // Remove trailing commas before } or ]
    QByteArray clean;
    clean.reserve(out.size());
    inString = false;
    escape = false;
    for (int i = 0; i < out.size(); ++i) {
        const char c = out[i];
        if (inString) {
            clean.append(c);
            if (escape) escape = false;
            else if (c == '\\') escape = true;
            else if (c == '"') inString = false;
            continue;
        }
        if (c == '"') {
            inString = true;
            clean.append(c);
            continue;
        }
        if (c == ',') {
            int j = i + 1;
            while (j < out.size() && (out[j] == ' ' || out[j] == '\t' || out[j] == '\r' || out[j] == '\n'))
                ++j;
            if (j < out.size() && (out[j] == '}' || out[j] == ']'))
                continue;
        }
        clean.append(c);
    }
    return clean;
}

} // namespace quattro
