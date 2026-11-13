# Embed every Qualcomm device tree for systemd-stub to select from SMBIOS data,
# and the machine's own for it to fall back on.
# DeviceTreeAuto requires literal paths, so the list is written here and
# refreshed before every boot image build, when a kernel update can bring new
# or renamed trees.

if quattro-hw-aarch64-qualcomm; then
  quattro-pkg-add systemd-ukify

  (
    export QUATTRO_QUALCOMM_DTB_ROOT QUATTRO_QUALCOMM_DTB_DIR QUATTRO_QUALCOMM_UKI_CONFIG QUATTRO_IMAGE_BOOT_REBUILD QUATTRO_PROC_ROOT
    quattro-update-qualcomm-device-trees --install
  )

  # limine runs pre.d's executables before it builds any boot image, the
  # kernel update's included. Root runs them, so the hook points at the
  # packaged command rather than a path under QUATTRO_PATH a user may own.
  boot_hooks_dir=${QUATTRO_QUALCOMM_BOOT_HOOKS_DIR:-/etc/boot/hooks/pre.d}
  mkdir -p "$boot_hooks_dir"
  ln -sfn /usr/bin/quattro-update-qualcomm-device-trees "$boot_hooks_dir/20-quattro-qualcomm-device-trees"
fi
