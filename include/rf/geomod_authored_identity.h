#ifndef RF_GEOMOD_AUTHORED_IDENTITY_H
#define RF_GEOMOD_AUTHORED_IDENTITY_H
#include "rf/geomod_authored_post.h"
#include "rf/lightmap.h"
/* Logical, tightly packed row-major pixels, never GPU allocation/pitch bytes.
 * format is stable source-format policy; bytes_per_pixel is exactly 2 or 4.
 * Name is a nonempty lowercase asset member name with forward slashes. */
typedef struct rf_geomod_identity_image {
    char name[64];
    uint32_t width,height,format,bytes_per_pixel,bytes;
    const void *pixels;
} rf_geomod_identity_image;
typedef struct rf_geomod_identity_material {
    uint32_t compiled_material;
    rf_geomod_identity_image image;
} rf_geomod_identity_material;
/* reference is only a lookup key. owner/source_face are stable authored IDs.
 * Explicit unlit=1 requires a zero chart/projection; lit rows carry canonical
 * projection and logical chart image content (not atlas/image handles).
 * Chart names are caller-owned stable source asset names, not runtime IDs. */
typedef struct rf_geomod_identity_reference {
    uint32_t reference,owner,source_face,compiled_material,unlit;
    rf_collision_face_filter filter;
    rf_lightmap_projection projection;
    rf_geomod_identity_image chart;
} rf_geomod_identity_reference;
enum { RF_GEOMOD_IDENTITY_COMPILED_MATERIALS=1 };
typedef struct rf_geomod_authored_identity_input {
    const rf_geomod_authored_post_view *asset;
    char level[64];
    const void *compiled_section,*editor_section;
    uint32_t compiled_bytes,editor_bytes;
    uint32_t source_operation,source_mode;
    uint32_t loader_policy,publication_policy,collision_policy,material_policy;
    uint32_t material_domain;
    const rf_geomod_identity_material *materials;uint32_t material_count;
    const rf_geomod_identity_reference *references;uint32_t reference_count;
} rf_geomod_authored_identity_input;
/* Source identity domain RFAS v1, SHA256, canonical LE scalar/float words.
 * No allocations or input mutation; output preserved on all failures.
 * All pointers/renderer slots/mesh generations/BVH ordering/opaque reference
 * numbers are excluded. Only immutable loader views with COMPILED materials
 * are accepted; caller must capture reference/chart/material rows before
 * remapping the source to render slots. Numeric keys cannot prove provenance.
 * No alias of output and input storage. Max768 faces/4096 corners per mesh,
 * 32 neighbors,128 material rows,768 reference rows,64MiB per content span.
 * Missing or duplicate lookup keys and mismatched source ownership reject.
 * Hidden source/neighbor faces may lack compiled references; windows may not.
 * Does not validate topology, implement RFDS, or replace separate template,
 * generated-substrate and admission-region identity domains. */
int rf_geomod_authored_identity(const rf_geomod_authored_identity_input *,unsigned char out[32]);
#endif
