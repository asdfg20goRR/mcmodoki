#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

if [[ ! -x ./minecraft ]]; then
    echo "minecraft was not found. Run ./build_game.sh first." >&2
    exit 1
fi

exec ./minecraft "$@"
