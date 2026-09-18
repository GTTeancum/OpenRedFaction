# Moving-surface response boundary

The ordinary nonrigid response at RF.exe 0x49d330 does not consume the recorded counterpart velocity at object offsets 0x1d8..0x1e3 in the exercised paths. Adding that velocity to the reconstructed response would be a new port policy, not a demonstrated missing retail calculation.

## Executed evidence

`tools/verify_physics_solid_contact.py --nxdk` now executes each of its 512 original cases again with counterpart velocities (2,0,0), (0,3,0), and (-4,-2,1). It restores the complete input object and stack for each run, compares all 5376 object bytes against the stationary-contact run except the 12 supplied velocity bytes, and separately asserts those input bytes remain unchanged. All 1536 variants pass. Existing 512 original/PC comparisons and 512 compiled NXDK comparisons also pass. Original executable SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

The source-address decompilation of 0x4a01b0 calls 0x48a400 before 0x49d330, and only enters the latter when the callback returns 2. Thus this result does not rule out moving-surface handling in the callback, query, scheduling, or another response route. The existing kind3 callback probe below resolves that boundary; generic mover query timing is the next integration boundary. These tests supply material getters and do not execute the full scene scheduler or demonstrate a moving door on Xbox.

## Implementation decision

Keep the established solid response unchanged. Current world/mover edge contact preserves committed geometry, material and counterpart identity, but that alone does not complete moving-surface collision. Investigate the callback and mover wake/contact scheduling before designing relative-motion, carry or crush behavior. Do not mark mover support complete from this routine-level invariance result.

No gameplay or rendering changes, new emulator session, screenshot, or performance claim in this pass. Report: artifacts/physics-solid-contact.json.

## Callback and relative-query follow-up

The previously retained `probe_fragment_contact_dispatch.py` was rerun: all 27 cases pass through real 48a400/412b40 and 49d330 with no counterpart-field reads. That is existing evidence reconfirmed, not 27 newly discovered behaviors. It resolves the dry kind3 callback concern above.

New `tools/probe_fragment_mover_motion.py` executes original 49bb70 through the first mover geometry call at 4df1c0. Only atexit registration is supplied; original broadphase, sphere access, matrix/vector helpers and remaining-time arithmetic execute. Thirty-six cases cross three body/mover remaining-time ratios, three translation vectors, centered/off-center spheres, and identity/90-degree mover orientation. All captured geometry requests match independently calculated local endpoints exactly.

For body remaining time tb and mover remaining time tm, the mover origin at the beginning of this body interval is `m0 + (1 - tb/tm) * (m1 - m0)`. Subtract that origin from the current world sphere position; subtract m1 from its predicted world position; transform both into the mover basis. Their difference is the local sweep delta. Thus a stationary body can have a nonzero relative query when a surface moves. This is collision-path timing, not a velocity addition in the impulse response.

The experiment stops before geometry returns: it does not prove contact publication, wake, carry, crush or complete moving-door gameplay. Body bases remain identity; the rotated cases rotate the mover basis only. Synthetic overlapping bounds, positive tm and tb <= tm are explicit constraints. Report: artifacts/geomod-postedit-re/fragment-mover-motion.json.

The scene currently commits group positions through `campaign_controller_commit` and synchronizes mover views; `rf_group_attached_pose` does not retain the original per-body remaining-time field. Integration therefore needs a deliberate motion interval shared with the fragment scheduler, preserving start/end poses until queries finish. Substituting committed pose minus velocity times a guessed timestep would not reproduce the tested contract. Sleeping-fragment admission/wake remains separate.
