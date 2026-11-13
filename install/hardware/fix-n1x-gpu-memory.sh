# NVIDIA N1x: the firmware reserves a Windows-sized GPU carve-out (62.5 GiB on
# the XPS 16, 32 GiB on the Lenovo Pro 9). Have it reserve 4 GiB from the next
# boot on; see quattro-apply-n1x-gpu-memory.

quattro-hw-aarch64-n1x || return 0

quattro-apply-n1x-gpu-memory
