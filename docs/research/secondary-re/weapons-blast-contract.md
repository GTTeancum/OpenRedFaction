# Ordinary radial blast damage: implementation handoff

The current launcher damage prototype should measure linear falloff from one victim physics position and cast one cover ray to that point. Its current nearest-visible-sphere-surface calculation changes both damage and cover behavior. This is actionable ordinary-actor evidence, not a claim that all blast behavior is reconstructed.

## Reproduction and evidence boundary

From `D:/Programming/GitHub/OpenRedFaction`:

```powershell
python -B tools/future_re/secondary/weapons_blast_contract.py
```

Result: **PASS: 22 victim cases, 6 cover-wrapper cases, 6 admission cases**. Output: `artifacts/secondary-re/weapons/blast-contract.json`. Original executable SHA256: `b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836` (`Installed_Game/RF.exe`). The probe runs PE instructions in process-local Unicorn; it never launches the game, builds shared sources, uses an emulator, or touches desktop input.

Victim cases execute complete `0x489010` through return, including vector subtraction `0x409fa0`, length `0x40a000`, ordinary-class shield predicate `0x42cca0`, and damage-call setup. The handle registry is supplied. `0x498e80` cover result is supplied and its arguments recorded. `0x4892c0` is a recorded damage-service boundary returning x87 zero; actual health, armor and reactions are not executed. Fixtures set ordinary class flags, no shield, no player camera feedback and no skeletal hit-direction path. The actor has life100, object position(100,200,300), object radius99 and physics radius88, while the independently supplied physics position varies near origin.

Cover cases separately execute complete original `0x498e80`, including real CF conversion `0x499190`, mover broad-phase/intermediate vector math, any-hit short-circuit and world query setup. Only `0x4df1c0` geometry services supply hit counts. Identity mover transforms and synthetic bounds are supplied. **These establish query construction and routing, not actual level visibility, collision filtering correctness, transformed campaign mover coverage or visual results.**

Admission cases execute `0x488dc0..0x488ded`, stopping before list processing for accepted requests and at ordinary return for rejected ones. No population traversal is claimed executed.

## Executed contract

| Boundary | Result |
|---|---|
| `0x488dc0..0x488ded` admission | Damage must be positive; radius must exceed stored float `0.10000000149011612`. Equal0.1 is rejected. |
| `0x489034..0x48904a` cover | Call `0x498e80(epicenter, victim+0xe4, 5, NULL)`; any nonzero low-byte result suppresses damage. |
| `0x489050..0x489058` | Compute vector `victim.physics_position - epicenter`. |
| `0x48910c..0x489130` | Compute `gain = 1 - length(vector)/radius`; only positive gain calls damage. |
| `0x48913d` | Store `damage * gain` to float. No victim radius is subtracted. |
| `0x489283..0x48929a` | Ordinary SP damage arguments: victim handle, attenuated amount, source handle, weapon type -1, supplied damage type, NULL position, killer UID -1, flags0. |
| `0x498e80` + `0x499190` | CF5 becomes mover GCF0x41; stationary query OR4 gives GCF0x45. A mover hit returns immediately without querying stationary geometry. |

For authored rocket damage400/radius5, distances0/1/2.5/3/4 produce400/320/200/160/80 in the executed probe. At distance5 and outside, no damage call occurs. At the adjacent float below5, the amount is0.00003814697265625. Blocking the single physics-position ray suppresses the damage even when other object fields indicate very large spheres.

The executed x87 instructions retain intermediate precision in Unicorn until their explicit float store. A host C implementation that rounds every arithmetic operator to float can differ in low bits (distance4, radius5 yields79.99999237060547 under all-float intermediate rounding, while this original-instruction probe produces80). Use double intermediates and final float conversion for the supplied vectors. Hardware x87 precision equivalence outside these vectors is **not** established by Unicorn; PC/NXDK helper comparison remains implementation validation work.

## Static findings and field identity

The retained community headers label `object+0xe4` as physics position; that name corroborates the executed offset and was used only as a read-only address/layout guide, with no third-party source copied. Port `rf_physics_body.state.position` is the intended corresponding state. Object `+0x3c` separately enters feedback direction code later in the original; it is not the executed falloff point.

Static disassembly of the outer `0x488dc0` also constructs epicenter +/- radius bounds, traverses multiple object lists and tests bounds overlap before calling `0x489010`. Its entity list additionally checks `0x4290d0` host predicate and `0x40a110` flag0x4000 predicate. Clutter/other lists and damaging projectiles have different admission. These populations, their runtime object semantics, shield path, multiplayer modifiers, damageable clutter and player feedback are outside the completed probe. Do not claim the current actor-only scene list implements the entire original radius-damage dispatcher.

## Concrete integration steps

1. Add a small allocation-free ordinary blast amount helper to `include/rf/weapon.h` / `src/core/weapon.c`: finite epicenter, finite victim physics position, positive damage, radius>float0.1, one positive linear gain, final float amount. Proposed helper and acceptance vectors live beside this report's probe; public invalid-input rejection is port policy, not original malformed-input behavior.
2. Replace the sphere loop in `src/diagnostic/scene.c:9687` (`scene_blast_amount`) with one ray from the blast origin to `body->state.position`. Use `rf_geometry_collision_ray(s->collision, &campaign_movers, origin, body->state.position, 5, NULL, &blocked)` directly or a specifically named blast wrapper. Keep `combat_shot_obstructed` at `scene.c:8151` unchanged because its CF0x27 is deliberately bullet-specific.
3. Existing `src/core/geometry.c:1224` exposes the correct CF-taking query boundary. Passing5 there allows existing conversion to produce mover0x41/world0x45. Do not pass native GCF0x45 into a CF API. Compared with current bullet CF0x27 (GCF0x461/0x465), blast lacks0x20 and0x400 filter bits; do not silently preserve bullet-only cover filtering.
4. In `scene_rocket_blast` (`scene.c:9708`), retain damage dispatch through existing shared player/NPC damage services, source attribution and explosive kind3. The existing normal-based0.01 epicenter bias is a separate prototype decision: this probe does not prove that bias. Do not modify it based solely on the falloff work.
5. Before rollout, compare helper amounts on PC/NXDK against the supplied vectors, exercise one fully occluded physics point with an exposed sphere and the reverse case, and verify CF5 on both world and mover geometry. Run actual damage integration for self/NPC, fatal/nonfatal and ordinary armor; rerun existing rocket scenarios because lower self damage is expected. These checks remain open, not passed by this research.

Storage cost can be zero retained bytes beyond stack temporaries; replacing up toN sphere rays with one ray also reduces blast work under the stock64MiB constraint. No source implementation or gameplay completion is claimed here.

## Reviewable C sketch and acceptance vectors

This sketch belongs to the handoff only; it has **not** been compiled or integrated. The result pointer and finite/overflow rejection are explicit port API policy. Preserve the caller's single cover-ray decision; this helper only computes the ordinary amount. The explicit float vector stores mirror `0x409fa0`; double expression evaluation followed by one final float cast matches the supplied original-instruction results without assuming each operator rounds to float.

```c
#include <float.h>
#include <math.h>

static int ordinary_blast_amount(const float origin[3],
    const float physics_position[3], float damage, float radius, float *out)
{
    float delta[3]; double squared = 0.0, gain, value; unsigned k;
    if (!origin || !physics_position || !out ||
        !isfinite(damage) || !isfinite(radius)) return -1;
    for (k = 0; k != 3; ++k) {
        double d;
        if (!isfinite(origin[k]) || !isfinite(physics_position[k])) return -1;
        d = (double)physics_position[k] - origin[k];
        if (d > FLT_MAX || d < -FLT_MAX) return -1;
        delta[k] = (float)d;
    }
    if (damage <= 0.0f || radius <= 0.1f) { *out = 0.0f; return 0; }
    for (k = 0; k != 3; ++k) squared += (double)delta[k] * delta[k];
    gain = 1.0 - sqrt(squared) / (double)radius;
    value = gain > 0.0 ? (double)damage * gain : 0.0;
    if (!isfinite(value) || value > FLT_MAX) return -1;
    *out = (float)value;
    return 0;
}
```

All following vectors use origin(0,0,0), damage400, radius5. The JSON contains complete raw damage words and recorded cover arguments for each executed case.

| Victim physics position | Cover supplied | Expected amount / damage call |
|---|---|---|
| (0,0,0) | clear | 400 / one |
| (1,0,0) | clear | 320 / one |
| (2.5,0,0) | clear | 200 / one |
| (1,2,2) or (-1,-2,-2) | clear | 160 / one |
| (4,0,0) or (0,0,4) | clear | 80 / one |
| (4.999999523162842,0,0) | clear | 0.00003814697265625 / one |
| (5,0,0), (5.000000476837158,0,0), (7,0,0) | clear | 0 / none |
| Every listed position | blocked | 0 / none |

Caller sketch: validate the request, issue the CF5 ray once, return amount0 when blocked, otherwise compute the ordinary amount. For exact original side-effect ordering, cover precedes the victim distance test; the outer request admission precedes both. Keeping the single ray before falloff also makes cover telemetry directly comparable to the executed trace. Do not claim the C sketch's overflow handling, invalid-input preservation, or all hardware float results are original behavior.
