# Code-explosion impact direction and scale (2026-09-15)

**Actionable visual mismatch:** the live rocket central emitters retain their authored upward direction. The original replaces that direction with the impact/explosion normal for every central emitter before creating it.

## Executed evidence

`tools/verify_code_explosion_direction.py` runs isolated original instructions from RF.exe SHA256 `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.

- `4c1a02..4c1a97` executes the code_explode branch with the actual vector helpers and liquid predicate4ce080; only the child explosion48e640 call is recorded. 36 cases cross stored scale-1/0/1.5, supplied normals+x/-z/null, and dry/below/on/above liquid surface.
- Supplied normals copy unchanged; null chooses(0,1,0). Scale passes unchanged, including zero and negative values. This is not parser validation evidence.
- A room marked as containing liquid passes explosion flags1 iff the impact is at or below room+0c plus liquid-depth+188. Above-surface or dry cases pass0. In recovered48e640, bit1 suppresses trails; it does not suppress all explosion particles.
- `48e900..48e91e` executes actual central definition direction assignment and its vector-copy helper, with no hooks. Three cases(+x,-z,arbitrary vector) replace initial(0,1,0) at emitter definition+10 with the explosion normal exactly. The later emitter constructor normalizes direction separately.

All39 cases pass; results are in `artifacts/crater-shading-re/code-explosion.json`.

## Recommended implementation

In `scene_impact_start`, use the contact normal for central emitter template direction instead of `p.direction`; preserve the existing emitter constructor's normalization. This should be visible for wall/ceiling impacts, particularly the directed fire emitter, and is an original behavior rather than cosmetic invention. Verify a nonhorizontal impact and its cleanup on PC/Xbox after integration.

Keep liquid flag propagation as a separate open feature if the current DEV path never has liquid. Do not apply arbitrary radius fallback inside code-explosion dispatch; the original forwards stored scale, although invalid input handling can remain stricter at the public API boundary.

## Limits

The branch fixture supplies a valid room, decoded vclip flags/name and effect scale. It does not execute the complete impact creator or child explosion, test attachment or light lifetimes, establish omitted-radius defaults, or prove full blast visual parity. No source code or emulator state was changed by this research.

Primary integration: central directions now use contact normals; verified with
PC selected-frame captures and native render-20260915-191220 (47 comparisons).
