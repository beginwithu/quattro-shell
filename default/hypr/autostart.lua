hl.on("hyprland.start", function()
  -- Slow app launch fix -- set systemd and D-Bus activation vars before
  -- starting session services.
  hl.exec_cmd("dbus-update-activation-environment --systemd --all")

  hl.exec_cmd("quattro-launch-shell")
  hl.exec_cmd("quattro-provision-first-run")
  hl.exec_cmd("quattro-powerprofiles-init")
  hl.exec_cmd("quattro-hyprland-software-rendering")
  hl.exec_cmd(o.launch("quattro-hyprland-monitor-watch"))
  hl.exec_cmd(o.launch("udiskie --automount --no-notify --no-tray"))

  -- Run post-boot hooks after startup config has loaded.
  hl.exec_cmd("sleep 2 && quattro-hook post-boot")
end)
