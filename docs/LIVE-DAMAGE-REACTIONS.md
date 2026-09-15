# Live NPC damage feedback

Live damage now dispatches retained NPC pain animation and sound. Previously,
combat_notify accepted only retaliation events, although rf_entity_damage_effects
already emitted pain-animation and pain-sound notifications. Only the diagnostic
fixture called the retained pain wrapper.

The callback now uses rf_scene_npc_pain_retained (reconstructed428740) and
rf_scene_npc_pain_sound with simulation milliseconds and a separate deterministic
RNG. Existing cooldown, AI/action and animation eligibility gates remain intact.
Each backend carries an error status; callback failures propagate to the scene
caller. Player hits, enemy-to-NPC hits and scripted damage use this context.
Live player pain is not routed through NPC owner functions.

The first run exposed RF_NOT_FOUND on lethal-hit sound dispatch: the retained
sound wrapper supports living NPCs and has no death-class override. The callback
now skips pain when NPC health is nonpositive, leaving death entry and its
presentation to handle fatal hits. Other errors emit COMBAT_PAIN_ERROR and
propagate. This explicit lifecycle split is practical port behavior, not a claim
that every original death-sound branch has been rebuilt.

The live attack loop now honors the retained animation lock before firing or
melee strikes. This is a practical integration of428740's existing lock, not
a claim that the original complete attack scheduler has been reconstructed.
No new fixed stun duration or damage multiplier is added. Navigation and
targeting continue; the pending attack deadline is preserved, so expiry can
resume immediately instead of adding another cooldown. Actors without a
pending pain lock continue normally.

## Verification

`tools/replay_live_pain.py` runs a staged L4S5 rifle pickup/encounter, captures
ordinary look input, then replays it without tracking. Three nonfatal hit
notifications across actors3270 and3305 produce two animation starts and two
sound dispatches; a later fatal hit uses death entry without an error. The
repeated hit on3305 retains the pain cooldown. Ten shots, four hits and one kill
match between tracking and recorded playback. COMBAT_PAIN exposes eight words
for animation/sound requests, starts/plays, target, clock, RNG and status.

Stock64MiB Xbox run `artifacts/xemu/render-20260915-035830` passes all34
PC/native comparisons for the120-frame recording, including exact pain state
[3,3,2,2,6815847,1200,3884216597,0]. Endpoint free memory is6309pages
(24.64453125MiB). All18 staged disc entries restore and the owned emulator exits.
Counters verify sound dispatch; this does not claim a listening check.

The3600-frame normal-input opening route also completes on PC, with11pain
requests,6animation starts and2sound dispatches. Its9shots,4hits,1kill and player
health match the prior route. Local evidence: artifacts/live-pain-entry-fixed.
`tools/verify_pain_reaction.py` passes2048 original/PC/NXDK orchestration cases
(871accepted). These checks do not establish later campaign completion or attack
interruption during flinches. The comparison tools for existing bindings remain
useful, but their older fixture results alone did not prove live dispatch.

## Attack-lock integration

`tools/replay_pain_attack_gate.py` extends the recorded rifle encounter to240
frames. Injured actor3270's1717ms lock blocks14due checks through1716ms; it
fires on frame104 (1733ms), then164 and224. Uninjured actor3271 fires at66,
126 and186. Thus the injured actor resumes on the first eligible frame and
both retain60-frame cadence; other actors are not globally paused.
Opt-in ENEMY_SHOT_TRACE logs attempts, including misses, so the check does
not mistake missing damage events for absent shots.

Stock64MiB Xbox artifact `artifacts/xemu/render-20260915-040603` passes all35
PC/native comparisons, including PAIN_ATTACK_GATE [20,14,3270,1717,1716,0].
Endpoint free memory is6261pages (24.45703125MiB). All18 staged disc entries
restore and the owned emulator exits. Exact attempt timestamps above are PC
trace evidence; native verification compares the exported states/counters.

The7356-frame PC route now clears the first L2S3 guard with25health instead
of5. The8402-frame tracked continuation clears guard2047 in maintenance with
15health,14shots,8hits and2L2S3 kills. Tracking-free playback matches all77
body words, NPC state, combat journal, pain/gate telemetry and final position;
miner2061 remains at100health. The152 captured look records reproduce exactly.
`tools/replay_l2s3_maintenance.py --verify-existing` validates the generator
and completed logs; omit the option to rerun capture and playback. The full
maintenance continuation has not yet been verified on Xbox.
