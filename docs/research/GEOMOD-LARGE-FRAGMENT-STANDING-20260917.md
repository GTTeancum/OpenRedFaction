# Real large-fragment player support

Earlier standing fixtures used radius0.530725 rubble and the original sphere
contact path. Synthetic cube tests covered larger fragments, but no ordinary
gameplay extraction/standing case had qualified the radius>1 polygon path.

`tools/probe_beam_center_fragments.py` fires three ordinary, analytically aimed
rockets at beam95 while retaining its connected posts94/93. The source geometry,
rocket size, fragment geometry and admission threshold are unchanged. The middle
shot at projectile target(-4.699,2.25,0.5) produces a real radius1.213921905 fragment
which falls and settles at(-5,-0.9990665,2.6365211). The Z1 target also produces
radius1.027201533 rubble. The Z0 control produces0.797659457 and0.996623158 pieces,
both below the polygon threshold. Generated inputs/saves/logs are local under
`artifacts/beam-center-fragments`; original assets are not tracked.

`tools/check_large_rubble_standing.py` loads the middle-shot checkpoint, turns
back west, moves sideways around the intact post, then walks/jumps onto the
fallen beam section. This avoids mistaking collision with the intact post for
rubble contact. Final player position is(-5.180543,0.095498,1.499985). The trace
records330 detached contacts with polygon face0 rather than the sphere sentinel;
the saved body radius is explicitly asserted>1. No inflated synthetic body,
teleport, host input or changed collision gate is used.

The3112-byte standing player/destruction save restores and advances120 steps
identically to the uninterrupted PC control. A separate ordinary camera turn
looks down at the supporting beam; the image was inspected and shows the wood
section beneath the player beside the retained upright post. Player position is
unchanged during that turn and362 continued polygon contacts are recorded. The
usual forward-facing endpoint has the support underfoot/out of view and therefore
is not itself visual proof of the chunk. No original-game screenshots used.

This proves this extracted geometry and standing/continuation route, not general
rotating-fragment behavior, angular carry, all large shapes or campaign readiness.
PC report: `artifacts/large-rubble-standing/report.json`. `--no-save` is an
inspection-only mode and does not claim save acceptance.

Stock64MiB XEMU run `artifacts/xemu/render-20260917-214830` passes76 checks for
the351-record approach from the extracted-fragment save. Native detached-player
telemetry matches all927 queries/330 polygon contacts, and the3112-byte checkpoint
is byte-identical both to the harness reference and the independent PC standing
save. Endpoint3387 free pages is13.230MiB, not a peak-memory guarantee. Native
frame inspected: room, damaged overhead beam, intact posts, launcher and HUD
remain visible from the supported player pose. The support itself is underfoot;
its PC look-down view and native contact/save state provide separate evidence.
The owned emulator exited and disc files were restored. Audio was disabled; no
GitHub image added. Native reload/walk-away from this standing save remains open.
