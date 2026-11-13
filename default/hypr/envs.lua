local paths = require("default.hypr.paths")
local require_optional = require("default.hypr.require_optional")

-- GUM environment variables for styling purposes.
require_optional.module("quattro.current.theme.gum_env")

-- Cursor size.
hl.env("XCURSOR_SIZE", "24")
hl.env("HYPRCURSOR_SIZE", "24")

local function file_readable(path)
  local f = io.open(path, "r")
  if f then
    f:close()
    return true
  end
  return false
end

-- Force all apps to use Wayland.
hl.env("GDK_BACKEND", "wayland,x11,*")
hl.env("QT_QPA_PLATFORM", "wayland;xcb")
if file_readable("/usr/lib/qt6/plugins/platformthemes/libqtengine.so") or file_readable("/usr/lib/qt/plugins/platformthemes/libqtengine.so") then
  hl.env("QT_QPA_PLATFORMTHEME", "qtengine")
elseif file_readable("/usr/lib/qt6/plugins/platformthemes/libqt6ct.so") then
  hl.env("QT_QPA_PLATFORMTHEME", "qt6ct")
end
hl.env("MOZ_ENABLE_WAYLAND", "1")
hl.env("ELECTRON_OZONE_PLATFORM_HINT", "wayland")
hl.env("OZONE_PLATFORM", "wayland")
hl.env("XDG_SESSION_TYPE", "wayland")

-- Allow better support for screen sharing (Google Meet, Discord, etc).
hl.env("XDG_CURRENT_DESKTOP", "Hyprland")
hl.env("XDG_SESSION_DESKTOP", "Hyprland")

-- Use XCompose file.
hl.env("XCOMPOSEFILE", paths.home .. "/.XCompose")

-- hyprctl setenv doesn't reach keybind dispatcher env; use hl.env.
hl.env("QUATTRO_PATH", paths.quattro_path)

local bin_dir = paths.quattro_path .. "/bin"
local kept = {}
for entry in (os.getenv("PATH") or "/usr/local/bin:/usr/bin"):gmatch("[^:]+") do
  if entry ~= bin_dir then table.insert(kept, entry) end
end
table.insert(kept, 1, bin_dir)
hl.env("PATH", table.concat(kept, ":"))

-- Hardware-specific environment.
require("default.hypr.nvidia")

hl.config({
  xwayland = {
    force_zero_scaling = true,
  },

  ecosystem = {
    no_update_news = true,
    no_donation_nag = true,
  },
})
