# Rail scanner silhouettes

## Implementation

The missing presentation path is now source-written: the scanner's existing
nearest-32 eligible living NPC markers can draw a flat green body silhouette
from the actor's admitted skeletal geometry. This changes presentation only.
The scanner still uses its existing 100-unit spherical range and requires the
body center to project strictly inside the viewport. Ammo, alternate-input
edges, ownership, holster/death/switch cancellation and weapon damage are
unchanged. Source-reviewed only; no build, runtime, images or functional tests
were performed for this slice. Compilation belongs to the parent hourly rollup;
visual and functional coverage belongs to testers.

The actual source gap was `scene_scanner_draw` in `scene.c`: eight screen-space
rectangles per selected center and a comment deferring silhouettes. The
existing `scene_scanner_collect` and `rf_weapon_scanner_select` already perform
selection without portal visibility or cover queries. This pass preserves that
through-wall policy; it does not reuse ordinary NPC room culling, modify room
visibility, reveal script-hidden/dead actors or implement a world-depth test.

## Geometry and ownership

- Requalify the complete marker handle, registry identity, body allocation,
  health and hidden flags immediately before drawing. Stale owners receive
  neither silhouettes nor fallback brackets.
- Resolve the registered current model pose through `campaign_model_pose`.
  Reject corpse-owned/unloaded models, unavailable geometry, invalid bone
  counts, mismatched model/pose bone counts and nonfinite matrices.
- Use a complete published bone generation. An older complete pose is allowed
  for a culled actor; mixed bone generations fall back. This pass never
  advances animation, evaluates a new pose, demand-loads motions, edits
  gameplay state or writes the shared collision skin cache.
- Match ordinary NPC body rendering's first/highest-detail LOD per SUBM and
  skip negative material slots. Geometry, bind transforms, world placement and
  skeletal matrices all come from existing scene owners. Held weapons and
  independent accessories are not additional scanner targets in this pass.
- Reuse `rf_model_geometry_render_batch` and `rf_preview_visit_model` for
  skinning, back-face routing, near-plane and viewport clipping. The latter
  already builds on recovered model projection/local-view and triangle paths
  documented at 0x52f154, 0x547485, 0x52f5b2 and 0x52f6c1. Those addresses ground
  the reused geometry machinery, not the scanner's appearance.

## Stock64 limits and renderer contract

Per HUD pass, admit at most 16,384 source vertices and 8,192 source triangles,
and submit at most 16,384 clipped triangles. Each selected owner admits at most
128 batches; each batch keeps the existing 4,096-vertex scratch limit. Metadata
traversal stays inside the existing 128-LOD file bound and 32-target selection.
Whole-owner source work is preflighted and charged before skinning, nearest
owners first. These limits are explicit port choices, not retail constants.

No heap allocation, new assets/textures, GPU buffers, retained geometry copy or
world/viewmodel mesh growth is introduced. The only new persistent storage is
eight diagnostic words (32 bytes). The largest new local array is the existing
50-bone preparation shape: 2,400 bytes plus 100 bytes of generation tags.
Other fixed locals and existing model/clip call frames remain stack-owned;
no compiler stack-use measurement was performed.

The pass serially borrows the scene's existing NPC render scratch, index array
and clip pool, after ordinary world/model submission. It never retains their
pointers across calls. The synchronous visitor copies a final three-vertex
packet into the existing solid HUD sink (`0x18000`), with no image reference.
On Xbox, existing HUD begin/flush/end fences and pushbuffer word limits remain
unchanged. The existing HUD sink disables depth testing and writes. The PC
staging sink receives the same packet; no platform renderer API changes.

Unavailable models/poses, clipping-capacity failures or exhausted frame budgets
retain the existing center brackets and do not fail the game frame. If an
earlier batch of that owner already drew before a later clipping failure or
emitted-triangle cap, the partial fill can remain alongside its fallback
bracket for that frame. A platform sink error is propagated rather than hidden,
so the normal renderer error/drain contract remains authoritative.

## First-pass presentation limits

The fill is opaque flat green, chosen so body triangle overlap does not build
brightness. Visible and wall-occluded actors have the same color. This is a
useful silhouette presentation, not a recovered thermal shader, edge outline,
heat map or claim of retail parity. It intentionally preserves the existing
center-based target eligibility, including omission of partly onscreen bodies
whose centers are offscreen. Culled animation can show its last complete pose.
The scanner label is drawn last so a body cannot obscure it.

`rf_scene_scanner_silhouette[8]` reports, per scanner HUD call: completed
silhouettes, emitted triangles, admitted source vertices, admitted source
triangles, bracket fallbacks, budget fallbacks, geometry errors and last error.
The prior `rf_scene_scanner[4]` selection diagnostics remain unchanged.
