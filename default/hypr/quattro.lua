-- Quattro Hyprland setup: helpers, defaults, and current theme overrides.

require("default.hypr.helpers")
local require_optional = require("default.hypr.require_optional")

-- Use Quattro defaults, but don't edit these directly.
require("default.hypr.autostart")
-- Before any monitor rule, so every one of them passes through it.
require("default.hypr.wide-gamut")
require("default.hypr.monitors")
require("default.hypr.monitor-workspaces")
if _G.quattro_default_bindings ~= false then
  require("default.hypr.bindings.media")
  require("default.hypr.bindings.clipboard")
  require("default.hypr.bindings.input-method")
  require("default.hypr.bindings.jis")
  require("default.hypr.bindings.tiling")
  require("default.hypr.bindings.utilities")
  require_optional.module("default.hypr.bindings.applications")
end
if _G.quattro_default_gestures ~= false then
  require("default.hypr.gestures")
end
require("default.hypr.envs")
require("default.hypr.looknfeel")
require("default.hypr.qconsole")
require("default.hypr.input")
require("default.hypr.windows")
require("default.hypr.pop")

-- Current theme overrides.
require_optional.module("quattro.current.theme.hyprland")

-- Lighter defaults for a session that renders on the CPU, over the theme's.
require("default.hypr.software-rendering")
