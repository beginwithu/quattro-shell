QT += core gui widgets svg multimedia dbus network concurrent waylandclient waylandclient-private testlib
CONFIG += c++20 wayland-scanner warn_on

TARGET = test_core
TEMPLATE = app
DESTDIR = $$PWD/../build/bin
OBJECTS_DIR = obj
MOC_DIR = moc

INCLUDEPATH += ../src $$OUT_PWD

LIBS += -lwayland-client -lpam -lpulse

WAYLANDCLIENTSOURCES += \
    /usr/share/wayland-protocols/stable/xdg-shell/xdg-shell.xml \
    ../protocols/wlr-layer-shell-unstable-v1.xml

HEADERS += \
    ../src/core/DesktopEntryStore.h \
    ../src/core/HyprlandClient.h \
    ../src/core/IpcServer.h \
    ../src/core/ShellConfig.h \
    ../src/core/ThemeManager.h \
    ../src/services/AudioService.h \
    ../src/services/BatteryService.h \
    ../src/services/BrightnessService.h \
    ../src/services/ClipboardService.h \
    ../src/services/FprintdService.h \
    ../src/services/MediaService.h \
    ../src/services/NightlightService.h \
    ../src/services/NotificationDaemon.h \
    ../src/services/PamAuth.h \
    ../src/services/SystemStatsService.h \
    ../src/ui/BackgroundWindow.h \
    ../src/ui/BorderPainter.h \
    ../src/ui/OsdWindow.h \
    ../src/wayland/LayerShellIntegration.h

SOURCES += \
    test_core.cpp \
    ../src/core/DesktopEntryStore.cpp \
    ../src/core/HyprlandClient.cpp \
    ../src/core/IpcServer.cpp \
    ../src/core/ShellConfig.cpp \
    ../src/core/ThemeManager.cpp \
    ../src/services/AudioService.cpp \
    ../src/services/BatteryService.cpp \
    ../src/services/BrightnessService.cpp \
    ../src/services/ClipboardService.cpp \
    ../src/services/FprintdService.cpp \
    ../src/services/MediaService.cpp \
    ../src/services/NightlightService.cpp \
    ../src/services/NotificationDaemon.cpp \
    ../src/services/PamAuth.cpp \
    ../src/services/SystemStatsService.cpp \
    ../src/ui/BackgroundWindow.cpp \
    ../src/ui/BorderPainter.cpp \
    ../src/ui/OsdWindow.cpp \
    ../src/wayland/LayerShellIntegration.cpp
