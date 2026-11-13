# Speakers and microphones for the ASUS ProArt PZ13 HT5306QA (Snapdragon X Plus
# X1P-42-100).
#
# The sound card asks for a board-named topology that linux-firmware doesn't
# carry, so without the package it never registers and there is no speaker or
# microphone. Its UCM profile comes with alsa-ucm-conf.

if quattro-hw-aarch64-qualcomm && quattro-hw-match "HT5306QA"; then
  quattro-pkg-add asus-proart-pz13-audio
fi
