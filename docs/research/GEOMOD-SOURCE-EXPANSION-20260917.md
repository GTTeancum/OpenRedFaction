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


## Scene-owned runtime polygons and query routing

The scene publication owner now retains bounded arrays for128 runtime surfaces/512 corners, copied from missing-reference authored neighbor polygons. Runtime ID is compiled-face-count plus the globally authored face token, checked for overflow and conflicting ownership/material/polygon/filter definitions. Matching shared neighbors across source owners deduplicate. Collision plane construction uses the normal convex-face validator. Positions, UVs, origins and filters live in the publication owner, and its existing4MiB budget includes the added arrays.

campaign_alpha_refresh_overlay binds the runtime-aware material backend when rows exist, preserving the original instrumentation for compiled samples. campaign_body_query routes surface callbacks through campaign_body_surface, resolving runtime IDs to their own compiled texture/render slot/surface class; movers and compiled IDs retain the original callback. Ordinary campaign ground material already comes from this body contact. This does not yet publish hidden IDs: scene publication still rejects missing references pending lighting/checkpoint integration.

Scene tests check copied ownership, shared-row deduplication, conflicting-owner rejection, missing ID atomicity and exact runtime body material selection. A full-suite run exposed a legacy fixture without s.geometry; publication_open now explicitly rejects that input and the fixture supplies its real geometry. All123 tests then pass, and the stock NXDK build passes. The actual post cut/save/reload PC replay also passes with byte-identical uninterrupted player/destruction continuation. Logs: artifacts/runtime-scene-{build,tests,xbox,continuation}.log. This run checked replay state, not new visual behavior; no XEMU or screenshot was produced.


## Installed beam end cut through scene publication and lighting

Scene publication jobs now forward their authored neighbor-void data, including other sources in grouped preparation. The UID95 path opts into hidden authored bindings; existing post profiles keep their previous admission policy. Hidden neighbor fragments resolve the owned runtime row by authored owner/token, use its collision ID/filter, preserve material/UV/provenance and set only the render face's compiled-reference field to the absent sentinel for new lightmap generation. Publication finish requires a completed chart for every pending-lighting face, including authored caps; unchanged authored chart words remain exact.

The installed beam scene test cuts at(-4.75,2.25,2.5), publishes49 faces including eight top549 fragments, bakes eight retained maps and prepares277 draw vertices. All base vertex/UV words remain unchanged by staging. The hidden caps keep material3 and receive atlas image2 with valid tiles. With every other chart complete, clearing only a cap's image rejects finish; restoring it permits commit. A downward full-world overlay ray from(-5,2.1,2.5) hits the exposed cap at fraction0.1 with upward normal and runtime ID compiled-face-count+549. This exercises actual scene publication/lighting/draw preparation and collision, not merely core clipping.

Full PC rebuild and all123 tests pass; the additional cap-only completion guard passes its rebuilt target; stock NXDK build passes. Logs: artifacts/beam-scene-{build,tests,xbox}.log and beam-scene-cap-guard-{build,test}.log. GPU rendering and XEMU acceptance have not been performed. Live beam selection stays disabled until scene digest capture, retained material journals and checkpoint reconstruction support authored cap charts and runtime collision IDs. The shared lighting path currently supplies its existing generated noise/dynamic-light treatment; cap visual fidelity remains unverified.


## Authored-cap retained lightmap journal policy

Retained material digest policy2 adds RFRM digest domainsv3(single source)/v4(collection). Map token0 still identifies crater substrate; authored maps use compiled-texture-index+1, qualified by verified immutable source identities and a caller-captured allowed-token list. The list is bounded to128 unique nonzero/non-sentinel tokens and must never be supplied by the save itself. Unused list entries/order are not journal state. Historical unused maps remain hashed, including material token, packing/projection, exact base RNG sequence and regenerated1555 pixels.

Hidden NEIGHBOR faces with absent compiled references and valid authored owner/token require a mapped nonzero authored material. Crater faces still require token0 and valid cutting-source ownership. Ordinary retained/visible neighbor faces retain the no-map sentinel. Legacy policy1 rejects authored tokens/lists and keeps existing hashes byte-identical. This changes only the digest API; current checkpoint codecs and scene capture still use policy1 until explicitly wired.

Tests cover mixed crater/cap maps, single/collection domain separation, allowed-list reorder invariance, missing/duplicate/invalid tokens, missing authored identity, visible/retained misuse, material substitution sensitivity, wrong map domain, historical unused cap maps and damaged RNG seeds. Full checked PC build, all123 tests and stock NXDK build pass. Logs: artifacts/cap-journal-{build,tests,xbox}.log. Next: capture trusted tokens from the immutable manifest, write/read them as logical map materials, and regenerate the same cap charts during checkpoint restore. No live save acceptance or visual test is claimed.


## Installed beam three-domain digest capture

Scene digest capture selects retained-material policy2 for UID95 and derives allowed logical material tokens from the immutable manifest. It maps renderer slots back to the smallest matching compiled texture token, preserving deterministic identity when slots deduplicate. Existing post profiles remain policy1. Authored cap maps preserve their base material and completed local lightmap content; their generated chart descriptors use AUTHORED_CHART with true owner/source token and a stable authored chart name. Crater chart names/hashes stay unchanged.

Hidden collision rows now resolve their runtime IDs against the scene-owned registry, checking owner/token and actual texture-slot correspondence before invoking full-room collision hashing. Cap-map matching also requires the face's material, not only matching plane/projection. A trusted allowed-token list is assembled from original manifest data, never from checkpoint bytes.

The installed beam capture test uses production authored asset setup, piece-extraction registration, real template end cut and private lighting. It captures49 faces/eight hidden cap fragments/eight maps across publication, composed collision and retained material domains, repeats them exactly, and verifies atomic rejection when a wood cap map is substituted with substrate or its seed is corrupted. Peak digest scratch is335040 bytes under512KiB. Full checked PC build, all123 tests and stock NXDK build pass. Logs: artifacts/beam-digest-{build,tests,xbox}.log.

This verifies capture only. RFCP extension policy admission, logical map serialization and reconstruction must still be updated before a beam save can round-trip; live source selection remains gated. No GPU/XEMU run or screenshot was made.


## Beam checkpoint writer and reconstruction round-trip

The128-byte owner extension keeps its wire layout and now accepts material policy2 only when the independently reconstructed expected record explicitly requires2. A zero expected policy retains legacy1 compatibility; unknown policies and policy mismatches reject. Scene capture sets this expectation from the loaded source profile. Checkpoint map+40 now writes logical substrate0 or a trusted manifest-derived authored texture token, never a renderer slot.

Journal import checks the expected profile policy, resolves authored tokens through original material manifests, validates the existing88-byte map geometry/projection via a private token-neutral copy, and reconstructs the correct runtime material. Hidden cap faces require valid authored identity, absent compiled reference and exact material/plane/containment matches; crater and ordinary source paths retain their previous restrictions. The trusted lookup handles substrate0 without needing authored texture context, preserving the standalone legacy journal adapter. Its synthetic fixture now declares policy1 explicitly.

The production beam-end fixture writes a2486-byte RFDS payload, reconstructs a private scene, compares all three expected digests, commits it and rewrites exactly the same2486 bytes. Its cap map stores token4 (compiled texture3). Unknown tokenUINTMAX and policy downgrade reject without leaving a pending candidate. Core extension tests independently verify explicit policy2 opt-in and legacy/unknown-policy rejection. This is CPU checkpoint acceptance, not RFCP/player transport or XEMU acceptance.

Full checked PC build, all123 tests and stock NXDK build pass. The existing actual post cut/save/reload replay remains byte-identical to uninterrupted continuation. Logs: artifacts/beam-checkpoint-{build,tests,xbox,post-regression}.log. Next: enable scoped beam95 source selection, update its finalized immutable policy revision, and verify rendered PC/native behavior and continued edits. No new capture or emulator session was made.


## Live beam95 admission and native rocket (2026-09-17)

Enabled explicit source95 selection in PC and the native harness. The default remains94; beam collections are rejected before placement/owner allocation because shared beam/post edits are not integrated. Loader3/publication11 now identify the live beam contract (hollow roof, inherited neighbor filters and authored cap charts); the established post policies and identities are unchanged.

`tools/check_beam_continuation.py` uses the measured retreat eye and sphere-contact aim at(-4.699,2.25,2.5). A normal rocket produces one committed cut, one detached piece and a2954-byte RFCP. The retained lightmap journal has tokens[0,0,4]: crater substrate plus the exposed authored wood surface, under material policy2. Saving after600frames and reloading for120updates reproduces the uninterrupted720-frame checkpoint byte for byte. This checks the player and destruction together, beyond the previous CPU-only beam fixture.

All123 PC tests and NXDK build pass. Native stock64MiB run `artifacts/xemu/render-20260917-171736` passes76 checks with an exact PC/Xbox checkpoint. Its native framebuffer and the PC output were inspected: a cut in the upper crossbeam and a detached chunk below, with the adjacent post, roof and weapon still present.3982 free pages at the endpoint is about15.55MiB; this is not a minimum-memory measurement. No screenshot was published to GitHub.

Logs: `artifacts/beam-live-{build,tests,xbox,replay,native}.log`; replay report `artifacts/beam-live/report.json`. Broader cuts, the next rocket after restore, closer cap visual inspection and combined beam/post destruction remain open. This run does not establish general architectural destruction fidelity.

Native reload `artifacts/xemu/render-20260917-171927` passes75 checks. It loads the Xbox-created checkpoint and advances121 replay frames (120updates); the resulting RFCP matches uninterrupted PC control bytes exactly. The native restored framebuffer was inspected and retains the beam opening, fallen chunk and surrounding scene.3997 free pages at the endpoint is about15.61MiB. Both harness sessions exited and restored the staged disc; no XEMU process remained. Reload log: `artifacts/beam-live-native-reload.log`.


## Second beam rocket after restore (2026-09-17)

`python -B tools/check_beam_continuation.py --next-shot` now turns from the first end to the opposite beam end, aiming the projectile center at(-4.699,2.25,-2.5), then fires at restored frame100.301 replay frames supply300 updates after frame0 reconstruction. The second cut produces two live retained fragments,37 published faces/180 vertices, and a4870-byte RFCP. Its six retained maps have tokens[0,0,4,0,0,4], preserving both authored wood charts. The tool requires the exact cut count, fragment count, material token sequence and byte equality against uninterrupted900-frame PC playback.

Stock64MiB native run `artifacts/xemu/render-20260917-172307` loads the prior Xbox-created save and passes76 checks. Its RFCP independently matches `artifacts/beam-next-shot/control.rfcp` byte for byte. The native framebuffer was inspected: both beam ends are opened, the retained middle span and adjacent posts/roof remain, and debris is visible below.3706 free pages at the endpoint equals14.48MiB; no minimum-memory claim. Log: `artifacts/beam-next-shot-native.log`.

A separate process-local PC approach (`artifacts/beam-close/close.bin`) walks90 frames at forward.8 and pitches up over60 frames. The measured endpoint is(-2.749997,-.401361,2.5). The native PC capture `close.ppm` shows the cut boundaries at close range, but the upward-facing exposed post cap is edge-on from this floor position. Detailed cap visual acceptance remains open and needs a viewpoint above the cap. No desktop input or original-game reference capture was used; no GitHub images were uploaded. Connected beam/post editing remains disabled pending shared-boundary integration.


## Connected-neighbor boundary primitive (2026-09-17)

Inspection found that `rf_geomod_publication_build_groups` deliberately aggregates independent publications; it does not resolve interactions between edited owners. In particular, an exposed post top imported by beam95 still describes the original post even after that post is cut, and beam crater surfaces are still occluded by the original post solid. Enabling the existing group switch would therefore preserve invalid boundaries. The single-beam restriction remains.

Added `rf_geomod_publication_cut_neighbors`: subtract the selected owner's ordered star-cut union from its authored boundary while preserving other owners, material/UV/provenance and generation. This reuses the existing bounded publication workspace and cutter tetrahedralization without allocation or workspace growth. It atomically copies outputs only after all clipping and convex partitioning succeeds. This is the boundary component, not a completed connected-source scene implementation.

The publication test covers a rotated.3-by1.3 cutter through a4-unit-square surface, a duplicate cut and a second overlapping cut shifted.2 alongX. Independent expected retained areas are3.61 and3.3852; the other owner's coincident surface remains area4. A61-by61 point grid per case compares output coverage with inverse-rotation cutter membership, excluding numerical boundary points, and detects missing or duplicate interiors. All output polygons pass strict collision construction; UVs, hidden authored IDs and materials are preserved. Capacity failure and nonstar rejection preserve tested output sentinels; zero cuts preserve the original two surfaces.

All123 PC tests and NXDK build pass. Logs: `artifacts/neighbor-cut-{build,tests,xbox}.log`. No emulator run was needed for this unconnected core primitive, and no new live behavior or visual acceptance is claimed.

Next: represent edited neighbor occlusion as original solid minus its cut union, reconcile newly exposed shared surfaces without duplicates, then stage both boundary and occluder changes with the live collection transaction and checkpoint reconstruction. Authored window/hidden-face ownership and material policy selection must be audited before removing the beam-group guard.


## Edited neighbor occlusion primitive (2026-09-17)

Added `rf_geomod_publication_occlude_neighbor`, completing the complementary volume operation for an edited neighbor: retain polygon area outside the original solid, plus area inside that solid which lies in an authored void or the ordered cut union. Authored void wins before cutters; each cutter tetrahedron subtracts earlier tetrahedra, avoiding duplicate interior area for overlapping or repeated cuts. Reverse-facing coplanar contact follows the existing polygon-subtraction convention and is not emitted again. Caller-selected polygons retain UV/material/provenance and generation; no allocations or workspace growth are introduced, and final output copy is atomic.

Seven independent area/coverage cases use a1.6-by1.6 solid section inside a2-by2 surface: no cut, one rotated cut, duplicate cuts, overlapping cuts, a void entirely inside the cut union, a disjoint void, and a void with no cuts. Expected visible areas are1.44,1.83,1.83,2.0548,2.0548,2.0948,1.48. Each case also checks a61-by61 point grid against analytical inverse-rotation membership, with numerical polygon-edge hits excluded. Every generated face passes strict convex collision construction. Same-facing and opposite-facing contact areas are1.48 and4, respectively; partition count is deliberately not prescribed. Capacity exhaustion and a mismatched void owner reject without changing tested output sentinels.

All123 PC tests and NXDK build pass (`artifacts/neighbor-occlusion-{build,tests,xbox}.log`); the final strengthened contact-area assertions also pass the targeted publication test. This is still a core operation, not live connected destruction. No new emulator/visual acceptance is claimed. Integration must stage neighborhood boundary changes and edited-solid occlusion together, retain shared authored IDs, remove duplicate shared surfaces, and reconstruct the same result during collection checkpoint restore before admitting beam/post groups.


## Connected publication and scene candidate (2026-09-17)

Added `rf_geomod_publication_build_connected` for up to four sources with disjoint original interiors and shared boundaries. Referenced selected sources defer static occlusion; generated crater and exposed neighbor pieces then pass through their current solid-minus-void-minus-cuts volumes. Exposed surfaces owned by another selected source are additionally clipped by that owner's own cuts. Retained compiled windows keep their existing path. Independent group publication is unchanged. Caller-supplied neighborhoods must describe the original solids of their referenced owners; this does not support arbitrary overlapping source solids.

The separate connected workspace is915256bytes, with bounded primary/filter work and two scratch banks, no internal allocation. The scene allocates it only for a multi-source collection containing beam95. Opening, candidate reporting, composition budget, final commit checks and owner cleanup all account for the extra allocation. Hidden authored-cap binding now depends on the imported runtime-surface registry, rather than which source happens to be selected.

Installed geometry test: cut beam95 at(-4.75,2.25,2.5), exposing eight pieces of post94's authored top549; then cut post94 at(-4.75,1.9,2.5). The old independent group builder retains that destroyed top. Connected publication removes all eight pieces and builds64faces/303vertices passing strict collision construction. Output-capacity failure preserves tested output sentinels.

The scene candidate fixture assembles the same two installed owners and deliberately selects post94 while beam95 exposes the hidden cap. Both pre/post-cut candidates prepare successfully; the shared cap disappears after the post cut, generated binding is admitted despite the selected post, and abort leaves the active empty publication untouched. Reported owner storage2207028bytes and candidate peak2635578bytes stay within the4MiB publication budget. This tests private preparation and abort, not lighting commit, live input or checkpoint restore.

All123 PC tests and NXDK build pass. Logs: `artifacts/connected-publication-{build,tests,xbox}.log`. Beam group selection remains disabled. Next: exercise lighting/draw and collection digest/writer/restore against the connected candidate, qualify the immutable collection policy, then enable the profile and run ordinary simultaneous/serial rockets on PC and stock64MiB XEMU. Close-up exposed-cap inspection from above remains open.


## Connected lighting and collection checkpoint acceptance (2026-09-17)

Extended the connected scene fixture through atlas baking, draw subdivision, publication commit and live collision overlay replacement. After the beam cut, hidden post-top549 retains authored material3 and a generated atlas binding; a downward ray from(-5,2.1,2.5) over.2units hits it at fraction.5. After the post cut the same ray has no hit. The old eight lightmap records and every retained rectangle's base pixels remain byte-identical while three new maps are appended. The final64-face scene produces378 draw vertices, with publication peak2824618bytes inside its4MiB allowance.

A separate installed-assets fixture constructs production beam95/post94 owners and their authentic source identities, independent lighting storage and detached-piece registries. It commits each cut, writes RFDS3, privately reconstructs the collection with the new connected builder, commits the restore and rewrites byte-identical saves:2666bytes/eight maps after the beam cut;4192bytes/eleven maps after the post cut. The second save intentionally retains the now-unused authored-cap lightmap history. Both stages reject a material-policy downgrade and an unknown authored material token without leaving a pending candidate or changing the subsequent valid save bytes.

All123 PC tests pass (`artifacts/connected-checkpoint-{build,tests}.log`). This turn changes tests only; production source is the preceding NXDK-verified commit. These CPU checks cover lighting/draw preparation, collision, collection digest reconstruction and save commit, not GPU rendering, live weapon dispatch or player RFCP continuation. No native or visual claim is added.

Next: admit an explicit beam95/post94 developer profile, qualify its reconstruction policy, then exercise ordinary rocket dispatch (including a blast touching both owners), PC/player save continuation and stock64MiB native validation. The selector and source-factory guards still block live beam groups until that activation is implemented. Above-cap visual inspection remains open.


## Live connected profile and simultaneous blast admission (2026-09-17)

Enabled the explicit developer profile `--authored-source95 --authored-sources2`, mapping to ordered owners[95,94]. Single-source selection and the existing post pairs retain their mappings; other beam collections remain rejected. Beam immutable publication policy is now12 (loader3), qualifying connected-neighbor semantics. Transitional beam saves from policy11 intentionally fail source identity; established post-only identities remain unchanged.

`tools/check_beam_continuation.py --connected` fires the existing ordinary rocket at the beam/post joint. The first attempt exposed a real admission gap missed by the CPU fixtures: editing two owners with production debris registries reserves16228560bytes (15.48MiB), exceeding the previous13MiB subsystem cap, and correctly rolled back both owners. Added a trace diagnostic for rejected group reservations. The connected profile now has an explicit16MiB destruction ceiling for edits and collection restore; other profiles retain their existing ceiling. This is a bounded subsystem reservation within stock64MiB, not an increase to Xbox RAM or an allocation of16MiB. Publication still has its4MiB bound and each debris registry2MiB.

The PC first blast now atomically records[95,1,94,1] at one room revision, producing one detached beam fragment and a4356-byte RFCP. Its lightmap tokens are[0,0,0]: the post top is also cut, so no surviving wood cap is generated there. Save/reload matches uninterrupted720-frame playback exactly. `--connected --next-shot` turns to the other beam end, cuts the beam again while post94 remains at one cut, and produces6272bytes with tokens[0,0,0,0,0,4] and two retained fragments. Reload plus the second rocket matches uninterrupted900-frame PC state. The PC framebuffer was inspected: the attached post top and beam are both cut, with debris below.

All123 PC tests pass. Initial native run `artifacts/xemu/render-20260917-175329` reached600frames with PC-equal gameplay words and an exact4356-byte checkpoint, but correctly remains markedFAIL because the harness still applied the old13MiB ceiling. That report is retained unchanged. Updated the harness's explicit connected-profile bound to16MiB and reran; no general memory check was removed. Native acceptance is recorded below separately.


Qualified native acceptance: `artifacts/xemu/render-20260917-175553` passes76 checks for the600-frame simultaneous beam/post blast. The4356-byte Xbox checkpoint equals PC bytes, and the native framebuffer was inspected.3551 free pages at the endpoint is13.87MiB. `artifacts/xemu/render-20260917-175750` loads that Xbox-created checkpoint and fires the second rocket over301frames; all76 checks pass and its6272-byte RFCP equals uninterrupted900-frame PC control exactly. The native frame was inspected: the first joint remains cut, the opposite beam end is now cut, and both fragments are visible.3322 free pages at the endpoint is12.98MiB. These are endpoint measurements, not guaranteed minimum headroom for arbitrary histories.

Logs: `artifacts/connected-live-native-qualified.log`, `artifacts/connected-next-shot-native.log`, `artifacts/connected-live-tests.log`, and the PC replay reports under `artifacts/connected-beam-live` / `artifacts/connected-beam-next-shot`. The harness builds NXDK successfully, restores the disc and closes its instance; no XEMU process remained after validation. No GitHub screenshot was added. Remaining scope includes broader histories, other connected owners, support behavior and above-cap visual inspection; this is not campaign-wide GeoMod completion.


## Connected reset, save and recut (2026-09-17)

Extended `tools/check_paired_reset_checkpoint.py --connected` to reuse the verified shared beam/post blast, apply the process-local crouch/use/alt reset at frame750 and save at850frames. Both owner histories reset to zero, publication returns to the original world at room revision2, both debris registries are empty, and the serialized lightmap count is zero. The reset RFCP is1192bytes.

Reloading that reset save and firing the same-direction rocket at resume frame91 produces two owner cuts at room revision3 and a new batch of three debris pieces. The5096-byte result exactly matches uninterrupted1150-frame playback. The second blast's fragment count is three, rather than the first blast's one; the comparison preserves the actual ongoing random/input history instead of assuming a reset rewinds the entire game. Both replay paths agree on the count, geometry and body state. PC captures were inspected: complete beam/post after reset, new shared damage and debris after recut. One body is still awake at the endpoint, so full settling is not claimed.

The original two-post reset/save/recut regression also passes. The PC replay extension changes only the harness; subsequent native testing exposed the stack issue below. Logs: `artifacts/connected-reset-replay.log` and `artifacts/connected-reset-post-regression.log`; reports and process-local recordings: `artifacts/connected-reset-checkpoint/`.

### Native stack failure and remaining debris divergence

Native reset run `render-20260917-180241` passes76 checks and writes the exact1192-byte reset checkpoint. Its loaded continuation restarts on the next rocket in both `180448` and exception-instrumented `180624`. The first fault is a write page fault at EIP0x4c8ea (`polygon_split_edges+0x5a`), ESP0xd0037e58, CR2=0xd0037e6c, followed by exception-delivery faults and a triple-fault reset. The clipping function uses3892 stack bytes; its subtraction caller uses4344. NXDK `bin/nxdk-link` defaults to `-stack:65536`; the failed XBE header at0x130 confirms65536bytes. NXDK cxbe copies PE stack reserve to XBE stack commit.

The project linker now reserves131072bytes for the main thread, adding64KiB to stock64MiB memory consumption. The rebuilt XBE header independently confirms131072bytes. This changes no geometry, replay input, fragment limit or comparison tolerance.

Run `artifacts/xemu/render-20260917-181215` completes all301frames and reaches diagnostic phase5 without restarting, but remains FAIL: its5096-byte checkpoint differs from PC in84bytes, all within the second of three fragment records. The first differing field is the local inertia tensor (absolute byte2744); world tensor, pose and motion also differ. Player state, destruction history and bytes outside that record match. The matching file sizes alone are not acceptance. Investigate mass/tensor arithmetic before claiming cross-platform continuation for this history.

Native framebuffer inspected: the beam/post joint has visible damage and wood debris is present beneath it; launcher, room and HUD render. Free pages3455 at the endpoint gives13.496MiB, not a guaranteed minimum for arbitrary histories. Harness restored the disc and closed its instance. Log: `artifacts/connected-reset-stack-native.log`. No GitHub image was added.

### Fragment center accumulation precision

Temporary per-call native instrumentation in `render-20260917-181909` proves matching input bounds, density and occupancy, with the first mismatch in the second fragment's center: Y=-1.874583333e-8 on Xbox versus -1.218479184e-8 on PC. This changes its origin and inertia before motion. The captured PC faces also produce matching PC/pre-inverse outputs through original4d1700 and compiled NXDK under Unicorn; that isolated emulation did not reproduce the live native drift and was insufficient to qualify it.

Original4d1a80/4d1a87 call40a070 twice, storing each weighted vector component as float;4d1a90 calls40a350, which explicitly adds and stores each center component as float (40a358/40a360/40a369). Merely enforcing weighted-product stores left the native84-byte checkpoint difference unchanged (`182122`). Recording each cell position, weight and accumulated center forced stores and made the instrumented run `182324` pass exactly. An uninstrumented attempt enforcing only weighted products and center accumulation still failed (`182525`). Cell coordinates must also retain their float stores; original4d1ab0/4d1ad4 explicitly store the stepped Y/X coordinates, and4d1a7c stores Z before vector multiplication. The implementation now makes cell mass, sampled positions, weighted intermediates and center accumulation volatile floats. All temporary capture buffers, file output and harness changes were removed before final acceptance testing.

Added three synthetic fractional-box cases to `probe_geomod_solid_mass.py` and its tracked binary-derived fixture:18 original mass/inertia cases now run, preserving the previous15 cases and adding32-cell occupancy at densities1,2.5,10. All123 PC tests pass; the connected reset/save/recut replay remains byte-identical to uninterrupted1150-frame control. Logs: `connected-mass-fixtures.log`, `connected-mass-final-tests.log`, `connected-mass-final-pc.log` under artifacts. Final uninstrumented native acceptance is separate below.

Final uninstrumented run `artifacts/xemu/render-20260917-182734` passes76/76 checks through301frames on stock64MiB. Its5096-byte Xbox RFCP exactly equals uninterrupted1150-frame PC control, including all three fragment states. Native framebuffer inspected: cut beam/post joint and wood rubble visible, room/weapon/HUD present.3455 available pages gives13.496MiB endpoint headroom. Disc restoration succeeded and the test process exited. The final source also passes all123 PC tests and the connected reset-save replay (`connected-mass-stores-tests.log`, `connected-mass-stores-pc.log`); native log is `connected-mass-stores-native.log`. The correction preserves original float-store boundaries without changing checkpoint comparison tolerances. This resolves the reproduced stack/precision failures, not arbitrary-history or full GeoMod acceptance.
