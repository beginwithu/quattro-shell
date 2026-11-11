-- Hyprland bootstrap for Quattro's Lua module path.

local home = os.getenv("HOME")
local reload_prefixes = {
  "default.hypr",
  "hypr",
  "quattro.current.theme",
  "quattro.toggles.hypr",
}

local function should_reload_module(module)
  for _, prefix in ipairs(reload_prefixes) do
    if module == prefix or module:sub(1, #prefix + 1) == prefix .. "." then
      return true
    end
  end

  return false
end

local modules_to_reload = {}
for module in pairs(package.loaded) do
  if should_reload_module(module) then
    table.insert(modules_to_reload, module)
  end
end

for _, module in ipairs(modules_to_reload) do
  package.loaded[module] = nil
end

-- The checkout: $QUATTRO_PATH, else the folder this file was loaded from.
local root = os.getenv("QUATTRO_PATH")
if not root or root == "" then
  local source = debug and debug.getinfo and debug.getinfo(1, "S").source or ""
  root = source:match("^@(.*)/default/hypr/bootstrap%.lua$") or (home .. "/quattro-shell")
end

-- Load generated state from ~/.local/state, user modules from ~/.config, and
-- Quattro defaults from $QUATTRO_PATH.
local state_home = os.getenv("XDG_STATE_HOME")
if not state_home or state_home == "" then state_home = home .. "/.local/state" end
local config_home = os.getenv("XDG_CONFIG_HOME")
if not config_home or config_home == "" then config_home = home .. "/.config" end

package.path = state_home
  .. "/?.lua;"
  .. config_home
  .. "/?.lua;"
  .. root
  .. "/?.lua;"
  .. package.path
