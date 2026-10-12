# Bounded Xbox frontend receipts

Source-only diagnostic successor to actual06, October 12, 2026. Actual06 remains
CHECK_FAILED: first New Game and End Session retirement/reset passed, but the
second New Game did not become accepted within its bound. The existing samples
do not establish whether the return-menu draw, input poll or input-edge path was
responsible. No behavioral correction is claimed here.

This change adds 36 bytes of diagnostic words to the existing Xbox frontend and
input provider. It changes no input mapping, edge/rearm ownership, renderer
waits, timeout, save/load gate, world allocation or gameplay behavior. There is
no new polling loop, fixture or framework. No builds or runtime were performed.

## Symbols and exact semantics

rf_xbox_frontend_trace is eight volatile uint32 words. It is reset at each
player_frontend entry before the existing frontend-state field is published.

0. Number of calls to rf_xbox_input_poll that have returned in this entry.
1. Completed connected, fully neutral/rearmed polls after the latest successful
   frontend draw. This count resets to zero on EVERY draw entry, so the poll
   before the first draw or before a redraw never establishes readiness.
2. Packed input observation copied after the latest returned poll.
3. Latest returned input-poll status, using the existing RF result encoding.
4. Draw-call entries, including ordinary menus and the loading screen.
5. Draw-call returns, including failed returns. This alone is not success.
6. Latest returned draw status, using the existing RF result encoding.
7. Current boundary: 1 input open; 2 input poll; 3 ordinary menu draw; 4 sleep;
   5 loading draw; 6 input close; 7 frontend returned. Zero means no boundary yet.

rf_xbox_input_menu_observation is one volatile uint32 word. The input poll clears
it before validation and samples it from the same controller state as gameplay.
It uses no extra USB/event pump. Bits:

- Bit0: raw A held
- Bit1: raw Start held
- Bit2: raw Back held
- Bit3: a connected controller was sampled
- Bit4: every mapped action button/trigger, Start and Back is released; menu,
  action and exit-chord rearm latches are clear; no disconnect occurred this poll
- Bits8..11: the existing menu-edge mask, shifted eight bits (Up1, Down2,
  Accept4, Cancel8)

A completed ready-neutral sample is exactly 0x18. Missing controllers, a fresh
replacement/disconnect and exit-chord early returns never set bit4. Raw A is a
read-only SDL getter before the exit-chord return, so it remains observable even
when that chord takes priority. Returned frontend trace word2 is the useful
latched sample; the input symbol can be incomplete while phase2 is active.

## Readiness protocol for the existing bounded observation

Keep the process-local controller neutral. Require the existing frontend
start/end counters for this entry, phase4, a positive completed-poll count,
a positive post-draw neutral count, observation0x18, input status0, at least
one draw, matching draw-entry/return counts and draw status0. Then require a
later completed neutral poll with advancing poll/neutral counts under the same
draw pair. A stopped-CPU readiness snapshot must independently satisfy those
conditions before issuing the normal A press.

The words are individual observations, not an atomic multiword transaction.
Phase4 is written only after this iteration's poll, optional draw and decisions
have finished. Recheck the readiness predicates in the stopped snapshot; do not
use a previous neutral value while phase1/2/3/5/6 indicates an unfinished call.

## What the next normal batch can distinguish

- Phase2 and no new returned polls: the input provider has not returned.
- Phase3/5 with draw entries ahead of returns: a menu/loading draw is unfinished.
- Advancing polls with raw A absent: no A reached the sampled SDL state.
- Advancing polls with raw A present but no Accept edge: held/edge state differs
  from the requested fresh press; this does not identify the lower-level cause.
- Accepted A followed by phase5: selection reached loading presentation.
- Returned error status: the existing failure path, not a timeout diagnosis.

These receipts narrow the boundary. They do not prove an SDK, GPU or controller
bug by themselves. Preserve actual06 files/helpers and their result unchanged;
only the parent-owned normal stock64 batch may execute a successor observation.
