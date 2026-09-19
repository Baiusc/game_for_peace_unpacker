#!/bin/sh
set -eu
copy=${1:?copy path required}
orig=${2:?original path required}
cp "$orig" "$copy"
