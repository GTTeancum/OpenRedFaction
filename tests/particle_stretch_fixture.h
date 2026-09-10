/* Process-local shared fixtures; not campaign content or original game logic. */
#ifndef RF_PARTICLE_STRETCH_FIXTURE_H
#define RF_PARTICLE_STRETCH_FIXTURE_H
#include "rf/visibility.h"
#include "rf/scene_preview.h"
#include <string.h>
static int particle_stretch_fixture(unsigned test,rf_particle_draw_vertex vertices[12],uint32_t *count)
{
    rf_visibility_camera camera={0};rf_particle_screen_polygon polygon={0};
    rf_particle_vertex_environment env={0};float position[3]={0,0,2},previous[3]={1,1,2};unsigned i;int status;
    camera.view.scale[0]=camera.view.scale[1]=camera.view.scale[2]=1;
    camera.projection.matrix[0]=camera.projection.matrix[4]=camera.projection.matrix[8]=1;
    camera.projection.perspective=1;camera.projection.flat_depth=.98f;
    camera.projection.clip.enabled=camera.projection.clip.depth_enabled=camera.projection.clip.far_enabled=1;
    camera.projection.clip.far_distance=100;
    camera.projection.projection.clamp=1;camera.projection.projection.half_width=320;camera.projection.projection.half_height=240;
    if(test==1){previous[0]=previous[1]=0;} /* ordinary fallback */
    if(test==2){position[0]=1.8f;previous[0]=3;previous[1]=0;} /* right clipping */
    if(test==3){position[2]=-2;previous[2]=-2;} /* rejected */
    if(test==4){previous[0]=-1;previous[1]=.5f;previous[2]=4;} /* depth motion projected onto view plane */
    if(test==5){previous[0]=previous[1]=0;previous[2]=4;} /* view-parallel displacement */
    status=rf_particle_world_stretch(&camera,position,previous,1.5f,1,1,&polygon);if(status)return status;
    env.rgba=0x80ffffff;env.vertex_color=env.vertex_alpha=1;env.depth_scale=env.reciprocal_scale=1;
    env.uv_scale[0]=env.uv_scale[1]=1;
    for(i=0;i<polygon.count;i++){status=rf_particle_vertex_encode(&env,polygon.vertices+i,vertices+i);if(status)return status;}
    *count=polygon.count;return RF_OK;
}
#endif
