# Flamethrower alternate canister first pass

The developer flamethrower alternate action now starts its authored animation, waits108 simulation ticks, releases a separately rendered powerup_flamecan.V3D, simulates gravity/collision while deselected, and explodes on contact (next tick) or lifetime expiry. The bounded pool holds eight projectiles; it performs no per-frame allocation. The first-person weapon remains separate from the projectile model. The four-second alternate cooldown inhibits primary/reload; interruption before release cancels the pending throw.

## Ammo and provenance

FLAME-CANISTER-AMMO-20260918.md records original42c310/42c362..42c386. On successful release the old loaded tank is replaced by a full magazine from reserve, including when the old tank was only partially filled. For insufficient reserve, this port explicitly installs the remaining gas; it never creates gas or negative reserves. A full projectile pool refuses release without an ammo debit. These low-reserve/failure choices are first-pass policy, not claims about original upstream admission.

Installed alternate damage100, blast radius7, release1.8s, cooldown4s, velocity10, collision radius0.051 and life10 are used. The adapter currently uses shared explosion damage/effects and contact detonation. The authored flame_can_explode recipe, fire-specific lingering damage, actual projectile orientation and finer timing remain refinement. The placeholder rocket effect uses visual radius2: applying the authored flame recipe radius8 to rocket smoke overwhelmed the screen. No GeoMod radius is invented because this alternate has no authored crater radius.

Save admission rejects pending throws and live canisters, which the current checkpoint format cannot preserve. Cooldown persistence and wider campaign saves remain open.

## Checks

Focused adapter check passes delayed release, debounce, cancellation, off-weapon flight/contact explosion, full/partial/empty-reserve replacement, and pool refusal without ammo loss. This uses real grenade flight with collision/effect stubs.

Actual PC replay canister.bin: one start/release/explosion, zero remaining flight, reserve1000->900 and loaded100 retained. Close blast reduces player health; inspected final output retains visible room and dissipating smoke after placeholder correction. Separate angled canister-flight.bin ends with one active canister, whose small world model is visibly in flight in the inspected frame. No original game or desktop input/capture used.

Native run artifacts/xemu/render-20260918-100259 passes80 comparisons over360 frames. Canister counters match1/1/1/0/0 (start/release/explosion/live/pool misses); ammo matches loaded100/reserve900. Endpoint available pages2430 =9.4921875MiB. Native framebuffer inspected: nearby explosion smoke, visible room, held weapon and reduced health agree with PC. Flight model was visually inspected on the separate PC replay, not a native mid-flight capture. Initial native build failed a misleading-indentation warning; fixed before this passing run.
