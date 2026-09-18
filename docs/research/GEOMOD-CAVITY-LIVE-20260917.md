# Developer wall destruction on PC and stock Xbox

This implements one local cavity-wall test, not general map destruction. Estimated project progress remains about50% overall and88% GeoMod.

## Admission and scene integration

The cavity owner retains all954 other authored brush bounds. Each cutter AABB must avoid those bounds and project wholly into one ordinary compiled wall window whose authored plane it intersects. Crossing a compiled window boundary, neighboring brush or excluded portal rejects conservatively. All stored cutters are checked before candidate publication. Tests cover accepted wall, interior-only, beam overlap, window seam and malformed bounds; rejection leaves the reference output unchanged.

Source66 selects the inward terrain core and cavity publication path. It does not install solid-fragment extraction. Source material/chart identity remains8ab755fd3d28b87ee0b0c6114570b818e1b1f00651c3755fb5f69f3d4977e325. Live source/manifest resident memory is99080bytes and capture peak1912303bytes, below the unchanged2MiB cap.

The initial candidate spawn was below the actual floor and fell; compiled face1004 puts the walkway atY2. The corrected spawn(-24,3.6,8) settles at(-24,2.879973,8). Actual eye height drives the replay aim. At frame267 the ordinary rocket hits(-33,4,8) exactly.

ctf06 has default hardness100 and no destructible region at this wall. Source66 explicitly adds a developer-only radius2 spherical hardness65 patch centered(-33,4,8); installed data and default hardness remain unchanged. This is a test fixture, not a claim that the retail wall was destructible. Existing post/beam profiles are unchanged. The scene settings digest includes this region.

## Verification

- `python -B tools/check_cavity_wall.py`: measured settled spawn, untouched aim control and ordinary launcher shot. One committed cut;185 published faces/885vertices; player survives and stays on walkway. Before/after native PC framebuffers inspected separately: intact panel versus visible dark rock crater, with surrounding wall/weapon/HUD preserved. Audio quality not inspected.
- All123 CTest cases pass; stock NXDK compile/link/XBE/ISO succeeds.
- `python -B tools/xemu_render_check.py --dev-room --spawn --level ctf06.rfl --archive levelsm.vpp --authored-source 66 --input artifacts/cavity-live/shot.bin --seconds 240` passes76 checks at400frames.
- Native evidence: `artifacts/xemu/render-20260917-235356`. Framebuffer inspected directly and shows crater/retained wall/launcher.4024 free physical pages =15.71875MiB at endpoint, not a peak free-memory measurement. Harness restored disc and closed its emulator; no desktop input/capture used.
- Earlier core tests separately verify an opened crater collision ray, blocked distant control,134 retained-window areas and preservation of656 unrelated room collision faces. Live player entry into this new wall crater has not been tested.

## Remaining work

Initial wall save/reload was not integrated in8ca75883. The continuation below adds the empty-piece contract. Deeper tunnels, traversal, general brush ordering and cross-window/portal cuts remain open. No blanket admission was added. No new image was uploaded to GitHub.


## Cavity checkpoint continuation

The RFDS2 owner extension now permits zero solid neighbors only for the admitted single UID66/fourteen-plane cavity profile; unrelated solids and collections retain their old minimum. Unit tests cover the exact profile and reject nearby UID/plane-count mismatches atomically. Source identity, publication/collision/material digests, history bounds, admission settings and player standing checks remain mandatory.

Restore reconstructs the inward core without a solid extraction callback or a detached-piece registry. Any nonempty piece payload for this cavity rejects before publication. Null registry commit/placement/support paths retain their existing empty semantics; no fictitious pieces are created.

`tools/check_cavity_wall.py` now saves the first shot with player state, reloads it, aims atY5 and fires a second overlapping rocket, then compares the entire checkpoint with uninterrupted play. PC produced identical8390-byte two-cut saves. The rendered continuation was inspected: the initial crater remains, and the upper overlapping cut expands it, with the wall/launcher/HUD intact. All123 tests pass and stock NXDK builds successfully. This proves one overlapping two-shot history, not arbitrary tunnel depth or general repeated-cut fidelity.
