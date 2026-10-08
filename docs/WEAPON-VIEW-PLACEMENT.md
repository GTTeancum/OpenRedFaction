# Authored first-person weapon placement

Source-written on 2026-10-08 for the parent-coordinated 23:00 Xbox batch.
No helper build, runtime test, emulator session, screenshot, or original-game
capture was performed. This change depends on the authored `position[3]` and
`fov` fields retained by the shared first-person resource owner.

## Original evidence and sign convention

The installed `weapons.tbl` defines the normal `$1st Person offset:` and
`$1st Person FOV:` independently for each first-person model. `RF.exe`
`4c3084..4c30f7` reads FOV into weapon descriptor `+74`, defaulting to the
90-degree float at `59613c`, and reads the required normal offset into `+4c`.
The separate split-screen fields occupy `+78` and `+58`.

In the normal first-person placement path, `4aa762..4aa786` copies the
descriptor `+4c` vector to player `+1018`. `4aa798` invokes `4facb0` with
the eye basis at entity `+7e0`, and `4aa7a6` invokes `40a350` to add the eye
position at entity `+7d4`. The matrix callee multiplies each offset component
by its corresponding eye-basis axis and sums them. The addition callee adds
the eye componentwise. Thus the base world-space vertex is:

    eye + eye_basis * (animated_mesh_vertex + authored_offset)

The shared first-person renderer already works entirely in the eye frame.
`rf_model_project_vertex` subtracts `view.camera` from each animated model
vertex before applying `view.rotation`. Therefore the new setup stores
`view.camera = -weapon->position`, where `position` retains the authored
offset. The subtraction gives `mesh - (-offset)`, namely `mesh + offset`.
It does not subtract the offset from mesh vertices, translate the bone pose,
or add the world-space eye a second time.

The draw path at `4ab3f0..4ab411` passes the normal descriptor `+74` FOV and
eye basis/position to `517eb0`, which forwards them to renderer `547150`.
This replaces the scene's slot-indexed fitted constants. Previously the
pistol, assault rifle, riot stick, shotgun, rocket launcher and grenade
had incorrect offsets; riot stick, shotgun, rocket and grenade also had
incorrect FOV. Machine-pistol alternate resources now use their own retained
metadata rather than falling through to assault-rifle framing.

Selected normal table examples, in model units/degrees:

| Weapon | Authored offset | FOV |
| --- | --- | --- |
| 12mm handgun | (-0.110, -0.140, -0.342) | 65 |
| Assault Rifle | (-0.064, 0.100, -0.280) | 65 |
| Riot Stick | (-0.178, -0.418, 0.072) | 40 |
| Shotgun | (-0.020, 0.056, 1.071) | 55 |
| Rocket Launcher | (0.050, 0.030, -0.080) | 70 |
| Grenade | (-0.202, -0.336, 0.206) | 70 |

## Matching direct and clipped projection

`scene_player_weapon_view_prepare` uses the existing reconstructed
`rf_visibility_view_scale_build` implementation of `547150`. For the
640x480 single-player view, FOV and aspect belong in the clip-space diagonal;
screen projection uses the 320x240 viewport half sizes. Both the ordinary
projector and newly generated clipping vertices now use that same view.

The previous code used an identity clip-space transform and put the focal
length only into `view.screen`. The clipping code tests against `+/-z`,
while generated vertices project with viewport half sizes. This mismatch
gave the same triangle a different projection once clipping generated a
vertex. The new setup fixes that discrepancy without changing the common
world/model clipper or Xbox renderer.

The original depth scale is normalized out of all three axes together.
Perspective X/Y ratios and side-plane inequalities are preserved, while the
existing model-unit 0.01 near plane, 1000 far plane and first-person depth
band remain intact. This is the existing port depth policy, not a claim to
recover retail depth-buffer configuration. The NV2A preview vertex shader
already consumes projected X/Y directly; no second platform transform is
needed.

## Image-free observation

The public `rf_scene_player_weapon_view[16]` contains the most recent draw
attempt. Float entries are raw binary32 words for exact guest readback:

| Word | Meaning |
| --- | --- |
| 0 | Frame plus one |
| 1 | Selected weapon table ID |
| 2 | Retained view-resource slot |
| 3..5 | Applied authored offset |
| 6 | Applied authored FOV |
| 7..9 | Model-space camera origin, the negated offset |
| 10..12 | Applied normalized clip-space diagonal |
| 13 | Emitted first-person vertices |
| 14 | Completed material batches |
| 15 | Status |

The array resets on each draw attempt. A hidden/dead/unarmed view has zero
vertices/batches and no applied metadata. Its frame identifies whether the
draw path actually ran; retained values from a skipped scene pass do not
establish a new draw. Existing `rf_scene_player_weapon[8]` retains animation,
resource, pose hash and draw status information.

The parent can read exact offset/FOV/camera words and require nonzero
vertices/batches in the existing armed L4S5 check. That demonstrates a live
authored placement was used, but is not a visual-parity claim. No additional
gameplay fixture or screenshot is needed.

## Boundaries

This slice reconstructs base normal first-person placement and projection.
Existing authored idle/fire/reload animation remains the resource owner's
responsibility. The subsequent original `4aa7ab..4aaa7c` dynamic angular
response and tag-relative corrections are not newly implemented here;
full turn lag/sway remains separate. Split-screen, original near/depth policy
and retail appearance validation are also outside this slice.
