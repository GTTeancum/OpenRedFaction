#ifndef RF_GEOMOD_LIMITS_H
#define RF_GEOMOD_LIMITS_H
/* Port working-set limit, separate from the original128 admission journal.
 * Raising this also requires geometry/publication and stock64MiB acceptance. */
#ifndef RF_GEOMOD_CUT_LIMIT
#define RF_GEOMOD_CUT_LIMIT 8
#endif
#if RF_GEOMOD_CUT_LIMIT < 1 || RF_GEOMOD_CUT_LIMIT > 32
#error GeoMod cutter mask requires a limit from1 through32
#endif
/* Publication and checkpoint readers must share the configured face bound. */
#ifndef RF_GEOMOD_PUBLICATION_FACES
#define RF_GEOMOD_PUBLICATION_FACES 768
#endif
#ifndef RF_GEOMOD_PUBLICATION_VERTICES
#define RF_GEOMOD_PUBLICATION_VERTICES 4096
#endif
#ifndef RF_GEOMOD_LIGHTMAP_LIMIT
#define RF_GEOMOD_LIGHTMAP_LIMIT 1024
#endif
#define RF_GEOMOD_HISTORY_HEADER_BYTES 28u
#define RF_GEOMOD_STAR_FACE_LIMIT 64u
#define RF_GEOMOD_STAR_VERTEX_LIMIT (RF_GEOMOD_STAR_FACE_LIMIT * 3u)
/* Per cutter: metadata plus bounded STAR vertices and faces. */
#define RF_GEOMOD_HISTORY_CUT_MAX_BYTES \
    (24u + RF_GEOMOD_STAR_VERTEX_LIMIT * 20u + RF_GEOMOD_STAR_FACE_LIMIT * 16u)
#define RF_GEOMOD_HISTORY_MAX_BYTES \
    (RF_GEOMOD_HISTORY_HEADER_BYTES + RF_GEOMOD_CUT_LIMIT * RF_GEOMOD_HISTORY_CUT_MAX_BYTES)
#endif
