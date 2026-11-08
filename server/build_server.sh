#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

if ! command -v gcc >/dev/null 2>&1; then
    echo "gcc was not found. Install GCC before building the server." >&2
    exit 1
fi

gcc server.c -O2 -std=c11 -Wall -Wextra -Werror -lm -o server
echo "Built server"
