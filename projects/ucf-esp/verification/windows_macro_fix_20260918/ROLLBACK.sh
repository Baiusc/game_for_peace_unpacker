#!/bin/sh
set -eu
MODIFIED_FILE=${1:-MODIFIED_FILE}
ORIGINAL_FILE=${2:-ORIGINAL_FILE}
cp "$ORIGINAL_FILE" "$MODIFIED_FILE"
cmp -s "$MODIFIED_FILE" "$ORIGINAL_FILE"
printf '%s\n' 'rollback: restored original input_sim.cpp'
