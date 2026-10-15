# Ordinary NPC Use facing and patrol pause

Update15:08 UTC: compiled in670bdf36 and included in the passing Actual15
ordinary startup check. Broader interaction/audio/save functionality remains
tester-owned; earlier source-only wording below is historical.

Source-written October15; compilation and tester functional coverage pending.

Original409950 resets retained primary/secondary AI motion (4091d0/409210),
selects the player (409050), resolves an aiming point (401110), and requests
ordinary angular steering (40cb50). Wandering4 becomesWaiting2 until speech
length+500ms; original waiting service repeatedly faces its retained target.

The port retains action and route ownership rather than temporarily publishing
4-to-2: existing patrol/save joins require those fields to stay consistent.
A finite optional sidecar interval continuously turns to the current player eye
minus0.1Y (crouched speaker uses target origin) and requests stand, after normal support
and falling work. Ordinary actor-default patrol translation pauses, then resumes
from its unchanged route cursor. Authored Goto/follow routes are not intercepted.
Missing/disabled speech retains a500ms interval; audio success is not authority.

Admission protects scripts, queued shots, combat, medic/alarm/flight/noise,
AI suppression, links, active pain and frozen poses. Accepted stronger ownership,
death, identity change, player attachment/replacement, expiry or scripted speech
replacement cancels the interval. Failed optional turn/reset work does not fail
Use or campaign operation. No script_look/combat_target/death fields are borrowed.

The optional presentation sidecar is now24 bytes/actor, including chatter timers,
immutable switches, actor/player handles and Use expiry. It is admitted only after
mandatory body allocations inside the unchanged640-KiB cap; allocation absence
preserves speech-only behavior. Body/checkpoint layouts and source-size allowances
are unchanged. Successful world publication resets transient interaction state;
failed loads preserve it. No exact original save-continuation claim is made.

Speech gestures are deliberately not started here. Original4299b0/421720 arms
authored speak41/speak_short42, but those nonloop clips need explicit residual
checkpoint ownership. The existing save guards are not weakened to admit them.
