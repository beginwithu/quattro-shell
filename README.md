# Quattro shell

A Quickshell desktop shell for **Hyprland** (bar, panels, notifications, launcher, OSD, lock screen,
polkit agent) taken from Omarchy and detached from it. You do not need Omarchy: Hyprland, Quickshell,
and a few ordinary packages are enough. MIT licensed, original copyright David Heinemeier Hansson
(see `LICENSE`).

## Install

```sh
git clone <this folder> ~/quattro-shell   # or extract the archive
cd ~/quattro-shell
./install.sh --lock                       # --lock installs the PAM files (sudo), once
```

Start the shell from Hyprland's autostart, not from anywhere else, because this script sets up
`QUATTRO_PATH` and `PATH` and requires an active Hyprland session:

```sh
~/quattro-shell/bin/quattro-launch-shell
```

The installer supports Arch Linux and its derivatives (EndeavourOS, CachyOS, Manjaro and the
like) and refuses to run elsewhere. It lists the missing packages, all from the official
repositories, and installs them with pacman after you confirm. If pacman cannot find a package,
run `sudo pacman -Syu` and try again; the installer never runs a partial `-Sy`.

Installer options: `--check` (dependency report only), `--no-deps` (do not install anything),
`-y`/`--yes` (do not ask before installing), `--sleep-lock` (lock before suspend),
`--theme NAME`, `--bin-dir DIR`, `--uninstall`. Commands are linked into `/usr/local/bin` (sudo asks
once); the checkout must stay where it is, because the links point into it. Run it as your normal
user, not as root. `--bin-dir ~/.local/bin` keeps the per-user behaviour. Themes:
`quattro-theme-set list` / `quattro-theme-set nord`.

`quattro-apply-lock` builds the lock screen's PAM service on `system-auth` (Arch, Fedora,
openSUSE, Void) or `common-auth` (Debian, Ubuntu).

## Hyprland configuration

Quattro runs exclusively on Hyprland and uses `Quickshell.Hyprland` natively for workspaces,
monitors, focus grab, global shortcuts, keyboard layouts, and rounding/gap synchronisation.

`default/hypr` holds the key bindings, window rules, and settings for Hyprland's Lua
configuration. In `~/.config/hypr/hyprland.lua`:

```lua
dofile(os.getenv("HOME") .. "/quattro-shell/default/hypr/bootstrap.lua")
require("default.hypr.quattro")
```

The checkout is found from `QUATTRO_PATH`, or from where `bootstrap.lua` was loaded. Add your own
bindings after those two lines with `o.bind("SUPER + X", "Description", "command")`. `SUPER + K`
opens a searchable list of the bindings Hyprland has loaded (Menu → Keybindings).

If you use a traditional `hyprland.conf` instead of Lua, start `quattro-launch-shell` via
`exec-once` and bind keys to these commands (or to the `quattro:*` `GlobalShortcut` names listed
in `default/quattro/shortcuts`):

```sh
quattro-menu                                  # main menu
quattro-shell quattro.dock apps               # application launcher
quattro-shell shell toggle quattro.clipboard  # clipboard history (also quattro.emojis, quattro.audio, ...)
quattro-audio-output-volume raise|lower|mute-toggle
quattro-brightness-display +5%                # or 5%-
quattro-toggle-nightlight
quattro-system-lock
quattro-capture-screenshot [--full]           # also -screenrecording [--with-desktop-audio], -text (OCR), -qr
```

The number of always-visible numbered workspace slots is set in `shell.json`:
`{ "id": "quattro.workspaces", "slots": 7 }`.

## Configuration

- `~/.config/quattro/shell.json`: bar and widget layout. Defaults: `config/quattro/shell.json`.
  Change it from the menu (Appearance) or with `quattro-bar position bottom` and
  `quattro-bar set quattro.clock format HH:mm`.
- `~/.config/quattro/shell.toml`: per-surface typography, spacing, controls, and colors. Corner
  radius and gaps are read directly from Hyprland (`decoration:rounding` and `general:gaps_out`).
- `~/.config/quattro/env`: environment for the shell (`QUATTRO_TERMINAL`, `QUATTRO_MENU_FONT`, ...).
- Extra menu entries: `~/.config/quattro/extensions/quattro-menu.jsonc`.
- Language: `quattro-language <code>` (`quattro-language list` shows the codes).

## What changed from Omarchy

Removed because they are tied to Omarchy: agents, dev-gallery, tailscale, dropbox,
remote-session, reminders, speedtest, wifiqr, image-picker, background (wallpaper), monitor,
the passwordless-sudo and screen-recording indicators, system updates, and Omarchy's theme
system (replaced by `quattro-theme-set` with 23 bundled `colors.toml` files). Use your own
wallpaper tool (`hyprpaper`, `swww`, `swaybg`, ...).

Adjusted for standalone Hyprland use:

- The `omarchy` namespace became `quattro` throughout; `OMARCHY_PATH` is gone.
- The dock became a clean application launcher (`AppGrid` only).
- The menu was rewritten (`default/quattro/quattro-menu.jsonc`): Apps, Trigger (clipboard, emoji,
  capture, toggles, share), Appearance (theme, bar), Setup (language, DNS, config), Restart, System.
  Entries keep Omarchy's ids, so the translations in `default/i18n` apply. Screen recording, text
  and QR capture, the color picker and LocalSend appear only when `wf-recorder`, `tesseract`,
  `zbarimg`, `hyprpicker` or `localsend` is installed.
- Weather location: `geo.omarchy.org` was replaced by `quattro-geolocate` (ipwho.is, then
  ipapi.co), which sends your public IP. Set a place by hand with
  `quattro-weather-location --set Name lat,lon` to avoid it.
- Night light: `quattro-nightlight` drives `hyprsunset`.
- Lock screen PAM files are generated from your distribution's own stack.
