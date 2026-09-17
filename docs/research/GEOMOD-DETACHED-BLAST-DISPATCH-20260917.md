# Extracted solids: ordinary blast is not debris relaunch

`python tools/probe_detached_blast_dispatch.py` executes checked original RF.exe
instructions without launching the game. Nine cases pass; generated details are
in artifacts/geomod-postedit-re/detached-blast-dispatch.json.

Full488dc0 builds its blast bounds and walks the entity5cb060, clutter5c9360,
other5cabb8 and admitted projectile872128 lists. Real AABB arithmetic executes.
The probe supplies in-range objects in all four lists and an independently
populated kind3 list5c98e8. All four positive controls reach489010; the kind3
object does not. The extracted-solid constructor4130b0 inserts into5c98e8 at
413288..4132ad (static instruction evidence, also consistent with the previously
executed kind3 descriptor). Do not add these solids to ordinary radial damage
merely because they share collision with the player.

The outer blast also calls491f50. Its effect-mesh service is intercepted here;
this probe does not prove the full blast has no other indirect solid effects.
Static inspection shows its first list uses782514 records with velocity+40 and
bounce count+60, unlike generic kind3 physics bodies. Neither that service nor
ordinary48fe30 debris relaunch is evidence for waking extracted solid bodies.

Full4892c0 direct damage executes with supplied registry lookup, no actor/player
association, ordinary single-player flags and difficulty predicate false.
Kind3 dispatch489551 admits amounts strictly greater than100. It subtracts the
entire admitted amount from health+34, not only the excess. Cases0,1,99.999,
100,the next float above100,200,400 check the threshold and resulting float
health. Body flags and velocity remain unchanged. Full412ad0 then executes
with a supplied non-expired lifetime result; real48ab40 marks object flag2
when health<=0. It does not subdivide the geometry at this boundary.

Static constructor413215..41323b initializes health as public radius+78 times
constant50 at5894cc. This initialization has not yet been independently executed
in the new probe. The existing object-contact handoff in
secondary-re/weapons-object-contact.md proves direct damage precedes radial
damage for its grenade fixtures at4c6132. Rocket-class admission/amount still
needs checking before wiring the rocket into this owner.

Implementation direction: verify the rocket direct-hit caller, then maintain
piece health/retirement alongside saved body ownership, remove retired geometry
from drawing and collision together, and preserve state through checkpoints.
Do not invent a universal blast wake or further subdivision based on particle
behavior. Subsequent geometry cuts, support loss and other force paths remain
separate open investigations. No runtime source changed; no new Xbox build or
visual acceptance is claimed by these probes.
