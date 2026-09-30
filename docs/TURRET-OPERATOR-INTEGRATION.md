# Authored NPC turret control

`scene_turret_operator.inc` adds bounded control association for an NPC already
attached through `scene_npc_seat_bind.inc`. The seat adapter remains the sole
owner of seat occupancy, parent/tag links, pose publication and physical detach.
The operator adapter gives targeting/cadence/fire to the existing turret weapon
owner and suppresses the seated NPC's separate handheld attack path.

## Evidence and policy

`NPC-VEHICLE-SEAT-ASSIGNMENT.md` identifies three installed stationary-turret
operators, independently of player boarding and generated Auto Turret heads:

| Level | NPC | Host | Initial NPC / host affiliation |
|---|---|---|---|
| L3S2 | guard1 UID936 | Stationary Turret UID935 | 0 / 0 |
| L5S1 | guard1 UID3023 | Stationary Turret UID45 | 0 / 0 |
| L19S1 | miner1 UID9882 | Stationary Turret_Plain UID9877 | 1 / 1 |

Original post-load assignment `0x40969f..0x4096dc` calls common attachment,
requests actor action13, requests host action2 unless host flags814 bit0x4000,
and sets host flags810 bit0x10000. The adapter reproduces that control handoff
only after a validated successful attachment. Rejection before control mutation
allows the seat owner to roll back its physical transaction. Runtime13 is an
occupied actor state, not an independent handheld firing license.

The current parent weapon path remains authoritative for muzzle/eye tags,
rotation limits, Vauss spread, cadence, cover, actual hit ownership and damage.
The adapter neither fires a second shot nor copies the operator's inventory to
the turret. Turret runtime handles remain physical shot sources. Excluding its
own operator from both acquisition and ray interception prevents self-hits;
other shooters can still hit that operator normally.

Host affiliation stays authoritative. L19S1 Set_Friendliness9939 links the two
turret hosts9877/9879, while seated miner9882 remains affiliation1. Copying the
miner's faction back onto the turret every frame would defeat that authored
host change to0. This is a documented first-pass ownership policy supported by
the installed relationships; exact original NPC possession targeting beyond
the recovered attachment path is not claimed.

No installed Attack, Shoot_At, Shoot_Once or Fire_Weapon_No_Anim event references
the three operators as shooters. Fresh binding rejects pending scripted firing,
reload, movement or animation work instead of dropping it. Unsupported scripted
operator orders and motion-only acquisition remain deferred. Attached action13,
ordinary waiting2 and default standing actions can control the host; catatonic,
motion-only or script-owned states hold fire. Host AI action restrictions still
apply independently. A living actor released while still in13 enters waiting2;
this is explicit first-pass exit policy, not a complete implementation of the
original `0x4096f0` cleanup.

## Exact parent hooks

1. Include `scene_turret_operator.inc` after `scene_turret_combat.inc`, before
   `scene_npc_seat_bind.inc`. It forward declares the seat helper's exact
   `scene_npc_seat_pair(host,actor,tag,&attached)` interface. The pair check must
   validate actual seat occupancy and generation-bearing actor/host identities.
   It must not call the operator helper recursively.
2. After a stationary host's physical seat ownership is published, call
   `scene_turret_operator_bind(host,actor,tag,now,0)`. On error, the seat adapter
   rolls back its physical ownership. Control bind validates everything before
   changing AI/control state; re-binding the same valid pair is idempotent and
   a conflicting actor or host is rejected. `now` is int32 simulation milliseconds
   in0..RF_TIMER_PERIOD. Generated head children and player possession do not use
   this NPC API; accepted hosts have use-kind4.
3. Before physical detach, actor/host unregister, death-driven detach or level
   retirement, call `scene_turret_operator_unbind(host,actor,now)`. It clears only
   control association, target/cadence and the attachment-owned control flag.
   The seat helper clears links and restores free movement. A missing endpoint
   is tolerated without touching a replacement object at a stale handle.
4. At the start of each NPC iteration in `campaign_enemy_tick`, **before**
   pending Shoot_Once/alternate-missile processing, skip the whole attack body
   when `scene_turret_operator_suppresses(owner->registration.handle)` is true.
   This does not replace the seat helper's separate free-navigation/gravity
   exclusion. Reject new unsupported script-shot/Attack/Shoot_At requests for
   bound operators at their callback boundary rather than enqueueing a request
   that the handheld gate intentionally cannot execute.
5. In `scene_turret_combat_tick`, query
   `scene_turret_operator_controls(host,&bound,&eligible,&affiliation,&actor)`
   before acquisition. If bound but ineligible, use the ordinary
   `scene_turret_combat_release`/inert path. If eligible, continue through the
   same existing aim/cadence/fire loop. **Do not** set host linked_handle to
   the actor: common attachment stores the host in the actor's linked_handle;
   a host's linked_handle denotes its own parent. Preserve the generated-child
   helper's distinct parent relationship admission.
6. In `scene_turret_scene_target`, exclude a candidate when
   `scene_turret_operator_excludes(source,candidate.handle)` is true. Use the
   host affiliation returned by controls, not the NPC's affiliation. In
   `scene_turret_scene_shot`, apply the same exclusion before testing the NPC's
   body **and shield**. Actual nearest-target damage remains unchanged.
7. After seat teardown call `scene_turret_operator_reset()`. Future ordinary
   save integration must reconstruct actual seat pairs first and then bind with
   `restoring=1`; this path registers control without overwriting imported AI,
   target or fire deadline. Reset old control associations before the complete
   restoration transaction. This helper alone does not serialize occupancy;
   reject unsupported occupied snapshots until both endpoint/seat components
   and their ordering are implemented. Never persist these runtime handles.

The helper stores at most128small control rows, performs registry checks before
dereferencing actors, and requires the seat-pair validator on admission and
every control eligibility check. Stale or dead operators hold fire until the
seat owner detaches them, avoiding an accidental immediate switch to unoccupied
autonomous fire during a broken association.

`rf_scene_turret_operator[8]` records binds, unbinds, eligible ticks, held ticks,
handheld skips, self-exclusions, last actor UID and validation errors. This is a
source-only deliverable: no Xbox build, runtime firing, seated animation, pose,
detach or ordinary-save check was performed by this helper. Death presentation
files remain unchanged from their earlier frozen delivery.

## Integrated follow-up

The shared hooks and RFNS persistence are integrated and Xbox-built. Native
active-seat/save checks are recorded in the NPC-TURRET-SEAT-* documents.
Operator motion-only action11 now uses the existing moving-candidate gate.
A host with an authored operator contract stays inert after its seat becomes
empty, while unassigned autonomous turrets retain their ordinary policy.
Death/detach lifecycle branches compile but have not had separate native checks.
