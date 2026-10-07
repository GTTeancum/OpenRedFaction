# Authored moving control children

## Implementation

First-list moving-group references include static control meshes and trigger
volumes as well as vehicle chassis. The scene now retains the exact ordinary
clutter/trigger owners and their full registry handles, binds admitted references
in authored order, and derives each pose from immutable authored bases using the
existing group transform service (`46bbe0` and reconstructed group propagation).

Static meshes publish their position, basis, collision bounds/tensor, query pose
and room together through the existing clutter pose service. Trigger volumes use
the runtime box-axis order produced by `rf_trigger_volume_init`; their shape,
size, radius, links, activation count, timers and gameplay flags retain their
normal owners. Parent motion is published before the ordinary trigger-contact
batch. Disabled triggers still move so their next activation sees the current
volume. Removed/dead clutter is not resurrected.

## Save boundary

No wire format changes are required. RFMC1 owns controller motion, RFPC1 owns
clutter health/flags, and the existing trigger checkpoint owns activation state.
The clutter identity continues hashing the immutable authored pose; a differing
live pose is admitted only for the exact registered child whose derived current
parent transform agrees with its published fields.

World preparation derives child poses from staged RFMC controllers and applies
them to copied collision props before NPC, player, passive rider and active
vehicle placement. Admitted controls lose the unchanged-authored overlap
exemption. All identity and geometry checks precede publication. The final
assignment-only pass follows the mission trigger copy, preserving restored
activation state and publishing the prepared mesh/volume poses atomically.
RFVA restoration rebuilds the original ordered control memberships alongside
vehicle children.

## Stock-64-MiB Xbox validation

`tools/xemu_vehicle_attached_controls.py --case all` passes two isolated L20S2
cases: translation and +Y yaw, each with 150 source frames and 40 fresh-process
load frames. This is a process-contained functional fixture: it submits explicit
positions to the ordinary registered-player contact/Use callback and its normal
linked activation path, without additional player relocation or host input. The fixture explicitly
stages the initial player view near Fighter 4717.
The old and moved trigger locations are separated by more than 4m.

Both cases verify all three clutter meshes (UIDs 4527/4528/4529), trigger 4530,
Fighter 4717 and controller 4543 retain their exact full handles. Every sampled
center and all nine basis values agree with independently calculated parent
transforms, and mesh/body/query publication agrees exactly. Lamp health remains
160/160, switch health 100 and Fighter health 2000. Ordinary Use at the current
volume activates the linked controller; old-position Use, current-position
without Use, and disabled Use do not activate it.

Each 3,480-byte ordinary save retains two activations and disabled flags 17 using
unchanged RFWC2/RFMC1/RFPC1/RFTC1/RFVA2 layouts and verified checksums. Fresh load
restores those fields and the moved geometry without setup replay. The first
observation includes one ordinary controller tick after restoration; subsequent
linked enabling and current-position Use produce exactly one new activation,
and motion continues. Source/load endpoints retain 6,295/6,135 free pages on
64 MiB in both cases. The translation trigger moves from
`[403.8125,-1.375,-406.34375]` to
`[407.984497,-5.545837,-406.34375]` before save; yaw saves angle
`-0.831941068` and reaches `-1.053016305` after load.

Final raw report: `artifacts/xemu/vehicle-attached-controls-20261007-143345/report.json`.
The initial `143025` FAIL remains preserved: the fixture incorrectly selected a
shortcut reserved for delayed type 2 events when requesting UnHide. Removing
that fixture flag selects the supported ordinary event path; gameplay source
was unchanged. All original disc inputs were restored and independently hashed.
The private proof archive retains each phase's exact prelaunch XBE/PE/map copies. The common tested PE
SHA256 is `632ad891fc1648eca5e6fea1668ad3ee4417525ebe14106a2b650cad8b445ac2`;
the four XBE packaging hashes remain separate in the report. Source and fixture
builds pass NXDK. Compiler stack audit retains the 128 KiB reserve and 45,212-byte
listed scene subtotal; this is not a runtime high-water measurement.

Private fixture edits are explicit in its recipe: unrelated actors/objects and
auto trigger 18344 are removed, the actual trigger's initial-disable flag and
30-second cooldown are cleared, real Invert 17967 is linked to 4530 for the
saved-disabled test, and controller legs are six seconds (yaw 120 degrees).
Retained entity/clutter records and geometry are byte-exact. Source UnHide 18359
reveals the Fighter; fresh load does not replay it.

## Limits

The adapter admits registered static clutter and triggers. Light, glare,
particle, item and other child families do not gain invented owners. Existing tag glows consume the normal parent pose-dirty notification; no new
glare ownership or lifecycle is introduced. Their runtime motion is not yet
verified by this fixture; after a load they settle on the next ordinary
attachment pass, so the first restored draw can retain the preceding glow pose. The bounded owner cap is 128; the
installed-level metadata audit's largest first-list total is 86 unique references
across all families. Duplicate memberships retain the existing four-transform
limit. Controls keep their authored group membership; no new detach lifecycle,
active-vehicle controller scheduling, passenger behavior or campaign progression
is introduced. The separate passive-owner first-word record-layout issue remains
outside this change.
