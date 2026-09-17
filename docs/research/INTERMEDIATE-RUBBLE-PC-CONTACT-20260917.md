# Natural intermediate rubble contact

The dry developer post can produce an intermediate-size fragment without editing
its geometry or body fields. `tools/find_intermediate_rubble.py` varies the recorded
rocket aim height; height -0.1 produces radius 0.5307253003120422. Heights 0 and
0.1 produce fragments below the original strict 0.5 player-admission cutoff.

`tools/check_intermediate_rubble_admission.py` reuses the successful recording,
walks forward on frames 360–501 and requests a jump on frame 470. The 600-frame
save run and uninterrupted 800-frame control each register 44 fragment contacts,
with sphere-route index UINT32_MAX and zero query error. The final player position
is (-7.695992, -0.618479, 2.526116), beyond the chunk.

Reloading at frame 600 and idling for 200 frames yields a byte-identical RFCP to
the uninterrupted control. The idle continuation has no new contacts, as expected
after crossing the chunk. The chunk remains present and unretired in all runs.

Evidence: `artifacts/geomod-postedit-re/intermediate-search/report.json` and
`artifacts/geomod-postedit-re/intermediate-rubble-admission/report.json`, recordings,
logs and RFCP files. These are small PC outputs; no emulator disk images were made.

This is numerical acceptance of natural extraction, sphere-route player contact
and post-crossing continuation. It does not establish saved standing support,
visual or animation quality, audible behavior, Xbox acceptance, or the full
original two-object response scheduler. Those remain separate work.

## Standing failure

`tools/check_intermediate_rubble_support.py` is currently a failing reproduction,
not an acceptance test. It stops forward input after frame470, retaining the
ordinary jump at470. At frame600 the player is (-4.85561943, 0.0602072142, 2.5).
The ground support check passes, but the subsequent rubble placement check
rejects saving with RF_NOT_FOUND (-3). An independent world-space sphere-distance
diagnostic measures clearance -0.424338056 between player sphere0 and fragment
sphere1. This greatly exceeds the 0.002 placement tolerance. Without requesting
a checkpoint, the replay completes with327 contacts and the same final position.

Do not relax save validation to accept this state. Investigate how the original
ground-query slope gate and the live movement/response scheduler interact: the
current source-contact adapter permits a penetrated standing state. The exact
cause is not yet isolated. Numerical logs are in
`artifacts/geomod-postedit-re/intermediate-rubble-support/`; no visual acceptance
is claimed. The newly added rejection diagnostic runs only when placement fails.

## Hit-presence fix

The player movement callers incorrectly interpreted source sphere UINT32_MAX as
no hit. Original actor-pair response produces a valid fraction and normal without
identifying the source sphere; the registry intentionally preserves that unknown
index. The callers now use fraction < 1 for hit presence, including locomotion,
stance clearance, headroom and support motion checks. No collision radius or save
tolerance was changed.

The previous contact-count-only acceptance did not prove collision response.
`tools/check_intermediate_rubble_blocking.py` now verifies ordinary forward walking
actually stops before the chunk: final position (-4.112841,-0.401362,2.495630), one
sphere-route contact, valid save, and byte-identical idle continuation versus an
uninterrupted800-frame run. The PC endpoint capture was inspected: environment,
post, weapon and HUD are present; an endpoint image does not verify motion quality.
Xbox compilation and121 existing tests pass; native behavior remains unverified.

The jump reproduction now takes a different path, with final position
(-2.343695,-0.392197,4.056082), and fails world placement (sphere1, reason2), before
the rubble-placement check. The former0.424338 overlap is historical pre-fix
evidence. Saved standing acceptance remains open; do not report the old crossing
or standing scripts as current acceptance without rerunning them.

## Open-boundary classification correction

The post-jump placement trace actually selected no face for player sphere1:
center(-2.3436954,0.0276969671,4.05608225), ray endpoint
(52.4216003,242.570068,-0.202106953). It was not a detected back-face or surface
overlap. The port checkpoint policy now retries inconclusive no-boundary rays
within its existing16-direction budget. Real back-face hits still reject
immediately, all-miss exhaustion rejects as ambiguous, and centers outside the
eligible world bounds reject. This is port save validation, not a claim that the
original4e3800 routine retries missing boundaries. Tests cover an open ceiling,
outside-world rejection and solid child geometry; all121 tests pass and NXDK builds.

The jump replay now saves and reloads, but its strict continuation comparison
still fails: only RFCP float words64(X) and72(Z) differ. Saved/reloaded position
is(-2.3436954021453857,-0.392197,4.056082248687744); uninterrupted800-frame position
is(-2.343688488006592,-0.392197,4.056087493896484). The capture scope accepts
velocity below.001, while restore resets velocity to zero. This residual-motion
policy mismatch remains open; no equality tolerance was added and no standing-on-
rubble claim is made. Optional RF_CHECKPOINT_PLACEMENT_TRACE logs classification
evidence on PC without altering validation outcomes.

## Residual velocity and replay boundary fixed

RFPLv2 stores finite residual standing velocity in the former reserved bytes68–79,
within the existing544-byte section and existing per-axis.001 admission bound.
OldRFPLv1 decodes to zero velocity, and the encoder retains v1 for bitwise zero
velocity. Capture and restore preserve the residual vector; ordinary restored
locomotion initialization no longer clears it. Malformed/nonfinite/out-of-scope
velocity rejects without publishing decoded state. This is a port format extension.

The replay renders before stepping and omits the step after its final frame.
A600-frame save therefore contains599 steps and has not consumed input599.
The resumed recording now starts with input599 and renders201 frames, executing
the200 steps needed to reach the800-frame uninterrupted control. Its full RFCP
is byte-identical, including the residual velocity. The former zero-velocity
fixtures hid this one-step test error; other continuation fixtures need auditing
before interpreting their transient-state equivalence.

Current PC acceptance: jump collision, landing beside rubble, successful save/load,
and exact continuation. Saved standing on top of rubble and nativeRFPLv2 acceptance
remain open. No tolerance was added to the byte comparison.

## Natural saved standing now verified on PC

`tools/find_rubble_landing.py` varied only ordinary jump timing and forward input.
Jump415 with forward input360–449 lands on the original extracted radius0.530725
chunk. Later jumps420/425 did not pass placement validation; broader approach
coverage remains open.

`tools/check_intermediate_rubble_standing.py` verifies the complete positive case:

- Saved player position(-4.656934,0.464791,2.634441),270 fragment contacts by frame600.
- Resumed idle remains at that position with402 new support contacts; uninterrupted
  frame800 has670 contacts and a byte-identical final RFCP.
- Walking backwards on resumed frames20–99 leaves the chunk, ending at
  (2.019286,-0.368479,2.634441). Its RFCP matches the uninterrupted walk-away control.
- Retiring the sole supporting fragment in a copy of the saved RFCP causes load
  rejection. The positive standing state therefore depends on fragment support.

The resumed recording begins with the unconsumed input599; the continuous
walk-away recording excludes the saved replay's unconsumed final input before
appending the resumed recording. Neither comparison drops or duplicates a step.

Evidence is in `artifacts/geomod-postedit-re/intermediate-rubble-standing/`.
The PC endpoint image was inspected and shows the elevated first-person view,
environment, weapon and HUD. This is not a frame-by-frame animation assessment.
Xbox runtime, broader surfaces and larger chunks still require validation.
