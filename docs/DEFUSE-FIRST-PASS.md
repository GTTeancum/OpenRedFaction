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
modal-stack lifecycle or exact ending timing. The world is not yet paused
during the modal puzzle, and timeout presentation is not verified.

`python tools/xemu_defuse_campaign.py` stages the authored trigger and replays
the 11 directions inside the guest process. The 140-frame stock-64-MiB run in
`artifacts/xemu/defuse-l20s3-20260929-155332/` passed: one puzzle opening,
progress 11, completion true, no errors, credits state 2, and 9,449 free
physical pages (36.91 MiB). The isolated test disc flags were restored. This
is a bounded functional check, not a campaign playthrough. No visual-content
inspection was performed under the current no-images instruction.
