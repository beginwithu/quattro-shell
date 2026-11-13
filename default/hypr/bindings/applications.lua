-- Essential application bindings.
o.bind("SUPER + RETURN", "Terminal", o.launch_terminal())
o.bind("SUPER + SHIFT + RETURN", "Browser", { quattro = "browser" })
o.bind("SUPER + SHIFT + F", "File manager", { quattro = "files" })
o.bind("SUPER + ALT + SHIFT + F", "File manager (cwd)", { quattro = "files-cwd" })
o.bind("SUPER + SHIFT + B", "Browser", { quattro = "browser" })
o.bind("SUPER + SHIFT + ALT + B", "Browser (private)", { quattro = "browser --private" })
o.bind("SUPER + SHIFT + N", "Editor", { quattro = "editor" })

if o.preinstalled_bindings_enabled() then
  -- Bindings for preinstalled Quattro applications, TUIs, and web apps,
  -- with fallbacks to standard Arch Linux packages when Quattro-repo apps aren't installed.
  o.bind("SUPER + ALT + RETURN", "Tmux", { quattro = "terminal-tmux" })
  o.bind("SUPER + CTRL + RETURN", "Tmux", { quattro = "terminal-tmux" })

  local calc_cmd = o.cmd_present("omacalc") and "omacalc"
    or (o.cmd_present("qalculate-gtk") and "qalculate-gtk"
    or (o.cmd_present("gnome-calculator") and "gnome-calculator"
    or (o.cmd_present("kcalc") and "kcalc" or nil)))
  if calc_cmd then
    o.bind("SUPER + SHIFT + Q", "Calculator", calc_cmd)
    o.bind("XF86Calculator", "Calculator", calc_cmd)
  else
    o.bind("SUPER + SHIFT + Q", "Calculator", { tui = "python3 -q" })
  end

  o.bind("SUPER + SHIFT + M", "Music", { quattro = "spotify" })
  if o.cmd_present("cliamp") then
    o.bind("SUPER + SHIFT + ALT + M", "Music TUI", { tui = "cliamp", focus = true })
  elseif o.cmd_present("cmus") then
    o.bind("SUPER + SHIFT + ALT + M", "Music TUI", { tui = "cmus", focus = true })
  elseif o.cmd_present("ncmpcpp") then
    o.bind("SUPER + SHIFT + ALT + M", "Music TUI", { tui = "ncmpcpp", focus = true })
  end

  if o.cmd_present("lazydocker") then
    o.bind("SUPER + SHIFT + D", "Docker", { tui = "quattro-launch-docker-tui" })
  end
  o.bind("SUPER + SHIFT + G", "Signal", { quattro = "signal" })
  o.bind("SUPER + SHIFT + O", "Obsidian", { launch = "obsidian", focus = "^obsidian$" })

  if o.cmd_present("omawrite") then
    o.bind("SUPER + SHIFT + W", "Omawrite", { launch = "omawrite" })
  elseif o.cmd_present("libreoffice") then
    o.bind("SUPER + SHIFT + W", "Writer", { launch = "libreoffice --writer" })
  end
  o.bind("SUPER + SHIFT + SLASH", "Passwords", { quattro = "1password" })

  o.bind("SUPER + SHIFT + C", "Calendar", { webapp = "https://app.hey.com/calendar/weeks/" })
  o.bind("SUPER + SHIFT + E", "Email", { webapp = "https://app.hey.com" })
  o.bind("SUPER + SHIFT + ALT + E", "New email", { webapp = "https://app.hey.com/messages/new?display=standalone&new_window=true" })
  o.bind("SUPER + SHIFT + Y", "YouTube", { webapp = "https://youtube.com/" })
  o.bind("SUPER + SHIFT + ALT + G", "WhatsApp", { webapp = "https://web.whatsapp.com/", focus = true })
  o.bind( "SUPER + SHIFT + CTRL + G", "Google Messages", { webapp = "https://messages.google.com/web/conversations", focus = true })
  o.bind("SUPER + SHIFT + P", "Google Photos", { webapp = "https://photos.google.com/", focus = true })
  o.bind("SUPER + SHIFT + S", "Google Maps", { webapp = "https://maps.google.com/", focus = true })
  o.bind("SUPER + SHIFT + X", "X", { webapp = "https://x.com/" })
  o.bind("SUPER + SHIFT + ALT + X", "X Post", { webapp = "https://x.com/compose/post" })
end
