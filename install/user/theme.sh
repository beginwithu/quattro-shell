# Setup user theme folder and seed the default only when no theme exists yet.
mkdir -p ~/.config/quattro/themes

if [[ ! -s $HOME/.local/state/quattro/current/theme.name ]]; then
  # iso-chroot and provision-owner both run without a live session to notify.
  if [[ ${QUATTRO_SETUP_CONTEXT:-runtime} != "runtime" ]]; then
    QUATTRO_THEME_HEADLESS=1 quattro-theme-set "Tokyo Night"
    rm -f ~/.config/google-chrome/SingletonLock ~/.config/chromium/SingletonLock # otherwise archiso owns the browser singleton
  else
    quattro-theme-set "Tokyo Night"
  fi
fi
quattro-theme-set-pi --activate
quattro-theme-set-fcitx5 --activate
# A headless theme set skips the retints, so the seeded config would keep its
# default icon theme until the first switch.
quattro-theme-set-qt

mkdir -p ~/.config/btop/themes
ln -snf "$HOME/.local/state/quattro/current/theme/btop.theme" ~/.config/btop/themes/current.theme
