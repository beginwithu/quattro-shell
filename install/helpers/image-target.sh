# Image builds and the hardware setup they defer to the machine's first boot.
# Sourced by quattro-apply-hardware and quattro-provision-hardware; no shebang.
#
# An image is built away from the machine it will run on, so hardware setup
# during the build would describe the builder, not the target. The builder
# declares the build in a root-owned manifest before it runs
# quattro-apply-system in the image root:
#
#   /var/lib/quattro/image/target
#     format=1
#     platform=x86|aarch64|aarch64-apple|aarch64-qualcomm|aarch64-n1x|aarch64-gb10
#
# While it exists, quattro-apply-hardware queues each hardware leaf in
# /var/lib/quattro/image/deferred-steps instead of running it, and arms
# quattro-provision-hardware.service. On the machine's first boot that service,
# or the platform's own first boot, retires the manifest (to target.booted) and
# runs the queue on the real hardware. Unknown manifest keys are ignored so the
# format can grow.
#
# An image carries no pacman master key of its own, or every machine flashed
# from it would share one: where install finalization would make the keyring
# (install/post-install/pacman.sh), a build requests it in
# /var/lib/quattro/image/pacman-keyring instead, and the first boot makes it
# before any deferred step runs.
#
# The manifest is also the only source of the image's platform: until that boot,
# quattro-hw-platform reports its platform instead of the build host's, so the
# initramfs, services and packages the build sets up are the target's. It reads
# the manifest by the same rules as quattro_image_read_manifest below.
#
# Root always uses the fixed paths. Only a non-root test may move them under
# QUATTRO_IMAGE_ROOT, so no environment variable can make a live system defer,
# skip or replay its hardware setup.

quattro_image_init() {
  if (( EUID == 0 )); then
    quattro_image_root=""
  elif [[ -n ${QUATTRO_IMAGE_ROOT:-} ]]; then
    quattro_image_root=$QUATTRO_IMAGE_ROOT
  else
    echo "Error: image setup state is root-owned; run as root" >&2
    return 1
  fi

  quattro_image_dir=$quattro_image_root/var/lib/quattro/image
  quattro_image_manifest=$quattro_image_dir/target
  quattro_image_queue=$quattro_image_dir/deferred-steps
  quattro_image_initramfs_baseline=$quattro_image_dir/initramfs-inputs
  quattro_image_boot_rebuild=$quattro_image_dir/boot-rebuild
  quattro_image_keyring_request=$quattro_image_dir/pacman-keyring
  quattro_image_unit=quattro-provision-hardware.service
  quattro_image_systemd_dir=$quattro_image_root/etc/systemd/system
}

# Owned by whoever runs this (root on a live system), not a symlink, and not
# writable by anyone else.
quattro_image_trusted() {
  local path=$1 owner mode

  [[ -e $path && ! -L $path ]] || return 1
  read -r owner mode < <(stat -c '%u %a' -- "$path") || return 1
  (( owner == EUID && (8#$mode & 8#022) == 0 ))
}

quattro_image_manifest_present() {
  [[ -e $quattro_image_manifest || -L $quattro_image_manifest ]]
}

# Sets quattro_image_platform, or fails without guessing whether this is a build.
quattro_image_read_manifest() {
  local line key value format="" platform=""

  if [[ ! -d $quattro_image_dir || ! -f $quattro_image_manifest ]] || ! quattro_image_trusted "$quattro_image_dir" ||
    ! quattro_image_trusted "$quattro_image_manifest"; then
    echo "Error: $quattro_image_manifest is not a root-owned regular file" >&2
    return 1
  fi

  while IFS= read -r line || [[ -n $line ]]; do
    [[ -n $line && $line != \#* ]] || continue
    if [[ $line != *=* ]]; then
      echo "Error: $quattro_image_manifest is malformed: $line" >&2
      return 1
    fi
    key=${line%%=*}
    value=${line#*=}
    case $key in
      format) format=$value ;;
      platform) platform=$value ;;
    esac
  done <"$quattro_image_manifest"

  if [[ $format != "1" ]]; then
    echo "Error: $quattro_image_manifest is not format=1" >&2
    return 1
  fi
  # Builders written before the platform names settled say apple-silicon,
  # generic-aarch64, generic or qualcomm (as quattro-hw-platform reads them too).
  case $platform in
    apple-silicon) platform=aarch64-apple ;;
    generic-aarch64) platform=aarch64 ;;
    generic) platform=x86 ;;
    qualcomm) platform=aarch64-qualcomm ;;
  esac
  case $platform in
    x86 | aarch64 | aarch64-apple | aarch64-qualcomm | aarch64-n1x | aarch64-gb10) ;;
    *)
      echo "Error: $quattro_image_manifest names no known platform: ${platform:-none}" >&2
      return 1
      ;;
  esac

  quattro_image_platform=$platform
}

# Holds the image state still for the rest of the caller, so hardware setup and
# a first-boot run never overlap or decide from a manifest the other retires.
# A machine that was never an image has no state and nothing to lock.
quattro_image_lock() {
  [[ -e $quattro_image_dir || -L $quattro_image_dir ]] || return 0
  if [[ ! -d $quattro_image_dir ]] || ! quattro_image_trusted "$quattro_image_dir"; then
    echo "Error: $quattro_image_dir is not a root-owned directory" >&2
    return 1
  fi
  exec {quattro_image_lock_fd}>>"$quattro_image_dir/lock" || return 1
  flock "$quattro_image_lock_fd"
}

quattro_image_write_queue() {
  local tmp

  tmp=$(mktemp "$quattro_image_dir/.deferred-steps.XXXXXX") || return 1
  if (( $# )); then
    printf '%s\n' "$@" >"$tmp" || { rm -f "$tmp"; return 1; }
  fi
  chmod 0644 "$tmp" && mv -f "$tmp" "$quattro_image_queue"
}

# Queue every leaf install/hardware/all.sh would run, in its order, running
# none of them, then arm the first-boot service. Rerunning rewrites the same queue.
quattro_image_defer_hardware() {
  local listing
  local -a steps=()

  listing=$(
    run_logged() {
      [[ $1 == "$QUATTRO_INSTALL"/hardware/*.sh && $1 != *..* ]] || {
        echo "Error: cannot defer a step outside the hardware setup: $1" >&2
        exit 1
      }
      printf 'install/%s\n' "${1#"$QUATTRO_INSTALL"/}"
    }
    source "$QUATTRO_INSTALL/hardware/all.sh"
  ) || return 1

  [[ -z $listing ]] || mapfile -t steps <<<"$listing"
  quattro_image_write_queue "${steps[@]}" || return 1

  install -d -m 0755 "$quattro_image_systemd_dir/multi-user.target.wants" &&
    install -m 0644 "$QUATTRO_INSTALL/provisioning/$quattro_image_unit" "$quattro_image_systemd_dir/$quattro_image_unit" &&
    ln -sfn "/etc/systemd/system/$quattro_image_unit" "$quattro_image_systemd_dir/multi-user.target.wants/$quattro_image_unit" ||
    return 1

  echo "Image build for $quattro_image_platform: deferred ${#steps[@]} hardware steps to first boot"
}
