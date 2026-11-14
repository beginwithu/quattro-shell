#include "core/DesktopEntryStore.h"
#include "core/HyprlandClient.h"
#include "core/IpcServer.h"
#include "core/ShellConfig.h"
#include "core/ThemeManager.h"
#include "services/AudioService.h"
#include "services/BatteryService.h"
#include "services/BrightnessService.h"
#include "services/ClipboardService.h"
#include "services/MediaService.h"
#include "services/NightlightService.h"
#include "services/NotificationDaemon.h"
#include "services/PolkitService.h"
#include "services/RemoteSessionService.h"
#include "services/SystemStatsService.h"
#include "services/TrayWatcher.h"
#include "ui/BackgroundWindow.h"
#include "ui/BarWindow.h"
#include "ui/ClipboardOverlay.h"
#include "ui/DevGalleryWindow.h"
#include "ui/DockWindow.h"
#include "ui/EmojiOverlay.h"
#include "ui/ImagePickerWindow.h"
#include "ui/LockWindow.h"
#include "ui/MenuWindow.h"
#include "ui/OsdWindow.h"
#include "ui/PolkitWindow.h"
#include "ui/ReminderOverlay.h"
#include "ui/SpeedTestOverlay.h"
#include "ui/ToastOverlay.h"
#include "ui/WifiQrOverlay.h"
#include "ui/panels/AudioBluetoothPanels.h"
#include "ui/panels/ClockPowerPanels.h"
#include "ui/panels/DropboxTailscaleMediaPanels.h"
#include "ui/panels/MonitorNetworkPanels.h"
#include "ui/panels/WeatherElsewhenPanels.h"
#include "wayland/GlobalShortcutManager.h"
#include "wayland/IdleMonitor.h"
#include "wayland/LayerShellIntegration.h"
#include "wayland/SessionLockManager.h"

#include <QApplication>
#include <QGuiApplication>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>

using namespace quattro;

namespace {

bool parseBoolArg(const QString &raw, bool fallback = true) {
    const QString v = raw.trimmed().toLower();
    if (v == QLatin1String("true") || v == QLatin1String("1") || v == QLatin1String("on") || v == QLatin1String("yes"))
        return true;
    if (v == QLatin1String("false") || v == QLatin1String("0") || v == QLatin1String("off") || v == QLatin1String("no"))
        return false;
    return fallback;
}

} // namespace

int main(int argc, char *argv[]) {
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY"))
        qputenv("QT_QPA_PLATFORM", "wayland");

    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("quattro-cpp"));
    app.setQuitOnLastWindowClosed(false);

    LayerShellIntegration::instance().ensureInitialized();
    SessionLockManager::instance().ensureInitialized();

    ThemeManager theme;
    ShellConfig config;
    HyprlandClient hypr;
    DesktopEntryStore desktopEntries(config.quattroPath());

    auto syncThemeFromHypr = [&]() {
        theme.setHyprlandMetrics(hypr.rounding(), hypr.gapsOut(), !hypr.animationsEnabled());
    };
    QObject::connect(&hypr, &HyprlandClient::optionsChanged, &theme, syncThemeFromHypr);
    syncThemeFromHypr();

    // Core services
    AudioService audio;
    SystemStatsService stats;
    NotificationDaemon notif;
    notif.registerDbus();

    TrayWatcher tray;
    tray.start();

    BatteryService battery(&stats);
    MediaService media(&audio);
    NightlightService nightlight;
    RemoteSessionService remoteSession(&hypr);
    BrightnessService brightness(&hypr);
    ClipboardService clipboard;
    clipboard.startWatchers();

    PolkitService polkitService;
    polkitService.start();

    // Shell windows & overlays
    BackgroundManager background(&theme);
    OsdWindow osd(&theme);
    MenuWindow menu(&theme, &config);
    LockController lock(&theme, &background);
    ToastOverlay toasts(&theme, &config, &notif);
    PolkitWindow polkitWin(&polkitService, &theme);
    ImagePickerWindow imagePicker(&theme, &config);
    DockManager dock(&hypr, &desktopEntries, &config, &theme);
    dock.setEnabled(true);

    ClipboardOverlay clipboardOverlay(&clipboard, &theme, &config);
    EmojiOverlay emojiOverlay(&theme, &config);
    ReminderOverlay reminderOverlay(&theme, &config);
    SpeedTestOverlay speedTestOverlay(SpeedTestOverlay::Mode::Network, &theme, &config);
    SpeedTestOverlay diskSpeedTestOverlay(SpeedTestOverlay::Mode::Disk, &theme, &config);
    WifiQrOverlay wifiQrOverlay(&theme, &config);
    DevGalleryWindow devGallery(&theme, &config);

    // Panels
    AudioPanel audioPanel(&audio, &theme, &config);
    BluetoothPanel bluetoothPanel(&theme, &config);
    ClockPanel clockPanel(&theme, &config);
    PowerPanel powerPanel(&battery, &stats, &theme, &config);
    MonitorPanel monitorPanel(&hypr, &brightness, &theme, &config);
    NetworkPanel networkPanel(&theme, &config);
    WeatherPanel weatherPanel(&theme, &config);
    ElsewhenPanel elsewhenPanel(&theme, &config);
    DropboxPanel dropboxPanel(&theme, &config);
    TailscalePanel tailscalePanel(&theme, &config);
    MediaPanel mediaPanel(&media, &theme, &config);

    QObject::connect(&networkPanel, &NetworkPanel::openWifiQrRequested, &wifiQrOverlay, [&]() {
        wifiQrOverlay.openAt();
    });
    QObject::connect(&networkPanel, &NetworkPanel::openSpeedTestRequested, &speedTestOverlay, [&]() {
        speedTestOverlay.openAt();
    });

    BarManager bars(&theme, &config, &hypr, &audio, &stats, &notif, &tray, &menu);
    bars.setExtraServices({&media, &nightlight, &remoteSession, &dock});

    // Uniform popup/panel registry so summon/hide/toggle handle all panels identically
    const QHash<QString, ShellPopupWindow *> popupMap = {
        {QStringLiteral("quattro.audio"), &audioPanel},
        {QStringLiteral("audio"), &audioPanel},
        {QStringLiteral("quattro.bluetooth"), &bluetoothPanel},
        {QStringLiteral("bluetooth"), &bluetoothPanel},
        {QStringLiteral("quattro.clock"), &clockPanel},
        {QStringLiteral("clock"), &clockPanel},
        {QStringLiteral("quattro.power"), &powerPanel},
        {QStringLiteral("power"), &powerPanel},
        {QStringLiteral("quattro.monitor"), &monitorPanel},
        {QStringLiteral("monitor"), &monitorPanel},
        {QStringLiteral("quattro.network"), &networkPanel},
        {QStringLiteral("network"), &networkPanel},
        {QStringLiteral("quattro.weather"), &weatherPanel},
        {QStringLiteral("weather"), &weatherPanel},
        {QStringLiteral("quattro.elsewhen"), &elsewhenPanel},
        {QStringLiteral("elsewhen"), &elsewhenPanel},
        {QStringLiteral("quattro.dropbox"), &dropboxPanel},
        {QStringLiteral("dropbox"), &dropboxPanel},
        {QStringLiteral("quattro.tailscale"), &tailscalePanel},
        {QStringLiteral("tailscale"), &tailscalePanel},
        {QStringLiteral("quattro.media"), &mediaPanel},
        {QStringLiteral("media"), &mediaPanel},
        {QStringLiteral("quattro.clipboard"), &clipboardOverlay},
        {QStringLiteral("clipboard"), &clipboardOverlay},
        {QStringLiteral("quattro.emojis"), &emojiOverlay},
        {QStringLiteral("emojis"), &emojiOverlay},
        {QStringLiteral("quattro.reminders"), &reminderOverlay},
        {QStringLiteral("reminders"), &reminderOverlay},
        {QStringLiteral("quattro.speedtest"), &speedTestOverlay},
        {QStringLiteral("speedtest"), &speedTestOverlay},
        {QStringLiteral("quattro.disk-speedtest"), &diskSpeedTestOverlay},
        {QStringLiteral("disk-speedtest"), &diskSpeedTestOverlay},
        {QStringLiteral("quattro.wifiqr"), &wifiQrOverlay},
        {QStringLiteral("wifiqr"), &wifiQrOverlay},
        {QStringLiteral("quattro.dev-gallery"), &devGallery},
        {QStringLiteral("dev-gallery"), &devGallery},
    };

    auto closeOtherPopups = [&](ShellPopupWindow *keep) {
        for (ShellPopupWindow *w : popupMap) {
            if (w && w != keep && w->isOpened())
                w->closePopup();
        }
    };

    auto summonPlugin = [&](const QString &id, const QString &payload, const QPoint &anchor = QPoint(), QScreen *screen = nullptr) -> QString {
        if (id == QLatin1String("quattro.menu") || id == QLatin1String("menu")) {
            menu.openJson(payload);
            return QStringLiteral("ok");
        }
        if (id == QLatin1String("quattro.osd") || id == QLatin1String("osd")) {
            osd.openJson(payload);
            return QStringLiteral("ok");
        }
        if (id == QLatin1String("quattro.lock") || id == QLatin1String("lock")) {
            return lock.requestLock();
        }
        if (id == QLatin1String("quattro.image-picker") || id == QLatin1String("image-picker")) {
            closeOtherPopups(&imagePicker);
            imagePicker.openPayload(payload);
            return QStringLiteral("ok");
        }
        if (id == QLatin1String("quattro.dock") || id == QLatin1String("dock")) {
            dock.setEnabled(true);
            return QStringLiteral("ok");
        }
        if (ShellPopupWindow *w = popupMap.value(id, nullptr)) {
            closeOtherPopups(w);
            w->openAt(anchor, screen);
            return QStringLiteral("ok");
        }
        return QStringLiteral("unknown");
    };

    auto hidePlugin = [&](const QString &id) {
        if (id == QLatin1String("quattro.menu") || id == QLatin1String("menu")) { menu.closeMenu(); return; }
        if (id == QLatin1String("quattro.osd") || id == QLatin1String("osd")) { osd.closeOsd(); return; }
        if (id == QLatin1String("quattro.image-picker") || id == QLatin1String("image-picker")) { imagePicker.cancel(); return; }
        if (id == QLatin1String("quattro.dock") || id == QLatin1String("dock")) { dock.setEnabled(false); return; }
        if (ShellPopupWindow *w = popupMap.value(id, nullptr))
            w->closePopup();
    };

    auto togglePlugin = [&](const QString &id, const QString &payload, const QPoint &anchor = QPoint(), QScreen *screen = nullptr) {
        if (id == QLatin1String("quattro.menu") || id == QLatin1String("menu")) {
            menu.toggleJson(payload);
            return;
        }
        if (id == QLatin1String("quattro.dock") || id == QLatin1String("dock")) {
            dock.toggleVisibility();
            return;
        }
        if (id == QLatin1String("quattro.image-picker") || id == QLatin1String("image-picker")) {
            if (imagePicker.isOpened()) imagePicker.cancel();
            else summonPlugin(id, payload, anchor, screen);
            return;
        }
        if (ShellPopupWindow *w = popupMap.value(id, nullptr)) {
            if (w->isOpened()) w->closePopup();
            else {
                closeOtherPopups(w);
                w->openAt(anchor, screen);
            }
        }
    };

    QObject::connect(&bars, &BarManager::panelToggleRequested, &bars,
                     [&](const QString &widgetId, const QPoint &globalPos, QScreen *screen) {
        togglePlugin(widgetId, QStringLiteral("{}"), globalPos, screen);
    });

    // OSD wiring
    QObject::connect(&audio, &AudioService::volumeOsdRequested, &osd, [&](int pct, bool muted) {
        osd.showOsd(muted ? QStringLiteral("volume-muted") : QString(),
                    QString(), pct, 100, true, QString(), 1200);
    });
    QObject::connect(&media, &MediaService::osdRequested, &osd,
                     [&](const QString &icon, const QString &msg, int val, const QString &progressText) {
        osd.showOsd(icon, msg, val, 100, val >= 0, progressText, 1400);
    });
    QObject::connect(&brightness, &BrightnessService::osdRequested, &osd,
                     [&](const QString &icon, const QString &msg, int val, const QString &progressText) {
        osd.showOsd(icon, msg, val, 100, true, progressText, 1200);
    });

    // Idle wiring
    IdleMonitor idle;
    auto syncIdle = [&]() {
        if (stats.stayAwake())
            idle.setTimeouts(0, 0);
        else
            idle.setTimeouts(config.idleScreensaverSec(), config.idleLockSec());
    };
    QObject::connect(&config, &ShellConfig::configChanged, &idle, syncIdle);
    QObject::connect(&stats, &SystemStatsService::indicatorsChanged, &idle, syncIdle);
    QObject::connect(&idle, &IdleMonitor::lockTriggered, &lock, [&]() {
        if (!lock.isLocked() && !stats.stayAwake())
            lock.requestLock();
    });
    QObject::connect(&idle, &IdleMonitor::screensaverTriggered, &idle, [&]() {
        if (!lock.isLocked() && !stats.stayAwake())
            QProcess::startDetached(QStringLiteral("quattro-launch-screensaver"), {});
    });
    syncIdle();

    // IPC server (identical Unix socket hash to Quickshell CLI bin/quattro-shell)
    IpcServer ipc;

    ipc.registerTarget(QStringLiteral("shell"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("ping"))
            return {true, QStringLiteral("ok")};
        if (method == QLatin1String("reloadConfig")) {
            config.reloadUserConfig();
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("rescanPlugins") || method == QLatin1String("reloadPlugins")) {
            desktopEntries.scan();
            menu.refreshMenuData();
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("toggleBarTransparency")) {
            return {true, config.cycleBarBackground()};
        }
        if (method == QLatin1String("cycleInput")) {
            QProcess::startDetached(QStringLiteral("hyprctl"), {QStringLiteral("switchxkblayout"), QStringLiteral("all"), QStringLiteral("next")});
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("applyTheme") && args.size() >= 2) {
            theme.applyThemeBase64(args[0].toUtf8(), args[1].toUtf8());
            hypr.refreshOptions();
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("switchBackground") && !args.isEmpty()) {
            background.setBackground(args[0], false);
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("prepareThemeIntro")) {
            return {true, background.prepareThemeIntro(args.isEmpty() ? QString() : args[0])};
        }
        if (method == QLatin1String("playThemeIntro")) {
            const int dur = args.size() >= 2 ? args[1].toInt() : 650;
            return {true, background.playThemeIntro(args.isEmpty() ? QString() : args[0], dur > 0 ? dur : 650)};
        }
        if (method == QLatin1String("selectedBackground")) {
            return {true, background.currentBackground()};
        }
        if (method == QLatin1String("listShellConfig")) {
            return {true, QString::fromUtf8(QJsonDocument(config.rootObject()).toJson(QJsonDocument::Compact))};
        }
        if (method == QLatin1String("debugBarGeometry")) {
            return {true, bars.debugGeometryJson()};
        }
        if (method == QLatin1String("putBarWidget") && !args.isEmpty()) {
            return {true, config.putBarWidget(args[0], args.size() >= 2 ? args[1] : QStringLiteral("{}"))};
        }
        if (method == QLatin1String("moveBarWidget") && !args.isEmpty()) {
            return {true, config.moveBarWidget(args[0], args.size() >= 2 ? args[1] : QStringLiteral("{}"))};
        }
        if (method == QLatin1String("setBarWidget") && args.size() >= 3) {
            return {true, config.setBarWidget(args[0], args[1], args[2], args.size() >= 4 ? args[3] : QString())};
        }
        if (method == QLatin1String("enablePlugin") && !args.isEmpty()) {
            if (args[0] == QLatin1String("quattro.dock")) dock.setEnabled(true);
            return {true, config.setPluginEnabled(args[0], true, args.size() >= 2 ? args[1] : QString())};
        }
        if (method == QLatin1String("setPluginEnabled") && args.size() >= 2) {
            const bool en = parseBoolArg(args[1], true);
            if (args[0] == QLatin1String("quattro.dock")) dock.setEnabled(en);
            return {true, config.setPluginEnabled(args[0], en, args.size() >= 3 ? args[2] : QString())};
        }
        if (method == QLatin1String("listPlugins")) {
            return {true, config.listPluginsJson()};
        }
        if (method == QLatin1String("summon") && !args.isEmpty()) {
            const QString payload = args.size() >= 2 ? args[1] : QStringLiteral("{}");
            return {true, summonPlugin(args[0], payload)};
        }
        if (method == QLatin1String("hide") && !args.isEmpty()) {
            hidePlugin(args[0]);
            return {true, QStringLiteral("ok")};
        }
        if ((method == QLatin1String("toggle") || method == QLatin1String("togglePanelAt")) && !args.isEmpty()) {
            const QString payload = args.size() >= 2 ? args[1] : QStringLiteral("{}");
            QPoint anchor;
            if (method == QLatin1String("togglePanelAt") && args.size() >= 3)
                anchor = QPoint(args[1].toInt(), args[2].toInt());
            togglePlugin(args[0], payload, anchor, qGuiApp->screenAt(anchor));
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("call") && args.size() >= 2) {
            return ipc.dispatch(args[0], args[1], args.mid(2));
        }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("image-selector"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("open")) {
            const QString dirs = args.value(0);
            const QString rowsB64 = args.value(1);
            const QString selected = args.value(2);
            const QString selFile = args.value(3);
            const QString doneFile = args.value(4);
            const bool showLabels = parseBoolArg(args.value(5), false);
            const bool filterable = parseBoolArg(args.value(6), false);
            return {true, imagePicker.openSelector(dirs, rowsB64, selected, selFile, doneFile, showLabels, filterable)};
        }
        if (method == QLatin1String("preload")) {
            return {true, imagePicker.preloadRows(args.value(0), args.value(1),
                                                  parseBoolArg(args.value(2), false),
                                                  parseBoolArg(args.value(3), false))};
        }
        if (method == QLatin1String("cancel")) {
            imagePicker.cancel();
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("openThemes")) {
            imagePicker.openThemes();
            return {true, QStringLiteral("ok")};
        }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("quattro.dock"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("toggleVisibility") || method == QLatin1String("toggle")) {
            dock.toggleVisibility();
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("setEnabled") && !args.isEmpty()) {
            dock.setEnabled(parseBoolArg(args[0], true));
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("appGrid") || method == QLatin1String("showAppGrid")) {
            dock.showAppGrid();
            return {true, QStringLiteral("ok")};
        }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("media"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("playPause") || method == QLatin1String("play-pause"))
            return {true, media.runAction(QStringLiteral("play-pause"), true, args.value(0)) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("play"))
            return {true, media.runAction(QStringLiteral("play"), true, args.value(0)) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("pause"))
            return {true, media.runAction(QStringLiteral("pause"), true, args.value(0)) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("next"))
            return {true, media.runAction(QStringLiteral("next"), true, args.value(0)) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("previous") || method == QLatin1String("prev"))
            return {true, media.runAction(QStringLiteral("prev"), true, args.value(0)) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("stop"))
            return {true, media.runAction(QStringLiteral("stop"), true, args.value(0)) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("switchSource"))
            return {true, media.switchSource(args.isEmpty() ? 1 : args[0].toInt(), false, true) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("status"))
            return {true, media.statusJson()};
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("nightlight"), [&](const QString &method, const QStringList &) -> IpcResult {
        if (method == QLatin1String("enable")) return {true, nightlight.enable()};
        if (method == QLatin1String("disable")) return {true, nightlight.disable()};
        if (method == QLatin1String("toggle")) return {true, nightlight.toggle()};
        if (method == QLatin1String("status")) return {true, nightlight.statusJson()};
        if (method == QLatin1String("refresh")) { nightlight.refresh(); return {true, QStringLiteral("ok")}; }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("battery"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("profile")) return {true, battery.activePowerProfile()};
        if (method == QLatin1String("setProfile") && !args.isEmpty()) {
            battery.setPowerProfile(args[0]);
            return {true, battery.activePowerProfile()};
        }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("osd"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("ping")) return {true, QStringLiteral("ok")};
        if (method == QLatin1String("show") && !args.isEmpty()) { osd.openJson(args[0]); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("close")) { osd.closeOsd(); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("state")) return {true, osd.isOpened() ? QStringLiteral("open") : QStringLiteral("closed")};
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("notifications"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("ping")) return {true, QStringLiteral("ok")};
        if (method == QLatin1String("isDnd")) return {true, notif.doNotDisturb() ? QStringLiteral("on") : QStringLiteral("off")};
        if (method == QLatin1String("toggleDnd")) return {true, notif.toggleDoNotDisturb() ? QStringLiteral("on") : QStringLiteral("off")};
        if (method == QLatin1String("setDnd") && !args.isEmpty()) {
            notif.setDoNotDisturb(parseBoolArg(args[0], false));
            return {true, notif.doNotDisturb() ? QStringLiteral("on") : QStringLiteral("off")};
        }
        if (method == QLatin1String("act")) return {true, notif.actOnNewest() ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("dismissOne")) return {true, notif.dismissNewest() ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("dismissAll")) { notif.dismissAll(); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("invokeLast")) return {true, notif.invokeNewestAction(0) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("invokeAction") && !args.isEmpty())
            return {true, notif.invokeNewestAction(args[0].toInt() - 1) ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("showHistory")) { notif.replayHistory(); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("dismiss") && !args.isEmpty())
            return {true, notif.dismissBySummary(args[0]) > 0 ? QStringLiteral("ok") : QStringLiteral("none")};
        if (method == QLatin1String("count")) return {true, QString::number(notif.count())};
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("lock"), [&](const QString &method, const QStringList &) -> IpcResult {
        if (method == QLatin1String("lock")) return {true, lock.requestLock()};
        if (method == QLatin1String("isLocked")) return {true, lock.isLocked() ? QStringLiteral("true") : QStringLiteral("false")};
        if (method == QLatin1String("status")) return {true, lock.statusJson()};
        if (method == QLatin1String("preview")) { lock.showPreview(); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("hidePreview")) { lock.hidePreview(); return {true, QStringLiteral("ok")}; }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("background"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("set") && !args.isEmpty()) {
            background.setBackground(args[0], true);
            return {true, QStringLiteral("ok")};
        }
        if (method == QLatin1String("refresh")) {
            background.refreshFromSymlink();
            return {true, QStringLiteral("ok")};
        }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("quattro.menu"), [&](const QString &method, const QStringList &args) -> IpcResult {
        if (method == QLatin1String("ping")) return {true, QStringLiteral("ok")};
        if (method == QLatin1String("refresh")) { menu.refreshMenuData(); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("open")) { menu.openJson(args.isEmpty() ? QStringLiteral("{}") : args[0]); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("close")) { menu.closeMenu(); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("toggle")) { menu.toggleJson(args.isEmpty() ? QStringLiteral("{}") : args[0]); return {true, QStringLiteral("ok")}; }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("quattro.bar"), [&](const QString &method, const QStringList &) -> IpcResult {
        if (method == QLatin1String("syncHidden")) { config.syncBarHidden(); return {true, QStringLiteral("ok")}; }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("quattro.indicators"), [&](const QString &method, const QStringList &) -> IpcResult {
        if (method == QLatin1String("refresh")) { stats.refreshIndicators(); return {true, QStringLiteral("ok")}; }
        return {false, {}};
    });

    ipc.registerTarget(QStringLiteral("quattro.system-update"), [&](const QString &method, const QStringList &) -> IpcResult {
        if (method == QLatin1String("refresh")) { stats.checkUpdates(); return {true, QStringLiteral("ok")}; }
        if (method == QLatin1String("clear")) { stats.clearUpdates(); return {true, QStringLiteral("ok")}; }
        return {false, {}};
    });

    // Register uniform toggle/open/close IPC handlers for every popup & panel target
    for (auto it = popupMap.constBegin(); it != popupMap.constEnd(); ++it) {
        const QString targetId = it.key();
        if (!targetId.startsWith(QStringLiteral("quattro.")))
            continue;
        ShellPopupWindow *win = it.value();
        ipc.registerTarget(targetId, [&, targetId, win](const QString &method, const QStringList &) -> IpcResult {
            if (method == QLatin1String("cycleFormat") && targetId == QLatin1String("quattro.clock")) {
                bars.cycleClockFormat();
                return {true, QStringLiteral("ok")};
            }
            if (method == QLatin1String("open")) {
                closeOtherPopups(win);
                win->openAt();
                return {true, QStringLiteral("ok")};
            }
            if (method == QLatin1String("close") || method == QLatin1String("hide")) {
                win->closePopup();
                return {true, QStringLiteral("ok")};
            }
            if (method == QLatin1String("toggle")) {
                togglePlugin(targetId, QStringLiteral("{}"));
                return {true, QStringLiteral("ok")};
            }
            return {false, {}};
        });
    }

    // Bind native Hyprland global shortcuts from default/quattro/shortcuts
    GlobalShortcutManager shortcuts(config.quattroPath());
    QObject::connect(&shortcuts, &GlobalShortcutManager::shortcutTriggered, &app,
                     [&](const QString &kind, const QString &target) {
        if (kind == QLatin1String("menu")) {
            if (target == QLatin1String("reminder-set")) {
                togglePlugin(QStringLiteral("quattro.reminders"), QStringLiteral("{}"));
            } else {
                menu.toggleJson(QStringLiteral("{\"menu\":\"%1\"}").arg(target));
            }
        } else if (kind == QLatin1String("panel")) {
            togglePlugin(target, QStringLiteral("{}"));
        } else if (kind == QLatin1String("audio")) {
            if (!media.handleVolumeKey(target)) {
                if (target == QLatin1String("raise")) audio.adjustVolume(5);
                else if (target == QLatin1String("lower")) audio.adjustVolume(-5);
                else if (target == QLatin1String("mute-toggle")) audio.toggleSinkMute();
            }
        } else if (kind == QLatin1String("brightness")) {
            brightness.handleKey(target);
        } else if (kind == QLatin1String("ipc")) {
            const int dot = target.indexOf(QLatin1Char('.'));
            if (dot > 0)
                ipc.dispatch(target.left(dot), target.mid(dot + 1), {});
        }
    });
    shortcuts.start();

    ipc.start(config.shellPath(), qEnvironmentVariable("WAYLAND_DISPLAY", QStringLiteral("wayland-0")));

    return app.exec();
}
