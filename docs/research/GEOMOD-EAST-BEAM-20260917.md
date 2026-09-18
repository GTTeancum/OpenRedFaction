# Opposite developer-room beam destruction

The ctf06 east beam UID98 is now selectable alone or with posts97/96 through
RF_REPLAY_AUTHORED_SOURCE=98 and RF_REPLAY_AUTHORED_SOURCES=3. Xbox uses the
same shared implementation and its existing authored-source.bin selector.
This expands the explicit developer profile; arbitrary campaign geometry and
simultaneous destruction of both beam assemblies are not yet enabled.

## Authored geometry and ownership

Installed editor records identify beam98 at(6,2.25,0), posts96/97, roof82 and
earlier subtractive roof-air86. The beam has six faces572..577; post97's top
is567 and roof82's underside is498. These are parsed asset identities, not
inferred original runtime behavior. Decoder eligibility retains the complete
local overlap, earlier-air66, flags, convexity, material and ownership checks.
Roof-air86 clips only roof82. Tests check the remaining underside area8,
post filter inheritance, center/end cuts and exposed cap ownership/material
binding. The two beams use the same reconstruction policy without changing
existing source identities; manifest tests verify repeatability and distinct
identities for all six supported owners.

Connected publication and source/material digest selection now include98.
The east profile starts west of the beam facing east, allowing retreat into
the open room. The initial east-side placement hit a wall during retreat and
invalidated the old aiming distance; a traced second rocket hit the distant
world wall instead of the beam. The corrected replay mirrors the measured
west-profile eye and target coordinates around X=.5. Small actual movement
rounding differs, so this is a practical aim fixture, not exact mirrored
simulation. No host input or original-game capture is used.

## Verification

All123 PC tests pass. Stock NXDK compilation/link, XBE and ISO creation pass.

`python -B tools/check_beam_continuation.py --source 98 --both-posts --next-shot`
passes: the first rocket edits98/97 and produces three retained fragments;
the second edits98/96. The8016-byte final player/destruction checkpoint after
reload is byte-identical to uninterrupted playback. Four then eight generated
chart entries resolve to authored wood material0. The opposite compiled roof
has different partitions, so this replay checks nonempty authored material
ownership without imposing the west beam's chart or fragment counts.

The PC endpoint framebuffer was inspected: damaged supports, detached wooden
chunks, room and launcher/HUD are visible. This does not qualify every frame,
audio, all fragment motion, or general destruction fidelity.

Artifacts: beam98-{all-build,tests,xbox,live}.log and
artifacts/east-triple-beam-next-shot/{report.json,save.png,resume.png}.

Native command:

```text
python -B tools/xemu_render_check.py --dev-room --spawn --player-checkpoint --geomod-checkpoint-out --level ctf06.rfl --archive levelsm.vpp --authored-source 98 --authored-sources 3 --input artifacts/east-triple-beam-next-shot/control.bin --seconds 240
```

Run artifacts/xemu/render-20260917-231139 passes77 checks over900 frames on
stock64MiB. The8016-byte Xbox-created checkpoint matches PC exactly (SHA256
34a13e256a029d687106aeacfb4b727bffacce52d46c9407dc365bfe5cb9af43).
Endpoint available memory3205 pages is12.52MiB. The native framebuffer was
inspected: damaged supports, detached wood, room and launcher/HUD are visible.
The owned emulator is closed and disc_restored is true. Audio was disabled.
The existing west-beam two-shot PC continuation control also still passes.

Remaining: broader blast histories and arbitrary authored-source eligibility. This is one
additional connected assembly, not generalized campaign GeoMod.


## Reload and moving-debris continuation

The native two-shot save from231139 was loaded in a second stock64MiB run,
artifacts/xemu/render-20260917-231514, with601 neutral frames. All77 checks
pass. Its8016-byte endpoint is identical to both the PC native-reference run
and uninterrupted PC settle-control. Memory available at endpoint is3364
pages (13.14MiB). The native framebuffer was inspected and retains the damaged
assembly and wood fragments. The owned emulator closed and restored its disc.

PC `--source 98 --both-posts --next-shot --settle` keeps all three pieces
stable for another600 updates, with exact saved versus uninterrupted bytes.
The existing `--airborne` timing on the second east-beam shot is insufficient:
all pieces were already asleep. Its strict moving-state assertion correctly
fails; this is not counted as an airborne qualification.

The replay now accepts `--save-frames` and `--moving-first-save`, requiring
at least one non-sleeping piece and no detached-motion error in the first save.
At300 frames the player checkpoint gate rejects active launcher cooldown16;
the gate remains unchanged. At320 frames cooldown has completed and all three
pieces remain awake (DETACHED_MOTION starts3,4,1,0 with error0). Command:

```text
python -B tools/check_beam_continuation.py --source 98 --both-posts --save-frames 320 --moving-first-save
```

It passes first-save/reload/control and further600-update settled continuation
with exact checkpoint bytes. All three pieces finish asleep above the floor.
The report now retains motion words for every stage, including the first save.
This tests physical continuation; no sequential visual-animation or audio
acceptance is implied.


Stock64MiB run artifacts/xemu/render-20260917-231729 restores the320-frame
PC-created moving save and runs601 neutral frames. All77 checks pass; the
5200-byte native output matches both its PC reference and uninterrupted
settle-control (SHA256 af12e69b3c33116a4150deeb3fba5aa9a51cd5f96494ee57b4924347b32d2d36).
All three fragments are asleep at endpoint;3353 free pages give13.10MiB.
The native endpoint framebuffer was inspected: retained wood chunks, damaged
support, room and launcher/HUD visible. Disc restored and owned XEMU closed.
This proves Xbox restoration of the supplied moving state; creation of that
particular moving save was on PC. The separate settled-state run used an
Xbox-created save. Neither result implies full campaign save support.

Native reloads used `--geomod-checkpoint-in` with the respective source save,
`--player-checkpoint --geomod-checkpoint-out --authored-source 98
--authored-sources 3 --dev-room --spawn --level ctf06.rfl --archive levelsm.vpp`,
and artifacts/east-beam-native-resume.bin (601 neutral RFI6 frames).
Logs: artifacts/beam98-{settle,moving-first,native-reload,native-moving-reload}.log.
