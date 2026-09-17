# Real rocket detachment and checkpoint coverage

## Corrected diagnosis

The existing two-shot recording hits the post near Y=-0.9, then Y=1.0. A temporary
component/support dump shows one connected remainder after each blast: six faces
after the first, seven after the second. Extraction intentionally retains the
largest connected component. Zero emitted pieces in that recording therefore
does not establish a topology failure. The earlier next-step diagnosis in
GEOMOD-EXACT-STAR-CORNERS-20260917.md was too strong.

An ordinary RFI6 rocket input aimed at Y=0.25 separates the post into two groups.
The bottom remainder has11 faces and the upper remainder6; the current face-count
policy retains the bottom and extracts the upper piece. This is not a support or
structural-anchor solver. The distant frame initially appeared to show the base
as the extracted piece; the component geometry corrects that interpretation.

No injected terrain cutter or original-game capture is used. The replay retains
the existing measured retreat, weapon selection and fire frame240, changes the
ordinary pitch inputs, and removes the second shot. Aim uses the existing
measured eye position and original pitch mapping from replay_authored_post.py.

## Reproduction and PC evidence

`python tools/check_authored_restart.py --case middle-shot --output-dir artifacts/geomod-postedit-re/middle-continuation`

The harness runs350 frames before saving, continues200 frames and compares an
uninterrupted550-frame control. All three contain exactly one registered batch,
one body,12 projected vertices,132992 PC resident bytes and zero draw errors.
Continued/control RFCP, RGCH and publication files match exactly. The harness
requires a nonempty detached draw and matching ownership across restart.

The output frame was inspected: room, post remnants and rocket/HUD render. The
upper fragment is small in this view, aligned with other posts and remains at
its birth pose; this is not a good demonstration of falling destruction.
Motion, collision response/registration, active-pose persistence and proper
fragment lighting remain unimplemented. This case proves birth-pose ownership
and reconstruction only.

## Stock Xbox evidence

`artifacts/xemu/render-20260917-052820/report.json` passes69 comparisons across
550 frames with67108864 bytes of base RAM and no extra RAM. The native framebuffer
was inspected and shows the same room/post-remnant composition and weapon/HUD.
DETACHED_PIECES matches PC: one batch, one piece,12 projected vertices,132992
resident bytes, no draw error. The2416-byte checkpoint matches PC exactly.
Available memory at endpoint is4008 pages (15.65625MiB). Disc staging is restored
and the harness-owned emulator exits; the unrelated emulator is untouched.

The native harness now compares DETACHED_PIECES ownership/draw/status fields
between PC and Xbox and bounds resident storage to2MiB. Resident byte counts are
not required to match because target layouts may differ. The authored destruction
budget check now matches the existing13MiB production reservation.

Native continuation also passes69 checks in artifacts/xemu/render-20260917-053112:
load the saved350-frame PC checkpoint and execute the remaining200 ordinary
input frames. One piece and12 projected vertices persist. The exported native
checkpoint is byte-identical to the uninterrupted native550-frame run. Its
framebuffer was inspected and reveals a visual discrepancy: the post appears
more intact after reload despite matching counters and exported checkpoint.
The cause is unresolved. These state checks do not prove correct restored
rendering, and visual reload acceptance remains open.
Disc staging is restored and the owned emulator exits.

Remaining: resolve the restored post visual discrepancy, then body motion/contact response, scene query registration, active-state
saves, fragment lighting, broader cuts and subdivision.

Follow-up: the specific visual reload discrepancy is fixed and verified in
GEOMOD-RELOAD-RENDER-STATE-20260917.md; earlier captures above retain the failure
as regression evidence.
