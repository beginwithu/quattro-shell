# quattro-arch

Hyprland desktop configuration and Quickshell environment adapted from Omarchy to run on a standard Arch Linux install without custom ISO tooling or external package repos. AI and agent integrations have been stripped out.

## Directory layout

- `config/` - user dotfiles (`hypr`, `alacritty`, `kitty`, `ghostty`, `foot`, `btop`, `tmux`, `starship.toml`, etc.)
- `default/` - runtime defaults (bash setup, Hyprland Lua modules, theme templates, systemd user units, fonts)
- `bin/` - `quattro-*` helper scripts
- `shell/` - Quickshell configuration (bar, dock, launcher, panels, notifications, lockscreen)
- `themes/` - 23 color schemes (`Tokyo Night`, `Catppuccin`, `Gruvbox`, `Kanagawa`, `Nord`, `Rose Pine`, etc.)
- `applications/` - `.desktop` entries and icons
- `etc/` - optional system files (`fastfetch`, `mpv`, `xkb`, `profile.d`, `sysctl.d`)
- `packages/` - Arch package lists (`arch-core.packages`, `arch-optional.packages`, `aur.packages`) and [package-map.md](packages/package-map.md)
- `install.sh` - deployment script (backs up overwritten configs to `~/.local/state/quattro-arch/backups/`)
- `verify.sh` - checks script syntax, Lua compilation, JSON configs, pacman package names, and sandbox install

## Install

Check dependencies and syntax first without touching the system:

```bash
./verify.sh
```

Preview what `install.sh` will copy:

```bash
./install.sh --user-only --dry-run
```

Install configs and required Arch packages (`[core]` / `[extra]`):

```bash
./install.sh --user-only --with-packages
```

Useful flags:

- `--with-optional` - include optional desktop/dev packages (`obsidian`, `libreoffice-fresh`, `docker`, etc.)
- `--with-aur` - install AUR packages via `yay` (`qtengine`, `google-chrome`, etc.)
- `--theme "<name>"` - pick an initial theme instead of `Tokyo Night`
- `--system` - install system-wide session files (`/usr/local/share/wayland-sessions/quattro.desktop`, `/etc/profile.d/quattro.sh`, `/etc/quattro.conf`, `xkb`, `fastfetch`, `mpv`; prompts for `sudo`)

## Starting the session

Log in via SDDM (choose **Quattro (Hyprland uwsm)** if installed with `--system`), or start it from a TTY:

```bash
uwsm start hyprland.desktop
```

## Notes for standard Arch

- `QUATTRO_PATH` resolves `~/.local/share/quattro` before `/usr/share/quattro`, so user-only installs work without root.
- Custom distro packages (`nvim`, `mise-bin`, `ttf-jetbrains-mono-nerd-basic`, `omarchy-power-profiles-daemon`, `vi`) are mapped to Arch repo names; see [packages/package-map.md](packages/package-map.md).
- Custom apps (`folio`, `omasnap`, `omacalc`, `omawrite`, `cliamp`, `qtengine`, `omarchy-screensaver`, `hyprland-preview-share-picker`) fall back to standard Arch tools (`nautilus`/`dolphin`/`thunar`, `grim`+`slurp`, `qalculate-gtk`/`kcalc`, `libreoffice`, `cmus`/`ncmpcpp`, `qt6ct`, `hyprland-guiutils`) when absent.
- Bootloader and ISO-specific configs (`limine`, `mkinitcpio`, `snapper`) are left out so existing Arch installs are untouched.
- Existing terminal configs (`kitty.conf` defaults) and browser flag files (`chrome-flags.conf`, `chromium-flags.conf`) are self-contained under `$HOME`.
