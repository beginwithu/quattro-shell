#!/bin/bash

# Quattro shell installer. Works from the folder it sits in; nothing is copied
# elsewhere except small symlinks and per-user config.
#
#   ./install.sh                 check dependencies, link commands, set up config and a theme
#   ./install.sh --lock          also install the PAM service the lock screen needs (sudo)
#   ./install.sh --sleep-lock    also lock the screen before suspend (user unit + logind drop-in, sudo)
#   ./install.sh --check         only report missing dependencies
#   ./install.sh --uninstall     remove the symlinks and units (your ~/.config/quattro stays)
#   --theme NAME                 start theme (default: tokyo-night)
#   --bin-dir DIR                where to link commands (default: /usr/local/bin, via sudo)

set -euo pipefail

ROOT=$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)
BIN_DIR=/usr/local/bin
LEGACY_BIN_DIR="$HOME/.local/bin"   # where earlier versions linked the commands
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/quattro"
STATE_DIR="${XDG_STATE_HOME:-$HOME/.local/state}/quattro"
THEME=tokyo-night
DO_LOCK=0 DO_SLEEP=0 CHECK_ONLY=0 UNINSTALL=0

# The commands people type or bind to keys. The rest are found through PATH by
# quattro-launch-shell, so they do not need to be on yours.
LINKED=(quattro-shell quattro-menu quattro-launch-shell quattro-restart-shell quattro-bar
  quattro-theme-set quattro-language quattro-audio-output-volume quattro-brightness-display
  quattro-brightness-keyboard quattro-toggle-nightlight quattro-capture-screenshot
  quattro-apply-lock quattro-system-lock quattro-system quattro-compositor quattro-osd
  quattro-notification-send quattro-weather-location)

while (( $# )); do
  case $1 in
    --lock) DO_LOCK=1 ;;
    --sleep-lock) DO_SLEEP=1 ;;
    --check) CHECK_ONLY=1 ;;
    --uninstall) UNINSTALL=1 ;;
    --theme) THEME=${2:?--theme needs a name}; shift ;;
    --bin-dir) BIN_DIR=${2:?--bin-dir needs a path}; shift ;;
    -h|--help) sed -n '3,12p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
    *) echo "Unknown option: $1" >&2; exit 1 ;;
  esac
  shift
done

# Config and theme belong to the calling user, so root would set up the wrong home.
if (( EUID == 0 && !CHECK_ONLY )); then
  echo "Run this as your normal user, not root; it calls sudo only where it needs to." >&2
  exit 1
fi

say() { printf '%s\n' "$*"; }

have() { command -v "$1" >/dev/null 2>&1; }

# Run a command that changes directory $1, through sudo unless we can write there.
privileged() {
  local dir=$1; shift
  if [[ -w $dir ]]; then "$@"; else sudo "$@"; fi
}

# Remove the links in $1 that point into this checkout's bin/, in one call.
remove_links() {
  local dir=$1 c old=()
  for c in "${LINKED[@]}"; do
    if [[ -L $dir/$c && $(readlink -f "$dir/$c") == "$ROOT/bin/"* ]]; then old+=("$dir/$c"); fi
  done
  if (( ${#old[@]} )); then
    privileged "$dir" rm -f -- "${old[@]}"
    say "Removed ${#old[@]} link(s) from $dir"
  fi
}

check_dependencies() {
  local missing_required=0
  say "Required:"
  for pair in "quickshell:quickshell" "jq:jq" "curl:curl"; do
    if have "${pair%%:*}"; then say "  ok       ${pair%%:*}"
    else say "  MISSING  ${pair%%:*}"; missing_required=1; fi
  done

  say "Optional, by feature:"
  opt() { # command package feature
    if have "$1"; then say "  ok       $1"; else say "  missing  $1  ($3; package: $2)"; fi
  }
  opt wpctl            wireplumber        "audio panel and volume keys"
  opt pactl            libpulse           "audio device switching"
  opt nmcli            networkmanager     "network panel"
  opt bluetoothctl     bluez-utils        "bluetooth panel"
  opt upower           upower             "battery"
  opt powerprofilesctl power-profiles-daemon "power profiles"
  opt brightnessctl    brightnessctl      "screen and keyboard brightness"
  opt wl-copy          wl-clipboard       "clipboard history"
  opt wtype            wtype              "pasting from clipboard and emoji pickers"
  opt grim             grim               "screenshots"
  opt slurp            slurp              "region screenshots"
  opt iw               iw                 "Wi-Fi band switching"
  opt notify-send      libnotify          "scripts that raise notifications"
  if have hyprsunset || have wlsunset || have gammastep; then say "  ok       night light backend"
  else say "  missing  hyprsunset | wlsunset | gammastep  (night light)"; fi
  if fc-list 2>/dev/null | grep -qi "nerd"; then say "  ok       a Nerd Font"
  else say "  missing  a Nerd Font  (icons; package: ttf-jetbrains-mono-nerd)"; fi
  if have pacman; then
    say ""
    say "On Arch: sudo pacman -S quickshell jq curl wireplumber libpulse networkmanager bluez-utils upower \\"
    say "          power-profiles-daemon brightnessctl wl-clipboard wtype grim slurp iw libnotify wlsunset ttf-jetbrains-mono-nerd"
  fi
  return $missing_required
}

if (( UNINSTALL )); then
  remove_links "$BIN_DIR"
  [[ $BIN_DIR == "$LEGACY_BIN_DIR" ]] || remove_links "$LEGACY_BIN_DIR"
  systemctl --user disable --now quattro-sleep-lock.service 2>/dev/null || true
  rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/quattro-sleep-lock.service"
  say "Removed links and units. Run ./bin/quattro-apply-lock --remove to drop the PAM files; your config in $CONFIG_DIR is untouched."
  exit 0
fi

if ! check_dependencies; then
  say ""; say "Install the missing required packages and run this again."
  exit 1
fi
if (( CHECK_ONLY )); then exit 0; fi

chmod +x "$ROOT"/bin/* 2>/dev/null || true

say ""
say "Linking commands into $BIN_DIR"
targets=()
for c in "${LINKED[@]}"; do
  if [[ -e $ROOT/bin/$c ]]; then targets+=("$ROOT/bin/$c"); fi
done
privileged "$BIN_DIR" mkdir -p "$BIN_DIR"
privileged "$BIN_DIR" ln -sf -t "$BIN_DIR" -- "${targets[@]}"
[[ $BIN_DIR == "$LEGACY_BIN_DIR" ]] || remove_links "$LEGACY_BIN_DIR"
case ":$PATH:" in *":$BIN_DIR:"*) ;; *) say "  note: $BIN_DIR is not on your PATH; add it to your shell profile." ;; esac

say "Setting up $CONFIG_DIR"
mkdir -p "$CONFIG_DIR"
if [[ ! -e $CONFIG_DIR/env ]]; then
  cat > "$CONFIG_DIR/env" <<'ENV'
# Environment for the Quattro shell, read by quattro-launch-shell. Examples:
#QUATTRO_COMPOSITOR=generic          # hyprland | sway | niri | generic (default: detected)
#QUATTRO_TERMINAL=foot               # terminal for TUIs and prompts
#QUATTRO_WORKSPACES_CMD=my-tags-feed # prints workspace JSON lines (see README)
#QUATTRO_WORKSPACE_FOCUS_CMD='my-focus {id}'
ENV
fi

if [[ ! -e $STATE_DIR/current/theme/colors.toml ]]; then
  QUATTRO_PATH=$ROOT "$ROOT/bin/quattro-theme-set" "$THEME" && say "Theme: $THEME"
fi

if (( DO_LOCK )); then
  "$ROOT/bin/quattro-apply-lock"
else
  say "Lock screen: run './install.sh --lock' once (needs sudo) or locking will refuse to start."
fi

if (( DO_SLEEP )); then
  unit_dir="${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user"
  mkdir -p "$unit_dir"
  sed "s|@QUATTRO_PATH@|$ROOT|g" "$ROOT/default/systemd/user/quattro-sleep-lock.service" > "$unit_dir/quattro-sleep-lock.service"
  sudo install -Dm644 "$ROOT/etc/systemd/logind.conf.d/20-quattro-inhibit-delay.conf" /etc/systemd/logind.conf.d/20-quattro-inhibit-delay.conf
  systemctl --user daemon-reload
  systemctl --user enable --now quattro-sleep-lock.service
  say "Sleep lock enabled. Restart systemd-logind (or reboot) for the longer inhibit window to apply."
fi

say ""
say "Done. Start the shell from your compositor's autostart with:"
say "    $ROOT/bin/quattro-launch-shell"
say "or try it now from a Wayland session:  quattro-launch-shell &"
