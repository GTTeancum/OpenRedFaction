#ifndef RF_MOTION_FILE_H
#define RF_MOTION_FILE_H
#include "rf/motion.h"
typedef struct rf_motion_file {
    rf_vpp *archive;
    rf_vpp_entry entry;
    uint32_t header[20]; /* Preserve unclassified fields and trailing regions. */
} rf_motion_file;
typedef struct rf_motion_track {
    uint32_t offset, size, rotation_count, position_count;
    rf_motion_weight_envelope envelope;
} rf_motion_track;
/* Bounded reconstruction of loader 0x53a9d6..0x53aa54: replace from the first
 * dot anywhere in the name, or append .rfa when absent. Case is preserved.
 * Input/output limit 63 bytes plus NUL; supports in-place use. Failure leaves
 * output unchanged. Empty input produces .rfa; callers must handle absent names. */
int rf_motion_compiled_filename(const char *authored,char compiled[64]);
/* No allocations. Archive must remain open. Failure clears the file handle. */
int rf_motion_file_open(rf_motion_file *file, rf_vpp *archive, const char *name);
/* Indexed access rereads archive metadata; outputs stay unchanged on failure. */
int rf_motion_file_track(const rf_motion_file *file, uint32_t index, rf_motion_track *out);
int rf_motion_file_rotation(const rf_motion_file *file, uint32_t track, uint32_t key, rf_motion_rotation_key *out);
int rf_motion_file_position(const rf_motion_file *file, uint32_t track, uint32_t key, rf_motion_position_key *out);
typedef struct rf_motion_sample {
    float rotation[4], position[3], weight;
} rf_motion_sample;
/* Binary-search validated ticks and read at most two keys of each kind.
 * No allocation; output stays unchanged on error. The archive is immutable
 * while this handle is open. Disk seeks still require a playback cache. */
int rf_motion_file_sample(const rf_motion_file *file, uint32_t track, int32_t tick, int bypass_fades, rf_motion_sample *out);
#endif
