# Rifle automatic alternate fire

Live integration (2026-09-15). The assault rifle now supports its authored
alternate trigger through shared PC/Xbox controls, damage and ammunition paths.
Primary fire still emits three-round bursts. Alternate fire emits one round
every0.1seconds while held, with the authored4-degree spread and60damage using
the armor-piercing damage kind. Player primary-fire spread remains separate
completion work; no broad weapon-fidelity claim is made here.

Evidence: installed `tables.vpp/weapons.tbl`, Assault Rifle declaration:
`alt_fire`, `alt_continuous_fire`, `$Alt Fire Wait:0.1`, `$Alt Spread Degrees:4.0`,
`$Damage:60`, `$Burst Count:3`, `$Burst Delay:0.1`, `$Burst Alt Fire:false`.
The existing parser supplies damage/timing values; absence of an alternate
amount defaults to primary damage. These are SP values.

Switching a pending primary burst to alternate cancels its remaining rounds
while retaining the cooldown. Primary input wins if both triggers are held.
These handoff choices are first-pass policy, not verified original dispatch
ordering. Alternate spread has an independent deterministic RNG, reset per
section; it does not change existing enemy or shotgun RNG streams. Every emitted
round consumes ammunition and uses the existing living-target/obstruction test.
Audio uses the rifle firing sound, not Riot Stick effects.

The previously omitted `fp_aslt_rfl_fire.rfa` loop is loaded and played without
restarting it for every bullet, then stopped on release, depletion, death or
primary fire. Its5152bytes fit a bounded8KiB extension to the rifle's previous
1MiB resource allowance. Other first-person weapon allowances are unchanged.
This is an explicit resource budget, not unrestricted memory growth.

## Checks

`python tools/replay_rifle_alternate.py` stages at authored rifle3415 in L4S5,
then uses normal pickup and weapon cycling, with no inventory injection:

| Case | Alternate rounds | Total rounds | Remaining magazine | Final animation |
|---|---:|---:|---:|---|
| Held for60ticks |10|10|32|Alternate loop|
| Released after60ticks |10|10|32|Idle|
| Primary burst interrupted by alternate |3|4|38|Idle|
| Alternate followed by primary burst |5|8|34|Idle|
| Both triggers held |0|6|36|Idle|

Each case kills the fixture guard and leaves the player alive. Checks reject
Riot Stick/shotgun side effects and firing/resource errors. The resource test
also verifies the actual loop for180ticks and an explicit return to idle.
Primary rifle burst, switching, burst cancellation and shotgun regression
fixtures are run separately.

Native `artifacts/xemu/render-20260915-005603`: PASS120frames, all32 selected
PC/Xbox comparisons including all8 rifle-alternate words. Ten alternate shots,
three hits and one kill,32rounds remaining, alternate loop still active.
Free pages:6326 (24.7109375MiB) on stock64MiB Xbox.
Rifle resource resident1042088bytes, peak1053684bytes, below its1056768-byte cap.
All18 disc entries restored and the owned emulator closed.

The journal confirms hits at frames60,66,72 in the held/released fixtures,
checking the actual six-tick cadence as well as total ammo consumption.

## Campaign limitation

The L2S3 shaft probes still stop at the upper guard. Raising the rifle before
clearing the doorway hits ceiling geometry; an attempted edge route remains
blocked by the doorway before the player reaches the main ladder. The lower
pit guard is not the source of the fatal shot in that route: the damage journal
identifies upper guard1773. Automatic fire is implemented, but the shaft route
still needs a verified combat/approach sequence. No health, inventory or enemy
damage adjustments were used to bypass it.
