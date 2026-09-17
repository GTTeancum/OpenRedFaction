# Debris contact with player actors

Important scope correction: original48f678 iterates the global PLAYER list
7c75e4/count7c7634, resolving each player's14 actor handle. It does not scan
all NPCs. This identity agrees with the retained drive-source and Driller
feedback reconstructions in DEATH-LIFECYCLE.md. Single-player integration
must target the player only; multiplayer remains outside the current goal.

`tools/probe_debris_actor.py` executes original48f678 through48f7a5/48f7a9.
144 cases cover suppression flags0/1/2/3, distances inside/on/outside the
sphere boundary, matching/mismatching retained rooms, one/two player actors,
and three velocity vectors including zero. Original room accessor, overlap,
length and normalization code execute. Actor lookup is supplied. Damage,
particle effect and player-direction services are intercepted and recorded;
no original health mutation, visual effect or player feedback is claimed.
The checked RF.exe SHA is the project's b8fb9ab4...c9b836 binary.

Recovered behavior:
- Flag2 suppresses the entire pass before entering the list.
- Actor room must equal the fragment's retained room.
- Sphere tangency does not hit; distance must be strictly below combined radii.
- On overlap flag2 is set before damage, including zero-speed overlap.
- Damage is float(sqrt(vx*vx+vy*vy+vz*vz)*fragment_radius*0.5).
- Damage arguments are target, amount, source-1, global872114, kind1,0,-1,0.
- Flag2 is not rechecked between players in this pass: two overlapping players
  both receive requests. It suppresses later passes, not the remainder here.
- Floor contacts also set flag2; small-fragment birth already returns it.
-4a5a20/4a5af0 are player contact-direction feedback, not physical knockback.

Shared `rf_geomod_debris_actor_contact` implements only the strict overlap
and damage arithmetic. Nine original output fixtures cover the boundary and
velocity combinations; all pass along with72 motion and140 gravity fixtures.
The scalar helper intentionally does not own flags, rooms, players or health.
Callers must preserve pass-entry admission and not re-gate against a flag
changed by a preceding candidate. Invalid inputs preserve both outputs.

Live connection remains open: retain suppression flags in chunks, preserve
floor/relaunch policy, use the player model radius/published origin and room,
route kind1 damage through the existing player owner, then implement verified
direction feedback and particles. This turn does not make live debris damage
players or NPCs. No new native gameplay run is claimed for this helper.

NXDK compile/link/XBE/XISO generation succeeds with the stock profile; log
artifacts/debris-motion/actor-xbox-build.log. This is build evidence, not
execution of the new contact helper on Xbox.
