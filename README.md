# Red Faction reconstruction for Original Xbox and PC

Work in progress: shared C/C++ reconstruction targeting a stock 64 MiB Xbox
through NXDK, with a maintained 32-bit PC build. Single-player comes first and
visual quality must meet the PS2 version. **There is no playable port yet.**

Current outputs include archive/level diagnostics, a shared static geometry,
texture and lightmap preview for PC and Xbox, and reconstructed filename checksum,
entity eye-update and character tag-lookup routines. Model loading, gameplay and
full campaign reconstruction remain open. See [TO-DO.MD](TO-DO.MD),
[architecture](docs/ARCHITECTURE.md), and [provenance](docs/PROVENANCE.md).

The original installation is read from
`D:/Programming/GitHub/OpenRedFaction/Installed_Game`. Do not edit it or put
its binary/assets into tracked source. Local analysis and builds stay ignored.

The project root is `D:/Programming/GitHub/OpenRedFaction`. Shared C code is in
`src/core`, platform entry points in `src/platform/pc` and `src/platform/xbox`,
and public headers in `include/rf`. The earlier C-drive workspace now contains
relocation pointers. Its generated builds are preserved in `local/legacy-build-c`;
fresh builds use the new project's `build` directory.

## PC build and checks

From this repository in PowerShell, with Visual Studio 2022 C++ installed:

```powershell
cmake -S . -B build/pc -G 'Visual Studio 17 2022' -A Win32
cmake --build build/pc --config Release
ctest --test-dir build/pc -C Release --output-on-failure
python tools/inventory.py 'D:\Programming\GitHub\OpenRedFaction\Installed_Game'
python tools/verify_archives.py
python tools/inspect_levels.py
python tools/verify_levels.py
python tools/inspect_geometry.py
python tools/verify_geometry.py
python tools/test_geometry_corruption.py
python tools/verify_images.py
python tools/inspect_lightmaps.py
python tools/verify_lightmaps.py
python -m pip install --target local/python unicorn==2.1.4
python tools/verify_checksum.py 'D:\Programming\GitHub\OpenRedFaction\Installed_Game\RF.exe'
```

## Xbox diagnostic

Generate the current PC base-texture reference (Pillow is required by image checks):

```powershell
./build/pc/Release/rf_pc_preview.exe Installed_Game/levels1.vpp L1S1.rfl artifacts/live-mines-textured-pc.ppm Installed_Game/maps1.vpp Installed_Game/maps2.vpp Installed_Game/maps3.vpp Installed_Game/maps4.vpp Installed_Game/maps_en.vpp
```

Omit the map archive arguments for the older untextured reference. Textured output
now includes linear filtering and lightmaps. The shared-grid PC/Xbox comparison
passes for this frozen view; original-game and PS2 parity remain unverified.

Local prerequisites: NXDK at `C:/nxdk`, MSYS2 at `C:/msys64`, Clang64 compiler
and MinGW64 runtime DLLs for the SDK's existing host tools.

```powershell
$env:MSYSTEM = 'CLANG64'
& 'C:\msys64\usr\bin\bash.exe' --noprofile --norc tools/build-xbox.sh
Copy-Item -LiteralPath 'D:\Programming\GitHub\OpenRedFaction\Installed_Game\tables.vpp' -Destination build/xbox/disc/tables.vpp
Copy-Item -LiteralPath 'D:\Programming\GitHub\OpenRedFaction\Installed_Game\levels1.vpp' -Destination build/xbox/disc/levels1.vpp
foreach ($name in @('maps1.vpp','maps2.vpp','maps3.vpp','maps4.vpp','maps_en.vpp')) {
    Copy-Item -LiteralPath (Join-Path 'Installed_Game' $name) -Destination (Join-Path 'build/xbox/disc' $name)
}
& 'C:\msys64\usr\bin\bash.exe' --noprofile --norc tools/build-xbox.sh
```

Outputs: `build/xbox/disc/default.xbe` and
`build/xbox/redfaction-diagnostic.iso`. The disc contains a private copy of the
original tables, first campaign-level archive, and five map archives, so keep that package local.
The diagnostic reports memory, reads Live Mines' section directory and spawn
transform, and loads its static geometry and base textures within explicit budgets.
It draws three base-textured and lightmapped geometry frames; gameplay remains open.

## XEMU diagnostic validation

```powershell
python tools/xemu_smoke.py
python tools/xemu_smoke.py --reference artifacts/live-mines-textured-pc.ppm
```

The harness starts only its own emulator process with a hidden-window launch,
an isolated configuration, no input bindings, a copied EEPROM, `-snapshot`,
64 MiB RAM, muted audio, and localhost QMP telemetry. It shuts down its own process
and writes a report under `artifacts/xemu`. It never sends host input or captures
the desktop. The default `xemu` display backend is required by this installed
build for successful diagnostic boot; `--display none` loaded the image but faulted
inside the guest kernel before entry. Both installed 4627 debug and Complex
4627 v1.03 BIOSes passed earlier boot checks with the rendering backend. The latest
debug-BIOS run validates resident materials and their pixel checksum, then captures
the game's framebuffer from guest RAM. See docs/VALIDATION.md for the limited
lit PC/Xbox comparison; no campaign fidelity or frame rate claim is made.

## Headless Ghidra analysis

```powershell
./tools/analyze.ps1
```

This hashes `RF.exe`, creates/opens its matching project under `local/ghidra`,
and writes a function inventory and selected raw decompilations under
`artifacts/analysis`. Defaults use Ghidra 11.3.2 and Java 21 from the installed
local paths; script parameters override these paths. Community symbol addresses
are candidates until checked against the original executable.
