# Transactional box and reset controls

The remaining interactive legacy DEV mutation controls now use the same private
lighting/draw/collision stage as rocket cuts. Use+alternate fire requests a box
cut; crouch+use+alternate fire requests reset. Reset still checks every body
sphere against the original room before preparing any replacement, so it does
not close the wall through a player inside an excavation.

New checked box/reset core APIs share the existing precommit callback contract.
Legacy ordinary APIs still pass a null callback. The scene dispatches template,
box or reset through one transaction implementation. History/noise/debris reset
cleanup runs only after successful publication and contains no fallible work.
The redundant postcommit scene_terrain_bind calls were removed from both controls.

Core checked-cut tests now cover rejecting and retrying box cuts and reset,
including saved history, live pointers/bytes, generation and collision queries.
The callback's candidate is compared with an independently committed control.

`tools/check_geomod_legacy_controls.py` runs four PC cases on the default profile:

- Intact room control.
- Map capacity1 rejects a box cut after private preparation begins: mesh and
  atlas remain byte-identical to the intact control; no generation advance.
- Ordinary box, reset, fresh box control.
- Rejected box, successful retry after capacity returns, reset, fresh box: final
  mesh/atlas match the ordinary control exactly. Four attempts produce three
  successful operations, final cut count1 and generation4.

Evidence: artifacts/geomod-legacy-controls/report.json. Existing
`tools/dev_geomod_lightmap_reset.py` also passes its eight-rocket-cut/reset/fresh
rocket sequence with generation11 and a freshly allocated15-map lighting owner.
These establish reset/re-cut behavior without relaxing allocation bounds.

Full default build and all112 CTests pass. Stock64MiB XEMU default-profile run
`artifacts/xemu/render-20260916-232800` executes the650-frame rejection/retry/
reset/fresh-cut sequence and passes57 comparisons. Both targets report final
cut count1,generation4,four attempts and three successful operations. Endpoint
8527 free pages (33.30859375MiB). Native endpoint inspection confirms a rectangular
test excavation, textured room and weapon/HUD. Intermediate frames and audio
were not visually/audibly validated. The owned emulator exited and staging was
restored; no GitHub image was added.

Remaining scope includes allocator-failure injection, initialization/checkpoint
replacement paths, and arbitrary destruction/collision fidelity. Diagnostic box
shape remains a test convenience rather than original rocket geometry. No new
visual fidelity or campaign functionality is claimed by this safety change.
