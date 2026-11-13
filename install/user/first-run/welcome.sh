# The catalog entry holds a real newline, not a literal \n: the card renders the
# body as it arrives, and elides past three lines.
quattro-notification-send -u critical -g  "$(quattro-i18n firstRun.keybindingsTitle)" \
  "$(quattro-i18n firstRun.keybindingsBody)" \
  --exec quattro-menu-keybindings
