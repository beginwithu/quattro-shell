notify_update() {
  quattro-notification-send -u critical -g  "$(quattro-i18n firstRun.updateTitle)" "$(quattro-i18n firstRun.updateBody)" \
    --exec quattro-launch-floating-terminal-with-presentation quattro-update
}

# Something for quattro-update to install: any repository package, Quattro's
# own among them, or a development checkout behind its upstream. checkupdates
# exits 2 when every package is current and 1 when it could not tell, so only
# a definite "nothing" holds the prompt back. It syncs the package databases,
# so it runs once; quattro-update-available would run it again, and is asked
# only about a checkout, which checkupdates cannot see.
update_pending() {
  local status=0
  checkupdates --nocolor >/dev/null 2>&1 || status=$?

  if (( status != 2 )); then
    return 0
  elif [[ $QUATTRO_PATH != "/usr/share/quattro" ]]; then
    quattro-update-available >/dev/null 2>&1
  else
    return 1
  fi
}

notify_wifi() {
  quattro-notification-send -u critical -g 󰖩 "$(quattro-i18n firstRun.wifiTitle)" "$(quattro-i18n firstRun.wifiBody)" \
    --exec quattro-shell shell toggle quattro.network
}

announce_network() {
  # Ethernet is still negotiating DHCP when the session starts, so probing
  # right away calls a working machine offline. NetworkManager reports startup
  # complete once it has tried every connection it could auto-activate, which
  # is the first moment the answer means anything.
  nm-online -q -s -t 30

  # -x takes that answer as it stands rather than waiting out the timeout, so
  # a laptop with nothing to connect to gets prompted immediately.
  if ! nm-online -q -x -t 30; then
    notify_wifi
    # Nothing to install or update until a link lands, so hold those prompts
    # for however long that takes. nm-online has no "forever", so wait out its
    # timeout as often as needed.
    until nm-online -q -t 86400; do :; done
  fi

  # Setup installs packages, so it waits for the link rather than for the
  # first update. It invites only once, and only when there is something to
  # set up.
  bash "$QUATTRO_PATH/install/user/first-run/setup-fingerprint.hook"

  # A fresh install from current media has nothing to update, and a toast
  # that leads nowhere teaches people to dismiss the next one.
  update_pending && notify_update
}

# Detached, so a slow or absent connection never holds up the rest of first run.
announce_network &
