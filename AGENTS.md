# AGENTS.md

Notes for working on `quattro-shell-cpp` (the C++20/Qt6 native rewrite of `quattro-shell`) and the bundled Hyprland distribution.

## Project layout

- `quattro-cpp.pro` & `Makefile` - out-of-tree `qmake6` build (`build/bin/quattro-cpp`) and unit test runner (`build/bin/test_core`).
- `src/main.cpp` - single-binary runtime entrypoint. Initializes Qt, `ThemeManager`, `ShellConfig`, `HyprlandClient`, `DesktopEntryStore`, `IpcServer`, system/device services, global shortcuts (`default/quattro/shortcuts`), all LayerShell surfaces (`BarWindow`, `BackgroundManager`, `DockManager`, `MenuWindow`, `LockWindow`, `PolkitWindow`, `OsdWindow`, `ToastOverlay`, overlays, and popout panels).
- `src/core/` - host core:
  - `ShellConfig.{h,cpp}` - JSONC config loader (`config/quattro/shell.json` + `~/.config/quattro/shell.json`), runtime mutations (`bar`, `dock`, `plugins`, widget settings), and plugin catalog JSON (`listPluginsJson()`).
  - `ThemeManager.{h,cpp}` - parses `colors.toml`, theme `shell.toml`, and `~/.config/quattro/shell.toml`, resolves font scaling (`space()`, `fontScale()`), borders/gradients (`surfaceBorderSpec()`), and animations (`duration()` / `reduceMotion()`).
  - `HyprlandClient.{h,cpp}` - talks to Hyprland's UNIX command/event sockets (`HYPRLAND_INSTANCE_SIGNATURE`).
  - `DesktopEntryStore.{h,cpp}` - scans `.desktop` entries, icon themes, and performs heuristic class/title matching for dock grouping and the app launcher.
  - `IpcServer.{h,cpp}` - `QLocalServer` speaking the `socat`-friendly `\x1f`/`\x1e` framing protocol used by `bin/quattro-shell`.
- `src/wayland/` - generated QtWayland client wrappers + integration helpers:
  - `LayerShellIntegration.{h,cpp}` (`wlr-layer-shell-unstable-v1.xml`)
  - `SessionLockManager.{h,cpp}` (`ext-session-lock-v1.xml`)
  - `IdleMonitor.{h,cpp}` (`ext-idle-notify-v1.xml`)
  - `GlobalShortcutManager.{h,cpp}` (`hyprland-global-shortcuts-v1.xml`)
- `src/services/` - native C++/D-Bus/PulseAudio/PAM/Polkit backends:
  - `AudioService`, `BatteryService`, `BrightnessService`, `ClipboardService`, `FprintdService` (includes video poster generation via `ffmpegthumbnailer`), `MediaService` (MPRIS2 + Pulse stream ranking), `NightlightService` (`hyprsunset`), `NotificationDaemon` (`org.freedesktop.Notifications`), `PamAuth`, `PolkitService` (`PolkitAgentListener` GObject subclass via `polkit-agent-1`), `RemoteSessionService`, `SystemStatsService`, `TrayWatcher`.
- `src/ui/` & `src/ui/panels/` - widget painting & interactive surfaces built on `UiPaint`, `BorderPainter`, and `ShellPopupWindow` (`CommonUi.{h,cpp}`):
  - Core surfaces: `BarWindow`, `BackgroundWindow` (image + Qt6Multimedia looping video + slanted wipe intro), `LockWindow`, `MenuWindow`, `OsdWindow`, `ToastOverlay`.
  - Overlays: `PolkitWindow`, `DockWindow` (`DockManager` + `DockWindow` + `AppGridWindow`), `ImagePickerWindow`, `ClipboardOverlay`, `EmojiOverlay`, `ReminderOverlay`, `WifiQrOverlay` (`libqrencode`), `SpeedTestOverlay` (`Network` & `Disk` modes), `DevGalleryWindow`.
  - Panels (`src/ui/panels/`): `AudioBluetoothPanels` (`AudioPanel`, `BluetoothPanel`), `ClockPowerPanels` (`ClockPanel`, `PowerPanel`), `MonitorNetworkPanels` (`MonitorPanel`, `NetworkPanel`), `WeatherElsewhenPanels` (`WeatherPanel`, `ElsewhenPanel`), `DropboxTailscaleMediaPanels` (`DropboxPanel`, `TailscalePanel`, `MediaPanel`).
- `assets/` & `assets.qrc` - static JSON bundled into Qt resources (`:/assets/emojis.json`, `:/assets/cities.json`, `:/assets/world.json`).
- `tests/` - QtTest unit tests (`test_core.cpp`, `tests.pro`).
- `default/`, `config/`, `themes/`, `bin/`, `etc/`, `applications/` - bundled Hyprland dotfiles, CLI tools, theme templates, SDDM theme, and installer scripts.
- `install.sh` & `packages/arch-core.packages` - builds (if absent) and installs `quattro-cpp` alongside Arch `[core]`/`[extra]` dependencies (no `quickshell` dependency; links `qt6-*`, `libpulse`, `pam`, `polkit`, `qrencode`).

## Runtime & paths

- `QUATTRO_PATH` defaults to `~/.local/share/quattro` (deployed by `./install.sh`) and falls back to `/usr/share/quattro`.
- Never hardcode `/home/...` or `/usr/share/omarchy`; read `$QUATTRO_PATH` / `qEnvironmentVariable("QUATTRO_PATH")` and `QDir::homePath()` / XDG dirs.
- `bin/quattro-launch-shell` launches `$QUATTRO_PATH/bin/quattro-cpp` (fallback `quattro-cpp` in `$PATH`) under `systemd-cat -t quattro-shell` and supervises non-zero exits while Hyprland is still alive.
- `bin/quattro-restart-shell` stops running `quattro-cpp` processes gracefully, respawns `quattro-launch-shell` via `hyprctl`, waits on `quattro-shell shell ping`, and re-secures stranded locks if needed.
- `bin/quattro-version-channel` reports `arch-standalone`.

## C++20 / Qt6 build & code rules

1. **`<version>` header collision in `qmake6`**:
   - The repo root contains a plain text file named `version`. `quattro-cpp.pro` therefore sets `CONFIG += no_include_pwd` together with `INCLUDEPATH += src $$OUT_PWD` so `$$PWD` is not placed on `-I` ahead of C++20 standard library `<version>` while generated `qwayland-*.h` headers in `build/` remain reachable. Do not remove `no_include_pwd`.

2. **GLib / Polkit header isolation**:
   - GLib headers (`<polkitagent/polkitagent.h>`) define `signals` and collide with Qt MOC keywords. Keep GLib/Polkit C includes strictly inside `.cpp` translation units (`src/services/PolkitService.cpp`) wrapped in `#pragma push_macro("signals") / #undef signals / ... / #pragma pop_macro("signals")`, and declare only forward structs or plain C++ types in `.h` files.

3. **Qt6 private headers & C++20 lambdas**:
   - `LayerShellIntegration` and `wayland-scanner` wrappers use `waylandclient-private`, tying builds cleanly to the installed Qt6 version.
   - Prefer explicit `[this]` (or `[this, ...]`) captures instead of `[=]` in C++20 lambdas to avoid `-Wdeprecated` warnings.
   - Use `QChar::isPrint()` rather than non-existent `QChar::isControl()` in Qt6 text input handlers.

4. **Shared UI painting, theme tokens, and motion**:
   - Reuse `UiPaint`, `BorderPainter`, `ShellPopupWindow`, and `ThemeManager` surface palettes (`bar()`, `popups()`, `tooltip()`, `notifications()`, `menu()`, `polkit()`, `lock()`, `imagePicker()`) rather than duplicating card/pill/toggle drawing logic.
   - Wrap animation durations in `theme()->duration(ms)` so `reduceMotion` is honored everywhere.
   - Keep `Qt::PlainText` / raw string drawing (no HTML rich-text parsing) for untrusted strings (notifications, SSIDs, window titles, clipboard previews, tray tooltips).

5. **Plugin & config compatibility**:
   - Keep `shell/plugins/*/manifest.json` metadata and `ShellConfig::listPluginsJson()` / `ShellConfig::setPluginEnabled()` compatible with `~/.config/quattro/shell.json` and `bin/quattro-plugin-catalog`.
   - New executables in `bin/` must include `# quattro:summary=...` at the top so `bin/quattro` discovers them.

## IPC & CLI entry points

`bin/quattro-shell` sends requests directly over the MD5-hashed UNIX socket (`${XDG_RUNTIME_DIR}/quattro-shell-<md5>.sock`, using `\x1f` field and `\x1e` record separators) served by `IpcServer` inside `quattro-cpp`:

```bash
quattro-shell shell ping
quattro-shell shell listPlugins
quattro-shell shell summon quattro.clock '{}'
quattro-shell shell toggle quattro.menu '{"menu":"root"}'
quattro-shell shell togglePanelAt 1200 32
quattro-shell shell reloadConfig
quattro-shell shell rescanPlugins
quattro-shell shell debugBarGeometry
quattro-shell image-selector preload
quattro-shell quattro.dock toggle
```

Registered targets in `src/main.cpp`:
- `shell`, `image-selector`, `quattro.dock`, `media`, `nightlight`, `battery`, `osd`, `notifications`, `lock`, `background`, `quattro.menu`, `quattro.bar`, `quattro.indicators`, `quattro.system-update`, plus uniform `open` / `close` / `hide` / `toggle` handlers for every `quattro.*` overlay and panel in `popupMap`.

## Build, tests & verification

- Rebuild the shell binary and run the Qt unit tests cleanly:
  ```bash
  make build
  QT_QPA_PLATFORM=offscreen make test
  ```
- Verify scripts, Lua configs, JSON configs, Arch repo packages, and sandboxed installation (`QUATTRO_INSTALL_SANDBOX=1 ./install.sh`):
  ```bash
  ./verify.sh
  ```
- `make test` and `./verify.sh` cover unit tests and non-privileged deployment cleanly, while compositor-bound LayerShell surfaces, SDDM greeter login, PAM/Polkit interactive prompts, and `sudo` steps in `install.sh` require a live Hyprland/Arch session or VM.
