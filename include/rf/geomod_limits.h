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
#define RF_GEOMOD_HISTORY_HEADER_BYTES 28u
/* Per cutter:24 metadata +60 vertices*20 bytes +20 faces*16 bytes. */
#define RF_GEOMOD_HISTORY_CUT_MAX_BYTES 1544u
#define RF_GEOMOD_HISTORY_MAX_BYTES \
    (RF_GEOMOD_HISTORY_HEADER_BYTES + RF_GEOMOD_CUT_LIMIT * RF_GEOMOD_HISTORY_CUT_MAX_BYTES)
#endif
