#ifndef RF_PC_RASTER_H
#define RF_PC_RASTER_H
#include "rf/preview.h"
#include "rf/material.h"
#include "rf/lightmap.h"
typedef struct rf_pc_raster {float *depth;unsigned char *rgb;uint32_t width,height,pixels,scale;} rf_pc_raster;
int rf_pc_raster_open(rf_pc_raster *r,uint32_t scale);
void rf_pc_raster_close(rf_pc_raster *r);
int rf_pc_raster_frame(rf_pc_raster *r,const rf_preview_mesh *mesh,const rf_materials *materials,const rf_lightmaps *lightmaps,uint32_t world_vertices);
int rf_pc_raster_save(const rf_pc_raster *r,const char *path);
/* Particle pass over the existing forward-Z target. Same explicit depth
 * adapter and default mode domain as the Xbox pass; no depth writes. */
int rf_pc_raster_particle(rf_pc_raster *r,const rf_particle_draw_vertex *vertices,
    uint32_t count,const rf_image *image,uint32_t mode,float depth_scale,float depth_bias,
    uint32_t fog_enabled,uint32_t fog_rgb);
#endif
