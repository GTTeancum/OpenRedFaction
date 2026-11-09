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
    if [[ "${path##*/}" == movies && -d "$path" && ! -L "$path" ]]; then
        for movie in "$path"/*; do
            case "${movie##*/}" in thqlogo.pss|intro.pss|ending.pss) [[ -f "$movie" && ! -L "$movie" ]] && continue ;; esac
            echo "Refusing unexpected tester package file: $movie" >&2; exit 1
        done
        continue
    fi
    if [[ ! -f "$path" || -L "$path" ]]; then
        echo "Unexpected tester package entry: $path" >&2
        exit 1
    fi
    case "${path##*/}" in
        default.xbe|tester-profile.flag|openrf.ini|rfdds.vpp|bluebeard.bty|geomod-template.bin|driller-single.bin|driller-double.bin) ;;
        tables.vpp|levels1.vpp|levels2.vpp|levels3.vpp|levelsm.vpp|maps1.vpp|maps2.vpp|maps3.vpp|maps4.vpp|maps_en.vpp|meshes.vpp|motions.vpp|audio.vpp|music.vpp|ui.vpp|ps2mainmenu.vpp|ps2menu.vpp) ;;
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
# Optional user-supplied PS2 Main Menu overlay, kept outside original inputs.
# Never download assets during a build or publish them with project source.
if [[ -n "${RF_MENU_OVERLAY_VPP:-}" ]]; then
    if [[ ! -f "$RF_MENU_OVERLAY_VPP" || -L "$RF_MENU_OVERLAY_VPP" ]]; then
        echo "RF_MENU_OVERLAY_VPP must name a regular supplied VPP." >&2
        exit 1
    fi
    menu_digest="$(sha256sum "$RF_MENU_OVERLAY_VPP" | cut -d ' ' -f1)"
    if [[ "$menu_digest" != 2905b36544373c156e2671f2edb936cce25d8071bb4196811ff40e8374e2e05a ]]; then
        echo "Menu overlay differs from the requested PS2 Main Menu release." >&2
        exit 1
    fi
    borrow_asset "$RF_MENU_OVERLAY_VPP" ps2mainmenu.vpp
elif [[ -e "$disc/ps2mainmenu.vpp" ]]; then
    echo "Existing menu overlay requires explicit RF_MENU_OVERLAY_VPP admission." >&2
    exit 1
fi
borrow_asset "$root/Installed_Game/bluebeard.bty" bluebeard.bty
for shape in geomod-template driller-single driller-double; do
    borrow_asset "$root/build/data/$shape.bin" "$shape.bin"
done
# Optional PS2 menus (Xbox only), converted from the user's own PS2 disc image
# by tools/build_ps2_menu_pack.py; an absent pack keeps the PC menus.
ps2_iso="${RF_PS2_ISO:-$root/Red Faction (USA) (v2.00).iso}"
ps2_pack="$root/build/data/ps2menu.vpp"
if [[ -f "$ps2_iso" && ( ! -f "$ps2_pack" || "$ps2_iso" -nt "$ps2_pack" || "$root/tools/build_ps2_menu_pack.py" -nt "$ps2_pack" || "$root/tools/ps2_peg.py" -nt "$ps2_pack" ) ]]; then
    # Needs Pillow, numpy and PyAV: prefer the Windows Python over MSYS's.
    ps2_python="${RF_PS2_PYTHON:-}"
    if [[ -z "$ps2_python" ]]; then
        # A login MSYS shell drops the Windows user variables; search the usual install path.
        for candidate in /c/Users/*/AppData/Local/Programs/Python/Python3*/python.exe python python3; do
            if "$candidate" -c 'import PIL, numpy, av' >/dev/null 2>&1; then ps2_python="$candidate"; break; fi
        done
    fi
    if [[ -z "$ps2_python" ]]; then echo "PS2 menu pack needs a Python with Pillow, numpy and PyAV (set RF_PS2_PYTHON)." >&2; exit 1; fi
    "$ps2_python" -I "$root/tools/build_ps2_menu_pack.py" "$ps2_iso" "$ps2_pack"
fi
if [[ -f "$ps2_pack" ]]; then borrow_asset "$ps2_pack" ps2menu.vpp; fi
# Movies both releases share (THQ logo, intro, ending; no trailers),
# copied unchanged from the same PS2 disc image by tools/extract_ps2_movies.py.
ps2_movies="$root/build/data/movies"
if [[ -f "$ps2_iso" ]]; then
    "${ps2_python:-python}" -I "$root/tools/extract_ps2_movies.py" "$ps2_iso" "$ps2_movies"
fi
if [[ -d "$ps2_movies" ]]; then
    mkdir -p "$disc/movies"
    for movie in thqlogo intro ending; do
        if [[ -f "$ps2_movies/$movie.pss" ]]; then borrow_asset "$ps2_movies/$movie.pss" "movies/$movie.pss"; fi
    done
fi
# Optional full-size DXT texture pack generated from the same archives.
if [[ -f "$root/build/data/rfdds.vpp" ]]; then borrow_asset "$root/build/data/rfdds.vpp" rfdds.vpp; fi
# Recreate the owned marker instead of writing through a possible hardlink.
rm -f "$disc/tester-profile.flag" "$disc/openrf.ini"
# User-editable settings (frame rate and fallback) beside default.xbe.
cp "$root/platforms/xbox/openrf.ini" "$disc/openrf.ini"
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
