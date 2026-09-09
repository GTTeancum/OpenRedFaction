#ifndef RF_MODEL_FILE_H
#define RF_MODEL_FILE_H
#include "rf/vpp.h"
#define RF_MODEL_MAX_SECTIONS 128
#define RF_MODEL_MAX_LODS 128
typedef struct rf_model_lod {
    uint32_t offset, size, attachment_offset, attachment_count;
    uint32_t batch_offset, batch_count, flags, auxiliary;
    uint32_t texture_offset,texture_count,section_index;
    float threshold;
} rf_model_lod;
/* File-relative regions in original 0x569920 order: positions, normals, UV,
 * indices, planes, extra, bone links, auxiliary. Encoding remains separate. */
typedef struct rf_model_batch {
    uint32_t vertices, triangles, format_bits;
    uint32_t offsets[8], sizes[8];
} rf_model_batch;
typedef struct rf_model_vertex {
    float position[3],normal[3],uv[2];
    uint8_t weights[4],bones[4];
} rf_model_vertex;
typedef struct rf_model_triangle { uint16_t indices[3],flags; } rf_model_triangle;
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
/* Bounded batch directory; no LOD blob allocation. Output unchanged on error. */
int rf_model_file_batch(const rf_model_file *model,uint32_t lod,uint32_t index,rf_model_batch *batch);
/* Installed 0x518c41 layout only. Preserves bone bytes/triangle flags without
 * assuming their runtime interpretation. Source non-finite normal bits are
 * preserved; positions/UV must be finite. No allocation; output unchanged on error. */
int rf_model_file_vertex(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,rf_model_vertex *vertex);
int rf_model_file_triangle(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,rf_model_triangle *triangle);
/* Signed backward distance from the extra-data stream. Nonpositive means
 * deform afresh; positive must refer to an earlier vertex in this batch.
 * Preserves the original signed value; output unchanged on failure. */
int rf_model_file_vertex_reuse(const rf_model_file *model,const rf_model_batch *batch,uint32_t index,int32_t *distance);
/* Resolve batch texture slot through LOD table to the flattened material
 * index used by rf_model_materials. Negative original slots return NOT_FOUND. */
int rf_model_file_batch_material(const rf_model_file *model,uint32_t lod,uint32_t batch,uint32_t *material);
typedef struct rf_model_draw_batch {
    uint32_t first_vertex,vertices,first_triangle,triangles,material;
} rf_model_draw_batch;
typedef struct rf_model_geometry {
    rf_model_draw_batch *batches;rf_model_vertex *vertices;
    rf_model_triangle *triangles;int32_t *reuse;
    uint32_t batch_count,vertex_count,triangle_count,accounted_bytes;
} rf_model_geometry;
/* Port-owned geometry for one LOD. Triangle indices/reuse remain batch-local;
 * material is flattened, or UINT32_MAX for an original negative draw slot.
 * Budget includes this struct and arrays, not allocator metadata or caller
 * archive/model/texture state. Zero-initialize; close before reuse. */
int rf_model_geometry_open(rf_model_geometry *geometry,const rf_model_file *model,uint32_t lod,uint32_t budget);
void rf_model_geometry_close(rf_model_geometry *geometry);
/* Select a flattened LOD index within one SUBM using its serialized thresholds.
 * Metric and render gates have the same meaning as rf_model_select_lod.
 * No allocation; output unchanged on failure. */
int rf_model_file_select_lod(const rf_model_file *model,uint32_t submesh,uint32_t flags,
    int alternate,int32_t minimum,int scaled,int animated,double metric,uint32_t *out);
#endif
