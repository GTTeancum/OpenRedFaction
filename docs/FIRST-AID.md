# First-aid cabinet pickups

L3S1 places First Aid Kit items26/27, but the scene pickup whitelist previously
recognized only the separate Medical Kit class. The kits were absent from
rendering, collection and persistence registration even when the cabinet opened.

## Implementation and provenance

The installed items.tbl defines First Aid Kit with static mesh meds.v3d,
quantity25 and no item flags. The shared scene now loads its compiled meds.v3m
resource through the existing bounded pickup renderer. It is a distinct class,
not a Medical Kit mesh alias. Its model/textures load only in levels using it;
the resource array gains one descriptor. Existing2MiB per-resource and global
texture-slot limits remain enforced.

Collection uses the existing living-player, two-unit distance and obstruction
checks. First aid shares the existing health-restoration adapter: authored
quantity, health ceiling100, no consumption when no health can be restored,
and persistent retirement of collected UIDs. Those adapter policies are not a
new claim of exact original pickup-function reconstruction. Health restoration
updates the health counter, not the armor counter. Weapon-class validation
excludes this new health class instead of treating it as shotgun ammunition.

An inventory scan finds28 first-aid kits in11 installed levels, including the
training level. This is placement coverage, not validation of all those routes.

## PC evidence

`python tools/replay_l3s1_entry.py --supplies` reproduces14280frames with normal
input from the earlier campaign prefix. Before this fix the exact input finished
with5health and no cabinet kit collection. Afterward it finishes with55health,
50health restored, and both l3s1.rfl UIDs26/27 marked taken. PICKUPS begins
[7,385,383,2,0,26]. Guard109 remains dead and miner789 remains at100health.
No initial health, inventory, actor placement or event changes were needed.

`--checkpoint` extends to14620frames, sidesteps to rifle1228, collects42rounds,
reloads and reaches(-33.033657,.112484,-17.354113), alive with55health and42loaded
rifle rounds. This also verifies cabinet supplies are not regranted while nearby.
It is PC-only. The checkpoint trigger starts guard892's authored approach;
further checkpoint traversal and combat remain open.

Both generators match their executed input bytes. PC and NXDK builds pass.
The native harness now compares PICKUP_VITALS as well as pickup counts and player
health.

## Xbox evidence

artifacts/xemu/render-20260915-021028: PASS14280frames, all33 selected state
comparisons, including all77 player-body words. Native health55, armor0,
health restored50 and armor restored0 exactly match PC. Native PICKUPS begins
[7,385,383,2,0,26], and L3S1 COMBAT begins[5,2,1], covering the ladder guard
and subsequent cabinet recovery without resetting that section's counters.
The two natural transitions also match, including entry into L3S1 at13085.

Endpoint free memory is6229pages (24.33203125MiB); this is not a whole-route
minimum-memory guarantee. All18 saved disc entries were restored and the owned
XEMU instance closed. The later rifle collection/reload/checkpoint extension
remains PC-only. The entire campaign and all28 first-aid placements are not
claimed complete by this encounter verification.
