#ifndef RF_GEOMOD_RETAINED_MATERIAL_DIGEST_H
#define RF_GEOMOD_RETAINED_MATERIAL_DIGEST_H
#include "rf/geomod_publication.h"
#include "rf/lightmap.h"
/* Semantic copy of the existing RFDS88-byte map record; never memcpy its
 * C layout onto disk. material_token0 means verified generated substrate. */
typedef struct rf_geomod_retained_material_map {
    float plane[4],minimum[3],maximum[3];
    uint32_t material_token,x,y,width,height,base_seed;
    rf_lightmap_projection projection;
} rf_geomod_retained_material_map;
typedef struct rf_geomod_retained_material_input {
    const unsigned char *source_identity,*substrate_identity; /*32 bytes each*/
    const rf_geomod_retained_material_map *maps;uint32_t map_count;
    const rf_geomod_publication_origin *origins;
    const uint16_t *face_maps;uint32_t face_count;
    uint32_t owner,serial,cuts,owner_generation,owner_cuts;
    uint32_t baked,sample,random,x,y,row,material_policy;
    const uint32_t *authored_material_tokens;uint32_t authored_material_count;
} rf_geomod_retained_material_input;
/* RFRM v1 SHA256 over the complete settled noise journal, not current atlas
 * appearance. Includes historical unreferenced maps in retained order, every
 * map's plane/bounds/packing/projection/seed, regenerated local base1555 pixels,
 * final RNG/cursor, owner continuation fields and face->retained-map ordinals.
 * Replays existing rf_geomod_light_noise + rf_lightmap_pack_1555(undoubled) in
 * local64-pixel chunks; no live atlas, GPU image handles, dynamic lights/cache,
 * or dynamic map.hash/FNV is an input. Source/substrate identities must already
 * be verified. This does not validate visible face geometry against its map.
 * Policy1,512x512 row packing, density4, <=1024 maps, configured publication faces and cutter limit.
 * Requires baked==map_count/sample0, cuts<=serial, serial!=UINTMAX and owner
 * continuation consistency. Empty zero-cut generation0 permits lazy RNG0 or1;
 * an initialized nonzero generation requires RNG1 even when empty.
 * nonempty chain starts1 and verifies every base_seed and final random value.
 * Every generated face requires map<map_count; source faces require65535.
 * Policy2 uses RFRM v3 (single)/v4 (collection) and additionally admits hidden
 * NEIGHBOR faces with completed maps. Token0 remains crater substrate; nonzero
 * tokens are compiled texture index+1, qualified by verified immutable source
 * identities and a caller-captured allowlist (<=128, unique, nonzero/nonMAX).
 * Never read that allowlist from the save. Hidden faces require an authored
 * token and absent compiled reference; crater maps must still use token0.
 * Historical unused authored maps remain part of the journal. Policy1 encoding
 * and hashes stay unchanged and reject a supplied authored allowlist.
 * All failures preserve output. No allocation/input mutation. Disjoint output.
 * Call after private history/lightmap reconstruction and before publication. */
int rf_geomod_retained_material_digest(const rf_geomod_retained_material_input *,unsigned char out[32]);
/* RFRM v2 binds one shared journal to an ordered collection of1..4 verified
 * immutable source identities and local cut counts. Aggregate cuts may exceed
 * room serial after simultaneous edits; each local count must not. owner must
 * equal the first UID. source_identity in the shared input is unused here.
 * Crater owners must belong to a source with cuts. RFRM v1 stays unchanged. */
typedef struct rf_geomod_retained_material_source {
    uint32_t uid,cuts;const unsigned char *identity;
} rf_geomod_retained_material_source;
int rf_geomod_retained_material_collection_digest(const rf_geomod_retained_material_input *,
    const rf_geomod_retained_material_source *,uint32_t count,unsigned char out[32]);
#endif
