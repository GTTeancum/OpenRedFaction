#ifndef RF_SCENE_PREVIEW_H
#define RF_SCENE_PREVIEW_H
#include "rf/material.h"
#include "rf/preview.h"
typedef struct rf_scene_world_geometry {
    const rf_geometry *world;
    rf_geometry_movers movers;
    uint32_t *offsets,*slots;
    uint32_t geometry_count,material_count,allocated_bytes;
    float camera_position[3],camera_orientation[3][3];
} rf_scene_world_geometry;
/* Retain mover source geometry and local material mappings for reprojection.
 * Borrows world (must outlive this owner); copies only the preview camera.
 * Archives and the source level may close after success. Output mesh, material
 * images, and geometry owner have separate ownership and close functions.
 * All outputs must be empty; failure preserves them. Geometry payload cap is
 * 1 MiB, plus owner and temporary geometry pointer array; mapping allocations
 * are included in material_budget during load and allocated_bytes afterward. */
int rf_scene_world_open_retained(const rf_level *level,const rf_geometry *world,
    rf_vpp *maps,uint32_t map_count,rf_preview_mesh *mesh,rf_materials *materials,
    uint32_t mesh_budget,uint32_t material_budget,rf_scene_world_geometry *geometry);
void rf_scene_world_geometry_close(rf_scene_world_geometry *geometry);
/* No allocation or archive access. Poses must match retained mover order and
 * count; NULL with zero pose_count selects authored file poses. Keeps the saved
 * inspection camera. Same stable-input/capacity contract as preview updater. */
int rf_scene_world_update(const rf_scene_world_geometry *geometry,
    const rf_group_attached_pose *poses,uint32_t pose_count,
    rf_preview_mesh *mesh,uint32_t capacity_bytes);
/* Load authored mover meshes with the world and a deduplicated texture table.
 * Outputs must be empty. Geometry source budget is 1 MiB plus pointer array;
 * mesh/material budgets include their own temporary allocations. Source mover
 * geometry is released after projection; runtime motion is not yet connected. */
int rf_scene_world_open(const rf_level *level,const rf_geometry *world,
    rf_vpp *maps,uint32_t map_count,rf_preview_mesh *mesh,rf_materials *materials,
    uint32_t mesh_budget,uint32_t material_budget);
/* Diagnostic close inspection camera 2.2 units in front of an authored actor.
 * Changes only the supplied level's preview camera; not a gameplay camera. */
int rf_scene_preview_camera(rf_level *level,int32_t uid);
/* Inspection only: view a mover along its thinnest local axis, centered on
 * vertex bounds with world-up. Positive distance selects one side, negative
 * the other. Does not recover gameplay camera/collision placement. */
int rf_scene_preview_mover_camera(rf_level *level,int32_t uid,float distance);
/* Append one authored miner using scripted frame 0 to an existing world mesh.
 * Port-owned diagnostic composition, not a scene/gameplay loader. On success
 * mesh/materials own the combined arrays/images; on failure remain unchanged.
 * Budgets cap final mesh bytes and material residency; temporary old/new arrays
 * coexist during commit, plus a 1 MiB actor mesh and animation workspace.
 * Caller records the original mesh count as the actor draw-range boundary. */
int rf_scene_preview_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget);
/* Stream 64 scripted poses with fixed world prefix/materials and one combined
 * allocation (world bytes plus 1 MiB actor capacity, included in mesh_budget).
 * Sink borrows the current combined mesh synchronously; it must not mutate it.
 * Setup failure preserves inputs. Once streaming starts, inputs own combined
 * resources even on sink/producer failure; caller closes them on every exit.
 * Last produced pose remains in mesh. Textures are loaded only during setup. */
typedef int (*rf_scene_frame_sink)(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world_vertices);
int rf_scene_stream_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context);
/* Same ownership/budgets as stream_miner; loads the actor class's base state set
 * and runs the authored-state inspection schedule. Additional state-set storage
 * and a 512 KiB temporary registration budget are outside mesh/material caps. */
int rf_scene_stream_miner_states(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context);
#endif
