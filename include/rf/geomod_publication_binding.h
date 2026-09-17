#ifndef RF_GEOMOD_PUBLICATION_BINDING_H
#define RF_GEOMOD_PUBLICATION_BINDING_H
#include "rf/geomod_publication.h"
#include "rf/lightmap.h"
/* Copy these values while source geometry is alive. No borrowed pointers.
 * reference keys the publication origin.reference, not its source_face.
 * material is the caller-resolved render slot. Mapping/image name resources
 * retained separately by the level owner; this helper does not own pixels.
 * Both mapping/image UINT32_MAX explicitly declare an unlightmapped source. */
typedef struct rf_geomod_publication_binding_reference {
    uint32_t reference,source_face,owner,material,mapping,image;
    rf_lightmap_projection projection;
} rf_geomod_publication_binding_reference;
enum { RF_GEOMOD_BINDING_SOURCE=0,RF_GEOMOD_BINDING_UNLIT=1,RF_GEOMOD_BINDING_GENERATED_PENDING=2 };
enum { RF_GEOMOD_BINDING_REJECT_GENERATED=0,RF_GEOMOD_BINDING_DEFER_GENERATED=1,RF_GEOMOD_BINDING_DEFER_AUTHORED=2 };
typedef struct rf_geomod_publication_bound_corner {
    float uv[2],lightmap_uv[2];
    uint32_t material,mapping,image,status;
    rf_geomod_publication_origin origin;
} rf_geomod_publication_bound_corner;
/* No allocation: output capacity and byte budget both required. Source mesh
 * faces must partition the corner array contiguously; sharing a corner index
 * between source faces would lose per-face UV/chart ownership and is rejected.
 * Complete validation precedes output writes. Inputs/output/out_pending must
 * not alias. Missing refs or owner/source mismatch reject atomically.
 * GENERATED_PENDING preserves baseUV/material/provenance but deliberately has
 * sentinel map/image and zero LMUV. Caller MUST finish its independent crater
 * mapping before publishing to a renderer; it is never an unlit fallback.
 * DEFER_AUTHORED also admits NEIGHBOR surfaces with sentinel reference and
 * valid authored owner/source/material. These retain authored UVs and require
 * a new lighting chart, not crater texture mapping. Other missing refs reject.
 * Projected sourceUV follows original4e49d0 using the copied mapping, preserving
 * explicit binary32 stores. This is not a four-channel CSG interpolation API.
 */
int rf_geomod_publication_bind(const rf_geomod_mesh_view *,
    const rf_geomod_publication_origin *,const rf_geomod_publication_binding_reference *,uint32_t,
    uint32_t generated_policy,rf_geomod_publication_bound_corner *,uint32_t capacity,
    uint32_t byte_budget,uint32_t *out_pending);
#endif
