# Crater appearance after blast-effect decay

`python tools/check_crater_effect_decay.py` replays the ordinary two-shot
`glass_house.rfl` route at 550 frames and appends 350 neutral frames for a
900-frame comparison. All fixture/replay environment overrides are cleared.
No original game or desktop interaction is used.

Both actual PC endpoints were inspected sequentially: smoke obscures the center
at 550; at 900 it is gone and the crater remains dark and visually ambiguous.
The final camera words, complete material bytes, physical geometry and full
atlas CSV are identical. Particle telemetry reports five active at 550 and
zero at 900. Thus this view's persistent appearance is not explained by a
lingering particle overlay or delayed atlas refresh. This is not a proof of
original visual parity or of a particular missing shading term.

The 71 used maps contain 4,277 texels with mean normalized RGB
(0.241811, 0.241811, 0.241811). These are unweighted atlas statistics, including
padding; they are not framebuffer brightness or a reason to add a multiplier.
Existing binary evidence supports noise lighting and two-times modulation.

Evidence: `artifacts/crater-effect-decay/report.json` and the two numbered
subdirectories. Replay SHA-256:
`0ee89c4d7d321f8a7e2dba2a898f08683b6fd2652461bac9597d309944f0fcbb`.
PC binary SHA-256:
`0a9d94150719e54997b5f4d53d08965603224207fc9ec39744ea2cfadd90b38e`.

The harness records a failure if any persistent surface or camera changes.
This pass changes diagnostic tooling only; no new Xbox build or visual-fidelity
acceptance is claimed. Next investigation should target persistent surface
projection/material/lightmap behavior, not transient blast effects. No images
were uploaded to GitHub.
