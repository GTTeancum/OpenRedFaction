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
Attached moving-child reloads, player/NPC support on the restored body, other
sections and natural triggers remain unverified.

The remaining work is to verify and finish moving collision/support, support
scripted vehicle movement and combat, extend save coverage, and connect the
authored natural event sources.
The selected player-boardable host is still separate from this passive-owner
collection. Other first-list object families remain unbound.
