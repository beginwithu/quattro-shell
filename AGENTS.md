# AGENTS.md

Notes for working on `quattro-shell` and the rest of this repository.

## Project layout

- `shell/` - long-running Quickshell instance (`quickshell -n -p "$QUATTRO_PATH/shell"`), started by `bin/quattro-launch-shell` and restarted by `bin/quattro-restart-shell`.
- `shell/shell.qml` - host `ShellRoot`. Owns `PluginRegistry`, `BarWidgetRegistry`, `AppLibrary`, `BrightnessKeys`, `BackgroundIntro`, `BackgroundSwitch`, config loading, IPC registration, and plugin mounting.
- `shell/Commons/` (`module qs.Commons`) - shared singletons (`AudioNodes`, `Border`, `Color`, `I18n`, `InputDevices`, `IpcRegistry`, `InputMethodState`, `Style`, `UntypedInput`, `Util`) plus `ShellIpc` and `Translations`.
- `shell/Ui/` (`module qs.Ui`) - shared UI components (`BarWidget`, `BarIndicator`, `BarIconButton`, `BorderSurface`, `Button`, `ConfirmDialog`, `Dropdown`, `FilterInput`, `KeyboardPanel`, `OverlayWindow`, `Panel`, `PanelController`, `PanelHero`, `PanelSlider`, `PopupCard`, `PopupMenuRow`, `SearchableDropdown`, `TextField`, `Toggle`, `ToggleSwitch`, etc.).
- `shell/services/` - host-level QML/JS services (`PluginRegistry.qml`, `BarWidgetRegistry.qml`, `AppLibrary.qml`, `BackgroundIntro.qml`, `BackgroundSwitch.qml`, `BrightnessKeys.qml`, plus capability-scoped plugin facades `Plugin*Api.qml`).
- `shell/plugins/` - first-party plugins loaded from `manifest.json`:
  - `bar/` (`quattro.bar`) and `bar/widgets/` / `bar/indicators/`
  - `dock/`, `menu/`, `launcher` (dock app grid), `osd/`, `lock/`, `notifications/`, `background/`, `clipboard/`, `emojis/`, `image-picker/`, `polkit/`, `reminders/`
  - `panels/` (`audio`, `bluetooth`, `clock`, `disk-speedtest`, `dropbox`, `elsewhen`, `monitor`, `network`, `power`, `speedtest`, `tailscale`, `weather`, `wifiqr`)
  - `services/` (`battery`, `idle`, `media`, `nightlight`, `remote-session`)
- `config/quattro/shell.json` - default shell layout (`bar`, `dock`, `idle`, `plugins`). User overrides live at `~/.config/quattro/shell.json` (canonical once created; no deep-merge).
- `default/quattro/quattro-menu.jsonc` - Quattro menu tree definition read by `shell/plugins/menu/`.
- `default/quattro/shortcuts` - global Hyprland shortcuts registered by the shell (`menu <route>`, `panel <plugin-id>`, `audio ...`, `brightness ...`, `ipc <target>.<method>`).
- `default/themed/shell.toml.tpl` - template rendering `~/.local/state/quattro/current/theme/shell.toml` and `colors.toml` via `bin/quattro-theme-set`.

## Runtime & paths

- `QUATTRO_PATH` defaults to `~/.local/share/quattro` (deployed by `./install.sh`) and falls back to `/usr/share/quattro`.
- Never hardcode `/home/...` or `/usr/share/omarchy`; read `shell.quattroPath` (from `Quickshell.env("QUATTRO_PATH")`) or `$QUATTRO_PATH` in scripts.
- Quickshell's built-in file watcher is disabled at launch (`QS_DISABLE_FILE_WATCHER=1` in `bin/quattro-launch-shell`). After editing QML inside `shell/`:
  - Reload user/plugin code via `quattro-shell shell rescanPlugins` (or `quattro-restart-shell` for host changes in `shell.qml`, `Commons/`, `Ui/`, or `services/`).
  - Reload `shell.json` via `quattro-shell shell reloadConfig`.

## QML rules for `quattro-shell`

1. **Qualify `Commons.Color`**: Qt 6.12 defines its own `Color` type. Always alias `qs.Commons` when referencing palette tokens:
   ```qml
   import qs.Commons
   import qs.Commons as Commons

   color: Commons.Color.background
   ```
   `Style`, `Util`, and `Border` can stay unqualified.

2. **Do not re-import shared services by relative path**: `shell/shell.qml` creates single `PluginRegistry`, `BarWidgetRegistry`, and `AppLibrary` instances and passes them to plugins via property injection (`shell`, `pluginRegistry`, `barWidgetRegistry`, `manifest`, `quattroPath`). Importing `"../../services"` in a plugin creates a separate empty instance.

3. **Plain text for external strings**: QML `Text` defaults to `Text.AutoText`, which parses HTML/rich text tags from untrusted sources (notifications, SSIDs, window titles, command output, tray tooltips). Always set `textFormat: Text.PlainText` on dynamic `Text` elements.

4. **Theme tokens, spacing, and motion**:
   - Colors come from `Commons.Color.<surface>.*` (`bar`, `popups`, `tooltip`, `notifications`, `menu`, `polkit`, `lock`, `imagePicker`).
   - Borders that follow theme gradients/per-side widths use `BorderSurface` (`qs.Ui`) + `Border` (`qs.Commons`).
   - Spacing and sizing come from `Style.spacing.*`, `Style.space(px)`, `Style.cornerRadius`, and `Style.font.*` rather than hardcoded pixels.
   - Animations must wrap durations in `Style.duration(ms)` (returns `0` when `Style.reduceMotion` is active) and use `Style.easeOut` or `Easing.OutCubic`, under 300ms (around 180ms enter, 120ms exit), scaling from `0.95`-`0.97` with opacity rather than `0`.

5. **Plugins & manifests**:
   - Each plugin in `shell/plugins/<dir>/` declares `schemaVersion: 1`, `id`, `name`, `kinds` (`bar`, `bar-widget`, `panel`, `overlay`, `menu`, `service`), and `entryPoints`.
   - `panel`, `overlay`, and `menu` entry points implement `open(payloadJson)` and `close()`.
   - Bar widgets in `bar.layout` can be dragged or moved across sections (`left`, `centerBegin`, `center`, `centerEnd`, `tray`, `right`) at runtime. Declare `property string fixedEdge` (or inherit `BarWidget` from `qs.Ui`) and bind to `fixedEdge` and `settings` instead of reading them once on `Component.onCompleted`.
   - Third-party plugins live in `~/.config/quattro/plugins/<id>/` and receive capability-scoped facades (`Plugin*Api.qml`) instead of raw host objects.

6. **Translations (`i18n`)**:
   - Host strings live in `default/i18n/<lang>/shell.json` (`Commons.I18n.t(...)`, `Commons.I18n.formatNumber(...)`, `Commons.I18n.formatLocale`).
   - Plugin-local strings use `Commons.Translations` with `<plugin>/i18n/en.json`. Keep whole sentences with `%1` placeholders (`.arg(...)`) and CLDR plural keys (`one`, `other`) via `tn(key, count)`.
   - Support RTL (`LayoutMirroring.enabled`, `Commons.I18n.rightToLeft`, `Util.readingKey(event)`).

## IPC & CLI entry points

Talk to the running shell with `bin/quattro-shell` (`-q` suppresses errors when headless/not running):

```bash
quattro-shell shell ping
quattro-shell shell listPlugins
quattro-shell shell summon quattro.clock '{}'
quattro-shell shell toggle quattro.menu '{"menu":"root"}'
quattro-shell shell togglePanelAt right 1
quattro-shell shell reloadConfig
quattro-shell shell rescanPlugins
quattro-shell shell debugBarGeometry
```

`bin/quattro` routes subcommands like `quattro bar ...`, `quattro plugin ...`, `quattro shell ...`, `quattro theme ...`. Any new executable in `bin/` must include `# quattro:summary=...` at the top so the router discovers it.

## Scope & verification

- This repo is a standalone Arch Linux setup without AI/agent desktop integrations (`opencode`, `herdr`, `quattro-agent*`, AI dictation, crash-watch AI hooks have been removed) and without bloat preinstalled webapps or AUR requirements. Keep new changes free of those dependencies.
- After editing scripts, Lua configs, JSON configs, or shell files, run:
  ```bash
  ./verify.sh
  ```
- `./verify.sh` checks Bash/Python syntax, `luac -p`, JSON validity, `pacman -Ssq` resolution for `packages/arch-core.packages`, and an isolated `QUATTRO_INSTALL_SANDBOX=1` run of `./install.sh`.
