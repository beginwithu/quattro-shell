# Configure pacman post-install without overwriting /etc/pacman.conf or adding
# custom repositories. A Mac's image brings its keyrings, as the ISO does on x86;
# every other aarch64 platform installs Arch Linux ARM's keyring and trusts it.
source "$QUATTRO_PATH/install/helpers/pacman.sh"
source "$QUATTRO_PATH/install/helpers/image-target.sh"
platform=$(quattro-hw-platform)
alarm_keyring=0
if [[ $platform == aarch64* && $platform != "aarch64-apple" ]]; then
  alarm_keyring=1
fi

if (( alarm_keyring )); then
  quattro-pkg-add archlinuxarm-keyring
fi

# An image build leaves the keyring to each machine's first boot, so no two
# share a master key (install/helpers/image-target.sh).
if (( alarm_keyring )); then
  if quattro_image_init && quattro_image_manifest_present; then
    install -m 0644 /dev/null "$quattro_image_keyring_request"
  else
    pacman-key --init
    pacman-key --populate
  fi
fi

# Wait for CUPS to own the file, the way quattro-settings does, so pacman does
# not turn the override into a .pacnew during ISO package installation.
if [[ -f $QUATTRO_PATH/etc-overrides/cups-cups-files.conf && -f /etc/cups/cups-files.conf ]]; then
  install -m 0640 -o root -g cups "$QUATTRO_PATH/etc-overrides/cups-cups-files.conf" /etc/cups/cups-files.conf
  rm -f /etc/cups/cups-files.conf.pacnew
fi

source "$QUATTRO_INSTALL/hardware/pacman.sh"
