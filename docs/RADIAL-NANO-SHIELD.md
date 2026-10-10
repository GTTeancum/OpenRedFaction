# Shared radial Nano shield damage

Independently source-reviewed and parent-integrated after the actual 09:00 UTC
batch against `5d55baeda849e221fa1278aa5b04b26c262317bb`. Awaiting the scheduled
10:00 Xbox compilation. No compilation, syntax checks, tests, fixtures or runtime
have been performed for this slice. Original inputs remain unchanged.

## Gap and evidence

The frozen shared blast loop called `rf_scene_npc_damage` after its existing
CF5 cover and positive body-position falloff. Its generic damage predicate
called `rf_entity_armor_immunity`, which consumed active Nano damage without
ever debiting shield armor. Direct-contact Nano consumption was already a
separate path and must remain separate.

Original RF.exe SHA256:
`b8fb9ab4c9bfc6f2868c30839d6cfc69f84b8c25d7e54eee1325f5b633c9b836`.
Read-only binary inspection established:

- `0x489034..0x48904a` runs a CF5 cover query from epicenter to physics+0xe4.
- `0x48905d..0x48907e` resolves the full object handle through
  `0x426fc0`/`0x40a0e0`, then calls `0x42cca0`. The shield is active only with
  class physics bit `0x02000000`, positive armor and actor+0x814 bit0x20 clear.
- `0x489084..0x48909d` and sphere helper `0x506fd0` require strict overlap
  between the epicenter and object+0x3c using float radius+1. The port's
  published position represents that object origin; its body-state position
  represents the different physics center used for cover and falloff.
  `0x506fe2` stores the radius sum as float; `0x4faf00` through `0x409fa0`
  stores each component delta as float at `0x409fb0/0x409fba/0x409fc4`, then
  `0x40a180` sums squared components with extended intermediates. The port
  retains those float stores with double accumulation, matching its existing
  bounded shared falloff convention without claiming bit-exact x87 boundaries.
- `0x4890a3..0x4890db` skips debit for object flag4; otherwise it subtracts
  `damage * (1 - distance(physics+0xe4, epicenter) / radius)` from armor and
  invokes `0x427df0` when armor is exhausted.
- `0x4890de..0x48910b` owns shield feedback and returns before the generic
  damage path. There is no health spill, class damage-factor scaling, ordinary
  armor split, generic retaliation or direct-contact Rail100 override here.

The inspection also established that the original radial shield branch does
not clamp signed outer-shell falloff to positive values. The bounded port
deliberately keeps its pre-existing positive shared-blast admission: body
centers at or beyond the ordinary blast radius receive no Nano debit, shield
feedback or armor increase. The radius+1 object-origin overlap is an additional
gate for admitted positive damage, not an expansion of the shared population.
Exhausted armor remains clamped to zero for the existing nonnegative vitals and
save contract. These policies are explicit limitations, not exact x87 parity.

## Consumer and ordering

`scene_nano_blast.inc` is included after the existing shared blast amount
helper. Its consumer takes the slot and captured full handle, resolves them
through `scene_ai_melee_npc_identity`, requires an entity-type owner and validates
its class before consulting the same active-Nano predicate. This qualification
checks the registry wrapper, entity view, damage handle and allocated body.
No new pointer, owner field, pool, timer, resource or save version is added.

The shared actor loop still filters the same living visible NPCs and player,
queries the same CF5 cover to the same body position, and computes the same
positive unscaled linear falloff. Only then does its NPC branch call the new
consumer, before generic damage. Player damage and the vehicle, turret and
clutter populations are unchanged. Existing source policy, blast callers,
optional pool-epoch guards and direct-victim participation are unchanged.

An active Nano owner consumes the pulse even when the strict object-origin
overlap rejects its debit. Object flag4 consumes it without changing armor.
Otherwise the already-computed positive body-center amount is deducted once.
An exhausting debit invokes the existing `scene_capek_shield_break` before
publishing zero armor. Failure returns without armor publication; successful
actual Capek break keeps the previously implemented fall, explicit break
history, restored authored base speed and delayed live-speed refresh. Other
Nano actors retain that hook's existing no-op movement policy.

Every consumed pulse skips generic health, damage effects, animation cancel,
combat health-hit accounting and death entry, including the breaking pulse.
It does not populate direct-contact Nano counters. Shield hit/break visual and
audio feedback remain deferred rather than being invented here. Shield-OFF,
armor-zero and non-Nano owners continue through the previous generic path.

## Direct Big Spit interception stays terminal

`scene_ai_spit_contact` still dispatches the direct consumer exactly once and
returns immediately when `scene_ai_laser_contact_result` reports a physical
shield, Nano or stale-actor consumed result. A direct Nano hit therefore still
suppresses the whole independent radial pulse, including on shield break.
This slice changes only a Nano actor reached by a radial pulse that was already
admitted, such as a nearby actor after world or ordinary-object impact. Small
Spit's zero blast radius and ordinary expiry remain non-explosive.

## Delivery and remaining work

The parent integrated this after the completed 09:00 batch; the next scheduled
Xbox compilation is 10:00. No independent build, syntax/test command or runtime fixture
was run. Existing historical direct-contact Xbox results do not validate this
new radial consumer. Natural radial partial-debit, shield break, invulnerability,
OFF/zero fallback, published/body-center separation and later save continuation
remain runtime-unverified. Full original signed-shell behavior, shield feedback
and broader explosion-population fidelity remain deferred.
