# Cutscene phase scheduler and point actions

Nine original instruction slice scenarios pass in `tools/future_re/campaign_cutscene_phases.py`; results `cutscene-phases.json`. Original executable hash matches companion reports. This executes45b67a..45b877, including real timer helpers, phase writes, camera copy and fraction arithmetic. Spline samples, next-point setup and completion effects are intercepted. Entry camera setup and rendering tail are not executed. This is scheduler evidence, not verified camera animation.

## Phase semantics

Original tick entry is cdecl45b5e0(byte). Scheduler45b67a gives descriptor+810 total timer priority. When due, increment+808 index once. Below+4 count, call45b3f0(desc,index), then leave: no catch-up loop. Exhausted count calls45bda0 stop,45b900 completion broadcast,434190(11,0),435450 in that order. The probe verifies next-point selection and endpoint order. Active-pointer clearing/control restoration remain covered by the separate lifecycle probe.

Otherwise a resolved path pointer at+820 enables three phases:

1. Pre-path timer+814 expires: invalidate it; clear elapsed float+854; set moving byte+858=1; restore camera position+824 from selected camera registry entry+4; initialize movement deadline+818 from point float+8.
2. While moving, elapsed += global5a4014 * constant5897b8. Call530060(path+8, destination camera position, elapsed / point.duration2).
3. Test movement deadline **after** sampling. Expiry clears moving and invalidates+818. Remaining total duration is post-path hold.

Point+4/+8/+12 therefore represent pre-path hold, path movement, post-path hold. Without a resolved path only total timer advances, though all three floats still contribute to total duration. Do not synthesize movement between unrelated cameras.

Immutable executable constants:5897b4 float982.7238159179688;5897b8 float0.9827237725257874;5893c0 float0.5. Point initialization uses duration*982.7238159+0.5 through573528 before timer4fa360. Tick uses elapsed increment above. These are not literal1000/1. Global5a4014 producer/units remain untraced, so do not invent a clock correction.

Probe prehold1/move2/posthold7: time999 no sample; at1000 movement starts with deadline2965 and fraction0.12284047 for frame-delta fixture0.25. Advancing directly to movement deadline samples again before disabling. This slice neither clamps fraction nor derives elapsed from absolute time. Late initial servicing starts movement deadline from servicing time. Total deadline10000 wins and selects next point with no path sample.

## Point action and tracking words: static recovery

- Point+16 is an optional look-at object handle. Tick45b7ee skips -1; otherwise48a4a0 resolves object, takes position+3c, applies48ac70, optionally substitutes entity+7d8 for vertical component, subtracts camera+824, normalizes4faaf0 and constructs orientation+830 through4fcea0. Exact48ac70 correction and the semantic name for+7d8 remain unproven.
- Point+20 is an optional point-start activated object handle. In45b3f0, descriptor-relative+1c+index*32 loads it;48a4a0 resolves it. Object kind+24 selects kind8:46aba0(object+2c,-1,-1), kind6:4b6760(object+2c,-1,-1), kind5:4c0220(object,-1,0,0). Missing/other kinds do nothing. Kind8 mover and kind6 event agree with existing dispatch; kind5 remains unnamed. These action/look-at branches are statically recovered, not executed in this probe.

Path resolver45b590 searches100-byte entries at645328 with count645324 through name comparison5001d0, returning match pointer orNULL. Literal authored `none` goes through ordinary lookup; no special literal branch appears. Path evaluation530060 and construction remain open.

## Later integration

Use three phases for resolved authored paths and retain static camera pose otherwise. Notify completion once after restoring control. A bounded active cutscene object can own indices/timers/FOV and borrow immutable camera/path records with no per-frame allocation.

Before path implementation: recover camera/path reader and530060 endpoints; execute point-start kind and look-at cases; trace5a4014; distinguish skip/cancel from natural finish. Add zero-move-duration with path, zero-total point, missing camera/path, long stall and point action changing scene cases. No playable campaign or final interpolation is claimed.
## Follow-on executed point-start evidence

`artifacts/future-campaign-re/cutscene_point_actions.py` now executes full45b3f0 for five target kinds: unresolved,5,6,8,other7. All pass; results `cutscene-point-actions.json`. Actual camera lookup45b230, array access, camera position/matrix copy, duration conversion and timer creation execute; object resolution and outgoing effects are intercepted. This upgrades the point-start dispatch above from static-only to executed boundary evidence, but does not execute the downstream effects themselves.

Kind5 calls4c0220(target pointer,-1,0,0); kind6 calls4b6760(target+2c,-1,-1); kind8 calls46aba0(target+2c,-1,-1); unresolved and7 do nothing. A(1,2,3) duration fixture at clock1000 yields total deadline6896 using the original conversion. Camera(4,5,6) and identity matrix copy exactly to descriptor+824/+830. Empty path-string fixture leaves path lookup absent. Named paths remain covered separately.
