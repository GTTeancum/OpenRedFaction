#!/usr/bin/env bash
# Opt-in stock-64-MiB interactive package. Never edit the diagnostic disc or
# write through an asset hardlink: original inputs are borrowed read-only.
set -euo pipefail
export NXDK_DIR="${NXDK_DIR:-/c/nxdk}"
export PATH="$NXDK_DIR/bin:/clang64/bin:/mingw64/bin:/usr/bin:$PATH"
root="$(cd "$(dirname "$0")/.." && pwd)"
if [[ "${1:-}" == "--repack" ]]; then shift; fi
if [[ $# != 0 ]]; then
    echo "Usage: tools/build-xbox.sh --tester [--repack]" >&2
    exit 1
fi
if [[ "${RF_GEOMOD_EXPANDED_PROFILE:-0}" != 0 ]]; then
    echo "The tester package requires RF_GEOMOD_EXPANDED_PROFILE=0 (stock 64 MiB)." >&2
    exit 1
fi
export RF_GEOMOD_EXPANDED_PROFILE=0
disc="$root/build/xbox/tester-disc"
iso="$root/build/xbox/redfaction-tester.iso"
if [[ -L "$disc" || -L "$iso" ]]; then
    echo "Tester output paths must not be symlinks." >&2
    exit 1
fi
mkdir -p "$disc"
# A separate exact-content tree cannot inherit replay files, grants, moved
# starts, forced events, frame limits or developer-room flags from a harness.
shopt -s nullglob dotglob
for path in "$disc"/*; do
    if [[ ! -f "$path" || -L "$path" ]]; then
        echo "Unexpected tester package entry: $path" >&2
        exit 1
    fi
    case "${path##*/}" in
        default.xbe|tester-profile.flag|bluebeard.bty|geomod-template.bin|driller-single.bin|driller-double.bin) ;;
        tables.vpp|levels1.vpp|levels2.vpp|levels3.vpp|levelsm.vpp|maps1.vpp|maps2.vpp|maps3.vpp|maps4.vpp|maps_en.vpp|meshes.vpp|motions.vpp|audio.vpp|music.vpp|ui.vpp) ;;
        *) echo "Refusing unexpected tester package file: $path" >&2; exit 1 ;;
    esac
done
borrow_asset() {
    local source="$1" destination="$disc/$2"
    if [[ ! -f "$source" || -L "$source" ]]; then
        echo "Required regular input is missing: $source" >&2
        exit 1
    fi
    if [[ -e "$destination" && "$source" -ef "$destination" ]]; then return; fi
    # ln replaces the directory entry, never writes existing linked bytes.
    # Do not silently copy a second full asset tree on cross-volume failure.
    if ! ln -f "$source" "$destination"; then
        echo "Tester inputs and build output must support same-filesystem hardlinks; no asset copy fallback was attempted." >&2
        exit 1
    fi
}
for archive in tables levels1 levels2 levels3 levelsm maps1 maps2 maps3 maps4 maps_en meshes motions audio music ui; do
    borrow_asset "$root/Installed_Game/$archive.vpp" "$archive.vpp"
done
borrow_asset "$root/Installed_Game/bluebeard.bty" bluebeard.bty
for shape in geomod-template driller-single driller-double; do
    borrow_asset "$root/build/data/$shape.bin" "$shape.bin"
done
# Recreate the owned marker instead of writing through a possible hardlink.
rm -f "$disc/tester-profile.flag"
printf 'RFTEST1\n' > "$disc/tester-profile.flag"
profile_stamp="$root/build/xbox/geomod-profile.stamp"
if [[ ! -f "$profile_stamp" ]] || [[ "$(cat "$profile_stamp")" != 0 ]]; then
    printf '0\n' > "$profile_stamp"
fi
# The marker and input membership determine this image, so repack every opt-in
# invocation. The diagnostic ISO/disc remain untouched; objects/main.exe share
# the same runtime-marker binary and the existing stock-profile build stamp.
rm -f "$iso"
printf 'Tester package: %s\nPrivate original assets are included; keep this image local.\n' "$iso"
cd "$root/build/xbox"
exec make -f "$root/platforms/xbox/Makefile" -j4 all NXDK_ONLY= \
    OUTPUT_DIR="$disc" GEN_XISO=redfaction-tester.iso RF_GEOMOD_EXPANDED_PROFILE=0
