# L3S1 entry gameplay

The uninterrupted L2S2a/L2S3 route now reaches L3S1 on PC and stock64MiB Xbox;
see MAIN-SHAFT.md for native scope. This document covers the subsequent entry encounter; native coverage through
the cabinet is recorded in FIRST-AID.md, while the checkpoint extension is PC-only. No placement, health, inventory or forced-event injection.

Run `python tools/replay_l3s1_entry.py` for the13870-frame guard encounter;
`--side-room` stops at13700 before the ladder. The generators exactly match
executed input files. Artifacts are area3-entry-guard and area3-entry-side.
The older replay_area3_entry.py still covers the earlier L2S3 entry encounter.

## Verified PC route

Turn east after the transition, pass miner789 without firing, then approach
trigger853 near(-44.25,-2.75,-19). Enter the side room and approach the ladder
from the west. Region780 is centered(-41.25,-.5,-15.625), size(1.5,7,1.5);
region84 overlaps at(-41.25,.5,-15.25), size(1,9,1).

Climb to the upper-floor height and aim east with a small northward component.
Guard109 receives rifle hits at local715 and721 (global13800 and13806),
ending at-30health. All five remaining rounds are expended: COMBAT[5,2,1].
Player finishes alive at(-41.526131,.272173,-15.623495), on the ladder,
with5health. Miner789 remains at100health.

The raw entity records identify789 as miner1, relationship value2;109 is
guard1, relationship0. This avoids confusing an armed ally with a hostile.
A failed attempt aimed below the upper floor; no collision changes were made.

## Nearby supplies and remaining work

Original item records place first-aid kits26/27 at approximately(-38.3,.9,-14.9)
and(-38.1,.6,-14.9), each quantity25, plus rifle1228 at(-37.09,.75,-15.11),
quantity42. Trigger12 links to key32 of mover29, 1st_aid01_door, and requires Use.
Further east are rifle916, ammo918 and medical kit919 near x-20..-13.

A local14050-frame extension dismounts and reaches(-37.784561,.131521,-15.628871)
alive, but has not collected the cabinet contents. Supplies, further L3S1
combat/events and native verification of this entry encounter remain open.

## First-aid pickup resolution

The missing class is now implemented with the authored meds model and the shared
health adapter. The14280-frame PC route collects both kits, raises health from5
to55, and retains their taken state. A14620-frame extension collects rifle1228,
reloads42rounds and reaches the checkpoint alive. Use `--supplies` or
`--checkpoint` with replay_l3s1_entry.py. See FIRST-AID.md for implementation,
verification and native scope: the14280-frame Xbox run now passes33 state
comparisons through the guard and both kits. Further checkpoint encounters remain open.
