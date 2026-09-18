#!/usr/bin/env bash
set -euo pipefail
DIR="$(cd "$(dirname "$0")" && pwd)"
cp "$DIR/ORIGINAL_FILE" "$DIR/MODIFIED_FILE"
printf 'rollback restored MODIFIED_FILE from ORIGINAL_FILE\n'
