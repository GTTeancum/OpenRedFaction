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
inventory has 77 records. Read-only `probe_campaign_geomod_brush.py` confirms
that nearby compiled room-33 face 4409 comes from brush UID 8756: 24 source
faces become 62 compiled faces, and the source is non-convex. The geometry
near UID 9456 includes the large subtractive brush UID 7778 with 488 faces.
Neither can be admitted by changing the test-level name in the convex-post
adapter. See `docs/research/L1S1-SCRIPTED-BLAST-BRUSH-20260922.md` for the
source offsets and limits of that read-only probe.

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
L1S1.rfl 8756` reports a closed but non-convex 24-face source; the existing
process-local rifle3415 pickup replay still grants and switches the weapon
without inventory injection. None of these checks proves campaign GeoMod
publication or a playable ordinary cut save.
