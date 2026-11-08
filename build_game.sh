#!/usr/bin/env bash
set -euo pipefail

cd -- "$(dirname -- "${BASH_SOURCE[0]}")"

if ! command -v cc >/dev/null 2>&1; then
    echo "A C compiler was not found. Install build-essential." >&2
    exit 1
fi

if command -v pkg-config >/dev/null 2>&1; then
    pkg_config=pkg-config
elif command -v pkgconf >/dev/null 2>&1; then
    pkg_config=pkgconf
else
    echo "pkg-config was not found. Run: sudo apt install pkg-config libglfw3-dev libfreetype-dev libgl1-mesa-dev libglu1-mesa-dev" >&2
    exit 1
fi

if ! "$pkg_config" --exists glfw3 freetype2; then
    echo "GLFW or FreeType development files are missing. Run: sudo apt install libglfw3-dev libfreetype-dev" >&2
    exit 1
fi

read -r -a dependency_cflags <<< "$("$pkg_config" --cflags glfw3 freetype2)"
read -r -a dependency_libs <<< "$("$pkg_config" --libs glfw3 freetype2)"

if ! printf '#include <GL/gl.h>\n#include <GL/glu.h>\n' | cc "${dependency_cflags[@]}" -E -x c - >/dev/null 2>&1; then
    echo "OpenGL development headers are missing. Run: sudo apt install libgl1-mesa-dev libglu1-mesa-dev" >&2
    exit 1
fi

cc main.c -O2 -std=c11 -Wall -Wextra -Werror \
    "${dependency_cflags[@]}" \
    -o minecraft "${dependency_libs[@]}" -lGL -lGLU -lm
echo "Built minecraft"
