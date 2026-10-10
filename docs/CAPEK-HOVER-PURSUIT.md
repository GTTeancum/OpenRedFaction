# Capek hover pursuit: bounded XYZ first pass

Status: independently source-reviewed and parent-integrated against `b9c3c178`;
awaiting the scheduled 09:00 UTC Xbox build. No compilation, syntax check, test,
fixture, emulator session, PC run or campaign route was performed here.

## Authored owner and original evidence

Read-only original `levels2.vpp/L11S3.rfl` inspection confirms UID4938 as
class/script `Capek`, selecting `Capek Cane`/`none`, initially hidden with no
seat host. The RFL entry is at VPP offset17197056, length1905210, SHA256
`86d24185992fb1380e53373b0b921481f70e1f602dbfc070b6c9b71310a999b8`;
its owner record is at RFL offset1311829. No activation or UID/level branch
is introduced, and movement does not depend on weapon or shield ownership.

- `tables.vpp/entity.tbl:588–657` authors Capek hover12, mass100, speed0.3,
  acceleration20 and movement radius0.5. Class flags are `0x0300011f`
  (`walk|fly|climb|holds_weapons|sentient|swim|envirosuit|nano_shield`),
  with flags2 `collide_player=1`. These are evidence, not new constants.
- `movemodes.tbl:110–116` defines hover12 translation `eye,parent,eye` and
  rotation `eye-obj,body-world,none`; Y is a real parent-space translation.
- Original `42a060+0x14` accepts12 as free motion. `40bb2c..40bb44` sends full
  route-point-minus-eye XYZ through `408e50`; `408e54..408e7b` normalizes,
  scales and accumulates it. The free check at `40bbdc` bypasses `40bbf6`'s
  nonfree Y replacement, retaining XYZ arrival at `40bc17..40bc1d`.
- Steering `40b8af..40b910` flattens only modes9/7 or class bit`0x400000`,
  absent in Capek. Full-target steering must remain intact; descriptor-owned
  eye pitch/body yaw are unchanged.

## Source boundary

`scene_capek_pursuit.inc` adds only exact `Capek`, authored12 and current12
admission, with retained record/class bounds. The predicate joins the existing
Sea Creature/Drone XYZ target-refresh and follow2 movement decisions. Target
refresh intentionally can begin a pursuit; translation still requires
`script_move.follow==2`. No general hover/Make_Fly admission is added.

The unchanged shared `scene_xyz_pursuit_live` requires current registry/entity
identity, positive health, no hidden/frozen/terminal state and no linked owner.
Existing seat and scripted-physics suspension gates remain first; refused
exact owners stay still rather than falling through to horizontal pursuit.

This reuses the documented body-space first-pass policy from
`DRONE-XYZ-PURSUIT.md`: target refresh, retained-route/final arrival and
translation all retain Y without inventing an eye-height correction. The
unchanged step consumes resolved `movement.speed`. The later owner-local
shield-speed follow-up now supplies8.0 before a real retained break and
authored0.3 after the ordinary post-break speed setter; existing numeric
speed selections remain in control (see `CAPEK-SHIELD-MOVEMENT.md`). The ordinary full-body `0x460` sweep owns
acceptance; route clearance, collision spheres and retry policy are unchanged.
Ground uphill projection/horizontal slide remain bypassed for XYZ pursuit;
blocking contacts reject the step through the existing path.

Only Sea Creature's existing `sub_pursuit` branch flattens movement steering.
Capek retains the full target. Angular preparation, ordinary commit,
publication, locomotion, target producers and authored-command priority stay
unchanged. Goto, Goto_Player/follow1, Follow_Waypoints, Look and standalone
combat aim keep their previous behavior.

## Limits and unchanged owners

Ground/idle-ground already admit only modes1/3. Existing Slow may select
current1 and then fails this exact12 admission. The later retained-break
consumer also excludes broken owners from XYZ-hover admission and supplies
the owner-local run default and speed policy in `CAPEK-SHIELD-MOVEMENT.md`.

No state, allocation, resource, timer, save row or format is added. Save,
velocity/command/body-angle, settled-state, spawn and placement-clearance
restrictions are unchanged, as are all Cane attack, contact, visual/audio
and save-only owners. No authored overlap or unsupported hover save is waived.

This is kinematic body-space pursuit, not exact eye-space reconstruction,
acceleration20 integration, hover physics, thrust, drag, sliding or animation.
Natural pursuit, route arrival, interruption, steering, collision, attack
interaction and save/load remain runtime-unverified. Parent alone owns
integration and scheduled stock-64-MiB Xbox validation; no runtime recipe
is added.
