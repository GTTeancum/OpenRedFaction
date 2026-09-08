# Selected turn effects

`rf_turn_apply_selected` in src/core/turn.c combines the reconstructed action,
timer and movement components for original block 0x41fbdc..0x41fc83. It begins
after the helper has selected this branch; the preceding tests are not included.

The block sets both movement candidates to logical 9, chooses action 20 for
positive local X and 19 otherwise, and starts it with weight one, freeze zero
and sound flag one. It then sets entity +7bc to one, sets deadlines +79c/+4d0/
+4d4/+744/+798 to game time plus 1,200 ms, and calls movement setter 0x427450
with request one. Signed zero chooses action 19. Missing motion mappings do
not suppress the later flag, timers, candidate or movement updates.

The C API stages numeric effects and validates them before mutating playback
resource references; errors preserve all outputs. It returns a sound-class
request for the caller to resolve/play. Unlike the original synchronous audio
call before timer/movement writes, that returned request is consumed afterward;
integration must preserve required audio ordering once that subsystem exists.
No heap allocation is used.

`tools/verify_turn_effects.py` matches 2,400 original executions of this block
with unmodified action-start, absent-sound resolver, timer and movement callees.
It compares all playback fields, resource references, both candidates, turn
flag, five deadlines and three movement settings. Cases vary local X, mapping
presence, initial slots, loop/freeze state, forced movement actions, configuration
flags and clock wrap boundaries. Sound classes are -1, so no valid sound is
played in this verification. Two C-only rejection cases verify unchanged state.
Local report: `artifacts/turn-effects-verification.json`.

Routine 0x41ae70, called earlier by the candidate helper, must not be treated as
an aim refresh. It validates a [0,63] entry index, resolves the entity handle,
clears a byte at entity +46c+index and flag +7d0 bit 0x2000, and can reach
sound/model/linked-entity operations. Its full semantics and side effects remain
unreconstructed. Complete 0x41f9f0 decision logic, its other turn branches,
valid-sound playback and runtime integration remain open.
