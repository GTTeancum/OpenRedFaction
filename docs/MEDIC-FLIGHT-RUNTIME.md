# Medic flight runtime staging, work in progress

Baseline ef0612a6 (active documentation successor54308182 has identical code). This is the runtime lane of the independently approved source design d302db81 / reviewer7f8cfa79. The initial interface and reviewed visibility/angular continuation amendments are approved for staging; executable component/joint review is pending. No build, test, route, engine execution or active source edit has occurred.

## Ownership

This lane owns scene.c, new scene_medic_flight_types.inc, scene_medic_flight.inc and scene_medic_flight_tick.inc. It also owns the successful accepted-Use target mirror in scene_medic_action.inc and narrow actor-command guards in scene_ai_mode_gameplay.inc, scene_ai_waypoint_acquire.inc, scene_npc_teleport.inc and the successful retirement hook in scene_npc_retirement_orders.inc. Navigation and codec workers own their separate private includes and core files. Do not integrate any lane alone.

The qualified constructor is the already admitted L3S1 UID783 medic1 record with original default2, absent host/Alarm UID and no no-flee switch. Qualification does not depend on healing reserve, syringe presence or actor speech. Executable onset additionally requires actual unarmed, substate0, full current local-player source, humanoid class without0x200/0x400000, no unsupported explicit route/command overlap, and complete supported visibility ownership.

## Required integration

- Shared types precede campaign_npc_body; runtime prototypes precede history/checkpoint consumers.
- Navigation implementation, then visibility implementation, then runtime implementation follow existing secondary-vehicle runtime declarations.
- Tick implementation follows actual ground, stance, body-sweep and landing owners.
- Final serialized-order successful actor registration initializes immutable constructor bias using the approved three-value cycle. No private restore candidate advances it.
- Existing surviving damage notification invokes the two source requests before the old armed-only retaliation path.
- Existing actor callback publishes the real current/pending player motion pair for the source cower blocker predicate.
- Same-level RFNC25 exports complete flight plus actual retained714 and independent shared RNG; codec owns wire/header/joins.
- Source default2 preserves threat/cache/deadlines/pair/route remnants. Successful retirement clears executable owner only after unregister; shared RNG receipt persists.
- Successful player form/non-follow restart marks outgoing-player targets UNRESOLVED. Failed/no-op forms do not mutate flight.

## Explicit policies and limits

Physical movement reuses the existing kinematic grounded NPC sweep, slope and wall handling. It does not replace the sourced alternate search/corridor follower. Real falling velocity keeps existing checkpoint guards; no fake scripted-physics owner or zero-velocity conversion.

Original AI500 is distinct from actor714.403660 clears714 before AI service;403383 tests world command squared magnitude strictly greater than binary64 1e-5, then403395..3b0 copies and transforms it through pre-physics published48 and clamps[-1,1]. The retained actor714 is serialized and remains available to glare/presentation until the next actual AI service. No end-of-tick save-clearing shortcut.

Construction uses approved deterministic zero initial steering174 and the original bias cycle over actual port registrations, without original heap/ordinal parity claim. Shared combat_pain_random is the explicit port RNG stream. Two duration draws,1370 draws, no-route4d4 and sight294 draws retain source ordering; optional persona/resource-duration work remains outside this projection.

The original forward factor uses eye basis7f8 (owner.look.orientation+6), distinct from actor714's published48 transform. Threat route/first LOS endpoint is actual published3c (rf_scene_actor_pose.public_position), not physics origin; eye7d4 remains separate.

Unsupported later visibility ownership defaults only owned current7 to authored2 at its next service, with no fake LOS/reroute/draw and no loss of retained metadata. This is a parent-approved capability policy. Genuine query failures remain errors.

Explicit Goto/Follow/Look ON/Shoot ON/Attack ON/waypoint acquisition/teleport overlaps after flight continuation remain unsupported before their unrepresented command mutation. Known Goto/Shoot_Once target invalidation and OFF default operations retain their separately source-qualified effects. Set_AI_Mode1/2 changes current state while preserving flight remnants; unsupported other modes are refused. Ordinary startup Play_Animation and pain remain independent.

Fresh accepted section replacement uses the approved constructor-reset history projection only; same-level continuation is exact under new grammar. Legacy absent live rows receive inert qualified constructor state while preserving saved AI/ordinary navigation, without route replay, bias-cycle advancement or RNG reseeding. Dead registered current7 retains inert metadata until actual retirement. New RNG receipt survives all medics retiring and joins real pain/burn rows without fake pain.present.

Runtime-only replacement fencing is additional to full registry/class checks, and is not a saved pointer or complete generic actor-lifetime engine. Scoped restore publication suppresses flight callbacks; codec owns candidate validation and preservation of the aliased existing navigation.start through its bounded load lifecycle. No global old-world rollback claim.

Entered-flight snapshots also retain the actual finite angular150/torque174 body vectors, including default and terminal remnants. Untouched constructor rows encode zero for these extra fields and do not overwrite unrelated ordinary physics; a genuine RFNC14 owner equality-joins them. The unconsumed look7b0 timestamp is explicitly outside the executable projection.

Temporary LOS allocation is capped at128KiB before onset through read-only capability admission. A real allocation failure after admitted capability remains an error. Navigation adds no persistent heap owner; the flight member adds164 bytes per retained NPC, with the known medic alone using the qualified continuation.

## Known authored command impact

The conservative historical-entered overlap boundary is a port limitation, never an original refusal predicate. The exact already used L3S1 event section (offset1075377,2493 bytes, SHA2560bf33a22194f4f2c5eb6b08f25362c58592052b7d83b91b247097db5f2b68390) contains29 records. UID783 appears only in links of When_Hit2163 and Play_Animation2211, and in no event word field. Both known linked consumers remain admitted. This establishes no actual authored command intersection for the selected owner; it does not assert arbitrary future command/flight coexistence. Full selected records and bounded extraction are in evidence/known-medic-event-references.json and its companion reader.

Accepted ordinary medic Use installs action14 and then binds the actual player through original409050 at40a746. Its successful commit now mirrors that target into an already-entered qualified flight continuation: exact same PLAYER/full handle is unchanged; NONE/UNRESOLVED/different handle binds PLAYER and sets sight294 due, preserving cache, route/pair and all other deadlines. This no-fail hook is before optional Foley and inactive during scoped restore or constructor-only flight. Completion/range/missing-target defaults do not clear the shared source target.
