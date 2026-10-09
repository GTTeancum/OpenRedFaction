# Authored Jeep gunner view

Source-written on 2026-10-09 for the parent-owned 01:00 UTC stock-64-MiB
Xbox batch. No helper build, test, emulator session, screenshot, or original
game execution was performed. Runtime admission, drawing and memory headroom
remain unverified until that batch.

## Missing behavior and original evidence

The previous gunner path drew only the world-attached `jeep_gun.v3m` and
suppressed both the driver's cockpit and the ordinary handheld view. It had
no consumer for the nineteenth authored first-person weapon definition.

Read-only inspection of the supplied `RF.exe` confirms that a Jeep gunner
does enter the ordinary first-person view path:

- `42acd0..42ad19` resolves the actor's linked host, requires the Jeep class
  flag through `40a2f0` (`class+724 & 0x400000`), and compares the second
  occupant's handle with that actor's handle.
- `4ab322..4ab32d` calls that predicate and jumps to `4ab345` on success,
  bypassing the ordinary attached-weapon suppression.
- `4ab395` obtains the selected weapon through `4a5910`; for a vehicle
  parent, `4a5933..4a5967` resolves the parent's primary weapon at `+2a4`.
- The admitted path reaches normal first-person FOV setup at
  `4ab3f0..4ab411`. Normal camera-local offset placement remains
  `4aa762..4aa7a6`, independently of animation/skinning.

The installed `weapons.tbl` Jeep Gun entry specifies:

- `fp_jgun.vcm`, resolved by the existing reader to `fp_jgun.v3c`
- Idle `fp_jgun_idle.mvf` and fire action `fp_jgun_fire.mvf`, resolved to RFA
- Normal offset `(0.192, 0.228, 0.885)` and FOV `65` degrees
- A separate split-screen offset, which is deliberately not used

Archive directory inspection finds the actual mesh at 66,741 bytes and the
idle/fire clips at 864/1,832 bytes. Its authored textures are `envirohand.tga`,
`hand1st.tga`, `JeepGun1.tga` (256x256 each), and `hgun1A.tga` (128x128).
The four decoded RGBA images total 851,968 bytes. These are input metadata,
not a measured successful resource load or draw.

## Integrated ownership and rendering

`scene_jeep_gunner_view_resources.inc` owns one independent mounted view per
scene. It uses the existing table reader, first-person mesh/motion owner,
prepared skinning matrices, and material reader. It does not create a
handheld slot, grant ownership or alter equipped weapon state. Archives are
used only during admission; no frame-time allocation or file I/O is added.

Admission occurs after existing vehicle-profile preload. The view is required
for the active Jeep and attempted for an already-admitted optional Jeep
profile. Loader peak is bounded at 1.5 MiB. Image merge preserves the existing
texture-slot limit, with all fallible work before ownership transfer. The
renderer retains its independent combined-image-byte bound. Optional preload
also retains the existing 16-MiB native
free-memory reserve. If optional view admission fails, its status is recorded
and the previous world-gun presentation remains available. Required active
Jeep failures propagate normally instead of silently claiming success.

The owner survives role changes and cross-class handoffs and closes once with
the scene, independently of the profile-pack aliases. Scene materials own
transferred images; the mounted owner retains geometry, material records and
motion payloads. No new save data is needed: the existing restored Jeep role,
host and live scheduler select presentation on the next frame.

`scene_jeep_gunner_view.inc` selects a living, visible, possessed Jeep gunner
outside cutscenes. It reuses `scene_player_weapon_view_prepare`, so the
authored offset is applied once through inverse camera translation, and direct
and clipped faces use the same FOV/aspect diagonal. It uses the existing
first-person near/far/depth-band and ambient-lighting policy. The view appends
after scoped world geometry and uses the existing bounded skin/clip scratch.

The previous world gun is omitted only while this admitted first-person view
is active; it remains for driver, on-foot, cutscene and optional-resource
fallback presentation. The world chassis remains visible in the gunner seat.
Collision, attachment poses, projectile origin, aim and damage are unchanged.
This duplicate-gun suppression is an integration policy; the original world's
full per-view attachment visibility machinery is not newly reconstructed.

Idle starts on entry or host change. Each new accepted scheduler shot requests
the real fire action, whose shared completion path returns to idle. Pose
advances at most once per presented frame, matching the existing 60-Hz
first-person timing policy. Warmup, empty reserve, and rejected full-pool
dispatch do not invent fire actions. This is a bounded first-playable action
adapter, not a claim to recover every original blend or recoil transition.

## Read-only observations for the scheduled batch

`rf_scene_jeep_gunner_view_load[8]`:

| Word | Meaning |
| --- | --- |
| 0 | View requested |
| 1 | Required active Jeep rather than optional profile |
| 2 | Admission status |
| 3 | Resident bytes before image transfer |
| 4 | Loader peak bytes |
| 5 | Texture count |
| 6 | Material bytes transferred to the scene |
| 7 | Optional-admission native free pages |

`rf_scene_jeep_gunner_view[24]` resets on every presentation, including driver
and on-foot frames. Float entries contain raw binary32 words.

| Word | Meaning |
| --- | --- |
| 0 | Frame plus one |
| 1 | Active gunner view |
| 2 | Host handle, or `UINT32_MAX` when inactive |
| 3 | Current authored clip slot |
| 4..5 | Emitted vertices and completed material batches |
| 6..7 | Admitted resident/peak bytes, before image transfer |
| 8..10 | Applied authored offset |
| 11 | Applied FOV |
| 12..14 | Inverse camera offset |
| 15..17 | Applied normalized clip-space diagonal |
| 18 | Prepared pose hash |
| 19 | Draw status |
| 20 | Existing `rf_scene_apc_primary[1]` accepted launch count |
| 21 | Existing current vehicle primary reserve |
| 22 | Mounted pose-step count |
| 23 | Last stepped frame plus one |

The planned original L10S1 Jeep check should require admission status zero,
nonzero gunner vertices/batches, exact offset/inverse-offset/FOV, and unchanged
ordinary launch/ammo evidence. Driver and post-exit probes must show current
frames with zero active-view/vertex counts. No visual-parity claim follows
from these numeric checks. Muzzle-flash VFX, turn lag, and broader animation
blend fidelity remain outside this slice.

## 01:00 batch admission correction

The first original L10S1 attempt on `106f0a1` reached this owner but failed
before frame zero: loader resident 928,260 bytes, peak 940,196 bytes, four
textures, then `RF_RANGE` before material transfer. Thus mesh/motion loading
succeeded; the failure is specifically merge admission, not a gunner draw.

Source review found that this new merge had incorrectly reapplied
`RF_CAMPAIGN_MATERIAL_BUDGET` (12 MiB) to the already-combined scene owner.
`resource_budget.h` defines that cap for initial world/actor admission;
existing NPC, clutter, handheld and vehicle banks then merge their separately
bounded images. `renderer.c` independently enforces the 20-MiB combined-image
cap (21 MiB for existing shield/fusion profiles), including lightmaps.

The correction follows the existing bounded-owner merge contract: keep the
1.5-MiB view loader bound, texture-slot bound, ownership checks and byte-counter
overflow guard; do not mistake the initial-world cap for the combined cap.
The renderer's final combined-image bound is unchanged. No image quality,
resolution, geometry, slot capacity or memory limit was increased.

`rf_scene_jeep_gunner_view_merge[8]` records pre-merge scene slots/bytes, view
slots/bytes, slot cap, initial-world byte cap, projected combined bytes, and
merge status. The parent retry can establish the exact rejecting old bound
and verify the admitted path. No helper build or runtime test was performed.
