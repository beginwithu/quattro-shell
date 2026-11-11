# First-party plugins

These plugins ship with Quattro and are discovered by the shell at startup.
They use the same `manifest.json` contract as third-party plugins; the
only difference is that the shell flags them with `__isFirstParty: true`.
First-party non-bar plugins are enabled unless listed in `disabledPlugins[]`;
`quattro.bar` is the default bar option and becomes inactive only while another
`kind: "bar"` plugin is selected. Services and keep-loaded panels are mounted
at startup; other panels, overlays, and menus are loaded on demand.

User-installed plugins live alongside these conceptually but on disk under
`~/.config/quattro/plugins/<plugin-id>/` rather than in this directory.

| Plugin        | id                        | kinds                   | entry point                           |
|---------------|---------------------------|-------------------------|---------------------------------------|
| Bar           | `quattro.bar`             | `bar`                   | `bar/Bar.qml`                         |
| Emojis        | `quattro.emojis`          | `overlay`               | `emojis/Emojis.qml`                   |
| Clipboard mgr | `quattro.clipboard`       | `overlay`               | `clipboard/Clipboard.qml`             |
| Quattro menu  | `quattro.menu`            | `menu`, `bar-widget`    | `menu/Menu.qml`, `menu/BarWidget.qml` |
| Notifications | `quattro.notifications`   | `service`               | `notifications/Service.qml`           |
| Audio         | `quattro.audio`           | `bar-widget`            | `panels/audio/Panel.qml`              |
| Bluetooth     | `quattro.bluetooth`       | `bar-widget`            | `panels/bluetooth/Panel.qml`          |
| Clock         | `quattro.clock`           | `bar-widget`            | `panels/clock/BarWidget.qml`          |
| Elsewhen      | `quattro.elsewhen`        | `bar-widget`            | `panels/elsewhen/Panel.qml`           |
| Network       | `quattro.network`         | `bar-widget`            | `panels/network/Panel.qml`            |
| Power         | `quattro.power`           | `bar-widget`            | `panels/power/Panel.qml`              |
| Weather       | `quattro.weather`         | `bar-widget`            | `panels/weather/BarWidget.qml`        |
| Media         | `quattro.media`           | `service`, `bar-widget` | `services/media/Service.qml`, `services/media/BarWidget.qml` |
| Battery       | `quattro.battery`         | `service`               | `services/battery/Service.qml`        |
| Idle          | `quattro.idle`            | `service`               | `services/idle/Service.qml`           |
| Night light   | `quattro.nightlight`      | `service`               | `services/nightlight/Service.qml`     |
| Lock screen   | `quattro.lock`            | `service`               | `lock/Service.qml`                    |
| OSD           | `quattro.osd`             | `panel`                 | `osd/Osd.qml`                         |
| Polkit agent  | `quattro.polkit`          | `service`               | `polkit/PolkitAgent.qml`              |
| Dock          | `quattro.dock`            | `service`               | `dock/Dock.qml`                       |

First-party bar-only widgets also carry manifests next to their QML files,
e.g. `bar/widgets/Workspaces.manifest.json`. Rich popup widgets live in their
own plugin directories, each with its own `manifest.json`.

## Bar

The built-in status bar and default full-bar option. Layout lives in the
top-level `bar:` subtree of `~/.config/quattro/shell.json` (with the shell
providing [`config/quattro/shell.json`](../../config/quattro/shell.json) when
the user has no file). See [`bar/README.md`](bar/README.md) for the widget catalogue
and customization schema.

## Lock screen

Session-lock surface using Quickshell's native `WlSessionLock` and two
separate PAM services: `quattro-lock-password` for password auth and,
only when fingerprints are enrolled, `quattro-lock-fingerprint` for
fingerprint auth. It mirrors the previous lock screen field dimensions,
colors, blurred wallpaper, placeholder, and Hyprland-driven corners.
The plugin sets `keepLoaded: true` so a plugin hot-reload (for example
an installed bar widget changing on disk) does not destroy the lock
client while Hyprland still holds the session lock.

## Polkit agent

Theme-aware authentication dialog for privileged actions. It uses
Quickshell's native `Quickshell.Services.Polkit.PolkitAgent` backend and
runs inside the long-lived `quattro-shell` process, replacing the old
`polkit-gnome-authentication-agent-1` autostart.

## Dock

App launcher overlay (`AppGrid`). `quattro-shell quattro.dock apps` toggles its searchable grid of installed applications, and the menu's Apps entry opens it.

## Quattro menu

Quickshell-powered Quattro command menu.
The menu UI lives in `menu/Menu.qml` as a first-party `menu` plugin and is
summoned through the shell (`quattro-shell shell summon quattro.menu ...`),
so it shares the long-running `quattro-shell` process instead of starting a
second Quickshell instance.

The menu definition lives outside the shell host code:

- defaults: `default/quattro/quattro-menu.jsonc`
- user extensions: `~/.config/quattro/extensions/quattro-menu.jsonc`

The shell parses both JSONC files at startup (with `watchChanges: true`
so edits take effect without a restart), evaluates `when:` / `checked:`
bash expressions in a single batched subprocess, and executes the
selected `action:` string directly via `Quickshell.execDetached`. The
long-running shell process keeps the parsed menu in memory, so the
keybind → IPC → visible path costs ~30ms cold.
