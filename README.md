# Quattro shell

A Quickshell shell (bar, panels, notifications, launcher, OSD, lock screen, polkit agent)
taken from Omarchy and detached from it. You do not need Omarchy: Quickshell and a few
ordinary packages are enough. MIT licensed, original copyright David Heinemeier Hansson
(see `LICENSE`).

## Install (Arch)

```sh
sudo pacman -S quickshell jq curl wireplumber libpulse networkmanager bluez-utils upower \
  power-profiles-daemon brightnessctl wl-clipboard wtype grim slurp iw libnotify wlsunset \
  ttf-jetbrains-mono-nerd

git clone <this folder> ~/quattro-shell   # or extract the archive
cd ~/quattro-shell
./install.sh --lock                       # --lock installs the PAM files (sudo), once
```

Start the shell from your compositor's autostart, not from anywhere else, because this
script sets up `QUATTRO_PATH` and `PATH`:

```sh
~/quattro-shell/bin/quattro-launch-shell
```

Installer options: `--check` (dependency report only), `--sleep-lock` (lock before suspend),
`--theme NAME`, `--bin-dir DIR`, `--uninstall`. Commands are linked into `/usr/local/bin` (sudo asks
once); the checkout must stay where it is, because the links point into it. Run it as your normal
user, not as root. `--bin-dir ~/.local/bin` keeps the old per-user behaviour. Themes: `quattro-theme-set list` / `quattro-theme-set nord`.

Other distributions: `./install.sh --check` lists what is missing; the package names differ
but the tools are the same. `quattro-apply-lock` builds the lock screen's PAM service on
`system-auth` (Arch, Fedora, openSUSE, Void) or `common-auth` (Debian, Ubuntu).

## Compositor support

Quickshell itself runs on any wlroots-style compositor (dwl, sway, river, labwc, niri,
Hyprland). Only these features depend on the compositor:

| Feature | Hyprland | sway | niri | dwl and others |
|---|---|---|---|---|
| Bar, panels, notifications, launcher, OSD, lock screen, polkit, tray | yes | yes | yes | yes |
| Workspaces widget | yes (native) | yes (`swaymsg` + `jq`) | yes (`niri msg` + `jq`) | hidden unless you supply a feed |
| Keyboard layout in the bar | yes | no | no | no |
| Close popups by clicking outside | Hyprland focus grab | transparent click-catcher layer | same | same |
| Global shortcuts (`GlobalShortcut`) | yes | no | no | no |
| Corner radius and gaps read from the compositor | yes | set in `shell.toml` | same | same |

Outside Hyprland, bind keys in your compositor's own config to these commands:

```sh
quattro-menu                                  # main menu
quattro-shell quattro.dock apps               # application launcher
quattro-shell shell toggle quattro.clipboard  # clipboard history (also quattro.emojis, quattro.audio, ...)
quattro-audio-output-volume raise|lower|mute-toggle
quattro-brightness-display +5%                # or 5%-
quattro-toggle-nightlight
quattro-system-lock
```

For dwl, for example, add `SHCMD("quattro-menu")` bindings to `config.h` and start the shell with
`dwl -s 'quattro-launch-shell'` or from your startup script.

### Workspaces/tags on other compositors (dwl and similar)

Set these in `~/.config/quattro/env`:

```sh
QUATTRO_WORKSPACES_CMD='your-script-that-prints-json'
QUATTRO_WORKSPACE_FOCUS_CMD='your-focus-script {id}'
```

`QUATTRO_WORKSPACES_CMD` must keep running and print one JSON line whenever something changes:

```json
{"focusedOutput":"DP-1","workspaces":[{"id":"1","label":"1","output":"DP-1","active":true,"focused":true,"occupied":true}]}
```

The number of always-visible numbered slots on Hyprland and sway is set in `shell.json`:
`{ "id": "quattro.workspaces", "slots": 7 }`.

## Configuration

- `~/.config/quattro/shell.json`: bar and widget layout. Defaults: `config/quattro/shell.json`.
  Change it from the menu (Appearance) or with `quattro-bar position bottom` and
  `quattro-bar set quattro.clock format HH:mm`.
- `~/.config/quattro/shell.toml`: per-surface shape and colors. Away from Hyprland:
  ```toml
  [shape]
  corner-radius = 6
  gaps-out = 10
  ```
- `~/.config/quattro/env`: environment for the shell (`QUATTRO_COMPOSITOR`, `QUATTRO_TERMINAL`, ...).
- Extra menu entries: `~/.config/quattro/extensions/quattro-menu.jsonc`.
- Language: `quattro-language <code>` (`quattro-language list` shows the codes).

## What changed from Omarchy

Removed because they are tied to Omarchy: agents, dev-gallery, tailscale, dropbox,
remote-session, reminders, speedtest, wifiqr, image-picker, background (wallpaper), monitor,
the passwordless-sudo and screen-recording indicators, system updates, and Omarchy's theme
system (replaced by `quattro-theme-set` with 23 bundled `colors.toml` files). Use your own
wallpaper tool (swaybg, swww, ...).

Replaced to make it universal:

- The `omarchy` namespace became `quattro` throughout; `OMARCHY_PATH` is gone.
- `Commons/Compositor.qml` and `bin/quattro-compositor` are the single place that knows
  which compositor is running.
- The Hyprland-bound dock became a plain launcher (`AppGrid` only).
- The menu was rewritten (`default/quattro/quattro-menu.jsonc`): Apps, Trigger, Appearance,
  Setup, Restart, System.
- Weather location: `geo.omarchy.org` was replaced by `quattro-geolocate` (ipwho.is, then
  ipapi.co), which sends your public IP. Set a place by hand with
  `quattro-weather-location --set Name lat,lon` to avoid it.
- Night light: `quattro-nightlight` picks hyprsunset, wlsunset or gammastep.
- Lock screen PAM files are generated from your distribution's own stack.

## Untested / things to check

1. The whole shell has not been run; QML errors cannot be caught here (no `qmllint`).
2. The `jq` filters in `quattro-compositor` (sway/niri) have not been executed.
3. The click-catcher layer that closes popups outside Hyprland may stack differently on some
   compositors.
4. `quattro-nightlight` with wlsunset/gammastep uses a fixed-temperature pattern that has not
   been verified.
5. On Hyprland with a Lua config, workspace switching tries `workspace N` and then the
   `hl.dsp.focus` expression; the "glassy" blur is not configured automatically (add a
   `layerrule` for the `quattro-bar` namespace yourself).
6. On Hyprland the Workspaces widget no longer groups workspaces per monitor in blocks of ten
   as Omarchy did.
