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
