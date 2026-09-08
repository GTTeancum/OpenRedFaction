# Weapon reset

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
0x4c90f0 again and discards its result at 0x41afcc. It does not invoke local
release there. Otherwise a locally associated, previously active entity calls
0x48aa90 only when its weapon matches one of five globals (0x872110, 0x872464,
0x872444, 0x85cd04, 0x85ccfc). Finally a player-associated entity calls
0x4a6f10(entity+1430,0), regardless of the captured active byte.

The compact state uses resolved zero/one predicates: character_present is
0x40a1e0 (non-null model wrapper and class +94 equal to 2); player_present is
0x42a8e0 (+7c bit 8 and non-null +1430); local_player is 0x48aa30's local-player
list lookup. Entity resolution, list ownership and class loading remain separate.
Callbacks may update these fields through their user data; later decisions read
them after preceding effects, matching the original order.

External sound, effect, local-release and player-reset adapters are required only
when reached. Missing operations return RF_NOT_FOUND before that operation,
retaining any earlier mutations. The release-sound adapter owns 0x4285a0 position
selection, 0x434d00 class resolution and 0x48a9c0 emission. These adapters are not
implemented by this change; successful audio/player effects remain unverified.

`tools/verify_weapon_reset.py` compares 2,400 original executions with full
entity-reset, playback and reference state. 1,448 complete; the other cases stop
at observation boundaries before unavailable operations: 171 sound stops,
75 release sounds, 91 effect stops, 156 local releases and 459 player resets.
Original callees are not replaced. At each boundary C must return RF_NOT_FOUND
with exactly the same preceding mutations. This verifies supported paths and
the operation boundaries, not successful external adapters.

The shared diagnostic now uses valid weapon index 0, marks it active at frame
48, and invokes this reset through the preparation callback. External handles
and release sound are absent; character playback is present. Its nonloop stop
causes a later sidestep restart, producing starts 17,17,18,18. The complete
64-frame profile, including 16 reset calls, weapon state and changed poses,
matches original instructions, PC and stock 64 MiB XEMU. This remains a
scripted rig rather than an initialized gameplay character.
