#!/bin/bash

set -euo pipefail

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

if [[ ${1:-} == "-h" || ${1:-} == "--help" ]]; then
  cat <<'EOF'
Usage: ./install.sh

Installs minimal Quattro core packages (Arch [core]/[extra]), runtime, user
configs, Tokyo Night theme, and system-wide session/environment files.
No flags needed.
EOF
  exit 0
fi

if (( $# > 0 )); then
  echo "Usage: ./install.sh (runs full install with no flags)" >&2
  exit 1
fi

SANDBOX=false
if [[ ${QUATTRO_INSTALL_SANDBOX:-0} == "1" ]]; then
  SANDBOX=true
elif (( EUID == 0 )); then
  echo "Run ./install.sh as your normal user (it will prompt for sudo when needed), not as root." >&2
  exit 1
fi

THEME="Tokyo Night"
INSTALL_DIR="$HOME/.local/share/quattro"
BACKUP_DIR="$HOME/.local/state/quattro-arch/backups/$(date +%Y%m%d-%H%M%S)"
BIN_LINK_DIR="$HOME/.local/bin"

backup_path() {
  local target="$1"
  if [[ -e $target || -L $target ]]; then
    local rel="${target#$HOME/}"
    local dest="$BACKUP_DIR/$rel"
    mkdir -p "$(dirname "$dest")"
    cp -a "$target" "$dest"
  fi
}

install_packages() {
  local -a pkgs=()
  mapfile -t pkgs < <(grep -hvE '^[[:space:]]*(#|$)' "$PROJECT_DIR/packages/arch-core.packages")

  if (( ${#pkgs[@]} > 0 )); then
    echo "==> Installing core Arch packages via pacman..."
    sudo pacman -S --needed --noconfirm "${pkgs[@]}"
  fi
}

install_runtime() {
  echo "==> Installing Quattro runtime to $INSTALL_DIR..."
  mkdir -p "$INSTALL_DIR"

  for item in applications bin default install shell themes icon.png icon.txt logo.svg logo.txt version LICENSE; do
    if [[ -e $PROJECT_DIR/$item ]]; then
      rm -rf "$INSTALL_DIR/$item"
      cp -a "$PROJECT_DIR/$item" "$INSTALL_DIR/$item"
    fi
  done

  echo "==> Linking quattro-* commands into $BIN_LINK_DIR..."
  mkdir -p "$BIN_LINK_DIR"
  for bin_file in "$PROJECT_DIR"/bin/*; do
    local name="${bin_file##*/}"
    ln -snf "$INSTALL_DIR/bin/$name" "$BIN_LINK_DIR/$name"
  done
}

install_user_configs() {
  echo "==> Deploying ~/.config from $PROJECT_DIR/config (backups -> $BACKUP_DIR)..."
  mkdir -p "$HOME/.config"

  local entry name target
  for entry in "$PROJECT_DIR"/config/* "$PROJECT_DIR"/config/.*; do
    name="${entry##*/}"
    [[ $name == "." || $name == ".." ]] && continue
    [[ -e $entry ]] || continue
    target="$HOME/.config/$name"

    if [[ -d $entry ]]; then
      mkdir -p "$target"
      # Copy sub-items individually so existing user files outside Quattro defaults are backed up
      while IFS= read -r -d '' src_file; do
        local rel="${src_file#$entry/}"
        local dst_file="$target/$rel"
        backup_path "$dst_file"
        mkdir -p "$(dirname "$dst_file")"
        cp -a "$src_file" "$dst_file"
      done < <(find "$entry" \( -type f -o -type l \) -print0)
    else
      backup_path "$target"
      cp -a "$entry" "$target"
    fi
  done

  # Resolve __QUATTRO_PATH__ in chrome/chromium flags
  for flags_file in "$HOME/.config/chrome-flags.conf" "$HOME/.config/chromium-flags.conf"; do
    if [[ -f $flags_file ]]; then
      sed -i "s|__QUATTRO_PATH__|$INSTALL_DIR|g" "$flags_file"
    fi
  done

  # Enable hyprland-preview-share-picker automatically if installed
  if command -v hyprland-preview-share-picker >/dev/null 2>&1 && [[ -f $HOME/.config/hypr/xdph.conf ]]; then
    sed -i 's|^    # custom_picker_binary = hyprland-preview-share-picker|    custom_picker_binary = hyprland-preview-share-picker|' "$HOME/.config/hypr/xdph.conf"
  fi

  # Seed ~/.bashrc
  backup_path "$HOME/.bashrc"
  cp -a "$PROJECT_DIR/default/bashrc" "$HOME/.bashrc"

  # Seed ~/.XCompose if missing
  if [[ ! -f $HOME/.XCompose ]]; then
    cat >"$HOME/.XCompose" <<EOF
# Run quattro-restart-xcompose to apply changes

# Include fast emoji access
include "$INSTALL_DIR/default/xcompose"
EOF
  fi

  # Seed branding files for fastfetch & screensaver
  mkdir -p "$HOME/.config/quattro/branding" "$HOME/.config/quattro/themes"
  [[ -f $HOME/.config/quattro/branding/about.txt ]] || cp -a "$PROJECT_DIR/logo.txt" "$HOME/.config/quattro/branding/about.txt"
  [[ -f $HOME/.config/quattro/branding/screensaver.txt ]] || cp -a "$PROJECT_DIR/icon.txt" "$HOME/.config/quattro/branding/screensaver.txt"

  # Seed Hyprland toggles directory (only flags.lua; individual toggles opt in when activated)
  mkdir -p "$HOME/.local/state/quattro/toggles/hypr"
  [[ -e $HOME/.local/state/quattro/toggles/hypr/flags.lua ]] || cp -a "$PROJECT_DIR/default/hypr/toggles/flags.lua" "$HOME/.local/state/quattro/toggles/hypr/flags.lua"
  # Clean up contradictory toggle pairs if an earlier install copied every toggle file
  if [[ -f $HOME/.local/state/quattro/toggles/hypr/window-glassy.lua && -f $HOME/.local/state/quattro/toggles/hypr/window-opaque.lua ]]; then
    rm -f "$HOME/.local/state/quattro/toggles/hypr/window-glassy.lua" "$HOME/.local/state/quattro/toggles/hypr/window-opaque.lua"
  fi
  if [[ -f $HOME/.local/state/quattro/toggles/hypr/corners-rounded.lua && -f $HOME/.local/state/quattro/toggles/hypr/corners-sharp.lua ]]; then
    rm -f "$HOME/.local/state/quattro/toggles/hypr/corners-rounded.lua" "$HOME/.local/state/quattro/toggles/hypr/corners-sharp.lua"
  fi

  # Seed UWSM user env drop-in & xdg-terminal-exec list
  mkdir -p "$HOME/.config/uwsm/env.d" "$HOME/.config/xdg-terminal-exec" "$HOME/.config/environment.d"
  cp -a "$PROJECT_DIR/default/uwsm/env.d/10-quattro" "$HOME/.config/uwsm/env.d/10-quattro"
  cp -a "$PROJECT_DIR/default/xdg-terminal-exec/hyprland-xdg-terminals.list" "$HOME/.config/xdg-terminal-exec/hyprland-xdg-terminals.list"
  cp -a "$PROJECT_DIR/default/environment.d/10-quattro-fcitx.conf" "$HOME/.config/environment.d/10-quattro-fcitx.conf"

  cat >"$HOME/.config/environment.d/20-quattro-path.conf" <<EOF
QUATTRO_PATH=$INSTALL_DIR
PATH=$BIN_LINK_DIR:$INSTALL_DIR/bin:\$PATH
EOF

  # Install desktop entries, icons, and fonts to ~/.local/share
  mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/fonts/quattro" "$HOME/.config/fontconfig/conf.d"
  cp -a "$PROJECT_DIR"/applications/*.desktop "$HOME/.local/share/applications/"
  cp -a "$PROJECT_DIR/default/fonts/quattro/quattro.ttf" "$HOME/.local/share/fonts/quattro/quattro.ttf"
  cp -a "$PROJECT_DIR/default/fontconfig/conf.avail/50-quattro.conf" "$HOME/.config/fontconfig/conf.d/50-quattro.conf"

  if [[ -d $PROJECT_DIR/applications/icons ]]; then
    mkdir -p "$HOME/.local/share/icons/hicolor/256x256/apps"
    cp -a "$PROJECT_DIR/applications/icons/." "$HOME/.local/share/icons/hicolor/256x256/apps/"
    cp -a "$PROJECT_DIR/icon.png" "$HOME/.local/share/icons/hicolor/256x256/apps/quattro.png"
  fi

  # User-level XKB option (Shift + CapsLock Compose), fastfetch, and mpv defaults
  mkdir -p "$HOME/.config/xkb/rules" "$HOME/.config/xkb/symbols"
  cp -a "$PROJECT_DIR/etc/xkb/rules/evdev" "$HOME/.config/xkb/rules/evdev"
  cp -a "$PROJECT_DIR/etc/xkb/symbols/quattro" "$HOME/.config/xkb/symbols/quattro"

  if [[ ! -f $HOME/.config/fastfetch/config.jsonc ]]; then
    backup_path "$HOME/.config/fastfetch/config.jsonc"
    mkdir -p "$HOME/.config/fastfetch"
    cp -a "$PROJECT_DIR/etc/fastfetch/config.jsonc" "$HOME/.config/fastfetch/config.jsonc"
  fi

  if [[ ! -f $HOME/.config/mpv/mpv.conf ]]; then
    backup_path "$HOME/.config/mpv/mpv.conf"
    mkdir -p "$HOME/.config/mpv"
    cp -a "$PROJECT_DIR/etc/mpv/mpv.conf" "$HOME/.config/mpv/mpv.conf"
  fi

  # Install user systemd units
  mkdir -p "$HOME/.config/systemd/user/wireplumber.service.d"
  for unit in "$PROJECT_DIR"/default/systemd/user/*.service; do
    cp -a "$unit" "$HOME/.config/systemd/user/"
  done
  cp -a "$PROJECT_DIR/default/systemd/user/wireplumber.service.d/"*.conf "$HOME/.config/systemd/user/wireplumber.service.d/"

  # Install default mimeapps if user doesn't have one yet
  if [[ ! -f $HOME/.config/mimeapps.list ]]; then
    cp -a "$PROJECT_DIR/default/applications/mimeapps.list" "$HOME/.config/mimeapps.list"
  fi

  # Refresh font cache
  if command -v fc-cache >/dev/null 2>&1; then
    fc-cache -f "$HOME/.local/share/fonts" || true
  fi
}

generate_initial_theme() {
  echo "==> Generating initial theme ($THEME)..."
  export QUATTRO_PATH="$INSTALL_DIR"
  export PATH="$BIN_LINK_DIR:$INSTALL_DIR/bin:$PATH"
  export QUATTRO_THEME_HEADLESS=1

  "$INSTALL_DIR/bin/quattro-theme-set" "$THEME"
  mkdir -p "$HOME/.config/btop/themes"
  ln -snf "$HOME/.local/state/quattro/current/theme/btop.theme" "$HOME/.config/btop/themes/current.theme"

  if command -v quattro-theme-set-qt >/dev/null 2>&1; then
    quattro-theme-set-qt || true
  fi
}

install_system_configs() {
  echo "==> Installing system-wide Arch integration (requires sudo)..."
  sudo mkdir -p \
    /usr/local/share/wayland-sessions \
    /etc/profile.d \
    /usr/share/uwsm/env.d \
    /etc/fastfetch \
    /etc/mpv \
    /etc/xdg/kitty \
    /etc/xkb/rules \
    /etc/xkb/symbols \
    /etc/fonts/conf.d \
    /etc/systemd/user/wayland-wm@hyprland.desktop.service.d

  sudo cp -a "$PROJECT_DIR/default/wayland-sessions/quattro.desktop" /usr/local/share/wayland-sessions/quattro.desktop
  sudo cp -a "$PROJECT_DIR/etc/profile.d/quattro.sh" /etc/profile.d/quattro.sh
  sudo cp -a "$PROJECT_DIR/default/uwsm/env.d/10-quattro" /usr/share/uwsm/env.d/10-quattro
  sudo cp -a "$PROJECT_DIR/etc/fastfetch/config.jsonc" /etc/fastfetch/config.jsonc
  sudo cp -a "$PROJECT_DIR/etc/mpv/mpv.conf" /etc/mpv/mpv.conf
  sudo cp -a "$PROJECT_DIR/etc/xdg/kitty/kitty.conf" /etc/xdg/kitty/kitty.conf
  sudo cp -a "$PROJECT_DIR/etc/xkb/rules/evdev" /etc/xkb/rules/evdev
  sudo cp -a "$PROJECT_DIR/etc/xkb/symbols/quattro" /etc/xkb/symbols/quattro
  sudo cp -a "$PROJECT_DIR/default/fontconfig/conf.avail/50-quattro.conf" /etc/fonts/conf.d/50-quattro.conf
  sudo cp -a "$PROJECT_DIR/etc/systemd/user/wayland-wm@hyprland.desktop.service.d/10-keep-shell.conf" /etc/systemd/user/wayland-wm@hyprland.desktop.service.d/10-keep-shell.conf

  printf 'export QUATTRO_PATH="%s"\n' "$INSTALL_DIR" | sudo tee /etc/quattro.conf >/dev/null
}

if ! $SANDBOX; then
  install_packages
fi

install_runtime
install_user_configs
generate_initial_theme

if ! $SANDBOX; then
  install_system_configs
fi

cat <<EOF

Done! Quattro installed to: $INSTALL_DIR

Next steps:
  1. Open a new bash login shell (or run: source ~/.bashrc)
  2. Switch themes anytime with:
     quattro-theme-set "Tokyo Night"   # or catppuccin, gruvbox, kanagawa, nord, etc.
  3. Log in via SDDM (Quattro (Hyprland uwsm)) or start from TTY:
     uwsm start hyprland.desktop
EOF
