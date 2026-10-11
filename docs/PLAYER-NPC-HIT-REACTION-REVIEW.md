# Final joint source disposition: ordinary hit reaction

2026-10-10 21:55 UTC. Reviewer: review_medic_use_ownership.

APPROVED FOR PARENT-OWNED INTEGRATION AFTER THE FROZEN22:00 BATCH OUTCOME. This is a source-review disposition only. No compilation, syntax check, test, emulator execution, shot, pain, save/load fixture, or runtime coverage was performed or inferred. No active repository source was edited by this reviewer.

Joint tree inspected: /workspace/shared/hit-reaction-joint-next, assembled by parent from exact d98ee2882c9caa467e0ba2bcb23d118b0f3fbce0 baseline with:
- Runtime patch74189d0ac094d4905e210e7dcb2f8053d7db8704b60171b90981c3976cd9d263
- Codec patch8dea024b635fa9afb4826fbed2b1f57d15a8ea9a3e925ad502fb91e270bce85f

All ten modified joint files were independently hashed and match the reviewed components:

```
9d02a92ca8b1ad29edab05fba7b0adb88e6891d31724236c31a16513f4774dc6  src/diagnostic/scene.c
f223b474db1d737dea8d0d104c17d5250eab85aa4cd31bcece0d2ac757ae71e0  src/diagnostic/scene_npc_hit_location.inc
22759d9e5184b5a73d9c6133ffccd89edc906cfd773a4afd74311a17914da15b  src/diagnostic/scene_handheld_penetration.inc
ecd2b6c4850e2e5902ce13fbe19ac321eb65fe79d227022c1e8c0a935fd0b31c  docs/PLAYER-NPC-HIT-REACTION.md
445e2923369cdff46d3c2f688ea50767f432143894690fa11368150873d79f8c  include/rf/npc_checkpoint.h
0ec497a862133dfc9a966ddbbf26e9976d26895b2b40a63e91065ec9bf2dd06c  src/core/npc_checkpoint.c
a31eb5dc0317d5121eb48c4b28b82151288c126129f532b047cee805cda94a62  src/diagnostic/scene_npc_pain_checkpoint.inc
8ee57641dd526c2dbd7b9c4c28c84eb0baa2bb789575fe65d72546aa1003d2db  src/diagnostic/scene_npc_checkpoint_capture.inc
37ed02b4601c827d27ad46863d04c42518d2e25d0af9a934b3e6410ab1fabe38  src/diagnostic/scene_npc_checkpoint_restore.inc
794f073c50d0838f38fe7b2df66ce3ee211b0051565ed28222ba8b4b410c0f6a  src/diagnostic/scene_burning_checkpoint.inc
```

## Integrated ownership result

The runtime patch supplies only the original retained828 hint from qualified actual contacts. Published-frame side, ordered sphere index, close-SP-Shotgun exception, no-choice preservation, source/target revalidation, shield/Nano ordering and immediate generic-damage boundary are preserved. Existing pain code exclusively owns suppression, real weapon reset, actual action demand/start, random cooldown and authored-duration lock. No additional AI/script/medic policy, timer, RNG draw or death hint is introduced.

The codec half retains the exact existing timers/action/RNG and full playback. RFNC24 has unchanged RFNC23 wire layout, with a four-byte in-memory capability separating its new enum/residual language from RFNC1..23. Old snapshots retain old enum and residual admission. New residual proof is exact-class/current-or-saved-selection, full registry/model/resource identity, requested nonloop slot and unfinished authored cursor, finite phase/weights including zero, and no freeze. It does not require hint match, pending timer, no combat, or ordinary visibility. It grants no ownership to other one-shots.

Ordinary residual-only capture carries the actual pain payload. Active script-owned clips do not acquire redundant residual-only pain presence. Burning stays exclusively RFAP-owned, and unresolved residual slots require a genuine validated composed burn join with unchanged exclusivity/RNG checks. Standalone RFNC cannot manufacture a missing burn owner. Existing whole-playback restore performs no pain/start/sound/damage replay.

Both first-pass codec defects were corrected in these exact hashes: no new hidden-ordinary residual veto, and no redundant residual-only pain owner for active script playback. Requested-slot-only resource residency respects sequential candidate demands. Existing publication timing and accepted fresh-section RFCH policy remain unchanged; no close-atomic old-world rollback or cross-section exact pain continuity is claimed.

## Joint interface and limits

Read the integrated include/call order: class-name helper is available from scene_npc_hit_location_types at scene.c704; runtime helper is included at11379; pain definitions come through capture at14045 before restore14047; burn join follows at16261. Both ordinary damage commits call the defined reaction publication helper. Both changed pain capture/prepare signatures match their sole callers. No new build-list source/include is needed, and no positional initialized state was introduced by these changes.

The runtime contact gains56 stack bytes. The nonwire checkpoint capability gains4 bytes per row and8 per saved/before restore-entry pair; sizeof-based budget checks remain. RFNC24 has no layout growth relative to23, but version promotion includes existing prior tails (164 bytes per row from15;276 from10). Fixed transport/stage caps, live NPC/history storage and original resources are unchanged.

Supporting component reviews:
- ../hit-reaction-runtime-next/INDEPENDENT-REVIEW.md (component code review plus documentation updates)
- ../hit-reaction-codec-next/INDEPENDENT-REVIEW.md, SHA2566946abad438340b46f3eaa0ddf50f7cce2f4b2946a1bcc96db1b5faa3d07ae61
- INDEPENDENT-REVIEW.md, source-design/legacy-language refinement SHA256c4e4033ec4c63a0304cd5df2679b5fa281cc3c4c07ec4ab922d4f67c10eb12ad

Neutral startup admission, if later performed by the parent, will not demonstrate a hit reaction or save continuation. Any actual execution claim must be based on the parent's separate evidence.

## Approved final comment-only amendment (21:56 UTC)

After the ten-file joint verification above, the codec worker corrected only three comment lines in scene_burning_checkpoint.inc. The reviewer diffed that exact file against the approved parent joint tree: there is no executable change. Final approved codec patch is033c51c70789441944b8430d9f85f1fa6b6def1ba26e1391433eca6e6b10d177; its burn include is4e92fd798f7c14daf323458c367be781ee236e3e66bdcc60a1b3a05a8884e5a8. Every other joint source/doc hash above remains unchanged. Runtime remains74189d0ac094d4905e210e7dcb2f8053d7db8704b60171b90981c3976cd9d263. This narrow amended set has the same approved source disposition and after22 integration boundary; parent owns copying/verification of that final comment-only file.

Final parent reconciliation verified at21:57 UTC: root joint src/diagnostic/scene_burning_checkpoint.inc matches4e92fd798f7c14daf323458c367be781ee236e3e66bdcc60a1b3a05a8884e5a8. Combined joint.patch is70acafc3cd3f313a4aad8471af93b8d179ea4c834401a228e2b79a84216574e1. This is the final approved joint artifact.
