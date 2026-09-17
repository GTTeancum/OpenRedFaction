# Actor/fragment pair admission

`python tools/probe_fragment_actor_admission.py` passes240 full original48be00 cases without hooks. Both argument orders cross player flag0/1, actor class use-kind0/1/3/9/10, fragment geometry pointer absent/present, and fragment body radii.49/.5/.5001/1/1.0001/3. Real4895d0,429990 and486c90 execute. Both objects remain byte-identical; output pair flags are checked.

For ordinary visible SP actors with collision participation enabled, admission requires fragment body radius strictly greater than0.5 and either (player flag plus fragment geometry present) or actor class physics use-kind1. Radius exactly0.5 rejects. The class selector is actor+294->1b4, not current movement mode+858->4; initial inspection conflated those fields and the executable probe corrected it.

If the actor is a player, fragment geometry exists and radius is strictly greater than1, pair bit8 or16 selects the fragment side by argument order. Radius exactly1 does not select that route. NPC use-kind1 admission leaves those bits clear. Original48ca60 dispatch inspection sends the flagged kind3 side to49b570 (polygon solid) and unflagged ordinary contacts to49a420 (body sphere pairs). Merely enabling the existing polygon-only player adapter for NPCs would therefore skip the demonstrated shape selection.

Scope: this probe proves admission and route bits, not execution of49a420/49b570, continuous relative motion, contact response scheduling or live NPC behavior. Other object flags, network policy and other entity types are outside this grid. The existing NPC terrain-only body query remains unchanged until the appropriate sphere-pair route is implemented. Player polygon-only rubble querying also requires this size-policy audit for smaller chunks. NPC ground probing has a separate original path and should not inherit player-only49b900 behavior automatically.

Next: implement the admitted sphere-pair query with source/target body poses, select sphere versus polygon contacts using the recovered policy, then validate NPC movement in the DEV testbed. Evidence: tools/probe_fragment_actor_admission.py and artifacts/geomod-postedit-re/fragment-actor-admission.json. Original binary SHA is asserted. No original game UI or host input was used.

## Shared registry query

`rf_geomod_piece_registry_npc_contact` now assembles live fragment body poses and
sphere lists for the existing original-derived `rf_collision_actors_general_response`
(49a420). It selects the earliest contact for an non-player vehicle actor kind0/use-kind1,
requires target radius strictly above0.5 and collision participation bit0x20,
and skips retired fragments. It uses caller bounds, both current/next poses,
masses and linear velocities without allocating or modifying either owner.

The result carries stable batch/piece identity separately from the contact packet;
its entity handle is UINT32_MAX because fragments are not campaign entities.
Callers must not resolve that value through the actor registry. The query excludes
optional external velocity contributions and does not publish target contacts or
deferred body flags. A complete actor/fragment pair scheduler and live NPC response
remain integration work; this helper alone does not establish gameplay collision.

The extracted-terrain test exercises an admitted crossing, class rejection, exact
radius0.5 rejection, an existing time-zero contact, malformed sphere data, output
preservation and byte-identical target body state. No new visual claim is made.

Validation: all121 PC CTest cases pass; the stock-profile NXDK XBE and ISO build
succeeds (existing linker merge warning). Native execution of this new query is
not yet verified. Logs: artifacts/geomod-postedit-re/npc-contact-pc-tests.log and
npc-contact-xbox-build.log.

## NPC movement adapter

`rf_scene_npc_body_sweep` now merges admitted fragment sphere contacts with the
existing terrain/mover result. Static geometry wins equal-time contacts. A copied
proposal receives fresh sweep bounds before querying; class data is required only
when owned rubble is present. No actor or fragment state is modified by the query.
The existing scripted movement/fall consumers already clamp movement at the
returned fraction. The separate NPC ground probe remains unchanged.

This integrates contact selection, not the full original two-object scheduler:
fragment counterpart contacts, deferred response flags, pushing and crushing are
still open. No live NPC/rubble encounter has been exercised yet. The broad suite
caught a null class-data access in the metadata-free NPC fixture; class lookup was
moved behind the rubble-presence gate and validated there.

Adapter validation: all121 PC tests pass after the fix, and stock-profile NXDK
XBE/ISO builds succeed. Logs are npc-movement-tests.log and npc-movement-xbox.log
under artifacts/geomod-postedit-re. These checks do not prove live visual behavior.

## Scene-adapter fixture

`tests/npc_rubble_tests.inc`, included by the existing NPC residency test, creates
an owned closed cube fragment through the registry emission transaction and a
registered NPC with a finite one-sphere body. It calls the actual
`rf_scene_npc_body_sweep` adapter against an empty world: crossing hits with an
upward normal and a fraction strictly between0 and1; class use-kind0, a path moved
20units sideways, and a retired fragment all miss and preserve the output packet.
The crossing leaves both source and target body states byte-identical.

This is a CPU scene fixture, not an authored live encounter, renderer check, or
NPC locomotion/animation acceptance. It exercises the previously untested owned
rubble branch without adding runtime hooks. The existing full suite remains the
regression gate; native NPC/rubble execution still needs validation.

## Live DEV miner rejection control

Opt-in `RF_REPLAY_DEV_NPC=1` on PC or `D:\dev-npc.flag` on Xbox loads one
installed L1S1 miner1 seed into the ctf06 DEV room, retains its owned raw storage
and allocation accounting, relocates its decoded spawn, and disables enemy attack
updates for this single-actor fixture. At frame400 it receives a scripted move
request; normal motion, body sweep, animation and drawing run afterward.
It is not a general NPC spawning interface. No original assets are modified.

The first800-frame PC run completes with the miner moving from z5.5 to1.25.
The actual class physics use-kind is9, not1, and the real detached chunk body
radius is0.471438289, below the strict0.5 admission threshold. Zero fragment
contacts are therefore expected and observed. The visible height change near
the post is not evidence of fragment contact. `tools/check_dev_npc_rubble.py`
records these facts as a rejection control; it must not claim positive collision.
An eligible class and a larger naturally extracted fragment remain required.
The inspected endpoint shows the miner beside the broken post, with the rocket
launcher and room visible. No movement animation sequence or audio was reviewed.

This run also exposed a startup diagnostic that failed every one-NPC scene
because its aim probe required a second NPC. That unavailable pair is now skipped;
ordinary weapon aiming is unchanged. Native execution of this fixture is pending.

## Meaning of use-kind and priority correction

The selector is the class **use interaction**, not a physics simulation mode.
`rf_entity_class_physics_read` maps authored `$Use: "vehicle"` to1 and
`$Use: "ai response"` to9; the type's name had obscured that distinction in
previous notes. Original486c90 reads the class field at1b4. The expanded
original48be00 probe now explicitly executes kinds9 and10 as well as0/1/3:
all240 cases pass, including kind9 rejection at every tested fragment size.
The installed table's vehicle classes are sub, APC, Jeep01, Fighter01,
masako_fighter, Shuttle and Driller01. These are future vehicle contact cases,
not a reason to change a miner's authored class to force a positive result.

The higher-priority discrepancy is the current player adapter, which queries
all fragment polygons regardless of body radius. Original admission rejects
radius<=0.5 even for a player; admitted player fragments through radius1 use
sphere pairs, with polygons only above1. The current radius0.471438289 post
chunk therefore cannot substantiate original-compatible player standing.
Historical standing/save tests prove internal consistency of the implemented
behavior, not its fidelity. Player movement, ground support and checkpoint
support must be audited together when correcting this routing.
