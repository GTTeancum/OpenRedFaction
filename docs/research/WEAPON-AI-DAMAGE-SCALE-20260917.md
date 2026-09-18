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
+118/+11c is not read here. The pair's meaning and selection/interpolation into
+120 remain unverified; no SP/MP label or direct pair[0] application is justified.

`python -B tools/verify_ai_damage_scale.py` executes the absent-tag assignment
and80 getter combinations using Unicorn. Original arithmetic and mode branches
execute unchanged; owner lookup and player/controlled predicate are supplied
boundaries. Four mode combinations, two fire modes, two owner classes and five
runtime scales match exact float32 expectations. This is not full original
parser execution or live gameplay validation.

## Validation and remaining work

All123 PC tests and the stock-profile NXDK compilation/link/XBE/ISO build pass.
Logs: artifacts/ai-damage-scale-{build,tests,xbox}.log;
binary report: artifacts/ai-damage-scale.json. No XEMU gameplay run or screenshot
was needed for this metadata change. Existing actor/fragment damage remains10.

Next: recover runtime scalar selection, integrate authored NPC damage consistently
for actor and fragment targets, then qualify actual NPC firing against rubble.
Difficulty scaling, authored cadence and other weapon classes remain separate.
No fragment health threshold or wake behavior changed.
