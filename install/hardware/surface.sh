# The Marvell Wi-Fi firmware is for x86 Surfaces only; Snapdragon Surfaces use Qualcomm Wi-Fi.
if quattro-hw-surface && quattro-hw-x86; then
  quattro-pkg-add linux-firmware-marvell
fi
