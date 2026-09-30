# Campaign attachment event evidence

Read-only code inspection of the installed `RF.exe` (SHA-256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`)
and the installed level inventory. This is implementation guidance, not a
claim that the events run in the reconstructed Xbox game.

`Item_Pickup_State` (type 54) and `Turn_Off_Physics` (type 62) both map through
factory `0x4b69d0` to the base constructor `0x4bee70`. The base ON method is
the empty `0x4b8cd0`; its ordinary link propagation uses `0x4b8b00` and
`0x4b65c0`. That target dispatcher recognizes registered events, triggers,
group controllers and ambient sounds, not placed items. All five installed
`Item_Pickup_State` events link only First Aid Kit placed-item UIDs. They do
not establish a runtime pickup-enabled toggle, so adding one based on the
event name would invent behavior. The eight `Turn_Off_Physics` events mostly
link moving-group keys and use ordinary base propagation; they likewise do
not establish a dedicated physics-off operation.

`Detach` (type 58) differs: factory case `0x4b7299` constructs vtable
`0x589bac`; ON method `0x4bcc50` resolves each linked object, checks its
eligibility and repeatedly finds its parent with `0x46b970`. It unlinks the
child through `0x46ba10`, preserves its world transform and clears the
child's parent reference. OFF uses the base method. The five authored Detach
events link the L5S3 moving submarine (UID 3977), three L20S1 fighter
attachments (1490, 12335, 12369), and an L20S2 hangar-lift child (4717).

The reconstructed runtime now registers passive pose owners for authored
group-linked vehicle entities outside the single player-boardable host. It
binds the first general-object UID list to those handles, propagates the
group's translation or rotation into their world poses, and removes all of a
linked child's group memberships on `Detach` ON. The detached pose stays live
and unchanged. Event links resolve to the same registered owner. Secondary
instances now load one static chassis resource per class, share it across
instances, merge its authored materials into the Xbox renderer, and submit
room-visible models at their live group poses. One body-local set of model
collision spheres per class is overlaid with authored `entity.tbl` sphere
settings, then queried at each secondary instance's current pose for player
and NPC body/ground sweeps. Its committed group velocity is available to the
ordinary support resolver. These remain passive rather than steerable vehicles.
They use a port-internal registry type so NPC-only event effects cannot treat
their lightweight pose records as full actor views.

`tools/xemu_vehicle_group_detach.py` stages L20S2's authored `When_Dead`
event 18354 to activate `Hanger Lift001`, then `Detach` 18377 at frame 60.
Stock-64-MiB XEMU completed 90 frames: vehicle 4717 moved from authored
X=416.25 to X=426.58 at the live probe, detached at X=426.03, and retained
exactly that pose through frame 90. The final run is
`artifacts/xemu/vehicle-group-detach-20260930-030536`, with 4,144 free pages.
The 10-frame L20S1 inventory run
`artifacts/xemu/vehicle-group-inventory-20260930-030006` registered all three
Fighters and their six group bindings with 5,078 free pages. These are focused
process-local fixtures, not campaign-route playthroughs or visual proof.
The L5S3 10-frame fixture
`artifacts/xemu/vehicle-group-submarine-20260930-030252` kept the selected
boardable submarine separate from group child 3977; it registered one passive
owner and one binding with 5,755 free pages.

The renderer pass reuses the already loaded player-boardable `sub` chassis,
while L20S1's three Fighters share one `Fighter01` resource and L20S2's
`masako_fighter` loads `Fighter02.v3m` without requiring a driver seat. Staged
nearby actor cameras on stock-64-MiB XEMU yielded four retained model batches
for the submarine (`vehicle-group-submarine-20260930-032618`), nine for the
three Fighters (`vehicle-group-inventory-20260930-032138`), and three for
Masako's fighter (`vehicle-group-masako-20260930-031921`). The latter two
finished with 4,802 and 3,835 free pages respectively. The native retained
renderer submits these batches without appending CPU preview vertices; the
per-batch path and resource ownership are verified, while final pixels and
PS2-level visual parity remain unverified under the current no-images rule.
The 90-frame L20S2 lift-and-Detach regression also passed with the secondary
draw path enabled (`vehicle-group-detach-20260930-032346`, 3,856 free pages).

The stock-64-MiB contact fixture walks the staged player toward L20S2's
Masako fighter for 120 frames. The final generic actor-collision build
(`vehicle-group-contact-20260930-034111`) reported 234 chassis hits, including
43 player ground contacts, and ended with 3,834 free pages. L20S1's
three-Fighter load (`vehicle-group-inventory-20260930-033916`) also passed
with 4,801 free pages and authored-sphere contact. This establishes Xbox
player-to-secondary-body contact; NPC use of the common query, moving-platform
carry and body motion against a stationary actor still need live checks.

Ordinary saves now wrap the optional boardable vehicle record in an `RFVA1`
vehicle-section record when group-owned secondary vehicles exist. Each row
stores an authored UID, attached flag and world pose; reload validates those
identities, restores the poses and rebuilds group memberships from the level's
authored first-list references. The separate mover candidate ignores those
general-object members while reconstructing collision geometry, then the
attachment owner publishes them in the same restore transaction. Existing
vehicle sections without `RFVA1` remain readable in levels without passive
attachments. `Reverse_Mover` type 89 is now admitted by the common event
checkpoint codec; its persistent motion is owned by the mover section.

The image-free stock-64-MiB L20S2 fixture
`artifacts/xemu/vehicle-attachment-save-20260930-035804` fired the authored
lift and Detach events, saved a 61,308-byte ordinary checkpoint, then loaded
it in a fresh scene. UID 4717 stayed detached at exactly
`(426.0252075, -10.4686127, -412.7147827)` with 3,662 free pages after load.
The companion `--attached` fixture
`artifacts/xemu/vehicle-attached-save-20260930-040612` saved UID 4717 while
the lift was moving at `(426.6378784, -11.0811043, -412.7147827)` and loaded
the still-linked child into a fresh scene. Over 20 resumed frames it moved to
`(426.1127319, -10.5561123, -412.7147827)`, with 18 movement updates and
3,662 free pages. Player/NPC support on a restored moving body, other sections
and natural triggers remain unverified.

The process-local `--carry` fixture in `tools/xemu_vehicle_group_detach.py`
places the player on the live collision-sphere roof at frame 30 and seeds an
already-supported state. Stock-64-MiB XEMU
(`artifacts/xemu/vehicle-group-carry-20260930-045304`) kept the player grounded
and alive through frame 120 with 3,833 free pages. From the frame-40 probe to
frame 120, the chassis moved X `426.43365→424.30389` and Y
`-10.87694→-8.74781`; the player moved X `425.53290→423.40314` and Y
`-8.33747→-6.20833`, matching the two chassis displacements within float
rounding. This verifies carrying an already-supported player, not natural
landing. A separate unseeded placement
(`artifacts/xemu/vehicle-group-carry-20260930-044844`) saw chassis contacts but
stayed airborne while the fighter rose, exposing the missing moving-host
support acquisition.

A first-pass moving-host acquisition now checks an airborne player's lowest
body sphere against upward-facing authored spheres on rising passive vehicles.
It accepts only a close foot-to-surface gap while the actor is not ascending,
sets the ordinary support owner/velocity and lets the existing player movement
path carry it. This is a practical port response rather than a recovered
retail scheduler. The unseeded `--rising` Xbox fixture
(`artifacts/xemu/vehicle-group-rising-20260930-050329`) placed the player near
the live fighter roof without assigning support. The new path accepted support
five times, ended grounded and alive, and matched fighter displacement from
frame 40 to 120: fighter X `426.37531→424.30389`, Y `-10.81861→-8.74781`;
player X `425.47455→423.40314`, Y `-8.28021→-6.20941`. Stock XEMU retained
3,833 free pages. Repeated acquisition indicates contact can still drop between
frames; broader falling arrivals, side push, ceilings and NPC support remain.

Ordinary player save state now resolves a passive vehicle support handle to its
authored UID. The loader validates the saved rider against the staged `RFVA1`
chassis pose before publishing either state, then restores the player's support
handle so the vehicle's live velocity continues to carry the rider. The stock
64-MiB `--riding` XEMU fixture saved at frame 90 and reloaded into a fresh scene
for 20 frames (`artifacts/xemu/vehicle-riding-save-20260930-052811`). The
checkpoint recorded support UID 4717; both the fighter and grounded rider
moved together after load, with 3,608 free pages at the end. This verifies a
staged rider on L20S2's attached fighter, not naturally acquired support across
other vehicles or the full campaign flow.

The remaining work is to refine continuous rising-platform contact and moving
side collision response, support scripted vehicle movement and combat, extend
NPC and player save coverage, and connect the
authored natural event sources.
The selected player-boardable host is still separate from this passive-owner
collection. Other first-list object families remain unbound.

Scripted NPC ground checks now include the registered passive-vehicle sphere
query and retain the authored chassis support handle. A near-foot fallback
reacquires that same handle when a translating spherical roof slips between
successive downward probes; it does not grant support from unrelated objects.
The focused L20S2 stock-64-MiB `--npc` fixture placed one live NPC on fighter
4717 at frame 30 and ran 65 frames. At frame 40 and at completion the NPC had
the fighter's support handle and normal ground mode. The run recorded 24
moving-ground contacts and 23 carry commits; the NPC's vertical displacement
matched the fighter's, with 3,832 free pages
(`artifacts/xemu/vehicle-group-npc-20260930-055508`). This proves a staged
scripted NPC contact/carry case; ordinary NPC save/reload, natural arrivals,
NPC side pushes and other chassis remain open.

A translation-only side-push pass now sweeps each attached secondary chassis
from its previous to current position against a stationary player. It moves
the player by the remaining chassis travel after a horizontal contact and
checks compiled-world, mover, detached-piece and boardable-host clearance
before committing. The stock-64-MiB `--side` L20S2 fixture placed the player
beside fighter 4717 while the lift was moving left; over 75 frames it recorded
19 side hits and 19 accepted pushes, with no blocked clearance and 3,831 free
pages (`artifacts/xemu/vehicle-group-side-20260930-062010`). Between its
frame-45 probe and completion the fighter moved X `426.22943→425.61676` and
the player X `423.00952→422.56274`. The existing unseeded rising-roof carry
fixture still passed after this change
(`artifacts/xemu/vehicle-group-rising-20260930-062301`). Rotating hosts,
wall crush, other passive hosts as clearance blockers and naturally staged
side encounters remain open. NPC ordinary saves also remain open: RFNC8
rejects nonzero support velocity and has no authored support UID field, so
that work needs an explicit checkpoint-format extension rather than merely a
restore-time reprobe.

The earlier lift fixture directly fires event 18354. Its natural `When_Dead`
source links Fighter01 UIDs 4801 and 18353, which are ordinary vehicle
entities rather than skeletal NPCs or group children. The scene now registers
all authored non-boardable vehicles with distinct identities, shared class
hulls and per-instance health/armor. Their hulls participate in the existing
firearm and projectile hit selection and the common entity-damage service;
the retained wreck identity exposes death to `campaign_death_query`. The
shared watcher treats linked event/trigger/controller/mover outputs as
nonliving, so the two Fighter deaths determine when its lift output fires.

The process-local stock-64-MiB Xbox `--natural` fixture now constructs short
rays against the authored Fighter hull spheres at frames 30 and 60, selects
each vehicle through the ordinary firearm vehicle-hit query, and applies
fatal rocket-class damage using the returned contact. It does not fire event
18354 directly. The 90-frame run
`artifacts/xemu/vehicle-group-natural-20260930-080957` passed: both rays
entered the hull from outside and selected their intended Fighter, both
vehicles were destroyed, `When_Dead`
fired once at 1016 ms, the lift moved, and 3,612 free pages remained. This
proves the shared ray-selection/contact/damage path in a contained case;
full player aiming, world cover and actual weapon flight remain unverified.
The focused stock-64-MiB Xbox player-weapon fixture
`artifacts/xemu/vehicle-player-shot-20260930-083146` staged one clear eye ray
beside L20S2 Fighter01 UID 4801, then let the ordinary player Sniper Rifle
combat tick select the hull, check world obstruction, consume one round and
apply the class's armor-piercing bullet multiplier. Health fell from 900 to
825, loaded ammo from 2 to 1, and 3,594 physical pages remained free. The
installed `Fighter01` damage factors are zero for bash and ordinary bullets,
0.3 for armor-piercing bullets and 0.5 for explosives; the earlier staged
handgun shot consumed ammo and correctly left health at 900. The passing
fixture proves one clear player-sourced precision shot, not player navigation
and aiming from an unstaged pose or a wall-blocked negative case.
The shared explosion scan now includes living ordinary vehicle owners. It uses
the same authored-center distance falloff and compiled-world cover ray as the
existing NPC and boardable-vehicle blast paths, then dispatches each accepted
amount through the vehicle's class damage factors and registered identity.
Applied player-sourced hits also update the ordinary recent-hit marker and
damage journal. The focused stock-64-MiB Xbox run
`artifacts/xemu/vehicle-player-shot-20260930-084546` combined the staged
Sniper Rifle hit (900 to 825 health, one round spent) with a later 40-damage
explosion centered on Fighter01 UID 4801. Its 0.5 explosive factor reduced
health to 805; both hits appeared in the damage journal, and 3,578 physical
pages remained free. This verifies one direct center blast and the ordinary
class multiplier, while edge falloff, simultaneous vehicles, destructible
world cover, NPC-held explosives and visual effects remain open. NPC support
saves require a candidate-world checkpoint change because NPC placement is
currently validated before the saved passive-vehicle poses are staged.
The follow-up stock-64-MiB Xbox run
`artifacts/xemu/vehicle-player-shot-20260930-091911` placed existing L20S2
armed NPC UID4726 at the fixture's cover-checked firing pose and issued its
ordinary scripted Attack order against Fighter UID4801. The NPC fired at
frames65 and95 and both hits passed hull contact, cover and class damage:
health fell805 to767.5, with18.75 applied per shot. Damage journal type2
identifies each as NPC-to-vehicle rather than player damage;3,544 physical
pages remained free and test disc state was restored. Natural travel into
range and blocked-cover rejection remain open.
The ordinary stock-64-MiB save/reload check
`artifacts/xemu/vehicle-npc-attack-save-20260930-094023` kept NPC UID4726
at its authored grounded pose, issued Attack toward Fighter UID4801 and held
pursuit until reload. The62,044-byte world checkpoint captured the active
order with805 hull health; a fresh process restored the same attacker, target
and health, leaving3,130 physical pages free. This establishes RFNC target UID
rebinding against a living RFVA2 passive owner. It does not establish save of
an NPC riding a moved vehicle: that placement still needs staged passive
poses before NPC candidate-world validation. Post-load pursuit and firing
continuation also remain unverified.
The Xbox `--attached` save/load run
`artifacts/xemu/vehicle-attached-save-20260930-073300` passed with all eight
L20S2 vehicle records and the lift attachment continuing after reload.
`RFVA2` now writes 80-byte rows with pose, attachment, health, armor, object
flags, damage flags and destroyed state. The loader accepts older `RFVA1`
56-byte pose rows by authored UID; vehicles absent from an old row set retain
their authored initial state. A stock-64-MiB Xbox save/load run
`artifacts/xemu/vehicle-death-save-20260930-075209` passed after the same two
Fighter deaths. The checkpoint stored both at -49,100 health with destroyed
state set; after loading, both remained nonliving and `When_Dead` remained
fired at 1016 ms without another damage event. The attached lift fighter
continued moving. Save and load ended with 3,633 and 3,441 free pages,
respectively, and disc flags were restored. Native loading of an older
`RFVA1` file has not been checked. Active burn/audio and attribution state,
wreck effects, live player-shot encounters and final visual content
remain open.
