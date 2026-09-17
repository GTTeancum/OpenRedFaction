# Restored player clearance against detached solids

Authored checkpoint staging now checks restored body geometry after static
world/player standing validation and before any scene publication. Each actual
player sphere is transformed through the player basis into world coordinates,
then into each restored fragment's local coordinates. This reads the privately
reconstructed registry and its saved poses, not current live bodies or birth
geometry placement.

`rf_checkpoint_solid_sphere_check` reuses checkpoint polygon/edge distance and
original-derived nearest-face classification. The existing.002 clearance margin
permits tangency. Surface overlap rejects; an entirely contained sphere center
also rejects. Closed outward solid orientation reverses the interior semantics
of inward-facing room surfaces: a nearest back face indicates solid interior;
a front face or no forward intersection is outside. Sixteen bounded edge
ambiguity retries are retained. No zero-motion sweep, bounding-box substitute,
allocation or player relocation is used. Input must be a closed oriented solid,
as provided by the existing extraction ownership gate; this is a port save-fit
policy, not a recovered original save ABI.

Core tests cover exterior clearance, tangency, surface overlap, inside-solid
rejection, registry placement with independently rotated chunks and offset player
spheres, and nonfinite-input output preservation. The real
middle-shot save/reload replay still produces identical checkpoint bytes and
post-region pixels. `tools/check_detached_placement.py` takes that actual RFCP,
translates the fragment current/next pose and bounds onto the saved player,
and confirms rejection specifically at DETACHED_PLAYER_RESTORE_REJECT. The
body remains finite and its immutable geometry/material/tensors are unchanged;
this is not merely an invalid-header test. Raw RFCP has no outer RFSG checksum.

The private checkpoint owner is discarded on rejection; the live publication
has not yet been committed. This gate only adds clearance. Static world support
is still required by the earlier standing gate, so saved standing-on-rubble
support remains unfinished rather than silently accepted without validation.
No live visual change is expected from valid saves. Native malformed-save and
more rotated/concave body-placement cases remain useful follow-up coverage.

Validation: all121 CTest cases passed and the NXDK build produced the XBE/ISO.
The added rotated-clearance cases then passed the targeted extraction test.
Native valid-save continuation at artifacts/xemu/render-20260917-070903 passes
74 checks over200 frames on stock64MiB. The native framebuffer was inspected:
the settled tilted fragment, room, weapon and HUD are present. PC/Xbox checkpoint
continuation matches; the harness restored the disc and exited its own emulator.
Malformed overlap rejection remains PC-only evidence. This run does not claim
new visual quality, moving support, or broader destruction acceptance.
