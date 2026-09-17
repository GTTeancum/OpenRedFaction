#ifndef RF_AUTHORED_OWNER_EXTENSION_H
#define RF_AUTHORED_OWNER_EXTENSION_H
#include "rf/vpp.h"
enum {
    RF_AUTHORED_OWNER_EXTENSION_BYTES=128,
    RF_AUTHORED_OWNER_PUBLICATION_POLICY=1,
    RF_AUTHORED_OWNER_COLLISION_POLICY=1,
    RF_AUTHORED_OWNER_MATERIAL_POLICY=1
};
/* RFDS2 bytes288..415: eight little-endian u32s followed by three SHA256
 * values. This is a port checkpoint extension, not an original save ABI. */
typedef struct rf_authored_owner_extension {
    uint32_t uid,mode,source_count,neighbor_count;
    uint32_t publication_policy,collision_policy,material_policy,serial;
    unsigned char publication_digest[32],collision_digest[32],material_digest[32];
} rf_authored_owner_extension;
/* Supplied from freshly validated immutable ownership and private candidate
 * reconstruction. Never copy these expectations from the untrusted payload. */
typedef struct rf_authored_owner_expected {
    uint32_t uid,source_count,neighbor_count;
    unsigned char publication_digest[32],collision_digest[32],material_digest[32];
    uint32_t material_policy; /*0 defaults to legacy1;2 explicitly permits authored cap maps.*/
} rf_authored_owner_expected;
/* Exact128-byte span required even for encode (not a minimum capacity).
 * Known outward mode0, publication/collision policy1, material policy1 or2
 * matching the independently reconstructed expectation. Source count4..32,
 * neighbors1..32; UID cannot beUINT32_MAX. cuts0..8 and cuts<=serial; serial
 * UINT32_MAX rejects. Zero-cut serial>0 is valid after reset.
 * Pure/no allocation. All failures preserve caller output byte-for-byte.
 * Inputs/output must be disjoint. No RFDS header, size arithmetic, checksum,
 * source digest, material-slot resolution, or candidate publication is done.
 * Caller must separately validate RFDS2/profile2 and reconstruct all expected
 * digests/ownership before this semantic gate; digest equality is not auth. */
int rf_authored_owner_extension_encode(const rf_authored_owner_extension *,
    const rf_authored_owner_expected *,uint32_t cuts,void *out,uint32_t bytes);
int rf_authored_owner_extension_decode(const void *,uint32_t bytes,
    const rf_authored_owner_expected *,uint32_t cuts,rf_authored_owner_extension *out);
/* RFDS3 collection extension: mode1, same128-byte layout; count2..4 comes
 * from the independently verified source directory. Counts aggregate owners.
 * Each local cut must be checked separately; this gate only bounds their sum.
 * Legacy mode0 routines reject collection mode. Same atomic-output contract. */
int rf_authored_owner_collection_encode(const rf_authored_owner_extension *,
    const rf_authored_owner_expected *,uint32_t count,uint32_t cuts,void *,uint32_t bytes);
int rf_authored_owner_collection_decode(const void *,uint32_t bytes,
    const rf_authored_owner_expected *,uint32_t count,uint32_t cuts,rf_authored_owner_extension *);
#endif
