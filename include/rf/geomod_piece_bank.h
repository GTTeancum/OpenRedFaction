#ifndef RF_GEOMOD_PIECE_BANK_H
#define RF_GEOMOD_PIECE_BANK_H
#include "rf/geomod.h"
typedef struct rf_geomod_piece_bank rf_geomod_piece_bank;
typedef struct rf_geomod_owned_piece {
    rf_geomod_mesh_view mesh;
    rf_geomod_piece_placement placement;
    const uint32_t *old_faces;
    const rf_collision_face_filter *filters;
    uint32_t id;
} rf_geomod_owned_piece;
/* One fixed allocation containing local mesh corners, faces, owner mapping,
 * filters and placement descriptors. Byte budget includes the owner itself;
 * allocator overhead is external. No allocation during append. */
int rf_geomod_piece_bank_open(uint32_t vertices,uint32_t faces,uint32_t pieces,
    uint32_t budget,rf_geomod_piece_bank **);
void rf_geomod_piece_bank_close(rf_geomod_piece_bank **);
/* Copy and recenter extracted geometry before its borrowed storage expires.
 * old_faces maps each mesh face into source_filters[source_count]. IDs must be
 * unique within the bank. Errors preserve all published entries/counts; unused
 * staging bytes may change. No atlas, physics, notification or scene publication. */
int rf_geomod_piece_bank_append(rf_geomod_piece_bank *,const rf_geomod_mesh_view *,
    const uint32_t *old_faces,const rf_collision_face_filter *source_filters,
    uint32_t source_count,uint32_t id);
int rf_geomod_piece_bank_get(const rf_geomod_piece_bank *,uint32_t index,rf_geomod_owned_piece *);
uint32_t rf_geomod_piece_bank_count(const rf_geomod_piece_bank *);
uint32_t rf_geomod_piece_bank_bytes(const rf_geomod_piece_bank *);
#endif
