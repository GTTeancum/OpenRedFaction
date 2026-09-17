# GeoMod source expansion census

The scene loader remains scoped to ctf06 posts93/94/96/97. Their successful cutting does not establish arbitrary room destruction. tools/inspect_geomod_source_candidates.py now enumerates flags-zero4..32-face brushes with compiled ownership exclusively in room3, transforms authored vertices with the loader's float rounding, measures exact-edge closure/convexity, and records nearby brush AABBs, flags and editor order. It is a diagnostic selector, not a general CSG evaluator. Nonzero flags are retained without conflating detail brushes (flags4) with air (flags2).

Input: existing full955-brush export artifacts/future-vehicles-re/ctf06-editor-brushes.json and installed compiled geometry. Output artifacts/geomod-source-candidates.json includes input-export and geometry hashes. All37 filtered candidates are closed and convex. The four currently supported posts reproduce their proven one-air/three-solid neighborhoods. None of the other filtered candidates has that same single nonzero-brush neighborhood. AABB intersections can include boundary-only contact and do not prove solid intersection.

## Next useful source: beam95

The authored beam is a six-face24-corner solid with bounds X[-5.25,-4.75], Y[2,2.5], Z[-4,4]. Its eight-unit length makes larger detached pieces geometrically possible; this is not an extraction result. The existing exporter consumed the exact877943-byte brush section and emitted ctf06-brush-95.rgm. Topology is closed/outward convex, not cavity mode. Eight compiled faces157/158/159/160/161/162/163/168 map to authored IDs554..559, all room3/material3.

Its flags-zero neighbors are roof80 and posts93/94, all touching at boundary-only AABB intersections. Air66 encloses the beam; extra air85 touches Y2.5 only. Editor ordering matters: roof80 index81, air85 index82, beam95 index95. Air85 therefore affects the roof before the beam is added. Merely admitting UID95 and treating the full original roof80 as an unchanged solid is not established as correct for exposed neighboring surfaces. The current post loader deliberately rejects this neighborhood.

Next implementation boundary: resolve the roof80-minus-air85 neighbor representation and retain the actual compiled beam windows/provenance before adding a beam profile. Verify uncut reconstruction against compiled geometry and then cut across the beam/post contact, including collision and support. Do not remove source/neighborhood guards globally or use repaired caps as a shortcut. This can extend core gameplay in the same enemy-free room without advancing the campaign.

Validation this turn: census agrees with all four current source profiles; beam export passes exact parse, closure and convexity and retains direct compiled ownership. No game source behavior changed, no build/emulator ran, and no new HDD/image was created. Larger live fragments remain unverified.
