# Snapdragon cameras run through libcamera, which apps reach through
# PipeWire's libcamera backend.

if quattro-hw-aarch64-qualcomm; then
  quattro-pkg-add pipewire-libcamera
fi
