#ifndef RF_CUTSCENE_H
#define RF_CUTSCENE_H
#include "rf/level.h"

/* Owned v180 camera/timeline/path resources. These are inert authored data;
 * scene control, point actions and camera playback belong to the runtime. */
typedef struct rf_cutscene_camera {
    uint32_t uid;
    float position[3],orientation[9]; /* right, up, forward rows */
} rf_cutscene_camera;
typedef struct rf_cutscene_point {
    uint32_t camera_uid,words[2];
    float durations[3];
    char path[64];
} rf_cutscene_point;
typedef struct rf_cutscene_descriptor {
    uint32_t selector,first_point,point_count,hide;
    float fov;
} rf_cutscene_descriptor;
typedef struct rf_cutscene_path {
    char name[64];
    uint32_t control_uids[4];
    float positions[4][3]; /* copied Bezier controls */
} rf_cutscene_path;
typedef struct rf_cutscene_resources {
    void *storage;
    rf_cutscene_camera *cameras;
    rf_cutscene_descriptor *descriptors;
    rf_cutscene_point *points;
    rf_cutscene_path *paths;
    uint32_t camera_count,descriptor_count,point_count,path_count,allocated_bytes;
} rf_cutscene_resources;

/* Empty destination required. Owns a single bounded allocation, borrows no
 * archive bytes, and leaves the destination unchanged on failure. */
int rf_cutscene_resources_open(const rf_level *level,uint32_t budget,rf_cutscene_resources *result);
void rf_cutscene_resources_close(rf_cutscene_resources *resources);
const rf_cutscene_descriptor *rf_cutscene_find(const rf_cutscene_resources *resources,uint32_t selector);
const rf_cutscene_camera *rf_cutscene_camera_find(const rf_cutscene_resources *resources,uint32_t uid);
const rf_cutscene_path *rf_cutscene_path_find(const rf_cutscene_resources *resources,const char *name);
void rf_cutscene_path_sample(const rf_cutscene_path *path,float fraction,float position[3]);
#endif
