# Desktop camera support for the Snapdragon ThinkPad T14s Gen 6 (21N1/21N2).
# Requires a kernel/device tree that already exposes the OV02C10 camera.

if quattro-hw-aarch64-qualcomm && quattro-hw-match '^21N[12]'; then
  # Pulls in libcamera and its software ISP IPA as dependencies.
  quattro-pkg-add pipewire-libcamera

  sudo install -Dm644 "$QUATTRO_PATH/default/udev/thinkpad-t14s-camera.rules" \
    /etc/udev/rules.d/70-thinkpad-t14s-camera.rules
fi
