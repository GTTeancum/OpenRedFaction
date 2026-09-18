# AI damage inputs and original getter evidence

The shared C parser now retains both `$AI Damage Scale:` values in
`rf_weapon_primary_definition.ai_damage_scale[2]`, defaulting both to1.
Installed handgun/rifle/riot-stick tests verify(1,1),(.4,.4),(.1,.1).
Synthetic cases cover unequal values, zero, missing second value, duplicates,
negative/out-of-budget values, NaN/infinity and unchanged output on rejection.
The finite nonnegative upper bound1000000 is a port input guard, not claimed
original validation. The field adds8 bytes per definition without allocation.

## Original evidence

RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

Parser4c3b05 names the optional tag at5a2e0c. Present values are stored at
class+118/+11c at4c3b1c/4c3b29. Absent branch4c3b31..4c3b3d stores EBX
into both;4c3731 seeds EBX with float1.0 bits.

Getter4c8b10 resolves the owner through40a0e0 and calls predicate48aaf0.
Predicate result1 bypasses scaling. The predicate includes the player flag
and controlled-owner handling; the probe supplies its result. Primary/alternate
selects class+108/+110 when globals64ecb9 and6fc4d8 are both zero, otherwise
+10c/+114. Non-player branches multiply by **class+120**. The authored pair
+118/+11c is not read here. Normal setup4c2a20 copies class+118 to+120;
alternate setup4c2ac0 copies+11c to+120. There is no interpolation in these
routines. The pointer begins at table base85cd08+260 and copies negative offsets
-148 to-140 (normal) or-144 to-140 (alternate), then advances by550.
The table loader calls normal setup at4c68d9. The scene uses that first member.

`python -B tools/verify_ai_damage_scale.py` executes the absent-tag assignment
and80 getter combinations using Unicorn. Both complete table-selection routines
are also executed for counts0/1/3/64 with unequal pairs and sentinel entries
beyond each count:520 exact field/boundary comparisons. Only alternate setup
helper481580 is supplied as an unrelated scalar-return boundary. Original arithmetic and mode branches
execute unchanged; owner lookup and player/controlled predicate are supplied
boundaries. Four mode combinations, two fire modes, two owner classes and five
runtime scales match exact float32 expectations. This is not full original
parser execution or live gameplay validation.

## Validation and remaining work

All123 PC tests and the stock-profile NXDK compilation/link/XBE/ISO build pass.
Logs: artifacts/ai-damage-scale-{build,tests,xbox}.log;
binary report: artifacts/ai-damage-scale.json. The initial metadata change had no XEMU run. The follow-up integration uses
normal authored primary damage in both actor and fragment paths. Handgun40,
rifle24 and riot-stick6 precede downstream target modifiers. Unsupported
classes retain10. A synthetic800*.5 control still verifies fragment retirement.

Next: qualify actual NPC firing against rubble on Xbox.
Difficulty scaling, authored cadence and other weapon classes remain separate.
No fragment health threshold or wake behavior changed.

The live PC actor8456 encounter uses ordinary NPC targeting and five handgun
hits to kill the stationary player within240 simulation ticks. This verifies
integration into live actor combat; it does not qualify a live rubble impact.
Integration logs: artifacts/npc-authored-damage-{build,tests,xbox,native}.log.

Native qualification: artifacts/xemu/render-20260917-222646 passes64 PC/Xbox
comparisons on stock64MiB. Five bullet hits, firing clip starts, audio-event
counts and player death match exactly. The final framebuffer was inspected:
textured mine corridor, armed NPC, death/respawn overlay and zero-health HUD
are present. Audio was disabled in the emulator, so audible quality is not
verified. Endpoint free memory5268 pages (20.578MiB); disc restored and the
owned emulator closed. This run contains no live NPC rubble impact.

Reproduce PC acceptance with `python -B tools/check_npc_authored_damage.py`.
The replay uses neutral input and verifies no liquid-damage contamination.
