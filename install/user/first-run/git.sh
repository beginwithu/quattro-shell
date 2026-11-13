# An upgrade, or a run of quattro-setup-git, can have set the identity already.
if [[ -z $(git config --global user.name) || -z $(git config --global user.email) ]]; then
  quattro-notification-send -u critical -g  "$(quattro-i18n firstRun.gitTitle)" "$(quattro-i18n firstRun.gitBody)" \
    --exec quattro-launch-floating-terminal-with-presentation quattro-setup-git
fi
