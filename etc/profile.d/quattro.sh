if [ -r "${QUATTRO_PATH:-$HOME/.local/share/quattro}/default/bash/env-bootstrap" ]; then
  . "${QUATTRO_PATH:-$HOME/.local/share/quattro}/default/bash/env-bootstrap"
elif [ -r /usr/share/quattro/default/bash/env-bootstrap ]; then
  . /usr/share/quattro/default/bash/env-bootstrap
fi
