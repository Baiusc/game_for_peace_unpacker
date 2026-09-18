#!/bin/sh
set -eu
cp "$(dirname "$0")/ORIGINAL_FILE" "$(dirname "$0")/MODIFIED_FILE"
printf "rollback restored\n"
