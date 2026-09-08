#ifndef RF_MODEL_FILE_H
#define RF_MODEL_FILE_H
#include "rf/vpp.h"
#define RF_MODEL_MAX_SECTIONS 128
typedef struct rf_model_section { uint32_t type, offset, size; } rf_model_section;
typedef struct rf_model_file {
    rf_vpp *archive;
    rf_vpp_entry entry;
    uint32_t section_count, submeshes;
    rf_model_section sections[RF_MODEL_MAX_SECTIONS];
} rf_model_file;
/* Structural V3C v0x40000 traversal; LOD payloads stay on disc. Caller retains
 * archive ownership. Result is cleared on failure. No mesh/animation decoding. */
int rf_model_file_open(rf_model_file *model, rf_vpp *archive, const char *name);
#endif
