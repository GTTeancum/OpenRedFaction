# Selected turn effects

## Assembled candidate helper

`rf_turn_update` assembles helper 0x41f9f0 around an explicit reset callback.
Its early exits preserve the original order: nonzero byte global 6fc4d8 selects
3/5; movement modes 12,15,13,11,9 with info +724 bit 0x20000 select 1/1;
active action 20 or 19 selects 9/9. These exits do not alter movement settings,
timers or playback. Otherwise it evaluates direction and repeats action checks
before invoking reset when both action mappings 20/19 differ from -1.

The callback owns original 0x41ae70 behavior and may change actor, playback or
context through its user data. Missing a required callback returns RF_NOT_FOUND;
the implementation does not silently skip the reset. Direction/local X is
captured before reset, while target validity, source/target positions and trigger
bytes are read afterward. Callback mutations cannot be rolled back by the core.
Info flags in actor and movement configuration must describe the same entity.

The post-reset distance gate subtracts positions +7d4 and +6fc with float stores
and evaluates magnitude using x87 precision against binary32 8.2 at 0x58956c.
A nonzero target-valid byte +6f8, distance strictly greater than that threshold,
and either trigger byte +53c/+53d select the integrated action-19/20 effects.
Other outcomes continue through the remaining candidate branches.

`tools/verify_turn_update.py` executes the complete original function with all
callees unchanged for 3,004 fixtures. The weapon-entry argument is -1, so the
real original reset returns without changing the entity; the C callback counts
that operation. Sound classes are -1. Both implementations match complete
playback/reference state, candidates, movement fields, deadlines, turn flag and
reset-call count. All six outcomes are observed: early 3/5, early 1/1, active
9/9, selected turn, secondary turn and fallback. Four targeted distance cases
include the 8.2 threshold and small orthogonal components. A C-only check rejects
a missing required reset callback without changing outputs.

Report: `artifacts/turn-update-verification.json`. This establishes the combined
control flow for that fixture domain, not populated reset behavior or audio
playback. Actual reset/audio adapters, actor initialization and Xbox runtime
integration are still required. Sound requests remain deferred as described
below, so synchronous audio ordering also remains integration work.

## Remaining candidate branches

`rf_turn_finish_candidates` reconstructs 0x41fc84 onward after the selected-turn
branch has not been taken. An eligible direction with mappings 17 and 18 both
different from -1 first applies movement request zero and sets candidates 3/5.
It tests action 18, then action 17 only if needed, through the remaining-time
predicate. Either active action suppresses restart, including a zero-weight
slot whose cursor is before the end. Otherwise positive local X starts action
18 and zero/negative X starts 17, with weight one, freeze zero and sound one.
Existing turn flags and deadlines are not changed by these branches.

When that branch is unavailable, it applies movement request one. Candidates
become 2/4 only if entity +2a4 equals global 0x872114, entity +554 equals one,
and both byte globals 0x6fc4d8 and 0x64ecb9 are zero. Otherwise they are 3/5.
The two global bytes are kept explicit; their full mode semantics remain open.

`tools/verify_turn_finish.py` compares 4,800 original executions through
unmodified movement, action-activity, action-start and absent-sound callees.
All playback fields, resource references, candidates, movement settings,
deadlines and turn flags match. Evidence: `artifacts/turn-finish-verification.json`.
Sound classes are -1; valid audio playback is not covered. The preceding reset
call and the decision that chooses the selected-turn branch remain unrecovered.

## Direction gate

`rf_turn_direction` reconstructs the direction gate starting at 0x41fa7c.
Info +728 bit 4 disables it; entity +588 must be positive and +7d0 bit 8 set.
It copies entity vector +7a0 and normalizes the copy through the behavior of
0x4fab30. A magnitude below binary32 .1 disables the turn; equality is accepted.
The original normalizes even a zero vector, producing unused invalid values
before rejecting it. C avoids that unnecessary division and returns ineligible.

The dot product uses the normalized copy and the third basis row at entity
+60, with terms accumulated Z, Y, X by 0x40a0b0. Values outside [-.5,.5] reject
the turn. The comparison uses x87 extended precision; a double rewrite accepted
boundary cases that the original rejected, including a vector (1,1e-18,0)
against the third basis (-.5,-.8660254,0). The shared comparison now retains the
original x87 result for both tests and restores the caller's control word.

Accepted vectors are transformed by 0x4faa30 using the original, unnormalized
vector and the orientation at +48. Each row accumulates Z, Y, X and stores a
float. The returned local X feeds turn-side selection. Ineligible results are
zeroed; invalid non-finite inputs leave outputs unchanged. These calculations
do not mutate an entity or invoke candidate/reset side effects.

`tools/verify_turn_direction.py` matches 5,208 original block executions through
unmodified normalization, dot and transform callees. It varies input flags,
counts, vectors and matrices, adds eight dot-boundary cases and 200 neighboring
float cases around the .1 magnitude threshold. Observation hooks stop before
later decisions. Evidence: `artifacts/turn-direction-verification.json`.
PC and NXDK compilation pass; no direction-gate XEMU runtime claim is made.

## Selected branch integration

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
