# Scripted attacks: first playable implementation

Attack event 38 now calls a scene-owned combat backend on activation and cancellation, including delayed activation. This is a practical shared PC/Xbox implementation, not a reconstruction of the complete original AI state machine.

## Source evidence

For RF.exe SHA256 b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836:

- Loader 462150 calls 4b86d0 for type 38; 4b86d0 stores its numeric argument at event +2b8.
- Factory 4b69d0 constructs the event through 4beae0, whose vtable is 589b1c.
- On action 4bcac0 resolves the +2b8 UID via 48a4a0, obtains the actor via 426fc0, and selects the first resolvable linked target. It resets AI through 408ac0 and assigns the target through 409050. A matching player name overrides that target with the player.
- Off action 4bcba0 clears the target through 409050(-1), then calls 407ee0.
- Installed L1S1 event 9394 has words[0]=8638 and target link UID 9391. This is the attacker/target direction used by the implementation.

These are static decompile/disassembly observations. Original AI consumers have not been execution-equivalence tested. The exact case sensitivity of the original name comparison remains unverified; the current backend accepts literal `player`.

## Runtime behavior

The scene resolves the attacker from its authored UID, chooses a live supported NPC/player target, and retains an explicit target handle. Cancellation, allegiance changes and respawn clear that state. The existing basic enemy firing loop checks geometry obstruction, range and cooldown, then routes damage to that target. NPC lethal damage enters the existing death path. A stale or dead explicit NPC target cannot silently become the player. Commanded movement stops when an explicit attack starts.

No attack command allocates memory; ownership adds two 32-bit fields per NPC. Other target families, pursuit, aiming/firing animation, weapon-specific NPC damage/rates and NPC-source retaliation remain unfinished. Existing basic firing uses ten damage and a sixty-tick interval. The world combat loop still pauses when the player is dead.

## Evidence and limits

All 37 CTest tests pass. Added coverage includes actual L1S1 event9394 dispatch, delayed on/off, missing-backend retention, ordered target selection, player override, cancellation, nonfatal NPC-to-NPC damage, cooldown and stale target handles. These tests do not demonstrate a full natural-trigger encounter or lethal scripted-shot presentation.

The NXDK build passes. XEMU replay `replay-20260914-105213` passes the stock64MiB 180-frame L1S2-to-L1S3 crossing and state checks. It is a compatibility check for the changed build, not proof that an authored Attack sequence played correctly. No capture was requested. Local evidence lives in `artifacts/scripted-attack/`; original analysis exports remain untracked.

## Pursuit first pass

Scripted armed attackers now request movement when their target is farther than twenty world units or their sight line is obstructed. They keep pursuing until a clear sight line is available inside sixteen units; the gap prevents rapid start/stop changes near one threshold. Decisions refresh every fifteen simulation ticks, independently of the shooting cooldown. These thresholds are practical first-pass choices, not recovered retail constants.

Pursuit feeds the existing navigation, ground support, steering and body-sweep path. Its destination follows the selected NPC body or the player body. Movement beyond one horizontal unit updates that destination and invalidates the retained route; smaller movement keeps the route and retry state. No new navigation allocation is introduced. Two additional words per NPC retain the decision deadline and a deferred stand request.

Combat pursuit owns follow mode2; authored Goto_Player keeps mode1. Cancelling or invalidating a pursuit clears its route and requests standing on the next live actor update. A new authored Goto command supersedes explicit combat targeting. Allegiance changes and respawn cancel combat-owned movement.

PC tests cover distant and wall-obstructed targets, destination/retry updates, clear-sight stopping, stale handles, walking-to-standing cancellation, and preservation of unrelated Goto_Player movement. They verify decision-to-movement handoff, not a complete character navigating an authored encounter. Full-route pursuit, doors/obstacles during pursuit, aiming/firing presentation and natural encounter validation remain open.

The NXDK build and stock64MiB XEMU replay `replay-20260914-105927` pass the 180-frame L1S2-to-L1S3 transition/state checks with pursuit enabled. No capture was requested. This is build compatibility coverage, not full authored-pursuit validation. All37 PC tests pass; evidence is in `artifacts/scripted-pursuit/`.
