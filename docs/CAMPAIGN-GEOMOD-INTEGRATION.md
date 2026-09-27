# Campaign GeoMod integration boundary

Current state (2026-09-26): A bounded L1S1 owner now handles scripted Explode
UID 9456. It reads editor brushes 8755 and 7778, constructs the cutter from
the shared template, admits a room-28-only cut, retains authored faces outside
the cutter, builds new walls, maps compiled textures to live material slots,
and stages one replacement collision tree. The scene hides the old room-28
render faces, binds the new collision overlay, then marks the cut active for
rendering. The generated walls use the level's loaded substrate material.

An 80-frame process-local PC replay and a stock 64 MiB XEMU replay agree on
one committed cut, 865 faces, 3,392 vertices, a 145,316-byte replacement tree,
12 foreign detail-face AABB overlaps and zero reported error. A short ray hits
a new wall that the original room tree misses on both builds. XEMU ends with
4,048 available guest pages (15.81 MiB). The text-only checker still stops on
an NPC draw-count difference (PC 3, Xbox 4). No visual frame was inspected
under the user's no-images instruction; appearance, lighting, room visibility
and visual parity remain unverified. This owner accepts only the first UID
9456 cut. A repeated ON keeps it without making a second cut. Ordinary saves
now encode this one authored cut as a 12-byte identity record. A fresh-scene PC
save/load/resave replay rebuilds room 28 before actor placement, retains the
same mesh and differential collision result, and emits the same 12-byte
destruction section on resave. An uncut L1S1 save still loads. The Xbox build
passes, but native HDD save/load of this cut has not yet been exercised.
Other scripted cuts, repeated destruction, portal-spanning edits and campaign
weapons still need general ownership and publication.

The sections below record the earlier staging evidence. Statements that say
publication or stock runtime validation has not happened describe those
earlier milestones, not the current build.

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

The shared `geomod_solid_clip` core now has a bounded, allocation-free
outside-union operation for polygons against overlapping closed air volumes.
A focused PC fixture with two overlapping 20-by-20 cavities keeps the expected
340 square units of a 30-by-30 section and preserves UVs; the stock NXDK
build also passes. This provides one CSG operation needed to retain rock faces
around multiple authored cavities. It has not been connected to the L1S1 room
owner or exercised on the actual 44/488-face source brushes. Those complex
brushes may exceed the present scratch capacities or need spatial filtering.
The union path now rejects planes whose actual face bounds miss each input
piece, while the established cutter keeps its earlier split behavior. At
compiled L1S1 face 3766, exact polygon bounds intersect 7 of brush 8755's 44
faces and 2 of brush 7778's 488; expanding those bounds by the blast's
5-world-unit radius intersects 44 and 27 respectively. This is a candidate
count, not proof that the full room cut fits the current scratch budget. A
remote-box check confirms that infinite planes alone do not create fragments.

The shared C editor-brush reader now opens UID 8755 and UID 7778 directly from
the installed L1S1 section without requiring decoded lengths for unrelated
opaque sidecars. It owns world-space collision faces and source-word IDs in
5,104 and 57,280 resident bytes respectively; its temporary section buffer
is released after selection. A 10-by-10 horizontal patch centered at UID 9456
clips against both actual closed air sources on PC within a 4,096-vertex / 1,024-
fragment work bank: 67.6862 square units retained in 97 fragments / 385
vertices. This demonstrates a bounded real-source CSG operation, not a room
cut: the compiled room's rendered/collision faces are not yet replaced, and
the reader has only been exercised at runtime on PC. Its source compiles in
the NXDK Xbox build; native loading and stock-memory validation remain.

The compiled face view now exposes its serialized source word. The shared C
lookup maps UID 8755 to exactly 40 compiled faces in room 28, including face
3766/source word 4395, and UID 7778 to 109 faces there; results stay in file
order. This is the source-to-compiled handoff needed to select replacement
surfaces without guessing from proximity. It does not yet determine which of
those faces intersect a particular cutter, nor publish modified faces into
the room render and collision trees. The existing overlay can replace one
room tree, but campaign publication must stage render and collision together.

The cutter template transform is now shared with campaign CSG. With the
installed template at UID 9456, radius 5, and the two authored air volumes,
the shared C wall builder emits 368 inward-facing convex crater polygons / 1,447
vertices. The PC probe checks winding toward the removed volume, material and
face bounds, then converts those polygons to collision faces and builds a
61,820-byte standalone collision tree within a 1 MiB limit. The same source
compiles into the NXDK build. The tree contains only new crater walls: it is
not yet a replacement for room 28's complete tree, and no live campaign
render/collision state changes. The next step is to clip the existing compiled
room faces by the cutter, merge retained surfaces with new wall polygons, and
publish both views atomically.

The room-28 compiled faces now clip outside the same cutter. The bounded-face
path avoids unnecessary remote plane splits; changed near-convex fragments are
partitioned into collision-valid pieces, while untouched authored faces retain
their original collision records. The PC probe removes face 3766 and retains
499 polygons / 1,956 vertices from the 411 source faces. Combining these with
368 crater-wall polygons yields a staged 867-face / 3,403-vertex room tree;
its collision tree occupies 145,652 bytes. The earlier 800-face scene draw
limit could not hold it; the new 1,024-face bound awaits stock 64 MiB runtime
accounting. The generated
walls also still need precise room assignment at portals before the staged
tree is authoritative. Neither the render view nor live collision changes in
the current build; the NXDK build only confirms the shared C code compiles.

Room-scope admission now checks every other room's compiled face bounds and
room-linked portal bounds against the cutter. For UID 9456, it finds zero
foreign solid faces and zero touching room-28 portals; 12 nearby faces belong
to room 15's operation-4/flag-8 detail brush UID 7030, which stays authored.
The scan across all 160 L1S1 editor brushes showed operation 4 compiling to
flag-8 detail faces, while operations 0/1/2 compile to flag-0 faces in this
level. This is an observed L1S1 mapping, not a general rule for all levels.
Cuts that touch another solid room or linked portal still need a grouped edit.
The default scene face capacity has been raised from 800 to 1,024 so the
867-face first cut has a bounded destination; PC and NXDK builds pass. Its
stock-Xbox runtime allocation and headroom are not yet measured, and the
staged tree is not yet bound to the scene.

The 867-face merge is now a shared C staging function rather than assembly
inside the PC probe. It combines retained compiled faces and crater walls,
preserves original collision finalizers for untouched faces, finalizes changed
fragments as generated faces, and builds a replacement room tree. The caller
supplies persistent mesh, collision, position and source-ID buffers plus an
explicit authored metadata face for generated surfaces. A too-small output
pool leaves the mesh/tree outputs unchanged. This is still preparation only:
neither ordinary render view nor collision overlay is rebound yet.

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
