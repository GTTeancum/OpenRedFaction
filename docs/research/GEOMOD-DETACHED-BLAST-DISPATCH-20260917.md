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

## Shared health state and projectile contact follow-up

`rf_geomod_piece_life_init/damage` now reconstruct bounded nonplayer kind3
health state. Initialization uses birth radius times50. Direct damage delegates
to existing rf_damage_dispatch_sp and then applies the health<=0 retirement
flag. Object flags remain separate from physics flags. The helper does not
wake bodies, allocate, cut geometry or publish scene removal. It stages a local
damage object so numeric failures preserve the caller's state. Skipping
already-retired objects and rejecting negative/nonfinite inputs are port policy.

The extraction test checks the seven original damage boundary cases, two-hit
health depletion, exact-zero retirement, immunity, repeated retired calls and
invalid-input preservation. All121 tests pass. NXDK builds, and
`python tools/probe_detached_blast_dispatch.py --nxdk` executes the compiled
Xbox helper in Unicorn: all seven health/flag results match original4892c0
plus412ad0 byte for byte. This is compiled-code evidence, not an XEMU run.

`tools/probe_detached_rocket_contact.py` adapts the prior object-contact harness
to a nonsticky, non-grenade type5 projectile and kind3 target. Supplied class
values use installed Rocket Launcher damage400, radius5 and damage kind3;
optional class flags remain0, so this is a rocket-style control rather than a
claim that every authored class field is loaded. Complete4c4b50/4c59f0 executes.
Direct damage requests400 against target456 before radial damage400/radius5;
the projectile is then marked dead after impact presentation. Damage, registry
and presentation providers are supplied, so target health is not mutated here.
The generic direct-damage arithmetic is independently exercised above.

Live registry ownership, saved health/retirement, and removal from drawing,
collision and motion together are the next integration work. No live chunk
removal is enabled by this helper-only commit. Timer-based disappearance and
further subdivision are not added or claimed.
