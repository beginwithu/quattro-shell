# quattro-shell-cpp

Native **C++20 / Qt6 / Wayland** rewrite of the [Hyprland](https://hypr.land/) Quattro shell bundle (replacing the Quickshell/QML runtime with a single compiled `quattro-cpp` binary), packaged with all user configs, themes, CLI scripts, and system integration for standard Arch Linux.

## Directory layout

- `src/` - C++20 shell source code:
  - `src/main.cpp` - application entrypoint, service wiring, global shortcuts, and IPC target registration
  - `src/core/` - configuration (`ShellConfig`), theming (`ThemeManager`), Hyprland IPC (`HyprlandClient`), `.desktop` scanner (`DesktopEntryStore`), and UNIX socket IPC (`IpcServer`)
  - `src/wayland/` - Wayland protocol bindings (`LayerShellIntegration`, `SessionLockManager`, `IdleMonitor`, `GlobalShortcutManager`)
  - `src/services/` - native Audio/PulseAudio, Battery/UPower, Brightness, Clipboard, Fingerprint (`fprintd`), Media (MPRIS), Nightlight (`hyprsunset`), Notifications (D-Bus), PAM auth, Polkit (`polkit-agent-1`), Remote Session, SystemStats, and SNI TrayWatcher
  - `src/ui/` & `src/ui/panels/` - top bar, dock & app grid, launcher menu, background (image + looping Qt6Multimedia video + theme intro wipe), lockscreen, polkit prompt, OSD, notifications/toasts, overlays (clipboard, emojis, image picker, reminders, Wi-Fi QR, network/disk speedtest, dev gallery), and popout panels (audio, bluetooth, clock, power, monitor, network, weather, elsewhen, dropbox, tailscale, media)
- `protocols/` - local Wayland protocol XMLs (`wlr-layer-shell-unstable-v1.xml`, `hyprland-global-shortcuts-v1.xml`)
- `assets/` & `assets.qrc` - compiled static datasets (`emojis.json`, `cities.json`, `world.json`)
- `tests/` - Qt Test suite (`tests/test_core.cpp`, `tests/tests.pro`)
- `quattro-cpp.pro` & `Makefile` - `qmake6` build and test targets (`build/bin/quattro-cpp`, `build/bin/test_core`)
- `bin/` - `quattro-*` CLI helpers and session scripts (`quattro-launch-shell`, `quattro-shell`, `quattro-restart-shell`, `quattro-theme-set`, etc.)
- `config/` - user dotfiles (`quattro/shell.json`, `hypr`, `foot`, `alacritty`, `kitty`, `ghostty`, `btop`, `tmux`, `starship.toml`, etc.)
- `default/` - runtime defaults (bash setup, Hyprland Lua modules, shortcuts, menu tree, theme templates, systemd user units, fonts, SDDM theme)
- `themes/` - color schemes (`Tokyo Night`, `Catppuccin`, `Gruvbox`, `Kanagawa`, `Nord`, `Rose Pine`, etc.)
- `applications/` & `etc/` - `.desktop` entries, icons, and system config files (`fastfetch`, `mpv`, `xkb`, `sudoers.d/quattro-dns`)
- `packages/` - core Arch package list ([arch-core.packages](packages/arch-core.packages)) and [package-map.md](packages/package-map.md)
- `AGENTS.md` - contributor & agent guide for the C++ shell architecture, build quirks, IPC, and verification
- `install.sh` - builds `quattro-cpp` if needed and deploys the runtime, configs, initial theme, and system integration
- `verify.sh` - runs Bash/Python syntax checks, `luac -p`, JSON validation, pacman package checks, and a sandboxed `install.sh` test

## Build & test

Build the shell binary and run the unit test suite (works headless with Qt's offscreen plugin):

```bash
make build
QT_QPA_PLATFORM=offscreen make test
```

Check scripts, Lua configs, JSON configs, package names, and sandbox deployment without modifying system or home files:

```bash
./verify.sh
```

## Install

Run the installer (installs Arch `[core]`/`[extra]` dependencies including Qt6, PulseAudio, PAM, Polkit, and `qrencode`, then deploys the runtime, `quattro-cpp`, user configs, initial `Tokyo Night` theme, and SDDM/systemd integration):

```bash
./install.sh
```

Existing user files in `~/.config` and `~/.bashrc` are overwritten directly without creating backups. For an isolated test deployment, run `QUATTRO_INSTALL_SANDBOX=1 ./install.sh`.

## Starting the session

Log in via SDDM (choose **Quattro (Hyprland uwsm)**), or start it from a TTY:

```bash
uwsm start hyprland.desktop
```

## Compatibility & Arch notes

- Existing `~/.config/quattro/shell.json`, `~/.config/quattro/shell.toml`, theme `colors.toml` / `shell.toml`, and `quattro-shell <target> <method>` IPC callers remain compatible with the C++ binary.
- `QUATTRO_PATH` defaults to `~/.local/share/quattro` and falls back to `/usr/share/quattro`.
- `packages/arch-core.packages` removes the `quickshell` dependency in favor of native `quattro-cpp` using standard Arch packages (`qt6-base`, `qt6-multimedia`, `qt6-svg`, `qt6-wayland`, `libpulse`, `pam`, `polkit`, `qrencode`).
