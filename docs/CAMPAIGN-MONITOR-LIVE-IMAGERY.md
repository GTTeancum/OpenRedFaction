# Live security-monitor imagery

Implemented on `codex/live-security-monitors`, based on
`1edd6b688ddd0a3e40ec35d7d5c77fcc64a4fa4a` (2026-10-12).
This is the source implementation handoff; no build, feature test, emulator
session, capture, or screenshot was run by this task. The parent owns the
serial Xbox build and short startup/running smoke; testers own functional
coverage. The RFEN8 follow-on remains uncompiled pending the parent build;
the remaining implementation limits are recorded below.

## Original evidence checked

Read-only disassembly of `Installed_Game/RF.exe`, SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`,
confirms:

- `0x5943d8` contains `screen.tga`; `0x412572..0x4125b7` compares material
  names at `+0x14`, writes the primary bitmap at `+0x10`, advances by 200
  bytes, and writes brightness 0.7 through the optional `+0xbc` pointer.
- `0x412600` writes mode at monitor `+8`: `0x41230f..0x412357` dispatches
  camera 0, black 1, noise 2. An authored header byte equal to 1 ORs the
  continuous bit 8; a later false byte does not clear it. Nonpositive
  intervals become 8 seconds.
- `0x4126a0` adds linked cameras and retains the first for an unbound screen.
  `0x412700` runs the interval timer and calls `0x412780` to cycle the camera
  list. The former document's refresh-only description was incomplete.
- `0x412306` clears validity bit 4 for continuous views. `0x412820` reads
  entity eye position `+0x7d4` and orientation `+0x7e0`, uses FOV 50, renders,
  and restores the main view through `0x4d2e10`.
- `0x4123ad..0x4123d5` generates binary noise using multiplication by 65539
  and bit 18. The port uses a separate rendering RNG so simulation RNG is
  unchanged.
- **Correction to the preliminary handoff:** `0x4b9af0` iterates the event's
  linked monitor props and calls `0x412990` for each. It does not invalidate
  every monitor in the level. Type 60 now follows those authored links.

## Implementation and ownership

`scene_monitor_state.inc` owns the existing 64 binding rows plus bounded
additive camera lists (256 UIDs per screen), a cycling clock and invalidation
generations. `Monitor_State` ON configures screens; OFF remains a no-op.
Continuous mode is sticky, matching the original OR operation. Modes and
camera changes invalidate the appropriate cached feed. Missing/removed
camera owners produce noise. Frame-based timers advance with the existing
simulation clock, with no extra simulation ticks.

`scene_monitor_render.inc` reserves up to eight 128x128 RGBA native images,
an opaque black fallback, and shared 64 KiB color plus 64 KiB reciprocal-depth
scratch. The image/material table is finalized before the first presentation.
Maximum additional storage is about 708 KiB including the camera lists and
descriptors, excluding allocator overhead and stack; native image pixels
alone are 512 KiB. The regular combined renderer image budget and 512-slot
limit still apply; admission reserves the full 4 MiB lightmap allowance plus
the dynamic terrain atlas and reduces the cache count if necessary. It does
not incorrectly charge separately loaded actor images against the initial
12 MiB world-material allowance.
Allocation failure releases all temporary resources, records diagnostics, and
leaves ordinary authored materials usable rather than aborting gameplay.

The secondary view borrows retained geometry and live owner poses. New
allocation-free triangle visitors in `preview.c` use its existing projection,
clipping, skinning output and UV conversion without constructing another
world mesh. The software rasterizer uses depth, perspective UVs, alpha
cutouts, retained lightmaps and generated vertex lighting. World, movers,
published GeoMod surfaces, clutter, retained skeletal actors, loaded vehicle
chassis and placed pickups contribute. No animation, object-render dispatch,
physics, event, primary visibility or gameplay RNG callback is invoked.

Screen selection uses primary-room visibility and a conservative screen-centre
frustum check. Refresh scheduling is round robin, at most one image generation
per presentation. Cached snapshots remain stable until invalidated; continuous
and noise screens refresh in turn. Eviction prefers offscreen screens, then
the oldest image. A screen without a resident entry draws black; it can never
borrow another screen's feed. Residency is fixed before constructing that
presentation's draw list.

Only a matching `screen.tga` material in the **current placed prop** receives
the cache slot. Both retained native model submissions and the CPU fallback
use that resolved slot. Shared model material records and global textures are
unchanged. Native publication runs in `renderer.c` after its GPU idle fence
and live-lightmap update, before drawing; the existing `sfence` publishes
swizzled pixel writes. GPU texture descriptors borrow those same allocations.
No second GPU copy or dynamic texture-table resizing is needed. Feed pixels
receive the recovered 0.7 brightness factor at publication.

Scratch is released with the scene. Image ownership transfers to the ordinary
material owner and follows its existing renderer-drain/close lifetime.
Section reset and successful environment restore invalidate the cache.
Partial render failures publish black, record the failure, and retry later
without failing the main gameplay frame. The optional PC staging path uses
the same source with its existing image layout; it was not compiled or run.

## Save continuity (RFEN8)

The follow-on implementation preserves the applied additive camera list in
its exact order, the selected camera UID, remaining cycle ticks, whether the
clock has started, and the sticky continuous flag. A later false event byte
can be retained as authored while the applied continuous flag stays set.
The cycle helper runs even when image allocation was declined. A save made
between presentations normalizes a due cycle in the encoded copy without
mutating the live list or running gameplay.

RFEN8 retains the existing header, force/nav/cutscene/form records, 20-byte
monitor bindings and eight-byte replay record. It then writes a 16-byte
support-point record (presence word and three velocity floats), followed by
an eight-byte monitor extension header (length and count). Each monitor has
six 32-bit words: screen UID, camera count, selected camera UID, remaining
ticks, clock-started flag and continuous flag, followed by its camera UIDs.
The camera interval is still the port's 60 Hz simulation interval, clamped
to 1..2147483647 ticks. Absolute frame counters, generations, GPU resources
and pixel contents are not saved. Restore holds the elapsed cycle phase and
rebases it on the first presentation, supporting both fresh and quick loads.

The extension is bounded to 64 monitors with 256 cameras each, at most
67,080 bytes. It uses the existing whole-save capacity refusal; the
110,524-byte file cap and storage path are unchanged. Actual records contain
only applied camera UIDs. The environment's single staging allocation holds
the current and incoming camera lists, charged to the caller's restore
budget (at most 134,656 additional bytes for both full tables). All failure
paths free that allocation without publishing monitor state.

Decoding checks section identity/checksum, exact lengths/counts, unique
screen/camera UIDs, loaded camera classes, the first-camera anchor, selection
membership, finite positive intervals, boolean flags and cycle bounds.
Absent support-point velocity must be canonical zero. Commit admission
also compares the live camera table and reset epoch with the preparation
snapshot, so a changed event, cycle or invalidation rejects stale staging.
Successful assignment copies the staged list directly and invalidates
transient feed images; it does not scan authored events or restart cycles.

RFEN1-7 remain readable. No-monitor saves still use RFEN6/7. Legacy saves
cannot recover applied-list history or a selected camera/cycle remainder:
their existing first-camera binding seeds an authored-link reconstruction
and a fresh cycle; an unbound legacy screen remains unbound. The saved
header byte supplies the available continuous-mode evidence. Loading an old
save cannot recover history that the old format never recorded. No attack
section or hourly-runner file changed in this follow-on.

## Known first-pass approximations

- Fixed square resolution, eight-entry residency, nearest-neighbor sampling,
  alpha cutouts and conservative visibility are port policies, not recovered
  original allocation or filtering behavior. Full secondary-scene traversal
  and reads from native texture memory need performance feedback on Xbox.
- Nested monitor faces are black to bound recursion. The pass omits particles,
  transparent blending, fog, glows, loose debris, weapon attachments, turret
  auxiliaries and the first-person player body. Native retained monitor draws
  use the normal rigid texture path; CPU fallback retains its existing ambient
  modulation in addition to the feed brightness factor.
- Images are regenerated after load. RFEN8 retains monitor state as described
  above; legacy saves use the documented reconstruction fallback.
- The original `0x4126df` camera-actor `+0x520 = 15` side effect is recorded
  but not imposed on the port's AI state machine without a verified mapping.
  Live imagery uses the already-published camera actor eye/look pose.
- First presentation, camera cycling, forced invalidation, removal, cache
  pressure and save/reload need tester coverage; source review is not runtime
  validation. No retail visual-parity claim is made.

## Integration handoff

The public addition is `rf_scene_update_monitor_images(lightmaps)` plus
`rf_scene_monitor_images[12]`; the existing native scene renderer calls it
under its current fence. The event service adds `monitor_force` next to
`monitor_state`. Save-owner changes stay in
`scene_world_environment_checkpoint.inc`; its public staged-restore interface
is unchanged. The follow-on extends RFEN to version 8 without changing the
world envelope or the attack-state sections.

Diagnostics are: presentations, resident images, owned scratch/pixel bytes,
refreshes, last camera-pass triangles, evictions, failures, last error,
last screen UID, last camera UID, effective mode, snapshot cache hits.
The separately bounded camera-list table adds 65.75 KiB to the reported
scratch/pixel total. No telemetry hashes read rendered images.

For the parent's hourly integration: build the feature commit on the usual
stock-64-MiB target and perform only the authorized short startup/running
smoke. This task created no artifacts/build payloads or binary assets and
did not access saved HDD images. It did not run cleanup, which remains the
parent's exclusive responsibility.
