# Fragment translational push admission

This is a port-policy foundation, not a recovered original mover algorithm and not yet connected to gameplay. It shares the existing mesh clearance implementation with fragment overlap recovery. No body or mover pose is published by the new query.

For proposed mover translation D, sweep a frozen-orientation fragment from P+D to P against the mover at its proposed endpoint. If the first contact fraction is t and its outward normal is N, the required fragment displacement is dot(D,N)*(1-t). Sweep that displacement against the finite world and other enabled movers, excluding the pushing mover. Given available clearance c, admit t+c/dot(D,N), or the entire translation when clearance suffices. Nonapproaching motion is unrestricted. The existing clearance margin is 0.0001 world units.

The collection query visits alive pieces in every retained source registry and returns the minimum admissible fraction. It performs no allocations, leaves body/mover state unchanged, and publishes its result only after every query succeeds. Angular motion, evolving fragment orientation, initial interpenetration and simultaneous moving obstacles are not established by these tests.

## Evidence

- All 126 PC tests pass. Scene controls cover free motion, a blocking finite ceiling, a disabled or distant ceiling, motion away from the body, malformed geometry and NaN rejection with unchanged output.
- Two retained source registries prove the later, larger fragment can restrict movement: admitted fraction 0.2999 versus 0.7999 when that source is absent. Both bodies remain unchanged.
- The 600-frame tall-lift replay matches `artifacts/xemu/render-20260918-061753/pc-reference.txt` for FRAGMENT_PLATFORM, DETACHED_MOTION and DETACHED_POSE. The final framebuffer is byte-identical. New replay outputs: `artifacts/fragment-push-replay/`.
- NXDK compile/link/package passes after declaring the presently unused collection helper inline. No new XEMU runtime acceptance is claimed for the preflight helper.
- Optional fragment-inner-work profiler patch applies cleanly and remains disabled.

## Required integration

`campaign_controller_tick` advances translation phase/distance in tick_begin; tick_move can change the key, and arrival handling fires linked events and finalizes timing before campaign_controller_commit publishes attached poses. Admission must therefore stage controller progress before these side effects. Rejecting or shortening only a published mover pose afterward would desynchronize controller state, attached objects and arrival events.

Stage each affected group's proposal, calculate final attached translations consistently for shared mover memberships, and admit the resulting motion before publishing controller state or firing arrival callbacks. Define blocked-time behavior and validate retry, arrival, multiple attached objects and multiple controllers. The current query uses other movers' current poses; independent per-controller admission is insufficient for simultaneous motion. Rotation needs its own sweep policy.

The existing tall-lift fixture still passes through the trapped fragment and still has 18 contact-limited frames. This foundation does not fix that visible behavior or increase the completion estimate.

## Live integration experiments: rejected

Two 600-frame PC experiments connected the query to the generated DEV platform's proposed translation and derived surface velocity from admitted displacement. Both failed to stop the trapped-fragment overlap. The experimental hook was removed; production source and the PC executable were restored to the accepted implementation. Authored controller arrival/events were never changed.

The first experiment retried the absolute prescribed endpoint and ended with 24 contact-limited frames. A second control capped each attempted upward displacement at 0.025 units, eliminating catch-up speed as a sufficient explanation. It still ended with 18 contact-limited frames and the platform at Y=2.04999995, its full prescribed endpoint. Peak fragment speeds were approximately 1.54285 in both cases. Energy remains bounded; overlap remains unresolved.

In the bounded control, FRAGMENT_PUSH 456 admits only 0.001334441 of the attempted 0.025-unit movement; FRAGMENT_PUSH 457 admits the full next step. Crucially, DETACHED_STEP_TRACE 456 is emitted before FRAGMENT_PUSH 456, already reports ten contacts, a limited step and 0.000902777 seconds remaining. The subsequent fragment step reports ten contacts with the entire 1/60 second remaining and unchanged position. These labels use the audit counter at different points in the frame; do not interpret them as identical sampling instants.

This evidence disproves the fixed-orientation clearance query as a sufficient live crush policy. It does not isolate a single geometric root cause: inspect the existing contact/overlap state and the fragment's dynamic pose progression before integrating it into authored controllers. A smaller controller step alone is not demonstrated to solve it. Do not convert a missed sweep into unconditional admission when pre-existing overlap is unresolved.

Local reproduction evidence is under `artifacts/fragment-push-live/`: replay.log/final.ppm, bounded.log/bounded.ppm, comparison.json and bounded-experiment.patch (against commit 4dbffe5d). The patch is intentionally excluded from production. No screenshot acceptance or XEMU run is claimed for either rejected experiment. After restoring source, the PC replay target rebuild and scene_detached_sources test pass.

## Separating static contact fix and bounded follow-up

Opt-in DETACHED_CONTACT_STATE tracing identified the first exhausted step: after a ceiling impulse, the static sphere query repeatedly returns fraction zero despite the full mesh separating. Start penetration is about 4.32e-8 units; end clearance is 0.0002067. The port now rejects this near-touching static-world sphere candidate only when minimum mesh clearance increases monotonically over the same four pose intervals used by corner sweeps. Penetration deeper than 1e-5 remains admitted. Subsequent mesh and mover queries still execute with an unrestricted limit, rather than declaring the whole path clear. Moving surfaces are excluded from this static filter.

Focused controls cover separating, inward, stationary, genuinely penetrating and rotation-through-plane motion. The formerly exhausted step456 finishes in four iterations/three contacts. All126 PC tests and ordinary lift/tip checks passed before the user's proportional-testing rule; no further broad runs are needed. Native render-20260918-071325 passes78 checks at600 frames on stock64MiB, including exact PC/platform audit state. Peak speed is1.542848 and17 limited frames remain. Native framebuffer inspection still shows the gray platform above/through trapped rubble, so this is not crush completion. The harness restored the disc and exited.

Per the user's engine-first direction, further confined-platform crush work is deferred in TO-DO while missing core gameplay is integrated. The read-only push admission helper remains disconnected from gameplay. Percentage estimates remain unchanged.
