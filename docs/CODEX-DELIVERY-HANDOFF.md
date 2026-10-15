# OpenRedFaction: local build and actual-hardware handoff

Delivery status: October 15, 2026, 12:08 p.m. EDT. Menus and the remaining
NPC speech integration are built and ready for local Codex build/hardware
handoff. This does not establish actual-hardware or broad functional correctness.
The user's delivery goal was midnight Eastern ending October 15.

## Evidence and manifest

Passing native stock64 source: `0d5149c14d2a0ffc33b6c4859f8313ec7d7c1985`,
tree `f855fd404b651a37d55c47324c556823de2a4424`.
The matching published source is commit `773ba6024dc90f7f9e1a721d3e42b669d437f672`.
Later delivery documentation commits do not change executable source.

See [Actual16](HOURLY-20261015-1600.md): native compile/link/package and ordinary
XEMU startup passed, reaching 12 simulation and 10 presentation frames with
clean QMP shutdown. Postcheck confirmed unchanged neutral input and no XEMU.
This does not establish hardware operation, full gameplay, functional speech,
visual/audio correctness or save/load coverage.

Artifact SHA-256 identities:

- XBE: `f227d57227ef5702310d17b6464369d1837d013d743a0c937ded2e2054849fd9`
- Map: `eb35711f3b8fa6d7311f649dc95d307798a04d0d389a5974bc0dff26852c6cc8`
- ISO: `ea54151ef4d7e00790b2cd7a48643fc823005304909fc5380244534059e42538`
- Proof ZIP: `26e0664b55d2019d1888baf24918aa815b5801ef356c314f6d64bf50be76af63`

Use the private delivery manifest for current source archive/bundle and input
Library identities. Cloud paths are not Windows paths. Verify every transferred
file's hash before using it; metadata-only Library checks do not prove bytes.

## Local setup

Verify the actual local checkout and preserve user changes. Historical project
root is `D:/Programming/GitHub/OpenRedFaction`; historical tool paths are
`C:/nxdk` and `C:/msys64`. These are starting points, not verified current setup.
The repository builds with NXDK; RXDK Neighborhood is the planned deployment
workflow and still needs local inspection for exact commands and compatibility.

Verify the installed NXDK revision/submodules and toolchain. The recorded NXDK
baseline is `fb5a9a7a58a431e8d70a9e7da87898059df376c0`. Each build verifies the
PDCLib `fclose.c` source SHA-256 against
`26e58ed5d2ff31ea3575de3bde0eff5b53d3995f7a1fdd8dea77cc25ebc92fbd`.
Do not bypass `tools/prepare_checked_stdio.py` on a different SDK.

Prepare the complete audio backend with:

```
python tools/build_apu_probe.py --backend-only
```

It uses nxdk-audio `fc2deca2cc1e434805ac03ca7c2f500b3b028f36` and DSP assembler
0.1.3. Keep headers and `passthrough.out` along with its generated C sources;
copying only the C files is insufficient. The script checks dependency hashes.

## Private input manifest

`Installed_Game` requires `tables.vpp`, `levels1.vpp`, `levels2.vpp`,
`levels3.vpp`, `levelsm.vpp`, `maps1.vpp`, `maps2.vpp`, `maps3.vpp`, `maps4.vpp`,
`maps_en.vpp`, `meshes.vpp`, `motions.vpp`, `audio.vpp`, `music.vpp`, `ui.vpp`
and `bluebeard.bty`.

Also provide these verified private generated inputs:

- `build/data/geomod-template.bin`, 988 bytes,
  SHA-256 `d489dc141999833ad88fa93dc68b686b0ecb5a864c8cb69ed78ea98c97450dab`
- `build/data/driller-single.bin`, 1,588 bytes,
  SHA-256 `2fac25f24f243f21ec453ff07bfbfb8c18630e1945f74f4e09bf154b76face41`
- `build/data/driller-double.bin`, 3,868 bytes,
  SHA-256 `fd013c70a8bed5f64a456a9e29cacc6e3a3d930701bef0b39c3a26e8abcae4f3`

RF.exe and private reconstruction exports are needed only to regenerate these
templates. A source checkout plus installed game archives alone is insufficient.
Prefer verified existing templates; never substitute guessed geometry.

Optional menu overlay is admitted only through `RF_MENU_OVERLAY_VPP`, with
SHA-256 `2905b36544373c156e2671f2edb936cce25d8071bb4196811ff40e8374e2e05a`.
Verify its local source path. Existing staging containing the overlay requires
that explicit input on rebuild; do not delete unknown staging contents to pass
admission. Stock-only packaging is supported with an appropriate clean staging
location. Inputs must be regular nonsymlink files on a filesystem supporting
hardlinks to the tester staging tree. There is no cross-volume copy fallback.

## Build

After verifying the recorded paths, from the repository root in PowerShell:

```
$env:MSYSTEM = 'CLANG64'
$env:RF_GEOMOD_EXPANDED_PROFILE = '0'
& 'C:\msys64\usr\bin\bash.exe' --noprofile --norc tools/build-xbox.sh --tester
```

Supply `RF_MENU_OVERLAY_VPP` when retaining the overlay. Expected outputs are
`build/xbox/tester-disc/default.xbe`, `build/xbox/main.map` and
`build/xbox/redfaction-tester.iso`. Allow approximately 1.15 GB for the ISO plus
build tools/outputs. The command without `--tester` produces the diagnostic
package. Keep replay files, forced events and altered starts out of tester-disc.

## Hardware admission

Before deploying, verify exact console identity, installed Neighborhood tools,
kernel/dashboard compatibility, transfer/launch commands, destination and free
space. The current tester requires exactly 16,384 physical pages (64 MiB);
a console reporting 128 MiB is rejected. Do not weaken the stock64 guard.

The launch method must expose the entire flat tester package as `D:\`.
Copying only default.xbe is insufficient. Verify debug/log collection and
map/symbol use through the actual installed tools rather than guessed commands.

Checkpoint and settings owners mount `R:` and `S:` to
`\Device\Harddisk0\Partition1\` and use OpenRedFaction files there. Existing
aliases are not borrowed or replaced. Confirm the environment and protect
existing console data before persistence tests. The protected emulator HDD
must not be accessed, copied, regenerated or used as hardware test data.

Codex owns reproducing local builds and collecting authorized deployment/log
results. Testers own controller interaction, functional coverage and campaign
progression. Record source/package identity with each observation. No hardware
test is established until an actual target run is observed.

## Distribution and completion

Original archives, original-derived templates, menu artwork, firmware and
playable packages remain private. The overlay has no included redistribution
license. GitHub receives source and documentation only, retaining provenance.

Report delivered source pin, locally built pin, compile/package outcome,
deployed hashes, actual hardware configuration, observed startup, exercised
scenarios, failures and unrun coverage separately. Keep unresolved implementation
work distinct from tester-owned coverage.

## FMV integration belongs to local Codex

User direction, October 15 at 11:44 a.m. EDT: a working Xbox Bink library is
already available locally; Codex owns FMV integration. It is explicitly outside
this cloud engineer's remaining implementation scope. Verify the library path,
API/toolchain compatibility and applicable distribution terms locally.

Original RF.exe imports binkw32.dll. Startup handler 0x4B2060 conditionally
plays `thqlogo.bik` at 0x4B2091 through movie player 0x520A90 before legal art.
Intro state4 handler 0x44EDE0 plays `intro.bik` at 0x44EDFD then requests state5.
The level-start path 0x45C540 checks L1S1 and conditionally plays the configured
intro at 0x45C862; tables.vpp/game.tbl names `intro.bik`. Movie registration at
0x4B1828 uses `data\\movies` and `*.bik`.

The supplied Installed_Game contains no loose video files or movie entries in
its 8,410 VPP directory records. Referenced `end.bik` and `sumtrailer.bik` are
also absent. Codex must locate the user's original video assets locally; no
asset download or redistribution is authorized by this handoff. The current
port has no FMV playback and opens the frontend directly. In-engine cutscene
cameras/timelines are a separate implemented system.

## Tester feedback record

Record source commit, deployed XBE/package/map hashes, console revision, verified
64-MiB report, kernel, dashboard, Neighborhood version and launch method.
For each issue include starting state, exact reproduction steps, observed and
expected results, frequency, severity, save-data impact (unknown if not checked),
and available logs. State coverage actually exercised and coverage not run.
