# Bound ordinary no-dwell Use triggers to a press

Source-only staged proposal, 2026-10-10. Baseline: frozen
`53dd1a928ca0168e854a8e5b5cf870910098b040`.

Deliverables: `use-trigger-edge.patch`, staged `src/diagnostic/scene.c`,
`actual-zero-dwell-door-owners.json`, and `original-input-edge-disassembly.txt`.
The active repository was not edited. No builds, tests, syntax checks or routes
were run. Integration belongs to the parent after the 16:00 batch.

## Concrete known authored owners

Only the existing selected L3S1 inventory and its original moving-group section
were read. The exact original archive entry/section offsets and full selected
records are saved in the JSON evidence.

- Trigger12 is enabled (`tail_flag=0`), Use-only (`flags[0]=1`), unlimited
  (`unknown_word=0xffffffff`), cooldown0, contact dwell0 (`values[1]=0`). It
  links key32 in the original `1st_aid01_door` group, member29.
- Trigger20 has those same Use/enabled/unlimited/zero-dwell properties and a
  0.25-second cooldown. It links key7004 in `big_crate_lid`, member21.

Current `rf_auto_trigger_init` maps those flags and cooldown. Runtime open maps
`unknown_word` to the activation limit and `values[1]` to contact dwell
(`src/core/event.c:1798–1803,2048–2060`). `campaign_trigger_contacts` clears
per-frame fired64 before polling. `rf_trigger_fire_sp` increments count,
retains unlimited triggers and rearms only positive cooldown. Consequently,
while eligible contact and unclaimed held Use persist, trigger12 can dispatch
again on the next frame and trigger20 can dispatch after its cooldown. Current
mover dispatch can reactivate an idle settled linked door/lid (independent
review: scene.c:3452–3474 -> level.c:1262–1280). This is source-derived playable
behavior, not an observed runtime reproduction or a promise that every held
frame changes a mover already in motion.

## Original press contract

Previously qualified full evidence:
`/workspace/shared/known-gameplay-next/original/medic-use-rfexe-disassembly.txt`
and `medic-dispatch-speech-rfexe-disassembly.txt` in that directory; interpretation
in `MEDIC-USE-SOURCE-CONTRACT.md` and `MEDIC-DISPATCH-SPEECH-ADDENDUM.md`.

- Default controls43d060 register action2/type0 at43d0a9. Type0 takes query
  43d606, calling51f140 at43d62e/43d645, rather than the held51f220 branch.
- 51f140 reads and clears the per-key counter at51f166–51f179; 51f220 simply
  reads held state at51f224. The newly saved bounded disassembly includes both.
- Input loop430e24 calls43d4f0 and only dispatches true results to4a6210 at
  430e3e. Action2 dispatch4a6493 calls4a4970, then4a29f0.
- 4a2a12 calls selected-entity consumer4a1660. Result1 at4a2a1a jumps to
  return4a2a4f, bypassing trigger fallback4a1970 at4a2a47.
- Fallback4a1b01/4a1b39 calls ordinary trigger walker4c0100(actor,1); it may
  subsequently call linked-trigger4c04e0. Trigger Use flag1 requires that true
  Use argument at4c07fe–4c0816.

Thus an unconsumed Use press can reach the trigger fallback; holding that same
press is not a fresh input action. A selected refusing medic still consumes it.

## Minimal staged change and boundaries

Inside `campaign_actor_trigger_contacts`, initialize a local `trigger_use` from
its already filtered/unconsumed input. Only for a player, prepared campaign
Use dispatch, Use-flagged trigger and nonpositive contact dwell, additionally
require the existing `scene_use_edge.edge`. Both actual owners have dwell0;
`<=0` matches the core no-dwell branch. Use this local value for all three
consumers: optional reach probe, explicit-filter contact and authored contact.

- No new latch or save state. Existing raw release, claimed press, player-owner
  and successful-load reconciliation continue to own input history.
- Ordinary automatic contacts still poll with their original input. NPC visits
  are unchanged. Legacy noncampaign dispatch (`prepared=0`) is unchanged.
- The global `scene_use_trigger_input` remains unchanged. Positive-dwell Use
  triggers retain their existing held-input approximation: core delay clears
  its deadline on a rejected contact, so a blanket global edge filter would
  prevent a positive dwell from completing. No claim is made to close that
  separate unqualified behavior.
- Existing finite-value validation still runs in the core. NaN does not enter
  this comparison branch and still fails that guard; no error guard is skipped.
- Airlock admission only receives the newly eligible press as before. Already
  accepted `scene_airlock_tick` service remains independent and is untouched.
  No delayed action is cancelled when the input edge ends.

Independent reviewer `/root/review_npc_use_response` reported no source-review
blocker. Compilation and actual gameplay remain unverified.
