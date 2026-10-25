#!/usr/bin/env bash
# Startup smoke test (#377) for Linux and macOS CI.
#
# Usage: scripts/startup-smoke.sh <app-binary> first|second
#   first   delete the settings store and log folder, then expect the
#           "default configuration created" line
#   second  keep the existing state and expect that line to be absent
#
# Runs the app with --startup-check (quits ~1 s after the login dialog shows)
# and checks the log: banner first, every startup step that began also ended,
# and the required R2.1 steps are present. Needs a display (Xvfb on Linux).
set -euo pipefail

app="$1"
mode="$2"

case "$(uname -s)" in
  Darwin)
    log_dir="$HOME/Library/Application Support/MaximumTrainer/MaximumTrainer_Redux"
    reset_settings() {
      defaults delete com.maximumtrainer.MaximumTrainer_Redux >/dev/null 2>&1 || true
      rm -f "$HOME/Library/Preferences/com.maximumtrainer.MaximumTrainer_Redux.plist"
    }
    ;;
  *)
    log_dir="${XDG_DATA_HOME:-$HOME/.local/share}/MaximumTrainer/MaximumTrainer_Redux"
    reset_settings() {
      rm -f "${XDG_CONFIG_HOME:-$HOME/.config}/MaximumTrainer/MaximumTrainer_Redux.conf"
    }
    ;;
esac
log="$log_dir/MaximumTrainer.log"

if [ "$mode" = first ]; then
  reset_settings
  rm -rf "$(dirname "$log_dir")"
fi

start_line=0
[ -f "$log" ] && start_line=$(wc -l < "$log")

code=0
# perl alarm instead of timeout(1), which macOS does not ship.
MT_NO_NETWORK=1 perl -e 'alarm shift; exec @ARGV' 60 "$app" --startup-check || code=$?

fail() { echo "::error::startup smoke ($mode): $*"; exit 1; }

[ -f "$log" ] || fail "log file not created at $log"
run_log=$(tail -n +"$((start_line + 1))" "$log")
echo "=== log for this run ($log) ==="
echo "$run_log"
echo "==============================="

[ "$code" -eq 0 ] || fail "app exited with code $code (142 = killed after 60 s)"

first=$(head -n 1 <<<"$run_log")
grep -q "starting ====" <<<"$first" || fail "first line is not the startup banner: $first"
for field in "Qt runtime" "OS: " "CPU architecture" "Command line" "Log file: " "Settings: "; do
  grep -q "$field" <<<"$run_log" || fail "banner field missing: $field"
done

for step in "configuration bootstrap" "QWebEngineProfile init" "metatype registration" \
            "GlobalVars: Account" "CredentialStore load" "GlobalVars: Settings" \
            "GlobalVars: SoundPlayer" "network managers" "GlobalVars: NetworkMonitor" \
            "stylesheet / theme" "login dialog shown"; do
  awk -v s="$step" 'index($0, s) && index($0, "… done (") { f = 1 } END { exit !f }' <<<"$run_log" \
    || fail "no 'done' line for step: $step"
done
while IFS= read -r label; do
  grep -qF "$label … done" <<<"$run_log" || fail "step began but never finished: $label"
done < <(sed -n 's/.*\[Startup\] \(.*\) … begin$/\1/p' <<<"$run_log")

if [ "$mode" = first ]; then
  grep -q "default configuration created" <<<"$run_log" || fail "first run did not create the default configuration"
else
  if grep -q "default configuration created" <<<"$run_log"; then
    fail "second run created the default configuration again"
  fi
fi
echo "startup smoke ($mode): OK"
