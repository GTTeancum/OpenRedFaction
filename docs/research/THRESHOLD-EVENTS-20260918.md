# Health and armor event integration

When_Life_Reaches87 and When_Armor_Reaches88 now poll registered player/NPC health or armor against signed authored words[0]. An eligible linked owner at/below threshold latches once before dispatch. Mixed links activate events/movers with sentinel source/actor and enable triggers without firing them. The recovered observer path is independent of common disabled/deadline gates. Stale/non-entity owners are skipped; dead registered actors retain zero vitals. State adds8bytes/event, counted by existing allocation size.

Focused checks: event_threshold, runtime_threshold and scene_vitals_query pass, including actual runtime mixed-link effects, one-shot latch and live/dead/stale scene reads. PC scene builds. No authored live level or XEMU scenario run for this batch. Threshold latch and cyclic state still need same-level persistence; no new disk format was invented.

Also removed the loop-wide player-health gate from enemy combat. A per-owner liveness gate clears player-targeted pursuit/combat while preserving NPC-targeted orders and authored movement. It is repeated after invalid NPC target release to avoid acquiring a dead player. scene_ai_target_liveness passes; NPC-vs-NPC live encounter continuation remains unverified.

Overall approximate implementation62%; scripted interactions approximately65% first pass. These are engineering estimates, not coverage percentages.

NXDK build also passes: artifacts/precision-live/threshold-xbox.log.

## In-memory revisit history

Cycle and threshold history now saves on scene close, restores after startup, and resets for a fresh campaign. Keys are level name plus authored UID. Cycle count/enabled and remaining deadline (rebased at reopen) survive, as do one-shot threshold latches. Authored period/limit/threshold remain fresh. Fixed40,968-byte owner mirrors existing Switch/trigger histories; no stale runtime handles or disk format. Focused scene_event_history passes real key/restore/rebase cases and PC/Xbox builds pass. Live route, removed monitors, generic delayed-action ownership and disk persistence remain open.
