# Settled crater lighting audit

The saved live atlas is neither uninitialized nor missing its recovered base
noise in the inspected fixed16 and swept14 fixtures. No brightness adjustment
is justified by this result. GPU texture identity, sampling and final visual
parity remain unproved by this CPU-side audit.

`tools/audit_geomod_idle_lighting.py` independently reconstructs each map from
its recorded seed using CRT recurrence214013/2531011, original fill4e5bb0
channel32+rand%64 and no-brightening1555 packing. It compares every byte,
including alpha and all color channels, and rejects overlapping allocations.
The input is the actual live atlas dump, not a freshly regenerated substitute.
Use this only for settled fixtures: active dynamic lights can legitimately
produce DIFFERENT. Each JSON records the input SHA256.

| Fixture | Maps | Texels compared | Referenced maps | Referenced faces | Differences |
|---|---:|---:|---:|---:|---:|
| geomod-9k-live |1267|40161|1126|1486|0|
| geomod-projected-sweep |1086|41687|777|1128|0|

Reports are retained as `idle-lighting.json` within those artifact directories.
Unused historical charts are included in byte comparison but excluded from
the reported referenced-chart histogram. Histogram weights texels across whole
referenced charts, not visible pixels, physical surface area or face count.

Referenced charts contain channel codes4..11. Normalized5-bit GPU sampling
followed by the world shader's factor2 yields multipliers0.2580645..0.7096774,
with means0.48452365 fixed /0.48439904 swept, before base texture and any vertex
color. This is a mathematical shader-input estimate, not framebuffer luminance;
PC integer decoding/filtering can introduce further rounding. Source checks:
`src/platform/xbox/preview_fragment.ps.cg`, `tools/pc_raster.c` and
`src/diagnostic/scene.c::scene_terrain_dynamic_lighting`.

The current settled path contains no persistent directional contribution in
these saved charts. Identical random-light ranges across surface orientations
offer little large-scale shape shading. This explains a possible readability
limitation but does not prove a missing original contribution: the recovered
original new-face initialization also uses noise, and later original lifecycle
must be established before changing the shading model.

Validation executed:

- Both complete saved-atlas audits pass:81848 texels total.
- `python tools/inspect_geomod_light_noise.py`:64 original/shared fills pass,
  including strided guards. That probe supplies CRT draws; the separate prior
  actual-CRT probe is described in GEOMOD-ORIGINAL-FILE-SAVE-NOISE-20260916.md.
- `python tests/test_geomod_idle_lighting.py`:four tests pass, including
  corrupted green/alpha, truncation and overlapping-chart negative controls.

Next actionable boundary: capture the actual base-material bytes and binding
used by the reconstructed renderer, then evaluate sampling/mip policy and the
original later lighting lifecycle. No emulator launched, screenshots uploaded,
or game rendering behavior changed in this audit.
