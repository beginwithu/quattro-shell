# Speakers, headset and microphones for the Acer Aspire 16 AI A16-11M
# (Snapdragon X X1-26-100).
#
# The sound card asks for a board-named topology that linux-firmware doesn't
# carry, so without the package it never registers and there is no speaker or
# microphone. Its UCM profile comes with alsa-ucm-conf.

if quattro-hw-aarch64-qualcomm && quattro-hw-match "Aspire A16-11M"; then
  quattro-pkg-add acer-aspire-16-ai-audio
fi
