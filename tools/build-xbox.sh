#!/usr/bin/env bash
set -euo pipefail
export NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
export PATH="$NXDK_DIR/bin:/clang64/bin:/mingw64/bin:/usr/bin:$PATH"
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build/xbox"
cd "$root/build/xbox"
exec make -f "$root/platforms/xbox/Makefile" -j4 "$@"
