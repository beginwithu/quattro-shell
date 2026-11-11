-- Quattro's monitor defaults: every monitor at its preferred mode, placed
-- automatically, at the scale Hyprland picks for it. Anything in
-- hypr/monitors.lua loads after these and wins.
hl.monitor({ output = "", mode = "preferred", position = "auto", scale = "auto" })

-- GDK_SCALE is the factor GTK draws its own UI at, which sizes X11/XWayland
-- windows: Quattro leaves those unscaled by the compositor so they stay crisp.
-- 2 suits the high-density displays Quattro assumes; a scale chosen in the
-- display panel replaces it with one following the lowest monitor scale.
hl.env("GDK_SCALE", "2")
