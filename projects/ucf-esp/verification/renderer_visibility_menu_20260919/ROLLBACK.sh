#!/usr/bin/env bash
set -euo pipefail
repo="$(cd "$(dirname "$0")/../../../../" && pwd)"
git -C "$repo" show HEAD:projects/ucf-esp/tools/frida_dump.js > "$repo/projects/ucf-esp/tools/frida_dump.js"
