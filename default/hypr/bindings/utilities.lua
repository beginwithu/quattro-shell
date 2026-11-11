o.bind("SUPER + SPACE", "Quattro menu", { menu = "root" })
o.bind("SUPER + ALT + SPACE", "Apps", "quattro-shell quattro.dock apps")
o.bind("SUPER + CTRL + E", "Emojis", { panel = "quattro.emojis" })
o.bind("SUPER + CTRL + C", "Capture menu", { menu = "trigger.capture" })
o.bind("SUPER + CTRL + O", "Toggle menu", { menu = "trigger.toggle" })
o.bind("SUPER + SHIFT + code:201", "Quattro menu", { menu = "root" })
o.bind("SUPER + ESCAPE", "System menu", { menu = "system" })
o.bind("XF86PowerOff", "Power menu", { menu = "system" }, { locked = true })
o.bind_toggle("SUPER + SHIFT + SPACE", "Toggle top bar", "bar")
o.bind("SUPER + SHIFT + CTRL + SPACE", "Theme menu", { menu = "theme" })
o.bind("SUPER + CTRL + T", "Theme menu", { menu = "theme" })
o.bind("SUPER + K", "Keybindings", "quattro-menu-keybindings")

-- xkbcommon names the comma keysym "comma"; the upper-case "COMMA" does not match.
o.bind("SUPER + comma", "Dismiss, invoke, or focus last notification", { ipc = "notifications.act" })
o.bind("SUPER + SHIFT + comma", "Dismiss all notifications", { ipc = "notifications.dismissAll" })
o.bind("SUPER + CTRL + comma", "Control notifications with the keyboard", { ipc = "notifications.keyboard" })
o.bind("SUPER + ALT + comma", "Dismiss last notification", { ipc = "notifications.dismissOne" })
o.bind("SUPER + SHIFT + ALT + comma", "Open notification history", { ipc = "notifications.showHistory" })
o.bind_toggle("SUPER + CTRL + ALT + comma", "Toggle silencing notifications", "notification-silencing")

o.bind_toggle("SUPER + CTRL + I", "Toggle locking on idle", "idle")

o.bind("PRINT", "Screenshot", "quattro-capture-screenshot")
o.bind("SUPER + CTRL + ALT + C", "Screenshot", "quattro-capture-screenshot")
o.bind("ALT + PRINT", "Screenrecording", "quattro-capture-screenrecording --stop-recording || quattro-menu toggle trigger.capture.screenrecord")
o.bind("SUPER + PRINT", "Color picker", "pkill hyprpicker || hyprpicker -a")
o.bind("SUPER + CTRL + PRINT", "Extract text (OCR) from screenshot", "quattro-capture-text")

o.bind("SUPER + CTRL + S", "Share", { menu = "share" })

o.bind("SUPER + CTRL + A", "Audio", { panel = "quattro.audio" })
o.bind("SUPER + CTRL + B", "Bluetooth", { panel = "quattro.bluetooth" })
o.bind("SUPER + CTRL + ALT + D", "Calendar", { panel = "quattro.clock" })
o.bind("SUPER + CTRL + ALT + E", "World clock", { panel = "quattro.elsewhen" })
o.bind("SUPER + CTRL + N", "Network", { panel = "quattro.network" })
o.bind("SUPER + CTRL + P", "Power", { panel = "quattro.power" })
o.bind("SUPER + SHIFT + T", "Activity", { tui = "btop" })

-- The letters above name a panel; the numbers count them. 1 is the leftmost
-- panel in the bar's right section, and a widget with no panel of its own (the
-- tray) is not counted, so the number matches the icon a user would point at.
-- A bar with fewer panels than this leaves the tail of the range doing nothing.
for panel = 1, 9 do
  o.bind(
    "SUPER + CTRL + code:" .. tostring(panel + 9),
    "Bar panel " .. panel,
    "quattro-shell -q shell togglePanelAt right " .. panel
  )
end

o.bind("SUPER + CTRL + Z", "Zoom in", function()
  local zoom = hl.get_config("cursor.zoom_factor") or 1
  hl.config({ cursor = { zoom_factor = zoom + 1 } })
end)

o.bind("SUPER + CTRL + ALT + Z", "Reset zoom", function()
  hl.config({ cursor = { zoom_factor = 1 } })
end)

o.bind("SUPER + CTRL + L", "Lock system", "quattro-system-lock")
