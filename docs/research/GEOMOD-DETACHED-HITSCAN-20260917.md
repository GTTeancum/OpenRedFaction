# Hitscan contact with detached chunks

Player hitscan now queries current-pose fragment polygons up to the nearest NPC
candidate. A nearer fragment blocks further damage; a preceding world/mover
hit still wins. Each shotgun pellet follows the same path, retaining its own
spread ray. A fragment hit records the existing surface-feedback frame. Riot
Stick uses its existing default impact sound when a fired strike hits; no new
pistol decal, chip particles or damage response is invented.

The shared `combat_shot_obstructed` predicate also includes fragments after its
world/mover query. This connects the existing enemy/NPC and player visibility
callers. It does not alter NPC body selection or damage ownership. Fragment
queries use0x460 world bullet filters, radius0 and the candidate fraction.
No owner tags are passed into entity damage APIs.

`tools/check_detached_hitscan.py` plays the actual middle-post rocket, switches
normally from rocket to pistol at frame390 and fires at420. The hit replay
reports one pistol query/one fragment hit with point hash3671246963. The control
aims above the piece and reports one query/zero fragment hits. Both retain one
rocket, one terrain cut, two total weapon shots and no NPC damage. These are
ordinary process-local recordings, not direct hit injection.

The scene test additionally isolates a real extracted polygon beyond world
bounds, verifies the same obstruction ray misses with the registry absent,
blocks with it present, and misses when the target endpoint precedes the chunk.
This exercises the actual shared obstruction adapter; it is not a live combat
encounter with a protected NPC. All121 existing tests pass; the expanded scene
obstruction test passes separately after its addition. NXDK builds.

Remaining work: player movement collision, fragment damage/breakup, blast wake,
impact presentation, broader weapon coverage and live actor-cover encounters.
No audible/visual bullet-impact quality or full original hitscan parity claim.

Stock64MiB XEMU evidence: artifacts/xemu/render-20260917-064821. The full
550-frame run passes72 state comparisons, including exact fragment hit count,
selected batch/piece and point hash. The native framebuffer was inspected and
shows the selected pistol, HUD, room and settled chunk. This final frame does
not establish a visible bullet impact; that presentation remains open. The
harness restored the staged disc and closed only its own emulator instance.
