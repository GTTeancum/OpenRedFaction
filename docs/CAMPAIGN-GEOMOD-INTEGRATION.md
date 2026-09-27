# Campaign GeoMod integration boundary

The first-playable cutter, collision publication, debris and standalone
destruction checkpoint work in the selected `ctf06.rfl` test geometry. The
ordinary single-player loader currently creates no terrain owner. In
`scene.c`, `scene_terrain_open` opens authored source/publication only for
`ctf06.rfl`; `scene_explosion_terrain` requires that owner and its room before
cutting. Campaign scripted geometry requests therefore reach region admission
but cannot yet change the world.

The first positive scripted target is L1S1 `Explode` UID 9456: radius 5.0,
eligible at hardness 50 near region 9884. UID 9466 has radius 0.75 and is
correctly below the original minimum of 1.0. The installed L1S1 region
inventory has 77 records. The expanded read-only editor probe now parses all
160 brushes and consumes exactly 649,126 section bytes; five brush records
carry opaque spans of 960 or 1,056 bytes that remain undecoded. A compiled
polygon-distance query at UID 9456 in room 28 places face 3766 of subtractive
brush UID 8755 nearest at 0.243 world units; brush UID 7778's face 3765 is
0.446 units away. Brush 8755 has 44 non-convex source faces; UID 7778 has 488
non-convex source faces with 778 compiled faces across nine rooms. These are
cavity boundaries, not isolated convex posts. Neither can be admitted by
changing the test-level name in the current cutter. See
`docs/research/L1S1-SCRIPTED-BLAST-BRUSH-20260922.md` for earlier source
evidence and the limits of proximity as a GeoMod admission test.

The next playable integration must derive an authored room owner and its
source-to-compiled face mapping, apply the existing cutter through that room's
CSG, then publish matching render, collision, material and player-support
state. A scripted UID 9456 replay must prove an actual persistent cut and
changed collision, not just an eligible request or nonzero draw calls. The
current ordinary world save rejects scenes with terrain and its DESTRUCTION
section is empty. After campaign cutting works, save/load needs staged
destruction publication alongside NPCs, movers, player and events. Its
110,524-byte transport cap may be too small for a full ordinary section plus
the destruction journal; the size must be measured on the chosen level before
changing the cap or storage format.

Text-only current checks: `python tools/inspect_campaign_geomod_regions.py`
reports the 77 L1S1 records; `python tools/probe_campaign_geomod_brush.py
L1S1.rfl --inventory` validates complete editor-section framing, and
`python tools/locate_campaign_geomod_surface.py` ranks actual compiled room
polygons by distance and source owner. A room-33 control at UID 9466 resolves
to the previously established UID 8756 source word 4409. The existing
process-local rifle3415 pickup replay still grants and switches the weapon
without inventory injection. None of these checks proves campaign GeoMod
publication or a playable ordinary cut save.
