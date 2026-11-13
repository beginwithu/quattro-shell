# Package mapping

Custom Omarchy packages and how they are handled on standard Arch Linux:

| Upstream | Arch Linux handling | Notes |
|---|---|---|
| `nvim` | `neovim` (`extra`) | Package rename |
| `vi` | `vim` (`extra`) | Package rename |
| `mise-bin` | `mise` (`extra`) | Package rename |
| `ttf-jetbrains-mono-nerd-basic` | `ttf-jetbrains-mono-nerd` (`extra`) | Package rename |
| `omarchy-power-profiles-daemon` | `power-profiles-daemon` (`extra`) | Package rename |
| `folio` | `nautilus` (`extra`) | Falls back to `nautilus` / `dolphin` / `thunar` / `xdg-open` |
| `omasnap` | `grim` + `slurp` + `wl-clipboard` + `satty` (`extra`) | `quattro-capture-screenshot` falls back to `quattro-capture-region` |
| `omacalc` | `qalculate-gtk` (`extra`) | Falls back to `qalculate-gtk` / `gnome-calculator` / `kcalc` |
| `omawrite` | `libreoffice-fresh` (`extra`) | Falls back to `libreoffice --writer` |
| `cliamp` | `cmus` / `ncmpcpp` (optional) | Bound only when present |
| `herdr` | Removed (uses `tmux` from `extra`) | `applications.lua` binds `SUPER + CTRL + RETURN` to `tmux` |
| `qtengine` | `qtengine` (AUR) or `qt6ct` (`extra`) | `default/hypr/envs.lua` sets `QT_QPA_PLATFORMTHEME` dynamically based on which plugin is installed |
| `hyprland-preview-share-picker` | `hyprland-guiutils` (`extra`) | `config/hypr/xdph.conf` leaves `custom_picker_binary` commented so `xdph` uses Arch's built-in picker |
| `omarchy-hyprland-titlebars` | Optional | `default/hypr/workspace-modes.lua` checks `io.open(TITLEBARS)` before loading |
| `omarchy-screensaver` | Optional | `bin/quattro-launch-screensaver` exits cleanly or locks if `force` |
| `owe` | Optional | Quickshell `BackgroundService` handles still images directly without `owe` |
| `omarchy-nvim` | `neovim` (`extra`) | User Neovim config / LazyVim works independently |
| `ttf-ia-writer` | Optional (`noto-fonts` / `adwaita-fonts` included) | Available manually or via AUR if desired |
| `limine` / `linux-omarchy` / `snapper` | Excluded | ISO/bootloader/kernel-specific packages are not forced onto an existing Arch Linux system |
