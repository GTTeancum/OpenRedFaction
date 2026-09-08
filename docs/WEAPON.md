# Weapon reset

## Selection queue

`rf_weapon_finish_selection` reconstructs 0x4a4c91..0x4a4db4 after the earlier
selection gates. Entity +1428 is a 32-bit mask checked by 0x42a6b0; a requested
index in [0,31] with its bit set remaps global 0x85ccd8 to 0x85cd00.
Already-pending requests return immediately. Global 0x87211c splits the
comparison between inventory +4 and +8 (primary/secondary current weapons).
An already-current request with a zero force byte returns, with a formatted
message when player +10 mask 0x10 is set (0x4a68d0).

Otherwise nonzero ownership byte, or the in-count descriptor +264 mask
0x40000 (0x4c9070), permits queuing. A zero defer byte then calls 0x4aa0b0.
Finally a nonzero player +f94 byte (0x4ace90) calls the reconstructed
`rf_weapon_clear_followup` (0x4ad8a0), even when ownership failed and no request
was queued. It clears bytes +f94/+f95 and the 32-bit value +f98, preserving
+f96/+f97. This byte is read from state after the apply callback, allowing
that adapter to update it through user data. The compact selection state is
now 16 bytes, including these eight bytes. The former followup callback and
input snapshot have been removed; this routine requires no external adapter.

Missing reached message/apply adapters return RF_NOT_FOUND before
that operation. Queuing and timer clearing remain committed if the apply
adapter is missing or fails. Invalid pointers or a descriptor count above 64
return RF_RANGE before mutation. No original descriptor read occurs for an
out-of-count index. This tail does not replace the complete 0x4a4a50 routine.

`tools/verify_weapon_selection.py` compares 4,000 original executions with
unchanged predicate, queue and followup-clear callees, stopping before the two
unavailable external operations. Expanded results: 3,071 complete, 408 message,
521 apply boundaries; 797 requests queued and 872 followup clears, including
687 clears without a queue mutation. The comparison checks both queue fields,
all eight followup bytes and unrelated player bytes.
It does not validate successful callback bodies or earlier selection gates.
PC/NXDK builds and PC tests pass; this tail is not yet in the Xbox diagnostic.

`rf_weapon_queue_selection` reconstructs the complete 0x4acd50 routine:
store the requested signed value in player +f80, then tail-call 0x4fa3e0
to set the deadline at +b8 to -1. It deliberately preserves all signed
weapon values; eligibility checks belong to callers such as 0x4a4a50.
The queue primitive preserves all other compact state fields. A null state returns RF_RANGE
as an added safety check. This queues a request; activation and presentation
remain unimplemented.

`tools/verify_weapon_queue.py` compares 640 executions against the unchanged
original instructions and timer callee, including signed extremes and every
combination of selected boundary values. It checks the original 4096-byte
player view for unrelated writes. PC tests and the NXDK build pass; this new
primitive has not yet been connected to the emulator diagnostic or gameplay.

The earlier description of 0x4a4e80 as a "paired switch" was too narrow.
Its body includes ammunition tests, firing through 0x425830, sound, reset and
empty-weapon handling. PAIR identifies the empty handler's paired branch,
which dispatches this firing routine with arguments (player,1,1); it does not
establish that the operation merely changes the selected weapon. Full firing
behavior still requires reconstruction. Actual selection 0x4a4a50 reaches
0x4acd50 after its eligibility checks.

## Empty-weapon handling and replacement choice

`rf_weapon_decide_empty` now reconstructs 0x4a6f41..0x4a70db after current-weapon
resolution. The earlier 0x4a5910 presentation update is still caller-owned.
It returns NONE, PAIR, MESSAGE or SELECT with a selected weapon where applicable;
it does not execute the outgoing firing/selection/message operation.

The initial gate requires player byte +f40 or the current weapon matching global
0x872118. Passenger predicate 0x42acd0 blocks a nonzero request byte. Current
weapon equal to 0x85cce0 is blocked by projectile predicate 0x4c9e30. Descriptor
+260 must be positive, and weapon 0x87210c is excluded. Ammo is checked on the
linked entity for linked classes 1/4, otherwise on the primary entity. Positive
reserve-plus-loaded or the in-count descriptor +264 mask 0x20 prevents action.

Weapons 0x85ccd8/0x85cd00 form the paired branch; the former uses that branch
only when byte global 0x64ecb9 is zero. Positive counterpart ammo requests
0x4a4e80(player,1,1). Otherwise the paired path goes directly to replacement
choice. Outside that path, linked classes 1/4 or a passenger request the
out-of-ammunition message via 0x4383c0 with zero flags. Remaining cases select
from the PRIMARY inventory's preference list (even when ammo was checked on a
linked entity), then request 0x4a4a50(player,weapon,1,0) for a valid selection.

The input's passenger/projectile predicates and linked classification are
resolved snapshots. `tools/verify_weapon_empty.py` exercises their unmodified
original callees, including a projectile-list fixture, then observes final
operation boundaries. All 4,000 cases match: 3,272 NONE, 64 PAIR, 202 MESSAGE,
462 SELECT. The shared runtime also hashes NONE before ammo exhaustion and
SELECT afterward, agreeing with original instructions and 64 MiB XEMU.

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

## Local-player transition evidence

0x4aa0b0 returns immediately unless its player is global 0x7c75d4. Otherwise
it clears followup state through 0x4ad8a0, calls 0x4aa080, clears byte +fb0,
requests state 7 through 0x4a9380, and calls the presentation-bearing current
weapon accessor 0x4a5910. Either true 0x4c8350(weapon,0/1) predicate invokes
0x41ae70(entity handle,weapon). After resolving the entity again, a weapon
matching global 0x872468 (0x4c90d0) stamps entity +136c with game time.
This routine does not directly install player +f80 as the current weapon;
calling it queued-weapon activation would overstate the recovered behavior.
The `apply_queued` adapter name describes its caller position, not a proven
complete activation operation. Its full body remains unreconstructed.

0x4aa080 tolerates a null player; otherwise it clears bytes +f9d and +f9c,
sets +fa0 to -1, and stamps +fa8 using 0x4fa360(offset=0). 0x4ab180 sets byte
+1044 to 1 on global local player 0x7c75d4 when non-null. These observations
identify the next state dependencies; they are not implemented by this change.
