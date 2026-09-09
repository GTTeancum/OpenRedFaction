#ifndef RF_SCENE_PREVIEW_H
#define RF_SCENE_PREVIEW_H
#include "rf/material.h"
#include "rf/preview.h"
/* Diagnostic close inspection camera 2.2 units in front of an authored actor.
 * Changes only the supplied level's preview camera; not a gameplay camera. */
int rf_scene_preview_camera(rf_level *level,int32_t uid);
/* Append one authored miner using scripted frame 0 to an existing world mesh.
 * Port-owned diagnostic composition, not a scene/gameplay loader. On success
 * mesh/materials own the combined arrays/images; on failure remain unchanged.
 * Budgets cap final mesh bytes and material residency; temporary old/new arrays
 * coexist during commit, plus a 1 MiB actor mesh and animation workspace.
 * Caller records the original mesh count as the actor draw-range boundary. */
int rf_scene_preview_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget);
#endif
