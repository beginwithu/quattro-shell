-- Learn how to configure Hyprland: https://wiki.hypr.land/Configuring/Start/

-- Quattro's bootstrap keeps path setup out of this user config.
local function resolve_quattro_path()
  local env_path = os.getenv("QUATTRO_PATH")
  if env_path and env_path ~= "" then
    return env_path
  end

  local user_path = (os.getenv("HOME") or "") .. "/.local/share/quattro"
  local f = io.open(user_path .. "/default/hypr/bootstrap.lua", "r")
  if f then
    f:close()
    return user_path
  end

  return "/usr/share/quattro"
end

dofile(resolve_quattro_path() .. "/default/hypr/bootstrap.lua")

-- Disable all Quattro default bindings. Add your own in hypr/bindings.lua.
-- quattro_default_bindings = false
--
-- Or disable only bindings for Quattro's preinstalled apps/web apps while
-- keeping core window-manager bindings:
-- quattro_preinstalled_bindings = false
--
-- Disable Quattro's default touchpad gestures. Add your own in hypr/input.lua.
-- quattro_default_gestures = false

-- Load Quattro defaults.
require("default.hypr.quattro")

-- Put your personal overrides in these files. They're loaded after Quattro's
-- defaults so package updates can improve the defaults without rewriting your
-- ~/.config/hypr files.
require("hypr.monitors")
require("hypr.input")
require("hypr.bindings")
require("hypr.looknfeel")
require("hypr.autostart")

-- Toggle config flags dynamically.
require("default.hypr.toggles")

-- Add any other personal Hyprland configuration below.
-- o.window("qemu", { workspace = "5" })
