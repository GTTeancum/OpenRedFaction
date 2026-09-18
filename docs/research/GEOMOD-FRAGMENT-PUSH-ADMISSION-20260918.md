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
