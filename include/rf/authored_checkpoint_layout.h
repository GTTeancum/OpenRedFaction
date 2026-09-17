#ifndef RF_AUTHORED_CHECKPOINT_LAYOUT_H
#define RF_AUTHORED_CHECKPOINT_LAYOUT_H
#include "rf/composed_checkpoint.h"
enum { RF_AUTHORED_CHECKPOINT_HEADER=416, RF_AUTHORED_CHECKPOINT_MAP_BYTES=88,
    RF_AUTHORED_CHECKPOINT_ADMISSION_BYTES=48 };
typedef struct rf_authored_checkpoint_layout {
    uint32_t bytes,core_offset,core_bytes,admission_offset,admissions;
    uint32_t map_offset,maps,face_offset,faces,piece_offset,piece_bytes;
} rf_authored_checkpoint_layout;
/* RFDS2 inner payload only. Computes exact nonoverlapping spans, bounded by
 * the existing composed-save transport capacity. No allocation or mutation.
 * Core decoder, admission semantics, map contents, reconstructed digests and
 * player placement remain REQUIRED separate gates. All failures preserve out. */
int rf_authored_checkpoint_layout_size(uint32_t core_bytes,uint32_t admissions,
    uint32_t maps,uint32_t faces,rf_authored_checkpoint_layout *out);
/* Optional RFPB1 trailer length lives in RFDS2 header word12 (zero in old saves). */
int rf_authored_checkpoint_layout_size_pieces(uint32_t core_bytes,uint32_t admissions,
    uint32_t maps,uint32_t faces,uint32_t piece_bytes,rf_authored_checkpoint_layout *out);
/* Checks header/version/trailer length, NUL-padded nonempty level name, exact
 * length and all span bounds before exposing offsets. Does not trust or decode
 * owner extension/RGCH; payload need only stay alive for the caller's checks.
 * No checksum: RFSG transport supplies it. Inputs/output must be disjoint. */
int rf_authored_checkpoint_layout_read(const void *data,uint32_t bytes,
    rf_authored_checkpoint_layout *out);
#endif
