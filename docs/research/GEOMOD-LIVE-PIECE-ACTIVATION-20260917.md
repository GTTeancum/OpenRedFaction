# Authored-post registry activation and closure correction

The authored-post scene now creates the detached registry with loaded physical
material index1 coefficients (486da0 descriptor), the final terrain renderer
material slot and seed1. Its shared clone/decode/mutate/publish transaction owns
piece staging. The regular scene draw path consumes committed batches; shutdown
closes them. Checkpoint preparation builds a separate replacement registry and
swaps it only after terrain/lighting publication succeeds. Body motion is not
serialized: this currently reconstructs birth poses, and motion must not be
considered save-complete.

Core budget rises from1024 to1152KiB. Detached owner reserves2MiB including its
construction scratch and old/pending allocations. The destruction subsystem
ceiling rises from12 to13MiB: restore actually required12901020 reserved bytes,
which correctly exceeded the old12582912 ceiling. This is not total-game memory
acceptance; stock64MiB native headroom remains to measure. No hardware profile
was enlarged.

## Correction to earlier extraction evidence

The first live replay initially emitted one quad (4 corners/1 face) as a body.
The orientation classifier does not establish closure. Exact-vertex connectivity
can isolate an open cap after a cut; removing it caused the next real rocket
blast to reject with RF_FORMAT. The earlier six-case installed probe likewise
proved body construction but did NOT prove valid closed solids. Those body
counts must not be treated as detachment-fidelity evidence.

Production extraction now checks every directed component boundary interval for
exactly one reversed collinear mate. Existing exact T-junction subdivisions are
accepted without moving vertices; open/multiply-covered boundaries stay in
terrain. Unit controls reject an open5-face box and single face. The prior closed
box and L/T-junction fixtures still pass. Real blast component topology needs
further repair: the six installed probes currently report failed0/extracted0,
and intentionally return failure because no accepted detachment was observed.
The gate is conservative and uses exact collinearity, so this does not prove
those real components are geometrically impossible solids.

## Executed scene evidence

PC process-local two-shot continuation now passes: saved350frames, continued200,
uninterrupted550; both committed blasts succeed. Continued and uninterrupted
RFCP/RGCH/publication files are byte-identical. Reset-zero continuation also
passes. Reports are in:

- artifacts/geomod-postedit-re/detached-active-continuation/report.json
- artifacts/geomod-postedit-re/detached-active-reset/report.json

The final two-shot DETACHED_PIECES row is1 0 0 0 720 0: registry active, zero
accepted batches,720PC resident bytes, no draw error. This is not visible body
acceptance. The earlier nosave frame/row with one body predates the closure fix
and is invalid evidence for the final implementation. The corrected control
frame was inspected: room, rocket weapon/HUD and remaining post geometry render;
no accepted detached chunk or physics motion is demonstrated.

Release extraction, extracted-replay and scene-transaction checks pass. Stock
NXDK scene/core build passes in artifacts/geomod-postedit-re/detached-active-xbox.log;
no XEMU runtime was launched. Read-only checkpoint reconstruction can revisit
known registry identities without altering body/RNG state; new identities still
require an active transaction. Error diagnostics identify transaction phase and
rejected blast transform for continued investigation.

Next: reconcile component edge/support identities on the actual blast geometry,
then verify retained-body drawing, add motion/collision and active-state saves,
and measure native memory/performance. Cavity extraction remains unsupported.
