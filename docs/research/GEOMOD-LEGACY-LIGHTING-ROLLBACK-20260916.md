# Transactional DEV rocket cuts

Ordinary legacy DEV rocket cuts now prepare lighting, render subdivision and
collision-overlay binding before committing the core cut. Failed preparation
discards the candidate and private resources. Successful preparation commits by
copying into the existing live owners; no allocation or validation follows core
publication. Authored-post edits retain their existing transaction path.

The new scene_terrain_legacy_edit.inc uses the checked template-cut API and the
existing private lighting stage. A temporary overlay validates the new face-ID
map and room tree. Publication preserves the live overlay, atlas and draw-buffer
addresses, repairing copied tree/index pointers. Rejection preserves the live
geometry, collision, atlas, noise owner and draw data. Admission and cut-basis RNG
still advance for rejected requests, as intended by the existing history policy.

Concurrent core/current/pending tree storage, staging reservation, copied overlay,
draw/color/ID arrays, atlas, light cache/workspace, template, debris and regions
are checked against SCENE_DESTRUCTION_BUDGET. The lighting stage remains capped
at2MiB. There is no cloned core/history replay added per cut. Timing label cut_ms
now includes precommit lighting/draw/collision preparation; it must not be
compared as pure CSG time against the former path.

PC fault injection uses RF_REPLAY_TERRAIN_MAP_LIMIT and optional
RF_REPLAY_TERRAIN_MAP_LIMIT_UNTIL. It restricts actual new map admission through
the ordinary allocation branch; it does not return an artificial success or
change existing charts. These environment controls are not enabled on Xbox.

`tools/check_geomod_lighting_rollback.py` passes six real process-local replays:

- Create and save a one-cut crater.
- Load it with no blast as a control.
- Load it and hit the map limit after allowing one new map: the new cut rejects,
  the session survives, and
  the physical mesh and settled atlas are byte-identical to the control.
- After that rejection, allow normal capacity: a later blast commits cut2.
- Walk into the old crater with and without a preceding rejected blast: final
  position/orientation words match exactly; X=-17.523134 crosses the old wall.

Evidence: artifacts/geomod-lighting-rollback/report.json. Full default PC build
and all112 CTests pass. Expanded normal20-pulse/16-cut replay passes exact
topology and closure. Its checkpoint, atlas and endpoint frame match the prior
reference byte-for-byte. Vertex positions and face records match;360 UV component
bytes differ, with maximum numeric delta4.76837158203125e-7. No visual difference
is claimed or introduced to hide this result. A current-build reload reproduces
the staged run's checkpoint, physical mesh and atlas exactly.

Remaining scope: broader allocation-failure coverage and non-rocket legacy
diagnostic cut/reset entry points. This is not a claim
that every scene operation or later debris-spawn failure is transactional.

Native normal-path verification: stock64MiB XEMU run
`artifacts/xemu/render-20260916-231302` completes2500 frames and passes58
comparisons with16 committed cuts. Xbox and PC checkpoints match the pre-staging
checkpoint exactly. Endpoint7849 free pages (30.66015625MiB); sampled free memory
is not a proof of the exact transient allocation minimum. The actual endpoint
framebuffer was inspected: textured room, dark crater, rocket launcher and HUD
remain present with the prior appearance. No new visual-fidelity acceptance;
audio was disabled, and intermediate frames were not individually inspected.
The owned emulator exited and disc staging was restored.

## Native capacity-failure recovery

The harness now accepts --terrain-map-limit and --terrain-map-limit-until,
staging an explicit8-byte terrain-map-limit.bin descriptor for Xbox and matching
PC environment values. Xbox rejects malformed size, zero/out-of-profile map
counts and zero frame limits. The descriptor is included in normal disc backup,
removal and restoration; it is absent during ordinary runs.

Stock64MiB `artifacts/xemu/render-20260916-232028` restores the one-cut seed,
permits one additional map before exhaustion (limit16), rejects the first blast,
then allows the next blast after frame300. It completes750 frames and passes58
checks. Both targets report exactly one rejection and one successful new cut,
ending with two cuts. The complete Xbox checkpoint matches both its paired PC
reference and the independent PC recovery replay. Endpoint8071 free pages
(31.52734375MiB). The native endpoint image was inspected: room, crater, weapon
and HUD are present. No audio or every-frame visual claim is made.

Native traversal after rejection also passes58 checks in
`artifacts/xemu/render-20260916-232200` (850frames,8071 free pages). It reports
one rejected blast and no new cut. All77 player-body words match PC, including
the position inside the preserved crater X=-17.523134. The full checkpoint
matches the independent walk_rejected PC replay. Endpoint inspection shows
the player inside the dark textured excavation, with weapon/HUD visible.
The owned emulator exited; all saved disc entries were checked restored,
including removal of the normally absent terrain-map-limit.bin descriptor.
