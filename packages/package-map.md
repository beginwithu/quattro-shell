# Package mapping

Custom Omarchy packages and how they are handled on standard Arch Linux:

| Upstream | Arch Linux handling | Notes |
|---|---|---|
| `nvim` | `neovim` (`extra`) | Package rename |
| `vi` | `vim` (`extra`) | Package rename |
| `mise-bin` | `mise` (`extra`) | Package rename |
| `ttf-jetbrains-mono-nerd-basic` | `ttf-jetbrains-mono-nerd` (`extra`) | Package rename |
| `omarchy-power-profiles-daemon` | `power-profiles-daemon` (`extra`) | Package rename |
| `folio` | `thunar` (`extra`) | `quattro-launch-files` falls back to `nautilus` / `dolphin` / `thunar` / `xdg-open` |
| `omasnap` | `grim` + `slurp` + `wl-clipboard` + `satty` (`extra`) | `quattro-capture-screenshot` falls back to `quattro-capture-region` |
| `omacalc` | Optional (`qalculate-gtk` / `kcalc` / Python REPL) | Bound if installed, otherwise opens Python REPL |
| `omawrite` | Removed (optional) | Not installed by default |
| `cliamp` | Removed (optional) | Not installed by default |
| `herdr` | Removed (uses `tmux` from `extra`) | `SUPER + ALT + RETURN` opens `tmux` |
| `qtengine` | `qt6ct` (`extra`) | `default/hypr/envs.lua` sets `QT_QPA_PLATFORMTHEME` dynamically |
| `hyprland-preview-share-picker` | `hyprland-guiutils` (`extra`) | `config/hypr/xdph.conf` leaves `custom_picker_binary` commented so `xdph` uses Arch's built-in picker |
| `omarchy-hyprland-titlebars` | Optional | `default/hypr/workspace-modes.lua` checks `io.open(TITLEBARS)` before loading |
| `omarchy-screensaver` | Optional | `bin/quattro-launch-screensaver` exits cleanly or locks if `force` |
| `owe` | Optional | Quickshell `BackgroundService` handles still images directly without `owe` |
| `omarchy-nvim` | `neovim` (`extra`) | User Neovim config / LazyVim works independently |
| `ttf-ia-writer` | Optional (`noto-fonts` / `adwaita-fonts` included) | Available manually or via AUR if desired |
| `limine` / `linux-omarchy` / `snapper` | Excluded | ISO/bootloader/kernel-specific packages are not forced onto an existing Arch Linux system |
