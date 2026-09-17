#ifndef RF_GEOMOD_PUBLICATION_DIGEST_H
#define RF_GEOMOD_PUBLICATION_DIGEST_H
#include "rf/geomod_authored_identity.h"
enum {RF_GEOMOD_DIGEST_AUTHORED_SOURCE=0,RF_GEOMOD_DIGEST_COMPILED_SOURCE=1};
enum {RF_GEOMOD_DIGEST_UNLIT=0,RF_GEOMOD_DIGEST_SOURCE_CHART=1,RF_GEOMOD_DIGEST_GENERATED_CHART=2,RF_GEOMOD_DIGEST_AUTHORED_CHART=3};
typedef struct rf_geomod_digest_material {
    uint32_t key; /* Lookup only; never serialized. */
    rf_geomod_identity_image image;
    uint32_t prehashed;unsigned char content_digest[32];
} rf_geomod_digest_material;
typedef struct rf_geomod_digest_chart {
    uint32_t key,kind,owner,source_face,retained_map;
    rf_lightmap_projection projection;
    rf_geomod_identity_image image;
    uint32_t prehashed;unsigned char content_digest[32];
} rf_geomod_digest_chart;
typedef struct rf_geomod_publication_digest_input {
    rf_geomod_mesh_view mesh;
    const rf_geomod_publication_origin *origins;
    const uint32_t *face_charts; /* One lookup key per published face. */
    const rf_geomod_digest_material *materials;uint32_t material_count;
    const rf_geomod_digest_chart *charts;uint32_t chart_count;
    uint32_t source_domain,publication_policy,material_policy;
} rf_geomod_publication_digest_input;
/* RFCM v1 logical image content fingerprint, usable for materials or charts.
 * Caller captures from original logical pixels before releasing them. */
int rf_geomod_image_content_digest(const rf_geomod_identity_image *,unsigned char out[32]);
/* RFAP v1 SHA256: published face order, authored(kind,owner,source token),
 * stable material/chart content and finite position/baseUV words. Changes in
 * lookup-table order, pointers, render slots, opaque reference IDs or mesh
 * generation do not change the digest. Face order/corner order DO matter.
 * source_domain validates core authored tokens vs scene compiled-reference
 * face.source_face; only origins supply the hashed stable authored token.
 * Policies1 only; configured publication faces/corners,128 materials; charts <= face limit.
 * Zero faces/corners/materials/charts encode an empty replacement publication
 * after reset; original source/collision must still be checked separately.
 * Source/unlit chart owner/token must match the face and retained_mapUINTMAX.
 * Generated faces REQUIRE completed generated charts, source_faceUINTMAX and
 * a non-UINTMAX retained noise-map ordinal. This ordinal is saved journal
 * identity, never a GPU mapping handle. Generated image/projection must use
 * canonical LOCAL tile coordinates and pixels (not atlas packing/whole atlas).
 * Generated tiles are4..64 per axis, source format5, packed1555 two-byte pixels.
 * AUTHORED_CHART requires a NEIGHBOR with absent compiled reference, a valid
 * authored source token, and a completed generated tile/retained-map ordinal.
 * In compiled source domain its face.source_face is UINTMAX; the origin keeps
 * its authored token. It never admits unlit or pending hidden surfaces.
 * A generated(owner,retained_map) pair has one chart row shared by its faces;
 * duplicate semantic rows reject even if their lookup keys differ.
 * prehashed0 hashes raw image pixels; prehashed1 requires pixelsNULL and a
 * trusted capture-produced RFCM digest plus original descriptor. Both paths
 * hash the same32 bytes. Never obtain trusted resource rows from a save blob.
 * Explicit unlit rows require zero projection/image; lit rows use the logical
 * canonical image rules from geomod_authored_identity.h. No pending fallback.
 * Pure/no allocation. Invalid input preserves output. Disjoint inputs/output.
 * Not a topology validator or a composed-collision/material-tail digest. */
int rf_geomod_publication_digest(const rf_geomod_publication_digest_input *,unsigned char out[32]);
#endif
