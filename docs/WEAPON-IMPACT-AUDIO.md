# Conventional weapon impact audio

Source-written 2026-10-09. Parent scene integration is present; Xbox compilation,
ordinary impact playback and admission under the live stock64MiB budget remain
unverified until the scheduled parent batch. No helper build, test, emulator,
fixture, image or original-game execution was performed.

## First-playable gap

The accepted ordinary player hitscan path consumed ammunition and applied
damage, but only Riot Stick had a material-impact sound callback. Ordinary
pistol/rifle/shotgun shots into a wall had no contact-sound consumer at all;
successful NPC, clutter and vehicle hits also omitted their weapon impact
sound. Launch playback and NPC pain/death audio are separate operations.

The new consumer handles actual accepted ordinary player firearm contacts:

- Static-world and moving-solid contact, including authored/runtime material
  lookup through the existing `campaign_body_surface` adapter
- Qualified NPC body contact after shield/nano interception, using its retained
  authored `collision_material`
- The already selected detached piece, clutter or vehicle contact, with an
  explicit Default fallback when this hook does not retain material identity

Parent integration excludes Riot Stick to preserve its existing consumer.
Sniper/rail precision, NPC-fired shots, turrets and mounted weapons have separate
dispatch paths and are not integrated in this slice. Their conventional group
metadata is available without claiming those consumers are complete.

## Owned RF.exe evidence

Read-only disassembly of `Installed_Game/RF.exe`, known owned-image SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- `4c2b80..4c2b95` initializes each ten-entry impact table to-1.
- `4c3e26..4c3f1d` parses repeated `$Impact Sound:` material/Foley pairs.
  `4686c0` maps material names in this order: Default, Rock, Metal, Flesh,
  Water, Lava, Solid, Sand, Ice, Glass. Unknown names return Default0.
- `4c3e4a..4c3ece` detects the authored ` underwater` material suffix, removes
  its11 characters, resolves the remaining material name and stores its Foley
  group in weapon descriptor `1a0 + material*4`.
- `4c3ed7..4c3f08` stores ordinary material groups in `178 + material*4`.
  Repeated slots overwrite earlier values, including unresolved/empty-1.
- `4c4f9b` and `4c511b` call contact presentation `4c5820` from the ordinary
  world-contact routine `4c4ec0`.
- `4c5912..4c5949` reads the actual contact material from projectile `1d0`,
  chooses the applicable material group and resolves a sample with `434da0`.
- If the sample is exactly-1, `4c594c..4c597f` resolves the Default entry in
  the same table. This is a sample-result fallback, not merely a group-exists
  check; even material0 can retry its own default group.
- `4c5982..4c5996` requests one positional `5056a0` sound at projectile
  contact point `1b4`, volume1, after selection/fallback.
- `434da0..434de8` returns-1 for invalid groups, selects the first sample for
  a single-entry group and otherwise uses random modulo group count.

The second table is **underwater**, not alternate fire. `4c98b0..4c98dc`
requires projectile `2f8` bit2 and descriptor `52c` equal1 or2. The existing
liquid dispatcher `4c4b50` writes that projectile flag. The current hitscan
path does not retain an equivalent full projectile-liquid owner, so the new
contact API consumes the dry table only. Alternate trigger state must never
be substituted for the underwater predicate.

Installed `tables.vpp/weapons.tbl` binds conventional firearms to `Gun Hit
Default`, `Gun Hit Rock`, `Gun Hit Metal`, `Gun Hit Flesh`, `Gun Hit Water`
and, where authored, `Gun Hit Glass`. Shotgun/HMG/rail Water uses `Medium Water
Splash`, whose two waveforms already belong to `Gun Hit Water`. Pistol and
undercover pistol additionally declare `default underwater`. These are distinct
from `Glock Ricochet` and from explosion-vclip Foley.

## New files and ownership

- `include/rf/weapon_impact_audio.h`: fixed `groups[64][2][10]` and read/load API
- `src/core/weapon_impact_audio_reader.inc`: included once by
  `entity_assets.c`, reusing its existing bounded lexer and named catalog order
- `src/diagnostic/scene_weapon_impact_audio.inc`: optional level-lifetime audio
  owner and actual-contact/ordered-world adapters
- Parent owns the single include in `entity_assets.c`, all `scene.c` glue,
  build/validation and milestone updates

The parser uses a bounded5124-byte temporary catalog so failed reads leave the
output unchanged without putting the catalog on the Xbox stack. The loader's
128KiB scratch ceiling includes both table text and catalog. Unknown material
names follow original Default mapping. Unsupported non-suffix placement of the
word `underwater` rejects rather than imitating malformed original strings.

## Integration API

Forward declarations near the existing empty-fire helpers:

- `static void scene_weapon_impact_audio_reset(void);`
- `static void scene_weapon_impact_audio_close(void);`
- `static void scene_weapon_impact_audio_load(rf_vpp *tables,rf_vpp *audio);`

Include the scene helper immediately after `combat_shot_obstructed`, where
the Foley/spatial services, material adapters and scene types are defined.

Call `load(&tables,&archive)` in `campaign_audio_open` after mandatory
ambient/force PCM admission and before final bank-residency statistics. Both
arguments are that function's still-open local archives. The function returns
void intentionally: optional audio admission cannot fail level loading.
The parent removed the earlier table-archive close after Foley/class binding;
the existing shared `audio_done` close now owns that archive's full lifetime,
including error paths. The audio archive still follows its original successful
transfer into `campaign_audio_archive`.

Call `reset()` during initial scene setup, before the audio load. Do not call
it on combat frame0 after resources were admitted. Call `close()` before the
Foley/bank owners are destroyed; it restores prior eviction flags without
unloading PCM still borrowed by voices. It does not need weapon-switch or
reload resets because each contact is an ordinary nonlooping voice.

Runtime helpers:

- `ready(weapon)` cheaply checks optional admission and a supported original
  conventional weapon identity
- `contact(weapon,material,point)` consumes one actual contact. `material=-1`
  explicitly selects original Default; valid authored material IDs are0..9.
  Other IDs and nonfinite points produce only diagnostic audio failure
- `world(stream,weapon,start,delta,limit)` first checks `ready`, then performs
  an audio-only original ordered-contact ray with flags0x26 and the caller's existing
  nearest-target limit. It maps the real face/solid to authored material, or
  explicitly falls back to Default when metadata is unavailable

Parent keeps all existing damage and obstruction decisions intact. The generic
`combat_shot_obstructed` function remains unchanged: it is also used by pickups
and visibility, which must not emit combat audio. The optional world query
does not control damage, replace its result or emit a hit for a miss. Existing
detached-piece priority, shield/nano early exits and per-pellet contact ordering
remain parent-owned.

## Bounded audio ownership

The allowlist is the ten conventional names already implemented by the player
weapon catalog. Their installed dry groups deduplicate to21 waveform files,
150954 bytes total. Startup preflight permits at most64 sample IDs and192KiB
of selected PCM, retaining at least256KiB of the shared bank budget after new
PCM admission for other runtime audio. These are explicit port resource limits,
not reconstructed retail limits.

Admission is all-or-nothing, after mandatory startup consumers. It rejects
looping samples and never evicts another sample. A failed load rolls back only
newly loaded, not-yet-published PCM. Previously resident rows, borrowers and
eviction flags are preserved. On success, all admitted rows are pinned by
temporarily clearing their shared eviction flags; close restores prior values.
Final ordinary bank bookkeeping counts the preloaded PCM exactly once.

Each accepted hit selects one sample using a separate sound-only RNG, performs
the original same-table Default fallback if the selected sample is-1, checks
that PCM is still resident and then uses the existing generation-qualified
spatial voice owner. No helper-side allocation, archive lookup, file read or
PCM reload occurs per hit. Native device-buffer admission and lifetime remain
the existing audio backend's responsibility. Voice exhaustion or any optional
audio failure cannot reject a shot, change damage or start a reload.

The impact sound-only RNG is transient and does not alter gameplay spread,
damage or save formats. Exact cross-save sound-variant continuity is deferred.
No decals, hit particles, projectile rewrite or visual effect changes are part
of this missing-feedback slice.

## Observability and remaining checks

`rf_scene_weapon_impact_audio[16]` records contact requests, starts, silent
results, errors, world queries/hits, admitted sample count/bytes, latest status,
weapon, material, group, sample, voice, RNG and ready state. Latest requested
point is `rf_scene_weapon_impact_audio_position[3]`. Existing opt-in combat
trace adds `WEAPON_IMPACT_AUDIO` lines; no new fixture or runtime input exists.

The parent review must preserve qualified hit ownership, particularly the
vehicle damage adapter's `handled` result. Ordinary blocked NPC/prop/vehicle
rays call the world helper only for feedback and keep their previous blocking
decision. Parent may retain Default for object contacts without a trustworthy
material rather than fabricating Flesh/Metal/Rock.

Compilation, actual optional admission, sound output, unusual bank pressure,
and repeated level teardown are not claimed verified. The next parent batch
can establish build/startup status; focused natural impact playback remains
separate if the scheduled ordinary reload recipe does not produce it.

## Ordered world-contact limitation

The shared `rf_geometry_collision_ray` intentionally preserves original
retained-fraction shortening across movers and static geometry; it is not a
generic closest-hit query. In overlapping mover/static cases it can report
a mover behind a nearer static face, so this first-pass audio position/material
has the same limitation. Existing damage and obstruction decisions are
unchanged. No separate nearest-query rewrite or exact impact-placement parity
is claimed; this audio refinement is deferred.
