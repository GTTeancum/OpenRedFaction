# Future campaign: Teleport_Player pose handoff — 2026-09-15

## Result and current gap

Type63 (`Teleport_Player`) is named by the loader but is not supported by the runtime action scheduler. It is a high-priority follow-on to cutscene completion: authored completion chains in L6S3 and L11S3 invoke it, and merely copying a spawn position would leave physics, rendering and eye orientations inconsistent.

`tools/future_re/campaign_teleport_player.py` executes **complete original4b9820**, the actual48a230 position/bounds update, vector/matrix copies,4fc060/504ea0 angle extraction and42d840 stores.32 cases pass in `teleport-player.json`, guarded by RF.exe SHA-256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`. External host lookup, level-name predicate, vehicle detach and player/network notification boundaries are intercepted. No shared source, builds, emulator or desktop control was used.

The fixtures cover absent player; unattached player; attached player with detach returning both0 and1; the L20S2 host-target exception; SP, multiplayer client and server gates; identity, yaw90 and two converted authored destination matrices. Generic event activation/delay/propagation is already independently verified and was not repeated here.

## Destination ABI and orientation ordering

4b9820 is cdecl with one event pointer stack argument. It obtains the player entity from global5cb054; null player returns immediately with no side effects. Destination position is event+40 (three floats), orientation is event+4c (nine runtime-order floats). Generic words/texts/linked-target arrays are not consulted by this action.

Do not memcpy the event's raw nine disk floats directly into a runtime matrix. Loader462150 calls52cac0 for type4/63/70;52cac0's present-value path reads the first vector into matrix+18, second into+0 and third into+c. Therefore disk forward/right/up becomes runtime right/up/forward: `[disk3..5, disk6..8, disk0..2]`. This is corroborated by the existing trigger matrix reader and matches the conversion used by the probe's two authored fixtures. The loader reader itself is disassembled evidence here, not newly executed in this script.

## Selected target and vehicle exception

The action resolves player+200 through426fc0. No resolved host means the target stays the player. With a host:

- If5001d0 reports the current-level string645fe4 matches literal `L20S2.rfl` at5a213c, the action changes its target to the host entity and does not detach the player.
- Otherwise it calls4279d0(player), then **ignores its AL return** and proceeds to teleport the player. The executable fixtures force AL0 andAL1 and verify identical target pose writes in both cases. Vehicle detach's own ownership/seat mutation is explicitly outside this probe and belongs to the vehicle agent.

This differs from ordinary interaction-driven exit, which the vehicle agent found checks the detach result. Do not use a generic “try exit then abort teleport if blocked” helper for this scripted event without an intentional compatibility decision.

## Exact successful target mutation

The action first copies destination position into target+f0, then calls48a230 with ECX=target and one pointer argument. The real helper executes:

- Destination position copied to target+3c, +e4 and+f0.
- Bounds vectors+190/+19c become destination minus/plus scalar target+180 when it is positive; for nonpositive radius they become destination directly.
- Target object flags+7c gain0x04000000, preserving all other bits.

The action then copies the complete destination matrix into **four** locations: target+48, +fc, +120 and+7e0. It converts matrix+48 through4fc060, negates the first returned angle and stores all three at target+864 via42d840. Identity yields(0,0,0); the yaw90 fixture yields(0,1.57079637,0). The probe retains the exact outputs for the authored matrices, avoiding an assumed alternative Euler convention.

For an unattached target, the tested24 bytes at+144/+150 remain unchanged (fixture values1..6). Thus this action and its actual48a230 helper do **not** unconditionally zero those vectors. A detach boundary could alter them independently; that was intercepted. Do not copy the cutscene-stop behavior, which does zero those two vectors, into the teleport action by analogy.

No direct room-tree locate, collision sweep or pathfinding call occurs in4b9820/48a230's executed SP path. The dirty object bit and later normal engine processing own follow-up work. The port must refresh its explicit room/collision/public-pose owners before consumers see the new position, but such scene integration is a practical requirement, not a recovered immediate original callback.

## Notification gate

After pose writes, the action reads byte globals64ecb9/64ecba. When the first equals1 and the second equals0, it returns without testing42a0d0 or invoking4a0770. Other combinations call42a0d0(target); only AL1 invokes4a0770(target). Existing global usage identifies these as multiplayer/client-server context, but the exact internal effect of4a0770 is outside this probe.48a230 also contains a multiplayer-server lookup branch4a3740; the fixture supplies no matching object and does not infer its logging/lookup behavior.

## Authored usage

All8 installed type63 records are retained in `teleport-player-authored.json`:

| Level | Event UID | Destination links |
| --- | --- | --- |
| L6S3 | 6998 | none |
| L11S3 | 10654 | none |
| L20S2 | 18458 | none; host-target level exception matters here |
| L7S3 | 7950 | none |
| L7S4 | 10853 | none |
| L8S4 | 8476 | none |
| L13S3 | 8711 | 8712 |
| L14S3 | 9657 | 10240,9656 |

The action itself ignores links, but ordinary event propagation still delivers them afterward. Therefore adding only a scene position callback without retaining existing action-then-propagation order is insufficient for the last two records.

## Proposed integration and acceptance

Add a `teleport_player` callback to the runtime event service boundary, enable type63 in scheduled event dispatch and call it only for the on action. The callback receives finite position and converted runtime orientation. Keep the existing off action and generic propagation policy; verify the original off dispatcher before assigning any off-side effect.

At the scene boundary synchronize `scene_actor_body.state` current/next position and orientation, public/group pose, `actor_look` facing state and eye orientation. Existing `rf_scene_campaign_pose_get` illustrates the separate body-position and eye-orientation owners; it is not a setter. Refresh room membership/support/ground caches deliberately, rather than using campaign import—which currently carries inventory/vitals and not this complete pose state. Do not reinitialize velocity or all actor state simply to achieve visible teleportation.

A useful first acceptance fixture triggers the real L6S3/L11S3 authored event through the existing event timer/link path, verifies all owned pose representations on the same tick, then walks/aims normally on the following tick. Additional cases: no player; nonzero delay; disabled event; linked follow-up events; identity/nontrivial authored orientation; moving player velocity preservation; attached host success/failure; L20S2 host-target rule. Vehicle detach integration must be coordinated with its separate RE work. Native camera and collision correctness still require a real PC/Xbox scene test after implementation.
