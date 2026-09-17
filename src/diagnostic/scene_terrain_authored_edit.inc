/* All fallible preparation precedes publication; cloned core is discarded on rejection. */
#include "rf/authored_reset_placement.h"
typedef struct scene_authored_edit_context {
    scene_stream *scene;const float *center,*basis;float scale;
    const rf_geomod_shallow_limit *limits;uint32_t count,reset;
} scene_authored_edit_context;
static int scene_authored_edit_create(void *opaque,uint32_t budget,rf_geomod_terrain **out)
{
    scene_stream *s=((scene_authored_edit_context *)opaque)->scene;
    scene_terrain_authored_assets *a=s->terrain_authored;
    rf_collision_face_filter generated=a->asset_view.source_filters[0];int status;
    generated.query_flags=0;generated.face_flags=256;
    status=rf_geomod_terrain_open(&a->source,a->asset_view.source_filters,&generated,0,
        SCENE_TERRAIN_SOURCE_VERTICES,SCENE_TERRAIN_FACES,budget,out);if(status)return status;
    return rf_geomod_terrain_set_mapping(*out,s->terrain_texture_width,s->terrain_texture_height);
}
static int scene_authored_edit_mutate(void *opaque,rf_geomod_terrain *candidate)
{
    scene_authored_edit_context *c=opaque;scene_stream *s=c->scene;
    if(c->reset)return rf_geomod_terrain_reset(candidate);
    return rf_geomod_terrain_cut_template_limits(candidate,s->terrain_template,c->center,c->basis,
        c->scale,s->terrain_material,c->limits,c->count);
}
static int scene_authored_edit_publish(void *opaque,rf_geomod_terrain *candidate,uint32_t serial)
{
    scene_stream *s=((scene_authored_edit_context *)opaque)->scene;
    rf_geomod_terrain *old=s->terrain;uint32_t previous=s->terrain_publication_serial;int status;
    s->terrain=candidate;s->terrain_publication_serial=serial;
    status=scene_terrain_authored_bind(s);
    s->terrain=old;s->terrain_publication_serial=previous;
    return status;
}
static void scene_authored_edit_abort(void *opaque)
{scene_terrain_publication_abort(((scene_authored_edit_context *)opaque)->scene);}
static int scene_terrain_authored_edit_run(scene_authored_edit_context *context)
{
    scene_stream *s=context->scene;
    scene_terrain_edit_callbacks ops={scene_authored_edit_create,scene_authored_edit_mutate,
        scene_authored_edit_publish,scene_authored_edit_abort};
    scene_terrain_edit_limits budget;scene_terrain_edit_result result;uint64_t external;int status;
    /* Destruction subsystem ceiling, not a claim that other game owners are free.
     * Actual whole-game stock64MiB headroom is checked in native runs. */
    external=(uint64_t)SCENE_PUBLICATION_BUDGET+SCENE_LIGHTING_STAGE_BUDGET+
        rf_scene_terrain_atlas[3]+s->terrain_authored->resident_bytes+sizeof(*s->terrain_draw)+
        SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s->terrain_colors)+sizeof(*s->terrain_light_cache)+
        sizeof(*s->debris)+sizeof(*s->terrain_template)+(uint64_t)s->terrain_region_count*sizeof(*s->terrain_regions)+
        1100u*(sizeof(uint32_t)+sizeof(rf_vfx_light_source))+SCENE_TERRAIN_FACES*sizeof(*s->terrain_ids)+(uint64_t)s->geometry->faces*sizeof(uint32_t);
    if(external>UINT32_MAX)return RF_RANGE;
    budget=(scene_terrain_edit_limits){SCENE_DESTRUCTION_BUDGET,(uint32_t)external,SCENE_TERRAIN_CORE_BUDGET};
    status=scene_terrain_edit_transaction(&s->terrain,&s->terrain_publication_serial,&budget,&ops,context,&result);
    if(!status && result.reserved_peak_bytes>rf_scene_geomod[4])rf_scene_geomod[4]=result.reserved_peak_bytes;
    return status;
}
static int scene_terrain_authored_template_edit(scene_stream *s,const float center[3],const float basis[9],
    float scale,const rf_geomod_shallow_limit *limits,uint32_t count)
{
    scene_authored_edit_context context={s,center,basis,scale,limits,count,0};
    return scene_terrain_authored_edit_run(&context);
}
/* Called only AFTER the original-room collision and zero-cut publication commit.
 * No allocation/fallible call here. Keep atlas registration/image/pixel addresses
 * and cut-basis RNG: the established DEV reset resets mappings, not global RNG.
 */
static inline void scene_terrain_authored_reset_cleanup(scene_stream *s)
{
    uint32_t i,k;
    s->terrain_history_count=0;
    memset(s->terrain_history,0,sizeof(s->terrain_history));memset(s->terrain_requested,0,sizeof(s->terrain_requested));
    memset(s->terrain_noise,0,sizeof(*s->terrain_noise));memset(s->terrain_atlas_pixels,0,512u*512u*2u);
    memset(s->terrain_tile,0,64u*64u*2u);memset(s->terrain_light_cache,0,sizeof(*s->terrain_light_cache));
    memset(s->terrain_tiles,0,SCENE_TERRAIN_FACES*sizeof(*s->terrain_tiles));
    memset(s->terrain_bindings,0,SCENE_TERRAIN_FACES*sizeof(*s->terrain_bindings));
    for(i=0;i<SCENE_TERRAIN_FACES;i++)s->terrain_bindings[i].image=UINT32_MAX;
    for(i=0;i<SCENE_TERRAIN_DRAW_VERTICES;i++)for(k=0;k<3;k++)s->terrain_colors[i][k]=1;
    memset(s->terrain_draw,0,sizeof(*s->terrain_draw));
    s->terrain_draw->view=(rf_geomod_mesh_view){s->terrain_draw->vertices,s->terrain_draw->faces,0,0,s->terrain_publication_serial};
    memset(rf_scene_terrain_noise,0,sizeof(rf_scene_terrain_noise));
    rf_scene_terrain_noise[0]=1;rf_scene_terrain_noise[6]=sizeof(*s->terrain_noise);
    memset(rf_scene_terrain_bake,0,sizeof(rf_scene_terrain_bake));memset(rf_scene_terrain_shadows,0,sizeof(rf_scene_terrain_shadows));
    memset(rf_scene_terrain_draw,0,sizeof(rf_scene_terrain_draw));rf_scene_terrain_draw[4]=s->terrain_publication_serial;
    rf_scene_terrain_atlas[4]=s->terrain_publication_serial;rf_scene_terrain_atlas[5]=rf_scene_terrain_atlas[6]=0;
    scene_terrain_dirty(s,0,0,512,512); /* Ordinary upload occurs after commit. */
    if(s->debris) {
        memset(s->debris,0,sizeof(*s->debris));s->debris->random.value=1;rf_debris_audio_init(&s->debris->audio);
        memset(rf_scene_debris_audio,0,sizeof(rf_scene_debris_audio));memset(rf_scene_debris,0,sizeof(rf_scene_debris));
        memset(rf_scene_debris_relaunch,0,sizeof(rf_scene_debris_relaunch));memset(rf_scene_debris_wet,0,sizeof(rf_scene_debris_wet));
        rf_scene_debris_wet[3]=UINT32_MAX;rf_scene_debris[6]=sizeof(*s->debris);
    }
}
/* Root owns button edge/held handling, attempted/success counters and reporting.
 * This owns body clearance, cloned reset, original full-room publication and
 * post-commit cleanup; the caller must not perform a second bind/reset afterward.
 * Accept actual grounded crouched spheres too: crouch is part of the DEV reset
 * gesture, and no standing capsule may be substituted for the current body.
 */
static inline int scene_terrain_authored_reset(scene_stream *s,rf_checkpoint_placement_result *fit)
{
    rf_checkpoint_placement p={0};rf_checkpoint_placement_result result={UINT32_MAX,RF_CHECKPOINT_PLACEMENT_UNSUPPORTED,0};
    scene_authored_edit_context context={s,NULL,NULL,0,NULL,0,1};int status;
    if(!s || !s->terrain || !s->terrain_authored || !s->terrain_publication ||
       !s->terrain_publication->base || s->terrain_publication->has_pending || !s->collision ||
       !s->terrain_noise || !s->terrain_atlas_pixels || !s->terrain_tile || !s->terrain_light_cache ||
       !s->terrain_tiles || !s->terrain_bindings || !s->terrain_draw || !s->terrain_colors ||
       !scene_actor_body.allocated_bytes || !scene_actor_body.spheres.items || !scene_actor_body.spheres.count)return RF_RANGE;
    if(!campaign_spawn || campaign_player_damage.state.effects.health<=0 || campaign_climb.region ||
       campaign_player_view.linked_handle!=-1 || campaign_support_handle || rf_scene_actor_landing[1]!=1){if(fit)*fit=result;return RF_NOT_FOUND;}
    p.world=s->collision;p.replaced_room=s->terrain_collision.room;
    p.query_flags=(scene_actor_body.state.state_124&~0x1000u)|4u;
    p.spheres=scene_actor_body.spheres.items;p.count=scene_actor_body.spheres.count;
    memcpy(p.position,scene_actor_body.state.position,12);memcpy(p.basis,scene_actor_body.state.orientation,36);
    status=rf_authored_reset_standing_check(s->terrain_publication->base,
        s->terrain_authored->asset_view.source_planes,s->terrain_authored->source.face_count,
        &p,scene_step_seconds,rf_scene_actor_movement_values.speed,&result);
    if(status){if(status==RF_NOT_FOUND && fit)*fit=result;return status;}
    /* No admission/cache/RNG/debris bytes change before this fallible operation.
     * Zero cuts publish the immutable original-room tree and empty render bank. */
    status=scene_terrain_authored_edit_run(&context);if(status)return status;
    scene_terrain_authored_reset_cleanup(s);if(fit)*fit=result;return RF_OK;
}
