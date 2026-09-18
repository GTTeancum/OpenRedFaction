# Rockets now collide with NPCs and moving geometry

The live rocket sweep previously tested static world/liquids and detached
fragments only. A rocket could pass through an NPC or closed moving door and
impact/cut the static world behind it. The shared scene sweep now includes
current committed mover geometry and live NPC body spheres, choosing a nearer
contact across world, movers, actors and fragments. Existing contacts win exact
ties; this composition is practical gameplay policy, not a recovered retail
contact ordering claim. No new allocation is needed during flight queries.

Actor queries transform the retained sphere centers and expand their radius by
the projectile radius, using original-derived508e40 sphere intersection. Contact
points are on the target surface, with normals toward the projectile. Hidden,
retired and dead NPCs are excluded. These are body-sphere contacts, not animated
mesh/headshot queries or continuous relative motion within a simulation tick.
Movers use committed transformed collision geometry and map contact points and
normals back to world space. Mover contacts stop/explode rockets but do not yet
apply generic mover health damage. Player-owned rockets do not query their owner.

## Damage and evidence

An actor contact now dispatches direct explosive damage before the existing
radial blast pass and enters ordinary death presentation when appropriate.
Rocket-local actor/mover tags are separated from fragment tags and never passed
as runtime object handles. Actor/mover hits cannot enter the world crater path.

`tools/probe_detached_rocket_contact.py` now executes original4c4b50/4c59f0
against both a kind3 solid and an actor. The supplied actor location multiplier
.5 produces direct200 from base400; both cases request radial400/radius5 after
the direct call. Providers record requests without applying health changes.
This proves dispatch ordering and multiplication, not a default hit multiplier.
The port currently uses provisional body-location multiplier1; model location
classification/multipliers remain open. Optional class modifiers are unverified.

## Tests and live replay

Scene regression tests cover positive-radius grazing that a center ray misses,
actor filtering, a closed mover ahead of an actor, raising that mover clear of
the ray, nearer rubble winning over both, and a nearer world wall winning over
all of them. All123 PC tests pass. The existing settled-rubble direct rocket
retirement control also passes unchanged.

`python -B tools/check_rocket_npc_contact.py` extends the controlled cover replay:
first create the connected beam fragment with one ordinary rocket, then let the
armed guard fire through the cover/removal sequence. At850 the player fires a
second rocket into the guard's body. It registers one actor contact, direct400,
one death transition and no second terrain edit; the guard stops firing.
The earlier elevated aim missed the body spheres and struck the wall, providing
a useful observed miss rather than evidence of a hit-location damage multiplier.

Native command:

```text
python -B tools/xemu_render_check.py --npc-projectile-test --dev-room --spawn --level ctf06.rfl --archive levelsm.vpp --authored-source 95 --authored-sources 3 --input artifacts/rocket-npc-contact/live.bin --seconds 240
```

Logs: artifacts/rocket-object-{all-build,tests,rubble-control}.log and
artifacts/rocket-npc-{acceptance,native}.log. Native run artifacts/xemu/render-20260917-225054 passes77 PC/Xbox checks.
Actor contact/direct400, death state and two impacts with only one terrain edit
match exactly. The native framebuffer was inspected: dying guard, impact smoke,
room/launcher/HUD visible. This is an endpoint view, not sequential inspection
of every death-animation frame. Endpoint free memory3008 pages (11.75MiB) on
stock64MiB. Disc restored; owned XEMU closed. Emulator audio was disabled.
Remaining: native moving-door encounter, animated model
hit locations, broader actor shapes and projectile types. No full weapon parity
or audible-quality claim.

## Mover transform correction

The first mover regression used query flags0x460, while live rockets use0x1004
(with optional0x100 for very small radii). Bit0x4 tells the solid query that
coordinates are already local. Forwarding it from the static-world query to a
mover skipped that mover's translation and rotation, leaving a raised door
blocking at its old location. Switching the regression to the actual rocket
flags reproduced the raised-door failure before the correction.

The mover adapter now clears0x4 alongside the world-only liquid bit0x1000.
All other query bits remain intact. Sixteen translated/rotated door cases check
analytic contact fraction0.3, world-space point and normal at22.5-degree steps;
the raised-door, actor, nearer rubble and nearer world controls also pass with
live rocket flags. All123 PC tests pass and stock NXDK compilation, linking,
XBE conversion and ISO creation succeed. Logs are
artifacts/rocket-mover-transform-{build,tests,xbox}.log.

This correction has PC behavioral coverage and Xbox build coverage. The prior
77-check native actor encounter did not exercise moving doors, and is not
native behavioral evidence for this correction. Live moving-door validation
remains open.
