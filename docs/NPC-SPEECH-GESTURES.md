# NPC speech gestures: integrated and compiled

Current status: October 15 at 16:07 UTC. Full enabled speech integration passed
native stock64 compile/link/package and bounded ordinary startup (12 simulation,
10 presentation frames) on0d5149c1. See [Actual16](HOURLY-20261015-1600.md).
Functional gestures and save/load coverage remain tester-owned. Earlier entries
below record historical development stages.

Historical October 15, 2026, 15:35 UTC: staged RFNC42 codec is source-written and independently
reviewed, not compiled. No speech gesture is enabled and existing scene callers
still use the previous APIs. Actual15 does not validate these later changes.

Original actor voice entry4299b0 arms speech duration/next-gesture fields1408/140c;
421720 selects fixed class-base action41 for a sufficiently long remaining line,
or42 above three quarters of its duration. Humming is audio-only. The implementation
must not borrow death or script-animation ownership for these nonlooping clips.

## Staged wire contract

RFNC42 appends one four-byte speech-motion mask after each RFNC41 phase word.
Bits0/1 designate fixed class-base41/42; other bits reject. The speech-aware API
emits41 for all-zero masks and42 only when a nonzero mask exists. Legacy decoding
supplies zero. Older writers reject nonzero masks and older readers reject42.
No audio playback or voice deadline is serialized.

The structural codec requires a living, nonretired, animated, non-scripted,
unfrozen row with enough slots. It cannot establish identity or resource
ownership: those complete scene joins are still required before enabling.

Decoded records grow by four bytes; restore entries currently grow by eight
because they contain two records. Allocation accounting uses actual sizeof
inside unchanged count, row, whole-save and cumulative load limits. Native
size confirmation is pending the next authorized hourly build.

## Historical remaining integration

- Explicit live residual mask bound to exact actor/pose and immutable class IDs;
  canonicalize aliases, never take over an unowned matching motion ID.
- Capture, candidate preparation and final-close publication of exact slots,
  payloads, metadata and references; keep old live ownership until final close.
- Structural retirement before competing owners, same-ID restarts, model
  transfer and unregister. Retirement must permit already-lethal health;
  capture/restore remain living-only.
- Remove owned slots transactionally. Zero-weight stopping is not retirement.
- Reconcile only physically removed slots; culling/audio expiry cannot erase
  ownership of a still-retained nonloop.
- Optional voice-driven start/service within existing resource limits; Humming
  excluded, healing independent of optional audio/gesture failure.

No new fixture, functional route or save/load run is authorized by this work.
Broader behavior and saved-game coverage remain tester-owned.

## October 15 15:49 UTC integration checkpoint

The scene wiring is now source-written, still uncompiled, with the scheduler
kept disabled while independent integration review finishes:

- Four-byte live mask, exact immutable class-base identity and canonical aliases
- Capture and requested-slot/whole-mask restore proofs; a narrow shared-row
  projection and allocation-free late-close gate
- Composed RFNC42 callers; standalone component publication refuses speech
- Old mask retained through assignment and published only after checked close;
  speech-only loads do not inherit reload/player/vehicle exclusions
- Checked retirement before stronger animation owners, weapon resets/selection,
  model transfer, registry removal and scene teardown
- Physical-removal reconciliation immediately after playback advance even when
  later pose evaluation fails; culling and zero weight retain actual ownership
- Optional simulation-clock voice scheduling with two transient words per
  existing voice, Humming excluded, post-audio-callback identity comparison,
  and successful-load reset of all outgoing speech timers

Actual allocations remain charged within existing budgets. Source size bounds
reflect the live word, decoded record, embedded owner copies and 56-byte shared
publication projection; native size/link confirmation is still pending.
No saved audio/timer replay, arbitrary nonloop admission or body-slot increase
is introduced. Actor eligibility and existing gameplay actions remain separate
from optional gesture success.

## October 15 15:52 UTC: reviewed source enabled for the next build

The full runtime/capture/restore/final-close/lifecycle integration passed
independent source review and is enabled in source. Native compilation and
ordinary startup remain pending; no functional or save/load result is claimed.

Review corrections include audio-poll identity protection, reconciliation before
returning a post-advance pose failure, and whole-model teardown discharge.
Complete model retirement releases every actual physical slot before clearing
its speech receipt. It does not require an abandoned load's intentionally old
mask to describe already-replaced playback. Partial resets and competing starts
still require the exact structural retirement proof; incoming ownership remains
unpublished on failed storage close.
