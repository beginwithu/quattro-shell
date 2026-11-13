#!/bin/bash

# Enable AND start the user systemd units we ship. Runs at first-run rather
# than at finalize-user time because the user manager isn't live during the
# ISO chroot — by first-run, the Hyprland/uwsm session is up and
# `systemctl --user enable --now` both writes the correct .wants symlinks
# (based on each unit's [Install]/WantedBy) and starts the services so the
# first session has bluetooth pairing, sleep lock, etc. live immediately
# instead of waiting for the next login. ConditionPath* in the unit files
# keep the enabled units inert on hardware they don't apply to.

set -euo pipefail

quattro-input-method configure --defaults || echo "Input method defaults deferred; run quattro setup input after login." >&2

systemctl --user daemon-reload

# One at a time: given a list, systemctl enables none of it when it cannot find
# one unit, and that should not cost the session its bluetooth agent as well.
failed=0
for unit in \
  bt-agent.service \
  quattro-recover-internal-monitor.service \
  quattro-sleep-lock.service \
  quattro-migrate-notify.service \
  quattro-fcitx5.service; do
  systemctl --user enable --now "$unit" || failed=1
done

if systemctl --user list-unit-files owed.service >/dev/null 2>&1; then
  systemctl --user enable --now owed.service || failed=1
fi

if [[ -e /usr/share/owe/10-owe-sync ]]; then
  quattro-hook-install theme-set /usr/share/owe/10-owe-sync
fi
exit "$failed"
