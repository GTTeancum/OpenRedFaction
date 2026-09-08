#ifndef RF_MODEL_FILE_H
#define RF_MODEL_FILE_H
#include "rf/vpp.h"
#define RF_MODEL_MAX_SECTIONS 128
#define RF_MODEL_MAX_LODS 128
typedef struct rf_model_lod { uint32_t offset, size, attachment_offset, attachment_count; } rf_model_lod;
typedef struct rf_model_attachment {
    char name[69];
    float rotation[4], position[3];
    int32_t parent;
} rf_model_attachment;
typedef struct rf_model_section { uint32_t type, offset, size, material_offset, material_count; } rf_model_section;
typedef struct rf_model_file {
    rf_vpp *archive;
    rf_vpp_entry entry;
    uint32_t section_count, submeshes;
    rf_model_section sections[RF_MODEL_MAX_SECTIONS];
    uint32_t lod_count;
    rf_model_lod lods[RF_MODEL_MAX_LODS];
} rf_model_file;
/* Structural V3C v0x40000 traversal; LOD payloads stay on disc. Caller retains
 * archive ownership. Result is cleared on failure. No mesh/animation decoding. */
int rf_model_file_open(rf_model_file *model, rf_vpp *archive, const char *name);
/* Read one raw 100-byte attachment without loading its LOD blob. Fields remain
 * local to the referenced bone; parent validation needs the loaded skeleton. */
int rf_model_file_attachment(const rf_model_file *model, uint32_t lod, uint32_t index, rf_model_attachment *attachment);
/* Stream one 84-byte serialized SUBM material. This is not the 200-byte
 * runtime material layout; conversion is separate. Output unchanged on error. */
int rf_model_file_material(const rf_model_file *model,uint32_t submesh,uint32_t index,uint8_t raw[84]);
#endif
