# Moving-surface response boundary

The ordinary nonrigid response at RF.exe 0x49d330 does not consume the recorded counterpart velocity at object offsets 0x1d8..0x1e3 in the exercised paths. Adding that velocity to the reconstructed response would be a new port policy, not a demonstrated missing retail calculation.

## Executed evidence

`tools/verify_physics_solid_contact.py --nxdk` now executes each of its 512 original cases again with counterpart velocities (2,0,0), (0,3,0), and (-4,-2,1). It restores the complete input object and stack for each run, compares all 5376 object bytes against the stationary-contact run except the 12 supplied velocity bytes, and separately asserts those input bytes remain unchanged. All 1536 variants pass. Existing 512 original/PC comparisons and 512 compiled NXDK comparisons also pass. Original executable SHA256: b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836.

The source-address decompilation of 0x4a01b0 calls 0x48a400 before 0x49d330, and only enters the latter when the callback returns 2. Thus this result does not rule out moving-surface handling in the callback, query, scheduling, or another response route. The callback remains the next investigation boundary. These tests supply material getters and do not execute the full scene scheduler or demonstrate a moving door on Xbox.

## Implementation decision

Keep the established solid response unchanged. Current world/mover edge contact preserves committed geometry, material and counterpart identity, but that alone does not complete moving-surface collision. Investigate the callback and mover wake/contact scheduling before designing relative-motion, carry or crush behavior. Do not mark mover support complete from this routine-level invariance result.

No gameplay or rendering changes, new emulator session, screenshot, or performance claim in this pass. Report: artifacts/physics-solid-contact.json.
