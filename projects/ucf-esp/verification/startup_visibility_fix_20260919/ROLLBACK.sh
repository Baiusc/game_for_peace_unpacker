#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../../../" && pwd)"
target="$repo/projects/ucf-esp/tools/frida_dump.js"
git -C "$repo" show HEAD:projects/ucf-esp/tools/frida_dump.js > "$target"
