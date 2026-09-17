#ifndef RF_AUTHORED_IDENTITY_CAPTURE_H
#define RF_AUTHORED_IDENTITY_CAPTURE_H
#include "rf/geomod_authored_identity.h"
#include "rf/geomod_publication_digest.h"
typedef struct rf_geomod_authored_chart_identity {
    uint32_t reference,compiled_material;
    rf_collision_face_filter filter;
    rf_geomod_digest_chart chart;
} rf_geomod_authored_chart_identity;
typedef struct rf_geomod_authored_identity_manifest {
    rf_geomod_digest_material *materials;uint32_t material_capacity,material_count;
    rf_geomod_authored_chart_identity *references;uint32_t reference_capacity,reference_count;
    rf_geomod_digest_material *substrate; /* Optional separate settings.texture output; key0. */
    uint32_t resident_bytes; /* Full supplied capacities + this descriptor. */
} rf_geomod_authored_identity_manifest;
/* Capture a bounded ctf06/UID93,94,96,97 source identity before original resources
 * close. Inputs remain borrowed; maps is an already-open contiguous array1..32
 * and rgb holds immutable ORIGINAL source RGB, never a dynamically lit atlas.
 * Asset materials must be the loader's unremapped compiled texture IDs.
 * Unique exact texture ownership is required across supplied maps; no archive
 * precedence guesses, missing pattern, reduction or mapping-image0 fallback.
 * Returns digest matching the installed capture probe's logical pixel/chart
 * convention. No console, scene mutation, archive reopen or retained owner.
 * Both outputs unchanged on any failure. All allocated scratch is released.
 * peak_bytes accounts context/arrays/editor/material/chart copies, temporary
 * decoder image rounded to4KiB, and8KiB conservative stack reserve including
 * the4096-byte image reader/SHA work. Excludes borrowed input owners and heap
 * allocator bookkeeping. Budget2MiB is the intended current profile cap.
 * Output/input buffers must be disjoint; caller retains valid immutable input
 * lifetimes and exclusive archive cursor access for the duration of capture. */
int rf_geomod_authored_identity_capture(const rf_level *,const rf_geometry *,
    const rf_geomod_authored_post_view *,rf_vpp *maps,uint32_t map_count,
    const rf_lightmap_rgb_owner *,uint32_t budget,unsigned char digest[32],uint32_t *peak_bytes);
/* Optional small trusted manifest, captured while original pixel owners exist.
 * Caller supplies disjoint arrays/capacities (materials1..128,refs1..768).
 * On success material keys are compiled IDs; reference/chart keys are compiled
 * input references. Image descriptors have pixelsNULL and prehashed RFCM32;
 * source charts preserve immutable projection/owner/token and original filter.
 * Explicit unlit charts remain zero image/prehashed0. No pixel borrow escapes.
 * Non-NULL substrate requests an independent original settings.texture decode
 * and trusted RFCM row, without adding a fake compiled material ID or changing
 * aggregate source identity. Its key0 is only a caller-remappable lookup key.
 * Entire arrays/counts/resident_bytes/digest/peak unchanged on any failure.
 * Supplied array capacities and private staging are charged to budget/peak;
 * caller must retain resident_bytes after transient capture memory is freed.
 * NULL manifest is exactly the backward-compatible capture behavior. */
int rf_geomod_authored_identity_capture_manifest(const rf_level *,const rf_geometry *,
    const rf_geomod_authored_post_view *,rf_vpp *maps,uint32_t map_count,
    const rf_lightmap_rgb_owner *,uint32_t budget,unsigned char digest[32],uint32_t *peak_bytes,
    rf_geomod_authored_identity_manifest *manifest);
#endif
