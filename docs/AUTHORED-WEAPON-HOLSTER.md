# Authored NPC weapon holstering

Source-written on 2026-10-09 for the parent-coordinated 18:00 Xbox batch.
Compilation and natural gameplay/save runtime are unverified. No build, test,
emulator, direct event, authored-route replay or new fixture was run for this
slice. The parent owns `scene.c` integration and scheduled validation.

## Concrete authored gap

The installed `levels1.vpp` member `L6S1.rfl` contains `Holster_Weapon`
event4590, with delay12 seconds and the single linked entity4552. Entity4552
is `guard1`, authored with `12mm handgun` as its primary weapon. Ordinary box
trigger4580 (`Trigger Door`, dimensions4/6/4) links4590 alongside4586,4582,
4583,5159. This is an authored trigger-driven effect, not a forced event or
test placement.

The other installed type64 placement is L14S3 event10324, linked from delayed
event10167, but10324 itself has no target links. No type65
`Holster_Player_Weapon` placement was found in the installed level archives.
This slice therefore implements the concrete NPC consumer only; it does not
invent player holster input, player draw/holster animation or inventory changes.

The shared dispatcher previously named type64 but had no action consumer or
delayed-tick admission. Its NPC world-weapon renderer already knew the original
holster flags, while the practical live enemy firing loop did not consult them.
The result was an authored guard retaining its displayed, usable handgun.

## Original executable evidence

Input: supplied `Installed_Game/RF.exe`, SHA256
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Evidence is static disassembly and direct decoding of original archive records;
no original executable behavior was simulated in this slice.

- Event type table5a1a3c names64 `Holster_Weapon` and65
  `Holster_Player_Weapon`. The generic ON selector4b9070 maps64 through4b912b
  to4b9980;65 maps through4b9135 to4b99e0.
- 4b9980 walks event29c's ordered links, resolves each through typed entity
  lookup426fc0 and ORs entity810 with0x800 and entity7d0 with0x200. It calls no
  inventory mutation, weapon selection, animation or audio service.
- The OFF selector4b9f80 sends64 and65 to no-op return4ba008. The common
  propagation selector4b8c40 admits downstream links for both types. OFF
  therefore does not unholster, and neither action absorbs normal event links.
- 4b99e0 sets only local-player entity810 bit0x800. That distinct unplaced
  player action remains outside this bounded implementation.
- Original firing425830 rejects a holstered owner through428e60 at42586b.
  428e60 first calls429f90: an entity linked to an entity whose class kind is4
  is exempt. Otherwise it reads entity810 bit0x800. Entity7d0 bit0x200 is not
  part of that fire predicate.
- Existing reconstructed `rf_weapon_world_visibility` independently rejects
  entity7d0 bit0x200, and rejects810 bit0x800 unless linked kind4. The scene
  already supplies both words to that renderer.

## Bounded implementation

`rf_runtime_triggers.holster_npc_weapon` is a borrowed type64 service. ON
walks resolved, generation-valid links; unsupported owners are skipped. OFF
does nothing. The existing scheduler, delay and propagation code is unchanged;
the bound consumer is now admitted by the delayed tick. No setup-force event
allowlist was expanded.

`scene_authored_holster.inc` resolves the exact registered living NPC in both
registries, then ORs the two original bits and mirrors them to existing damage
and firing views. It preserves every unrelated bit, inventory, magazine,
selection, animation, movement/order and deadline. No allocation is added.

The parent-owned firing hooks use the recovered810 predicate immediately
before actual shot admission. The ordinary hook follows awareness, target
cleanup, pursuit and reload handling; a second hook guards the existing
secondary queued-shot branch. Queued-shot expiry is untouched. The renderer
continues consuming its existing two-bit visibility policy. Full original AI
draw/unholster transitions remain separate from this authored one-way effect.

## Save continuity

RFNC16 adds one4-byte compact word after RFNC15's pain tail on every row:

- bit0: entity810 bit0x800
- bit1: entity7d0 bit0x200
- all other bits invalid

The codec admits nonzero values only for living nonretired rows. Existing
UID/class, supported weapon/inventory, motion, physics, pain and candidate-world
guards still apply. The writer selects16 only when a row has either holster
bit; later RFNC15 pain rows cannot downgrade the selected version. Otherwise
the existing10..15 selection remains. Maximum serialized row size grows from
1604 to1608 bytes; existing composed budgets still govern admission.

RFNC1..15 decode to zero holster bits. Living assignment restores or clears
only these two bits in the view and relevant mirror words. It does not replay
the event or any gameplay/audio callback. Thus fresh load retains holstering,
and loading an earlier living state clears later holstering. Terminal corpse
entity810 continues to come exclusively from the existing `death_flags_810`;
the new tail does not overwrite it. This is composed checkpoint continuity,
not a new cross-section living-NPC persistence system.

## Parent integration points

1. Include `scene_authored_holster.inc` before `campaign_enemy_tick`, after NPC
   owner, seed and entity-registry declarations are visible.
2. Bind `campaign_triggers.holster_npc_weapon=campaign_holster_npc_weapon` and
   `campaign_triggers.holster_weapon_context=NULL` alongside the other NPC
   event callbacks.
3. After the secondary queued-shot branch's existing live/hidden admission,
   before its missile launch, skip firing when
   `campaign_npc_holster_blocks_fire(owner)` returns true.
4. In the ordinary branch, use the same guard after existing ammo/reload
   handling and before the shot deadline/animation-lock/aim gates.

`rf_scene_authored_holster` is four read-only diagnostic counters: calls,
applied, last authored UID and held fire attempts. No gameplay fixture is
included. The planned 18:00 runtime case remains the already-reserved original
L7S1 sound-switch case; a successful build alone cannot establish natural
holster visibility, firing suppression or RFNC16 save/load behavior.
