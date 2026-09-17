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
