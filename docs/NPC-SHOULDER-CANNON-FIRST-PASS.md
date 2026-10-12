# Authored NPC shoulder-cannon combat

Source-written against game baseline `7168ad0c4704c1f762c44f612302b37ebbe31a14`.
The parent subsequently recorded docs-only `958a14176e06af352027ef00ebd01219e62bb3c0`;
game bytes are unchanged. Work is staged externally. Compilation and all new
gameplay/save behavior are unverified until the parent-owned hourly stock64 batch.
No builds, tests, fixtures, campaign runs, emulator, images, HDD access, cleanup,
Git operations or original-input writes were performed by this worker.

## Confirmed gap and evidence

The baseline's ordinary and specialized NPC primary selectors all refuse
`shoulder_cannon`. Actual authored owners consequently stop at `!selected.primary`
before awareness, pursuit or attack. Player Fusion and collectible/drop resource
support do not supply an NPC attack consumer.

A read-only parse of installed v180 entity records found 16 explicit primaries,
all class `merc_heavy`, secondary `none`:

- `levels2.vpp`, `L20S1.rfl`: 12473, 12488, 12507
- `levels2.vpp`, `L20S2.rfl`: 18174, 18226, 18239, 18256, 18276, 18284
- `levels3.vpp`, `L14S2.rfl`: 10441
- `levels3.vpp`, `L15S2.rfl`: 8299
- `levels3.vpp`, `L15S4.rfl`: 9583
- `levels3.vpp`, `L18S2.rfl`: 8574, 8585
- `levels3.vpp`, `L18S3.rfl`: 11347, 11404

Installed `weapons.tbl` supplies damage 1000, SP AI scale 0.16, attack range 30,
clip 1, reserve capacity 5, fire wait 0.8 seconds, reload 1 second, speed 20,
lifetime 15, collision radius 0.051, blast radius 25 and crater radius 7.
It names `ShellTest.vfx`, `Weapon_shoulder.v3d`, `Shoulder Launch`, and
`shoulder_cannon_explode` with impact size 9. Homing and Sticky are false;
no alternate attack is authored. The existing loadout constructor owns inventory.

Read-only disassembly of installed RF.exe reconfirms:

- `004c8bd7..004c8be9`: nonplayer SP primary damage multiplies descriptor
  `+0x120` by `+0x108`. The existing AI helper therefore supplies 160, not 1000.
- `004c5cfc..004c5d38` obtains direct-contact damage through `004c8b10`;
  `004c6132` calls damage dispatch `004892c0`.
- `004c62b5..004c62f5` obtains independent radial damage through that getter
  and calls `00488dc0` after direct damage.

The staging evidence folder retains the extracted row, exact entity record
offsets and these disassembly excerpts. Original launch, timing and flight
execution was not observed. Retaining accepted raw damage is the existing
port policy; original late source/descriptor mutation is not reconstructed.

## Connected first-playable behavior

- Exact, registered living NPC admission joins ordinary sight, opposed NPC
  acquisition, retaliation, pursuit, actor/vehicle Attack and primary point or
  finite Shoot_Once orders. Existing seat, holster, pain, hidden, catatonic,
  retreat and scripted-AI suppression remain in the parent scheduler.
- One accepted shell debits one real loaded round. Empty clips use the existing
  finite one-second reload and target-independent completion service. Exhaustion
  uses the existing owned-weapon fallback policy, including its explicit Attack
  exception. Pool refusal preserves ammunition and forced requests.
- Launch and fire animation use the selected actor's existing presentation;
  authored `Shoulder Launch` joins the bounded positional NPC audio resolver.
  Reload uses the existing action-39 mapping and Foley.
- The existing four Fusion flight slots, liquid/world/mover/body/vehicle/prop
  collision, ShellTest rendering, central explosion emitters and terrain damage
  are reused. NPC and player flights compete for those same four slots.
- A retained NPC receipt selects NPC direct-impact dispatch. Accepted source and
  raw 160 damage survive source death or selection changes. Full victim handles
  and the existing world epoch qualify callbacks. Nano consumes before ordinary
  impact effects; direct body/prop/vehicle damage precedes independent radial
  damage. Current player difficulty applies once per final direct or radial
  player request. Raw shield/prop/vehicle values remain unscaled.
  Independent source review corrected the radial call to pass the existing
  world epoch into the shared scan, retaining its between-victim callback guards.
- Player Fusion keeps its existing launch wrapper, 1000 damage, input and impact
  dispatch. The shared frame-zero pool reset moves before NPC scheduling so an
  accepted frame-zero NPC shell cannot be erased by the later player service.
- NPC-only demand bit 12 opens shared Fusion metadata, flight mesh and effects.
  It does not request a first-person view or grant player ownership. Existing
  independent pickup/drop demand can still request player resources.

## Save and footprint boundaries

RFAP1..5 kind 6 remains player-only. An active NPC-owned Fusion shell explicitly
refuses save/export before checkpoint output rather than mislabeling or dropping
it. Valid load preparation is unaffected, and successful timeline publication
uses existing pool reset/assignment. NPC flight serialization is deferred.

Loaded/reserve ammunition, ordinary attack/reload deadlines and target ownership
remain in existing NPC fields. Exact descriptor-only residual fire/reload clip
admission is extended to Fusion after gameplay obligations end, preserving all
candidate inventory, class, mapping, playback, resource and generation checks.
Existing restrictions on ordinary active reloads, scripts and other save state
remain. No file format changes or save-guard relaxations were made.

There is no new per-NPC member, heap owner, pool or allocation ceiling. Each of
the four existing shared flights gains one 32-bit NPC receipt; the existing
temporary checkpoint stage copies that same structure. Six observational
counters add 24 bytes. Compiler alignment/stack and actual stock64 residency
remain for the parent batch. Existing mesh/material/particle caps are unchanged.

## Deferred fidelity and integration

Straight eye-origin aim and immediate primary launch use the existing Fusion
policy. Predictive aim, original charge/thrust timing, minimum blast-safe firing
distance, physical Riot Shield projectile interception, flight-loop audio,
explosion audio, dynamic glow and trail embellishments are not added here.
Friendly and self blast damage retain the existing source-aware explosion policy.
These limits do not turn the attack into a generic ray or a presentation-only shot.

Apply `NPC-SHOULDER-CANNON.patch` as hunks against the recorded baseline; do not
replace the parent's whole `scene.c`. Two new private includes need no build-file
change. The patch also contains this document. Evidence and exact base copies
stay outside tracked source. No generated build/run payloads need cleanup.

Suggested parent milestone: "Authored NPC shoulder-cannon users now have finite
source-integrated projectile combat with original SP AI scaling and guarded
save refusal for live NPC shells; compilation and tester behavior remain pending
(docs/NPC-SHOULDER-CANNON-FIRST-PASS.md)."
