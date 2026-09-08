# Weapon reset

## Empty-weapon handling and replacement choice

The remaining callback at 0x4a6f10 handles an empty weapon, rather than a generic
player-state reset. It checks reserve plus loaded ammunition, may show the
string "Out of ammunition" at 0x5a05e0, handles a paired weapon through 0x4a4e80,
and can choose another weapon with 0x4a6e50 before calling 0x4a4a50. The current
`player_reset` callback name is historical; this full operation is still open.
Its current-weapon lookup 0x4a5910 also calls 0x4ae0d0 for a valid weapon, which
has presentation/model effects and must not be treated as a pure accessor.

`rf_weapon_reserve` reconstructs 0x42add0: a null entity, negative weapon, or
negative descriptor ammo type returns zero; otherwise descriptor +24 indexes
entity reserves at +2ac. The shared view contains 32 reserves, 64 loaded counts
at +32c, and 64 ownership bytes at +42c. Added bounds checks reject nonnegative
weapon/ammo indices outside those arrays rather than reproducing original reads
past them.

`rf_weapon_choose_available` reconstructs 0x4a6e50 after entity lookup. It scans
all 32 preference entries at player +1154 in order; invalid or unowned weapons
are skipped (0x403250). If descriptor +260 is positive, reserve plus loaded
count must be positive. The sum retains original 32-bit wrapping behavior.
With byte player +f41 nonzero, weapons whose descriptor +268 has mask 0x100
are deferred: retain the first such eligible weapon as fallback, but continue
looking for an eligible unflagged weapon. Return -1 if none is available.
The meaning of the flag is not inferred beyond this observed selection rule.

`tools/verify_weapon_inventory.py` matches 4,000 complete original executions
of both functions with unchanged lookup, ownership and flag callees. 3,475
choose a weapon; fixtures include null entities, duplicates/invalid preference
entries, deferred fallbacks, shared/negative ammo types and wrapping count sums.
Two C-only cases verify weapon/ammo bounds rejection.

The shared 64-frame diagnostic also observes this choice: weapon 1 is a deferred
fallback; weapon 0 is chosen while reserve 0 is one, then weapon 1 is selected
after reserve reaches zero at frame 32. PC, original instructions and stock
64 MiB XEMU agree. The diagnostic hashes reserve and replacement outputs but
does not apply an actual weapon switch; presentation and the complete empty-
weapon handler remain separate work.

`rf_weapon_reset` follows 0x41ae70 after a valid type-zero entity has been
resolved. Null state and indices outside [0,63] return unchanged. Descriptor
views contain flags +264/+268 and release sound class +204 from the original
1360-byte records at 0x85cd08. All 64 descriptors must be available even when
global weapon count 0x872448 is smaller: the early active-weapon path uses the
fixed 64 limit; predicate 0x4c90f0 additionally checks the dynamic count.

The entry's active byte is captured before any reset. If nonzero, byte global
0x64ecbb differs from exactly 1, and descriptor +264 has mask 0x2 or 0x4 set:
stop sound +81c if it differs from -1, then store -1; emit release sound when
its class is nonnegative, storing the returned handle in +820; and stop
nonlooping animation weights when 0x40a1e0 reports a character and +810 mask 0x1
is clear. The stop is the shared 0x51c390 reconstruction, retaining slots and
references until update. Every valid reset then clears the entry's active byte
and entity +7d0 bit 0x2000.

If weapon predicate 0x4c90f0 succeeds (+268 bit 0x40 with an in-count index)
and +13d4 is not -1, call 0x48f130(handle,0); the caller does not clear that
handle. Next, a player-associated entity with a previously active entry calls
0x4c90f0 again and discards its result at 0x41afcc. Otherwise a locally
associated, previously active entity calls
0x48aa90 only when its weapon matches one of five globals (0x872110, 0x872464,
0x872444, 0x85cd04, 0x85ccfc). 0x48aa90 is a read-only lookup returning the
first matching local player pointer, and this caller discards that result too.
The earlier interpretation as a local-release operation was incorrect; the
false callback dependency and unused state/context fields have been removed.
The original 0x41afbb..0x41b015 block has no state changes on stable views.
Finally a player-associated entity calls
0x4a6f10(entity+1430,0), regardless of the captured active byte.

The compact state uses resolved zero/one predicates: character_present is
0x40a1e0 (non-null model wrapper and class +94 equal to 2); player_present is
0x42a8e0 (+7c bit 8 and non-null +1430). Entity resolution, list ownership and
class loading remain separate.
Callbacks may update these fields through their user data; later decisions read
them after preceding effects, matching the original order.

External sound, effect and player-reset adapters are required only
when reached. Missing operations return RF_NOT_FOUND before that operation,
retaining any earlier mutations. The release-sound adapter owns 0x4285a0 position
selection, 0x434d00 class resolution and 0x48a9c0 emission. Effect switching is
now reconstructed in effect.c and used by the diagnostic; successful audio/player
effects remain unverified.

`tools/verify_weapon_reset.py` compares 2,400 original executions with full
entity-reset, playback and reference state. 1,598 complete; the other cases stop
at observation boundaries before deliberately absent adapters: 160 sound stops,
97 release sounds, 83 effect stops and 462 player resets. 132 original 0x48aa90
lookups now execute to completion instead of being mistaken for side effects.
Original callees are not replaced. At each boundary C must return RF_NOT_FOUND
with exactly the same preceding mutations. This verifies supported paths and
the operation boundaries, not successful external adapters.

The shared diagnostic now uses valid weapon index 0, marks it active at frame
48, and invokes this reset through the preparation callback. Sound handles and
release sound are absent; character playback and an effect pair are present. Its nonloop stop
causes a later sidestep restart, producing starts 17,17,18,18. The complete
64-frame profile, including 16 reset calls, weapon state and changed poses,
matches original instructions, PC and stock 64 MiB XEMU. This remains a
scripted rig rather than an initialized gameplay character.

## Effect switching

`rf_effect_set_enabled` reconstructs 0x48f130 and 0x4973b0/0x4973d0. Each
original 40-byte effect record provides two object pairs at 0x75ec48/4c and
0x75ec50/54; nonzero byte global 0x64ecb9 selects the latter. Both pointers
must be non-null or neither changes. Disabling clears byte +140 and leaves
deadline +154 intact. Enabling with any nonzero int changes the byte to exactly
one and stamps current game time only when its previous value was not one.
Aliased pointers retain this same sequential behavior. Bounds checks reject
invalid record indices instead of accessing arbitrary original memory.

`tools/verify_effect_switch.py` matches 4,000 complete original executions,
including missing pairs, aliases, noncanonical enabled bytes, override low-byte
selection and clock boundaries. Two C-only cases reject invalid indices.
This controls existing effect state; effect creation, simulation and rendering
are not implemented. The shared runtime invokes a real effect-stop adapter
on weapon reset and hashes both enabled bytes and preserved timestamps.
