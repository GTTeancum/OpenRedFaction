# Detached piece ownership dispatch

`tools/probe_geomod_piece_dispatch.py` executes original RF.exe stage 2 from
466dcd through 466f4c in Unicorn, including real loop control, intrusive list
mutation, placement copying, changed-box arithmetic, and stage advancement.
The original SHA is checked by the script. Nine cases cover enable byte 0,
1 and 2 crossed with free owner pool capacity 0, 1 and 3, each processing
three supplied extracted components.

Observed:

- Extraction at 466df7 receives successive labels 0, 1, 2. EBP is pushed
  and incremented at 466f25. Earlier notes saying repeated label 0 were wrong.
- Only enable byte exactly 1 permits allocation from the free intrusive list
  at 6485d0. The worker does not allocate another owner when the list is empty.
- A free node is unlinked, assigned the piece pointer at +8 and placement at
  +12, passed to 466550, and appended to the active list at 647c30.
- Disabled/exhausted cases send the piece to 4136e0 with argument 1. The
  supplied destruction service records this call; internal disposal is not
  exercised by this probe.
- All three pieces publish changed boxes before the owner admission branch,
  including discarded pieces. The independently verified 32-box cap still
  applies. Owner shortage must not suppress world-change notifications.
- After processing, 4666a0 runs when the enable byte is nonzero (including
  2), then 4f0b90 runs. Stage advances from 2 to 3.

Supplied services: component selection count, geometry extraction, placement,
piece initialization, destruction and finalization. Thus this proves worker
dispatch/ownership behavior, not the geometry mutations or physics inside
those services. Report: artifacts/geomod-postedit-re/piece-dispatch.json.

Production integration must retain stable extraction labels during the batch,
budget a bounded piece pool, and preserve notifications independently of
piece visibility/owner admission. Next work remains source-mesh removal,
face/lightmap relocation, dynamic piece lifetime and chronological rebuild
persistence. There is no new runtime implementation or Xbox acceptance here.

Follow-up: GEOMOD-PIECE-SUBDIVISION-20260917.md identifies this pool as
temporary subdivision work entries; terminal pieces go through466440 before
the entry returns to the free pool. Do not use it as a persistent body pool.
