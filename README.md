# quattro-arch

Minimal Hyprland desktop configuration and Quickshell environment adapted from Omarchy to run on a standard Arch Linux install without custom ISO tooling, bloatware apps, or AI/agent integrations.

## Directory layout

- `config/` - user dotfiles (`hypr`, `foot`, `alacritty`, `kitty`, `ghostty`, `btop`, `tmux`, `starship.toml`, etc.)
- `default/` - runtime defaults (bash setup, Hyprland Lua modules, theme templates, systemd user units, fonts)
- `bin/` - `quattro-*` helper scripts
- `shell/` - Quickshell configuration (bar, dock, launcher, panels, notifications, lockscreen)
- `themes/` - 23 color schemes (`Tokyo Night`, `Catppuccin`, `Gruvbox`, `Kanagawa`, `Nord`, `Rose Pine`, etc.)
- `applications/` - core `.desktop` entries and icons
- `etc/` - system config files (`fastfetch`, `mpv`, `xkb`, `profile.d`, etc.)
- `packages/` - core Arch package list ([arch-core.packages](packages/arch-core.packages)) and [package-map.md](packages/package-map.md)
- `AGENTS.md` - guide for working on `quattro-shell` (Quickshell architecture, IPC, theming, QML conventions)
- `install.sh` - single-command installer (deploys runtime, configs, theme, and system integration)
- `verify.sh` - checks script syntax, Lua compilation, JSON configs, pacman package names, and sandbox install

## Install

Check script syntax, Lua files, and package names without touching the system:

```bash
./verify.sh
```

Run the installer (installs lean core Arch `[core]`/`[extra]` packages, runtime, user configs, initial `Tokyo Night` theme, and system integration):

```bash
./install.sh
```

Existing user files in `~/.config` and `~/.bashrc` are overwritten directly without creating backups.

## Starting the session

Log in via SDDM (choose **Quattro (Hyprland uwsm)**), or start it from a TTY:

```bash
uwsm start hyprland.desktop
```

## Notes for standard Arch

- `QUATTRO_PATH` resolves `~/.local/share/quattro` before `/usr/share/quattro`, so user-only installs work without root.
- Custom distro packages (`nvim`, `mise-bin`, `ttf-jetbrains-mono-nerd-basic`, `omarchy-power-profiles-daemon`, `vi`) are mapped to Arch repo names; see [packages/package-map.md](packages/package-map.md).
- Custom apps (`folio`, `omasnap`, `omacalc`, `omawrite`, `cliamp`, `qtengine`, `omarchy-screensaver`, `hyprland-preview-share-picker`) fall back to standard Arch tools (`nautilus`, `grim`+`slurp`, `qalculate-gtk`/`kcalc`, `libreoffice`, `cmus`/`ncmpcpp`, `qt6ct`, `hyprland-guiutils`) when absent.
- Bootloader and ISO-specific configs (`limine`, `mkinitcpio`, `snapper`) are left out so existing Arch installs are untouched.
- Existing terminal configs (`kitty.conf` defaults) and browser flag files (`chrome-flags.conf`, `chromium-flags.conf`) are self-contained under `$HOME`.
