# Authored post checkpoint placement prerequisite — 2026-09-16

Status: compiled and executed by the primary integration task; all assertions pass. Retained output: `artifacts/authored-post-live/checkpoint-placement.log`.

`tools/check_authored_checkpoint_placement.c` reconstructs `ctf06.rfl` UID94 from the real archive and decodes the exact exported two-shot RGCH. It retains the existing publication checker's authored source/provenance checks without modifying that root-owned tool. It builds a private partial-room composition and uses `rf_checkpoint_standing_check` against all **798 pending collision faces**, with the immutable full collision world providing other rooms. Passing the publication's 12-face count with the composed tree would silently omit unchanged room geometry; the probe explicitly rejects that adapter mistake.

Inputs are the existing `artifacts/authored-post-live/two-shot.rgch` and `artifacts/lava-clearance/actual-body.bin`. The latter is a real exported miner sphere union (three spheres), not a synthetic capsule; it is an earlier miner export rather than a newly captured ctf06 body. The tool accepts another same-format live body export as its third argument, validates the RFS1 header/count/record extent, and uses its sphere records unchanged. No player eye offset is substituted for collision geometry.

Assertions:

- At body `(4.446455955505371,-0.4013611376285553,2.5)`, standing fit accepts safe outside placement against the full pending room.
- At `(-5,-0.4013611376285553,2.5)`, standing fit accepts the destroyed opening and actual revealed floor support.
- After resetting only the private terrain history and preparing the original 790-face room, the same inside-post placement returns `RF_NOT_FOUND`; outside placement still fits.
- All 786 unreplaced compiled faces preserve descriptors, filters, vertices and metadata; 82 liquid faces survive. Lower and upper shot corridors are clear after the two cuts, and the exposed floor is at Y=-1.25.
- Nearby existing floor and water controls preserve compiled IDs and exact hit records before/after composition and again after zero-cut reset. These controls use flags4 and0x1004 respectively; player standing fit uses ordinary flags4 and does not acquire liquid collision implicitly.
- While examining either pending candidate, the immutable world and active composition remain original. Aborting both candidates preserves the active tree pointer/count. No player teleport or live publication occurs.

Build/run from `D:\Programming\GitHub\OpenRedFaction` in a cmd shell (parent-owned execution):

```bat
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86 >nul
cl /nologo /std:c11 /W4 /MD /D_CRT_SECURE_NO_WARNINGS /O2 /fp:strict /Iinclude tools\check_authored_checkpoint_placement.c /Foartifacts\authored-post-live\check-authored-placement.obj /Feartifacts\authored-post-live\check-authored-placement.exe build\pc\Release\rf_core.lib
artifacts\authored-post-live\check-authored-placement.exe Installed_Game\levelsm.vpp artifacts\authored-post-live\two-shot.rgch artifacts\lava-clearance\actual-body.bin > artifacts\authored-post-live\checkpoint-placement.log 2>&1
```

A nonzero exit or missing terminal `PASS full_pending_room_actual_body` is a failed/incomplete prerequisite, not permission to loosen fit tolerances. The command assumes an existing matching Release core library; it does not request a shared rebuild.

Production hook: after profile2 history replay, publication conversion and private collision composition, construct a temporary `rf_geomod_terrain_view` whose `faces`, `tree` and `mesh.face_count` all describe the **full pending room**. Run saved standing placement before any live geometry/noise/atlas/player mutation. Abort pending owners on rejection and try the older save slot. Keep original-source reset clearance separate: it must reject a player inside the restored solid, while checkpoint restoration of a valid cut opening must accept that player. This tool does not implement RFDSv2, prove runtime restore, test moving entities, validate GPU staging, or establish 64MiB peak usage.
