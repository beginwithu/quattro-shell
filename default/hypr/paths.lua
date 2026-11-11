-- Shared path constants for Quattro's Hyprland Lua modules.
-- Lua files loaded with require() have separate local scopes, so modules that
-- need these paths import this table instead of repeating os.getenv() lookups.

local home = os.getenv("HOME")

-- A variable that is set but empty means "unset" (XDG Base Directory spec);
-- bash's ${VAR:-fallback} in the sibling tools treats it the same way.
-- The checkout: $QUATTRO_PATH, else the folder this file was loaded from
-- (<root>/default/hypr/paths.lua), else ~/quattro-shell.
local function quattro_root()
  local from_env = os.getenv("QUATTRO_PATH")
  if from_env and from_env ~= "" then
    return from_env
  end

  local source = debug and debug.getinfo and debug.getinfo(1, "S").source or ""
  return source:match("^@(.*)/default/hypr/paths%.lua$") or (home .. "/quattro-shell")
end

local function env_or(name, fallback)
  local value = os.getenv(name)
  if value == nil or value == "" then
    return fallback
  end
  return value
end

return {
  home = home,
  config_home = env_or("XDG_CONFIG_HOME", home .. "/.config"),
  state_home = env_or("XDG_STATE_HOME", home .. "/.local/state"),
  quattro_path = quattro_root(),
}
