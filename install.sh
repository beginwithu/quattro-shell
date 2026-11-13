#!/bin/bash

set -euo pipefail

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)

MODE="user"
WITH_PACKAGES=false
WITH_OPTIONAL=false
WITH_AUR=false
DRY_RUN=false
THEME="Tokyo Night"
PREFIX=""

usage() {
  cat <<'EOF'
Usage: ./install.sh [options]

Deploy the pure Arch Linux port of Quattro configs and runtime.

Modes:
  --user-only         Install runtime, bins, themes, and configs into $HOME only (default, no sudo)
  --system            Also install system integration (/etc/profile.d, uwsm, wayland-sessions, xkb, etc.)

Packages:
  --with-packages     Install required Arch [core]/[extra] packages via pacman
  --with-optional     Also install optional Arch [core]/[extra] desktop/dev packages (implies --with-packages)
  --with-aur          Also install optional AUR packages via yay (qtengine, google-chrome, etc.)

Options:
  --theme <name>      Initial theme to generate (default: "Tokyo Night")
  --prefix <dir>      Override Quattro runtime install directory (default: ~/.local/share/quattro)
  --dry-run           Print planned operations without making changes
  -h, --help          Show this help
EOF
}

while (( $# > 0 )); do
  case "$1" in
    --user-only) MODE="user"; shift ;;
    --system) MODE="system"; shift ;;
    --with-packages) WITH_PACKAGES=true; shift ;;
    --with-optional) WITH_PACKAGES=true; WITH_OPTIONAL=true; shift ;;
    --with-aur) WITH_AUR=true; shift ;;
    --theme)
      THEME="${2:-}"
      [[ -n $THEME ]] || { echo "Error: --theme requires a value" >&2; exit 1; }
      shift 2
      ;;
    --prefix)
      PREFIX="${2:-}"
      [[ -n $PREFIX ]] || { echo "Error: --prefix requires a value" >&2; exit 1; }
      shift 2
      ;;
    --dry-run) DRY_RUN=true; shift ;;
    -h|--help) usage; exit 0 ;;
    *) echo "Unknown option: $1" >&2; usage >&2; exit 1 ;;
  esac
done

INSTALL_DIR="${PREFIX:-$HOME/.local/share/quattro}"
BACKUP_DIR="$HOME/.local/state/quattro-arch/backups/$(date +%Y%m%d-%H%M%S)"
BIN_LINK_DIR="$HOME/.local/bin"

run() {
  if $DRY_RUN; then
    printf '[dry-run]'
    printf ' %q' "$@"
    printf '\n'
  else
    "$@"
  fi
}

backup_path() {
  local target="$1"
  if [[ -e $target || -L $target ]]; then
    local rel="${target#$HOME/}"
    local dest="$BACKUP_DIR/$rel"
    run mkdir -p "$(dirname "$dest")"
    run cp -a "$target" "$dest"
  fi
}

install_packages() {
  local -a pkgs=()
  mapfile -t pkgs < <(grep -hvE '^[[:space:]]*(#|$)' "$PROJECT_DIR/packages/arch-core.packages")

  if $WITH_OPTIONAL; then
    local -a opt_pkgs=()
    mapfile -t opt_pkgs < <(grep -hvE '^[[:space:]]*(#|$)' "$PROJECT_DIR/packages/arch-optional.packages")
    pkgs+=("${opt_pkgs[@]}")
  fi

  if (( ${#pkgs[@]} > 0 )); then
    echo "==> Installing Arch repository packages via pacman..."
    run sudo pacman -S --needed --noconfirm "${pkgs[@]}"
  fi

  if $WITH_AUR; then
    local -a aur_pkgs=()
    mapfile -t aur_pkgs < <(grep -hvE '^[[:space:]]*(#|$)' "$PROJECT_DIR/packages/aur.packages")
    if (( ${#aur_pkgs[@]} > 0 )); then
      if command -v yay >/dev/null 2>&1; then
        echo "==> Installing optional AUR packages via yay..."
        run yay -S --needed --noconfirm "${aur_pkgs[@]}"
      else
        echo "Warning: yay not found; skipping AUR packages (${aur_pkgs[*]})" >&2
      fi
    fi
  fi
}

install_runtime() {
  echo "==> Installing Quattro runtime to $INSTALL_DIR..."
  run mkdir -p "$INSTALL_DIR"

  for item in applications bin default install shell themes icon.png icon.txt logo.svg logo.txt version LICENSE; do
    if [[ -e $PROJECT_DIR/$item ]]; then
      run rm -rf "$INSTALL_DIR/$item"
      run cp -a "$PROJECT_DIR/$item" "$INSTALL_DIR/$item"
    fi
  done

  echo "==> Linking quattro-* commands into $BIN_LINK_DIR..."
  run mkdir -p "$BIN_LINK_DIR"
  for bin_file in "$PROJECT_DIR"/bin/*; do
    local name="${bin_file##*/}"
    run ln -snf "$INSTALL_DIR/bin/$name" "$BIN_LINK_DIR/$name"
  done
}

install_user_configs() {
  echo "==> Deploying ~/.config from $PROJECT_DIR/config (backups -> $BACKUP_DIR)..."
  run mkdir -p "$HOME/.config"

  local entry name target
  for entry in "$PROJECT_DIR"/config/* "$PROJECT_DIR"/config/.*; do
    name="${entry##*/}"
    [[ $name == "." || $name == ".." ]] && continue
    [[ -e $entry ]] || continue
    target="$HOME/.config/$name"

    if [[ -d $entry ]]; then
      run mkdir -p "$target"
      # Copy sub-items individually so existing user files outside Quattro's defaults are preserved in backup
      while IFS= read -r -d '' src_file; do
        local rel="${src_file#$entry/}"
        local dst_file="$target/$rel"
        backup_path "$dst_file"
        run mkdir -p "$(dirname "$dst_file")"
        run cp -a "$src_file" "$dst_file"
      done < <(find "$entry" \( -type f -o -type l \) -print0)
    else
      backup_path "$target"
      run cp -a "$entry" "$target"
    fi
  done

  # Resolve __QUATTRO_PATH__ in chrome/chromium flags
  for flags_file in "$HOME/.config/chrome-flags.conf" "$HOME/.config/chromium-flags.conf"; do
    if $DRY_RUN; then
      echo "[dry-run] sed -i s|__QUATTRO_PATH__|$INSTALL_DIR|g $flags_file"
    elif [[ -f $flags_file ]]; then
      sed -i "s|__QUATTRO_PATH__|$INSTALL_DIR|g" "$flags_file"
    fi
  done

  # Enable hyperland-preview-share-picker automatically if installed
  if command -v hyprland-preview-share-picker >/dev/null 2>&1 && [[ -f $HOME/.config/hypr/xdph.conf ]] && ! $DRY_RUN; then
    sed -i 's|^    # custom_picker_binary = hyprland-preview-share-picker|    custom_picker_binary = hyprland-preview-share-picker|' "$HOME/.config/hypr/xdph.conf"
  fi

  # Seed ~/.bashrc
  backup_path "$HOME/.bashrc"
  run cp -a "$PROJECT_DIR/default/bashrc" "$HOME/.bashrc"

  # Seed ~/.XCompose if missing
  if [[ ! -f $HOME/.XCompose ]]; then
    if $DRY_RUN; then
      echo "[dry-run] write $HOME/.XCompose"
    else
      cat >"$HOME/.XCompose" <<EOF
# Run quattro-restart-xcompose to apply changes

# Include fast emoji access
include "$INSTALL_DIR/default/xcompose"
EOF
    fi
  fi

  # Seed branding files for fastfetch & screensaver
  run mkdir -p "$HOME/.config/quattro/branding" "$HOME/.config/quattro/themes"
  [[ -f $HOME/.config/quattro/branding/about.txt ]] || run cp -a "$PROJECT_DIR/logo.txt" "$HOME/.config/quattro/branding/about.txt"
  [[ -f $HOME/.config/quattro/branding/screensaver.txt ]] || run cp -a "$PROJECT_DIR/icon.txt" "$HOME/.config/quattro/branding/screensaver.txt"

  # Seed Hyprland toggles
  run mkdir -p "$HOME/.local/state/quattro/toggles/hypr"
  for toggle in "$PROJECT_DIR"/default/hypr/toggles/*.lua; do
    local tname="${toggle##*/}"
    [[ -e $HOME/.local/state/quattro/toggles/hypr/$tname ]] || run cp -a "$toggle" "$HOME/.local/state/quattro/toggles/hypr/$tname"
  done

  # Seed UWSM user env drop-in & xdg-terminal-exec list
  run mkdir -p "$HOME/.config/uwsm/env.d" "$HOME/.config/xdg-terminal-exec" "$HOME/.config/environment.d"
  run cp -a "$PROJECT_DIR/default/uwsm/env.d/10-quattro" "$HOME/.config/uwsm/env.d/10-quattro"
  run cp -a "$PROJECT_DIR/default/xdg-terminal-exec/hyprland-xdg-terminals.list" "$HOME/.config/xdg-terminal-exec/hyprland-xdg-terminals.list"
  run cp -a "$PROJECT_DIR/default/environment.d/10-quattro-fcitx.conf" "$HOME/.config/environment.d/10-quattro-fcitx.conf"

  if $DRY_RUN; then
    echo "[dry-run] write $HOME/.config/environment.d/20-quattro-path.conf"
  else
    cat >"$HOME/.config/environment.d/20-quattro-path.conf" <<EOF
QUATTRO_PATH=$INSTALL_DIR
PATH=$BIN_LINK_DIR:$INSTALL_DIR/bin:\$PATH
EOF
  fi

  # Install desktop entries, icons, and fonts to ~/.local/share
  run mkdir -p "$HOME/.local/share/applications" "$HOME/.local/share/fonts/quattro" "$HOME/.config/fontconfig/conf.d"
  run cp -a "$PROJECT_DIR"/applications/*.desktop "$HOME/.local/share/applications/"
  run cp -a "$PROJECT_DIR/default/fonts/quattro/quattro.ttf" "$HOME/.local/share/fonts/quattro/quattro.ttf"
  run cp -a "$PROJECT_DIR/default/fontconfig/conf.avail/50-quattro.conf" "$HOME/.config/fontconfig/conf.d/50-quattro.conf"

  if [[ -d $PROJECT_DIR/applications/icons ]]; then
    run mkdir -p "$HOME/.local/share/icons/hicolor/256x256/apps"
    run cp -a "$PROJECT_DIR/applications/icons/." "$HOME/.local/share/icons/hicolor/256x256/apps/"
    run cp -a "$PROJECT_DIR/icon.png" "$HOME/.local/share/icons/hicolor/256x256/apps/quattro.png"
  fi

  # User-level XKB option (Shift + CapsLock Compose), fastfetch, and mpv defaults
  run mkdir -p "$HOME/.config/xkb/rules" "$HOME/.config/xkb/symbols"
  run cp -a "$PROJECT_DIR/etc/xkb/rules/evdev" "$HOME/.config/xkb/rules/evdev"
  run cp -a "$PROJECT_DIR/etc/xkb/symbols/quattro" "$HOME/.config/xkb/symbols/quattro"

  if [[ ! -f $HOME/.config/fastfetch/config.jsonc ]]; then
    backup_path "$HOME/.config/fastfetch/config.jsonc"
    run mkdir -p "$HOME/.config/fastfetch"
    run cp -a "$PROJECT_DIR/etc/fastfetch/config.jsonc" "$HOME/.config/fastfetch/config.jsonc"
  fi

  if [[ ! -f $HOME/.config/mpv/mpv.conf ]]; then
    backup_path "$HOME/.config/mpv/mpv.conf"
    run mkdir -p "$HOME/.config/mpv"
    run cp -a "$PROJECT_DIR/etc/mpv/mpv.conf" "$HOME/.config/mpv/mpv.conf"
  fi

  # Install user systemd units
  run mkdir -p "$HOME/.config/systemd/user/wireplumber.service.d"
  for unit in "$PROJECT_DIR"/default/systemd/user/*.service; do
    run cp -a "$unit" "$HOME/.config/systemd/user/"
  done
  run cp -a "$PROJECT_DIR/default/systemd/user/wireplumber.service.d/"*.conf "$HOME/.config/systemd/user/wireplumber.service.d/"

  # Install default mimeapps if user doesn't have one yet
  if [[ ! -f $HOME/.config/mimeapps.list ]]; then
    run cp -a "$PROJECT_DIR/default/applications/mimeapps.list" "$HOME/.config/mimeapps.list"
  fi

  # Refresh font cache
  if command -v fc-cache >/dev/null 2>&1; then
    run fc-cache -f "$HOME/.local/share/fonts" || true
  fi
}

generate_initial_theme() {
  echo "==> Generating initial theme ($THEME)..."
  if $DRY_RUN; then
    echo "[dry-run] QUATTRO_PATH=$INSTALL_DIR QUATTRO_THEME_HEADLESS=1 $INSTALL_DIR/bin/quattro-theme-set \"$THEME\""
    return
  fi

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
  run sudo mkdir -p /usr/local/share/wayland-sessions /etc/profile.d /usr/share/uwsm/env.d /etc/fastfetch /etc/mpv /etc/xdg/kitty /etc/xkb/rules /etc/xkb/symbols /etc/fonts/conf.d

  run sudo cp -a "$PROJECT_DIR/default/wayland-sessions/quattro.desktop" /usr/local/share/wayland-sessions/quattro.desktop
  run sudo cp -a "$PROJECT_DIR/etc/profile.d/quattro.sh" /etc/profile.d/quattro.sh
  run sudo cp -a "$PROJECT_DIR/default/uwsm/env.d/10-quattro" /usr/share/uwsm/env.d/10-quattro
  run sudo cp -a "$PROJECT_DIR/etc/fastfetch/config.jsonc" /etc/fastfetch/config.jsonc
  run sudo cp -a "$PROJECT_DIR/etc/mpv/mpv.conf" /etc/mpv/mpv.conf
  run sudo cp -a "$PROJECT_DIR/etc/xdg/kitty/kitty.conf" /etc/xdg/kitty/kitty.conf
  run sudo cp -a "$PROJECT_DIR/etc/xkb/rules/evdev" /etc/xkb/rules/evdev
  run sudo cp -a "$PROJECT_DIR/etc/xkb/symbols/quattro" /etc/xkb/symbols/quattro
  run sudo cp -a "$PROJECT_DIR/default/fontconfig/conf.avail/50-quattro.conf" /etc/fonts/conf.d/50-quattro.conf

  if $DRY_RUN; then
    echo "[dry-run] write /etc/quattro.conf pointing to $INSTALL_DIR"
  else
    printf 'export QUATTRO_PATH="%s"\n' "$INSTALL_DIR" | sudo tee /etc/quattro.conf >/dev/null
  fi
}

if $WITH_PACKAGES; then
  install_packages
fi

install_runtime
install_user_configs
generate_initial_theme

if [[ $MODE == "system" ]]; then
  install_system_configs
fi

cat <<EOF

Done! Quattro Arch port installed to: $INSTALL_DIR

Next steps:
  1. Ensure packages are installed (if you didn't pass --with-packages):
     ./install.sh --with-packages
  2. Open a new bash login shell (or run: source ~/.bashrc)
  3. Switch themes anytime with:
     quattro-theme-set "Tokyo Night"   # or catppuccin, gruvbox, kanagawa, nord, etc.
  4. Start Hyprland from TTY/SDDM via:
     uwsm start hyprland.desktop
EOF
