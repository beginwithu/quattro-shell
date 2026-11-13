quattro_log_to_stdout() {
  [[ ${QUATTRO_LOG_TO_STDOUT:-} == "1" || -z ${QUATTRO_INSTALL_LOG_FILE:-} ]]
}

quattro_log_line() {
  if quattro_log_to_stdout; then
    echo "$1"
  else
    echo "$1" >>"$QUATTRO_INSTALL_LOG_FILE"
  fi
}

start_install_log() {
  if ! quattro_log_to_stdout; then
    mkdir -p "$(dirname "$QUATTRO_INSTALL_LOG_FILE")"
    touch "$QUATTRO_INSTALL_LOG_FILE"
    chmod 666 "$QUATTRO_INSTALL_LOG_FILE" 2>/dev/null || true
  fi

  export QUATTRO_START_TIME="${QUATTRO_START_TIME:-$(date '+%Y-%m-%d %H:%M:%S')}"
  export QUATTRO_START_EPOCH="${QUATTRO_START_EPOCH:-$(date +%s)}"

  quattro_log_line "=== Quattro Setup Started: $QUATTRO_START_TIME ==="
}

stop_install_log() {
  local end_time end_epoch duration mins secs
  end_time=$(date '+%Y-%m-%d %H:%M:%S')
  end_epoch=$(date +%s)

  quattro_log_line "=== Quattro Setup Completed: $end_time ==="

  if [[ -n ${QUATTRO_START_EPOCH:-} ]]; then
    duration=$((end_epoch - QUATTRO_START_EPOCH))
    mins=$((duration / 60))
    secs=$((duration % 60))
    quattro_log_line "Quattro setup: ${mins}m ${secs}s"
  fi
}

run_logged() {
  local script="$1"
  local exit_code errexit_was_set=0

  quattro_log_line "[$(date '+%Y-%m-%d %H:%M:%S')] Starting: $script"

  case $- in
    *e*)
      errexit_was_set=1
      set +e
      ;;
  esac

  local runner=(bash -eE)
  if [[ ${QUATTRO_INSTALL_DEBUG:-} == "1" ]]; then
    runner=(bash -x -eE)
  fi

  if quattro_log_to_stdout; then
    PS4='+ ${BASH_SOURCE[0]##*/}:${LINENO}:${FUNCNAME[0]:-main}: ' \
      "${runner[@]}" -c 'source "$1"' bash "$script" </dev/null 2>&1
  else
    PS4='+ ${BASH_SOURCE[0]##*/}:${LINENO}:${FUNCNAME[0]:-main}: ' \
      "${runner[@]}" -c 'source "$1"' bash "$script" </dev/null >>"$QUATTRO_INSTALL_LOG_FILE" 2>&1
  fi

  exit_code=$?
  (( errexit_was_set )) && set -e

  if (( exit_code == 0 )); then
    quattro_log_line "[$(date '+%Y-%m-%d %H:%M:%S')] Completed: $script"
  else
    quattro_log_line "[$(date '+%Y-%m-%d %H:%M:%S')] Failed: $script (exit code: $exit_code)"
  fi

  return $exit_code
}
