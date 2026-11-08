#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

if [[ ! -x ./server ]]; then
    echo "server binary was not found. Run ./build_server.sh first." >&2
    exit 1
fi

exec ./server "$@"
