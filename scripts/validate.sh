#!/usr/bin/env bash
# Stress-test the plugin with pluginval at its strictest level, several times.
#
#   scripts/validate.sh [plugin.vst3] [seed]
#
# pluginval's tests use random values, so one pass proves less than it looks.
# This runs five seeds. A failure prints its seed; pass it as the second
# argument to repeat exactly that run.

set -euo pipefail

plugin="${1:-$HOME/Library/Audio/Plug-Ins/VST3/THE89TH.vst3}"
seeds=("${2:-1}")
[[ $# -lt 2 ]] && seeds=(1 2 3 4 5)

pluginval="${PLUGINVAL:-}"
if [[ -z "$pluginval" ]]; then
    if command -v pluginval >/dev/null 2>&1; then
        pluginval="$(command -v pluginval)"
    elif [[ -x /Applications/pluginval.app/Contents/MacOS/pluginval ]]; then
        pluginval=/Applications/pluginval.app/Contents/MacOS/pluginval
    else
        echo "pluginval not found. Install it with 'brew install --cask pluginval'," >&2
        echo "or download it from https://github.com/Tracktion/pluginval/releases," >&2
        echo "or point PLUGINVAL at the binary." >&2
        exit 1
    fi
fi

if [[ ! -e "$plugin" ]]; then
    echo "No plugin at $plugin. Build first, or pass a path." >&2
    exit 1
fi

failed=0
for seed in "${seeds[@]}"; do
    log="$(mktemp)"
    if "$pluginval" --strictness-level 10 --validate-in-process --random-seed "$seed" \
                    --validate "$plugin" >"$log" 2>&1; then
        echo "seed $seed: pass"
    else
        echo "seed $seed: FAIL"
        grep '!!!' "$log" | head -5 || true
        echo "  full log: $log"
        failed=1
        continue
    fi
    rm -f "$log"
done

exit "$failed"
