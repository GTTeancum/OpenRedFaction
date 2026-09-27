#ifndef RF_EDITOR_BRUSH_H
#define RF_EDITOR_BRUSH_H
#include "rf/geometry.h"
#include "rf/collision.h"

typedef struct rf_editor_brush {
    void *storage;
    rf_collision_face *faces;
    uint32_t *source_words;
    uint32_t uid,operation,face_count,corner_count,resident_bytes;
} rf_editor_brush;

/* Read one uniquely identified v180 editor brush from section 0x2000000.
 * Faces/points are owned and in world coordinates. This does not establish
 * Boolean order, manifold closure or permission to publish a room cut.
 * Pass an empty output. Both functions preserve output on failure; close
 * releases a successful one.
 * Decoder searches bounded candidate records, so unknown sidecars on other
 * brushes do not need to be interpreted. */
int rf_editor_brush_decode(const void *section,uint32_t bytes,uint32_t uid,
    uint32_t budget,rf_editor_brush *out);
int rf_editor_brush_open(const rf_level *level,uint32_t uid,uint32_t budget,
    rf_editor_brush *out);
/* Compiled file-order polygons owned by this brush in one room. Count-only
 * query accepts NULL ids/zero capacity; source words may own multiple output
 * polygons. Does not classify Boolean survival or grant cut permission. */
int rf_editor_brush_compiled_faces(const rf_editor_brush *brush,
    const rf_geometry *geometry,uint32_t room,uint32_t *ids,
    uint32_t capacity,uint32_t *count);
void rf_editor_brush_close(rf_editor_brush *brush);
#endif
