# Final nuke defusal first pass

L20S3's authored trigger UID 18306 activates `Defuse_Nuke` event UID 18307.
The shared event dispatcher now forwards event type 86 ON to a level-owned
puzzle; OFF has no special effect. The player enters an 11-direction sequence
in two phases (four, then seven), with a difficulty-based countdown. The Xbox
D-pad supplies directions while the puzzle is active, and successful completion
reaches the existing credits terminal state. Ordinary quick-save is rejected
while the puzzle is active because its transient state is not serialized.

The trigger/event join, two-phase 11-symbol structure, and initial time limits
of 62.34, 46.77, 41.03 and 30.26 seconds come from the original-binary cases
in `research/secondary-re/campaign-defuse-nuke-20260916.md`. This port's
deterministic sequence seed (`event UID ^ 0x5a17c0de`), edge handling, mistake
reset, text HUD, and immediate credits handoff are first-pass policies. They
do not claim original RNG ownership, artwork, fonts, audio, animation, media,
modal-stack lifecycle or exact ending timing. The port now holds player physics,
events, NPC updates and particles during the active modal puzzle while still
presenting the level. On timeout it sends an unforced 1000-damage hit through
the ordinary player-damage owner; fatal hits enter the existing death/restart
flow. This follows original `43b800`/`4a4dd0` outcome evidence, though its
rising/falling fade and return to gameplay after a survivable hit are not yet
presented like the original. A correct final input takes priority over a
timeout on that frame, matching the completion-first outcome gate.

`python tools/xemu_defuse_campaign.py` stages the authored trigger and replays
the 11 directions inside the guest process. The 140-frame stock-64-MiB run in
`artifacts/xemu/defuse-l20s3-20260929-155332/` passed: one puzzle opening,
progress 11, completion true, no errors, credits state 2, and 9,449 free
physical pages (36.91 MiB). The isolated test disc flags were restored. This
is a bounded functional check, not a campaign playthrough. No visual-content
inspection was performed under the current no-images instruction.

The follow-up 140-frame stock-64-MiB replay in
`artifacts/xemu/defuse-l20s3-20260929-155811/` passed with 50 paused world
frames and 89 event ticks, then reached credits with the same 9,449 free
pages. This establishes the bounded modal simulation hold and completion
path; it does not prove every renderer, audio or timeout behavior.

The process-local `--timeout` variant shortens only the guest fixture timer to
0.25 seconds. Its 80-frame stock-64-MiB run in
`artifacts/xemu/defuse-l20s3-timeout-20260929-160426/` passed with one
1000-damage request, health `-800`, one player death, no named endgame request,
and 9,433 free pages. The normal 140-frame success replay was repeated after
the timing change in `artifacts/xemu/defuse-l20s3-20260929-160512/` and still
reached credits with 9,449 free pages. Neither run visually inspects the HUD
under the current no-images instruction.
