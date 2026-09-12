#include "rf/scene_preview.h"
#include "rf/corpse_effect.h"
static void corpse_surface_fixture(rf_visibility_camera *camera,rf_particle_vertex_environment *environment,rf_corpse_surface_effect *effect)
{
    memset(camera,0,sizeof(*camera));memset(environment,0,sizeof(*environment));memset(effect,0,sizeof(*effect));
    camera->projection.matrix[0]=camera->projection.matrix[4]=camera->projection.matrix[8]=1;
    camera->projection.perspective=1;camera->projection.clip.enabled=1;camera->projection.clip.depth_enabled=1;
    camera->projection.clip.far_distance=10;camera->projection.projection.clamp=1;
    camera->projection.projection.half_width=320;camera->projection.projection.half_height=240;
    environment->vertex_color=environment->vertex_alpha=1;environment->depth_scale=environment->reciprocal_scale=1;
    environment->uv_scale[0]=environment->uv_scale[1]=1;
    effect->position[2]=4;effect->basis[0]=effect->basis[4]=effect->basis[8]=1;
    effect->growth_time=5;effect->max_extent=.5f;effect->growth_rate=1.5707963705062866f/5;effect->color=0x00ffffff;
}
