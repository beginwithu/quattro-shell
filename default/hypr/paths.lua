-- Shared path constants for Quattro's Hyprland Lua modules.
-- Lua files loaded with require() have separate local scopes, so modules that
-- need these paths import this table instead of repeating os.getenv() lookups.

local home = os.getenv("HOME")

-- A variable that is set but empty means "unset" (XDG Base Directory spec);
-- bash's ${VAR:-fallback} in the sibling tools treats it the same way.
local function env_or(name, fallback)
  local value = os.getenv(name)
  if value == nil or value == "" then
    return fallback
  end
  return value
end

local function resolve_quattro_path()
  local env_path = os.getenv("QUATTRO_PATH")
  if env_path ~= nil and env_path ~= "" then
    return env_path
  end

  local user_path = (home or "") .. "/.local/share/quattro"
  local f = io.open(user_path .. "/default/hypr/quattro.lua", "r")
  if f then
    f:close()
    return user_path
  end

  return "/usr/share/quattro"
end

return {
  home = home,
  config_home = env_or("XDG_CONFIG_HOME", home .. "/.config"),
  state_home = env_or("XDG_STATE_HOME", home .. "/.local/state"),
  quattro_path = resolve_quattro_path(),
}
