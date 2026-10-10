# Spike TriBeam: prepaid delayed direct-energy primary

Status: source integrated after independent review; awaiting the 05:00 Xbox build. No compilation, syntax checks, runtime replay, new fixture, route progression, screenshots, grants or original-input changes were performed. The scheduled parent-owned Xbox batch remains the first compilation opportunity. This document records implementation and original source evidence, not a gameplay pass.

## Missing original consumer

Read-only original level decoding found selected `TriBeam Laser` on these actual `Spike` owners:

- L7S4.rfl:10810,10878,11127.
- L9S4.rfl:977,978,1140.
- L7S1.rfl:5034.

The source worker verified class, primary, health150, friendliness0 and creation flags0. Secondary entries are none or empty. Existing actual NPC loadout acquisition supplies the selected weapon and power-cell reserve. This slice adds no synthetic weapon, ammo grant, placement edit or substitute hitscan.

The conventional primary parser intentionally does not represent this delayed clipless finite-energy family. The dedicated exact profile preserves its separate contract rather than widening Laser, firearm or explosive admission.

## Exact authored profile

weapons.tbl1838–1867 establishes:

- Name TriBeam Laser; projectile spikeprojectile.vfx; power cell maximum200/200, zero magazine and no reload.
- Energy damage kind5, damage225, AI range30/30, speed30, collision radius0.2 and lifetime20 seconds.
- Fire wait5 seconds; one Impact Delay2.5 seconds; one projectile at release. The weapon's name does not mean three shots or three damage applications.
- Zero damage radius, no homing/sticking/glow; reset flags0 and flags2 undeviating/no_fire_through0x28.
- No explicit AI spread, AI damage scale, projectile count, burst, piercing, alternate delay or clip override. Existing zero AI-spread/default scale1 policy is retained, without claiming the undeviating bit proves that default.
- Launch Spike Attack; Fly Sound spike_attack_02.wav near8/gain0.9; Laser Hit material sound groups and tribeam hit Vclip are separate presentation metadata.

The profile reader requires the exact supported scalars, damage kind and identity, validates mapped supply/reset catalogs, and rejects unsupported mechanical additions. It never changes catalog indexes or save layouts.

## Acceptance, delay and release

Original nonmelee426197–426273 starts action2 and schedules ordinary cadence actor+4b8 and delayed launch actor+4c0. It resolves the positional Launch sound at4261e5–426225, with the original class+750 override path.426261 calls4257c0; the clipless branch42580e–425827 consumes the real reserve during accepted windup. This is not a sound-only delay around an immediate shot.

The implementation admits a live exact-owned primary, reserves one of64 paired pending/flight slots, prepares both deadlines and consumes one real cell. It then publishes cadence and a presentation reservation before fire action/audio callbacks. The five-second cadence and2.5-second launch delay both begin at acceptance. Presentation failure cannot create an immediate projectile or replay the shot; callback cancellation/reset cannot rearm a copied reservation.

Pool refusal occurs before acceptance/debit. Reserving the same flight slot is a bounded port allocation policy, ensuring no later pool-pressure loss of a paid shot. It is not an assertion that the original engine uses64 reserved slots.

Due handler409340 checks actor+4c0 at40939b, clears it at4093aa, and calls the projectile factory4c77a0 at40956b. There is no second debit, refund, range/cover gate or retry there. The implementation likewise consumes the pending row before its callback-free release preparation and never calls the immediate Laser debit path. Flight lifetime starts at release, not at windup.

The source must bypass generic ammo readiness and fallback while pending. Its final accepted cell legitimately leaves reserve0 for2.5 seconds; this cannot suppress the already-paid release. After release the ordinary fallback path may replace an empty weapon without changing the independent projectile.

## Due-time aim and source identity

Original4094fa–409526 recomputes the muzzle through41b040.40953a tests class+728 bit0x20; actual Spike lacks this alternate-aim bit.409557–40956b therefore passes actor+7e0 current eye basis to the factory, rather than the onset target ray or41b040's target-directed result.

The first pass launches from current owner eye position along current look.orientation forward, retains the full current basis for the projectile visual, and snapshots the accepted full source handle. It does not reacquire or home on a target. Precise Spike muzzle attachment is still an explicit approximation. Target handle at onset is diagnostic only; the pending owner has no target identity to accidentally redirect or cancel.

The source is requalified through current full registration, entity lookup, body and damage-owner identity, exact current primary and ownership. Source death/retirement, actual disarm/selection replacement or successful world/load reset cancels the pending release opportunity without refunding its already spent reserve. A released flight is independent of source retirement and cannot acquire the identity of a reused actor slot.

## Hidden, pain, scripts and holstering

Hidden deferral is positively supported upstream:41daf0 calls40a110, whose40a118–40a11e tests actor7c&0x4000;41db04 returns before41dbe2→4032e0→403320→409340. Hiding preserves the existing deadline. The first visible update releases once if overdue, without restarting the2.5 seconds.

Normal holster428f00 clears both delayed timers at428f89–428f9f. Class-primary reset425700 clears them at425780–425796. These are different from generic reset41ae70, called by pain at428837–428842 without timer clearing. Therefore the current port's generic NPC weapon-reset/pain callback must not receive blanket cancellation.

Authored Holster_Weapon4b9980 only sets flags810|0x800 and7d0|0x200; no timer clear was established. Due409340 has no target, pain, AI mode or order cancellation gate. This owner preserves paid windup across those changes. Source removal/death and accepted actual disarm/weapon replacement have explicit pending-only hooks; target retirement never cancels another source's windup. Hidden and retired/dead flags remain distinct.

## Shared direct-flight contact

The reviewed Laser preparation/sweep/contact primitive is generalized narrowly. Laser still owns its exact immediate positive-reserve admission and debit, profile,64-slot pool, counters and reset epoch. TriBeam has its own64-slot pool and counters, with damage kind5 carried immutably in each flight. Laser carries kind1 as before.

Both use existing finite straight-flight/liquid stepping and retained nearest-contact world, mover, detached-piece, actor, vehicle and clutter composition. TriBeam has no radial explosion, GeoMod crater, penetration, ricochet or invented three-beam fan. Physical shields keep the existing directional centerline approximation while world/body/environment sweeps retain radius0.2. Current NPC shield poses are prepared before query; parent supplies prepared player shield pose. Winning shield and Nano contacts consume the projectile, including shield-breaking contacts, before ordinary damage.

Full target identity is retained across contact callbacks. Terminal flight retirement precedes damage/effect callbacks, and per-pool reset epochs stop a stale update from publishing after timeline replacement. Save-only pending queries also cover active presentation/service/flight-update callbacks.

## Projectile visual and telegraph

The original projectile meshes.vpp/spikeprojectile.vfx is925 bytes, version0x3000e, SHA2566e91abb95eb5e3f14c89f1763be529ebf38170608ba0ec06ff9d3f6efe05a895. The source decode has one four-vertex/two-face FocusFlare parented to Scene Root, rate15, frames0–30,31 samples and definition duration31. Its material is SpikeFireFlare.tga in maps2.vpp,12332 bytes,64×64 RGB24. Mesh enabled byte0 does not invalidate the material-bearing instance. Existing bounded VFX/material machinery supports this projectile without broad old-format parser changes.

Foley1472–1473 maps Spike Attack to spike_attack_01.wav near8/gain0.9; sound metadata identifies it as nonlooping. It is requested exactly once at accepted windup, alongside action2, never at delayed release. Optional audio failure does not reject an accepted shot.

Separate warmup spiketribeam.vfx contains old DMMY hierarchy and animated texture behavior not admitted by the current projectile loader. This slice does not silently substitute that effect. Exact warmup visual, flight-loop audio, impact audio/Vclip and precise muzzle remain deferred. Fire action plus authored onset sound is the practical telegraph; the actual delay remains mandatory regardless of missing warmup visual.

## Saves and bounded scope

Pending windups and live TriBeam flights have explicit save-only rejection, not a new RFNC/RFAP layout. Successful load/world/life publication and teardown retire both; failed staging leaves the live state untouched. Shared NPC-row capture needed for load preparation must not accidentally inherit save-only rejection. Settled five-second cooldown continues through existing combat_due.

The parent combat hook consumes ordinary/opposed actor-directed TriBeam attempts. Point/once and vehicle-target script adapters are deliberately outside this first-pass admission, rather than being approximated by an unrelated actor. No runtime claim is made for final-cell release, hidden overdue release, current-pose aim, contact/cover/shields, source retirement, save refusal/restoration, resource admission, audio or drawing until separately validated.
