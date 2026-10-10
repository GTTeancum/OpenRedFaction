# Canonical Drone pitch checkpoint continuation

Status: source-written against `4956ef25e6fac7dca25342dc2d31f6ec918f5c7d`,
independently source-reviewed and parent-integrated after the noon batch.
No compilation, syntax check, test, runtime, new fixture, emulator session or
campaign route was run for this slice. All behavior is unverified. The scheduled
original old-save regression can establish legacy compatibility only; it cannot
exercise new RFNC19 living/retired pitch or post-load aim/pursuit continuation.

## Original evidence and the omitted owner

Read-only original `Installed_Game/RF.exe` evidence was inspected with SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:

- `42b7a0..42b7be` saves all actor `+864` body angles; `42be4e..42be5a`
  restores them. Eye basis `+7e0` is independently retained at
  `42b77f..42b78c` / `42be10..42be1d`.
- Generic `48ad10` packs published orientation `+48`; `48ae50` reconstructs
  physics orientation `+fc` and published orientation `+48`.
- Original `4a0d70`, like `rf_look_orientation`, reconstructs the ordinary
  body from pitch/yaw. Merely restoring a matrix would be overwritten by
  subsequent steering/preparation/commit if body angles remained yaw-only.
- `487f82..487f98` skips floor acquisition for nonfalling descriptors other
  than run1. Exact use-none/fly11 qualifies after candidate ownership proof.
- Installed `entity.tbl:4086..4138` authors exact Drone, robot fly11, use none,
  flags fly/sentient (`0x12`). `movemodes.tbl:102..108` resolves rotations
  `{body-obj, body-world, none}` = `{2,3,0}`, so ordinary motion owns body
  pitch and yaw without banking.

Before this change RFNC retained yaw, eye angles and RFNC7 look continuation,
but no body pitch. Capture rejected pitch/roll and restore reconstructed
`{0,yaw,0}`. Existing RFNC14 physics is explicitly accepted scripted freeze/wake
history: its publication sets `scripted_physics_seen` from presence. Reusing it
for ordinary Drone motion would fabricate history.

## RFNC19 ownership and compatibility

The codec appends a 76-byte tail after RFNC18's Capek latch on every row only
when the component writer selects version19. The tail contains:

- `kind` at byte0: 0 absent, 1 living ordinary Drone body, 2 retired look tombstone
- Body pitch at byte4; existing base-row yaw remains the yaw owner
- Body and object masked flags at bytes8/12, using the unchanged
  `0x99400001` and `0x06000000` masks
- Five XYZ vectors at bytes16..75: velocity, angular velocity, mass-vector
  momentum, force and torque

Kind0 is entirely zero. Kind1 is living, nonretired and nonseated. Kind2 has
zero physics flags and vectors and requires retirement. Both are exclusive
with the RFNC14 scripted-physics tail and with a terminal dead pose. Every
vector is finite; the existing absolute linear-velocity component bound of
`.001` is unchanged. The maximum row size increases from1612 to1688 bytes;
existing component/count/budget limits remain in force. Version19 is selected
only if an admitted row retains this owner. All RFNC1..18 layouts and decoder
semantics remain unchanged, and their new tail decodes entirely zero.

The scene emits kind1 for every admitted canonical ordinary Drone, including
pitch0 and zero angular velocity. A zero pitch at a crossing must never route
through yaw-only capture and silently drop nonzero angular velocity or torque.
The actual look command/delta/offset/vector remain in RFNC7; there is no second
copy or made-up reset of body-delta continuation. No new live ordinary marker
is needed. Existing script assignment sees an absent scripted tail and leaves
both scripted markers zero after RFNC19 restore.

## Deliberately bounded canonical admission

`scene_npc_drone_checkpoint.inc` requires exact immutable Drone identity,
authored and current fly11, use-none, class flags0x12 and descriptor rotations
`{2,3,0}`. It does not require pursuit/follow2: stopped pursuit, standalone
ordinary aim and supported existing Look_At/movement orders retain their own
existing owners and may leave a real pitch. No weapon, UID or level grants
save ownership.

The capture profile additionally requires:

- No scripted-physics marker or history, seat, link, occupant or attachment
- Exact body roll0; eye angles/deltas/offsets and look command all zero;
  roll body-delta0, with actual finite pitch/yaw body-delta retained by RFNC7
- Retained eye limits matching the immutable class and canonical movement
  orientation; no unsaved custom eye envelope
- All living current, next, published model and look bases matching
  `rf_look_orientation({pitch,yaw,0})`, with finite component tolerance
  `1e-6` solely for authored binary32 basis versus extracted-yaw rounding
- A complete ordinary commit/publication boundary: scalar144=1, identical
  current/next/published/model positions, constructor/class/sphere flags outside
  the serialized mask, zero state124/word164/word168, reference15c=-1 and
  canonical contact vector138 `{0,1,0}`
- No falling/support flags, dynamic support handle or additive support
  velocity; quiet linear velocity and all preexisting command/deadline,
  animation, pain, combat, route, inventory and composed-save guards remain

Unmasked body flags must equal the immutable factory/class flags plus the
installed sphere parameter10 bit2000; fly11 startup changes no other low bits.
Force-air-cap200000, pending/contact/special branches and every other
unrepresented dynamic body bit therefore reject capture. Restore reconstructs
that baseline rather than inheriting later resident flags.

Fresh resident counting skips only the new canonical-save profile: an authored
seed can retain a raw pitched/rolled frame before the saved candidate replaces
it. Every preexisting fresh-resident guard stays in place, and the incoming
RFNC19 candidate still requires the full canonical identity/eye proof. Ordinary
SAVE capture never receives that initialization exception.

Arbitrary teleported/rolled bases fail the matrix proof even when their body
roll scalar is zero; there is no fallback to legacy capture after canonical
profile validation fails. Noncanonical eye state, physical partial steps,
unsupported deaths and script-history/pitched composition remain rejected.
The constructor's retained support material alone is not interpreted as an
acquired floor: absent support is proved by the body flags/handle and zero
additive velocity, consistent with the existing exact-hover proof.

## Candidate construction and publication

Restore validates immutable identity, saved fly11 movement, tail ownership and
canonical eye state before any body publication. Retained constructor-only eye
limits must match the identity-bound class during admission as well as capture.
It installs saved pitch and
yaw before building the candidate basis. That basis feeds current/next body
orientation, world tensor, radius-based broad bounds, model basis, transformed
eye position and full candidate-world placement. Canonical ordinary look
orientation is the same reconstructed basis; legacy rows retain their existing
eye-physics reconstruction.

After the existing private position publication and scripted-physics prepare,
the new helper assigns saved masks and all five vectors to the private body.
Canonical omitted scratch fields are reconstructed explicitly so later
resident script/contact state cannot leak into a modern candidate. The final
publication only copies previously admitted state; no steering, integration,
physics event, wake, movement setter, attack, animation start or death/blast
callback is replayed by the new owner.

The exact RFNC19 fly11 absent-support proof uses a separate context flag.
World volume, candidate mover/prop obstacles and room lookup still execute;
only the inappropriate floor-acquisition sweep is skipped. Existing NPC pair,
player/NPC, vehicle/NPC and prop joins consume the same transformed pitched
sphere basis with their unchanged overlap policy. Legacy Drone rows do not
receive the new floor bypass. Existing Capek hover admission is unchanged.

## Retired pitch without reactivation

Successful removal closes the physical body while retaining look state. A
kind2 capture requires an unregistered, closed body and the same canonical
retained look proof. It writes pitch with zero physics payload. Kind2 restore
also requires resident fly11, canonical movement orientation, absent support
handle and zero additive support velocity, because its inert movement/support
banks are intentionally not republished. Restore assigns
pitch/yaw and canonical retained look basis, then uses the existing retired
path; it does not allocate a body, restore model geometry, run clearance for a
nonexistent body or replay removal/death/blast effects.

A retired model bank can still contain its authored basis after fresh load.
It is not an executable pose owner and is deliberately excluded from the
retired recapture proof. The canonical retained look basis is checked both
before and after restore, permitting faithful recapture without reviving or
mutating that stale model. Robot-death pending/draining/fault guards and other
save-only lifecycle exclusions are unchanged.

## Remaining limits

This is a bounded first-playable continuation, not general flying physics or
arbitrary orientation persistence. Scripted pitch/freeze composition, roll,
noncanonical teleport bases, unsupported physical intermediates and expanded
flight/collision fidelity remain out of scope. No timers, scene resources or
new gameplay fixtures were introduced. Parent owns integration and the
scheduled stock-64-MiB Xbox batch. Actual living pitch roundtrip, retired
pitch recapture, post-load angular continuation and collision behavior remain
unverified until an appropriate authorized runtime check exercises them.
