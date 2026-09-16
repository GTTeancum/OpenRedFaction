# Debris floor threshold and terminal settling (2026-09-16)

`artifacts/future-vehicles-re/debris_floor_settling.py` executes original48f9f1 through the actual continuing/terminal endpoints for162 boundary cases. Results: `debris-floor-settling.json`. Original RF.exe SHA256b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836. No game launch, scene edits or builds.

## Exact threshold and order

Constant5895d4 is **0x3f333333**, binary32 **0.699999988079071**, exactly C`.7f`. Instruction48f9f1 compares contact normalY against this constant; only less-than (or unordered NaN) bypasses the floor branch. Equality qualifies. The current scene threshold is correct; do not tune it.

For qualifying floor contacts,48fa05..48fa13 decrements signed chunk+64 and ORs chunk+78 with2, preserving other bits. The branch then performs optional impact aggregation, **before** checking whether decremented count is positive at48fa91. Count>0 continues to the normal impulse tail48fac9. Count<=0 assigns chunk age+70 from lifetime+74, optionally retains the contacted detail-owner pointer at+68, and returns without executing any bounce/coefficient/cone/spin RNG.

The original leaves position and velocity unchanged in this terminal branch. Scene's zeroed velocity is an implementation convenience only while terminal chunks remain excluded from subsequent physics. Do not use it as evidence for an original damping rule. A pre-existing zero counter would decrement to−1 if original48f900 reaches this branch; the scene's earlier zero-counter skip avoids re-entering terminal physics.

## Original boundary vectors

Each normalY bit pattern was tested with incoming count0/1/2, speed0/2/3, detail byte0/1/2 and owner byte98 zero/nonzero. Other chunk flags begin0xa4; age.25 and lifetime2.5; velocity must remain exact at the admission/terminal boundary.

| NormalY bits | Value | Incoming count | Count after | Flags | Age | Route |
|---|---:|---:|---:|---:|---:|---|
|3f333332|0.6999999284744263|1|1|a4|.25|continue impulse|
|3f333333|0.699999988079071|1|0|a6|2.5|terminal, zero bounce RNG|
|3f333334|0.7000000476837158|1|0|a6|2.5|terminal, zero bounce RNG|
|3f333333|0.699999988079071|2|1|a6|.25|continue impulse|
|3f333333|0.699999988079071|0|−1|a6|2.5|terminal, zero bounce RNG|

## Retained detail owner and aggregation

Actual4e36a0/494a50 execute in the oracle, without predicate stubs. Terminal settling stores face+44 into chunk+68 only when face+44 exists, its byte0 equals **exactly1**, and its byte98 equals0. Existing `GEOMOD-ELIGIBILITY-DETAIL-COLLISION-20260915.md` identifies this as breakable-detail/glass eligibility; this is not proof of generic moving-platform attachment. If predicate fails, original leaves the previous chunk+68 unchanged. Detailbyte2 fails even though it is nonzero.

Qualifying floor contacts with speed squared strictly above4 aggregate impact count/position and speed before the terminal check (both continuing and final impacts can contribute). The fixture's dry room proves speed2 is excluded and speed3 included. The code skips this aggregation when the room indicates liquid and original4ce080 confirms submerged; that underwater branch is statically inspected here, not executed. Beyond speed squared100, contribution caps at10; this cap is not part of the162-case matrix.

## Integration assessment

Current `.7f` and decrement-before-helper terminal ordering require no correction. Future meaningful gap is retaining settled chunks' breakable-detail owner so its later destruction can invalidate/release the resting fragment; this report does not yet establish the consumer of chunk+68. Keep that separate from the now-verified continuing-bounce implementation.
