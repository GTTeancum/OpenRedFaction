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
