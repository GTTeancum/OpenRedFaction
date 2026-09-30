# Ordinary NPC projectile combat

NPCs now request rocket/grenade definitions, world models, flights and impact effects from their living held/owned loadout before scene resources open. These requests do not grant player weapons or load extra first-person views. Known retired actors do not request combat resources; hidden living actors retain resources for script reveal. Existing fixed eight-flight pools, finite NPC ammunition, source-aware damage and updates after thrower death remain shared.

The former DEV-only attack gates now require prepared projectile resources. Authored per-instance weapon overrides are also parsed and applied after class defaults; see ENTITY-INSTANCE-WEAPONS.md for binary evidence and installed examples. This enables the actual level-data path rather than requiring a DEV gun assignment.

## Focused verification

- Level parser checks cover both weapon strings, empty/none, retained ownership and truncated records.
- Loadout checks cover preserving default alternatives, unknown/empty inheritance, category clearing, primary reserve fill, the Rocket Launcher three-reserve exception and secondary acquisition.
- Actual scene scheduler checks run with DEV disabled, reject missing resources without consuming ammunition, then launch real finite flights once resources are ready. Existing collision/source checks pass.
- Demand checks cover names independent of scene slot IDs, held ownership, empty ammo, hidden/dead/retired actors and no player inventory/view-mask mutation.
- PC replay tool: `python tools/check_ai_projectile_ordinary.py --run`. This creates a controlled encounter on original L1S1 geometry with guard8456, player9858 and per-instance projectile overrides; other actors/events are removed. Original tables and other assets remain read-only hardlinks. It is a synthetic encounter, not a claim of campaign progression.
- Both 650-frame PC replays passed: rockets `[3,3,0,0,0]`; grenades `[4,37,2,2,0]`. Neutral player never fires. Both final captures inspected: rocket encounter ends in player death; grenade encounter shows the throwing guard and player damage.

- Native grenade encounter `artifacts/xemu/render-20260922-180307`: PASS650frames, identical AI grenade `[4,37,2,2,0]`, blast and enemy combat counters,5213freepages (20.36MiB), stock64MiB. Native framebuffer inspected at the time: guard mid-throw, level and player weapon visible. Original disc restored and emulator closed.
- Native rocket encounter `artifacts/xemu/render-20260927-064328/report.json`: PASS650frames on stock64MiB XEMU with4995freepages (19.51MiB). `AI_ROCKETS` is `[3,3,0,0,0]` on both PC and Xbox; `ENEMY_COMBAT` also matches `[650,1,3,0,0,3248814004,1,0]`. This uses the same controlled, non-DEV L1S1 per-instance override fixture and `--no-images`. No visual content was captured or checked in this run. PASS means the runner's selected gameplay checks passed; its report excludes a platform-specific pickup bookkeeping field from parity.
- A source guard preserves model-less NPC animation startup when applying overrides; the later native rocket replay includes that source change.

## Remaining first-pass work

Full campaign integration, secondary-weapon AI policy and authored grenade release timing remain open. Aggregate live logs do not prove exact remaining NPC rounds or exhaustion; finite debit is verified by focused implementation tests. Audio dispatch has not been auditioned. Broader encounters and presentation are deferred to integration/testing feedback. Current runtime checks are Xbox-only; the PC target is compiled but gameplay is not run.

Ordinary player/vehicle saves now capture live NPC grenades and rockets into
the optional 17th PROJECTILES section of RFWC3. Each fixed-pool row persists
flight motion, remaining lifetime/fuse, damage, the authored source UID and
the projectile-specific contact or orientation state. Restore validates all
rows and rebinds the source before publishing them with the staged NPC state.
The envelope still emits RFWC2 without active flights and reads RFWC1/2.
DEV checkpoint admission retains its earlier in-flight guard.

Stock-64-MiB XEMU saved one active grenade at frame 200 and one rocket at
frame 34 in controlled, ordinary L1S1 encounters. Both RFWC3 payloads
reloaded and advanced 60 frames. The grenade remained active while the guard
launched another; rocket telemetry recorded the restored impact and a later
new impact. The guest reported no checkpoint failure, with at least 4688 free
physical pages at the endpoints. The initial harness reports label these runs
FAIL because their continuation accounting incorrectly rejected legitimate new
launches or omitted rocket impacts; the harness logic has since been fixed.
These checks establish numeric continuation only, not visual or audio quality.
The unchanged authored prop UID 8785 overlapped the saved player by 7.7 cm;
the shallow player/prop restore allowance is now 10 cm, the same bound used
for NPCs, so this legal native position can load.

Open: an active flight whose source NPC has already retired cannot yet be
captured, broader natural campaign encounters remain untested, and exact
grenade terminal effects after reload remain unverified.
