#!/bin/sh
set -eu
game_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$game_dir/tools/launch.py" "$@"
