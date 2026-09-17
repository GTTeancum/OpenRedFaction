# Hitscan contact uses extracted-solid health

The existing nearest-fragment branch now submits each unoccluded player hit to
rf_geomod_piece_registry_damage before ending that pellet's traversal. This
uses the same shared kind3 health/retirement path as rocket contact. Static
world/mover obstruction still wins before damage, and a nearer NPC still wins
before the fragment branch. There is no new bounding-box target or through-wall
hit. Ordinary primary damage, alternate damage and held-taser per-tick damage
follow the existing actor-side amount selection. Shotgun damage remains per
pellet; values are not summed to circumvent the original strict>100 gate.

`tools/check_detached_hitscan.py` now saves both the real pistol-hit and the
above-fragment miss controls. Both preserve health23.730451583862305 and one
visible/solid fragment. The hit alone sets generic damage-contact flag00200000;
the miss retains0. This proves damage routing despite no health loss for a
below-threshold shot. Stored health/flags are read from the actual RFPB2 trailer,
not inferred from draw counters. One rocket creates the piece; the later pistol
shot is the second shot in the combat journal. No NPC damage is reported.

PC build and the affected scene-authored, extraction and checkpoint-layout
checks pass. NXDK builds. Broad weapon variants, high-damage hitscan retirement,
additional impact decals/audio and NPC-fired chunk damage remain unverified.
The earlier core damage boundary and retirement/save tests are retained.

Stock64MiB XEMU run render-20260917-073439 completes550 frames and passes74
checks with4003 available pages. Native checkpoint matches PC byte for byte;
health remains23.730451583862305 and flags00200000. Native framebuffer inspected:
pistol/HUD, room and retained tilted post fragment are present. The harness
restored the disc and closed its own process. This is the pistol-hit control;
the above-fragment miss was verified on PC only.
