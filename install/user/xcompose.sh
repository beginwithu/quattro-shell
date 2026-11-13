# Set default XCompose that is triggered with Shift + CapsLock
tee ~/.XCompose >/dev/null <<EOF
# Run quattro-restart-xcompose to apply changes

# Include fast emoji access
include "${QUATTRO_PATH:-$HOME/.local/share/quattro}/default/xcompose"

# Identification
<Multi_key> <space> <n> : "$QUATTRO_USER_NAME"
<Multi_key> <space> <e> : "$QUATTRO_USER_EMAIL"
EOF
