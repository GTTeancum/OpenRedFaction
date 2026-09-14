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
