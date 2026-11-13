# A language the install media preset (/etc/quattro/language) becomes this
# user's desktop language; the system step generated its locale.
code=
[[ -r /etc/quattro/language ]] && read -r code </etc/quattro/language
if [[ -n $code ]]; then
  "$QUATTRO_PATH/bin/quattro-language" seed "$code" || echo "Could not set the preset language $code" >&2
fi
