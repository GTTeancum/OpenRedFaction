#include "rf/eye.h"
#include "rf/scene_preview.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
static rf_scene_input_poll player_poll;
static void *player_context;
static uint32_t player_frame_limit;
static rf_scene_input player_input;
static uint32_t campaign_spawn;
static float campaign_position[3],campaign_orientation[9];
uint32_t rf_scene_player_spawn_diagnostic[19];
int rf_scene_set_campaign_spawn(const rf_level *level)
{
    unsigned i,j;
    if(!level){campaign_spawn=0;memset(rf_scene_player_spawn_diagnostic,0,sizeof(rf_scene_player_spawn_diagnostic));return RF_OK;}
    for(i=0;i<3;++i) {
        if(!isfinite(level->player_position[i]))return RF_FORMAT;
        for(j=0;j<3;++j)if(!isfinite(level->player_orientation[i][j]))return RF_FORMAT;
    }
    memcpy(campaign_position,level->player_position,12);memcpy(campaign_orientation,level->player_orientation,36);
    memset(rf_scene_player_spawn_diagnostic,0,sizeof(rf_scene_player_spawn_diagnostic));
    rf_scene_player_spawn_diagnostic[0]=1;memcpy(rf_scene_player_spawn_diagnostic+1,campaign_position,12);
    memcpy(rf_scene_player_spawn_diagnostic+4,campaign_orientation,36);campaign_spawn=1;return RF_OK;
}
static uint32_t (*profile_clock)(void);
static uint32_t profile_last,profile_active;
uint32_t rf_scene_profile[8][4],rf_scene_profile_stage[2];
void rf_scene_set_profile(uint32_t (*milliseconds)(void))
{profile_clock=milliseconds;profile_active=0;memset(rf_scene_profile,0,sizeof(rf_scene_profile));}
static void profile_mark(uint32_t stage)
{
    uint32_t now,elapsed,*row;uint64_t total;
    rf_scene_profile_stage[1]=stage;
    if(!profile_clock)return;
    now=profile_clock();elapsed=now-profile_last;profile_last=now;
    if(!profile_active || !stage)return;
    row=rf_scene_profile[stage];total=((uint64_t)row[2]<<32)+row[1]+elapsed;
    ++row[0];row[1]=(uint32_t)total;row[2]=(uint32_t)(total>>32);
    if(elapsed>row[3])row[3]=elapsed;
}
uint32_t rf_scene_player_input_frames[64][7];
void rf_scene_set_input(rf_scene_input_poll poll,void *context,uint32_t frame_limit)
{player_poll=poll;player_context=context;player_frame_limit=frame_limit;}
static int player_begin_frame(void *context,uint32_t frame)
{
    rf_scene_input value={0};uint32_t i,*r=rf_scene_player_input_frames[frame%64];int status;(void)context;
    if(!frame)memset(rf_scene_player_input_frames,0,sizeof(rf_scene_player_input_frames));
    status=player_poll(player_context,frame,&value);if(status)return status;
    for(i=0;i<3;++i)if(!isfinite(value.move[i]) || fabsf(value.move[i])>1)return RF_FORMAT;
    for(i=0;i<2;++i)if(!isfinite(value.look[i]) || fabsf(value.look[i])>1)return RF_FORMAT;
    if(value.crouch>1)return RF_FORMAT;
    player_input=value;r[0]=frame;memcpy(r+1,&value,sizeof(value));
    profile_active=frame>=16;rf_scene_profile_stage[0]=frame;profile_mark(0);return RF_OK;
}
uint32_t rf_scene_showcase_enabled;
int rf_scene_showcase_camera(rf_level *level)
{
    unsigned i;int status=rf_scene_preview_mover_camera(level,8544,-8.0f);
    if(status)return status;
    for(i=0;i<3;++i)level->player_position[i]+=2.0f*level->player_orientation[0][i];
    level->player_position[1]-=.5f;
    return RF_OK;
}
int rf_scene_preview_mover_camera(rf_level *level,int32_t uid,float distance)
{
    rf_geometry_movers movers={0};uint32_t i,j,axis=0;int status;
    float minimum[3],maximum[3],center[3],forward[3],right[3],up[3],length;
    rf_collision_ray_hit local={0},world;
    if(!level || !isfinite(distance) || fabsf(distance)<0.1f)return RF_RANGE;
    status=rf_geometry_movers_open(level,1024*1024,&movers);if(status)return status;
    for(i=0;i<movers.count;++i)if(movers.items[i].uid==uid)break;
    if(i==movers.count){status=RF_NOT_FOUND;goto done;}
    {
        const rf_geometry_mover *m=movers.items+i;
        if(!m->geometry.vertices){status=RF_FORMAT;goto done;}
        status=rf_geometry_vertex(&m->geometry,0,minimum);if(status)goto done;
        memcpy(maximum,minimum,sizeof(minimum));
        for(i=1;i<m->geometry.vertices;++i) {
            float p[3];status=rf_geometry_vertex(&m->geometry,i,p);if(status)goto done;
            for(j=0;j<3;++j){if(p[j]<minimum[j])minimum[j]=p[j];if(p[j]>maximum[j])maximum[j]=p[j];}
        }
        for(j=0;j<3;++j){center[j]=(minimum[j]+maximum[j])*0.5f;if(maximum[j]-minimum[j]<maximum[axis]-minimum[axis])axis=j;}
        memcpy(local.point,center,sizeof(center));local.normal[axis]=distance>0?1.0f:-1.0f;
        status=rf_collision_contact_world(&local,m->position,m->orientation,&world);if(status)goto done;
    }
    length=sqrtf(world.normal[0]*world.normal[0]+world.normal[1]*world.normal[1]+world.normal[2]*world.normal[2]);
    if(!isfinite(length) || length<0.0001f){status=RF_RANGE;goto done;}
    for(j=0;j<3;++j)forward[j]=world.normal[j]/length;
    right[0]=forward[2];right[1]=0;right[2]=-forward[0];
    length=sqrtf(right[0]*right[0]+right[2]*right[2]);
    if(length<0.0001f){status=RF_RANGE;goto done;}
    right[0]/=length;right[2]/=length;
    up[0]=forward[1]*right[2];up[1]=forward[2]*right[0]-forward[0]*right[2];up[2]=-forward[1]*right[0];
    for(j=0;j<3;++j) {
        level->player_position[j]=world.point[j]-fabsf(distance)*forward[j];
        level->player_orientation[0][j]=right[j];level->player_orientation[1][j]=up[j];level->player_orientation[2][j]=forward[j];
    }
done:
    rf_geometry_movers_close(&movers);return status;
}
void rf_scene_world_geometry_close(rf_scene_world_geometry *geometry)
{
    if(!geometry)return;
    rf_geometry_movers_close(&geometry->movers);free(geometry->offsets);free(geometry->slots);
    memset(geometry,0,sizeof(*geometry));
}
int rf_scene_world_open_retained(const rf_level *level,const rf_geometry *world,
    rf_vpp *maps,uint32_t map_count,rf_preview_mesh *mesh,rf_materials *materials,
    uint32_t mesh_budget,uint32_t material_budget,rf_scene_world_geometry *geometry)
{
    rf_geometry_movers movers={0};rf_geometry_materials bundle={0};
    const rf_geometry **sources=NULL;uint32_t i;int status;
    if(!geometry || geometry->world || geometry->movers.data || geometry->offsets || geometry->slots ||
        !mesh || mesh->vertices || mesh->bytes || !materials || materials->items || materials->count)return RF_RANGE;
    status=rf_geometry_movers_open(level,1024*1024,&movers);if(status)goto done;
    if(rf_scene_showcase_enabled) {
        /* Half the authored endpoint displacement for both pairs of exit panels.
         * Values originate in L1S1 section 3000, keys 8591..8594 and 8603..8606. */
        for(i=0;i<movers.count;++i) {
            rf_geometry_mover *m=movers.items+i;
            if(m->uid==8544){m->position[0]+=(-55.73634338378906f+51.56401062011719f)*.5f;m->position[2]+=(-21.200519561767578f+19.514785766601562f)*.5f;}
            if(m->uid==8543){m->position[0]+=(-42.98756408691406f+47.62348937988281f)*.5f;m->position[2]+=(-16.04967498779297f+17.922714233398438f)*.5f;}
            if(m->uid==8524){m->position[0]+=( -38.530059814453125f+39.82415771484375f)*.5f;m->position[2]+=(40.75068664550781f-45.580322265625f)*.5f;}
            if(m->uid==8523){m->position[0]+=(-42.218231201171875f+40.92414093017578f)*.5f;m->position[2]+=(54.51513671875f-49.68550109863281f)*.5f;}
        }
    }
    sources=malloc(((size_t)movers.count+1)*sizeof(*sources));if(!sources){status=RF_RANGE;goto done;}
    sources[0]=world;for(i=0;i<movers.count;++i)sources[i+1]=&movers.items[i].geometry;
    status=rf_geometry_materials_open(&bundle,sources,movers.count+1,maps,map_count,material_budget);
    if(!status)status=rf_preview_build_world(mesh,world,&movers,NULL,&bundle,level,mesh_budget);
    if(!status) {
        rf_scene_world_geometry next={0};next.world=world;next.movers=movers;
        next.offsets=bundle.offsets;next.slots=bundle.slots;next.geometry_count=bundle.count;next.material_count=bundle.textures.count;
        next.allocated_bytes=sizeof(next)+movers.allocated_bytes-sizeof(movers)+
            (bundle.count+1)*sizeof(uint32_t)+bundle.offsets[bundle.count]*sizeof(uint32_t);
        memcpy(next.camera_position,level->player_position,12);memcpy(next.camera_orientation,level->player_orientation,36);
        *geometry=next;memset(&movers,0,sizeof(movers));bundle.offsets=bundle.slots=NULL;
        *materials=bundle.textures;memset(&bundle.textures,0,sizeof(bundle.textures));
    }
done:
    free(sources);rf_geometry_materials_close(&bundle);rf_geometry_movers_close(&movers);return status;
}
int rf_scene_world_open(const rf_level *level,const rf_geometry *world,
    rf_vpp *maps,uint32_t map_count,rf_preview_mesh *mesh,rf_materials *materials,
    uint32_t mesh_budget,uint32_t material_budget)
{
    rf_scene_world_geometry geometry={0};
    int status=rf_scene_world_open_retained(level,world,maps,map_count,mesh,materials,mesh_budget,material_budget,&geometry);
    rf_scene_world_geometry_close(&geometry);return status;
}
int rf_scene_world_update(const rf_scene_world_geometry *geometry,
    const rf_group_attached_pose *poses,uint32_t pose_count,
    rf_preview_mesh *mesh,uint32_t capacity_bytes)
{
    rf_geometry_materials mapping={0};rf_level camera={0};
    if(!geometry || !geometry->world || (poses?pose_count!=geometry->movers.count:pose_count!=0))return RF_RANGE;
    mapping.offsets=geometry->offsets;mapping.slots=geometry->slots;mapping.count=geometry->geometry_count;
    mapping.textures.count=geometry->material_count;
    memcpy(camera.player_position,geometry->camera_position,12);memcpy(camera.player_orientation,geometry->camera_orientation,36);
    return rf_preview_update_world(mesh,capacity_bytes,geometry->world,&geometry->movers,poses,&mapping,&camera);
}
int rf_scene_world_update_camera(const rf_scene_world_geometry *geometry,
    const rf_group_attached_pose *poses,uint32_t pose_count,const float position[3],
    const float orientation[3][3],rf_preview_mesh *mesh,uint32_t capacity_bytes)
{
    rf_scene_world_geometry view;uint32_t i,j;
    if(!geometry || !position || !orientation)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]))return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(orientation[i][j]))return RF_RANGE;
    }
    view=*geometry;memcpy(view.camera_position,position,12);memcpy(view.camera_orientation,orientation,36);
    return rf_scene_world_update(&view,poses,pose_count,mesh,capacity_bytes);
}
int rf_scene_world_update_camera_staged(const rf_scene_world_geometry *geometry,
    const rf_group_attached_pose *poses,uint32_t pose_count,const float position[3],
    const float orientation[3][3],rf_preview_mesh *mesh,uint32_t capacity_bytes,
    rf_preview_vertex *scratch,uint32_t scratch_bytes)
{
    rf_geometry_materials mapping={0};rf_level camera={0};uint32_t i,j;
    if(!geometry || !geometry->world || !position || !orientation ||
       (poses?pose_count!=geometry->movers.count:pose_count!=0))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]))return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(orientation[i][j]))return RF_RANGE;
    }
    mapping.offsets=geometry->offsets;mapping.slots=geometry->slots;
    mapping.count=geometry->geometry_count;mapping.textures.count=geometry->material_count;
    memcpy(camera.player_position,position,12);memcpy(camera.player_orientation,orientation,36);
    return rf_preview_update_world_staged(mesh,capacity_bytes,scratch,scratch_bytes,
        geometry->world,&geometry->movers,poses,&mapping,&camera);
}
int rf_scene_preview_camera(rf_level *level,int32_t uid)
{
    rf_level_entity entity;uint32_t i,j;int status;
    if(!level)return RF_RANGE;
    status=rf_level_entity_find(level,uid,&entity);if(status)return status;
    for(i=0;i<3;++i) {
        level->player_position[i]=entity.position[i]+2.2f*entity.orientation[2][i];
        for(j=0;j<3;++j)level->player_orientation[i][j]=(i==1?1.0f:-1.0f)*entity.orientation[i][j];
    }
    return RF_OK;
}
int rf_scene_preview_route_camera(rf_level *level,int32_t uid)
{
    rf_level_entity entity;int status;
    if(!level)return RF_RANGE;
    status=rf_level_entity_find(level,uid,&entity);if(status)return status;
    memcpy(level->player_position,entity.position,12);
    level->player_position[0]+=16.6f;level->player_position[1]-=1.0f;level->player_position[2]+=2.4f;
    memset(level->player_orientation,0,sizeof(level->player_orientation));
    level->player_orientation[0][0]=-1;level->player_orientation[1][1]=1;level->player_orientation[2][2]=-1;
    return RF_OK;
}
typedef struct scene_stream {
    rf_preview_mesh *mesh;rf_materials *materials;const rf_model_materials *bundle;
    uint32_t world,base,capacity;rf_scene_frame_sink sink;void *context;
    const rf_geometry_collision_world *collision;
    const rf_geometry *geometry;unsigned char *surface_indices;float actor_spawn[3];uint32_t eye_flags;
} scene_stream;
static const rf_scene_world_geometry *actor_follow_world;
void rf_scene_actor_follow(const rf_scene_world_geometry *world) {actor_follow_world=world;}
uint32_t rf_scene_actor_follow_summary[5]; /* frames, world hash, peak world bytes, camera hash, CPU capacity */
uint32_t rf_scene_actor_follow_frames[64][14]; /* absolute frame, world vertices, camera position/orientation */
static const float scene_step_seconds=1.0f/60.0f;
rf_physics_body scene_actor_body;
uint32_t rf_scene_actor_physics_diagnostic[8];
uint32_t rf_scene_actor_initial_animation[12];
float rf_scene_actor_initial_eye_offsets[6];
int32_t rf_scene_actor_initial_eye_tag;
uint32_t rf_scene_actor_eye_enabled;
uint32_t rf_scene_actor_turn_enabled,rf_scene_actor_look_enabled,rf_scene_actor_look_frames[64][33];
static rf_look_pose actor_look;
uint32_t rf_scene_actor_eye_frames[64][46]; /* frame, rf_eye_input, rf_first_person_pose */
uint32_t rf_scene_actor_animation_timing[64][3];
typedef struct actor_sweep_record {
    float start[3],delta[3],radius;int32_t status;uint32_t matched;
    rf_geometry_world_sweep_hit hit;
} actor_sweep_record;
actor_sweep_record rf_scene_actor_sweep_records[48];
rf_physics_body_state rf_scene_actor_fall_state;
rf_physics_body_state rf_scene_actor_initial_state;
uint32_t rf_scene_actor_tick_stats[8]; /* magic, frames, passes, contacts, capped frames, max passes, last remaining bits, status */
rf_group_attached_pose rf_scene_actor_pose;
uint32_t rf_scene_actor_initial_world[8],rf_scene_actor_initial_fall[8];
uint32_t rf_scene_actor_render_frames[64][5]; /* vertices, hash, body position bits */
typedef struct actor_ground_record {
    rf_physics_ground_probe probe;
    rf_geometry_world_sweep_hit hit;
    uint32_t matched;
} actor_ground_record;
_Static_assert(sizeof(actor_ground_record)==132,"Guest ground record layout");
actor_ground_record rf_scene_actor_ground_records[64];
uint32_t rf_scene_actor_ground_stats[8]; /* magic, records, hits, walkable, first walkable frame, hash, stride, status */
uint32_t rf_scene_actor_landing[8]; /* magic, descriptor index, frame, landings, grounded ticks, status, support commits, support losses */
rf_movement_descriptor rf_scene_actor_movement[2]; /* authored run and fall */
rf_entity_movement_values rf_scene_actor_movement_values;
rf_movement_config rf_scene_actor_movement_config;
rf_movement_settings rf_scene_actor_movement_settings;
float rf_scene_actor_run_traction;
rf_entity_material rf_scene_actor_surface_values[10];
uint32_t rf_scene_actor_ground_material;
uint32_t rf_scene_actor_surface_frames[64][2]; /* material index, traction bits */
static int scene_surface_open(rf_vpp *archive,scene_stream *stream)
{
    rf_vpp_entry entry;rf_surface_materials *palette=NULL;void *text=NULL;
    uint32_t i;int status=rf_vpp_find(archive,"materials.tbl",&entry);char name[256];
    if(status)return status;
    if(!entry.size || entry.size>65536 || !stream->geometry)return RF_RANGE;
    text=malloc(entry.size);palette=malloc(sizeof(*palette));
    if(!text || !palette) {status=RF_IO;goto done;}
    status=rf_vpp_read(archive,&entry,0,text,entry.size);if(status)goto done;
    status=rf_surface_materials_read(text,entry.size,palette);if(status)goto done;
    stream->surface_indices=malloc(stream->geometry->textures?stream->geometry->textures:1);
    if(!stream->surface_indices) {status=RF_IO;goto done;}
    for(i=0;i<stream->geometry->textures;++i) {
        status=rf_geometry_texture_name(stream->geometry,i,name,sizeof(name));if(status)goto done;
        stream->surface_indices[i]=(unsigned char)rf_surface_material_lookup(palette,name);
    }
    memcpy(rf_scene_actor_surface_values,palette->materials,sizeof(rf_scene_actor_surface_values));
    rf_scene_actor_ground_material=0;rf_scene_actor_run_traction=palette->materials[0].traction;
done:
    free(text);free(palette);return status;
}
uint32_t rf_scene_actor_movement_frames[64][3]; /* response, speed, numeric mode */
static int actor_set_speed_mode(int crouched)
{
    int status=rf_movement_set_mode(&rf_scene_actor_movement_settings,&rf_scene_actor_movement_config,
        crouched?0:1,-1,scene_actor_body.state.mass,0);
    if(!status)scene_actor_body.state.coefficients[1]=rf_scene_actor_movement_settings.response;
    return status;
}
uint32_t rf_scene_actor_ground_modes[64];
rf_physics_stance_cache rf_scene_actor_stance_cache;
uint32_t rf_scene_actor_stance_request,rf_scene_actor_stance_flags;
uint32_t rf_scene_actor_stance_frames[64][4]; /* requested, flags, sphere hash, standing blocked */
uint32_t rf_scene_actor_drive_enabled;
uint32_t rf_scene_actor_live_enabled;
uint32_t rf_scene_actor_frame_count=64;
uint32_t rf_scene_actor_ring_frames[64];
uint32_t rf_scene_actor_live_summary[8]; /* magic, frames, geometry hash, body hash, mode, landings, losses, status */
float rf_scene_actor_input_frames[64][3];
uint32_t rf_scene_actor_contact_count;
uint32_t rf_scene_actor_contacts[64][25]; /* frame, pass, mode, 15 input + 7 result floats */
void rf_scene_actor_drive(int profile) {rf_scene_actor_drive_enabled=(uint32_t)profile;}
static void actor_command(uint32_t frame,float command[3])
{
    if(player_poll){memcpy(command,player_input.move,12);return;}
    memset(command,0,12);
    if(rf_scene_actor_drive_enabled==1 && frame>=24 && frame<48)command[0]=.25f;
    if(rf_scene_actor_drive_enabled==2 && frame>=24 && frame<63)command[0]=-1.0f;
    if(rf_scene_actor_live_enabled && frame>=63)command[0]=1.0f;
}
uint32_t rf_scene_actor_locomotion_frames[64][12]; /* executed, mode/direction, input, candidates, current/next/duration */
static int actor_movement_select(void *context,uint32_t frame,rf_motion_controller *controller,const int32_t motions[23])
{
    rf_motion_movement movement={0};uint32_t *record;int status;(void)context;
    if(frame>=rf_scene_actor_frame_count)return RF_RANGE;
    actor_command(frame,movement.vector);
    movement.mode=frame?(int32_t)rf_scene_actor_landing[1]:3;
    movement.direction=rf_scene_actor_movement_settings.mode;
    /* Ordinary unarmed candidates from 41f6ee..41f729. Priority/AI candidate
     * selection remains outside this miner fixture. */
    movement.idle_state=0;movement.move_state=2;movement.alternate_state=4;
    status=rf_motion_select_movement(controller,motions,&movement);if(status)return status;
    record=rf_scene_actor_locomotion_frames[frame%64];record[0]=1;
    record[1]=(uint32_t)movement.mode;record[2]=(uint32_t)movement.direction;
    memcpy(record+3,movement.vector,12);record[6]=0;record[7]=2;record[8]=4;
    record[9]=(uint32_t)controller->current;record[10]=(uint32_t)controller->next;
    memcpy(record+11,&controller->duration,4);return RF_OK;
}
static int actor_ground_query(const rf_geometry_collision_world *world,actor_ground_record *r)
{
    float start[3],delta[3];uint32_t k;int status;
    memset(r,0,sizeof(*r));
    /* Original falling/grounded depths, stationary support velocity. Queries
     * are retained every frame; commits obey the grounded movement gate. */
    status=rf_physics_ground_prepare(scene_actor_body.spheres.items,scene_actor_body.spheres.count,
        scene_actor_body.state.position,scene_actor_body.state.state_124,rf_scene_actor_landing[1]==3,
        scene_step_seconds,rf_scene_actor_movement_values.speed,0,&r->probe);if(status)return status;
    for(k=0;k<3;++k) {
        start[k]=(float)((double)r->probe.start[k]+r->probe.sphere.center[k]);
        delta[k]=(float)((double)r->probe.end[k]-r->probe.start[k]);
    }
    status=rf_geometry_collision_world_sweep(world,r->probe.query_flags,start,delta,r->probe.sphere.radius,1,&r->hit,&r->matched);
    if(status)return status;
    return RF_OK;
}
static int actor_ground_check(const rf_geometry_collision_world *world,uint32_t frame)
{
    actor_ground_record *r=rf_scene_actor_ground_records+(frame%64);uint32_t k;int status;
    if(frame==0) {
        memset(rf_scene_actor_ground_stats,0,sizeof(rf_scene_actor_ground_stats));
        rf_scene_actor_ground_stats[0]=0x52464750;rf_scene_actor_ground_stats[4]=UINT32_MAX;
        rf_scene_actor_ground_stats[5]=2166136261u;rf_scene_actor_ground_stats[6]=sizeof(*r);
    }
    rf_scene_actor_ground_modes[frame%64]=rf_scene_actor_landing[1];
    status=actor_ground_query(world,r);if(status)return status;
    ++rf_scene_actor_ground_stats[1];
    if(r->matched && r->hit.hit.fraction<1) {
        ++rf_scene_actor_ground_stats[2];
        if(r->hit.hit.normal[1]>=.5f) {
            ++rf_scene_actor_ground_stats[3];
            if(rf_scene_actor_ground_stats[4]==UINT32_MAX)rf_scene_actor_ground_stats[4]=frame;
        }
    }
    for(k=0;k<sizeof(*r);++k)rf_scene_actor_ground_stats[5]=(rf_scene_actor_ground_stats[5]^((const unsigned char*)r)[k])*16777619u;
    rf_scene_actor_ground_stats[7]=1;return RF_OK;
}
float rf_scene_actor_contact[7]; /* impact speed, contact normal, response velocity */
float rf_scene_actor_contact_time[4]; /* first adjusted fraction/time, final remaining time, passes */
static int actor_sweep(const rf_geometry_collision_world *world,const rf_physics_body_state *state,
    float normal[3],float *fraction,uint32_t *sphere,uint32_t query_flags)
{
    float delta[3],start[3];uint32_t i,k,matched;
    *fraction=1;*sphere=UINT32_MAX;
    for(k=0;k<3;++k)delta[k]=state->next_position[k]-state->position[k];
    if(delta[0]==0 && delta[1]==0 && delta[2]==0)return RF_OK; /* 4df1c0 zero-displacement exit */
    for(i=0;i<scene_actor_body.spheres.count;++i) {
        const rf_physics_sphere *s=scene_actor_body.spheres.items+i;rf_geometry_world_sweep_hit hit;int status;
        for(k=0;k<3;++k)start[k]=(float)((double)state->position[k]+(double)s->center[0]*state->orientation[k]+
            (double)s->center[1]*state->orientation[3+k]+(double)s->center[2]*state->orientation[6+k]);
        status=rf_geometry_collision_world_sweep(world,query_flags,start,delta,s->radius,1,&hit,&matched);if(status)return status;
        if(matched && hit.hit.fraction<*fraction) {*sphere=i;*fraction=hit.hit.fraction;memcpy(normal,hit.hit.normal,12);}
    }
    return RF_OK;
}
actor_ground_record rf_scene_actor_stance_ground[64];
uint32_t rf_scene_actor_stance_support[64][9]; /* query, mode before/after, position before/after */
static int actor_stance_ground_commit(const rf_geometry_collision_world *world,uint32_t frame)
{
    actor_ground_record *r=rf_scene_actor_stance_ground+(frame%64);
    uint32_t *d=rf_scene_actor_stance_support[frame%64];int status,walkable;
    d[0]=1;d[1]=rf_scene_actor_landing[1];memcpy(d+3,scene_actor_body.state.position,12);
    status=actor_ground_query(world,r);if(status)return status;
    walkable=r->matched && r->hit.hit.fraction<1 && r->hit.hit.normal[1]>=.5f;
    if(walkable) {
        if(rf_scene_actor_landing[1]==3) {
            status=rf_physics_static_land(&scene_actor_body.state,&r->probe,r->hit.hit.fraction);if(status)return status;
            rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=frame;
            ++rf_scene_actor_landing[3];rf_scene_actor_landing[5]=1;
        } else {
            status=rf_physics_static_support(&scene_actor_body.state,&r->probe,r->hit.hit.fraction);if(status)return status;
            ++rf_scene_actor_landing[6];
        }
    } else if(rf_scene_actor_landing[1]==1) {
        scene_actor_body.state.flags|=1;rf_scene_actor_landing[1]=3;++rf_scene_actor_landing[7];
    }
    status=rf_group_pose_set_position(&rf_scene_actor_pose,scene_actor_body.state.position);if(status)return status;
    d[2]=rf_scene_actor_landing[1];memcpy(d+6,scene_actor_body.state.position,12);return RF_OK;
}
static int actor_stance_update(const rf_geometry_collision_world *world,uint32_t request,int *blocked,int live_frame)
{
    int status;*blocked=0;
    if(request>1)return RF_RANGE;
    int crouched=(rf_scene_actor_stance_flags&0x400)!=0;
    if(crouched!=(int)request) {
        if(!request) {
            rf_physics_body_state probe=scene_actor_body.state;float normal[3],fraction;uint32_t sphere;
            status=rf_physics_stand_endpoint(probe.position,rf_scene_actor_stance_cache.height_difference,probe.next_position);if(status)return status;
            status=actor_sweep(world,&probe,normal,&fraction,&sphere,(probe.state_124|4));if(status)return status;
            *blocked=sphere!=UINT32_MAX;
        }
        if(!*blocked) {
            status=rf_physics_stance_centers(&scene_actor_body.spheres,rf_scene_actor_stance_cache.centers[request],
                rf_scene_actor_stance_cache.count,&rf_scene_actor_stance_flags,request);if(status)return status;
            /* 4289d0/428a60 query immediately after replacing centers. */
            if(live_frame>=0) {status=actor_stance_ground_commit(world,(uint32_t)live_frame);if(status)return status;}
            status=actor_set_speed_mode(request!=0);if(status)return status;
        }
    }
    return RF_OK;
}
uint32_t rf_scene_actor_selector_frames[64][8]; /* current, next, effect, handled, flags before/after, blocked, mode */
static int rf_scene_actor_stance_blocked;
static int actor_selector_effect(void *context,uint32_t frame,const rf_motion_stance_decision *decision,
    const rf_motion_controller *controller)
{
    scene_stream *stream=context;uint32_t *record;int status;
    if(frame>=rf_scene_actor_frame_count)return RF_RANGE;
    memset(rf_scene_actor_stance_ground+(frame%64),0,sizeof(*rf_scene_actor_stance_ground));
    memset(rf_scene_actor_stance_support[frame%64],0,sizeof(rf_scene_actor_stance_support[0]));
    record=rf_scene_actor_selector_frames[frame%64];
    record[0]=(uint32_t)controller->current;record[1]=(uint32_t)controller->next;
    record[2]=decision->effect;record[3]=(uint32_t)decision->handled;record[4]=rf_scene_actor_stance_flags;
    rf_scene_actor_stance_blocked=0;
    rf_scene_actor_stance_request=(rf_scene_actor_stance_flags&0x400)!=0;
    if(decision->effect!=RF_MOTION_STANCE_NONE) {
        if(decision->effect!=RF_MOTION_STANCE_CROUCH && decision->effect!=RF_MOTION_STANCE_STAND)return RF_FORMAT;
        rf_scene_actor_stance_request=decision->effect==RF_MOTION_STANCE_CROUCH;
        status=actor_stance_update(stream->collision,rf_scene_actor_stance_request,&rf_scene_actor_stance_blocked,(int)frame);if(status)return status;
    }
    record[5]=rf_scene_actor_stance_flags;record[6]=(uint32_t)rf_scene_actor_stance_blocked;
    record[7]=(uint32_t)rf_scene_actor_movement_settings.mode;return RF_OK;
}
uint32_t rf_scene_actor_clearance_diagnostic[8];
float rf_scene_actor_clearance_queries[2][12]; /* start, end, normal, fraction, sphere, blocked */
static uint32_t actor_stance_hash(void)
{
    uint32_t i,h=2166136261u;const unsigned char *p=(const unsigned char*)scene_actor_body.spheres.items;
    for(i=0;i<scene_actor_body.spheres.count*sizeof(*scene_actor_body.spheres.items);++i)h=(h^p[i])*16777619u;
    p=(const unsigned char*)&rf_scene_actor_movement_settings;
    for(i=0;i<sizeof(rf_scene_actor_movement_settings);++i)h=(h^p[i])*16777619u;
    return h;
}
/* Test the live transition helper on a copy near the first real ceiling above
 * this crouched actor. A clear upward sweep establishes the approach path;
 * the rendered actor, owned spheres and gameplay counters are restored. */
static int actor_clearance_check(const rf_geometry_collision_world *world)
{
    rf_physics_body saved=scene_actor_body;rf_physics_sphere spheres[8];
    rf_movement_settings settings=rf_scene_actor_movement_settings;
    uint32_t flags=rf_scene_actor_stance_flags,sphere,i,before,after;float normal[3],fraction,ceiling;
    int status,blocked=0;
    memset(rf_scene_actor_clearance_diagnostic,0,sizeof(rf_scene_actor_clearance_diagnostic));
    memset(rf_scene_actor_clearance_queries,0,sizeof(rf_scene_actor_clearance_queries));
    rf_scene_actor_clearance_diagnostic[0]=0x5246434c;
    if(!(flags&0x400) || saved.spheres.count>8)return RF_FORMAT;
    memcpy(spheres,saved.spheres.items,saved.spheres.count*sizeof(*spheres));scene_actor_body.spheres.items=spheres;
    scene_actor_body.state.next_position[1]=scene_actor_body.state.position[1]+64;
    status=actor_sweep(world,&scene_actor_body.state,normal,&fraction,&sphere,scene_actor_body.state.state_124|4);
    if(status)goto done;
    if(sphere==UINT32_MAX || normal[1]>=0) {status=RF_NOT_FOUND;goto done;}
    ceiling=scene_actor_body.state.position[1]+fraction*64;
    for(i=0;i<2;++i) {
        float *q=rf_scene_actor_clearance_queries[i];rf_physics_body_state probe;
        scene_actor_body.state.position[1]=ceiling-(i?rf_scene_actor_stance_cache.height_difference+1:.2f);
        probe=scene_actor_body.state;
        status=rf_physics_stand_endpoint(probe.position,rf_scene_actor_stance_cache.height_difference,probe.next_position);if(status)goto done;
        status=actor_sweep(world,&probe,normal,&fraction,&sphere,probe.state_124|4);if(status)goto done;
        memcpy(q,probe.position,12);memcpy(q+3,probe.next_position,12);
        if(sphere!=UINT32_MAX)memcpy(q+6,normal,12);
        q[9]=fraction;q[10]=sphere==UINT32_MAX?-1:(float)sphere;
        before=actor_stance_hash();
        status=actor_stance_update(world,0,&blocked,-1);if(status)goto done;
        after=actor_stance_hash();q[11]=(float)blocked;
        if(i==0) {
            rf_scene_actor_clearance_diagnostic[4]=before;rf_scene_actor_clearance_diagnostic[5]=after;
            if(!blocked || before!=after || rf_scene_actor_stance_flags!=flags ||
               memcmp(spheres,saved.spheres.items,saved.spheres.count*sizeof(*spheres)) ||
               memcmp(&settings,&rf_scene_actor_movement_settings,sizeof(settings))) {status=RF_FORMAT;goto done;}
            rf_scene_actor_clearance_diagnostic[2]=1;
        } else {
            if(blocked || before==after || (rf_scene_actor_stance_flags&0x400) || rf_scene_actor_movement_settings.mode!=1) {status=RF_FORMAT;goto done;}
            rf_scene_actor_clearance_diagnostic[3]=1;
        }
        ++rf_scene_actor_clearance_diagnostic[1];
    }
    rf_scene_actor_clearance_diagnostic[6]=flags;rf_scene_actor_clearance_diagnostic[7]=1;
done:
    scene_actor_body=saved;rf_scene_actor_movement_settings=settings;rf_scene_actor_stance_flags=flags;
    return status;
}
int rf_scene_actor_fall_check(const rf_geometry_collision_world *world,uint32_t out[8])
{
    rf_physics_body_state current=scene_actor_body.state,proposal;
    float support[3]={0},normal[3],fraction=1;uint32_t step,i,sphere=UINT32_MAX,hash=2166136261u;
    if(!world || !out || !scene_actor_body.allocated_bytes || !scene_actor_body.spheres.count)return RF_RANGE;
    memset(rf_scene_actor_contact,0,sizeof(rf_scene_actor_contact));
    memset(rf_scene_actor_contact_time,0,sizeof(rf_scene_actor_contact_time));
    rf_scene_actor_initial_state=current;
    for(step=0;step<120;++step) {
        proposal=current;
        {int status=rf_physics_fall_propose(&proposal,1.0f/60,9.8f,support);if(status)return status;}
        {int status=actor_sweep(world,&proposal,normal,&fraction,&sphere,0x460);if(status)return status;}
        if(sphere!=UINT32_MAX) {
            /* Stationary non-liquid floor, no rotating actor predicate.
             * Continued substeps and actor pose/room commit remain separate. */
            int status=rf_physics_contact_advance(&proposal,1.0f/60,fraction,rf_scene_actor_contact_time+1);if(status)return status;
            rf_scene_actor_contact_time[0]=proposal.scalar_144;
            status=rf_physics_static_contact(&proposal,normal,support,support,rf_scene_actor_contact);if(status)return status;
            memcpy(rf_scene_actor_contact+1,normal,sizeof(normal));
            memcpy(rf_scene_actor_contact+4,proposal.velocity,sizeof(proposal.velocity));
            current=proposal;
            {
                float remaining=rf_scene_actor_contact_time[1];uint32_t pass=1;
                current.flags|=0x1000000;
                while(remaining>0) {
                    float hit_fraction,impact;uint32_t hit_sphere;
                    status=rf_physics_fall_propose(&current,remaining,9.8f,support);if(status)return status;
                    status=actor_sweep(world,&current,normal,&hit_fraction,&hit_sphere,0x460);if(status)return status;
                    if(hit_sphere==UINT32_MAX) {
                        memcpy(current.position,current.next_position,sizeof(current.position));current.scalar_144=1;remaining=0;
                    } else {
                        status=rf_physics_contact_advance(&current,remaining,hit_fraction,&remaining);if(status)return status;
                        status=rf_physics_static_contact(&current,normal,support,support,&impact);if(status)return status;
                    }
                    if((pass>3 && remaining<.25f) || pass>9) {++pass;break;}
                    ++pass;
                }
                rf_scene_actor_contact_time[2]=remaining;rf_scene_actor_contact_time[3]=(float)pass;
                status=rf_physics_spheres_bounds(scene_actor_body.spheres.items,scene_actor_body.spheres.count,current.position,&current.bounds);if(status)return status;
            }
            break;
        }
        /* Fixture accepts only unobstructed translations. Full actor pose/room
         * commit and contact response are not represented by this assignment. */
        current=proposal;memcpy(current.position,current.next_position,sizeof(current.position));
        {int status=rf_physics_spheres_bounds(scene_actor_body.spheres.items,scene_actor_body.spheres.count,current.position,&current.bounds);if(status)return status;}
    }
    rf_scene_actor_fall_state=current;
    for(i=0;i<sizeof(current);++i)hash=(hash^((const unsigned char*)&current)[i])*16777619u;
    out[0]=0x5246464c;out[1]=1;out[2]=step;out[3]=sphere;memcpy(out+4,&fraction,4);out[5]=hash;
    memcpy(out+6,current.position+1,4);memcpy(out+7,current.velocity+1,4);return RF_OK;
}
static int actor_trace_contacts=1;
static int actor_tick(const rf_geometry_collision_world *world,rf_physics_body_state *state,const float command[3],const float ground_normal[3])
{
    float remaining=scene_step_seconds,support[3]={0},normal[3];uint32_t pass=0,contacts=0;int status;
    int grounded=rf_scene_actor_landing[1]==1;
    if(grounded)++rf_scene_actor_landing[4];
    state->flags&=~0x1000000u;
    do {
        float fraction,impact;uint32_t sphere;
        if(grounded) {
            float input[3];
            status=rf_movement_transform(rf_scene_actor_movement[0].translation,command,
                state->orientation,state->orientation,state->orientation,input);if(status)return status;
            status=rf_physics_run_propose(state,remaining,rf_scene_actor_movement_settings.speed,
                rf_scene_actor_movement_values.acceleration,rf_scene_actor_run_traction,input,ground_normal,support);
        } else status=rf_physics_fall_propose(state,remaining,9.8f,support);
        if(status)return status;
        memset(state->vector_e0,0,sizeof(state->vector_e0)); /* full 49f3c0 clears force after proposal */
        state->flags|=0x1000000;
        status=actor_sweep(world,state,normal,&fraction,&sphere,0x460);if(status)return status;
        if(sphere==UINT32_MAX) {
            memcpy(state->position,state->next_position,sizeof(state->position));state->scalar_144=1;remaining=0;
        } else {
            uint32_t scratch[25],*record=scratch;
            if(actor_trace_contacts) {
                if(rf_scene_actor_contact_count>=64) {if(!rf_scene_actor_live_enabled)return RF_RANGE;}
                else record=rf_scene_actor_contacts[rf_scene_actor_contact_count++];
            }
            record[0]=rf_scene_actor_tick_stats[1];record[1]=pass;record[2]=rf_scene_actor_landing[1];
            memcpy(record+3,state->velocity,24);memcpy(record+9,normal,12);
            memcpy(record+12,support,12);memcpy(record+15,support,12);
            ++contacts;
            status=rf_physics_contact_advance(state,remaining,fraction,&remaining);if(status)return status;
            status=rf_physics_static_contact(state,normal,support,support,&impact);if(status)return status;
            memcpy(record+18,state->velocity,24);memcpy(record+24,&impact,4);
        }
        if((pass>3 && remaining<.25f) || pass>9) {++pass;break;}
        ++pass;
    } while(remaining>0);
    ++rf_scene_actor_tick_stats[1];rf_scene_actor_tick_stats[2]+=pass;rf_scene_actor_tick_stats[3]+=contacts;
    if(remaining>0)++rf_scene_actor_tick_stats[4];
    if(pass>rf_scene_actor_tick_stats[5])rf_scene_actor_tick_stats[5]=pass;
    memcpy(rf_scene_actor_tick_stats+6,&remaining,4);rf_scene_actor_tick_stats[7]=1;
    return RF_OK;
}
uint32_t rf_scene_actor_route_enabled;
uint32_t rf_scene_actor_routes[8][16];
/* Longer physics-only routes from the final rendered body. Each route owns its
 * evolving state; restore the render diagnostic after all routes. No host input. */
static int actor_routes(scene_stream *stream)
{
    static const float commands[8][3]={{1,0,0},{-1,0,0},{0,0,1},{0,0,-1},
        {.70710677f,0,.70710677f},{-.70710677f,0,.70710677f},{.70710677f,0,-.70710677f},{-.70710677f,0,-.70710677f}};
    rf_physics_body_state saved=scene_actor_body.state;
    uint32_t landing[8],ticks[8],material=rf_scene_actor_ground_material,route,step;
    float traction=rf_scene_actor_run_traction;
    memcpy(landing,rf_scene_actor_landing,sizeof(landing));memcpy(ticks,rf_scene_actor_tick_stats,sizeof(ticks));
    memset(rf_scene_actor_routes,0,sizeof(rf_scene_actor_routes));actor_trace_contacts=0;
    for(route=0;route<8;++route) {
        uint32_t *out=rf_scene_actor_routes[route],hash=2166136261u;int status=RF_OK;float previous[3];
        memcpy(previous,saved.position,12);
        scene_actor_body.state=saved;memcpy(rf_scene_actor_landing,landing,sizeof(landing));
        memset(rf_scene_actor_tick_stats,0,sizeof(rf_scene_actor_tick_stats));
        rf_scene_actor_run_traction=traction;rf_scene_actor_ground_material=material;out[14]=UINT32_MAX;
        for(step=0;step<600;++step) {
            actor_ground_record ground;rf_physics_body_state next=scene_actor_body.state;rf_group_attached_pose pose=rf_scene_actor_pose;
            int walkable,moved=memcmp(previous,next.position,12)!=0;uint32_t before=rf_scene_actor_landing[1],i;
            memcpy(previous,next.position,12);
            status=actor_ground_query(stream->collision,&ground);if(status)break;
            walkable=ground.matched && ground.hit.hit.fraction<1 && ground.hit.hit.normal[1]>=.5f;
            if(walkable) {
                rf_geometry_face face;
                status=rf_geometry_get_face(stream->geometry,ground.hit.face,&face);if(status)break;
                if(face.texture>=stream->geometry->textures) {status=RF_FORMAT;break;}
                rf_scene_actor_ground_material=stream->surface_indices[face.texture];
                rf_scene_actor_run_traction=rf_scene_actor_surface_values[rf_scene_actor_ground_material].traction;
                if(before==3) {status=rf_physics_static_land(&next,&ground.probe,ground.hit.hit.fraction);
                    rf_scene_actor_landing[1]=1;++out[2];out[15]=step;
                } else if(moved)status=rf_physics_static_support(&next,&ground.probe,ground.hit.hit.fraction);
                if(status)break;
            } else if(before==1 && moved) {
                next.flags|=1;rf_scene_actor_landing[1]=3;++out[3];if(out[14]==UINT32_MAX)out[14]=step;
            }
            status=actor_tick(stream->collision,&next,commands[route],ground.hit.hit.normal);if(status)break;
            status=rf_group_pose_set_position(&pose,next.position);if(status)break;
            memcpy(next.position,pose.position,12);memcpy(next.next_position,pose.pending,12);
            memcpy(next.bounds.minimum,pose.minimum,12);memcpy(next.bounds.maximum,pose.maximum,12);
            scene_actor_body.state=next;
            for(i=0;i<sizeof(next);++i)hash=(hash^((const unsigned char*)&next)[i])*16777619u;
            ++out[1];
        }
        out[0]=(uint32_t)status;out[4]=rf_scene_actor_tick_stats[3];out[5]=rf_scene_actor_tick_stats[4];
        out[6]=rf_scene_actor_landing[1];out[7]=hash;
        memcpy(out+8,scene_actor_body.state.position,12);memcpy(out+11,scene_actor_body.state.velocity,12);
    }
    scene_actor_body.state=saved;memcpy(rf_scene_actor_landing,landing,sizeof(landing));
    memcpy(rf_scene_actor_tick_stats,ticks,sizeof(ticks));rf_scene_actor_ground_material=material;
    rf_scene_actor_run_traction=traction;actor_trace_contacts=1;return RF_OK;
}
int rf_scene_actor_world_check(const rf_geometry_collision_world *world,uint32_t out[8])
{
    uint32_t i,q,k,n=0,hits=0,hash=2166136261u;float fraction=1;
    if(!world || !out || !scene_actor_body.allocated_bytes || scene_actor_body.spheres.count>8)return RF_RANGE;
    for(i=0;i<scene_actor_body.spheres.count;++i)for(q=0;q<6;++q) {
        const rf_physics_sphere *sphere=scene_actor_body.spheres.items+i;
        actor_sweep_record *r=rf_scene_actor_sweep_records+n;
        memset(r,0xa5,sizeof(*r));memset(r->delta,0,sizeof(r->delta));r->delta[q/2]=(q&1)?-2:2;r->radius=sphere->radius;
        for(k=0;k<3;++k)r->start[k]=(float)((double)scene_actor_body.state.position[k]+
            (double)sphere->center[0]*scene_actor_body.state.orientation[k]+
            (double)sphere->center[1]*scene_actor_body.state.orientation[3+k]+
            (double)sphere->center[2]*scene_actor_body.state.orientation[6+k]);
        r->status=rf_geometry_collision_world_sweep(world,0x460,r->start,r->delta,r->radius,1,&r->hit,&r->matched);
        if(r->status)return r->status;
        if(r->matched) {++hits;if(r->hit.hit.fraction<fraction)fraction=r->hit.hit.fraction;}
        ++n;
    }
    for(i=0;i<n*sizeof(*rf_scene_actor_sweep_records);++i)hash=(hash^((const unsigned char*)rf_scene_actor_sweep_records)[i])*16777619u;
    out[0]=0x52464157;out[1]=1;out[2]=n;out[3]=hits;memcpy(out+4,&fraction,4);
    out[5]=hash;out[6]=sizeof(*rf_scene_actor_sweep_records);out[7]=scene_actor_body.allocated_bytes;return RF_OK;
}
static int actor_follow_view(void *context,uint32_t frame,const rf_motion_controller *controller,rf_model_projection *view)
{
    scene_stream *stream=context;float position[3],orientation[3][3]={{-1,0,0},{0,1,0},{0,0,-1}};
    uint32_t *r=rf_scene_actor_follow_frames[frame%64];int status;
    profile_mark(1);
    if(scene_actor_body.allocated_bytes)memcpy(position,scene_actor_body.state.position,12);
    else memcpy(position,stream->actor_spawn,12);
    if(rf_scene_actor_eye_enabled) {
        rf_eye_input input={0};rf_first_person_pose pose;float eye_position[3];
        uint32_t *record=rf_scene_actor_eye_frames[frame%64];
        if(!scene_actor_body.allocated_bytes || rf_scene_actor_initial_eye_tag<0)return RF_FORMAT;
        memcpy(input.position,position,12);memcpy(input.orientation,scene_actor_body.state.orientation,36);
        memcpy(input.standing_offset,rf_scene_actor_initial_eye_offsets,12);
        memcpy(input.crouching_offset,rf_scene_actor_initial_eye_offsets+3,12);
        input.eye_tag=rf_scene_actor_initial_eye_tag;input.flags=stream->eye_flags;
        input.current_state=controller->current;input.previous_state=controller->next;
        input.transition_duration=controller->duration;input.transition_elapsed=controller->elapsed;
        status=rf_eye_position(&input,eye_position);if(status)return status;
        if(rf_scene_actor_look_enabled) {
            uint32_t *look=rf_scene_actor_look_frames[frame%64];
            if(!frame){
                memset(&actor_look,0,sizeof(actor_look));memset(rf_scene_actor_look_frames,0,sizeof(rf_scene_actor_look_frames));
                if(campaign_spawn) {
                    rf_spawn_look_angles angles;
                    status=rf_look_spawn_angles(campaign_orientation,scene_actor_body.state.orientation,
                        rf_scene_actor_movement[0].rotation,&angles);if(status)return status;
                    memcpy(actor_look.state.body_angles,angles.body,12);memcpy(actor_look.state.eye_angles,angles.eye,12);
                    memcpy(rf_scene_player_spawn_diagnostic+13,&angles,24);
                }
            }
            actor_look.state.command[0]=player_poll?player_input.look[0]:frame?((frame%180)<90?.25f:-.25f):0;
            actor_look.state.command[1]=player_poll?player_input.look[1]:rf_scene_actor_turn_enabled && frame?((frame%240)<120?.2f:-.2f):0;
            status=rf_look_update_pose(&actor_look.state,1.0f,scene_step_seconds,&actor_look);if(status)return status;
            if(rf_scene_actor_turn_enabled) {
                float tensor[9];
                status=rf_physics_tensor_world(scene_actor_body.state.local_tensor,actor_look.body_orientation,tensor);if(status)return status;
                memcpy(scene_actor_body.state.orientation,actor_look.body_orientation,36);
                memcpy(scene_actor_body.state.next_orientation,actor_look.body_orientation,36);
                memcpy(scene_actor_body.state.world_tensor,tensor,36);
                memcpy(input.orientation,actor_look.body_orientation,36);
                status=rf_eye_position(&input,eye_position);if(status)return status;
            }
            look[0]=frame;memcpy(look+1,&actor_look,sizeof(actor_look));
            status=rf_first_person_pose_copy(eye_position,input.orientation,(const float(*)[3])actor_look.eye_orientation,&pose);
        } else status=rf_first_person_pose_copy(eye_position,input.orientation,input.orientation,&pose);
        if(status)return status;
        memcpy(position,pose.position,12);memcpy(orientation,pose.eye_orientation,36);
        record[0]=frame;memcpy(record+1,&input,sizeof(input));memcpy(record+25,&pose,sizeof(pose));
    } else {position[1]+=.7f;position[2]+=2.4f;}
    /* The actor portion is idle until animation emits this tick's model. Use
     * it for transactional world projection before the actor is appended. */
    {uint32_t world_capacity=(stream->capacity-1024*1024)/sizeof(rf_preview_vertex)*sizeof(rf_preview_vertex);
     rf_preview_failure[0]=0;
     if(rf_scene_actor_eye_enabled && stream->mesh->bytes>world_capacity)
        status=rf_scene_world_update_camera(actor_follow_world,NULL,0,position,orientation,stream->mesh,stream->capacity);
     else {
        status=rf_scene_world_update_camera_staged(actor_follow_world,NULL,0,position,orientation,
        stream->mesh,world_capacity,stream->mesh->vertices+world_capacity/sizeof(rf_preview_vertex),
        stream->capacity-world_capacity);
        /* First-person rendering has no visible actor prefix to reserve. A
         * large world may use the whole allocation through the transactional
         * two-pass path instead of terminating at the staging-half boundary. */
        if(status==RF_RANGE && rf_scene_actor_eye_enabled && rf_preview_failure[0])
            status=rf_scene_world_update_camera(actor_follow_world,NULL,0,position,orientation,stream->mesh,stream->capacity);
     }
     if(status)return status;}
    profile_mark(2);
    stream->world=stream->mesh->count;
    memcpy(view->camera,position,12);memcpy(view->rotation,orientation,36);
    {uint32_t axis;for(axis=3;axis<6;++axis)view->rotation[axis]*=4.0f/3.0f;}
    r[0]=frame;r[1]=stream->world;memcpy(r+2,position,12);memcpy(r+5,orientation,36);
    {uint32_t i,*d=rf_scene_actor_follow_summary;if(!frame) {d[0]=d[2]=0;d[1]=d[3]=2166136261u;}
     d[0]=frame+1;d[4]=stream->capacity;if(stream->mesh->bytes>d[2])d[2]=stream->mesh->bytes;
     for(i=0;i<stream->mesh->bytes;++i)d[1]=(d[1]^((const unsigned char*)stream->mesh->vertices)[i])*16777619u;
     for(i=0;i<sizeof(rf_scene_actor_follow_frames[0]);++i)d[3]=(d[3]^((const unsigned char*)r)[i])*16777619u;}
    profile_mark(3);return RF_OK;
}
rf_entity_room_state rf_scene_actor_room_state;
uint32_t rf_scene_actor_room_frames[64][9],rf_scene_actor_room_summary[8];
typedef struct actor_room_context {const rf_geometry_collision_world *world;uint32_t called,face,retries;} actor_room_context;
static int actor_room_locate(void *context,const float position[3],rf_entity_room_result *result)
{
    actor_room_context *c=context;rf_collision_room_location hit;int status;
    c->called=1;status=rf_geometry_collision_world_locate(c->world,position,&hit);if(status)return status;
    memset(result,0,sizeof(*result));result->room=hit.room==UINT32_MAX?0:hit.room+1;
    result->name="";c->face=hit.face;c->retries=hit.retries;return RF_OK;
}
static int actor_room_refresh(const rf_geometry_collision_world *world,uint32_t frame)
{
    actor_room_context context={world,0,UINT32_MAX,0};uint32_t old=rf_scene_actor_room_state.room,i;
    uint32_t *r=rf_scene_actor_room_frames[frame%64],*d=rf_scene_actor_room_summary;int status;
    rf_scene_actor_room_state.flags=rf_scene_actor_pose.flags;
    /* This miner is not the local player; room-audio notification is absent. */
    status=rf_entity_room_refresh(&rf_scene_actor_room_state,scene_actor_body.state.position,0,actor_room_locate,NULL,&context);if(status)return status;
    rf_scene_actor_pose.flags=rf_scene_actor_room_state.flags;
    r[0]=frame;r[1]=rf_scene_actor_room_state.room;r[2]=rf_scene_actor_room_state.flags;
    memcpy(r+3,rf_scene_actor_room_state.query_position,12);r[6]=context.face;r[7]=context.called;r[8]=context.retries;
    d[0]=frame+1;d[1]+=context.called;d[2]+=context.called && context.face==UINT32_MAX;
    d[3]+=old!=rf_scene_actor_room_state.room;if(!frame)d[5]=r[1];d[6]=r[1];d[7]+=context.retries;
    for(i=0;i<36;++i)d[4]=(d[4]^((const unsigned char*)r)[i])*16777619u;
    return RF_OK;
}
static int scene_frame(void *context,uint32_t frame,rf_preview_mesh *actor)
{
    scene_stream *stream=context;uint32_t i,slot;
    profile_mark(4);
    if(stream->collision && frame==0) {
        int status=rf_scene_actor_world_check(stream->collision,rf_scene_actor_initial_world);if(status)return status;
        status=rf_scene_actor_fall_check(stream->collision,rf_scene_actor_initial_fall);if(status)return status;
        memset(&rf_scene_actor_room_state,0,sizeof(rf_scene_actor_room_state));
        memset(rf_scene_actor_room_frames,0,sizeof(rf_scene_actor_room_frames));
        memset(rf_scene_actor_room_summary,0,sizeof(rf_scene_actor_room_summary));rf_scene_actor_room_summary[4]=2166136261u;
        memset(&rf_scene_actor_pose,0,sizeof(rf_scene_actor_pose));
        rf_scene_actor_pose.radius=scene_actor_body.state.bounds.radius;
        status=rf_group_pose_set_position(&rf_scene_actor_pose,scene_actor_body.state.position);if(status)return status;
        memset(rf_scene_actor_tick_stats,0,sizeof(rf_scene_actor_tick_stats));rf_scene_actor_tick_stats[0]=0x5246544b;
        rf_scene_actor_movement_settings.response=scene_actor_body.state.coefficients[1];
        status=actor_set_speed_mode(0);if(status)return status;
        memset(rf_scene_actor_clearance_diagnostic,0,sizeof(rf_scene_actor_clearance_diagnostic));
        rf_scene_actor_stance_flags=0;memset(rf_scene_actor_stance_frames,0,sizeof(rf_scene_actor_stance_frames));
        rf_scene_actor_contact_count=0;memset(rf_scene_actor_contacts,0,sizeof(rf_scene_actor_contacts));
        memset(rf_scene_actor_landing,0,sizeof(rf_scene_actor_landing));
        rf_scene_actor_landing[0]=0x52464c44;rf_scene_actor_landing[1]=3;rf_scene_actor_landing[2]=UINT32_MAX;
    }
    uint64_t bytes=(uint64_t)stream->world*sizeof(rf_preview_vertex)+(rf_scene_actor_eye_enabled?0:actor->bytes);
    if(actor->count%3 || actor->bytes!=(uint64_t)actor->count*sizeof(rf_preview_vertex) ||
       bytes>stream->capacity)return RF_RANGE;
    for(i=0;i<actor->count;++i) {
        if(actor->vertices[i].material>=stream->bundle->count)return RF_FORMAT;
        memcpy(&slot,stream->bundle->items[actor->vertices[i].material].record.bytes+0x10,4);
        if(slot>=stream->materials->count-stream->base)return RF_FORMAT;
    }
    for(i=0;i<actor->count;++i) {
        memcpy(&slot,stream->bundle->items[actor->vertices[i].material].record.bytes+0x10,4);
        actor->vertices[i].material=stream->base+slot;
    }
    if(!rf_scene_actor_eye_enabled)memcpy(stream->mesh->vertices+stream->world,actor->vertices,actor->bytes);
    if(stream->collision) {
        uint32_t hash=2166136261u;
        int status,blocked=rf_scene_actor_stance_blocked;
        if(frame==47 && !player_poll) {status=actor_clearance_check(stream->collision);if(status)return status;}
        memcpy(rf_scene_actor_movement_frames[frame%64],&rf_scene_actor_movement_settings,12);
        rf_scene_actor_stance_frames[frame%64][0]=rf_scene_actor_stance_request;
        rf_scene_actor_stance_frames[frame%64][1]=rf_scene_actor_stance_flags;
        {uint32_t j,h=2166136261u;const unsigned char *p=(const unsigned char*)scene_actor_body.spheres.items;
         for(j=0;j<scene_actor_body.spheres.count*sizeof(*scene_actor_body.spheres.items);++j)h=(h^p[j])*16777619u;
         rf_scene_actor_stance_frames[frame%64][2]=h;}
        rf_scene_actor_stance_frames[frame%64][3]=(uint32_t)blocked;
        status=actor_room_refresh(stream->collision,frame);if(status)return status;
        status=actor_ground_check(stream->collision,frame);if(status)return status;
        {
            const actor_ground_record *ground=rf_scene_actor_ground_records+(frame%64);
            if(ground->matched && ground->hit.hit.fraction<1 && ground->hit.hit.normal[1]>=.5f) {
                rf_geometry_face face;
                status=rf_geometry_get_face(stream->geometry,ground->hit.face,&face);if(status)return status;
                if(face.texture>=stream->geometry->textures)return RF_FORMAT;
                rf_scene_actor_ground_material=stream->surface_indices[face.texture];
                rf_scene_actor_run_traction=rf_scene_actor_surface_values[rf_scene_actor_ground_material].traction;
            }
            rf_scene_actor_surface_frames[frame%64][0]=rf_scene_actor_ground_material;
            memcpy(rf_scene_actor_surface_frames[frame%64]+1,&rf_scene_actor_run_traction,4);
        }
        actor_command(frame,rf_scene_actor_input_frames[frame%64]);
        rf_scene_actor_ring_frames[frame%64]=frame;
        for(i=0;i<actor->bytes;++i)hash=(hash^((const unsigned char*)actor->vertices)[i])*16777619u;
        rf_scene_actor_render_frames[frame%64][0]=actor->count;rf_scene_actor_render_frames[frame%64][1]=hash;
        memcpy(rf_scene_actor_render_frames[frame%64]+2,scene_actor_body.state.position,12);
        if(rf_scene_actor_live_enabled) {
            uint32_t *d=rf_scene_actor_live_summary;
            if(!frame) {memset(d,0,32);d[0]=0x52464c56;d[2]=d[3]=2166136261u;}
            d[1]=frame+1;
            for(i=0;i<actor->bytes;++i)d[2]=(d[2]^((const unsigned char*)actor->vertices)[i])*16777619u;
            for(i=0;i<sizeof(scene_actor_body.state);++i)d[3]=(d[3]^((const unsigned char*)&scene_actor_body.state)[i])*16777619u;
            d[4]=rf_scene_actor_landing[1];d[5]=rf_scene_actor_landing[3];d[6]=rf_scene_actor_landing[7];d[7]=1;
        }
    }
    stream->mesh->count=stream->world+(rf_scene_actor_eye_enabled?0:actor->count);
    stream->mesh->bytes=rf_scene_actor_eye_enabled?stream->world*sizeof(rf_preview_vertex):(uint32_t)bytes;
    {
        int status;profile_mark(5);
        status=stream->sink(stream->context,frame,stream->mesh,stream->materials,stream->world);if(status)return status;
        profile_mark(6);
        if(stream->collision && frame+1<rf_scene_actor_frame_count) {
            rf_physics_body_state next=scene_actor_body.state;
            const actor_ground_record *ground=rf_scene_actor_ground_records+(frame%64);
            int walkable=ground->matched && ground->hit.hit.fraction<1 && ground->hit.hit.normal[1]>=.5f;
            int moved=0;uint32_t axis;
            if(frame)for(axis=0;axis<3;++axis) {
                float previous;memcpy(&previous,rf_scene_actor_render_frames[(frame-1)%64]+2+axis,4);
                if(previous!=next.position[axis])moved=1;
            }
            if(rf_scene_actor_landing[1]==3 && walkable) {
                status=rf_physics_static_land(&next,&ground->probe,ground->hit.hit.fraction);if(status)return status;
                rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=frame;
                ++rf_scene_actor_landing[3];rf_scene_actor_landing[5]=1;
            } else if(rf_scene_actor_landing[1]==1 && moved) {
                if(walkable) {
                    status=rf_physics_static_support(&next,&ground->probe,ground->hit.hit.fraction);if(status)return status;
                    ++rf_scene_actor_landing[6];
                } else {
                    /* Ordinary actor 4281a0: set falling flag and mode 3. */
                    next.flags|=1;rf_scene_actor_landing[1]=3;++rf_scene_actor_landing[7];
                }
            }
            status=actor_tick(stream->collision,&next,rf_scene_actor_input_frames[frame%64],ground->hit.hit.normal);if(status)return status;
            status=rf_group_pose_set_position(&rf_scene_actor_pose,next.position);if(status)return status;
            memcpy(next.position,rf_scene_actor_pose.position,12);memcpy(next.next_position,rf_scene_actor_pose.pending,12);
            memcpy(next.bounds.minimum,rf_scene_actor_pose.minimum,12);memcpy(next.bounds.maximum,rf_scene_actor_pose.maximum,12);
            scene_actor_body.state=next;
        }
        profile_mark(7);
        return RF_OK;
    }
}
static int scene_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context,int state_mode,const rf_geometry_collision_world *collision,const rf_geometry *geometry)
{
    rf_vpp archive,motions;rf_model_file model;rf_level_actor_assets binding={0};rf_entity_physics_config physics_config;
    rf_entity_state_set *states=NULL;int motions_opened=0;
    rf_animation_placement placement;rf_preview_mesh actor={0};rf_model_materials bundle={0};
    rf_preview_vertex *vertices=NULL;rf_material *items=NULL;const char *names[64];
    uint64_t bytes,count,capacity;uint32_t i;int status;scene_stream stream={0};
    if(!level || !mesh || !materials || !mesh->vertices || !materials->items ||
       mesh->count%3 || mesh->bytes!=(uint64_t)mesh->count*sizeof(*mesh->vertices) ||
       materials->allocated_bytes>=material_budget)return RF_RANGE;
    if(rf_scene_actor_eye_enabled && !actor_follow_world)return RF_RANGE;
    if(campaign_spawn && (rf_scene_showcase_enabled || !rf_scene_actor_eye_enabled ||
       !rf_scene_actor_look_enabled || !rf_scene_actor_turn_enabled || !collision || !sink))return RF_RANGE;
    if(actor_follow_world && (!sink || !collision || actor_follow_world->world!=geometry ||
        actor_follow_world->material_count!=materials->count))return RF_RANGE;
    stream.world=mesh->count;stream.base=materials->count;stream.geometry=geometry;
    if(sink && (uint64_t)mesh->bytes+1024*1024>mesh_budget)return RF_RANGE;
    status=rf_vpp_open(&archive,meshes_path);if(status)return status;
    if(campaign_spawn) {
        rf_entity_skeletal_assets *assets=malloc(sizeof(*assets));
        if(!assets){status=RF_IO;goto done;}
        status=rf_entity_skeletal_assets_load(tables_path,"miner1","",&archive,512*1024,assets);
        if(!status) {
            memset(&binding,0,sizeof(binding));binding.entity.uid=-999;strcpy(binding.entity.class_name,"miner1");
            memcpy(binding.entity.position,campaign_position,12);memcpy(binding.entity.orientation,campaign_orientation,36);
            binding.assets=assets->assets;binding.mesh=assets->mesh;
        }
        free(assets);if(status)goto done;
    } else {status=rf_level_actor_assets_load(level,uid,tables_path,&archive,512*1024,&binding);if(status)goto done;}
    if(rf_scene_showcase_enabled) {
        uint32_t i,j;
        for(i=0;i<3;++i) {
            binding.entity.position[i]=level->player_position[i]+3.8f*level->player_orientation[2][i]-1.1f*level->player_orientation[0][i];
            for(j=0;j<3;++j)binding.entity.orientation[i][j]=(i==1?1.0f:-1.0f)*level->player_orientation[i][j];
        }
        binding.entity.position[1]-=.5f;
    }
    memcpy(stream.actor_spawn,binding.entity.position,12);
    if(strcmp(binding.mesh.name,"miner.v3c")) {status=RF_FORMAT;goto done;}
    status=rf_animation_placement_from_level(level,&binding.entity,&placement);if(status)goto done;
    if(sink) {
        rf_vpp tables;status=rf_vpp_open(&tables,tables_path);if(status)goto done;
        status=rf_entity_physics_config_load(&tables,binding.entity.class_name,512*1024,&physics_config);
        if(!status && collision)status=rf_movement_descriptor_load(&tables,physics_config.authored.movement_index,65536,rf_scene_actor_movement);
        if(!status && collision)status=rf_movement_descriptor_load(&tables,3,65536,rf_scene_actor_movement+1);
        if(!status && collision)status=rf_entity_movement_load(&tables,binding.entity.class_name,512*1024,&rf_scene_actor_movement_values);
        if(!status && collision)status=scene_surface_open(&tables,&stream);
        rf_vpp_close(&tables);if(status)goto done;
        stream.eye_flags=physics_config.authored.flags2;
        /* Live landing currently implements the ordinary class-run branch.
         * Reject other descriptors/special landing classes rather than silently
         * treating them as this passive miner fixture. */
        if(collision && (physics_config.authored.movement_index!=1 ||
           !(physics_config.authored.flags&1) || (physics_config.authored.flags&0x2000000))) {status=RF_FORMAT;goto done;}
        if(collision) {
            uint32_t mode,axis;
            rf_scene_actor_movement_config.flags=physics_config.authored.flags;
            rf_scene_actor_movement_config.base_speed=rf_scene_actor_movement_values.speed;
            rf_scene_actor_movement_config.slow_factor=rf_scene_actor_movement_values.slow_factor;
            rf_scene_actor_movement_config.alternate_factor=rf_scene_actor_movement_values.fast_factor;
            rf_scene_actor_movement_config.response=rf_scene_actor_movement_values.acceleration;
            rf_scene_actor_movement_config.override_slow=rf_scene_actor_movement_config.override_normal=0;
            /* No eye/parent pose has been installed in this actor fixture yet.
             * Only body/disabled axes are valid here; the shared transform
             * supports all reference frames when their real poses are supplied. */
            for(mode=0;mode<2;++mode)for(axis=0;axis<3;++axis)
                if(rf_scene_actor_movement[mode].translation[axis]!=0 && rf_scene_actor_movement[mode].translation[axis]!=2) {status=RF_FORMAT;goto done;}
        }
        rf_physics_body_close(&scene_actor_body);memset(rf_scene_actor_physics_diagnostic,0,sizeof(rf_scene_actor_physics_diagnostic));
        placement.physics_config=&physics_config;placement.physics_body=&scene_actor_body;
        placement.physics_diagnostic=rf_scene_actor_physics_diagnostic;
        placement.initial_animation=rf_scene_actor_initial_animation;placement.animation_timing=rf_scene_actor_animation_timing;
        memset(rf_scene_actor_initial_eye_offsets,0,sizeof(rf_scene_actor_initial_eye_offsets));
        placement.initial_eye_offsets=rf_scene_actor_initial_eye_offsets;
        rf_scene_actor_initial_eye_tag=-1;placement.initial_eye_tag=&rf_scene_actor_initial_eye_tag;
        memset(rf_scene_actor_eye_frames,0,sizeof(rf_scene_actor_eye_frames));
        if(collision && state_mode) {
            placement.stance_cache=&rf_scene_actor_stance_cache;placement.stance_flags=&rf_scene_actor_stance_flags;
            placement.stance_effect=actor_selector_effect;placement.stance_context=&stream;
            placement.movement_select=actor_movement_select;placement.step_seconds=scene_step_seconds;
            rf_scene_actor_frame_count=player_poll?(player_frame_limit?player_frame_limit:UINT32_MAX):(rf_scene_actor_live_enabled?664:64);
            if(player_poll){placement.begin_frame=player_begin_frame;placement.crouch_request=&player_input.crouch;}
            placement.frame_count=rf_scene_actor_frame_count;placement.animation_timing_wrap=rf_scene_actor_live_enabled;

            memset(rf_scene_actor_locomotion_frames,0,sizeof(rf_scene_actor_locomotion_frames));
            memset(rf_scene_actor_stance_ground,0,sizeof(rf_scene_actor_stance_ground));
            memset(rf_scene_actor_stance_support,0,sizeof(rf_scene_actor_stance_support));
            rf_scene_actor_stance_flags=rf_scene_actor_stance_request=0;
            memset(rf_scene_actor_selector_frames,0,sizeof(rf_scene_actor_selector_frames));
            rf_scene_actor_movement_settings.mode=1;
        }
    }
    if(state_mode) {
        states=malloc(sizeof(*states));if(!states) {status=RF_IO;goto done;}
        status=rf_vpp_open(&motions,motions_path);if(status)goto done;motions_opened=1;
        status=rf_entity_state_set_open(tables_path,binding.entity.class_name,"",&motions,512*1024,states);if(status)goto done;
    } else {
        status=rf_animation_preview_placed(meshes_path,motions_path,&placement,0,&actor,1024*1024);if(status)goto done;
    }
    status=rf_model_file_open(&model,&archive,binding.mesh.name);if(status)goto done;
    for(i=0;i<binding.assets.texture_count;++i)names[i]=binding.assets.textures[i];
    status=rf_model_materials_open_skin(&bundle,&model,names,binding.assets.texture_count,maps,map_count,
        material_budget-materials->allocated_bytes);if(status)goto done;
    bytes=((uint64_t)mesh->count+actor.count)*sizeof(*vertices);
    capacity=sink?(actor_follow_world?RF_SCENE_FOLLOW_CAPACITY:(uint64_t)mesh->bytes+1024*1024):bytes;
    count=(uint64_t)materials->count+bundle.textures.count;
    if(capacity>mesh_budget || capacity>SIZE_MAX || bytes>capacity || count>256) {status=RF_RANGE;goto done;}
    for(i=0;i<actor.count;++i) {
        uint32_t material=actor.vertices[i].material,slot;
        if(material>=bundle.count) {status=RF_FORMAT;goto done;}
        memcpy(&slot,bundle.items[material].record.bytes+0x10,4);
        if(slot>=bundle.textures.count) {status=RF_FORMAT;goto done;}
        actor.vertices[i].material=materials->count+slot;
    }
    vertices=malloc((size_t)capacity);items=malloc((size_t)count*sizeof(*items));
    if(!vertices || !items) {status=RF_IO;goto done;}
    memcpy(vertices,mesh->vertices,mesh->bytes);if(actor.bytes)memcpy(vertices+mesh->count,actor.vertices,actor.bytes);
    memcpy(items,materials->items,materials->count*sizeof(*items));
    memcpy(items+materials->count,bundle.textures.items,bundle.textures.count*sizeof(*items));
    free(mesh->vertices);free(materials->items);
    mesh->vertices=vertices;vertices=NULL;mesh->count+=actor.count;mesh->bytes=(uint32_t)bytes;
    materials->items=items;items=NULL;materials->count=(uint32_t)count;
    materials->allocated_bytes+=bundle.textures.allocated_bytes;
    materials->loaded+=bundle.textures.loaded;materials->missing+=bundle.textures.missing;
    /* Image ownership moved to combined materials, instance records stay local. */
    free(bundle.textures.items);memset(&bundle.textures,0,sizeof(bundle.textures));
    if(sink) {
        rf_preview_close(&actor);
        stream.mesh=mesh;stream.materials=materials;stream.bundle=&bundle;
        if(actor_follow_world) {placement.prepare_view=actor_follow_view;placement.view_context=&stream;}
        stream.capacity=(uint32_t)capacity;stream.sink=sink;stream.context=context;stream.collision=collision;
        if(state_mode)status=rf_animation_stream_states(meshes_path,motions_path,1024*1024,&placement,states,scene_frame,&stream);
        else status=rf_animation_stream_placed(meshes_path,motions_path,1024*1024,&placement,scene_frame,&stream);
        if(!status && collision && rf_scene_actor_route_enabled && !rf_scene_actor_live_enabled)status=actor_routes(&stream);
    }
done:
    free(stream.surface_indices);free(states);if(motions_opened)rf_vpp_close(&motions);
    free(vertices);free(items);rf_preview_close(&actor);rf_model_materials_close(&bundle);
    rf_vpp_close(&archive);return status;
}
int rf_scene_preview_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget)
{
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,NULL,NULL,0,NULL,NULL);
}
int rf_scene_stream_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context)
{
    if(!sink)return RF_RANGE;
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,sink,context,0,NULL,NULL);
}
int rf_scene_stream_miner_states(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context)
{
    if(!sink)return RF_RANGE;
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,sink,context,1,NULL,NULL);
}
int rf_scene_stream_miner_body(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context,const rf_geometry_collision_world *collision,const rf_geometry *geometry)
{
    if(!sink || !collision || !geometry)return RF_RANGE;
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,sink,context,1,collision,geometry);
}
