QT += core gui widgets dbus network svg multimedia concurrent waylandclient waylandclient-private
CONFIG += c++20 wayland-scanner link_pkgconfig warn_on no_include_pwd
PKGCONFIG += polkit-agent-1

TARGET = quattro-cpp
TEMPLATE = app
DESTDIR = $$PWD/build/bin
OBJECTS_DIR = obj
MOC_DIR = moc

INCLUDEPATH += src $$OUT_PWD

LIBS += -lwayland-client -lpam -lpulse -lqrencode

RESOURCES += assets.qrc

WAYLANDCLIENTSOURCES += \
    /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
    protocols/wlr-layer-shell-unstable-v1.xml \
    protocols/hyprland-global-shortcuts-v1.xml \
    /usr/share/wayland-protocols/staging/ext-session-lock/ext-session-lock-v1.xml \
    /usr/share/wayland-protocols/staging/ext-idle-notify/ext-idle-notify-v1.xml

HEADERS += \
    src/core/DesktopEntryStore.h \
    src/core/HyprlandClient.h \
    src/core/IpcServer.h \
    src/core/ShellConfig.h \
    src/core/ThemeManager.h \
    src/services/AudioService.h \
    src/services/BatteryService.h \
    src/services/BrightnessService.h \
    src/services/ClipboardService.h \
    src/services/FprintdService.h \
    src/services/MediaService.h \
    src/services/NightlightService.h \
    src/services/NotificationDaemon.h \
    src/services/PamAuth.h \
    src/services/PolkitService.h \
    src/services/RemoteSessionService.h \
    src/services/SystemStatsService.h \
    src/services/TrayWatcher.h \
    src/ui/BackgroundWindow.h \
    src/ui/BarWindow.h \
    src/ui/BorderPainter.h \
    src/ui/ClipboardOverlay.h \
    src/ui/CommonUi.h \
    src/ui/DevGalleryWindow.h \
    src/ui/DockWindow.h \
    src/ui/EmojiOverlay.h \
    src/ui/ImagePickerWindow.h \
    src/ui/LockWindow.h \
    src/ui/MenuWindow.h \
    src/ui/OsdWindow.h \
    src/ui/PolkitWindow.h \
    src/ui/ReminderOverlay.h \
    src/ui/SpeedTestOverlay.h \
    src/ui/ToastOverlay.h \
    src/ui/WifiQrOverlay.h \
    src/ui/panels/AudioBluetoothPanels.h \
    src/ui/panels/ClockPowerPanels.h \
    src/ui/panels/DropboxTailscaleMediaPanels.h \
    src/ui/panels/MonitorNetworkPanels.h \
    src/ui/panels/WeatherElsewhenPanels.h \
    src/wayland/GlobalShortcutManager.h \
    src/wayland/IdleMonitor.h \
    src/wayland/LayerShellIntegration.h \
    src/wayland/SessionLockManager.h

SOURCES += \
    src/main.cpp \
    src/core/DesktopEntryStore.cpp \
    src/core/HyprlandClient.cpp \
    src/core/IpcServer.cpp \
    src/core/ShellConfig.cpp \
    src/core/ThemeManager.cpp \
    src/services/AudioService.cpp \
    src/services/BatteryService.cpp \
    src/services/BrightnessService.cpp \
    src/services/ClipboardService.cpp \
    src/services/FprintdService.cpp \
    src/services/MediaService.cpp \
    src/services/NightlightService.cpp \
    src/services/NotificationDaemon.cpp \
    src/services/PamAuth.cpp \
    src/services/PolkitService.cpp \
    src/services/RemoteSessionService.cpp \
    src/services/SystemStatsService.cpp \
    src/services/TrayWatcher.cpp \
    src/ui/BackgroundWindow.cpp \
    src/ui/BarWindow.cpp \
    src/ui/BorderPainter.cpp \
    src/ui/ClipboardOverlay.cpp \
    src/ui/CommonUi.cpp \
    src/ui/DevGalleryWindow.cpp \
    src/ui/DockWindow.cpp \
    src/ui/EmojiOverlay.cpp \
    src/ui/ImagePickerWindow.cpp \
    src/ui/LockWindow.cpp \
    src/ui/MenuWindow.cpp \
    src/ui/OsdWindow.cpp \
    src/ui/PolkitWindow.cpp \
    src/ui/ReminderOverlay.cpp \
    src/ui/SpeedTestOverlay.cpp \
    src/ui/ToastOverlay.cpp \
    src/ui/WifiQrOverlay.cpp \
    src/ui/panels/AudioBluetoothPanels.cpp \
    src/ui/panels/ClockPowerPanels.cpp \
    src/ui/panels/DropboxTailscaleMediaPanels.cpp \
    src/ui/panels/MonitorNetworkPanels.cpp \
    src/ui/panels/WeatherElsewhenPanels.cpp \
    src/wayland/GlobalShortcutManager.cpp \
    src/wayland/IdleMonitor.cpp \
    src/wayland/LayerShellIntegration.cpp \
    src/wayland/SessionLockManager.cpp
