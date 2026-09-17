# Live player damage from debris

The ordinary moving-fragment update now invokes player contact after collision
and gravity, only while bounce count remains positive. The single-player
adapter checks the retained fragment/player room and original flag2, uses the
player body origin and model radius, then calls the verified strict sphere
and speed/radius damage helper. NPCs are deliberately excluded: original48f678
iterates players, not the NPC population.

The fragment retains original birth flags. Qualifying floor contacts OR2;
relaunch preserves the flags. Accepted overlap ORs2 before damage, including
zero-damage overlaps. Subsequent updates cannot repeatedly damage the player.
Kind1/source-1 damage uses the existing player health/armor owner and combat
feedback. Player direction is derived from negative normalized velocity and
marked through the reconstructed4a5a20/4a5af0 helpers. Zero-speed overlap uses
finite zero direction as a port guard rather than original normalization NaNs.
Original42e3d0 impact particles remain open, as does exact global effect RNG
scheduling. This adapter is not full original debris-impact visual parity.

## Explicit scene fixture

PC RF_REPLAY_DEBRIS_PLAYER_TEST and native debris-player-test.flag enable a
frame100 adapter fixture; tools/xemu_render_check.py exposes --debris-player-test
and stages/restores that file. It does not insert a fabricated rendered fragment
into the pool. The fixture invokes the actual scene contact service with one
controlled radius.5/speed5 fragment: wrong room and already-suppressed calls
must leave health unchanged; an admitted call must apply damage; its next-frame
repeat must leave health unchanged.

PC550-frame test passes: amount1.25, health100->99.4000015, armor100->99.3499985,
one accepted contact/damage event and player direction4 (flag32768).
The ordinary two-shot control has zero player overlaps: fragments retain room3
while the retreating player occupies room4. That is expected admission behavior,
not proof of an ordinary flying fragment striking the player. Ten relevant
CTest cases pass. tools/verify_debris_player_scenario.py requires the exact
positive fixture outcomes; matching zero counters cannot satisfy it.

Open: ordinary trajectory contact, repeated-blast suppression, damage death
handling in this route, impact particles and visible direction feedback.

## Native result

Stock64MiB run artifacts/xemu/render-20260917-005617 passes65 state
comparisons over550frames with4158pages free (16.242MiB). The explicit positive
scenario verifier passes against its captured report. All246 captured ripple
vertices and input sections remain bit-exact with PC. Native endpoint inspection
confirms the room, damaged post, weapon and HUD; no claim is made for transient
impact particles or damage-flash appearance from that endpoint. Disc restoration
completed and the owned emulator exited; other projects were left untouched.
