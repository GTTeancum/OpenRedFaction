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


## Hollow neighbor occlusion

Publication jobs now optionally associate one outward convex void with each neighbor solid owner. Occlusion uses P minus (solid minus void): retain the polygon outside the solid plus the portion inside both solid and void. Both regions use existing bounded scratch, and final pieces pass the same strict partition/collision rules. Unknown void owners, repeated void owners and invalid planes reject before output publication. This is intentionally one convex void per owner, sufficient for the measured roof80/air85 relationship; it is not a general ordered editor CSG engine.

The first boundary fixture caught duplicate coplanar area. Existing subtraction preserves opposite-facing contact; adding the void intersection again was wrong on that boundary. The implementation now detects that preserved contact and avoids re-emitting its area. Six cases cover both winding orientations at the roof bottom, inside the hollow prism, and above it. For a6-by8 query rectangle, opposite-facing bottom contact retains48 units, same-facing bottom clipping retains40, the Y2.75 interior slice retains28, and the Y3.25 slice retains16. Tests verify strict collision conversion, UV/material/provenance and malformed owner handling. The prior clipped-boundary tests remain separate evidence for the exposed roof strips.

This optional job data is not yet populated by the scene loader. Callers must provide the effective neighbor surface boundary, including cavity walls where relevant, and capture the new source identity before accepting saves. Current post jobs have zero voids and continue through the original subtraction path. The next integration step is constructing beam95's validated owner/neighborhood and wiring its clipped roof surfaces plus hollow occluder together; no live beam behavior is claimed yet.

Validation: checked full PC build and all122 tests pass; stock NXDK build passes. Logs: artifacts/roof-occlusion-{full-build,tests,xbox}.log. No emulator run, HDD clone or image was produced for this core-only change.


## Beam95 authored loader integrated

The scoped authored decoder now accepts UID95 in addition to the four posts. It requires the actual earlier air66, earlier air85 (five faces/eighteen corners), and exactly roof80/posts93/94 as flags-zero neighbors. The beam's upper Y bound must equal air85's lower bound. Other source IDs and unexpected neighborhoods remain rejected. It imports air85 with reversed winding and validates the resulting outward closed convex plane set; authored vertices/UVs are retained, rather than using the analytical fixture's idealized normals.

The owned asset view now exposes neighbor_voids/count. Beam loading retains the actual roof80 convex occluder paired with air85's void, and clips the neighbor surface mesh through the prior boundary routine. Parse owner/brush tables are freed before allocating publication clipping scratch. The measured asset is18876 resident bytes with1248463 peak bytes, within the existing2MiB loader budget. Peak-minus-one rejects without publishing an owner. All source, clipped neighbor and void-plane pointers remain owner-backed after parsing ends.

Installed-asset testing checks the eight compiled beam windows, real roof bottom strips (7.99999905 area before beam footprint exclusion), one owner80 void, and a real template center cut. Publication produces41 faces with resolvable references and no false exposed roof patch across the center opening. This is direct loader/core publication validation, not scene rendering, detached-body or save acceptance. The existing four-post and paired publication cases remain passing.

Remaining live integration constraints are explicit: save-identity capture still admits only the four prior post profiles, and scene selection has not been enabled for95. Destroying the beam near its supports can reveal previously fully hidden post tops: authored face IDs543 and549 have no compiled references anywhere in the installed geometry. Their authored UV/material data exist, but the current compiled-reference-based lighting/identity path needs a deliberate representation for these exposed surfaces rather than a fabricated reference. The successful center cut does not settle these end-cut cases.

Validation: checked full PC build, all122 tests and stock NXDK build pass. Logs: artifacts/beam-loader-{build,tests,full-build,full-tests,xbox}.log. No emulator or HDD clone/image was produced in this loader-only step.


## Hollow-source identity and installed beam manifest

Authored identity now uses RFAS digest domainv2 for assets with neighbor voids, hashing each void's owner, plane count and canonical float plane words. Domainv1 serialization remains byte-for-byte unchanged for existing solid-only assets. This changes the identity hash input, not the RFCP or collection directory wire format. Void ownership must resolve to an existing neighbor solid, each owner may appear once, planes must be finite/unit and the existing32-owner/32-plane limits apply. Invalid inputs preserve the caller's digest.

The immutable capture path now admits the validated beam95 loader view, requiring exactly the roof80 void. Beam capture uses loader policy2/publication policy10, while the four prior post captures retain policy1/9. Installed manifest testing covers all five sources, checks repeatability and distinct identities, resolves all eight beam windows, and verifies unchanged known UID94 digest. The beam manifest has21 compiled reference rows,7584 resident bytes and1550527 peak capture bytes, under2MiB. Its measured digest is add83239d3f01ec42cc5b85648574a8e7b84d3313a6c6e0773f96dd1608d365f.

Core tests verify that adding a void or changing its offset/orientation changes identity; relocating the same plane storage does not. Unknown owner, duplicate owner, missing pointer, over-limit count, nonfinite plane and non-unit normal reject without output mutation. Removing the optional void restores the original canonical v1 digest. All122 tests pass after a checked full build. This establishes immutable beam identity capture, not live checkpoint restore: scene binding and hidden post-cap rendering metadata remain to implement before enabling beam selection.

Stock NXDK compilation also passes. Logs: artifacts/beam-identity-{build,full-build,tests,xbox}.log. No emulator, HDD clone or image was produced.


## Hidden authored cap binding

The publication binder now has explicit DEFER_AUTHORED opt-in, admitting NEIGHBOR faces whose compiled reference is absent while requiring a valid authored owner, source face and material. It preserves authored base UVs and provenance and returns GENERATED_PENDING lighting with sentinel map/image. The origin kind distinguishes these authored surfaces from crater faces: callers must generate lighting without replacing their authored texture mapping. Existing policies still reject hidden faces. Missing non-sentinel references and retained faces without references remain errors; validation precedes output writes.

An installed beam end cut at (-4.75,2.25,2.5), radius1.05000007, produces49 faces including eight pieces of previously hidden post94 top549. Each cap passes the new binding policy, retains its actual UV/material/provenance and rejects the old crater-only policy. The earlier center-cut case still produces41 faces without hidden references. Synthetic tests also exercise rejection atomicity, invalid ownership and missing-reference distinctions.

Validation: full checked PC build, all122 tests, and stock NXDK build pass (artifacts/hidden-cap-{build,tests,xbox}.log). This is core binding evidence only; the live scene still needs explicit collision identity and generated lighting for these authored caps before beam95 can be enabled. No emulator session or capture was made.


## Completed hidden-cap chart identity

Scene-path inspection found an additional prerequisite: RFAP publication validation previously required a compiled reference for every non-crater surface and admitted generated lighting only for crater origins. The digest now supports AUTHORED_CHART, a distinct chart kind for a hidden NEIGHBOR with valid authored source token and absent compiled reference. It requires a completed canonical local 1555 tile and retained-map ordinal; absent lighting and unlit fallback reject. The origin retains authored identity, while compiled-domain mesh faces use the absent-reference sentinel. The new chart kind is serialized explicitly; existing RFAP canonical hashes are unchanged.

Generated chart ownership is unique across both crater and authored chart kinds for each owner/retained-map pair. Tests cover authored/compiled-domain equality, raw/prehashed tile equality, changed UV/projection sensitivity, absent pixels, invalid source token, retained-face misuse, fabricated compiled reference, missing map ordinal, duplicate chart identity and atomic rejection. Full checked PC build, all122 tests and stock NXDK build pass. Logs: artifacts/hidden-cap-digest-{build,tests,xbox}.log.

Live integration remains open: scene publication still requires compiled IDs for collision lookup; hidden neighbor filters need explicit ownership, scene chart capture must emit the new kind, and beam hollow-neighbor data must reach publication jobs. The existing generated-lighting stage selects absent-reference faces, which can serve hidden caps once these metadata paths are wired. No live rendering or save/reload acceptance is claimed by these digest tests.


## Owned neighbor collision properties

The authored asset view now exposes one neighbor_filters row per published neighbor face. Import uses the neighbor brush's own visible compiled face for missing-reference room ownership/state, then preserves the target authored face flags and signed portal field. Compiled counterparts provide their own room state. Validity is checked with the shared collision predicate, without converting a rejected collision response into a loader rejection or inventing ordinary-source eligibility for neighboring geometry. Roof clipping copies the matching authored owner/source row to each output child. All arrays reside in the bounded asset owner.

Installed tests compare all six post94 neighbor rows in beam95 against post94's independently loaded source filters. A payload mutation changes only hidden top549's flags and portal to -7; the beam loader retains those exact values while compiled faces remain unchanged, and restoring the input payload does not modify the owned result. The real end cut remains49 faces/eight hidden cap pieces. Full checked PC build and all122 tests pass; stock NXDK build passes. Existing known source identity tests remain passing. The new metadata is not yet consumed by scene publication; immutable capture already includes original editor/compiled section bytes, but any changed live collision publication policy must be versioned when enabled.

Beam resident memory is22360 bytes, peak1251947 under the2MiB loader budget; each existing post asset is4348 resident bytes, peak1038595. Logs: artifacts/neighbor-filters-{build,tests,xbox}.log. The next scene constraint is representing absent compiled collision IDs while preserving authored material/ownership for queries and composed collision digests. No XEMU session or screenshot was produced.


## Hidden collision identity distinct from compiled references

Full-room collision digest rows now carry metadata_id for published hidden NEIGHBOR surfaces only. Their origin.reference remains UINTMAX, their authored owner/source token stays intact, and metadata_id must join the composition's non-sentinel runtime face ID. A hidden ID cannot alias an unchanged or ordinary published surface, or another hidden owner/token/material. Split fragments of the same authored surface may share an ID. Runtime IDs remain excluded from the stable digest; existing canonical RFAC hashes remain unchanged.

Tests verify runtime-ID relocation invariance, missing/mismatched IDs, absent authored token, misuse as retained data, alias collisions on either side of the hidden row, shared valid split IDs and collision-filter sensitivity. All122 tests pass after full PC rebuild; stock NXDK build passes. Logs: artifacts/hidden-collision-id-{build,tests,xbox}.log. This enables validation only; it does not yet allocate or bind runtime IDs in the scene.

Concrete next integration: campaign_alpha_refresh_overlay currently calls rf_geometry_material_collision_bind, which resolves every ID through rf_geometry_get_face; campaign_body_query uses rf_geometry_body_surface with the same assumption. Player ground material lookups also call rf_geometry_get_face directly (scene.c around8037/13798). These consumers need an explicit runtime material/UV lookup for hidden IDs before scene publication can enable them. Texture sampling must use the cap's actual authored positions/UVs, not a visible sibling's polygon. The overlay and composition themselves already accept non-sentinel opaque metadata IDs. No emulator run or capture was made.


## Explicit runtime polygon material lookup

material.h/material.c now provide rf_geometry_material_runtime, an opt-in wrapper around the existing compiled collision material view. Borrowed runtime surface rows carry a unique non-sentinel ID outside the compiled face range, compiled texture index, plane, positions and authored UVs. No geometry file metadata is fabricated or extended. Lookup resolves compiled texture and renderer slot for downstream material responses; the indexed collision backend dispatches original faces through the existing path and runtime faces through their own polygon UV interpolation and the same original image sampler.

Binding validates bounded rows (maximum1024), non-aliasing IDs, finite coordinates/UVs, unit normals and texture slots before publishing the backend. It allocates no memory and copies no pixels. Parent authored polygons may serve clipped collision children, because the collision tree determines the actual contact; the material sampler determines UV only. Owners must keep rows, geometry, pixels and workspace alive and immutable. This API does not itself validate collision topology or activate a scene registry.

The new runtime_surface_material test mixes compiled and runtime faces using remapped texture slots and deliberately different UVs: the compiled polygon reads transparent red while the runtime cap reads opaque green at the same XY location. It checks missing IDs/images, invalid slots/normals/UVs, duplicate/compiled-alias IDs, outside-polygon samples, wrong bitmap, preserved outputs/backend on failures and ID relocation. Checked full PC build and all123 tests pass; stock NXDK build passes. Logs: artifacts/runtime-surface-{build,tests,xbox}.log.

Scene consumers still need to install owned runtime rows at authored asset setup, route campaign_alpha_refresh_overlay through this wrapper, and use its texture/slot lookup for body/ground material responses. Lighting and checkpoint integration remain gated. No XEMU run or screenshot was made.
