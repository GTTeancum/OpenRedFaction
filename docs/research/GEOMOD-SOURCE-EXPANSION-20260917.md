# GeoMod source expansion census

The scene loader remains scoped to ctf06 posts93/94/96/97. Their successful cutting does not establish arbitrary room destruction. tools/inspect_geomod_source_candidates.py now enumerates flags-zero4..32-face brushes with compiled ownership exclusively in room3, transforms authored vertices with the loader's float rounding, measures exact-edge closure/convexity, and records nearby brush AABBs, flags and editor order. It is a diagnostic selector, not a general CSG evaluator. Nonzero flags are retained without conflating detail brushes (flags4) with air (flags2).

Input: existing full955-brush export artifacts/future-vehicles-re/ctf06-editor-brushes.json and installed compiled geometry. Output artifacts/geomod-source-candidates.json includes input-export and geometry hashes. All37 filtered candidates are closed and convex. The four currently supported posts reproduce their proven one-air/three-solid neighborhoods. None of the other filtered candidates has that same single nonzero-brush neighborhood. AABB intersections can include boundary-only contact and do not prove solid intersection.

## Next useful source: beam95

The authored beam is a six-face24-corner solid with bounds X[-5.25,-4.75], Y[2,2.5], Z[-4,4]. Its eight-unit length makes larger detached pieces geometrically possible; this is not an extraction result. The existing exporter consumed the exact877943-byte brush section and emitted ctf06-brush-95.rgm. Topology is closed/outward convex, not cavity mode. Eight compiled faces157/158/159/160/161/162/163/168 map to authored IDs554..559, all room3/material3.

Its flags-zero neighbors are roof80 and posts93/94, all touching at boundary-only AABB intersections. Air66 encloses the beam; extra air85 touches Y2.5 only. Editor ordering matters: roof80 index81, air85 index82, beam95 index95. Air85 therefore affects the roof before the beam is added. Merely admitting UID95 and treating the full original roof80 as an unchanged solid is not established as correct for exposed neighboring surfaces. The current post loader deliberately rejects this neighborhood.

Next implementation boundary: resolve the roof80-minus-air85 neighbor representation and retain the actual compiled beam windows/provenance before adding a beam profile. Verify uncut reconstruction against compiled geometry and then cut across the beam/post contact, including collision and support. Do not remove source/neighborhood guards globally or use repaired caps as a shortcut. This can extend core gameplay in the same enemy-free room without advancing the campaign.

Validation this turn: census agrees with all four current source profiles; beam export passes exact parse, closure and convexity and retains direct compiled ownership. No game source behavior changed, no build/emulator ran, and no new HDD/image was created. Larger live fragments remain unverified.


## Roof interface resolved from compiled geometry

Authored roof80 is an outward triangular prism with bottom Y2.5, peak Y3.5 at Z0 and span Z[-4,4]. Air85 is an inward triangular prism with bottom Y2.5, peak approximatelyY3 at Z0 and span Z[-3,3]. The solid-only export utility correctly rejects air85's inward orientation; it is not malformed or suitable for pretending to be an additive neighbor.

At beam95's top planeY2.5, air85 removes the roof center for Z(-3,3). Only the two end strips Z[-4,-3] and Z[3,4] meet the beam. Total beam top area is4, exposed top area is2.9999998807907104, and geometric roof contact area is1. This is geometry contact, not a recovered dynamic-support rule. Compiled beam top face168 (authored556) explicitly covers the open middle. Compiled roof underside faces164..167 (authored478) cover7 square units: the8-unit outer roof strips minus the1-unit beam contact. No raw32-unit roof bottom may be published over the hollow center.

New tools/check_geomod_beam_roof_boundary.py loads installed geometry directly, checks the exact compiled face identities/plane and areas, then compares25600 point classifications against separate exposed-beam and exposed-roof masks. Points are cell centers chosen away from the relevant edges; this does not claim every boundary float is exact. All checks pass. Report artifacts/geomod-beam-roof-boundary.json includes geometry hash6437d8f2fc5fc9535cabb928c29e48b8428d6a3a22a414b3be88aef39fe7117b.

The scene publisher currently exposes raw neighboring brush faces after a cut. Beam admission therefore needs the already-subtracted roof boundary (or an equivalent proven ordered-Boolean representation), including the correct UV/provenance for newly uncovered end patches. The boundary audit supplies a concrete uncut reference and rejects the tempting but incorrect full-roof-bottom approximation. Full volume representation, allocation budget, beam/player collision, live extraction and Xbox acceptance are still open. No gameplay change or emulator launch occurred in this audit.


## Shared C boundary-clipping implementation

Added rf_geomod_publication_clip_neighbors. It subtracts convex void volumes from selected existing neighbor surfaces, matching each void's owner to the surface origin.owner. It reuses the existing bounded publication work banks, polygon subtraction and strict partitioner, preserving generation, material, UVs and source provenance. It allocates nothing; callers must budget/provide the existing351124-byte work buffer and disjoint outputs. All outputs remain unchanged on validation, clipping or capacity failure.

The API deliberately does not generate interior cavity surfaces or a representation of the remaining solid volume. The caller must supply these separately wherever needed. This distinction matters for roof80-minus-air85: clipping its bottom produces the exposed end strips, but does not by itself solve the complete roof Boolean or justify treating its whole original prism as a valid occluder.

The C regression uses the actual roof80 bottom coordinates and air85 triangular-prism cross-section. It produces8 square units of roof-end strips before subtracting the beam footprint, corresponding to the measured7 visible plus1 beam-contact units. It verifies every roof vertex stays outside the open Z(-3,3) band, all output faces satisfy strict collision conversion, UVs/provenance/materials survive, an unrelated owner retains all32 square units, zero voids preserve both source faces, and capacity/malformed-plane errors leave all output arrays and the output view byte-identical. This is a targeted reconstructed boundary operation, not evidence of general editor CSG or live beam support.

Scene-loader wiring, complete neighbor representation and stock64MiB runtime validation remain open; the source whitelist is unchanged.

Validation: checked full PC build and all122 tests pass; stock-profile NXDK build passes. Logs: artifacts/roof-boundary-{full-build,tests,xbox}.log. No emulator launch or new HDD/image was needed for this unintegrated core addition.
