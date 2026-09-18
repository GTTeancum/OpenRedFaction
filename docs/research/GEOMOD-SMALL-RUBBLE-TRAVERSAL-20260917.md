# Small-rubble traversal and standing-test correction

The old tools/check_rubble_standing.py fixture predates the recovered strict
radius>0.5 player-contact admission rule. Its real extracted chunk has radius
0.47143828868865967. Running it now correctly produces zero rubble contacts and
fails its obsolete standing assertion. Its stale generated PASS report has been
marked SUPERSEDED, not presented as current acceptance. Original admission
provenance remains in GEOMOD-FRAGMENT-ACTOR-ADMISSION-20260917.md.

The tool is replaced by tools/check_small_rubble_traversal.py. An ordinary rocket
creates the same live chunk, then the player walks across its centerline without
jumping. The path begins at(4.450001,-0.401361,2.5) and finishes at approximately
(-5.944962,-0.368479,2.553672), beyond the chunk centerX=-4.969604. The chunk
remains alive and settled; all player/rubble queries return no contact. The tool
checks radius eligibility, both sides of the crossing, lateral alignment, no
jump, and exact2760-byte saved continuation. It clears its own report before
running and includes the save-boundary input in the resumed recording.

Positive control: tools/check_intermediate_rubble_standing.py still passes
ordinary jumping/standing on radius0.530725 rubble, exact standing and walk-away
save continuation, and rejection of a save with retired supporting rubble.
These are distinct required behaviors; no collision threshold was relaxed.
Logs: artifacts/small-rubble-traversal.log and intermediate-standing-current.log.

Stock64MiB native traversal render-20260917-210858 runs600 records and passes76
checks. All2760 saved bytes exactly equal the PC saved state. Both platforms
record884 queries and zero fragment contacts. Endpoint3958 pages is15.461MiB;
this is not a peak-memory guarantee. Disc restoration succeeded and the owned
emulator exited. Log: artifacts/small-rubble-native.log. No new native reload run
is claimed; exact resumed/control comparison here is PC evidence.

The PC pre-walk frame was inspected and shows the damaged post and small resting
rubble with residual smoke. The native endpoint was inspected and shows the
player beyond the destroyed-post location, facing the adjacent intact structure,
with room/weapon/HUD present; the crossed chunk is behind the camera. Intermediate
frames were not visually reviewed and audio was disabled. No GitHub image added.
This is a recovered-policy gameplay regression and test correction, not a new
standing or moving-support implementation. Moving/larger-fragment interaction
coverage remains open.
