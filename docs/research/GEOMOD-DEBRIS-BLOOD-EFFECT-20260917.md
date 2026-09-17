# Debris contact blood effect

Original42e3d0 is not a generic gray impact puff. Initialization42db25..42db53
names bloodsplat.vbm for its single billboard and resolves vclip bloodsplat.
The adjacent somenewblood_A.tga resource belongs to the separate blood-pool
path and must not be substituted for this contact effect.

`tools/probe_debris_blood.py` executes complete42e3d0, with real constructor,
vector and color helpers, through natural return. It records496840 particle
allocation and436490 world-effect submission. Sixteen cases cover damage
0/.001/1.25/400, zero/a5 stack fill, and supplied particle allocation failure
or success. The original executable SHA256 is checked. Supplied bitmap37,
frame count6 and clip42 are synthetic resolved tokens, not asset identities.

The first request is pool0, position at the fragment, zero velocity, radius
float(sqrt(damage)*float0.05), life.5, colorff7f7f7f, destination0, growth0,
acceleration0, flags0 and secondary0. The original caller leaves gravity scale,
VBM finish age and copied48 untouched on its stack. The port explicitly zeros
these inactive fields instead of preserving uninitialized memory.

The second request always follows, even if particle allocation fails:
436490(bloodsplat,room,0,position,.25,0,0). The installed vclip parser confirms
85 bloodsplat-drop.tga particles. The definition has gravity/explode particle
flags and authored life/radius/velocity ranges. Actual436490, burst creation,
allocation and rendering do not execute in this probe. The zero secondary
argument must be preserved; do not turn this visual contact into another
radial damage event.

`rf_particle_blood_prepare` now constructs the first packet without allocation.
Four76-byte packets match original output exactly; negative damage preserves
output. The existing debris test suite also retains72 motion,140 gravity and
9 actor-contact comparisons. Source services and original execution evidence
are recorded in artifacts/debris-motion/blood-original.json.

Still open: bind both blood assets to the bounded scene material owner, recover
and integrate the authored one-shot burst path, preserve allocation-failure
ordering, connect after live contact damage, and validate particle lifecycle
and actual rendering on PC/Xbox. No new live blood effect is claimed here.

Stock-profile NXDK compilation, link, XBE and XISO generation pass; log
artifacts/debris-motion/blood-xbox-build.log. No native effect execution or
new visual acceptance is claimed by that build.

## Burst count and live integration correction

The authored count85 is not the contact's emitted count. Actual4c1a97..4c1cec
multiplies count by request scale.25 and repeats while integer iteration is
below21.25: **22 requests**. tools/probe_blood_burst.py executes20 loops over
five scales, two seeds and allocation success/failure. All consume five random
draws per prepared drop even if allocation fails; original CRT/math helpers
execute and only the TLS pointer and allocation service are supplied.
22 captured packets match rf_particle_blood_drop_prepare exactly, including
110 draws and final random state. The helper deliberately supports only the
installed blood recipe's unit+Z direction, zero offset/radius and unprojected
speed; it is not a general vclip implementation.

The live contact now creates the billboard, then all22 drops using the fixed
particle pool and retained debris RNG. Failure to find a free particle cannot
cancel subsequent requests. The scene binds bloodsplat.vbm and the authored
bloodsplat-drop.tga through the existing animation loader and renderer. Both
consume496276bytes under a512KiB combined budget; there is no frame-time asset
allocation. Resource cleanup closes both owners. Ordinary corpse-pool assets
and generic rocket emitters are not substituted.

Visual verification found an important prior integration bug: actor room state
is index+1 while the fragment room is an index. The comparison now converts
the fragment to the actor handle convention. The old explanation that the
ordinary replay was in different rooms was incorrect. Its current321 eligible
checks yield zero overlaps by distance. The diagnostic fixture now uses the
real room conversion and a contact position in front of the camera, inside
the player's model sphere, so visible blood can be inspected.

PC106-frame diagnostic capture shows blood splatter near the crosshair and the
red damage flash. The fixture has one billboard and22 drops with no allocation
failures, health99.4000015 and armor99.3499985. The550-frame continuation passes,
with no repeated damage (84 contact checks, one accepted fixture hit). The
ordinary two-shot control still passes five original wet-birth and35 original
crossing comparisons. Ordinary flying-fragment impact acceptance remains open.

The106-frame native visual capture at artifacts/xemu/render-20260917-011411
shows the blood effect and damage flash. Blood counters match PC exactly.
That run is **not** overall acceptance: it fails the existing TERRAIN_NOISE
requirement because no terrain cut has occurred yet. Both sides correctly
report all-zero noise state. The check remains unchanged; full two-shot
continuation is the acceptance scenario. Disc restoration completed.

## Full native acceptance

artifacts/xemu/render-20260917-011617 completes550frames and passes66
comparisons, including the positive damage/blood verifier. It retains4036free
pages (15.765625MiB) on stock64MiB. The fixed pool emits one billboard and22
drops, with zero exhaustion; their packet hash and RNG match PC. All138 final
ripple vertices (1932words) and capture sections match PC. Endpoint inspection
confirms the room, damaged post, debris, weapon and HUD. Transient blood visual
evidence comes from the earlier106-frame capture, not the expired550 endpoint.
The disc was restored and the owned emulator exited. Full PC build and all115
CTests pass (blood-full-build.log / blood-full-ctest.log).

Remaining: ordinary flying-fragment player contact, broader suppression/death
scenarios, pool exhaustion in the live effect and full visual parity. Rough
project estimate remains50% overall; GeoMod approximately75% following the
player-impact integration, with crater readability/topology work still open.
