#!/usr/bin/env bash
set -euo pipefail
BASE_DIR="$(cd "$(dirname "$0")" && pwd)"
TARGET="${1:-$BASE_DIR/MODIFIED_FILE}"
cp "$BASE_DIR/ORIGINAL_FILE" "$TARGET"
printf 'restored=%s\n' "$TARGET"
