#!/usr/bin/env bash
set -euo pipefail
export NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
export PATH="$NXDK_DIR/bin:/clang64/bin:/mingw64/bin:/usr/bin:$PATH"
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build/xbox"
cd "$root/build/xbox"
# Repack changed disc flags without make -W default.xbe, which can suppress
# rebuilding that XBE even when compilation produces a newer main.exe.
if [[ "${1:-}" == "--repack" ]]; then
    shift
    rm -f "$root/build/xbox/redfaction-diagnostic.iso"
fi
exec make -f "$root/platforms/xbox/Makefile" -j4 "$@"
