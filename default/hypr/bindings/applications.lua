-- Essential application bindings.
o.bind("SUPER + RETURN", "Terminal", o.launch_terminal())
o.bind("SUPER + ALT + RETURN", "Tmux", { quattro = "terminal-tmux" })
o.bind("SUPER + SHIFT + RETURN", "Browser", { quattro = "browser" })
o.bind("SUPER + SHIFT + F", "File manager", { quattro = "files" })
o.bind("SUPER + ALT + SHIFT + F", "File manager (cwd)", { quattro = "files-cwd" })
o.bind("SUPER + SHIFT + B", "Browser", { quattro = "browser" })
o.bind("SUPER + SHIFT + ALT + B", "Browser (private)", { quattro = "browser --private" })
o.bind("SUPER + SHIFT + N", "Editor", { quattro = "editor" })

if o.preinstalled_bindings_enabled() then
  local calc_cmd = o.cmd_present("qalculate-gtk") and "qalculate-gtk"
    or (o.cmd_present("gnome-calculator") and "gnome-calculator"
    or (o.cmd_present("kcalc") and "kcalc" or nil))
  if calc_cmd then
    o.bind("SUPER + SHIFT + Q", "Calculator", calc_cmd)
    o.bind("XF86Calculator", "Calculator", calc_cmd)
  else
    o.bind("SUPER + SHIFT + Q", "Calculator", { tui = "python3 -q" })
  end
end
