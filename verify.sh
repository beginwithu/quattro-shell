#!/bin/bash

set -euo pipefail

PROJECT_DIR=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
failures=0

pass() { printf 'PASS: %s\n' "$1"; }
fail() { printf 'FAIL: %s\n' "$1" >&2; failures=$((failures + 1)); }

echo "==> 1. Checking Bash & Python script syntax..."
script_errors=0
while IFS= read -r -d '' script; do
  shebang=$(head -n1 "$script" 2>/dev/null || true)
  if [[ $shebang == *bash* || $script == "$PROJECT_DIR/default/bash/"* ]]; then
    if ! bash -n "$script" 2>/dev/null; then
      echo "  Bash syntax error in: $script" >&2
      script_errors=$((script_errors + 1))
    fi
  elif [[ $shebang == *python* ]]; then
    if ! python3 -c 'import ast,sys; ast.parse(open(sys.argv[1],"rb").read())' "$script" 2>/dev/null; then
      echo "  Python syntax error in: $script" >&2
      script_errors=$((script_errors + 1))
    fi
  fi
done < <(find "$PROJECT_DIR/install.sh" "$PROJECT_DIR/verify.sh" "$PROJECT_DIR/bin" "$PROJECT_DIR/default/bash" -type f -print0)
(( script_errors == 0 )) && pass "All Bash & Python scripts pass syntax checks" || fail "$script_errors script syntax error(s)"

echo "==> 2. Checking Lua syntax..."
if command -v luac >/dev/null 2>&1; then
  lua_errors=0
  while IFS= read -r -d '' lua_file; do
    if ! luac -p "$lua_file" 2>/dev/null; then
      echo "  Lua syntax error in: $lua_file" >&2
      lua_errors=$((lua_errors + 1))
    fi
  done < <(find "$PROJECT_DIR/config/hypr" "$PROJECT_DIR/default/hypr" -name '*.lua' -type f -print0)
  (( lua_errors == 0 )) && pass "All Hyprland Lua files pass luac -p" || fail "$lua_errors Lua syntax error(s)"
else
  echo "SKIP: luac not installed (skipping Lua bytecode check)"
fi

echo "==> 3. Checking JSON configs..."
json_errors=0
for jf in \
  "$PROJECT_DIR/config/quattro/shell.json" \
  "$PROJECT_DIR/config/qtengine/config.json" \
  "$PROJECT_DIR/etc/fastfetch/config.jsonc"; do
  if ! python3 -m json.tool "$jf" >/dev/null 2>&1; then
    echo "  Invalid JSON: $jf" >&2
    json_errors=$((json_errors + 1))
  fi
done
(( json_errors == 0 )) && pass "All JSON config files parse cleanly" || fail "$json_errors JSON error(s)"

echo "==> 4. Verifying Arch repository package lists..."
if command -v pacman >/dev/null 2>&1; then
  missing=$(comm -23 \
    <(grep -hvE '^[[:space:]]*(#|$)' "$PROJECT_DIR/packages/arch-core.packages" | sort -u) \
    <(pacman -Ssq | sort -u))
  if [[ -z $missing ]]; then
    pass "All packages in arch-core.packages exist in pacman repos"
  else
    echo "$missing" | sed 's/^/  Missing package: /' >&2
    fail "Some packages were not found in pacman sync DB"
  fi
else
  echo "SKIP: pacman not found"
fi

echo "==> 5. Running isolated sandbox install & theme rendering test..."
tmp_home=$(mktemp -d)
trap 'rm -rf "$tmp_home"' EXIT

if HOME="$tmp_home" XDG_CONFIG_HOME="$tmp_home/.config" XDG_DATA_HOME="$tmp_home/.local/share" XDG_STATE_HOME="$tmp_home/.local/state" \
  QUATTRO_INSTALL_SANDBOX=1 "$PROJECT_DIR/install.sh" >/dev/null; then
  pass "Sandbox install.sh succeeded"
else
  fail "Sandbox install.sh failed"
fi

expected_files=(
  "$tmp_home/.bashrc"
  "$tmp_home/.XCompose"
  "$tmp_home/.config/hypr/hyprland.lua"
  "$tmp_home/.config/alacritty/alacritty.toml"
  "$tmp_home/.config/kitty/kitty.conf"
  "$tmp_home/.config/foot/foot.ini"
  "$tmp_home/.config/ghostty/config"
  "$tmp_home/.config/btop/btop.conf"
  "$tmp_home/.config/btop/themes/current.theme"
  "$tmp_home/.config/starship.toml"
  "$tmp_home/.config/tmux/tmux.conf"
  "$tmp_home/.config/chrome-flags.conf"
  "$tmp_home/.config/chromium-flags.conf"
  "$tmp_home/.local/share/quattro/bin/quattro"
  "$tmp_home/.local/bin/quattro"
  "$tmp_home/.local/state/quattro/current/theme/colors.toml"
  "$tmp_home/.local/state/quattro/current/theme/alacritty.toml"
  "$tmp_home/.local/state/quattro/current/theme/kitty.conf"
  "$tmp_home/.local/state/quattro/current/theme/foot.ini"
  "$tmp_home/.local/state/quattro/current/theme/ghostty.conf"
  "$tmp_home/.local/state/quattro/current/theme/btop.theme"
  "$tmp_home/.local/state/quattro/current/theme/hyprland.lua"
  "$tmp_home/.local/state/quattro/current/theme/qtengine.colors"
  "$tmp_home/.local/state/quattro/current/theme/shell.toml"
)

missing_deployed=0
for f in "${expected_files[@]}"; do
  if [[ ! -e $f ]]; then
    echo "  Missing deployed artifact: $f" >&2
    missing_deployed=$((missing_deployed + 1))
  fi
done
(( missing_deployed == 0 )) && pass "All expected configs and rendered theme files exist" || fail "$missing_deployed missing deployed file(s)"

if grep -q '__QUATTRO_PATH__' "$tmp_home/.config/chromium-flags.conf" || grep -q '/usr/share/quattro' "$tmp_home/.config/chromium-flags.conf"; then
  fail "chromium-flags.conf still contains unresolved path"
else
  pass "chromium-flags.conf extension paths resolved to sandbox QUATTRO_PATH"
fi

if [[ $(HOME="$tmp_home" QUATTRO_PATH="$tmp_home/.local/share/quattro" "$tmp_home/.local/bin/quattro-version-channel") == "arch-standalone" ]]; then
  pass "quattro-version-channel reports arch-standalone"
else
  fail "quattro-version-channel output unexpected"
fi

if (( failures == 0 )); then
  echo
  echo "ALL CHECKS PASSED."
  exit 0
else
  echo
  echo "$failures CHECK(S) FAILED." >&2
  exit 1
fi
