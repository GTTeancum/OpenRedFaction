# Authored vehicle affiliation

## Integration

Active vehicle construction now copies the uniquely matched authored seed's
`spawn.friendliness` into its stored damage-owner affiliation after the existing
UID/class/vitals checks. Previously the active owner retained the shared class
prototype's affiliation 0, although passive construction already copied the
authored value. Parking a formerly active friendly host could therefore make it
eligible as a hostile homing target until a fresh load reconstructed it.

This is the same authored field decoded by `rf_level_entity_spawn_read` in
`src/core/level.c`, not a new inference from occupancy, class or whether the
player may board. Developer hosts without an authored UID retain their class
defaults. Shared profile prototypes remain immutable.

Existing active/parked handoffs copy the entire damage owner. Checkpoint
publication retains the freshly constructed affiliation while restoring saved
health/armor, and passive loading likewise retains its authored team. No save
format is changed: authored-only identity continues through RFSW1/RFVA2/RFVC3.

The occupied damage-source override remains unchanged: outgoing damage may use
the current driver's affiliation without rewriting the stored host team.

## Focused Xbox check

`tools/xemu_vehicle_affiliation.py` stages the established enemy-free two-Jeep
CTF06 fixture. Original A (UID 7629) retains authored friendliness 1; copied B
(UID 915200) changes only the declared fixture friendliness field to 0 in
addition to the pair fixture's existing UID/transform edits.

Read-only live snapshots bind the exact active and passive UIDs/full handles to
their stored affiliations before and after ordinary Use handoffs, an ordinary
save, a fresh boot and return boarding. Existing two-owner ammunition, vitals,
pose and save-codec assertions remain in force. A constant three-word layout
descriptor supplies offsets rather than relying on hardcoded C structure
layouts; snapshots do not write guest memory.

The NXDK/Clang 19 build and serial Linux cloud XEMU 0.8.136 checks pass with
exactly 67,108,864 bytes of RAM:

- Source, 350 frames: at frame 193 A is active with team 1 and full handle
  33358332; B is passive with team 0 and handle 33292795. At frame 325 their
  roles are exchanged with the same teams and full handles.
- The 15,768-byte ordinary save retains B occupied, A parked, and independently
  spent ammunition A=996/B=998. Both owners retain health 400 and armor 0.
- Fresh boot, 140 frames: at frame 40 B is active with team 0 and A is parked
  with team 1. After ordinary exit and Use, frame 111 has A active/team 1 and B
  parked/team 0. Handles are freshly rebound on boot and remain stable through
  the return. Saved ammunition, RNG and pose checks pass without firing replay.
- Endpoint free pages are 5,964 for source and 5,804 after load. The player
  remains alive, both requested endpoints complete, and disc flags restore.

Evidence: `artifacts/xemu/vehicle-affiliation-20261007-113544/report.json` and
its original source/load results and saved payload. The initial failed launch
at `vehicle-affiliation-20261007-113433` is retained: the inherited harness
assumed `scene-preview.flag` was already present, so the clean disc stopped
before scene startup at frame 3. The new harness now stages and restores that
flag explicitly. No gameplay change was needed for the retry.

## Scope

This repairs authored stored-host affiliation. It does not implement scripted
vehicle team changes or add persistence for such changes. It makes no new claim
about guided-shot damage, NPC drivers, cross-profile handoffs or original-game
visual/audio fidelity. Existing occupied-driver attribution is preserved by
source review, not separately exercised by this fixture.
Friendly-owner homing exclusion follows the unchanged affiliation predicate;
this fixture checks its stored input, not a new runtime acquisition or rejection.
