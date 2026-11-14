#include "../src/core/DesktopEntryStore.h"
#include "../src/core/IpcServer.h"
#include "../src/core/ShellConfig.h"
#include "../src/core/ThemeManager.h"
#include "../src/services/BatteryService.h"
#include "../src/services/BrightnessService.h"
#include "../src/services/ClipboardService.h"
#include "../src/services/FprintdService.h"
#include "../src/services/MediaService.h"
#include "../src/services/NightlightService.h"
#include "../src/ui/BackgroundWindow.h"
#include "../src/ui/OsdWindow.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QtTest/QtTest>

using namespace quattro;

class TestCore : public QObject {
    Q_OBJECT
private slots:
    void testThemeColorsAndShellToml() {
        QTemporaryDir envDir;
        QVERIFY(envDir.isValid());
        const QByteArray prevConfig = qgetenv("XDG_CONFIG_HOME");
        const QByteArray prevState = qgetenv("XDG_STATE_HOME");
        qputenv("XDG_CONFIG_HOME", envDir.filePath(QStringLiteral("config")).toUtf8());
        qputenv("XDG_STATE_HOME", envDir.filePath(QStringLiteral("state")).toUtf8());

        ThemeManager tm;
        tm.loadColorsText(QStringLiteral(
            "foreground = \"#eeeeee\"\n"
            "background = \"#111111\"\n"
            "accent = \"#4488ff\"\n"
            "red = \"#ff4444\"\n"));
        QCOMPARE(tm.foreground(), QColor(QStringLiteral("#eeeeee")));
        QCOMPARE(tm.background(), QColor(QStringLiteral("#111111")));
        QCOMPARE(tm.accent(), QColor(QStringLiteral("#4488ff")));
        QCOMPARE(tm.urgent(), QColor(QStringLiteral("#ff4444")));

        tm.loadThemeShellText(QStringLiteral(
            "[bar]\n"
            "background = \"#222222\"\n"
            "background-alpha = 0.5\n"
            "size-horizontal = 30\n"
            "[font]\n"
            "base-size = 14\n"
            "[notifications]\n"
            "border = \"#4488ff #ff4444 45deg\"\n"
            "border-width = \"1 2 3 4\"\n"));
        QCOMPARE(tm.bar().background, QColor(0x22, 0x22, 0x22, 128));
        QCOMPARE(tm.fontBaseSize(), 14);
        QCOMPARE(tm.barSizeHorizontal(), 35); // 30 * (14/12) = 35

        const BorderSpec spec = tm.surfaceBorderSpec(
            QStringLiteral("notifications"), QStringLiteral("border"), tm.accent(), 2);
        QVERIFY(spec.gradient.enabled);
        QCOMPARE(spec.gradient.colors.size(), 2);
        QCOMPARE(spec.gradient.angleDeg, 45.0);
        QCOMPARE(spec.widths.top, 1);
        QCOMPARE(spec.widths.right, 2);
        QCOMPARE(spec.widths.bottom, 3);
        QCOMPARE(spec.widths.left, 4);

        // User override wins over theme
        tm.loadUserShellText(QStringLiteral("[font]\nbase-size = 12\n"));
        QCOMPARE(tm.fontBaseSize(), 12);
        QCOMPARE(tm.barSizeHorizontal(), 30);

        if (prevConfig.isEmpty()) qunsetenv("XDG_CONFIG_HOME");
        else qputenv("XDG_CONFIG_HOME", prevConfig);
        if (prevState.isEmpty()) qunsetenv("XDG_STATE_HOME");
        else qputenv("XDG_STATE_HOME", prevState);
    }

    void testJsoncStripAndShellConfig() {
        const QByteArray jsonc =
            "{\n"
            "  // line comment\n"
            "  \"a\": \"http://example.com//keep\", /* block\ncomment */\n"
            "  \"b\": [1, 2, ],\n"
            "}\n";
        const QByteArray stripped = ShellConfig::stripJsonc(jsonc);
        const QJsonDocument doc = QJsonDocument::fromJson(stripped);
        QVERIFY(doc.isObject());
        QCOMPARE(doc.object().value(QStringLiteral("a")).toString(), QStringLiteral("http://example.com//keep"));
        QCOMPARE(doc.object().value(QStringLiteral("b")).toArray().size(), 2);

        ShellConfig cfg;
        const QByteArray userJson =
            "{\"version\":1,\"bar\":{\"position\":\"bottom\",\"background\":\"glassy\","
            "\"layout\":{\"left\":[{\"id\":\"quattro.menu\"}],\"center\":[{\"id\":\"quattro.clock\"}],\"right\":[]}}}";
        QVERIFY(cfg.applyJsonText("{}", userJson));
        QCOMPARE(cfg.barPosition(), QStringLiteral("bottom"));
        QCOMPARE(cfg.barBackground(), QStringLiteral("glassy"));
        QCOMPARE(cfg.barLayout().left.size(), 1);
        QCOMPARE(cfg.barLayout().left[0].id, QStringLiteral("quattro.menu"));
    }

    void testIpcSocketProtocol() {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        qputenv("XDG_RUNTIME_DIR", dir.path().toUtf8());

        IpcServer srv;
        srv.registerTarget(QStringLiteral("shell"), [](const QString &method, const QStringList &args) -> IpcResult {
            if (method == QLatin1String("ping"))
                return {true, QStringLiteral("ok")};
            if (method == QLatin1String("echo") && !args.isEmpty())
                return {true, args[0]};
            return {false, {}};
        });

        QVERIFY(srv.start(QStringLiteral("/tmp/quattro/shell"), QStringLiteral("wayland-0")));
        QVERIFY(QFileInfo::exists(srv.socketPath()));

        auto callSock = [&](const QByteArray &req) -> QByteArray {
            QLocalSocket sock;
            sock.connectToServer(srv.socketPath());
            if (!sock.waitForConnected(1000))
                return {};
            sock.write(req);
            sock.flush();
            while (sock.state() == QLocalSocket::ConnectedState && !sock.bytesAvailable())
                QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
            sock.waitForReadyRead(500);
            return sock.readAll();
        };

        QCOMPARE(callSock("shell\x1f" "ping\x1e"), QByteArray("OK\x1f" "ok\x1e"));
        QCOMPARE(callSock("shell\x1f" "echo\x1f" "hello\x1e"), QByteArray("OK\x1f" "hello\x1e"));
        QCOMPARE(callSock("unknown\x1f" "ping\x1e"), QByteArray("SKIP\x1e"));
    }

    void testOsdIconMapping() {
        QCOMPARE(OsdWindow::iconFor(QStringLiteral("volume-muted"), 50), QStringLiteral(""));
        QCOMPARE(OsdWindow::iconFor(QStringLiteral("brightness"), 80), QStringLiteral("󰍹"));
        QCOMPARE(OsdWindow::iconFor(QString(), 25), QStringLiteral(""));
        QCOMPARE(OsdWindow::iconFor(QString(), 80), QStringLiteral(""));
    }

    void testShellConfigDockBarAndPlugins() {
        QTemporaryDir envDir;
        QVERIFY(envDir.isValid());
        const QByteArray prevCfg = qgetenv("XDG_CONFIG_HOME");
        const QByteArray prevQPath = qgetenv("QUATTRO_PATH");
        qputenv("XDG_CONFIG_HOME", envDir.filePath(QStringLiteral("config")).toUtf8());
        qputenv("QUATTRO_PATH", QByteArrayLiteral("/home/ez/Projects/quattro-shell-cpp"));

        ShellConfig cfg;
        QVERIFY(cfg.applyJsonText("{\"version\":1}", "{\"version\":1}"));

        // Bar widget insertion & lookup
        const QString putRes = cfg.putBarWidget(QStringLiteral("quattro.clock"), QStringLiteral("{\"section\":\"center\",\"index\":0}"));
        QCOMPARE(putRes, QStringLiteral("ok"));
        QVERIFY(cfg.setBarWidgetSetting(QStringLiteral("quattro.clock"), QStringLiteral("format"), QStringLiteral("24h")));
        const QJsonObject entry = cfg.barWidgetEntry(QStringLiteral("quattro.clock"));
        QCOMPARE(entry.value(QStringLiteral("id")).toString(), QStringLiteral("quattro.clock"));
        QCOMPARE(entry.value(QStringLiteral("format")).toString(), QStringLiteral("24h"));

        // Dock mutations
        QVERIFY(cfg.setDockPosition(QStringLiteral("left")));
        QCOMPARE(cfg.dockPosition(), QStringLiteral("left"));
        QVERIFY(cfg.setDockAutohide(true));
        QCOMPARE(cfg.dockAutohide(), true);
        QVERIFY(cfg.setDockPinned({QStringLiteral("firefox"), QStringLiteral("kitty")}));
        QCOMPARE(cfg.dockPinned().size(), 2);

        // Plugin enable toggle + catalog JSON
        const QString enableRes = cfg.setPluginEnabled(QStringLiteral("quattro.dock"), true);
        QCOMPARE(enableRes, QStringLiteral("ok"));
        const QJsonArray plugins = QJsonDocument::fromJson(cfg.listPluginsJson().toUtf8()).array();
        QVERIFY(!plugins.isEmpty());

        if (prevCfg.isEmpty()) qunsetenv("XDG_CONFIG_HOME");
        else qputenv("XDG_CONFIG_HOME", prevCfg);
        if (prevQPath.isEmpty()) qunsetenv("QUATTRO_PATH");
        else qputenv("QUATTRO_PATH", prevQPath);
    }

    void testDesktopEntryStoreLookup() {
        QTemporaryDir envDir;
        QVERIFY(envDir.isValid());
        const QString appsDir = envDir.filePath(QStringLiteral("applications"));
        QVERIFY(QDir().mkpath(appsDir));

        {
            QFile f(appsDir + QStringLiteral("/org.demo.Term.desktop"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[Desktop Entry]\nType=Application\nName=Demo Terminal\nExec=demoterm --flag %u\nIcon=utilities-terminal\nStartupWMClass=DemoTerm\n");
        }
        {
            QFile f(appsDir + QStringLiteral("/hidden-helper.desktop"));
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write("[Desktop Entry]\nType=Application\nName=Hidden Helper\nExec=helper\nNoDisplay=true\n");
        }

        DesktopEntryStore store(envDir.path());
        store.scan();

        QVERIFY(store.findById(QStringLiteral("org.demo.Term")) != nullptr);
        QVERIFY(store.heuristicLookup(QStringLiteral("DemoTerm")) != nullptr);
        QVERIFY(store.heuristicLookup(QStringLiteral("demoterm")) != nullptr);

        bool sawHidden = false;
        bool sawVisible = false;
        for (const auto &e : store.visibleApps()) {
            if (e.id == QLatin1String("hidden-helper")) sawHidden = true;
            if (e.id == QLatin1String("org.demo.Term")) sawVisible = true;
        }
        QVERIFY(sawVisible);
        QVERIFY(!sawHidden);
    }

    void testServicesPureHelpers() {
        // MediaService::rankIdlePlayer
        MprisPlayerInfo paused;
        paused.playbackStatus = QStringLiteral("Paused");
        paused.title = QStringLiteral("Song");
        MprisPlayerInfo proxyPaused = paused;
        proxyPaused.isProxy = true;
        QVERIFY(MediaService::rankIdlePlayer(paused, true) > MediaService::rankIdlePlayer(paused, false));
        QVERIFY(MediaService::rankIdlePlayer(paused, false) > MediaService::rankIdlePlayer(proxyPaused, false));

        // NightlightService::isNightlightTemp
        QVERIFY(NightlightService::isNightlightTemp(4000));
        QVERIFY(!NightlightService::isNightlightTemp(6500));
        QVERIFY(!NightlightService::isNightlightTemp(0));

        // BrightnessService::brightnessKeyTarget
        QCOMPARE(BrightnessService::brightnessKeyTarget(QStringLiteral("raise"), 45), 50);
        QCOMPARE(BrightnessService::brightnessKeyTarget(QStringLiteral("raise"), 98), 100);
        QCOMPARE(BrightnessService::brightnessKeyTarget(QStringLiteral("lower"), 10), 5);
        QCOMPARE(BrightnessService::brightnessKeyTarget(QStringLiteral("lower"), 2), 1);

        // BatteryService::shouldWarnLowBattery
        bool nextNotified = false;
        QVERIFY(BatteryService::shouldWarnLowBattery(true, false, 8, false, nextNotified));
        QVERIFY(nextNotified);
        QVERIFY(!BatteryService::shouldWarnLowBattery(true, false, 8, true, nextNotified));
        QVERIFY(nextNotified);
        QVERIFY(!BatteryService::shouldWarnLowBattery(true, true, 8, true, nextNotified));
        QVERIFY(!nextNotified);

        // ClipboardService::isDuplicateKey
        QVector<ClipboardEntry> list;
        ClipboardEntry ce;
        ce.key = QStringLiteral("text:hello");
        list.push_back(ce);
        QVERIFY(ClipboardService::isDuplicateKey(list, QStringLiteral("text:hello")));
        QVERIFY(!ClipboardService::isDuplicateKey(list, QStringLiteral("text:world")));
    }

    void testFprintdHelpersAndVideoDetection() {
        QCOMPARE(FprintdService::classifyProbeOutput(QStringLiteral("Fingerprints found for user ez:\n - #0: right-index-finger")),
                 QStringLiteral("yes"));
        QCOMPARE(FprintdService::classifyProbeOutput(QStringLiteral("User has no fingers enrolled")),
                 QStringLiteral("no"));
        QCOMPARE(FprintdService::classifyProbeOutput(QStringLiteral("something unexpected")),
                 QStringLiteral("unknown"));
        QCOMPARE(FprintdService::errorBackoffMs(1), 1000);
        QCOMPARE(FprintdService::errorBackoffMs(2), 2000);
        QCOMPARE(FprintdService::errorBackoffMs(20), FprintdService::kErrorRetryCapMs);
        QVERIFY(FprintdService::spannedSleep(5000, 1000));
        QVERIFY(!FprintdService::spannedSleep(1200, 1000));
        QCOMPARE(FprintdService::nextStreak(2, false, false), 3);
        QCOMPARE(FprintdService::nextStreak(2, false, true), 1);
        QCOMPARE(FprintdService::nextStreak(2, true, false), 0);
        QVERIFY(FprintdService::isUnavailableStreak(3));
        QVERIFY(FprintdService::cachedPosterPathFor(QStringLiteral("/nonexistent-demo-12345.mp4")).isEmpty());
        QTemporaryDir posterDir;
        QVERIFY(posterDir.isValid());
        const QString dummyVid = posterDir.filePath(QStringLiteral("demo.mp4"));
        { QFile vf(dummyVid); QVERIFY(vf.open(QIODevice::WriteOnly)); vf.write("mp4"); }
        const QString posterPath = FprintdService::cachedPosterPathFor(dummyVid);
        QVERIFY(posterPath.endsWith(QStringLiteral(".jpg")));
        QVERIFY(posterPath.contains(QStringLiteral("/quattro/lock-poster/poster-")));

        QVERIFY(BackgroundManager::isVideoPath(QStringLiteral("/tmp/wallpaper.mp4")));
        QVERIFY(BackgroundManager::isVideoPath(QStringLiteral("/tmp/intro.WEBM")));
        QVERIFY(!BackgroundManager::isVideoPath(QStringLiteral("/tmp/wallpaper.png")));
    }
};

QTEST_MAIN(TestCore)
#include "test_core.moc"
