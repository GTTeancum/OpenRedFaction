# Linux-hosted Xbox builds and bounded checks

The game remains an NXDK Xbox target. Linux is a build/emulator host, not an
additional gameplay implementation target. Use the normal stock-64-MiB profile
(`RF_GEOMOD_EXPANDED_PROFILE=0`). No image capture or host input is needed.

## Build

Provide a complete NXDK checkout and its standard LLVM, make, CMake, flex and
bison prerequisites. Build NXDK libraries with their own default flags first;
the game enables stricter warnings for project sources.

```sh
export NXDK_DIR=/absolute/path/to/nxdk
export PATH="$NXDK_DIR/bin:$PATH"
make -C "$NXDK_DIR" NXDK_ONLY=1 -j4
python3 tools/build_apu_probe.py --backend-only
bash tools/build-xbox.sh
```

The APU preparation selects the pinned official DSP assembler for Windows or
x86-64 Linux and verifies the platform-specific archive SHA-256. It preserves
the existing backend adaptations and provenance record.

The normal disc build still requires private `Installed_Game/*.vpp` inputs,
`Installed_Game/bluebeard.bty`, and the three prepared `build/data` templates.
Keep all original inputs, templates,
firmware, disc images and generated outputs out of Git.

## Runtime

Use an official Linux XEMU executable with an offscreen-capable SDL/OpenGL
backend, plus the user's existing private firmware and copied EEPROM. Set:

```sh
export RF_XEMU_ROOT=/absolute/path/to/private-xemu-inputs
export RF_XEMU_BINARY=/absolute/path/to/xemu
```

`RF_XEMU_ROOT` contains `eeprom.bin`, `MCPX/mcpx_1.0.bin`, and
`BIOS/xbox-4627_debug.bin`. The common bounded runner in
`tools/xemu_native_world_save.py` continues to create a per-run configuration,
copy the EEPROM, disable input auto-binding/networking, request exactly 64 MiB,
and verify memory size using QMP.

On Linux the common runner uses process-owned stdin/stdout QMP pipes, defaults
to SDL's `offscreen` backend and software OpenGL, and writes data/cache files
inside the run directory. Windows retains its existing TCP-QMP path. Guest
memory reads are unchanged. No framebuffer, screen or image capture is added.

The runner holds a Linux file lock for the emulator session and passes its
descriptor to the owned child, including when
separate process namespaces prevent process inventory from seeing another
harness. The child retains exclusion if its harness dies. A busy session is left
untouched. Continue running all Xbox checks
serially; do not start campaign-route checks.

Harness build helpers can use `xemu_host.xbox_build_command('--repack')` instead
of a hard-coded MSYS2 path. `RF_BASH` can override the host shell.

Focused host transport/environment/serialization checks:

```sh
python3 tests/xemu_host_tests.py
```

## Verification scope

On 2026-10-07, the cloud Linux host compiled and linked the existing stock-profile
Xbox XBE with Clang 19/NXDK, started official XEMU 0.8.136 with SDL offscreen and
Mesa llvmpipe, verified 67,108,864 bytes of RAM, and read the reconstructed game's
diagnostic signature through pipe QMP. A source-only boot reached the expected
missing-input error before level loading. This establishes the host path; it
does not establish vehicle gameplay without the private runtime inputs.
The subsequent exact-build Fighter/submarine checks with verified private
inputs are recorded in [Vehicle homing targets](VEHICLE-HOMING-TARGETS.md).
