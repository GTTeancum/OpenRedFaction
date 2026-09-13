#ifndef RF_VOLUME_PIXEL_FIXTURE_H
#define RF_VOLUME_PIXEL_FIXTURE_H
#include "rf/visibility.h"
#include <string.h>
static int volume_pixel_fixture(rf_particle_draw_vertex vertices[12],uint32_t *count)
{
    rf_visibility_camera_parameters p={0};rf_visibility_camera camera={0};rf_particle_screen_polygon polygon={0};
    rf_particle_vertex_environment e={0};rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    const float end[3]={-2,0,8},start[3]={2,0,8};uint32_t i;int status;
    p.viewport.width=640;p.viewport.height=480;p.viewport.pixel_aspect=1;p.viewport.fov=90;p.viewport.far_distance=128;p.viewport.perspective=1;
    p.basis[0]=p.basis[4]=p.basis[8]=1;p.near_distance=.1f;p.clip_enabled=1;p.projection_clamp=1;
    status=rf_visibility_camera_setup(&p,&camera);if(status)return status;
    status=rf_volume_beam_project(&camera,end,start,2,&polygon);if(status)return status;
    if(polygon.count!=4)return RF_FORMAT;
    status=rf_particle_render_decode(RF_PARTICLE_GLOW_MODE,&re,&states);if(status)return status;
    e.rgba=0x80ffffff;e.vertex_color=states.vertex_color;e.vertex_alpha=states.vertex_alpha;
    e.depth_scale=e.reciprocal_scale=camera.view.scale[2];e.uv_scale[0]=e.uv_scale[1]=1;
    for(i=0;i<polygon.count;++i){status=rf_particle_vertex_encode(&e,polygon.vertices+i,vertices+i);if(status)return status;}
    *count=polygon.count;return RF_OK;
}
#endif
