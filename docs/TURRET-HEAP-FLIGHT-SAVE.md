# HEAP paid-flight save continuation

Source-written against 1277996030a39f9cfb884c51a7ca890f1a4f2320 on October 12,
2026. This implements the existing reconstructed HEAP owner's checkpoint
continuation; it is not a reconstruction of the original RF.exe save format.
No compilation, functional test, runtime session, HDD access, fixture, image or
Git publication was performed. Parent owns integration and hourly compilation,
packaging and short startup smoke. Functional coverage belongs to testers.

## Implemented scope and original evidence

Ordinary composed-world saves retain all eight already-accepted HEAP flight
slots with their position, straight velocity, radius, remaining lifetime, raw
damage, blast/crater/impact sizes, authored collision host, player-or-host damage
source, and independent launch-time operator exclusion. Restore never launches
another shot, changes ammunition, advances cadence, reattaches an operator,
replays launch audio or dispatches damage.

Direct read of Installed_Game/tables.vpp's weapons.tbl HEAP record confirms
from_eye and undeviating, empty V3D/fly-sound names, explosive damage, radius 0.02,
speed 200, lifetime 5, fire wait0.8, damage 250, AI scale 0.2, player/AI spread 0/2,
blast 5, crater 4 and impact radius 2. Homing and sticky are false. The existing
owner keeps those loaded parameters; this slice does not change flight motion,
collision, damage or turret cadence. Existing source provenance remains
RF.exe 464010 for authored weapon overrides and 4c2a20/4c8b10 for the selected SP
AI damage scale. Shared projectile integration and appearance remain the
reconstruction described in TURRET-HEAP-FIRST-PASS.md and WEAPON.md.

## RFAP11 wire contract

RFWC3, the 16-byte RFAP header and 112-byte row size are unchanged. RFAP1–10 remain
readable with their previous policies, and capture emits those formats as before
when no HEAP flight exists. Only a HEAP row selects 11. RFEN8 is untouched.

Kind 25 encodes one paid HEAP slot:

- +0 kind 25; +4 pool slot 0–7; +8 positive authored turret UID
- +12 damage source role: 1 host turret, 2 local player
- +16 position,3 float32; +28 velocity,3 float32
- +40 collision radius float32
- +44 exclusion kind: 0 none, 1 local player, 2 authored NPC
- +48 positive remaining lifetime float64
- +56 raw damage; +60 blast radius; +64 crater radius; +68 impact radius, float32
- +72 excluded NPC UID for kind 2, otherwise zero
- +76–111 reserved zero

Player-sourced rows require player exclusion; turret-sourced rows permit either
none or an authored NPC exclusion. Duplicate slots, invalid roles, mismatched
role/exclusion pairs, invalid UID, reserved nonzero bytes or wrong payload reject
the entire component. No pointer, generation handle, current occupant, model,
weapon selection, inventory grant, native sound ID or partial contact is saved.

All flight values must match the independently admitted HEAP definition:
radius and raw effect scalars are exact; remaining time is positive and at most
the authored lifetime; positions are finite and bounded; velocity is finite and
retains speed within 0.001 relative normalization tolerance. Actual accepted bits
are preserved. There is no guidance, gravity, orientation or liquid-query owner
in the existing HEAP flight type to invent or serialize.

## Authored and candidate ownership

Capture proves exactly one authored-prefix turret with matching source UID,
seed/class mapping and full object/entity/view/damage registration. A dead but
still registered host remains a valid paid-flight owner. Generated heads lack
authored identity and refuse. Loading resolves that same authored identity,
then requires its exact UID, handle and class in the candidate RFTU stage.
Fresh constructor identity alone cannot authorize the saved flight.

An excluded NPC independently proves the existing RFAP6 full-registration,
body, authored positive UID, nonretired current-level persistence identity and
then joins exactly one registered, nonretired candidate RFNC entry by UID,
handle and authored slot. Registered corpses are allowed. Missing or retired
excluded actors refuse; no exclusion is silently discarded or rebound to a
reused registry slot. Local-player attribution and exclusion resolve the full
local-player role and join the staged living RFPL player.

Current seat links and current selected weapons are deliberately not conditions
on an already-paid shot. Dismount, ordinary registered death, a host weapon
switch or later operator reassignment do not replace its launch-time exclusion.
RFTU/RFNC and existing seat/player components separately validate their own
current candidate owners. Player-held-input, vehicle and ordinary player save
restrictions remain unchanged.

## Resources, callback boundary and publication

Kind 25 supplies NPC-only resource bit 23. An idempotent factored loader loads the
exact HEAP primary/explosive metadata when saved flights demand it, independent
of the current constructor's selected weapon. Existing bounded table parsers
and their 128 KiB scratch limits remain. The existing HEAP-present condition then
demands shared impact/blast definitions; no player view, projectile mesh, loop
sound, inventory item or persistent resource cache is added.

A four-byte stack-owned service-depth counter covers complete mounted and
autonomous turret control ticks, including launch callbacks followed by cadence
publication, and the complete HEAP flight/contact tick. Resets never clear it.
Save/export refuses while one of these intervals is in service, even if the last
shot has retired. Ordinary RFAP capture accepts settled pending flights; old
standalone player/NPC/composed exports retain refusal. Shared prepare/read does
not inherit outgoing live-state gates.

The sole HEAP publisher runs only after final successful world/storage close.
It resets the outgoing pool/frame diagnostics and copies the admitted paid
slots plus live count. Successful absent/legacy components leave an empty HEAP
pool. Preparation or failed storage close never publishes or clears it. Direct
in-session replacement of outgoing pending/in-service HEAP remains refused;
ordinary QuickLoad constructs the fresh scene. This is a bounded new-owner
publication guarantee, not a claim of whole-world rollback for old components.

## Capacity and charged memory

The structural RFAP maximum rises 669 to 677 rows. Its theoretical component
maximum rises 74944 to 75840 bytes, exactly 896 bytes. One newly present HEAP row
costs 112 bytes, or 140 bytes including the 16-byte component header and 12-byte
RFWC3 prefix extension. All eight flights add at most 924 bytes when the component
was otherwise absent. No pool is shortened and no row is dropped to fit.

The existing 110524-byte whole-save cap and 2 MiB restore budget stay unchanged.
Preparation rejects insufficient budget before allocating the full actual
sizeof(scene_ai_projectile_checkpoint_stage), and the world loader charges that
same actual sizeof once. The added tail is eight existing scene_turret_heap_round
values plus 17 uint32 words: 8*sizeof(scene_turret_heap_round)+68 bytes before
ABI padding. With four-byte double alignment the existing round is 72 bytes
and the tail is 644 bytes; with eight-byte double alignment it is 80 bytes and
the tail plus eight-byte-aligned stage rounding is 712 bytes. Compiler layout is
not measured in this source-only pass; actual sizeof, including any ABI padding,
is the allocation and budget authority. The runtime eight-slot array is moved
to an earlier declaration include, not duplicated; only the 4-byte service
counter is new live storage. No per-frame or per-shot heap allocation is added.

## Remaining implementation boundaries

Removed/retired excluded NPCs need a separate historical-exclusion policy and
remain refused. Generated turret heads and different projectile families are
outside kind 25. Pending delayed melee and robot-death receipts remain guarded.
Existing player movement, mounted held-input and occupied-vehicle limitations
are not expanded. Exact retail projectile/effect fidelity remains separate from
this operational continuation slice.
