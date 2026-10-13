# Ordinary combat repositioning and RFNC38

## Status

Source implementation completed October 13, 2026. Commit 2ca9872ddaabfcba2829590f79cdf145a3316334 passed the parent stock-64-MiB Xbox compile/link/XBE/XISO and short startup smoke at 03:03:43 UTC. The smoke observed 12 simulation frames and 10 presentation frames, followed by acknowledged QMP quit and clean exit 0. No functional or save/load test result is claimed; testers own that coverage.

## Behavior and source evidence

Ordinary grounded, armed humanoid Direct and Evasive actors can choose a short local route while already fighting an in-range, visible target. The immutable cohort uses actual class physics, placed attack style, ordinary Waiting/default-patrol source ownership, standalone model ownership and authored stand/walk mappings. Stand Ground and vehicle, turret, attached, held-clutter and special creature classes are excluded. Source inventory found 831 static candidates across 64 installed levels, including 22 linked Attack owners across nine levels; these are source counts, not runtime coverage.

Original 405af0/4061fb and 406cc0/406ede dispatch the stationary/relocating tactical clock separately from fire cadence. 408ac0 sets an inclusive 2000–4000 ms initial interval. 406820 finishes relocation, installs the next 2000–3999 ms interval (3000–4999 when existing actor7bc equals one), and installs an independent 500 ms new-primary gate checked by 4016e1. No crouch-policy mutation is added.

Original 4cecd0/40b520 and 504db0 choose among ordered admissible neighbors followed by the current region, using the original 1/32768 random multiplier. Two-node routes stay in the selected current region; neighboring choices retain four nodes, including an authored self-neighbor when selected as an adjacency. Ground Y comes from the prepared navigation query point. The 4025d0 clearance calculation retains its XY-only projection numerator, XYZ denominator/distance and float stores. A candidate needs squared clearance at least nine. The original 4020c0 player-facing predicate uses the retained authored player-class FOV cosine and the target's eye-forward basis.

## Bounded integration

- One due actor per simulation frame, a persisted round-robin cursor and at most four samples per selected actor. Start-region search is prepared once for those attempts. Existing graph search storage is reused serially.
- Private per-owner CRT state avoids changing weapon spread randomness. No new per-frame allocation, asset bank or graph copy.
- An exhausted attempt or temporary motion-capacity miss installs a private 200–500 ms retry. Losing range/LOS consumes that frame's selection opportunity without performing route samples.
- Actual selected ranged weapon and existing native active-primary ownership are checked. Foreign active-primary bits are refused rather than cleared through fabricated callbacks.
- Full owner, target, current weapon and receipt are rechecked after visibility, resource-demand and navigation boundaries. Route publication itself is assignment-only, with embedded endpoint addresses and graph ordinals validated first.
- Translation uses the existing collision-aware ground, slope and wall-slide path. A typed local route faces its combat target while following its route. Completion, expiry or exhausted blockage returns it to stationary combat. A typed route never falls back to general pursuit search when its retained geometry is empty.
- Paid delayed services, secondary attacks and reload completion keep their existing ordering. The independent tactical recovery gate blocks only new ordinary primary admission. Explicit one-shot commands retain their existing path.

## Orders and lifecycle

A newly accepted explicit Attack restarts the tactical interval, including the same target. Its prepared receipt preserves existing private RNG and any independent primary recovery; same-target Blind Pursuit age is not refreshed. Incidental same-target retaliation and autonomous Alarm refresh retain the current tactical cycle. Real target/order replacement closes the previous phase immediately.

Goto publication, actual competing route replacement and combat release retire the tactical phase while preserving independent recovery. Temporary hiding, disarming, unsupported held weapons and stronger temporary service suppression do not erase a retained receipt. Death and successful section teardown clear it. Paid order-coupled controllers defer timed relocation retirement until their ordinary service completes.

## Persistence and protected publication

RFNC38 adds a four-byte component scheduling cursor and a conditional tactical tail: four bytes when absent, 24 when present. The tail holds phase, remaining phase time, recovery presence/remaining time and private RNG. Existing combat target identity and move geometry own their existing wire fields. A canonical recovery-only receipt has no tactical target or phase and does not require a currently selected ranged weapon. RFNC1–37 remain readable; their absent tactical state is initialized only by later ordinary service.

Capture is read-only. Candidate admission joins source/style, real target identity, registered living owners, on-foot player/seat relationships, exact local route shape, adjacency, embedded endpoints and finite geometry. Selection-time target clearance is not recomputed against a target that may have moved since the route began. Stand/walk and existing active motions share the existing bounded resource planner.

Outgoing or incoming tactical presence selects the existing protected publication lane before resource demand. Prepared target and deadlines, raw receipt, outgoing owner snapshot, scheduling cursor and player cosine are sealed and rechecked before the native close. Incoming receipts and cursor are assigned only at final successful load completion. No callbacks, RNG draws, route selection or capture-time repair occur in final publication. Section history gains no perpetual combat owner. Component-only restoration refuses tactical ownership rather than bypassing composed joins.

## Memory and unchanged caps

All allocations charge actual sizeof. Xbox32 compile-time upper bounds are source-reviewed limits, not measured compiler layouts: live NPC 3160 bytes, decoded row 2376, restore entry 10648, restore header 72, seal row 2640 and seal header 9328. Baseline estimates were respectively 3128, 2348, 10544, 64, 2592 and 9312 bytes. A build must fail if the final layout exceeds these bounds.

For N live NPCs and R copied rows, conservative incremental overlaps are:

- Resource-inspection decode: 32N + 28R + 12 persistent bytes, plus a local four-byte cursor.
- World decode/prepare: 32N + 132R + 20 bytes; decoded rows are freed before sealing.
- Sealed restore: 32N + 152R + 36 bytes.

Persistent additions are eight scheduling bytes and four player-class cosine bytes. Restore-entry growth includes two enlarged decoded records and the enlarged outgoing NPC snapshot. The seal-header allowance includes the enlarged stage-header copy, cursor and cosine. Existing copied row locals grow 28 bytes each; existing whole-NPC locals grow at most 32 each. New helper locals are bounded receipt/candidate/endpoint copies, with no whole-NPC copy; full compiler stack usage is not measured.

Existing 2240-byte row, 110524-byte file, 640-KiB NPC constructor, 2-MiB world-stage and 1280-KiB motion-cache limits are unchanged. Capacity refusal preserves the previous save. Motion admission can postpone optional local movement when the fixed cache cannot accommodate it.

## Deliberately deferred

Full directional strafe/crouch tactics, lost-target search, additional special-class combat, vehicle behavior and functional campaign/save verification are outside this slice. The one-actor scheduler, private RNG, existing walk animation, bounded retries and ordinary collision movement are explicit first-playable adaptations, not claims of cycle-exact original AI parity.
