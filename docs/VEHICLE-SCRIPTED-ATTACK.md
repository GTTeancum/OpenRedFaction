# Scripted Fighter Attack

## Implementation status

Authored vehicle Attack now has a working-code path in the isolated source
checkpoint. The combined c29d6f source passed the parent's12:00 UTC NXDK
build; it has not been run in-game. Per the user's latest instruction, the
parent owns one consolidated test pass per hour; this slice
adds no fixture, test suite, emulator launch, capture or campaign playthrough.

## Original evidence

`docs/SCRIPTED-ATTACKS.md` records RF.exe SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`:
loader462150 calls4b86d0, storing the shooter UID at event+2b8;
ON4bcac0 resolves it through48a4a0/426fc0, resets AI through408ac0 and
assigns the linked target through409050. The player name overrides links.
OFF4bcba0 clears the target with409050(-1), then calls407ee0.
That executable is absent from this cloud input set, so the recorded binary
analysis was read, not rerun. No new executable-equivalence claim is made.

The current original `levels2.vpp/L12S1.rfl` bytes establish:

- Attack9698, `attack_jeep_driver`: zero delay, words[9700,0], link7646.
- Shooter9700: Fighter01, friendliness0, health900/armor0, primary
  Fighter Minigun, explicitly secondary `none`, no seat host. The moving-group
  payload contains no reference to9700.
- Target7646: miner1, friendliness2, health1/armor1, no weapons, authored
  driver of Jeep7629. The Jeep has health400 and friendliness1.
- `weapons.tbl`: Fighter Minigun capacity900, AP damage100, AI scale0.20,
  AI range25, cadence0.05seconds, startup0.10seconds, speed275, lifetime2,
  sphere radius0.02 and `undeviating`. AI hits therefore request20 AP damage;
  existing target-class factors, armor, invulnerability and damage attribution
  still apply. Autonomous rockets are not invented for the instance's `none`.

Level SHA256: `f1650e3c49077ea3d3957fca01981a4dff345d24b8b0f07a22388070bf4dc65c`.
Attack record SHA256: `65f96232223d42051014148aa4057012dcb701775e0dc7fa6cb09f74f51ef8b0`.
Fighter record SHA256: `3eb863f778ff5f32dc54bd50ce00018f6ae42ad36e76f91b79a427f6f58ef2d2`.
The compact source audit is packaged in `artifacts/vehicle-attack/source-evidence.json`;
it contains metadata and hashes, no original asset bytes.

## Ownership and stepping

`campaign_script_attack` tries a typed vehicle adapter before skeletal NPCs.
Only a real Fighter01 owner with an admitted minigun is accepted; passive
kind11 owners never masquerade as NPC entity views. Actual selected or passive
registration, damage handle and authored UID must agree. The original event
record is required for independent motion admission. Masako's different class,
unsupported replacement primary guns, occupied shooters, attached/group-owned
or rider-owned independent hosts retain explicit unsupported results.

Each admitted shooter retains its own target, finite ammo, warmup/cooldown and
48 bounded bullet slots. No callback reloads assets or rebuilds a cockpit.
The shared existing readers load the primary and AI metadata during startup.
A repeat replaces targeting while preserving ammunition and accepted cooldown.
OFF clears targeting/spin-up; already emitted rounds finish normally. A dead,
removed or stale target does not turn into the player. A Goto/Follow_Waypoints
or selected AI-mode replacement cancels the older attack.

An independent Attack order drives the existing dry full-hull Fighter solver.
It samples the target's live eye, yaws/pitches the actual chassis, and releases
throttle inside75% of the authored range. This standoff and angular-control
policy are practical integration choices, not recovered retail constants.
There is no obstacle-avoidance/pathfinding reconstruction in this slice.
Hidden/frozen/occupied/rider-owned hosts do not emit new fire.

Fire originates at actual muzzle_1 and retains its transformed forward vector.
The target must be within authored range and the forward alignment gate; a
nearest-cover query must be clear or hit the intended actor/its occupied hull.
The real swept bullet then chooses the nearest world/mover/rubble/actor/hull
contact and uses ordinary AP damage. A Jeep hull can intercept a shot aimed at
its driver; the adapter never bypasses it to directly kill the authored target.
Source credit is the firing vehicle's full handle. Existing source-affiliation
and occupied-driver rules remain unchanged. Shared tracer/audio code presents
accepted rounds without introducing new assets.

Player takeover cancels the order and shares remaining minigun supply and
cadence. Switching cannot transfer a host with an active order or live scripted
round. An already parked switch-bank host cannot acquire a competing independent
solver; ordinary promotion is required. Existing independent-host switching
restrictions remain. Passive weapons do not borrow the selected vehicle's ammo
or projectile pool, so another selected class does not suppress their fire.

## Ordinary saves

Optional RFAK1 lives inside RFNS and outside RFAL/RFPV/RFSV/RFVA. It is omitted
when no scripted shooter was admitted, preserving legacy payload bytes.
The16-byte header stores version, inner length and owner count. Each44-byte
owner row stores authored shooter/event/target keys, active state, ammunition,
spin-up/cooldown/held state, shot count and live-flight count. Each48-byte flight
row stores its original slot, position, velocity, exact binary64 remaining
lifetime and previous position. Radius and source handle are reconstructed from
the source-bound definition and freshly registered shooter.

Inactive/dead shooters remain in this state bank, preserving spent ammunition
and bullets after OFF/death. Readers validate lengths, budget, duplicate owners
and flight slots, class/registration, original event relationship, target keys,
finite bounded flight state and cadence before publication. Target handles are
rebound by authored UID, or by the explicit player key, never serialized.
RFSV continues owning rigid physics. Its Attack kind is inferred from original
Event38/words[0] rather than adding a fabricated linked shooter or changing its
row format. Cross-admission requires every active independent attack to have
its matching motion owner and every attack-kind motion owner to have RFAK state.
Publication follows selected/passive/seat/secondary restore with no event replay.

Old saves without RFAK clear this bank. Older executables do not understand the
new wrapper or Attack-kind RFSV order. Legacy RFCP capture refuses an admitted
scripted shooter rather than silently discarding its state. Player-emitted
Fighter projectile saves retain their previous restrictions; this codec covers
the newly implemented autonomous minigun flights.

## Remaining work

A concise independent source review found no material blocker in the authored
9700-to7646 path. The parent's12:00 UTC combined NXDK build passes; runtime verification remains
pending. Natural encounter timing, long pursuit around obstacles, exact retail
steering, other vehicle weapon classes, autonomous secondary selection,
scripted movement of already parked switch-bank hosts, cross-section retention
and broader rider/occupancy combinations remain outside this implementation.
No overall percentage increase or retail-readiness claim is made for this slice.
