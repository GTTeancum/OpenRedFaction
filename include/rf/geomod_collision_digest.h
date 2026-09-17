#ifndef RF_GEOMOD_COLLISION_DIGEST_H
#define RF_GEOMOD_COLLISION_DIGEST_H
#include "rf/collision_composition.h"
#include "rf/geomod_publication_digest.h"
enum {RF_GEOMOD_COLLISION_AUTHORED=0,RF_GEOMOD_COLLISION_COMPILED=1,RF_GEOMOD_COLLISION_PUBLISHED=2};
typedef struct rf_geomod_collision_digest_row {
    uint32_t canonical_order,domain,fragment,material;
    rf_geomod_publication_origin origin;
    uint32_t metadata_id; /* Only for published hidden NEIGHBOR with referenceUINTMAX. */
} rf_geomod_collision_digest_row;
typedef struct rf_geomod_collision_digest_input {
    const rf_collision_composition_view *composition;
    const rf_geomod_collision_digest_row *rows;uint32_t row_count;
    const rf_geomod_digest_material *materials;uint32_t material_count;
    const unsigned char *source_identity; /* Exactly32 bytes, already verified. */
    uint32_t room,collision_policy;
} rf_geomod_collision_digest_input;
/* RFAC v1 SHA256 over the FULL room. rows are indexed by composition source
 * order, not tree order; origin.reference MUST join composition.face_ids.
 * Hidden published NEIGHBOR rows retain referenceUINTMAX and a valid authored
 * token; metadata_id instead joins the non-UINTMAX runtime collision ID.
 * Hidden IDs cannot alias other surfaces; split fragments of the same authored
 * owner/token/material may share one. These lookup IDs are never hashed.
 * tree.source_indices and row.canonical_order must both be permutations0..N-1.
 * Canonical order = unchanged compiled input faces in ascending reference
 * order, then published faces in publication order (fragment0..published-1).
 * O(N^2) bounded validation/traversal, no allocation/tree/scratch mutation.
 * Nodes, BVH order, pointers, generation, references and material keys are not
 * hashed. Stable source_identity + room qualify provenance. COMPILED domain
 * explicitly represents original source geometry lacking authored ownership:
 * origin.owner=room, source_face=original compiled input ID=reference,
 * kindRETAINED, fragment0. Never use this to hide missing published ownership.
 * AUTHORED original rows carry true owner/token and a stable per-token fragment
 * ordinal; PUBLISHED rows preserve each publication fragment separately.
 * Duplicate(domain,kind,owner,token,fragment) is ambiguous and rejects.
 * COMPILED rows' material is the original compiled texture index, qualified
 * by source_identity; it hashes tagged RFCI+index without a pixel lookup.
 * It may describe animated liquid without pretending it has static pixels.
 * AUTHORED/PUBLISHED materials resolve existing logical image content; fixed material-domain
 * digest RFCM v1 is hashed per face. All filter/plane/bounds/corner words and
 * triangle_surface are explicit scalars. No struct padding is read as data.
 * Exact full-room coverage required,1..8192 faces,3..256 corners each,
 *0..128 material rows (zero only when no row needs RFCM), collision policy1. This checks canonical encoding, not
 * geometric validity or save identity authenticity. All failures preserve
 * output; input/output disjoint. Retained data must remain immutable. */
int rf_geomod_collision_digest(const rf_geomod_collision_digest_input *,unsigned char out[32]);
#endif
