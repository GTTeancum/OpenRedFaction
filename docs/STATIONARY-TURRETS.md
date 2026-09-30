# Static turret gameplay

## Models and replacement bodies (2026-09-30)

`scene_turret_models.inc` retains authored static geometry, collision geometry,
attachments and material rows for Stationary Turret, Stationary Turret_Plain
and Auto Turret Head. The two stationary classes select their class-declared
damaged model when the gameplay owner dies; the auto head has no authored
replacement and returns `UINT32_MAX` as its dead-model index. Damage, retirement
and transform ownership belong to the gameplay adapter, not the resource cache.

| Class | Live compiled model | Replacement compiled model |
|---|---|---|
| Stationary Turret | sturret_head.v3m | sentry_turret01_dam.v3m |
| Stationary Turret_Plain | sentry_turret_plain.v3m | sentry_turret01_dam_plain.v3m |
| Auto Turret Head | turret_top01.v3m | none authored |

The cache deduplicates case-insensitive compiled filenames, allows six models
and three class bindings, and bounds geometry, collision, tags, material rows,
texture pixels and resource-loading scratch to 2MiB. Models load before any
texture slots are published; texture storage then transfers into the scene's
ordinary materials owner. Model close releases its own geometry/rows/tags;
scene material teardown releases the transferred pixels. Replacement bodies
reuse these resources without allocating on every death.

`scene_turret_draw.inc` accepts the selected model index and the gameplay
owner's actual position/orientation. It submits the same retained static model
path as existing clutter/vehicles, with their bounded CPU fallback. Highest
LOD and the existing first-pass static lighting policy are retained. This does
not establish final lighting parity, independent head/base articulation,
replacement-body physics or original turret effects.

Direct installed `meshes.vpp` inspection found one SUBM per model. Both live
stationary models have a model-local CSPH at `(0,-0.459,0)`, radius
`0.705964267`, parent -1; `eye` at `(0,-0.530805886,0)`; and `muzzle_1` at
`(-0.00209189,-0.546117127,0.934889257)`. Their `primary_1` tag is
`(0,-0.546,0)`. The auto head has no CSPH or eye tag; its `muzzle_1` is
`(0.002023716,-0.023634067,0.234180525)` and `primary_1` is
`(0.002633206,-0.020460198,0.232380927)`. Both damaged models have no tags or
CSPH. Consumers must use their own documented fallback for absent data rather
than claiming an authored attachment/sphere.

Raw CSPH coordinates are asset metadata, not the complete physics creation
rule: reconstructed `rf_model_creation_spheres` recenters a single sphere at
the object origin (486fc5..487040). A body adapter should apply that helper
and class rules when reproducing ordinary physics creation. Static model ray
queries can use retained `collision.parts` with `rf_collision_model_trace`;
supply the world segment plus owner origin/matrix, initialize hit time to the
current nearest limit and use reset0/flags0. Accepted hit points/normals remain
model-local and require the owner transform before world-space effects.

Evidence: `artifacts/turret-model-inventory.json`, read directly from the
installed archive through the existing structural model inspector. All five
models together reference twelve unique authored textures, conservatively
1,298,432 decoded RGBA bytes. This is asset metadata inspection, not an NXDK
build, rendered-output check or original runtime execution. Parent integration and focused Xbox evidence are recorded below.

Integration hooks: load with `scene_turret_models_open` once campaign seeds and
mesh/map archives are available, before runtime owners resolve their class
pair through `scene_turret_model_indices`; resolve authored attachments with
`scene_turret_model_tag`; render with `scene_turret_model_draw`; close with
`scene_turret_models_close` during level teardown. Include the resource file
after campaign-seed declarations and the draw file after the retained static
model helpers. The telemetry array `rf_scene_turret_resources` records model
count, class count, retained/peak bytes, tag count and CSPH count.

## Integrated first pass and Xbox evidence

Static owners now register alongside skeletal NPCs, publish authored UID links,
participate in shared hitscan/projectile/blast damage and physical actor sweeps,
and select their replacement model on death. When_Dead, vitals queries/changes,
Slay and Set_AI_Mode reach these owners. Enemy retaliation can resolve a turret
as its target. Death removes live hit/collision participation; wreck blocking is
not implemented. Auto Turret Head uses the render bound as its explicit physical
sphere fallback because its model has no authored CSPH.

`tools/xemu_turret_combat.py` passed on stock64MiB Xbox in
`artifacts/xemu/turret-combat-20260930-124945/report.json`. The enemy-free CTF06
fixture copies L2S2a UID5547 with its original class,200 instance health, armor
and model bindings; only placement and a catatonic AI action are staged.
Ten ordinary handgun contact/damage calls apply40 damage times the authored0.5
factor, reducing health200 to180 on the first hit and killing once on the tenth.
The live and dead resources submitted117 and402 draw batches respectively,
with no later live target hit. Resource retention862,916bytes and3,398 free
pages (~13.27MiB) fit the stock target. This proves selection/submission and
functional damage, not visual appearance, audio or weapon-input handling.

Autonomous aiming/firing is connected through `scene_turret_scene_combat.inc`;
see STATIONARY-TURRET-COMBAT.md for its separate native evidence and policies.
RFTU1 ordinary-save integration is implemented with staged validation; native
save/load verification passed as recorded below. NPC Attack-to-turret
save targets, active-target restore coverage, parent/base attachments, possession,
runtime-generated Auto Turret heads, death-effect appearance/audio verification and visual inspection
remain open. No original-game runtime or screenshots were used.

## Ordinary save/load and authored controls

RFTU1 wraps the existing vehicle component without changing its inner payload;
it also supports turret-only levels. It validates authored identity, vitals,
pose, aim/mount state, flags, allegiance, target UID and remaining fire timing
before the assignment transaction. Dead model selection is derived from the
class binding. Legacy unwrapped saves retain authored turret defaults.

`artifacts/xemu/turret-save-20260930-130030/report.json` passes60 source frames
and30 fresh-load frames: ten hits kill UID5547, the ordinary save records its
negative health/dead flag, and load restores one dead owner. Only the wreck
submits after load (58 batches); no firing, reacquisition or repeated damage/
death callback occurs. Endpoint minimum3,253 free pages (~12.71MiB).
The first run exposed frame-zero drawing/acquisition before the existing load
transaction; turret AI and drawing now wait until that initialization frame
finishes. The original failed run is retained as evidence.

The L19S1 Set_Friendliness9939 and L14S1 Make_Invulnerable10185 callback gaps
found in the installed-level audit are now wired to static owners. Allegiance
changes immediately clear target, burst and firing deadline; invulnerability
uses the same objectflag4 as other entities. These branches compile on Xbox;
natural authored trigger execution has not been checked. See
TURRET-SCRIPT-ORDER-AUDIT.md for exact links and evidence.

Authored death-effect scheduling is now connected and checked on Xbox, including
no replay after loading a wreck; see TURRET-DEATH-EFFECTS.md.
