#!/usr/bin/env bash
set -euo pipefail
export NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
export PATH="$NXDK_DIR/bin:/clang64/bin:/mingw64/bin:/usr/bin:$PATH"
root="$(cd "$(dirname "$0")/.." && pwd)"
mkdir -p "$root/build/xbox"
# The scene streams authored controller samples from the local original archive.
# Disc data remains ignored; the archive is never loaded wholesale into RAM.
mkdir -p "$root/build/xbox/disc"
if [[ "$root/Installed_Game/bluebeard.bty" -nt "$root/build/xbox/disc/bluebeard.bty" ]]; then
    cp "$root/Installed_Game/bluebeard.bty" "$root/build/xbox/disc/bluebeard.bty"
    rm -f "$root/build/xbox/redfaction-diagnostic.iso"
fi
if [[ "$root/Installed_Game/audio.vpp" -nt "$root/build/xbox/disc/audio.vpp" ]]; then
    cp "$root/Installed_Game/audio.vpp" "$root/build/xbox/disc/audio.vpp"
    rm -f "$root/build/xbox/redfaction-diagnostic.iso"
fi
# Original level GeoMod textures (including rock02.tga) live in ui.vpp.
if [[ "$root/Installed_Game/ui.vpp" -nt "$root/build/xbox/disc/ui.vpp" ]]; then
    cp "$root/Installed_Game/ui.vpp" "$root/build/xbox/disc/ui.vpp"
    rm -f "$root/build/xbox/redfaction-diagnostic.iso"
fi
# Prepared from the user's verified executable; no original geometry is tracked.
if [[ ! -f "$root/build/data/geomod-template.bin" ]]; then
    echo "Run python tools/inspect_geomod_template.py and python tools/pack_geomod_template.py first." >&2
    exit 1
fi
if [[ "$root/build/data/geomod-template.bin" -nt "$root/build/xbox/disc/geomod-template.bin" ]]; then
    cp "$root/build/data/geomod-template.bin" "$root/build/xbox/disc/geomod-template.bin"
    rm -f "$root/build/xbox/redfaction-diagnostic.iso"
fi
cd "$root/build/xbox"
# Repack changed disc flags without make -W default.xbe, which can suppress
# rebuilding that XBE even when compilation produces a newer main.exe.
if [[ "${1:-}" == "--repack" ]]; then
    shift
    rm -f "$root/build/xbox/redfaction-diagnostic.iso"
fi
exec make -f "$root/platforms/xbox/Makefile" -j4 "$@"
