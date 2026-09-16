# First same-level player + destruction checkpoint

Read-only source audit; no production changes, builds or runtime checks. Target is a playable DEV-room save of destruction, standing player pose, health/armor, owned supported weapons and ammunition on the identical level. It is not an arbitrary campaign quicksave.

## Current owners and gaps

| State | Exact current owner / entry points | Save implication |
| --- | --- | --- |
| Body position | scene.c `scene_actor_body.state.position`, `next_position`; `rf_scene_actor_pose` publishes render/object pose | Save position, rebuild both physics/public owners together before first movement query |
| Facing/aim | `actor_look.state.body_angles[3]`, `eye_angles[3]`; `rf_look_update_pose` derives body/eye matrices; body world tensor follows `rf_physics_tensor_world` | Preserve separate body/eye angles; avoid placing pitched eye matrix directly into body physics |
| Existing pose handoff | `rf_scene_campaign_pose_get` copies body position plus `actor_look.eye_orientation`; `rf_scene_set_campaign_spawn` accepts level spawn matrix; frame0 look setup decomposes it through spawn-angle helper | Suitable transition convention, not proof of exact same-level body/eye-state roundtrip |
| Health/armor | `campaign_player_damage.state.effects.health/armor` plus player entity view flags | Reuse living-player validation, rebuild derived view rather than serialize handles/flags |
| Loadout | `campaign_player_inventory`: owned[64] bytes, reserve[32] signed32, loaded[64] signed32; `campaign_equipped_slot` maps supported slots through `campaign_slot_weapon` | Serialize inventory using explicit widths and selected catalog weapon ID, never resource pointers or slot alone |
| First-person resources | `stream->player_weapon[]` owns geometry/material/clips and playback; `stream->player_slot`, shot/reload mirrors | Reopen normal resources; resume idle, reset clip/action mirrors; do not serialize rf_player_weapon |
| Current transition handoff | `rf_campaign_player_state` in campaign.h; `rf_scene_campaign_player_get/set`; `rf_campaign_player_copy` in campaign.c | Pointer-free in-memory struct exists; current PC PLAYER_STATE_IN/OUT is raw struct bytes, not portable versioned disk serialization |
| Destruction | RFDS v1, successful RGCH history, independent admissions/RNG/maps | Preserve existing RFDS unchanged as embedded section; world restoration precedes player collision placement |

`rf_campaign_player_copy` checks matching catalog hash, selected ID<64 or UINT32_MAX, finite health>0 and armor>=0, ownership bytes0/1, nonnegative loaded/reserve, selected weapon owned. It does **not** enforce catalog count, supported scene weapon set, per-weapon magazine caps, reserve capacities or sensible health ceilings. Exact table rules must be added to the player record validator; do not treat this function as complete untrusted-file validation.

Three concrete scene integration defects to address first:

1. Frame0 `campaign_combat_tick` clears inventory, calls `campaign_ammo_reset`, then applies `campaign_import_pending`. Its supported selected-weapon predicate lists pistol/rifle/Riot Stick/shotgun/unarmed and omits DEV rocket. A saved equipped rocket currently rejects.
2. Immediately afterward `!campaign_inventory_ready` applies queued startup item grants; DEV mode loops all supported weapons, acquires them and sets reserves to table capacity. This overwrites imported low ammo/partial ownership. Disk restore must be applied after this initialization, or explicitly suppress those grants/refills for a validated disk restore. Ordinary new-game and section-transition behavior must remain distinct.
3. Imported UINT32_MAX selects slot0, then `campaign_ammo_publish` may choose another owned gun. Preserve the declared unarmed semantics explicitly or reject that combination in first scope; do not promise unarmed restoration while autoequip changes it.

At the end of rendered frames, scene builds `campaign_player_export` from live inventory/vitals and selected weapon. Pose getter reads the live body independently. Capture the new player record at one shared stable frame boundary alongside RFDS, rather than reading a stale export after a later movement update.

## Minimal explicit format

Use a new wrapper **RFCP version1**, leaving RFDS version1 intact and readable independently. This is a proposal, not an implemented parser. All words little-endian, floats finite IEEE754 binary32; reserved bytes zero.

Wrapper32 bytes: magic RFCP at0; version1 at4; exact total length at8; header length32 at12; RFDS length at16; player length544 at20; scope flags1 (same-level standing DEV) at24; reserved0 at28. Body is RFDS then exactly one RFPL record.

RFPL544 bytes:

| Offset | Bytes | Field |
| --- | --- | --- |
| 0 | 4 | RFPL |
| 4 | 4 | version1 |
| 8 | 4 | exact length544 |
| 12 | 4 | flags0 |
| 16 | 4 | weapon catalog hash |
| 20 | 4 | selected catalog weapon ID or UINT32_MAX |
| 24 | 4 | health |
| 28 | 4 | armor |
| 32 | 12 | body position |
| 44 | 12 | body angles |
| 56 | 12 | eye angles |
| 68 | 12 | reserved0 |
| 80 | 64 | owned bytes |
| 144 | 128 | reserve signed32[32] |
| 272 | 256 | loaded signed32[64] |
| 528 | 16 | reserved0 |

Keep two-slot RFSG envelope unchanged. The wrapper adds576 bytes. Smallest memory-safe first implementation keeps the existing110524-byte transport/buffer cap and rejects RFDS sections above109948 bytes when adding player state; document that bound instead of silently truncating. Current9554-byte baseline and approximately53KiB eight-cut case fit comfortably. If all current maximal RFDS states must fit, explicitly increase common cap by576 to111100 and update every native/PC harness budget check with measured64MiB evidence. That is a bounded alternative, not an implicit allocation increase.

The RFDS identity already binds level/source/filter/material/templates/config; player record additionally validates current weapon catalog identity. Store current supported catalog indices only under that matching catalog, not across altered tables.

## First scope and validation

Restrict first capture to a living, standing, settled player in enemy-free Glass House: no pending level transition, mover/vehicle attachment, ladder/swim mode, crouch transition, active rocket, firing burst, reload, burn/death or scripted camera/control handoff. These gates are necessary because the proposed record intentionally omits their state. Report a concrete not-ready result rather than snapshotting inconsistent partial actions. Resume at rest with neutral commands, zero velocity/forces/angular velocity and idle weapon; this is checkpoint behavior, not exact midair/timer continuation.

Validate player record entirely before scene publication: exact lengths/zero reservations; catalog identity and supported IDs; ownership0/1; ammo nonnegative and within applicable table limits; reject loaded ammo in unsupported weapon IDs; living finite health and finite nonnegative armor with an explicitly selected table-supported cap policy; finite body/eye angles in the domains accepted by rf_look_orientation/rf_look_update_pose. Build a local rf_look_pose with neutral inputs and zero time, verify finite orthonormal matrices and world tensor before commit. Do not clamp corrupt ammo/angles silently.

Saved standing body must occupy valid restored space. Extend the existing history_check candidate visitor with a read-only pose fit against candidate terrain plus unchanged world/mover blockers, using the same body sphere set and query filters as gameplay. A point-in-room test alone does not prove the body fits. Never bind the pending terrain into the live collision overlay to perform validation. If a complete combined-world fit check cannot be expressed without publication, keep this as a fresh-scene load and abort scene creation after restored-world fit failure; that fallback must not be passed off as a fully pure semantic selector. This is the main remaining design gate before RFCP is safe for two-slot fallback selection.

## Restore ordering / smallest implementation split

1. Parse/select RFCP with immutable RFDS and local player record; validate both with one combined candidate visitor. RFDS-only old files stay destruction-only and do not imply player restoration.
2. Run normal scene/resource/class construction. Decode/publish RFDS terrain and bindings before any player collision or physics step.
3. Introduce one pending disk-player record separate from `campaign_import_pending`, which currently represents a section handoff. Apply pose after fresh body creation but before frame0 movement/eye processing. Ensure frame0 look initialization uses saved body/eye angles rather than overwriting them from campaign spawn.
4. Initialize ordinary weapon classes/resources/startup state, then apply disk loadout/vitals exactly once after queued grants and DEV supply. Resolve selected ID through supported slots including rocket; preserve explicit unarmed state. Publish ammo/HUD and player entity mirror, reset fire/reload/weapon clip to idle.
5. Set both body position/next_position and body orientation/next_orientation, rebuild tensor/bounds, publish group/object pose, refresh room/collision ownership, clear ground/contact/eye interpolation caches for normal recomputation. Do not serialize/reuse process handles, cached pointers or timer absolute values.
6. Capture RFDS and player record from the same settled post-tick point; one bounded buffer then existing two-slot write/readback/native flush. Update ready diagnostics only after both sections encode and store succeeds.

The minimal code changes are a small portable RFPL codec/validator, pending same-level restore adapter in scene checkpoint/player initialization, and combined RFCP selector. No generalized entity graph or campaign serializer is needed for this milestone.

## Existing transition persistence is separate

PC play.c and Xbox main.c use identical high-level flow: obtain player state and departing pose, open destination, apply `rf_level_transition_place`, reconstruct scene resources, set campaign spawn and import inventory/vitals. No on-disk slot is involved. `rf_campaign_local_goals`, pickup/defeated actor owners, switch/trigger checkpoints and revisit state already preserve some progression in the current process. None is embedded in RFDS/RFPL. Saving a campaign level with only the proposed player record would reset enemies/events/pickups on reload; therefore first scope must stay DEV until those owners are deliberately added.

## Original-code evidence boundary

The weapon inventory layout is backed by existing shared API evidence: entity+42c owned64, +2ac reserves32 and +32c loaded64; consumption4257c0, acquisition4030d0 and startup422cf5..422dbd have separately reconstructed helpers. `FUTURE-CAMPAIGN-TELEPORT-PLAYER-20260915.md` executes original4b9820/48a230 and shows multiple pose owners and deferred room refresh; it also shows teleport does not universally zero velocity. Therefore resetting motion here is an explicit checkpoint policy, not an assertion of original teleport/save behavior. Destruction save addresses/failed admissions are covered by `GEOMOD-SP-SAVE-ADMISSION-CONTRACT-20260916.md`. This audit does not recover or claim the original full player disk-save layout.

## Required focused proof

- Roundtrip RFPL across PC/Xbox, exact ownership/ammo/vitals and separate body/eye orientation; malformed lengths/NaN/reserved/ammo/catalog rejection leaves output unchanged.
- Save partially depleted pistol/rifle/shotgun/rocket, reload and confirm no startup/DEV refill and correct equipped resources/HUD; also test explicitly unarmed behavior.
- Save looking up/down and turned sideways; first neutral frame retains aim, body up remains correct, then movement and firing use expected directions.
- Reload beside/inside an opened crater; body placement uses restored terrain and remains collision-consistent. Reject blocked placement without corrupting older selected slot.
- Reject midreload/rocket-active/swim/ladder/dead/pending-transition capture under first-scope gates, without writing either slot.
- One native process writes RFCP, fresh process loads it, walks and fires; compare inventory delta and pose against uninterrupted checkpoint-policy control. Keep full campaign-save claims excluded.

## Implemented portable RFPL codec

include/rf/player_checkpoint.h and src/core/player_checkpoint.c implement a544-byte little-endian standalone player record with catalog, inventory, vitals and canonical standing body/eye pose validation. tests/player_checkpoint_tests.c covers roundtrip, every truncation, reserved bytes, nonfinite values, ammo/catalog limits and unchanged outputs on error. CTest player_checkpoint_codec passes; nxdk-cc C11/O2/Wall/Wextra/Werror standalone compilation passes. This is reconstruction format, not the original save ABI. Scene capture/restore, candidate body fit and native roundtrip are still required; no full-player persistence claim.
