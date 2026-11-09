#!/bin/bash

# Quattro shell installer. Works from the folder it sits in; nothing is copied
# elsewhere except small symlinks and per-user config.
#
#   ./install.sh                 install missing dependencies, link commands, set up config and a theme
#   ./install.sh --lock          also install the PAM service the lock screen needs (sudo)
#   ./install.sh --sleep-lock    also lock the screen before suspend (user unit + logind drop-in, sudo)
#   ./install.sh --check         only report missing dependencies
#   --no-deps                    do not install dependencies, only check them
#   -y, --yes                    install dependencies without asking first
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
DO_LOCK=0 DO_SLEEP=0 CHECK_ONLY=0 UNINSTALL=0 NO_DEPS=0 ASSUME_YES=0

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
    --no-deps) NO_DEPS=1 ;;
    -y|--yes) ASSUME_YES=1 ;;
    --uninstall) UNINSTALL=1 ;;
    --theme) THEME=${2:?--theme needs a name}; shift ;;
    --bin-dir) BIN_DIR=${2:?--bin-dir needs a path}; shift ;;
    -h|--help) sed -n '3,14p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0 ;;
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

# tier|command|used for|pacman|apt|dnf|zypper|xbps
# req and rec are installed automatically, opt is only reported. A package name
# that does not exist on a distribution is reported when it fails, not fatal.
DEPS=(
  "req|quickshell|the shell itself|quickshell|quickshell|quickshell|quickshell|quickshell"
  "req|jq|JSON handling|jq|jq|jq|jq|jq"
  "req|curl|weather and geolocation|curl|curl|curl|curl|curl"
  "rec|wpctl|audio panel and volume keys|wireplumber|wireplumber|wireplumber|wireplumber|wireplumber"
  "rec|pactl|audio device switching|libpulse|pulseaudio-utils|pulseaudio-utils|pulseaudio-utils|pulseaudio-utils"
  "rec|nmcli|network panel|networkmanager|network-manager|NetworkManager|NetworkManager|NetworkManager"
  "rec|bluetoothctl|bluetooth panel|bluez-utils|bluez|bluez|bluez|bluez"
  "rec|upower|battery|upower|upower|upower|upower|upower"
  "rec|powerprofilesctl|power profiles|power-profiles-daemon|power-profiles-daemon|power-profiles-daemon|power-profiles-daemon|power-profiles-daemon"
  "rec|brightnessctl|screen and keyboard brightness|brightnessctl|brightnessctl|brightnessctl|brightnessctl|brightnessctl"
  "rec|wl-copy|clipboard history|wl-clipboard|wl-clipboard|wl-clipboard|wl-clipboard|wl-clipboard"
  "rec|wtype|pasting from clipboard and emoji pickers|wtype|wtype|wtype|wtype|wtype"
  "rec|grim|screenshots|grim|grim|grim|grim|grim"
  "rec|slurp|region screenshots|slurp|slurp|slurp|slurp|slurp"
  "rec|iw|Wi-Fi band switching|iw|iw|iw|iw|iw"
  "rec|notify-send|scripts that raise notifications|libnotify|libnotify-bin|libnotify|libnotify-tools|libnotify"
  "rec|socat|fast IPC for key bindings|socat|socat|socat|socat|socat"
  "opt|ddcutil|external monitor brightness|ddcutil|ddcutil|ddcutil|ddcutil|ddcutil"
  "opt|magick|bar text color from the background|imagemagick|imagemagick|ImageMagick|ImageMagick|ImageMagick"
  "opt|fprintd-list|fingerprint unlock|fprintd|fprintd|fprintd|fprintd|fprintd"
)
NERD_FONT_PKG=ttf-jetbrains-mono-nerd   # pacman only; elsewhere the font is downloaded
NERD_FONT_URL=https://github.com/ryanoasis/nerd-fonts/releases/latest/download/JetBrainsMono.tar.xz

PM="" col=0
for pm in pacman apt-get dnf zypper xbps-install; do
  if have "$pm"; then PM=$pm; break; fi
done
case $PM in
  pacman) col=3 ;; apt-get) col=4 ;; dnf) col=5 ;; zypper) col=6 ;; xbps-install) col=7 ;;
esac

missing_req=() missing_pkgs=() need_nightlight=0 need_font=0

scan_dependencies() {
  local row f
  missing_req=() missing_pkgs=() need_nightlight=0 need_font=0
  for row in "${DEPS[@]}"; do
    IFS='|' read -ra f <<<"$row"
    have "${f[1]}" && continue
    [[ ${f[0]} == req ]] && missing_req+=("${f[1]}")
    [[ ${f[0]} != opt && -n $PM ]] && missing_pkgs+=("${f[col]}")
  done
  if ! have hyprsunset && ! have wlsunset && ! have gammastep; then
    need_nightlight=1
    [[ -n $PM ]] && missing_pkgs+=(wlsunset)
  fi
  if [[ $(fc-list : family 2>/dev/null) != *[Nn]erd* ]]; then
    need_font=1
    [[ $PM == pacman ]] && missing_pkgs+=("$NERD_FONT_PKG")
  fi
  # Unique, in a stable order.
  if (( ${#missing_pkgs[@]} )); then
    mapfile -t missing_pkgs < <(printf '%s\n' "${missing_pkgs[@]}" | sort -u)
  fi
  return 0
}

report_dependencies() {
  local tier row f
  for tier in req rec opt; do
    case $tier in
      req) say "Required:" ;;
      rec) say "Recommended (installed automatically):" ;;
      opt) say "Optional:" ;;
    esac
    for row in "${DEPS[@]}"; do
      IFS='|' read -ra f <<<"$row"
      [[ ${f[0]} == "$tier" ]] || continue
      if have "${f[1]}"; then say "  ok       ${f[1]}"
      else say "  missing  ${f[1]}  (${f[2]}${PM:+; package: ${f[col]}})"; fi
    done
    if [[ $tier == rec ]]; then
      if (( need_nightlight )); then say "  missing  hyprsunset | wlsunset | gammastep  (night light)"
      else say "  ok       night light backend"; fi
      if (( need_font )); then say "  missing  a Nerd Font  (icons)"
      else say "  ok       a Nerd Font"; fi
    fi
  done
}

confirm() {
  (( ASSUME_YES )) && return 0
  [[ -t 0 ]] || return 1
  local reply
  read -r -p "$1 [Y/n] " reply
  [[ -z $reply || $reply == [Yy]* ]]
}

pm_install() {
  case $PM in
    pacman) sudo pacman -S --needed --noconfirm "$@" ;;
    apt-get) sudo apt-get install -y "$@" ;;
    dnf) sudo dnf install -y "$@" ;;
    zypper) sudo zypper --non-interactive install "$@" ;;
    xbps-install) sudo xbps-install -y "$@" ;;
  esac
}

# One transaction first. If any name is unknown the manager refuses the whole
# set, so fall back to one package at a time and report only the ones that fail.
install_packages() {
  local pkg failed=()
  if [[ $PM == apt-get ]]; then sudo apt-get update || say "apt-get update failed; continuing."; fi
  say "Installing: ${missing_pkgs[*]}"
  if pm_install "${missing_pkgs[@]}"; then return 0; fi
  say "The combined install failed; trying one package at a time."
  for pkg in "${missing_pkgs[@]}"; do
    pm_install "$pkg" || failed+=("$pkg")
  done
  if (( ${#failed[@]} )); then
    say "Could not install: ${failed[*]}"
    if [[ $PM == pacman ]]; then say "If downloads fail with 404, run 'sudo pacman -Syu' first and try again."; fi
  fi
  return 0
}

install_nerd_font() {
  local dir="${XDG_DATA_HOME:-$HOME/.local/share}/fonts/JetBrainsMonoNerd"
  if ! have curl || ! have tar; then say "Cannot download the Nerd Font: curl and tar are needed."; return 0; fi
  say "Downloading JetBrainsMono Nerd Font to $dir"
  mkdir -p "$dir"
  if curl -fsSL "$NERD_FONT_URL" | tar -xJ -C "$dir"; then
    if have fc-cache; then fc-cache -f "$dir"; fi
  else
    say "The font download failed (check the network, and that xz is installed)."
  fi
}

quickshell_hint() {
  say "quickshell is still missing:"
  case $PM in
    pacman) say "  sudo pacman -S quickshell   (run 'sudo pacman -Syu' first if it is not found)" ;;
    apt-get) say "  Debian: quickshell is in testing and unstable."
             say "  Ubuntu: sudo add-apt-repository ppa:avengemedia/danklinux, then sudo apt install quickshell" ;;
    dnf) say "  sudo dnf copr enable errornointernet/quickshell, then sudo dnf install quickshell" ;;
    zypper) say "  Packaged on the Open Build Service in home:AvengeMedia:danklinux." ;;
    *) say "  See the installation guide at https://quickshell.org" ;;
  esac
}

install_dependencies() {
  (( ${#missing_pkgs[@]} || need_font )) || return 0
  say ""
  if [[ -z $PM ]]; then
    say "No supported package manager found (pacman, apt, dnf, zypper, xbps); install the missing items above by hand."
  elif (( ${#missing_pkgs[@]} )); then
    if ! have sudo; then
      say "sudo is needed to install: ${missing_pkgs[*]}"
    elif confirm "Install ${missing_pkgs[*]} with $PM?"; then
      install_packages
    else
      say "Skipped. Re-run with --yes to install without asking, or install them yourself."
    fi
  fi
  if (( need_font )) && [[ $PM != pacman ]] && confirm "Download JetBrainsMono Nerd Font?"; then
    install_nerd_font
  fi
}

if (( UNINSTALL )); then
  remove_links "$BIN_DIR"
  [[ $BIN_DIR == "$LEGACY_BIN_DIR" ]] || remove_links "$LEGACY_BIN_DIR"
  systemctl --user disable --now quattro-sleep-lock.service 2>/dev/null || true
  rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/systemd/user/quattro-sleep-lock.service"
  say "Removed links and units. Run ./bin/quattro-apply-lock --remove to drop the PAM files; your config in $CONFIG_DIR is untouched."
  exit 0
fi

scan_dependencies
report_dependencies
if (( CHECK_ONLY )); then
  if (( ${#missing_req[@]} )); then exit 1; else exit 0; fi
fi
if (( !NO_DEPS )); then
  install_dependencies
  scan_dependencies
fi
if (( ${#missing_req[@]} )); then
  say ""
  say "Still missing: ${missing_req[*]}"
  if [[ " ${missing_req[*]} " == *" quickshell "* ]]; then quickshell_hint; fi
  say "Install the missing required packages and run this again."
  exit 1
fi

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
