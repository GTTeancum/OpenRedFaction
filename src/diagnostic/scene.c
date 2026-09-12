#include "rf/eye.h"
#include "rf/scene_preview.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include "rf/corpse_effect.h"
#include "rf/player.h"
#include "rf/event.h"
#include "rf/audio.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "rf/visibility.h"
#include "rf/level_particles.h"
/* Explicit replay setup, not an authored player-start reconstruction. */
int rf_scene_stage_door(rf_level *level)
{
    rf_geometry_movers movers={0};const rf_geometry_mover *a=NULL,*b=NULL;
    float position[3],matrix[3][3]={{0}};uint32_t i;int status;
    if(!level)return RF_RANGE;
    status=rf_geometry_movers_open(level,1024*1024,&movers);if(status)return status;
    for(i=0;i<movers.count;i++) {
        if(movers.items[i].uid==8544)a=movers.items+i;
        if(movers.items[i].uid==8543)b=movers.items+i;
    }
    if(!a || !b){rf_geometry_movers_close(&movers);return RF_NOT_FOUND;}
    /* The local forward axis is the authored sliding direction, not the
     * doorway normal. Rotate it about world up to approach from the hall. */
    matrix[2][0]=a->orientation[2][2];matrix[2][2]=-a->orientation[2][0];
    for(i=0;i<3;i++)position[i]=(a->position[i]+b->position[i])*.5f-matrix[2][i]*3;
    position[1]+=.625f;matrix[1][1]=1;matrix[0][0]=matrix[2][2];matrix[0][2]=-matrix[2][0];
    memcpy(level->player_position,position,12);memcpy(level->player_orientation,matrix,36);
    rf_geometry_movers_close(&movers);return RF_OK;
}
int rf_scene_stage_lift(rf_level *level)
{
    rf_geometry_movers movers={0};uint32_t i;int status;
    if(!level)return RF_RANGE;
    status=rf_geometry_movers_open(level,1024*1024,&movers);if(status)return status;
    status=RF_NOT_FOUND;
    for(i=0;i<movers.count;i++)if(movers.items[i].uid==8670) {
        memcpy(level->player_position,movers.items[i].position,12);
        level->player_position[0]-=.5f;
        level->player_position[1]+=.625f;
        memset(level->player_orientation,0,36);
        level->player_orientation[0][0]=level->player_orientation[1][1]=level->player_orientation[2][2]=1;
        status=RF_OK;break;
    }
    rf_geometry_movers_close(&movers);return status;
}
int rf_scene_stage_force(rf_level *level,uint32_t uid)
{
    rf_level_force_reader reader;rf_level_force_region record;int status;
    if(!level)return RF_RANGE;
    status=rf_level_forces_begin(level,&reader);if(status)return status;
    while((status=rf_level_force_next(&reader,&record))==RF_OK)if(record.uid==uid) {
        memcpy(level->player_position,record.position,12);level->player_position[0]+=.125f;
        memset(level->player_orientation,0,36);
        level->player_orientation[0][0]=level->player_orientation[1][1]=level->player_orientation[2][2]=1;
        return RF_OK;
    }
    return status;
}
int rf_scene_stage_climb(rf_level *level,uint32_t mode)
{
    rf_level_entity_reader reader;rf_player_movement_region region;float position[3];uint32_t i;int status;
    if(!level || (mode!=1 && mode!=2))return RF_RANGE;
    status=rf_level_regions_begin(level,&reader);if(status)return status;
    status=rf_level_region_next(&reader,&region);if(status)return status;
    memcpy(position,region.center,12);
    if(mode==2)for(i=0;i<3;++i) {
        float side=(float)(-region.matrix[2][i]*(region.size[2]*.5f+1.0f));
        float up=(float)(region.matrix[1][i]*(1.0f-region.size[1]*.5f));
        position[i]=(float)((double)position[i]+side+up);
    }
    memcpy(level->player_position,position,12);return RF_OK;
}
int rf_scene_replay_header(FILE *file,uint32_t *count,uint32_t *record_size)
{
    long bytes,offset=0;uint32_t header[2],size=24;
    if(!file || !count || !record_size)return RF_RANGE;
    if(fseek(file,0,SEEK_END) || (bytes=ftell(file))<=0 || fseek(file,0,SEEK_SET))return RF_FORMAT;
    if(fread(header,4,1,file)!=1)return RF_FORMAT;
    if(header[0]==0x32494652u || header[0]==0x33494652u) {
        size=header[0]==0x32494652u?28:32;
        if(fread(header+1,4,1,file)!=1 || header[1]!=size)return RF_FORMAT;
        offset=8;
    }
    if(bytes<=offset || (bytes-offset)%size || (bytes-offset)/size>60000 || fseek(file,offset,SEEK_SET))return RF_FORMAT;
    *count=(uint32_t)(bytes-offset)/size;*record_size=size;return RF_OK;
}
static rf_scene_input_poll player_poll;
static void *player_context;
static uint32_t player_frame_limit;
static rf_scene_input player_input;
static uint32_t campaign_spawn;
static uint32_t campaign_crouched,campaign_jump_held;
static rf_physics_gravity scene_gravity={9.8f,{0,-9.8f,0}};
uint32_t rf_scene_player_jump[4],rf_scene_player_jump_frames[128][8];
static float campaign_position[3],campaign_orientation[9];
uint32_t rf_scene_player_spawn_diagnostic[19];
int rf_scene_set_campaign_spawn(const rf_level *level)
{
    unsigned i,j;
    /* Original level setup 435aeb resets gravity independently of jump strength. */
    rf_physics_gravity_set(&scene_gravity,9.8f);
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
    if(value.crouch>1 || value.jump>1 || value.use>1)return RF_FORMAT;
    player_input=value;r[0]=frame;memcpy(r+1,&value,24); /* Preserve the legacy movement/stance ring. */
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
    rf_scene_profile_stage[1]=10;
    status=rf_geometry_movers_open(level,1024*1024,&movers);
    if(status==RF_NOT_FOUND) {movers.allocated_bytes=sizeof(movers);status=RF_OK;}
    if(status)goto done;
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
    rf_scene_profile_stage[1]=11;
    status=rf_geometry_materials_open(&bundle,sources,movers.count+1,maps,map_count,material_budget);
    if(!status){rf_scene_profile_stage[1]=12;status=rf_preview_build_world(mesh,world,&movers,NULL,&bundle,level,mesh_budget);}
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
typedef struct scene_particle_workspace {
    rf_render_queue_record records[2048];rf_render_sphere spheres[2048];
    uint32_t order[2048];float distances[2048];
} scene_particle_workspace;
typedef struct scene_stream {
    rf_preview_mesh *mesh;rf_materials *materials;const rf_model_materials *bundle;
    uint32_t world,base,capacity,npc_base,npc_textures;rf_scene_frame_sink sink;void *context;
    rf_model_projection npc_view;void *npc_memory;uint16_t *npc_indices;rf_model_clip_pool *npc_pool;
    const rf_geometry_collision_world *collision;
    const rf_geometry *geometry;unsigned char *surface_indices;float actor_spawn[3];uint32_t eye_flags;
    rf_level_visibility visibility;
    rf_level_particles particles;rf_level_particle_tick_result particle_first;
    rf_visibility_camera particle_camera;scene_particle_workspace *particle_workspace;uint32_t particle_frame;
} scene_stream;
static scene_stream *particle_draw_stream;
uint32_t rf_scene_particle_draw_summary[7],rf_scene_particle_draw_frames[64][6];
static int scene_particle_draw_one(scene_stream *stream,uint32_t index,rf_scene_particle_sink sink,void *context,uint32_t row[6])
{
    rf_particle_screen_polygon polygon;rf_particle_draw_vertex vertices[12];rf_particle_vertex_environment environment={0};
    rf_particle_render_environment render_environment={1,1,0,2};rf_particle_render_states states={0};
    const rf_particle *p;const rf_particle_animation *animation;const rf_image *image;uint32_t frame,mode,i,j;int status;
    if(index>=RF_PARTICLE_CAPACITY)return RF_RANGE;
    p=stream->particles.state->records+index;++row[2];
    if(p->bitmap>=stream->particles.materials.texture_count)return RF_RANGE;
    animation=&stream->particles.materials.textures[p->bitmap].animation;
    status=rf_particle_frame_index(p,&frame);if(status)return status;
    if(frame>=animation->count)return RF_RANGE;
    image=animation->images+frame;
    mode=rf_particle_render_mode(p->flags,RF_PARTICLE_NORMAL_MODE,RF_PARTICLE_GLOW_MODE);
    if(p->flags&0x4000u)status=rf_particle_world_stretch(&stream->particle_camera,p->position,p->previous_position,
        p->radius,image->width,image->height,&polygon);
    else status=rf_particle_world_billboard(&stream->particle_camera,p->position,p->orientation,p->radius,
        image->width,image->height,&polygon);
    if(status)return status;
    if(!polygon.count)return RF_OK;
    status=rf_particle_render_decode(mode,&render_environment,&states);if(status)return status;
    environment.rgba=p->color_current;environment.vertex_color=states.vertex_color;environment.vertex_alpha=states.vertex_alpha;
    environment.depth_scale=environment.reciprocal_scale=stream->particle_camera.view.scale[2];
    environment.uv_scale[0]=environment.uv_scale[1]=1;
    for(i=0;i<polygon.count;i++) {
        const unsigned char *bytes=(const unsigned char*)(vertices+i);
        status=rf_particle_vertex_encode(&environment,polygon.vertices+i,vertices+i);if(status)return status;
        for(j=0;j<sizeof(*vertices);j++)row[5]=(row[5]^bytes[j])*16777619u;
    }
    row[5]=(row[5]^mode)*16777619u;row[5]=(row[5]^p->bitmap)*16777619u;
    if(sink){status=sink(context,vertices,polygon.count,image,mode);if(status)return status;}
    ++row[3];row[4]+=polygon.count;return RF_OK;
}
int rf_scene_draw_particles(rf_scene_particle_sink sink,void *context)
{
    scene_stream *stream=particle_draw_stream;scene_particle_workspace *workspace;
    uint32_t row[6]={0,0,0,0,0,2166136261u},room,i,j;int status;
    if(!stream || !stream->particle_workspace)return RF_OK;
    workspace=stream->particle_workspace;row[0]=stream->particle_frame;
    for(room=0;room<stream->visibility.state.visible_count;room++) {
        uint32_t count=0;
        status=rf_level_particles_queue_room(&stream->particles,stream->visibility.state.order[room]+1,
            &stream->particle_camera.frustum,NULL,NULL,workspace->records,2048,&count);if(status)return status;
        row[1]+=count;
        for(i=0;i<count;i++) {
            memcpy(workspace->spheres[i].position,workspace->records[i].position,12);
            workspace->spheres[i].radius=workspace->records[i].radius;workspace->spheres[i].sorted=workspace->records[i].sorted;
        }
        status=rf_render_sphere_order(workspace->spheres,count,stream->particle_camera.view.origin,workspace->order,workspace->distances);if(status)return status;
        for(i=0;i<count;i++) {
            const rf_render_queue_record *entry=workspace->records+workspace->order[i];
            if(entry->callback==RF_LEVEL_PARTICLE_DRAW_SINGLE) {
                status=scene_particle_draw_one(stream,entry->object,sink,context,row);if(status)return status;
            } else if(entry->callback==RF_LEVEL_PARTICLE_DRAW_EMITTER) {
                uint32_t list=RF_PARTICLE_BASE_LISTS+entry->object,visited=0;
                if(entry->object>=RF_PARTICLE_EMITTER_CAPACITY)return RF_RANGE;
                for(j=stream->particles.state->lists[list].next;j!=RF_PARTICLE_CAPACITY+list;j=stream->particles.state->records[j].next) {
                    if(j>=RF_PARTICLE_CAPACITY || ++visited>RF_PARTICLE_CAPACITY)return RF_RANGE;
                    status=scene_particle_draw_one(stream,j,sink,context,row);if(status)return status;
                }
            } else return RF_RANGE;
        }
    }
    memcpy(rf_scene_particle_draw_frames[row[0]%64],row,sizeof(row));
    ++rf_scene_particle_draw_summary[0];for(i=1;i<5;i++)rf_scene_particle_draw_summary[i]+=row[i];
    rf_scene_particle_draw_summary[5]=(rf_scene_particle_draw_summary[5]^row[5])*16777619u;return RF_OK;
}
uint32_t rf_scene_visibility_summary[6],rf_scene_visibility_frames[64][17];
uint32_t rf_scene_particles_summary[8],rf_scene_particles_frames[64][12];
uint32_t rf_scene_particle_view_enabled;
uint32_t rf_scene_particle_view_back;
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
static rf_level_owned_regions campaign_regions;
static rf_physics_force_collection campaign_forces;
static uint32_t campaign_force_class_flags,campaign_force_class_kind;
static float campaign_force_air_limit;
static rf_camera_effect_state campaign_camera_effect;
static rf_screen_flash campaign_player_flash;
typedef struct campaign_player_damage_owner {
    rf_entity_damage_state state;
    float factors[11];uint32_t class_flags,object_flags;
} campaign_player_damage_owner;
static campaign_player_damage_owner campaign_player_damage;
static struct {rf_entity_eye_limits eye_limits;float model_radius;} campaign_player_geometry;
static struct {int32_t groups[3],deadline,voice;} campaign_player_pain_sound;
uint32_t rf_scene_player_pain_audio[9]; /* same fields as NPC pain audio */
uint32_t rf_scene_player_vitals[6]; /* health/armor/class health/class armor bits, owner bytes, factor hash */
static int campaign_player_damage_open(rf_vpp *tables,const char *name,const rf_entity_class_physics *physics)
{
    campaign_player_damage_owner owner={0};rf_entity_creation_vitals_class cls;
    rf_entity_creation_vitals_state vitals={0};rf_vpp_entry entry;void *text=NULL;int status;uint32_t i,hash=2166136261u;
    status=rf_vpp_find(tables,"entity.tbl",&entry);if(status)return status;
    if(!entry.size || entry.size>512*1024)return RF_RANGE;
    text=malloc(entry.size);if(!text)return RF_RANGE;
    status=rf_vpp_read(tables,&entry,0,text,entry.size);
    if(!status)status=rf_entity_vitals_config_read(text,entry.size,name,&cls);
    if(!status)status=rf_entity_damage_factors_read(text,entry.size,name,owner.factors);
    if(!status)status=rf_entity_eye_limits_read(text,entry.size,name,&campaign_player_geometry.eye_limits);
    free(text);if(status)return status;
    vitals.object_flags=8;rf_entity_creation_vitals(&vitals,&cls,0);owner.object_flags=vitals.object_flags;
    owner.state.effects.health=vitals.health;owner.state.effects.armor=vitals.armor;
    owner.state.effects.class_health=cls.health;owner.state.effects.class_armor=cls.armor;
    owner.state.effects.class_flags_728=physics->flags2;owner.class_flags=physics->flags;
    owner.state.effects.voice=UINT32_MAX;owner.state.responsible_handle=UINT32_MAX;owner.state.burn_source=UINT32_MAX;
    campaign_player_damage=owner;
    campaign_player_pain_sound.groups[0]=campaign_player_pain_sound.groups[1]=campaign_player_pain_sound.groups[2]=-1;
    campaign_player_pain_sound.voice=-1;
    status=rf_timer_set(&campaign_player_pain_sound.deadline,0,0);if(status)return status;
    memset(rf_scene_player_pain_audio,0,sizeof(rf_scene_player_pain_audio));
    memcpy(rf_scene_player_vitals,&owner.state.effects.health,16);rf_scene_player_vitals[4]=sizeof(owner);
    for(i=0;i<sizeof(owner.factors);++i)hash=(hash^((const unsigned char *)owner.factors)[i])*16777619u;
    rf_scene_player_vitals[5]=hash;return RF_OK;
}
uint32_t rf_scene_force_ticks[12]; /* ticks, matches, eligible, carry, replace, turbulence, shakes, sounds, UID, RNG, cap, status */
uint32_t rf_scene_campaign_forces[3]; /* count, owned bytes, ordered runtime record hash */
uint32_t rf_scene_force_state[3]; /* current count, enabled count, full record hash */
static void campaign_force_snapshot(void)
{
    uint32_t i,hash=2166136261u,enabled=0;
    for(i=0;i<campaign_forces.count;++i)if(campaign_forces.items[i].active&255u)++enabled;
    for(i=0;i<campaign_forces.count*sizeof(*campaign_forces.items);++i)
        hash=(hash^((const unsigned char *)campaign_forces.items)[i])*16777619u;
    rf_scene_force_state[0]=campaign_forces.count;rf_scene_force_state[1]=enabled;rf_scene_force_state[2]=hash;
}
static rf_object_registry campaign_registry;
static rf_runtime_events campaign_events;
static rf_level_owned_ambient campaign_ambient;
static rf_ambient_instances campaign_ambient_instances;
static rf_ambient_slot campaign_ambient_slots[RF_AMBIENT_SLOTS];
static uint32_t campaign_ambient_frame;
uint32_t rf_scene_ambient_schedule[6]; /* ticks, clock, occupied slots, pending timers, instance/table hashes */
uint32_t rf_scene_ambient_records[3]; /* authored count, owner bytes, ordered record hash */
uint32_t rf_scene_ambient_instances[4]; /* registered, rejected, owner bytes, ordered state hash */
uint32_t rf_scene_switch_state[3]; /* count, enabled count, ordered persistent state hash */
static void campaign_switch_snapshot(void)
{
    uint32_t i,j,count=0,enabled=0,hash=2166136261u;
    for(i=0;i<campaign_events.count;++i)if(campaign_events.items[i].switch_state) {
        const rf_switch_state *state=campaign_events.items[i].switch_state;
        ++count;if(!state->disabled)++enabled;
        for(j=0;j<sizeof(*state);++j)hash=(hash^((const unsigned char *)state)[j])*16777619u;
    }
    rf_scene_switch_state[0]=count;rf_scene_switch_state[1]=enabled;rf_scene_switch_state[2]=hash;
}

static rf_runtime_triggers campaign_triggers;
static rf_level_owned_groups campaign_groups;
static rf_group_runtime_collection campaign_group_runtime;
static rf_group_registration campaign_group_registration;
static rf_geometry_collision_movers campaign_movers;
static rf_group_registered_mover *campaign_mover_wrappers;
static rf_level_uid_object *campaign_mover_objects;
static rf_group_object *campaign_mover_bindings;
static rf_group_mover_memberships campaign_memberships;
uint32_t rf_scene_campaign_memberships[5]; /* groups, links, retained/peak bytes, ordered binding hash */

static uint32_t campaign_mover_count;
static rf_entity_registry campaign_entities;
static rf_entity_seeds campaign_seeds;
static rf_entity_skeletons campaign_skeletons;
static rf_entity_poses campaign_poses;
static rf_entity_base_motions campaign_base_motions;
static rf_entity_motion_catalog campaign_motion_catalog;
static rf_entity_playback_resources campaign_playback_resources;
static rf_entity_render_models campaign_render_models;
static rf_entity_collision_models campaign_collision_models;
static float (*campaign_model_query_scratch)[3];
uint32_t rf_scene_npc_pose_demand[5]; /* queries, evaluations, errors, fixture cases/errors */
static int campaign_model_pose_ready(uint32_t slot,rf_entity_pose *pose);
uint32_t rf_scene_npc_model_queries[7]; /* models, owned bytes, scratch bytes, queries, hits, hash, errors */
typedef struct campaign_model_owner {
    rf_model_skeletal_registration registration;rf_entity_pose *pose;
    float position[3],basis[9];uint32_t appearance,room;rf_entity_owned_pose *owned;
} campaign_model_owner;
static campaign_model_owner *campaign_model_owners;
static rf_entity_collision_cache *campaign_collision_caches;
uint32_t rf_scene_npc_collision_cache[6]; /* created, peak bytes, refreshes, hash, retired, errors */
static rf_model_skeletal_registration *campaign_model_head;
static uint32_t campaign_model_owner_count;
static uint32_t campaign_model_owned_bytes,campaign_model_owned_count;
uint32_t rf_scene_npc_models[4]; /* published, peak owner bytes, retired, errors */
/* Retire one model without releasing the registry table or shared textures.
 * Empty slots are repeatable; failed preflight preserves the loaded owner. */
int rf_scene_model_retire(uint32_t slot)
{
    campaign_model_owner *owner;rf_entity_pose *pose;rf_entity_playback_model *model;uint32_t bytes=0;int status;
    if(slot>=campaign_model_owner_count || !campaign_model_owners)return RF_RANGE;
    owner=campaign_model_owners+slot;pose=owner->pose;
    if(!owner->registration.loaded)return (!pose && !owner->owned)?RF_OK:RF_RANGE;
    if(!pose || !campaign_playback_resources.models || pose->skeleton>=campaign_playback_resources.model_count ||
       owner->registration.active!=&pose->playback.completion.active)return RF_RANGE;
    if(owner->owned) {
        uint32_t override_bytes=pose->overrides?pose->bone_count*sizeof(*pose->overrides):0;
        bytes=owner->owned->allocated_bytes;
        if(pose!=&owner->owned->pose || !pose->bone_count || pose->bone_count>50 ||
           !owner->owned->storage || pose->matrices!=owner->owned->storage ||
           (pose->overrides && (void*)pose->overrides!=(unsigned char*)owner->owned->storage+pose->bone_count*48) ||
           (void*)pose->generations!=(unsigned char*)owner->owned->storage+pose->bone_count*48+override_bytes ||
           bytes!=sizeof(*owner->owned)+pose->bone_count*50+override_bytes || !campaign_model_owned_count ||
           bytes>campaign_model_owned_bytes)return RF_RANGE;
    }
    model=campaign_playback_resources.models+pose->skeleton;
    status=rf_model_skeletal_retire(&owner->registration,&campaign_model_head,campaign_model_owner_count,model->resources,model->count);
    if(status)return status;
    if(owner->owned) {
        status=rf_entity_owned_pose_close(owner->owned,&campaign_playback_resources);if(status)return status;
        free(owner->owned);owner->owned=NULL;campaign_model_owned_bytes-=bytes;--campaign_model_owned_count;
    }
    if(campaign_collision_caches && campaign_collision_caches[slot].matrices) {
        rf_entity_collision_cache_close(campaign_collision_caches+slot);++rf_scene_npc_collision_cache[4];
    }
    owner->registration.loaded=0;owner->registration.active=NULL;owner->pose=NULL;
    ++rf_scene_npc_models[2];return RF_OK;
}
void campaign_models_close(void)
{
    uint32_t i;for(i=0;i<campaign_model_owner_count;++i)
        if(rf_scene_model_retire(i))++rf_scene_npc_models[3];
    if(campaign_collision_caches)for(i=0;i<campaign_model_owner_count;++i)rf_entity_collision_cache_close(campaign_collision_caches+i);
    free(campaign_collision_caches);campaign_collision_caches=NULL;
    free(campaign_model_owners);campaign_model_owners=NULL;campaign_model_owner_count=0;campaign_model_head=NULL;
}
static int campaign_models_open(void)
{
    uint32_t i;uint64_t bytes=(uint64_t)campaign_poses.count*sizeof(*campaign_model_owners),cache_bytes=(uint64_t)campaign_poses.count*sizeof(*campaign_collision_caches);int status=RF_OK;
    if(campaign_model_owners || campaign_model_head || campaign_collision_caches)return RF_RANGE;
    for(i=0;i<campaign_poses.count;++i)if(campaign_poses.items[i].skeleton!=UINT32_MAX) {
        if(!campaign_poses.items[i].bone_count || campaign_poses.items[i].bone_count>50)return RF_RANGE;
        cache_bytes+=(uint64_t)campaign_poses.items[i].bone_count*50;
    }
    if(cache_bytes>256*1024)return RF_RANGE;
    memset(rf_scene_npc_collision_cache,0,sizeof(rf_scene_npc_collision_cache));rf_scene_npc_collision_cache[3]=2166136261u;
    memset(rf_scene_npc_models,0,sizeof(rf_scene_npc_models));
    if(bytes>32*1024)return RF_RANGE;if(!campaign_poses.count)return RF_OK;
    campaign_model_owners=calloc(campaign_poses.count,sizeof(*campaign_model_owners));if(!campaign_model_owners)return RF_RANGE;
    campaign_model_owner_count=campaign_poses.count;rf_scene_npc_models[1]=(uint32_t)bytes;
    campaign_collision_caches=calloc(campaign_poses.count,sizeof(*campaign_collision_caches));
    if(!campaign_collision_caches){campaign_models_close();return RF_IO;}
    rf_scene_npc_collision_cache[1]=(uint32_t)cache_bytes;
    for(i=0;i<campaign_model_owner_count;++i) {
        campaign_model_owner *owner=campaign_model_owners+i;rf_entity_pose *pose=campaign_poses.items+i;
        if(pose->skeleton==UINT32_MAX)continue;
        if(!campaign_playback_resources.models || pose->skeleton>=campaign_playback_resources.model_count){status=RF_RANGE;break;}
        owner->room=UINT32_MAX;owner->appearance=UINT32_MAX;
        owner->pose=pose;owner->registration.loaded=1;owner->registration.active=&pose->playback.completion.active;
        status=rf_model_skeletal_register(&owner->registration,&campaign_model_head,campaign_model_owner_count);if(status)break;
        ++rf_scene_npc_models[0];
        status=rf_entity_collision_cache_open(pose,(uint32_t)sizeof(*campaign_collision_caches)+pose->bone_count*50,campaign_collision_caches+i);if(status)break;
        ++rf_scene_npc_collision_cache[0];
    }
    if(status)campaign_models_close();return status;
}

/* Runtime consumers resolve the published model pose, not its original level
 * array slot. Empty authored slots have no model; inconsistent owners fail. */
static int campaign_model_pose(uint32_t slot,rf_entity_pose **result)
{
    campaign_model_owner *owner;
    if(!result || slot>=campaign_model_owner_count || !campaign_model_owners)return RF_RANGE;
    owner=campaign_model_owners+slot;*result=NULL;
    if(!owner->registration.loaded)return RF_OK;
    if(!owner->pose || owner->pose->skeleton==UINT32_MAX || !owner->registration.next || !owner->registration.previous ||
       owner->registration.active!=&owner->pose->playback.completion.active)return RF_RANGE;
    *result=owner->pose;return RF_OK;
}
int rf_scene_model_collision_query(uint32_t slot,rf_collision_model_part_query *query,
    rf_collision_model_response_hit *hit,uint32_t reset,uint32_t *accepted)
{
    rf_entity_pose *pose;rf_collision_model_skin_pose view;const rf_model_skin_geometry *geometry;uint32_t i;int status;
    if(!query || !hit || !accepted)return RF_RANGE;
    status=campaign_model_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    if(!campaign_collision_caches || !campaign_collision_models.items || !campaign_render_models.items ||
        pose->skeleton>=campaign_collision_models.count || pose->skeleton>=campaign_render_models.count)return RF_RANGE;
    status=campaign_model_pose_ready(slot,pose);if(status)return status;
    status=rf_entity_collision_cache_view(campaign_collision_caches+slot,pose,campaign_render_models.items[pose->skeleton].stored,&view);
    if(status)return status;geometry=campaign_collision_models.items+pose->skeleton;
    status=rf_collision_model_skinning_query(geometry->batches,geometry->batch_count,&view,query,hit,campaign_model_query_scratch,reset,accepted);
    if(status){++rf_scene_npc_model_queries[6];return status;}
    ++rf_scene_npc_model_queries[3];rf_scene_npc_model_queries[4]+=*accepted;
    rf_scene_npc_model_queries[5]=(rf_scene_npc_model_queries[5]^*accepted)*16777619u;
    /* Misses may retain an original caller's otherwise uninitialized hit. */
    if(*accepted)for(i=0;i<sizeof(*hit);++i)rf_scene_npc_model_queries[5]=(rf_scene_npc_model_queries[5]^((const unsigned char *)hit)[i])*16777619u;
    return RF_OK;
}
static int campaign_model_geometry_open(void)
{
    uint64_t bytes;int status;
    if(campaign_model_query_scratch)return RF_RANGE;
    status=rf_entity_collision_models_open(&campaign_render_models,1024*1024,&campaign_collision_models);if(status)return status;
    bytes=(uint64_t)campaign_collision_models.max_vertices*12;
    if(bytes+campaign_collision_models.resident_bytes>1024*1024){status=RF_RANGE;goto fail;}
    campaign_model_query_scratch=bytes?malloc((size_t)bytes):NULL;
    if(bytes && !campaign_model_query_scratch){status=RF_IO;goto fail;}
    memset(rf_scene_npc_pose_demand,0,sizeof(rf_scene_npc_pose_demand));
    memset(rf_scene_npc_model_queries,0,sizeof(rf_scene_npc_model_queries));
    rf_scene_npc_model_queries[0]=campaign_collision_models.count;rf_scene_npc_model_queries[1]=campaign_collision_models.resident_bytes;
    rf_scene_npc_model_queries[2]=(uint32_t)bytes;rf_scene_npc_model_queries[5]=2166136261u;return RF_OK;
 fail:
    rf_entity_collision_models_close(&campaign_collision_models);return status;
}
static int campaign_model_collision_refresh(uint32_t slot)
{
    rf_entity_pose *pose;rf_collision_model_skin_pose view;uint32_t i,n;int status=campaign_model_pose(slot,&pose);
    if(status)return status;
    if(!pose || !campaign_collision_caches || !campaign_render_models.items || pose->skeleton>=campaign_render_models.count)return RF_RANGE;
    if(campaign_render_models.items[pose->skeleton].bone_count!=pose->bone_count)return RF_RANGE;
    status=rf_entity_collision_cache_view(campaign_collision_caches+slot,pose,campaign_render_models.items[pose->skeleton].stored,&view);
    if(!status)status=rf_model_prepare_skinning(view.stored,view.evaluated,view.bone_count,view.generation,view.prepared,view.generations,view.capacity);
    if(status){++rf_scene_npc_collision_cache[5];return status;}
    ++rf_scene_npc_collision_cache[2];n=pose->bone_count*50;
    for(i=0;i<n;++i)rf_scene_npc_collision_cache[3]=(rf_scene_npc_collision_cache[3]^((unsigned char*)campaign_collision_caches[slot].matrices)[i])*16777619u;
    return RF_OK;
}
int rf_scene_model_clear_bone_override(uint32_t slot,uint32_t bone)
{
    rf_entity_pose *pose;int status=campaign_model_pose(slot,&pose);
    if(status)return status;if(!pose)return RF_NOT_FOUND;
    /* Original effective index-1 addresses138c: slot15's tick low byte. */
    if(bone==UINT32_MAX) {
        uint32_t bits;memcpy(&bits,&pose->playback.completion.active.slots[15].tick,4);
        bits&=0xffffff00u;memcpy(&pose->playback.completion.active.slots[15].tick,&bits,4);return RF_OK;
    }
    if(bone>=pose->bone_count)return RF_RANGE;
    if(!pose->overrides)return RF_NOT_FOUND;
    pose->overrides[bone].enabled=0;return RF_OK;
}
int rf_scene_model_stop_nonlooping(uint32_t slot)
{
    rf_entity_pose *pose;rf_entity_playback_model *model;int status;
    status=campaign_model_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    if(!campaign_playback_resources.models || pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+pose->skeleton;
    return rf_motion_stop_nonlooping(&pose->playback,model->resources,model->count);
}

/* Copy placement so the model does not borrow its actor's render metadata.
 * A corpse can later publish its own transform through this same boundary. */
static int campaign_model_place(uint32_t slot,const float position[3],const float basis[9],uint32_t appearance,uint32_t room)
{
    rf_entity_pose *pose;campaign_model_owner *owner;uint32_t i;int status;
    if(!position || !basis)return RF_RANGE;
    status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose)return RF_OK;
    for(i=0;i<3;++i)if(!isfinite(position[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_RANGE;
    owner=campaign_model_owners+slot;
    memcpy(owner->position,position,12);memcpy(owner->basis,basis,36);
    owner->appearance=appearance;owner->room=room;return RF_OK;
}
/* Bounded campaign ownership handoff. The caller must have accepted corpse
 * placement before this operation. No corpse/actor state is published here. */
int rf_scene_model_detach(uint32_t slot,const float position[3],const float basis[9],uint32_t room)
{
    rf_entity_pose *pose;rf_entity_owned_pose *owned;campaign_model_owner *owner;uint32_t i;int status;
    if(!position || !basis)return RF_RANGE;
    status=campaign_model_pose(slot,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    owner=campaign_model_owners+slot;
    if(owner->owned || campaign_model_owned_count>=RF_CORPSE_CAPACITY || campaign_model_owned_bytes>96*1024)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(position[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_RANGE;
    owned=calloc(1,sizeof(*owned));if(!owned)return RF_RANGE;
    status=rf_entity_registered_pose_take(&owner->registration,&owner->pose,&campaign_playback_resources,
        96*1024-campaign_model_owned_bytes,owned);
    if(status){free(owned);return status;}
    owner->owned=owned;campaign_model_owned_bytes+=owned->allocated_bytes;++campaign_model_owned_count;
    memcpy(owner->position,position,12);memcpy(owner->basis,basis,36);owner->room=room;
    return RF_OK;
}
/* Model tokens for this bridge are stable registry slot+1; zero means absent.
 * Context points to the accepted corpse room. Replacement loading is separate. */
int rf_scene_corpse_bind_model(void *context,rf_corpse_create_source *source,rf_corpse *corpse)
{
    if(!context || !source || !corpse)return RF_RANGE;
    if(!corpse->update.model)return RF_OK;
    if(corpse->update.model!=source->model || source->model_kind!=2 ||
       (source->replacement_model && source->replacement_model[0]))return RF_FORMAT;
    return rf_scene_model_detach(corpse->update.model-1,corpse->update.position,corpse->update.basis,*(const uint32_t*)context);
}
/* Current campaign startup uses base action mappings. Weapon remapping must
 * supply its original declaration view before enabling this callback there. */
int32_t rf_scene_corpse_motion(void *context,rf_corpse_create_source *source,const char *name)
{
    uint32_t cls;(void)context;if(!source)return -2;
    if(!source->model || source->model_kind!=2 || !name)return -1;
    cls=source->class_index;
    if(cls>=campaign_base_motions.class_count || !campaign_base_motions.classes ||
       cls>=campaign_motion_catalog.class_count || !campaign_motion_catalog.mappings ||
       campaign_motion_catalog.mappings[cls].weapon!=-1)return -2;
    return rf_entity_declared_action_lookup(campaign_base_motions.classes[cls].action_declarations,source->model,source->model_kind,name);
}
/* Populate only retained class inputs to416940. Actor fields and resolved
 * emitter index remain caller-owned; borrowed class strings require level life. */
int rf_scene_corpse_class_source(uint32_t cls,rf_corpse_create_source *source)
{
    const rf_entity_seed_class *definition;const rf_entity_motion_mapping *mapping;
    if(!source || !campaign_seeds.classes || cls>=campaign_seeds.class_count ||
       !campaign_motion_catalog.mappings || cls>=campaign_motion_catalog.class_count)return RF_RANGE;
    definition=campaign_seeds.classes+cls;mapping=campaign_motion_catalog.mappings+cls;
    if(mapping->weapon!=-1)return RF_FORMAT;
    source->class_index=cls;source->model_kind=definition->model_kind;
    source->class_flags_724=definition->physics.flags;source->class_flags_728=definition->physics.flags2;
    source->class_health=definition->vitals.health;source->class_value=definition->corpse.body_temperature;
    source->replacement_model=definition->corpse.model;source->emitter_lifetime=definition->corpse.emitter_lifetime;
    memcpy(source->motions,mapping->actions,sizeof(source->motions));return RF_OK;
}
/*4164c0 using the transferred model's cached matrices and the corpse's owned
 * physics spheres. Source class radii and extra physics spheres are retained. */
int rf_scene_corpse_pose(rf_corpse_owned *corpse)
{
    rf_entity_pose *pose;const rf_entity_render_model *model;uint32_t slot;int status;float radius;
    if(!corpse || !corpse->corpse.update.model)return RF_RANGE;
    slot=corpse->corpse.update.model-1;status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || !campaign_model_owners[slot].owned || !campaign_render_models.items ||
       pose->skeleton>=campaign_render_models.count)return RF_RANGE;
    model=campaign_render_models.items+pose->skeleton;
    if(model->collision_sphere_count>8 || model->bone_count!=pose->bone_count)return RF_RANGE;
    status=rf_model_corpse_spheres_refresh(model->collision_spheres,model->collision_sphere_count,
        pose->matrices,pose->bone_count,corpse->body.spheres.items,corpse->body.spheres.count,
        corpse->body.state.position,&corpse->body.state.bounds,&radius);
    if(status)return status;
    corpse->corpse.physics_radius=radius;corpse->corpse.model_radius=radius;return RF_OK;
}
/*5033f0: stop only looping weights; retain slots and references for advance. */
int rf_scene_corpse_reset(const rf_corpse *corpse)
{
    rf_entity_pose *pose;rf_entity_playback_model *model;uint32_t slot;int status;
    if(!corpse || !corpse->update.model)return RF_RANGE;
    slot=corpse->update.model-1;status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || !campaign_model_owners[slot].owned || pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+pose->skeleton;
    return rf_motion_stop_looping(&pose->playback,model->resources,model->count);
}
/*48ac70 from the corpse's own object transform and transferred skeletal pose.
 * The no-attachment path intentionally does not inspect the model. */
/* Authored bone/LOD-attachment view. Original51d420 also caches CSPH names
 * between these groups. The installed95-model audit proves they do not shadow
 * the corpse eye/spine queries; generic cached-tag placement remains separate.
 * Indices are local to this file view; do not mix with original runtime IDs. */
int rf_scene_corpse_file_tag(uint32_t model,const char *name,uint32_t fallback,int32_t *index)
{
    rf_entity_pose *pose;const rf_entity_skeleton *skeleton;const rf_model_file *file;
    rf_model_name names[50],query;rf_model_name_group groups[3]={{0}};
    rf_model_attachment attachment;uint32_t i;int32_t found;int status;
    if(!model || !name || !index || fallback>1)return RF_RANGE;
    status=campaign_model_pose(model-1,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    if(pose->skeleton>=campaign_skeletons.count || pose->skeleton>=campaign_render_models.count)return RF_RANGE;
    skeleton=campaign_skeletons.items+pose->skeleton;file=&campaign_render_models.items[pose->skeleton].file;
    if(skeleton->count!=pose->bone_count || skeleton->count>50 || (skeleton->count && !skeleton->bones))return RF_RANGE;
    query.data=name;query.length=strlen(name);
    for(i=0;i<skeleton->count;++i){names[i].data=skeleton->bones[i].name;names[i].length=strlen(names[i].data);}
    if(fallback)return rf_model_find_bone_substring(names,skeleton->count,query,index);
    groups[0].names=names;groups[0].count=skeleton->count;
    status=rf_model_find_tag(groups,query,&found);
    if(status==RF_OK){*index=found;return RF_OK;}if(status!=RF_NOT_FOUND)return status;
    if(!file->lod_count)return RF_FORMAT;
    if(file->lods[0].attachment_count>INT32_MAX-skeleton->count)return RF_RANGE;
    groups[0].count=1;
    for(i=0;i<file->lods[0].attachment_count;++i) {
        status=rf_model_file_attachment(file,0,i,&attachment);if(status)return status;
        names[0].data=attachment.name;names[0].length=strlen(attachment.name);
        status=rf_model_find_tag(groups,query,&found);
        if(status==RF_OK){*index=(int32_t)(skeleton->count+i);return RF_OK;}
        if(status!=RF_NOT_FOUND)return status;
    }
    return RF_NOT_FOUND;
}
int rf_scene_corpse_file_tag_point(const rf_corpse_surface_source *source,int32_t index,float point[3])
{
    rf_entity_pose *pose;rf_model_attachment attachment;float local[12],tag[12],placed[12];int status;
    if(!source || !source->model || !point || index<0)return RF_RANGE;
    status=campaign_model_pose(source->model-1,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    if(!pose->matrices || pose->skeleton>=campaign_render_models.count)return RF_RANGE;
    if((uint32_t)index<pose->bone_count)memcpy(tag,pose->matrices[index],48);
    else {
        status=rf_model_file_attachment(&campaign_render_models.items[pose->skeleton].file,0,(uint32_t)index-pose->bone_count,&attachment);
        if(status)return status;
        if(attachment.parent<0 || (uint32_t)attachment.parent>=pose->bone_count)return RF_RANGE;
        status=rf_model_attachment_transform(attachment.rotation,attachment.position,local);if(status)return status;
        status=rf_model_compose_transform(local,pose->matrices[attachment.parent],tag);if(status)return status;
    }
    status=rf_model_place_tag(tag,source->basis,source->position,placed);
    if(!status)memcpy(point,placed+9,12);return status;
}
/* File-tag source binding: installed eye/spine selections match the original
 * with local index translation; live death dispatch remains separate work. All geometry, lightmaps and model poses are borrowed;
 * calls serialize the room tree scratch and never evaluate/advance the pose. */
typedef struct scene_corpse_surface_context {
    rf_geometry_collision_world *world;
    rf_geometry_resident_lightmap_context lightmaps;
    int status;
} scene_corpse_surface_context;
static uint32_t scene_corpse_surface_metadata(void *context,uint32_t model)
{
    scene_corpse_surface_context *c=context;rf_entity_pose *pose=NULL;
    if(c->status)return 0;
    if(!model){c->status=RF_RANGE;return 0;}
    c->status=campaign_model_pose(model-1,&pose);
    return c->status || !pose ? 0:model;
}
static int32_t scene_corpse_surface_lookup(void *context,uint32_t model,const char *name,uint32_t fallback)
{
    scene_corpse_surface_context *c=context;int32_t index=-1;int status;
    if(c->status)return -1;
    status=rf_scene_corpse_file_tag(model,name,fallback,&index);
    if(status && status!=RF_NOT_FOUND)c->status=status;
    return status ? -1:index;
}
static int scene_corpse_surface_place(void *context,const rf_corpse_surface_source *source,int32_t index,float point[3])
{
    scene_corpse_surface_context *c=context;
    return c->status ? c->status:rf_scene_corpse_file_tag_point(source,index,point);
}
static int scene_corpse_surface_query(void *context,uint32_t descriptor,const float point[3],rf_corpse_surface_hit *hit,uint32_t *matched)
{
    scene_corpse_surface_context *c=context;
    return rf_geometry_corpse_surface(c->world,descriptor,point,hit,matched);
}
static int scene_corpse_surface_color(void *context,uint32_t face,const float point[3],uint32_t *color)
{
    scene_corpse_surface_context *c=context;
    return rf_geometry_corpse_resident_color(&c->lightmaps,face,point,color);
}
int rf_scene_corpse_file_surface_effects(rf_corpse_surface_pool *pool,const rf_corpse_surface_source *source,
    const uint32_t *flags,uint32_t enabled,rf_geometry_collision_world *world,
    const rf_geometry *geometry,const rf_lightmaps *maps)
{
    scene_corpse_surface_context context={world,{geometry,maps},RF_OK};
    rf_corpse_surface_backend backend={scene_corpse_surface_metadata,scene_corpse_surface_lookup,
        scene_corpse_surface_place,scene_corpse_surface_query,scene_corpse_surface_color,&context};int status;
    status=rf_corpse_source_effects(pool,source,flags,enabled,&backend);
    return context.status ? context.status:status;
}
int rf_scene_corpse_follow_point(const rf_corpse *corpse,float point[3])
{
    rf_entity_pose *pose;uint32_t slot;int status;
    if(!corpse || !point)return RF_RANGE;
    if(corpse->attachment_index==UINT32_MAX)
        return rf_model_object_follow_point(NULL,0,-1,NULL,corpse->update.position,point);
    if(!corpse->update.model)return RF_RANGE;
    slot=corpse->update.model-1;status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || !campaign_model_owners[slot].owned)return RF_RANGE;
    return rf_model_object_follow_point(pose->matrices,pose->bone_count,(int32_t)corpse->attachment_index,
        corpse->update.basis,corpse->update.position,point);
}
/* An actor loses access when its model storage is handed off. Model rendering
 * and eventual corpse updates continue through campaign_model_pose. */
static int campaign_actor_pose(uint32_t slot,rf_entity_pose **result)
{
    int status=campaign_model_pose(slot,result);if(status)return status;
    if(campaign_model_owners[slot].owned)*result=NULL;
    return RF_OK;
}
static void **campaign_npc_motion_data;
static uint32_t *campaign_npc_motion_sizes;
static uint32_t campaign_npc_motion_count,campaign_npc_motion_bytes;
/* Port residency policy: one immutable payload per shared cache identity.
 * A selected clip must be resident before pose sampling, including actions
 * started after initialization. Under pressure, only identities with zero
 * references across all model registrations may be released. */
static int campaign_npc_motion_reserve(uint32_t bytes)
{
    uint32_t cache,reclaim=0,needed,i,j,references;int status;
    if(campaign_npc_motion_bytes>1024*1024 || bytes>1024*1024)return RF_RANGE;
    if(bytes<=1024*1024-campaign_npc_motion_bytes)return RF_OK;
    needed=bytes-(1024*1024-campaign_npc_motion_bytes);
    /* Preflight the full candidate set before discarding any payload. The
     * simulation is single-threaded; references cannot change between passes. */
    for(cache=0;cache<campaign_npc_motion_count;++cache)if(campaign_npc_motion_data[cache]) {
        status=rf_entity_playback_cache_references(&campaign_playback_resources,cache,&references);if(status)return status;
        if(!references) {
            if(campaign_npc_motion_sizes[cache]>campaign_npc_motion_bytes-reclaim)return RF_RANGE;
            reclaim+=campaign_npc_motion_sizes[cache];
        }
    }
    if(reclaim<needed)return RF_RANGE;
    for(cache=0;cache<campaign_npc_motion_count && needed;++cache)if(campaign_npc_motion_data[cache]) {
        status=rf_entity_playback_cache_references(&campaign_playback_resources,cache,&references);if(status)return status;
        if(references)continue;
        /* Every alias must stop borrowing before the shared payload is freed. */
        for(i=0;i<campaign_motion_catalog.model_count;++i)for(j=0;j<campaign_motion_catalog.models[i].count;++j) {
            rf_motion_file *file=&campaign_motion_catalog.models[i].items[j].file;
            if(file->resident==campaign_npc_motion_data[cache])file->resident=NULL;
        }
        free(campaign_npc_motion_data[cache]);campaign_npc_motion_data[cache]=NULL;
        campaign_npc_motion_bytes-=campaign_npc_motion_sizes[cache];
        needed=campaign_npc_motion_sizes[cache]>=needed?0:needed-campaign_npc_motion_sizes[cache];
        campaign_npc_motion_sizes[cache]=0;
    }
    return RF_OK;
}
static int campaign_npc_motion_require(uint32_t skeleton,uint32_t id)
{
    uint32_t cache;rf_motion_file *file;void *data;int status;
    if(skeleton>=campaign_playback_resources.model_count || skeleton>=campaign_motion_catalog.model_count ||
       id>=campaign_playback_resources.models[skeleton].count || id>=campaign_motion_catalog.models[skeleton].count ||
       !campaign_npc_motion_data || !campaign_npc_motion_sizes)return RF_RANGE;
    cache=campaign_playback_resources.models[skeleton].cache_ids[id];
    if(cache>=campaign_npc_motion_count)return RF_RANGE;
    file=&campaign_motion_catalog.models[skeleton].items[id].file;
    if(campaign_npc_motion_data[cache]) {
        if(file->resident==campaign_npc_motion_data[cache])return RF_OK;
        return rf_motion_file_bind_memory(file,campaign_npc_motion_data[cache],campaign_npc_motion_sizes[cache]);
    }
    status=campaign_npc_motion_reserve(file->entry.size);if(status)return status;
    data=malloc(file->entry.size);if(!data)return RF_IO;
    status=rf_vpp_read(file->archive,&file->entry,0,data,file->entry.size);
    if(!status)status=rf_motion_file_bind_memory(file,data,file->entry.size);
    if(status){free(data);return status;}
    campaign_npc_motion_data[cache]=data;campaign_npc_motion_sizes[cache]=file->entry.size;
    campaign_npc_motion_bytes+=file->entry.size;return RF_OK;
}
/* Corpse5033b0/5033e0 use model-local motion IDs, not actor action slots. */
int rf_scene_corpse_play(const rf_corpse *corpse,int32_t motion)
{
    rf_entity_pose *pose;rf_entity_playback_model *model;uint32_t slot;int status;
    if(!corpse || !corpse->update.model || motion<0)return RF_RANGE;
    slot=corpse->update.model-1;status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || !campaign_model_owners[slot].owned)return RF_RANGE;
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)return status;
    model=campaign_playback_resources.models+pose->skeleton;
    return rf_motion_start(&pose->playback,model->resources,model->count,motion,1,1);
}
/*503360/501ab0/51ba80 timing only. Type2 does not consume world transforms.
 * Pose matrices remain generation-tagged and must be evaluated before use. */
int rf_scene_corpse_advance(const rf_corpse *corpse,float elapsed)
{
    rf_entity_pose *pose;rf_entity_playback_model *model;uint32_t slot;int status;
    if(!corpse || !corpse->update.model)return RF_RANGE;
    slot=corpse->update.model-1;status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || !campaign_model_owners[slot].owned || !campaign_playback_resources.models ||
       pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+pose->skeleton;
    return rf_motion_update(&pose->playback,model->resources,model->count,elapsed);
}
/* Original51ba00 asks51b500 to evaluate before preparing skinning matrices.
 * Stationary NPCs have no retained pending root displacement. Transferred
 * corpses must use their explicit displacement-aware evaluation first. */
static int campaign_model_pose_ready(uint32_t slot,rf_entity_pose *pose)
{
    uint32_t i;int status=RF_OK;float displacement[3]={0};
    ++rf_scene_npc_pose_demand[0];
    if(!pose->generations || !pose->bone_count || pose->bone_count>50 || pose->playback.generation>65535){status=RF_RANGE;goto done;}
    for(i=0;i<pose->bone_count;++i)if(pose->generations[i]!=(uint16_t)pose->playback.generation)break;
    if(i==pose->bone_count)return RF_OK;
    if(campaign_model_owners[slot].owned || pose->playback.completion.active.count>16){status=RF_RANGE;goto done;}
    for(i=0;i<pose->playback.completion.active.count;++i) {
        int32_t id=pose->playback.completion.active.slots[i].motion;
        if(id<0){status=RF_RANGE;goto done;}
        status=campaign_npc_motion_require(pose->skeleton,(uint32_t)id);if(status)goto done;
    }
    status=rf_entity_pose_evaluate(pose,&campaign_skeletons,&campaign_motion_catalog,displacement);
    if(!status)++rf_scene_npc_pose_demand[1];
 done:
    if(status)++rf_scene_npc_pose_demand[2];return status;
}
/* Explicit demand evaluation after timing advancement; callers supply pending
 * root displacement. Failed sampling can leave a partial cache, as in the
 * shared evaluator, and must prevent subsequent pose consumers. */
int rf_scene_corpse_evaluate(const rf_corpse *corpse,float pending_displacement[3])
{
    rf_entity_pose *pose;uint32_t slot,i;int status;
    if(!corpse || !corpse->update.model || !pending_displacement)return RF_RANGE;
    slot=corpse->update.model-1;status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || !campaign_model_owners[slot].owned || pose->playback.completion.active.count>16)return RF_RANGE;
    for(i=0;i<pose->playback.completion.active.count;++i) {
        int32_t motion=pose->playback.completion.active.slots[i].motion;if(motion<0)return RF_RANGE;
        status=campaign_npc_motion_require(pose->skeleton,(uint32_t)motion);if(status)return status;
    }
    status=rf_entity_pose_evaluate(pose,&campaign_skeletons,&campaign_motion_catalog,pending_displacement);
    return status?status:campaign_model_collision_refresh(slot);
}
int rf_scene_corpse_duration(const rf_corpse *corpse,int32_t motion,double *seconds)
{
    rf_entity_pose *pose;const rf_motion_file *file;uint32_t slot;int status;
    if(!corpse || !corpse->update.model || motion<0 || !seconds)return RF_RANGE;
    slot=corpse->update.model-1;status=campaign_model_pose(slot,&pose);if(status)return status;
    if(!pose || !campaign_model_owners[slot].owned || !campaign_motion_catalog.models ||
       pose->skeleton>=campaign_motion_catalog.model_count ||
       (uint32_t)motion>=campaign_motion_catalog.models[pose->skeleton].count ||
       !campaign_motion_catalog.models[pose->skeleton].items)return RF_RANGE;
    file=&campaign_motion_catalog.models[pose->skeleton].items[motion].file;
    *seconds=rf_motion_duration((int32_t)file->header[4],(int32_t)file->header[5]);return RF_OK;
}
/*48a230 for the resolved attached item. Registry/type/lifetime validation is
 * the item owner's responsibility; this bridge updates its retained world pose. */
int rf_scene_corpse_item_position(rf_group_attached_pose *pose,const float point[3])
{return rf_group_pose_set_position(pose,point);}
typedef struct rf_scene_corpse_item_ops {
    rf_corpse_item_view *(*lookup)(void *,int32_t);
    int (*move)(void *,rf_corpse_item_view *,const float[3]);void *context;
} rf_scene_corpse_item_ops;
typedef struct campaign_corpse_tick {
    rf_corpse_owned *owner;float *pending;const rf_scene_corpse_item_ops *items;int status;
} campaign_corpse_tick;
static int corpse_tick_model(campaign_corpse_tick *c,uint32_t model)
{
    if(!c->status && model!=c->owner->corpse.update.model)c->status=RF_RANGE;
    return c->status;
}
static void corpse_tick_reset(void *context,uint32_t model)
{campaign_corpse_tick *c=context;if(!corpse_tick_model(c,model))c->status=rf_scene_corpse_reset(&c->owner->corpse);}
static void corpse_tick_play(void *context,uint32_t model,int32_t motion)
{campaign_corpse_tick *c=context;if(!corpse_tick_model(c,model))c->status=rf_scene_corpse_play(&c->owner->corpse,motion);}
static double corpse_tick_duration(void *context,uint32_t model,int32_t motion)
{campaign_corpse_tick *c=context;double seconds=NAN;if(!corpse_tick_model(c,model))c->status=rf_scene_corpse_duration(&c->owner->corpse,motion,&seconds);return seconds;}
static void corpse_tick_advance(void *context,uint32_t model,float dt,const float *position,const float *basis)
{campaign_corpse_tick *c=context;(void)position;(void)basis;if(!corpse_tick_model(c,model))c->status=rf_scene_corpse_advance(&c->owner->corpse,dt);}
static void corpse_tick_pose(void *context)
{
    campaign_corpse_tick *c=context;if(c->status)return;
    c->status=rf_scene_corpse_evaluate(&c->owner->corpse,c->pending);
    if(!c->status)c->status=rf_scene_corpse_pose(c->owner);
}
static rf_corpse_item_view *corpse_tick_item(void *context,int32_t id)
{
    campaign_corpse_tick *c=context;if(c->status)return NULL;
    if(!c->items || !c->items->lookup || !c->items->move){c->status=RF_NOT_FOUND;return NULL;}
    return c->items->lookup(c->items->context,id);
}
static int corpse_tick_follow(void *context,float point[3])
{
    campaign_corpse_tick *c=context;if(c->status)return c->status;
    if(c->owner->corpse.attachment_index!=UINT32_MAX)c->status=rf_scene_corpse_evaluate(&c->owner->corpse,c->pending);
    if(!c->status)c->status=rf_scene_corpse_follow_point(&c->owner->corpse,point);return c->status;
}
static void corpse_tick_move(void *context,rf_corpse_item_view *item,const float point[3])
{campaign_corpse_tick *c=context;if(!c->status)c->status=c->items->move(c->items->context,item,point);}
/*417290 orchestration with retained model adapters and explicit external item
 * ownership. No rendering, deferred deletion, or live-list dispatch here. */
int rf_scene_corpse_update(rf_corpse_owned *owner,float dt,int32_t now,rf_corpse_emitter_link *emitters,
    uint32_t limit,float pending_displacement[3],const rf_scene_corpse_item_ops *items)
{
    campaign_corpse_tick c={owner,pending_displacement,items,0};int status;
    rf_corpse_update_backend backend={corpse_tick_reset,corpse_tick_play,corpse_tick_duration,corpse_tick_advance,
        corpse_tick_pose,corpse_tick_item,corpse_tick_follow,corpse_tick_move,&c};
    if(!owner || !pending_displacement)return RF_RANGE;
    status=rf_corpse_update(&owner->corpse.update,dt,now,emitters,limit,&backend);
    return c.status?c.status:status;
}
static int campaign_npc_pose_residency(uint32_t actor)
{
    uint32_t i;int status;
    if(actor>=campaign_poses.count || actor>=campaign_seeds.records.count)return RF_RANGE;
    {
        rf_entity_pose *pose;status=campaign_model_pose(actor,&pose);if(status)return status;
        if(!pose)return RF_OK;
        if(pose->playback.completion.active.count>16 ||
           campaign_seeds.items[actor].class_index>=campaign_motion_catalog.class_count)return RF_RANGE;
        for(i=0;i<pose->playback.completion.active.count+3;++i) {
            uint32_t id;int32_t selected;
            if(i<pose->playback.completion.active.count)selected=pose->playback.completion.active.slots[i].motion;
            else {
                uint32_t which=i-pose->playback.completion.active.count;
                int32_t state=which==0?pose->controller.current:which==1?pose->controller.next:
                    pose->controller.override_enabled?pose->controller.override_state:-1;
                if(state<0)continue;if(state>=23)return RF_RANGE;
                selected=campaign_motion_catalog.mappings[campaign_seeds.items[actor].class_index].states[state];
            }
            if(selected<0)continue;id=(uint32_t)selected;
            status=campaign_npc_motion_require(pose->skeleton,id);if(status)return status;
        }
    }
    return RF_OK;
}
static int campaign_npc_motion_residency(void)
{
    uint32_t actor,i,j;int status;
    campaign_npc_motion_count=campaign_playback_resources.cache_count;
    if(campaign_npc_motion_count>(1024*1024)/(sizeof(void*)+sizeof(uint32_t)))return RF_RANGE;
    campaign_npc_motion_bytes=campaign_npc_motion_count*(sizeof(void*)+sizeof(uint32_t));
    if(!campaign_npc_motion_count)return RF_OK;
    campaign_npc_motion_data=calloc(campaign_npc_motion_count,sizeof(void*));if(!campaign_npc_motion_data)return RF_IO;
    campaign_npc_motion_sizes=calloc(campaign_npc_motion_count,sizeof(uint32_t));if(!campaign_npc_motion_sizes)return RF_IO;
    for(actor=0;actor<campaign_poses.count;++actor) {
        status=campaign_npc_pose_residency(actor);if(status)return status;
    }
    for(i=0;i<campaign_motion_catalog.model_count;++i)for(j=0;j<campaign_motion_catalog.models[i].count;++j) {
        uint32_t cache=campaign_playback_resources.models[i].cache_ids[j];rf_motion_file *file=&campaign_motion_catalog.models[i].items[j].file;
        if(campaign_npc_motion_data[cache]) {
            status=rf_motion_file_bind_memory(file,campaign_npc_motion_data[cache],campaign_npc_motion_sizes[cache]);if(status)return status;
        }
    }
    return RF_OK;
}

static rf_entity_appearances campaign_appearances;
static rf_entity_materials campaign_npc_materials;
uint32_t rf_scene_npc_materials[8]; /* appearances, materials, images, resident, peak, binding hash, pixel bytes, pixel hash */
uint32_t rf_scene_npc_geometry[7]; /* models, LODs, vertices, triangles, bytes, geometry hash, prepared skin hash */
uint32_t rf_scene_npc_startup[4]; /* actors, bones, playback hash, matrix/cache hash */
static rf_entity_view campaign_player_view;
static const rf_animation_model_view *campaign_player_model;
int rf_scene_draw_player_flash(rf_scene_particle_sink sink,void *context)
{
    rf_screen_flash next,draw;rf_particle_draw_vertex vertices[4];uint32_t active,i,color;int status;
    if(!particle_draw_stream || !campaign_spawn)return RF_OK;
    if(rf_entity_lookup(&campaign_entities,campaign_player_view.handle)!=&campaign_player_view)return RF_NOT_FOUND;
    next=campaign_player_flash;
    status=rf_screen_flash_step(&next,scene_step_seconds,0,&draw,&active);if(status || !active)return status;
    color=((uint32_t)draw.rgba[3]<<24)|((uint32_t)draw.rgba[0]<<16)|((uint32_t)draw.rgba[1]<<8)|draw.rgba[2];
    memset(vertices,0,sizeof(vertices));
    for(i=0;i<4;++i) {
        vertices[i].screen[0]=(i==1 || i==2)?640:0;vertices[i].screen[1]=i>=2?480:0;
        vertices[i].reciprocal_w=1;vertices[i].argb=color;
    }
    if(sink){status=sink(context,vertices,4,NULL,0x18000);if(status)return status;}
    campaign_player_flash=next;return RF_OK;
}
int rf_scene_player_damage_flash(uint32_t player_entity_handle)
{
    if(rf_entity_lookup(&campaign_entities,(int32_t)player_entity_handle)!=&campaign_player_view)return RF_NOT_FOUND;
    return rf_screen_flash_set(&campaign_player_flash,255,0,0,128);
}
int rf_scene_player_flash_step(uint32_t player_entity_handle,float seconds,uint32_t freeze,
    rf_screen_flash *draw,uint32_t *active)
{
    if(rf_entity_lookup(&campaign_entities,(int32_t)player_entity_handle)!=&campaign_player_view)return RF_NOT_FOUND;
    return rf_screen_flash_step(&campaign_player_flash,seconds,freeze,draw,active);
}
int rf_scene_player_feedback(uint32_t player_entity_handle,float strength,float duration,int32_t now)
{
    if(rf_entity_lookup(&campaign_entities,(int32_t)player_entity_handle)!=&campaign_player_view)return RF_NOT_FOUND;
    return rf_camera_effect_start(&campaign_camera_effect,strength,duration,now);
}
static rf_registered_entity_view campaign_player_object;
uint32_t rf_scene_campaign_player[4]; /* registered handle, kind, initial object flags, adapter bytes */
uint32_t rf_scene_trigger_contacts[6]; /* polls, ready, last ready UID, lower door ready, unsupported, status */
typedef struct campaign_controller_effects {
    uint32_t source,actor,pending,starts,start_frame;
    rf_group_sound_state sounds;
} campaign_controller_effects;
static campaign_controller_effects *campaign_controller_requests;
static rf_audio_bank campaign_audio_bank;
static rf_foley_owner campaign_foley;
static int32_t (*campaign_footstep_groups)[10];
static int32_t (*campaign_pain_groups)[2];
static int32_t *campaign_impact_groups;
uint32_t rf_scene_npc_impact_groups[3]; /* classes,owned bytes,binding hash */
uint32_t rf_scene_npc_pain_groups[3]; /* classes, resident bytes, binding hash */
uint32_t rf_scene_foley[10]; /* groups,samples,missing,resident,peak,bank count,global hash,ID hash,classes,class hash */
static uint32_t npc_hash_bytes(uint32_t hash,const void *data,uint32_t bytes);
static rf_vpp campaign_audio_archive;
/* Only lazily loaded ambient PCM is eligible; preload users cannot yet reload. */
static uint8_t campaign_audio_evictable[2600];
static int32_t campaign_ambient_pan[RF_AMBIENT_SLOTS];
uint32_t rf_scene_ambient_audio[8]; /* sweeps, starts, stops, refreshes, failures, lazy PCM bytes, active, effect hash */
static rf_sound_metadata_owner campaign_audio_metadata;
/* Authored count/owner bytes/loop count; registered matched/looping/missing;
 * registered packed-word hash; full compact rows+order byte hash. */
uint32_t rf_scene_sound_metadata[8];
static int campaign_metadata_open(const char *path)
{
    FILE *stream=fopen(path,"rb");void *text=NULL;long bytes;uint32_t i;int status=RF_IO;
    memset(rf_scene_sound_metadata,0,sizeof(rf_scene_sound_metadata));
    if(!stream)return RF_IO;
    if(fseek(stream,0,SEEK_END) || (bytes=ftell(stream))<0 || fseek(stream,0,SEEK_SET))goto done;
    if(!bytes || bytes>524288) {status=RF_RANGE;goto done;}
    text=malloc((size_t)bytes);if(!text) {status=RF_RANGE;goto done;}
    if(fread(text,1,(size_t)bytes,stream)!=(size_t)bytes)goto done;
    status=rf_sound_metadata_open(text,(uint32_t)bytes,524288,&campaign_audio_metadata);
    if(!status) {
        rf_scene_sound_metadata[0]=campaign_audio_metadata.count;
        rf_scene_sound_metadata[1]=campaign_audio_metadata.allocated_bytes;
        rf_scene_sound_metadata[6]=rf_scene_sound_metadata[7]=2166136261u;
        for(i=0;i<campaign_audio_metadata.count;++i)
            rf_scene_sound_metadata[2]+=(campaign_audio_metadata.rows[i].loop_flags>>30)&1;
        for(i=0;i<campaign_audio_metadata.count*sizeof(*campaign_audio_metadata.rows);++i)
            rf_scene_sound_metadata[7]=(rf_scene_sound_metadata[7]^((unsigned char *)campaign_audio_metadata.rows)[i])*16777619u;
        for(i=0;i<RF_SOUND_METADATA_CAPACITY*sizeof(*campaign_audio_metadata.order);++i)
            rf_scene_sound_metadata[7]=(rf_scene_sound_metadata[7]^((unsigned char *)campaign_audio_metadata.order)[i])*16777619u;
    }
done:
    free(text);fclose(stream);return status;
}
static rf_audio_mixer campaign_audio_mixer;
static int16_t campaign_audio_frame[1600];
static rf_scene_audio_sink campaign_audio_sink;
static void *campaign_audio_context;
static rf_scene_audio_observer campaign_audio_observer;
static void *campaign_audio_observer_context;
void rf_scene_set_audio_observer(rf_scene_audio_observer observer,void *context)
{campaign_audio_observer=observer;campaign_audio_observer_context=context;}
static rf_scene_audio_events campaign_audio_events;
static void *campaign_audio_events_context;
void rf_scene_set_audio_events(const rf_scene_audio_events *events,void *context)
{
    if(events)campaign_audio_events=*events;else memset(&campaign_audio_events,0,sizeof(campaign_audio_events));
    campaign_audio_events_context=context;
}
void rf_scene_set_audio(rf_scene_audio_sink sink,void *context)
{campaign_audio_sink=sink;campaign_audio_context=context;}
/* loaded samples, retained bytes, missing names, rejected resources, played,
 * unavailable requests, rendered frames, PCM byte hash. No device output yet. */
uint32_t rf_scene_live_audio[8];
uint32_t rf_scene_sound_bank[4]; /* global declarations, resident samples, PCM file bytes, metadata bytes */
/* Switch count, resident named activations, rejection-slot residency, added PCM bytes. */
uint32_t rf_scene_switch_audio[4];
typedef struct campaign_spatial_voice {
    uint32_t handle,sample;float position[3],volume;int32_t last_volume,last_pan;
    uint32_t positional;
} campaign_spatial_voice;
static campaign_spatial_voice campaign_spatial_voices[RF_AUDIO_VOICES];
static rf_audio_voice_ids campaign_device_voice_ids;
static float campaign_listener_position[3],campaign_listener_right[3];
/* Initial updates, refresh updates, integer gain/pan hash, noncenter updates,
 * changed settings, fixed tracking bytes. Unity PCM diagnostic is separate. */
uint32_t rf_scene_spatial_audio[6];
/* Controller loop starts, explicit stops, mixed voice blocks, observed cursor wraps. */
uint32_t rf_scene_controller_audio[4];
static void campaign_spatial_update(campaign_spatial_voice *voice,int initial)
{
    const rf_audio_parameters *parameters=rf_audio_bank_parameters(&campaign_audio_bank,voice->sample);
    float spatial[2],gains[2],volume;int32_t device_volume,pan;
    if(!parameters || !voice->handle || !voice->positional)return;
    rf_audio_position(voice->position,campaign_listener_position,campaign_listener_right,
        parameters->near_distance,parameters->far_distance,parameters->rolloff,voice->volume,spatial);
    /* Controller group/category gains remain unity. Original initial and
     * listener-refresh paths differ in default-volume application. */
    volume=initial?spatial[1]*voice->volume*parameters->volume:spatial[1];
    if(volume<0)volume=0;if(volume>1)volume=1;
    if(spatial[0]<-1)spatial[0]=-1;if(spatial[0]>1)spatial[0]=1;
    device_volume=rf_audio_device_volume(volume,0);pan=(int32_t)((double)spatial[0]*1000);
    if(rf_audio_device_gains(device_volume,pan,gains))return;
    ++rf_scene_spatial_audio[initial?0:1];
    rf_scene_spatial_audio[2]=(rf_scene_spatial_audio[2]^(uint32_t)device_volume)*16777619u;
    rf_scene_spatial_audio[2]=(rf_scene_spatial_audio[2]^(uint32_t)pan)*16777619u;
    if(pan)++rf_scene_spatial_audio[3];
    if(device_volume!=voice->last_volume || pan!=voice->last_pan) {
        ++rf_scene_spatial_audio[4];voice->last_volume=device_volume;voice->last_pan=pan;
        if(!initial && campaign_audio_events.gain)campaign_audio_events.gain(campaign_audio_events_context,voice->handle,gains[0],gains[1]);
    }
}
static void campaign_audio_listener(const float position[3],const float right[3])
{
    uint32_t i;memcpy(campaign_listener_position,position,12);memcpy(campaign_listener_right,right,12);
    for(i=0;i<RF_AUDIO_VOICES;i++)if(campaign_spatial_voices[i].handle)campaign_spatial_update(campaign_spatial_voices+i,0);
}
static int32_t campaign_ambient_register(void *context,const char *name,float near_distance,float volume,float rolloff)
{
    rf_audio_bank *bank=context;uint32_t index;int status;
    status=rf_audio_bank_declare(bank,name,near_distance,volume,rolloff,&index);
    if(!status)return (int32_t)index;
    if(status==RF_NOT_FOUND)++rf_scene_live_audio[2];else ++rf_scene_live_audio[3];
    return -1;
}
typedef struct campaign_foley_registration {int error;uint32_t missing;} campaign_foley_registration;
static int32_t campaign_foley_register(void *context,const char *name,float near_distance,float volume,float rolloff)
{
    campaign_foley_registration *result=context;uint32_t index;int status;
    status=rf_audio_bank_declare(&campaign_audio_bank,name,near_distance,volume,rolloff,&index);
    if(!status)return (int32_t)index;
    if(status==RF_NOT_FOUND)++result->missing;else if(!result->error)result->error=status;
    return -1;
}
static int campaign_ambient_gains(float volume,int32_t pan,float gains[2])
{
    if(volume<0)volume=0;if(volume>1)volume=1;
    return rf_audio_device_gains(rf_audio_device_volume(volume,0),pan,gains);
}
static int campaign_ambient_reload(uint32_t sample)
{
    uint32_t released[2];int status;
    if((campaign_audio_events.play || campaign_audio_events.play_mode) && !campaign_audio_events.release_idle_sample)
        return rf_audio_bank_reload(&campaign_audio_bank,&campaign_audio_archive,sample);
    status=rf_audio_bank_reload_idle(&campaign_audio_bank,&campaign_audio_mixer,&campaign_audio_archive,sample,
        campaign_audio_evictable,sizeof(campaign_audio_evictable),campaign_audio_events.release_idle_sample,
        campaign_audio_events_context,released);
    rf_scene_sound_bank[1]-=released[0];rf_scene_sound_bank[2]-=released[1];rf_scene_live_audio[1]=campaign_audio_bank.bytes;
    return status;
}
static int32_t campaign_ambient_start_voice(void *context,int32_t sample,float gain,float pan,uint32_t looping)
{
    rf_ambient_slot *slot=context;uint32_t handle;float gains[2];int32_t device_pan,id;
    const rf_wave_pcm *pcm;const rf_audio_parameters *parameters=rf_audio_bank_parameters(&campaign_audio_bank,(uint32_t)sample);
    const rf_sound_metadata *metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,
        campaign_audio_metadata.order,campaign_audio_bank.samples[sample].name);
    /* Installed loop starts are zero. Reject unsupported offsets explicitly. */
    if(!parameters || (looping && metadata && (metadata->loop_flags&0x07ffffffu)))goto failed;
    pcm=rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample);
    if(!pcm) {
        if(campaign_ambient_reload((uint32_t)sample))goto failed;
        if((uint32_t)sample<sizeof(campaign_audio_evictable))campaign_audio_evictable[sample]=1;
        pcm=rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample);
        ++rf_scene_sound_bank[1];rf_scene_sound_bank[2]+=campaign_audio_bank.samples[sample].bytes;
        rf_scene_live_audio[1]=campaign_audio_bank.bytes;
        rf_scene_ambient_audio[5]+=campaign_audio_bank.samples[sample].bytes;
    }
    gain=rf_audio_sample_gain(parameters->volume,1,gain);
    if(pan< -1)pan=-1;if(pan>1)pan=1;device_pan=(int32_t)((double)pan*1000);
    if(campaign_ambient_gains(gain,device_pan,gains) ||
       rf_audio_voice_start(&campaign_audio_mixer,pcm,(uint32_t)(gains[0]*32768),(uint32_t)(gains[1]*32768),looping,&handle))goto failed;
    if((campaign_audio_events.play && !campaign_audio_events.play_mode) ||
       (campaign_audio_events.play_mode && campaign_audio_events.play_mode(campaign_audio_events_context,handle,pcm,gains[0],gains[1],looping))) {
        rf_audio_voice_stop(&campaign_audio_mixer,handle);goto failed;
    }
    /* Successful mixer handles may have their sign bit set. Ambient slots
     * retain an independent nonnegative device identity, including zero. */
    if(rf_audio_voice_ids_bind(&campaign_device_voice_ids,handle,&id)) {
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
        rf_audio_voice_stop(&campaign_audio_mixer,handle);goto failed;
    }
    memset(campaign_spatial_voices+(handle&0xffff),0,sizeof(*campaign_spatial_voices));
    campaign_ambient_pan[slot-campaign_ambient_slots]=device_pan;
    ++rf_scene_ambient_audio[1];++rf_scene_live_audio[4];
    rf_scene_ambient_audio[7]=(rf_scene_ambient_audio[7]^(uint32_t)sample)*16777619u;
    return id;
failed:
    ++rf_scene_ambient_audio[4];return -1;
}
static void campaign_ambient_stop_voice(void *context,int32_t voice)
{
    uint32_t handle;(void)context;
    if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&handle)) {
        rf_audio_voice_stop(&campaign_audio_mixer,handle);
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
    }
    ++rf_scene_ambient_audio[2];rf_scene_ambient_audio[7]=(rf_scene_ambient_audio[7]^(uint32_t)voice)*16777619u;
    /* PCM stays owned until reset; a void stop callback cannot certify all borrowers released. */
}
static void campaign_ambient_refresh_voice(void *context,int32_t voice,int32_t sample,const float position[3])
{
    rf_ambient_slot *slot=context;float gain,gains[2];int32_t volume;uint32_t handle;
    const rf_audio_parameters *parameters=rf_audio_bank_parameters(&campaign_audio_bank,(uint32_t)sample);
    gain=rf_audio_ambient_gain(parameters,position,campaign_listener_position,1,1,1);
    if(campaign_ambient_gains(gain,campaign_ambient_pan[slot-campaign_ambient_slots],gains)) {++rf_scene_ambient_audio[4];return;}
    if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&handle)) {
        rf_audio_voice_gain(&campaign_audio_mixer,handle,(uint32_t)(gains[0]*32768),(uint32_t)(gains[1]*32768));
        if(campaign_audio_events.gain)campaign_audio_events.gain(campaign_audio_events_context,handle,gains[0],gains[1]);
    }
    ++rf_scene_ambient_audio[3];volume=rf_audio_device_volume(gain,0);
    rf_scene_ambient_audio[7]=(rf_scene_ambient_audio[7]^(uint32_t)volume)*16777619u;
}
static void campaign_ambient_process(void)
{
    static const rf_ambient_voice_backend backend={campaign_ambient_start_voice,campaign_ambient_stop_voice,campaign_ambient_refresh_voice};
    uint32_t i;rf_scene_ambient_audio[6]=0;++rf_scene_ambient_audio[0];
    for(i=0;i<RF_AMBIENT_SLOTS;++i) {
        rf_ambient_slot *slot=campaign_ambient_slots+i;const rf_audio_parameters *parameters;
        const rf_sound_metadata *metadata;float spatial[2];uint32_t looping,handle,index;
        if(slot->sample<0)continue;
        parameters=rf_audio_bank_parameters(&campaign_audio_bank,(uint32_t)slot->sample);
        if(!parameters){++rf_scene_ambient_audio[4];continue;}
        metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,campaign_audio_bank.samples[slot->sample].name);
        /* Original543580 static fallback5a7c60 has loop/music bits zero. */
        looping=metadata?(metadata->loop_flags>>30)&1:0;
        rf_audio_position(slot->position,campaign_listener_position,campaign_listener_right,
            parameters->near_distance,parameters->far_distance,parameters->rolloff,slot->volume,spatial);
        rf_ambient_voice_update(slot,spatial[1],spatial[0],1,looping,&backend,slot);
        if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,slot->voice,&handle)) {
            index=handle&0xffff;
            if(campaign_audio_mixer.voices[index].active)++rf_scene_ambient_audio[6];
        }
    }
}
static int campaign_ambient_schedule(int32_t now,uint32_t initial)
{
    uint32_t i;int status=rf_ambient_schedule(&campaign_ambient_instances,campaign_ambient_slots,1,now,initial);
    if(status)return status;
    if(!initial)++rf_scene_ambient_schedule[0];rf_scene_ambient_schedule[1]=(uint32_t)now;
    rf_scene_ambient_schedule[2]=rf_scene_ambient_schedule[3]=0;
    rf_scene_ambient_schedule[4]=rf_scene_ambient_schedule[5]=2166136261u;
    for(i=0;i<RF_AMBIENT_SLOTS;i++)rf_scene_ambient_schedule[2]+=campaign_ambient_slots[i].sample>=0;
    for(i=0;i<campaign_ambient_instances.count;i++)rf_scene_ambient_schedule[3]+=campaign_ambient_instances.items[i].deadline>=0;
    for(i=0;i<campaign_ambient_instances.count*sizeof(*campaign_ambient_instances.items);i++)
        rf_scene_ambient_schedule[4]=(rf_scene_ambient_schedule[4]^((unsigned char *)campaign_ambient_instances.items)[i])*16777619u;
    for(i=0;i<sizeof(campaign_ambient_slots);i++)
        rf_scene_ambient_schedule[5]=(rf_scene_ambient_schedule[5]^((unsigned char *)campaign_ambient_slots)[i])*16777619u;
    return RF_OK;
}
static int campaign_audio_open(const char *tables_path,const char *level_name,const char *player_class)
{
    char path[1024];size_t prefix=0,n;uint32_t i,j,index,capacity;
    rf_vpp archive={0},tables={0};rf_audio_declaration *declarations=NULL;uint32_t declared=0,foley_groups,foley_samples,foley_bank_bytes,global_hash=2166136261u;
    rf_vpp_entry foley_entry,entity_entry;void *foley_text=NULL,*entity_text=NULL;
    campaign_foley_registration foley_registration={0};int status;
    memset(rf_scene_foley,0,sizeof(rf_scene_foley));
    memset(rf_scene_npc_pain_groups,0,sizeof(rf_scene_npc_pain_groups));
    memset(campaign_spatial_voices,0,sizeof(campaign_spatial_voices));
    memset(campaign_listener_position,0,sizeof(campaign_listener_position));
    memset(campaign_listener_right,0,sizeof(campaign_listener_right));campaign_listener_right[0]=-1;
    memset(rf_scene_spatial_audio,0,sizeof(rf_scene_spatial_audio));rf_scene_spatial_audio[2]=2166136261u;
    rf_scene_spatial_audio[5]=sizeof(campaign_spatial_voices)+sizeof(campaign_listener_position)+sizeof(campaign_listener_right);
    memset(rf_scene_live_audio,0,sizeof(rf_scene_live_audio));rf_scene_live_audio[7]=2166136261u;
    memset(rf_scene_sound_bank,0,sizeof(rf_scene_sound_bank));
    memset(rf_scene_switch_audio,0,sizeof(rf_scene_switch_audio));
    memset(rf_scene_controller_audio,0,sizeof(rf_scene_controller_audio));
    memset(rf_scene_ambient_audio,0,sizeof(rf_scene_ambient_audio));rf_scene_ambient_audio[7]=2166136261u;
    memset(campaign_ambient_pan,0,sizeof(campaign_ambient_pan));
    rf_audio_voice_ids_init(&campaign_device_voice_ids);
    rf_audio_mixer_init(&campaign_audio_mixer);
    for(i=0;i<campaign_group_runtime.count;i++)for(j=0;j<4;j++) {
        campaign_controller_requests[i].sounds.samples[j]=-1;
        campaign_controller_requests[i].sounds.handles[j]=-1;
    }
    for(n=0;tables_path[n];n++)if(tables_path[n]=='/' || tables_path[n]=='\\')prefix=n+1;
    if(prefix+sizeof("bluebeard.bty")>sizeof(path))return RF_RANGE;
    memcpy(path,tables_path,prefix);memcpy(path+prefix,"bluebeard.bty",sizeof("bluebeard.bty"));
    status=campaign_metadata_open(path);if(status)return status;
    memcpy(path,tables_path,prefix);memcpy(path+prefix,"audio.vpp",sizeof("audio.vpp"));
    status=rf_vpp_open(&archive,path);if(status)return status;
    status=rf_vpp_open(&tables,tables_path);if(status)goto audio_done;
    status=rf_sound_table_load(&tables,65536,NULL,0,&declared);if(status)goto audio_done;
    if(declared) {
        declarations=malloc((size_t)declared*sizeof(*declarations));if(!declarations){status=RF_RANGE;goto audio_done;}
        status=rf_sound_table_load(&tables,65536,declarations,declared,&declared);if(status)goto audio_done;
    }
    status=rf_vpp_find(&tables,"foley.tbl",&foley_entry);if(status)goto audio_done;
    if(!foley_entry.size || foley_entry.size>256*1024){status=RF_RANGE;goto audio_done;}
    foley_text=malloc(foley_entry.size);if(!foley_text){status=RF_RANGE;goto audio_done;}
    status=rf_vpp_read(&tables,&foley_entry,0,foley_text,foley_entry.size);if(status)goto audio_done;
    status=rf_foley_table_read(foley_text,foley_entry.size,NULL,0,NULL,0,&foley_groups,&foley_samples);if(status)goto audio_done;
    capacity=declared+foley_samples+campaign_group_runtime.count*4+campaign_ambient.count;
    for(i=0;i<campaign_events.count;i++)if(campaign_events.items[i].switch_state) {
        ++rf_scene_switch_audio[0];
        if(campaign_events.items[i].authored->record.texts[0][0])++capacity;
    }
    if(!capacity)capacity=1;if(capacity>2600)capacity=2600;
    status=rf_audio_bank_open(&archive,capacity,1024*1024,&campaign_audio_bank);
    if(status)goto audio_done;
    for(i=0;i<declared;i++) {
        const rf_audio_declaration *row=declarations+i;
        status=rf_audio_bank_declare(&campaign_audio_bank,row->name,row->near_distance,row->volume,row->rolloff,&index);
        if(status)goto audio_done;
        /* Original4347f0 requires the registered index to equal the table row. */
        if(index!=i){status=RF_FORMAT;goto audio_done;}
    }
    rf_scene_sound_bank[0]=declared;
    free(declarations);declarations=NULL;
    for(i=0;i<declared;++i)global_hash=npc_hash_bytes(global_hash,&campaign_audio_bank.samples[i].parameters,sizeof(rf_audio_parameters));
    foley_bank_bytes=campaign_audio_bank.bytes;
    status=rf_foley_open(foley_text,foley_entry.size,256*1024,campaign_foley_register,&foley_registration,&campaign_foley);
    if(!status)status=foley_registration.error;if(status)goto audio_done;
    if(campaign_audio_bank.bytes!=foley_bank_bytes){status=RF_FORMAT;goto audio_done;}
    rf_scene_foley[0]=campaign_foley.group_count;rf_scene_foley[1]=campaign_foley.sample_count;
    rf_scene_foley[2]=foley_registration.missing;rf_scene_foley[3]=campaign_foley.resident_bytes;
    rf_scene_foley[4]=campaign_foley.peak_bytes+foley_entry.size;
    rf_scene_foley[5]=campaign_audio_bank.count;rf_scene_foley[6]=2166136261u;
    for(i=0;i<declared;++i)rf_scene_foley[6]=npc_hash_bytes(rf_scene_foley[6],&campaign_audio_bank.samples[i].parameters,sizeof(rf_audio_parameters));
    if(global_hash!=rf_scene_foley[6]){status=RF_FORMAT;goto audio_done;}
    rf_scene_foley[7]=npc_hash_bytes(2166136261u,campaign_foley.samples,campaign_foley.sample_count*4);
    free(foley_text);foley_text=NULL;
    if(campaign_seeds.class_count || player_class) {
        if(campaign_seeds.class_count>640){status=RF_RANGE;goto audio_done;}
        if(campaign_seeds.class_count) {
            campaign_footstep_groups=malloc(campaign_seeds.class_count*sizeof(*campaign_footstep_groups));
            if(!campaign_footstep_groups){status=RF_RANGE;goto audio_done;}
            campaign_pain_groups=malloc(campaign_seeds.class_count*sizeof(*campaign_pain_groups));
            if(!campaign_pain_groups){status=RF_RANGE;goto audio_done;}
            /* First-load class128 starts in zero-filled original static storage. */
            campaign_impact_groups=calloc(campaign_seeds.class_count,sizeof(*campaign_impact_groups));
            if(!campaign_impact_groups){status=RF_RANGE;goto audio_done;}
        }
        status=rf_vpp_find(&tables,"entity.tbl",&entity_entry);if(status)goto audio_done;
        if(!entity_entry.size || entity_entry.size>1024*1024){status=RF_RANGE;goto audio_done;}
        entity_text=malloc(entity_entry.size);if(!entity_text){status=RF_RANGE;goto audio_done;}
        status=rf_vpp_read(&tables,&entity_entry,0,entity_text,entity_entry.size);if(status)goto audio_done;
        for(i=0;i<campaign_seeds.class_count;++i) {
            const char *name=campaign_seeds.records.items[campaign_seeds.classes[i].record_index].record.class_name;
            status=rf_entity_footstep_groups_read(entity_text,entity_entry.size,name,&campaign_foley,campaign_footstep_groups[i]);
            if(status)goto audio_done;
            status=rf_entity_pain_groups_read(entity_text,entity_entry.size,name,&campaign_foley,campaign_pain_groups[i]);
            if(status)goto audio_done;
            status=rf_entity_impact_sound_group_read(entity_text,entity_entry.size,name,&campaign_foley,campaign_impact_groups+i);
            if(status)goto audio_done;
        }
        if(player_class) {
            status=rf_entity_damage_sound_groups_read(entity_text,entity_entry.size,player_class,&campaign_foley,campaign_player_pain_sound.groups);
            if(status)goto audio_done;
        }
        free(entity_text);entity_text=NULL;
        rf_scene_foley[8]=campaign_seeds.class_count;
        rf_scene_foley[9]=npc_hash_bytes(2166136261u,campaign_footstep_groups,campaign_seeds.class_count*sizeof(*campaign_footstep_groups));
        rf_scene_foley[3]+=campaign_seeds.class_count*sizeof(*campaign_footstep_groups);
        rf_scene_npc_pain_groups[0]=campaign_seeds.class_count;
        rf_scene_npc_pain_groups[1]=campaign_seeds.class_count*sizeof(*campaign_pain_groups);
        rf_scene_npc_pain_groups[2]=npc_hash_bytes(2166136261u,campaign_pain_groups,rf_scene_npc_pain_groups[1]);
        rf_scene_foley[3]+=rf_scene_npc_pain_groups[1];
        rf_scene_npc_impact_groups[0]=campaign_seeds.class_count;
        rf_scene_npc_impact_groups[1]=campaign_seeds.class_count*sizeof(*campaign_impact_groups);
        rf_scene_npc_impact_groups[2]=npc_hash_bytes(2166136261u,campaign_impact_groups,rf_scene_npc_impact_groups[1]);
        rf_scene_foley[3]+=rf_scene_npc_impact_groups[1];
        if(rf_scene_foley[3]+entity_entry.size>rf_scene_foley[4])rf_scene_foley[4]=rf_scene_foley[3]+entity_entry.size;
    }
    free(declarations);declarations=NULL;rf_vpp_close(&tables);
    /* All installed section500 records precede section3000 controllers. Their
     * first successful metadata registration must retain parameter precedence.
     * The deterministic port sound bank is enabled even without a host device. */
    status=rf_ambient_instances_open(&campaign_ambient,65536,campaign_ambient_register,
        &campaign_audio_bank,&campaign_ambient_instances);if(status)goto audio_done;
    rf_scene_ambient_instances[0]=campaign_ambient_instances.count;
    rf_scene_ambient_instances[1]=campaign_ambient_instances.rejected;
    rf_scene_ambient_instances[2]=campaign_ambient_instances.allocated_bytes;
    rf_scene_ambient_instances[3]=2166136261u;
    for(i=0;i<campaign_ambient_instances.count*sizeof(*campaign_ambient_instances.items);i++)
        rf_scene_ambient_instances[3]=(rf_scene_ambient_instances[3]^((unsigned char *)campaign_ambient_instances.items)[i])*16777619u;
    if(!status)for(i=0;i<campaign_group_runtime.count;i++)for(j=0;j<4;j++) {
        const char *name=campaign_group_runtime.items[i].source->record.sounds[j];
        if(!name[0])continue;
        int loaded=rf_audio_bank_register(&campaign_audio_bank,name,
            !strcmp(level_name,"L14S2.rfl")?10.0f:5.0f,
            campaign_group_runtime.items[i].source->record.sound_values[j],1.0f,&index);
        if(!loaded)loaded=rf_audio_bank_reload(&campaign_audio_bank,&archive,index);
        if(!loaded)campaign_controller_requests[i].sounds.samples[j]=(int32_t)index;
        else if(loaded==RF_NOT_FOUND)++rf_scene_live_audio[2];
        else ++rf_scene_live_audio[3];
    }
    {
        uint32_t before=campaign_audio_bank.bytes,rejection=0;
        for(i=0;i<campaign_events.count;i++) {
            const rf_runtime_event *event=campaign_events.items+i;int loaded;
            if(!event->switch_state)continue;
            if(event->switch_state->mode==1 || event->switch_state->mode==2)rejection=1;
            /* Original4b83e0 ->5054b0(name,5,1,1), then5054d0(index).
             * Empty names return -1 in543580 and do not preload. */
            if(!event->authored->record.texts[0][0])continue;
            loaded=rf_audio_bank_declare(&campaign_audio_bank,event->authored->record.texts[0],5,1,1,&index);
            if(!loaded)loaded=rf_audio_bank_reload(&campaign_audio_bank,&archive,index);
            if(!loaded)++rf_scene_switch_audio[1];
            else if(loaded==RF_NOT_FOUND)++rf_scene_live_audio[2];
            else ++rf_scene_live_audio[3];
        }
        /* Bounded port residency policy: prepare original4bc520's shared
         * nonspatial rejection slot2 before closing the loading archive. */
        if(rejection) {
            status=rf_audio_bank_reload(&campaign_audio_bank,&archive,2);if(status)goto audio_done;
            rf_scene_switch_audio[2]=1;
        }
        rf_scene_switch_audio[3]=campaign_audio_bank.bytes-before;
    }
    rf_scene_live_audio[0]=campaign_audio_bank.count;rf_scene_live_audio[1]=campaign_audio_bank.bytes;
    for(i=0;i<campaign_forces.count;i++)if(campaign_forces.items[i].flags&0x40) {
        status=rf_audio_bank_reload(&campaign_audio_bank,&archive,0x53);if(status)goto audio_done;
        break;
    }
    rf_scene_live_audio[1]=campaign_audio_bank.bytes;
    for(i=0;i<campaign_audio_bank.count;i++)if(rf_audio_bank_sample(&campaign_audio_bank,i)) {
        ++rf_scene_sound_bank[1];rf_scene_sound_bank[2]+=campaign_audio_bank.samples[i].bytes;
    }
    rf_scene_sound_bank[3]=campaign_audio_bank.bytes-rf_scene_sound_bank[2];
    for(i=0;i<campaign_audio_bank.count;++i) {
        const rf_sound_metadata *metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,
            campaign_audio_metadata.order,campaign_audio_bank.samples[i].name);
        uint32_t a=metadata?metadata->loop_flags:0,b=metadata?metadata->keyoff_flags:0;
        if(metadata) {++rf_scene_sound_metadata[3];rf_scene_sound_metadata[4]+=(a>>30)&1;}
        else ++rf_scene_sound_metadata[5];
        rf_scene_sound_metadata[6]=(rf_scene_sound_metadata[6]^a)*16777619u;
        rf_scene_sound_metadata[6]=(rf_scene_sound_metadata[6]^b)*16777619u;
    }
audio_done:
    free(foley_text);free(entity_text);
    free(declarations);rf_vpp_close(&tables);
    if(!status) {
        campaign_audio_archive=archive;memset(&archive,0,sizeof(archive));
        campaign_audio_bank.archive=&campaign_audio_archive;
    } else campaign_audio_bank.archive=NULL;
    rf_vpp_close(&archive);
    return status;
}
static int32_t campaign_sound_start(int32_t sample,const float position[3],float volume,uint32_t category,uint32_t positional,float pan)
{
    const rf_wave_pcm *pcm;const rf_sound_metadata *metadata;uint32_t handle,looping;int32_t id;
    float gains[2];campaign_spatial_voice *voice;
    (void)category; /* Category settings still default to unity. */
    pcm=sample<0?NULL:rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample);
    if(!pcm)goto failed;
    metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,campaign_audio_metadata.order,
        campaign_audio_bank.samples[sample].name);
    looping=metadata?(metadata->loop_flags>>30)&1u:0;
    /* Same installed static-buffer support as ambient playback. */
    if(looping && metadata && (metadata->loop_flags&0x07ffffffu))goto failed;
    /* Deterministic PCM stays unity/nonspatial, with authored loop mode.
     * The device receives the separate listener-driven gain calculation. */
    if(rf_audio_voice_start(&campaign_audio_mixer,pcm,32768,32768,looping,&handle))goto failed;
    voice=campaign_spatial_voices+(handle&0xffff);
    voice->handle=handle;voice->sample=(uint32_t)sample;voice->volume=volume;voice->positional=positional;
    memcpy(voice->position,position,12);voice->last_volume=INT32_MIN;voice->last_pan=INT32_MIN;
    if(positional)campaign_spatial_update(voice,1);
    else {
        /*505560 -> sample start: category gains currently unity, default
         * sample gain applies once. Flat voices never follow the listener. */
        float gain=rf_audio_sample_gain(campaign_audio_bank.samples[sample].parameters.volume,1,volume);
        voice->last_volume=rf_audio_device_volume(gain,0);voice->last_pan=(int32_t)((double)pan*1000);
    }
    if(rf_audio_device_gains(voice->last_volume,voice->last_pan,gains) ||
       (campaign_audio_events.play && !campaign_audio_events.play_mode) ||
       (campaign_audio_events.play_mode && campaign_audio_events.play_mode(campaign_audio_events_context,
            handle,pcm,gains[0],gains[1],looping))) {
        rf_audio_voice_stop(&campaign_audio_mixer,handle);memset(voice,0,sizeof(*voice));goto failed;
    }
    if(rf_audio_voice_ids_bind(&campaign_device_voice_ids,handle,&id)) {
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
        rf_audio_voice_stop(&campaign_audio_mixer,handle);memset(voice,0,sizeof(*voice));goto failed;
    }
    ++rf_scene_live_audio[4];rf_scene_controller_audio[0]+=looping;return id;
failed:
    ++rf_scene_live_audio[5];return -1;
}
static int32_t campaign_sound_play(void *context,int32_t sample,const float position[3],float volume,uint32_t category)
{
    (void)context;return campaign_sound_start(sample,position,volume,category,1,0);
}
int rf_scene_sound_play_request(const rf_player_sound_request *request,int32_t *voice)
{
    uint32_t i;float pan;int32_t id;
    if(!request || !voice || request->spatial>1)return RF_RANGE;
    if(request->group)return RF_NOT_FOUND;
    memcpy(&pan,&request->pan,4);
    if(!isfinite(request->volume) || (!request->spatial && !isfinite(pan)))return RF_FORMAT;
    if(!request->spatial && (pan<-10 || pan>10))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(request->position[i]))return RF_FORMAT;
    if(request->sound_id<0 || !rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)request->sound_id))return RF_NOT_FOUND;
    id=campaign_sound_start(request->sound_id,request->position,request->volume,request->group,request->spatial,pan);
    if(id<0)return RF_IO;*voice=id;return RF_OK;
}
static void campaign_sound_request(rf_group_runtime_entry *entry,campaign_controller_effects *request,uint32_t effects)
{
    if(effects&RF_GROUP_SOUND_START)rf_group_sound_start(&request->sounds,entry->translation.motion.flags,
        entry->translation.motion.next_key,entry->pose.public_position,campaign_sound_play,NULL);
    if(effects&RF_GROUP_SOUND_END) {
        /* Original 46a0d0: stop retained moving voice, then play arrival slot. */
        if(request->sounds.handles[1]!=-1) {
            uint32_t source;
            if(!rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,request->sounds.handles[1],&source)) {
                rf_scene_controller_audio[1]+=campaign_audio_mixer.voices[source&0xffff].loop!=0;
                rf_audio_voice_stop(&campaign_audio_mixer,source);
                if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,source);
                memset(campaign_spatial_voices+(source&0xffff),0,sizeof(*campaign_spatial_voices));
            }
            request->sounds.handles[1]=-1;
        }
        request->sounds.handles[2]=campaign_sound_play(NULL,request->sounds.samples[2],entry->pose.public_position,1,0);
    }
}
static rf_group_controller_view *campaign_controller_views;
static rf_group_pose_slot campaign_pose_slots[RF_OBJECT_CAPACITY];
uint32_t rf_scene_live_motion[8]; /* ticks, propagated frames, holds, reversals, arrivals, sound requests, unresolved key effects, status */
float rf_scene_live_door_positions[6];
static uint32_t campaign_actor_controller;
uint32_t rf_scene_live_activation[8]; /* fired, controller calls/starts, event calls, pending effects, status, door8593/8591 start frame+1 */
typedef struct campaign_activation_context {int32_t now;uint32_t frame;rf_level_particles *particles;} campaign_activation_context;
static int campaign_link_effect(void *context,uint32_t kind,uint32_t handle,uint32_t source,uint32_t actor)
{
    campaign_activation_context *c=context;int status;
    if(kind==6) {
        rf_startup_events_report report={0};++rf_scene_live_activation[3];
        status=rf_runtime_event_fire(&campaign_triggers,handle,source,actor,c->now,&scene_gravity,c->particles, &campaign_forces,&report);
        rf_scene_live_activation[4]+=report.unsupported_actions+report.other_targets+report.unresolved_targets;return status;
    } else {
        rf_group_registered_controller *controller=rf_object_registry_lookup(&campaign_registry,handle);
        rf_group_runtime_entry *entry;rf_group_activation_actor facts;const rf_entity_view *view;
        campaign_controller_effects *request;uint32_t started;
        if(!controller || controller->object_kind!=8)return RF_NOT_FOUND;
        entry=controller->runtime;if(entry->kind!=RF_GROUP_RUNTIME_TRANSLATION)return RF_FORMAT;
        request=campaign_controller_requests+(entry-campaign_group_runtime.items);
        view=rf_object_lookup(&campaign_entities,(int32_t)actor);
        facts.present=view!=NULL;facts.flags=view?view->flags_7c:0;
        facts.entity_present=rf_entity_lookup(&campaign_entities,(int32_t)actor)!=NULL;
        facts.controller_handle=campaign_actor_controller;++rf_scene_live_activation[1];
        status=rf_group_activation_begin(&entry->translation.motion,entry->source->record.key_count,
            handle,&facts,&started);if(status)return status;
        campaign_actor_controller=facts.controller_handle;
        if(started) {
            /* Retain outstanding alert/wakeup work. Sound dispatch below has
             * a nonspatial PCM adapter; source is needed by occupancy lookup. */
            request->source=source;request->actor=actor;request->pending=1;
            campaign_sound_request(entry,request,RF_GROUP_SOUND_START);
            ++request->starts;request->start_frame=c->frame;++rf_scene_live_activation[2];++rf_scene_live_activation[4];
            if(entry->source->keys[0].uid==8593)rf_scene_live_activation[6]=c->frame+1;
            if(entry->source->keys[0].uid==8591)rf_scene_live_activation[7]=c->frame+1;
        }
        return RF_OK;
    }
}
/* Default player contact path; special ownership/key/script gates stay explicit. */
static int campaign_trigger_contacts(const rf_group_attached_pose *pose,int32_t now,uint32_t frame,rf_level_particles *particles,uint32_t use)
{
    uint32_t i,clock_bits;float positions[3][3],seconds=(float)now*.001f;int32_t player=campaign_player_view.handle;
    campaign_activation_context context={now,frame,particles};memcpy(&clock_bits,&seconds,4);
    rf_trigger_actor_facts facts;rf_trigger_contact_filter filter={0,-1,0,NULL};
    int status=rf_trigger_actor_resolve(&campaign_entities,&campaign_player_view,-1,-1,&player,1,&facts);
    if(status)return status;
    memcpy(positions[0],pose->public_position,12);memcpy(positions[1],pose->position,12);memcpy(positions[2],pose->pending,12);
    for(i=0;i<campaign_triggers.count;++i) {
        rf_runtime_trigger *trigger=campaign_triggers.items+i;
        const rf_level_trigger *record=&trigger->authored->record;uint32_t ready=0,fired=0;
        trigger->state.flags&=~64u; /* 4bf740 per-frame fired reset; deferred trigger stages remain open. */
        if(trigger->activation.object_flags&2)continue;
        if(record->value_byte>4 || record->fields[2]!=UINT32_MAX || record->fields[0]!=UINT32_MAX ||
           record->fields[1]!=UINT32_MAX || record->script[0] ||
           (trigger->state.flags&(2u|128u))) {++rf_scene_trigger_contacts[4];continue;}
        status=rf_trigger_contact_filter_authored(trigger,facts.handle,-1,&filter);if(status)return status;
        status=rf_runtime_trigger_contact(&campaign_triggers,trigger->handle,&facts,positions,&filter,now,use,&ready);
        ++rf_scene_trigger_contacts[0];rf_scene_trigger_contacts[5]=(uint32_t)status;if(status)return status;
        if(ready) {++rf_scene_trigger_contacts[1];rf_scene_trigger_contacts[2]=record->uid;
            if(record->uid==8542)++rf_scene_trigger_contacts[3];
            status=rf_runtime_trigger_fire_links(&campaign_triggers,trigger->handle,(uint32_t)player,now,
                clock_bits,0,0,campaign_link_effect,&context,&fired);
            rf_scene_live_activation[5]=(uint32_t)status;if(status)return status;
            rf_scene_live_activation[0]+=fired;}
    }
    return RF_OK;
}
static int campaign_controller_tick(int32_t now,rf_level_particles *particles,const float player_position[3])
{
    uint32_t i,j;int status;rf_trigger_occupant actor;
    actor.handle=(uint32_t)campaign_player_view.handle;actor.flags=campaign_player_view.flags_7c;
    memcpy(actor.position,player_position,12);
    for(i=0;i<campaign_group_runtime.count;++i) {
        rf_group_runtime_entry *entry=campaign_group_runtime.items+i;
        rf_group_translation_runtime *runtime=&entry->translation;rf_group_translation_frame tick;
        campaign_controller_effects *request=campaign_controller_requests+i;uint32_t occupied=0,sounds=0;
        rf_runtime_trigger *trigger=NULL;
        if(entry->kind!=RF_GROUP_RUNTIME_TRANSLATION)continue;
        status=rf_group_translation_tick_begin(runtime,entry->source->keys,entry->source->record.key_count,1.0f/60,now,&tick);if(status)return status;
        if(tick.stage==RF_GROUP_TICK_GATES) {
            if(request->starts) {
                trigger=rf_object_registry_lookup(&campaign_registry,request->source);
                if(trigger && trigger->object_kind!=5)trigger=NULL;
            }
            if((runtime->motion.flags&2) && runtime->motion.mode!=1) {
                status=rf_trigger_occupancy(trigger?&trigger->volume:NULL,&actor,1,NULL,0,NULL,NULL,&occupied);if(status)return status;
            }
            if(occupied && (runtime->motion.flags&0x2001)==0x2001) {
                double delay=(double)tick.dwell*1000.+.5;
                if(delay<0 || delay>RF_TIMER_PERIOD)return RF_RANGE;
                status=rf_timer_set(&runtime->deadline,now,(int32_t)delay);if(status)return status;
                ++rf_scene_live_motion[2];
            } else if(occupied && tick.step.timing!=0 && !(runtime->motion.flags&1) &&
                      tick.step.distance<tick.progress.length && runtime->motion.next_key==0) {
                status=rf_group_translation_reverse(runtime,entry->source->keys,entry->source->record.key_count);if(status)return status;
                ++rf_scene_live_motion[3];
            } else {
                status=rf_group_translation_tick_move(runtime,&tick);if(status)return status;
                if(tick.stage==RF_GROUP_TICK_ARRIVAL) {
                    const rf_level_group_key *key=entry->source->keys+runtime->motion.current_key;
                    ++rf_scene_live_motion[4];
                    if(key->links[0]!=UINT32_MAX) {
                        for(j=0;j<campaign_events.count;++j)if(campaign_events.items[j].authored->record.uid==key->links[0]) {
                            rf_startup_events_report report={0};uint32_t handle=UINT32_MAX,k;
                            for(k=0;k<campaign_group_registration.count;++k)if(campaign_group_registration.controllers[k].runtime==entry)handle=campaign_group_registration.controllers[k].handle;
                            status=rf_runtime_event_fire(&campaign_triggers,campaign_events.items[j].handle,handle,UINT32_MAX,now,&scene_gravity,particles, &campaign_forces,&report);if(status)return status;
                            rf_scene_live_motion[6]+=report.unsupported_actions+report.unresolved_targets+report.other_targets;break;
                        }
                        if(j==campaign_events.count)++rf_scene_live_motion[6];
                    }
                    /* 46a060 remaining key-link effects have no live backend yet. */
                    for(j=1;j<3;++j)if(key->links[j]!=UINT32_MAX)++rf_scene_live_motion[6];
                    status=rf_group_translation_tick_finish(runtime,&tick,entry->source->record.key_count,&sounds);if(status)return status;
                    if(sounds){campaign_sound_request(entry,request,sounds);++rf_scene_live_motion[5];}
                }
            }
        }
        memcpy(entry->pose.pending,runtime->pending,12);entry->pose.flags=runtime->object_flags;
    }
    status=rf_geometry_collision_movers_propagate(&campaign_movers,campaign_controller_views,campaign_group_runtime.count,1.0f/60,0);if(status)return status;
    ++rf_scene_live_motion[1];
    return RF_OK;
}
/* 487e00 commits controllers through 46a8f0 before querying actor support.
 * Keep propagated velocity available during physics with old committed origins. */
static int campaign_controller_commit(void)
{
    uint32_t i;int status;
    for(i=0;i<campaign_group_runtime.count;++i) {
        rf_group_runtime_entry *entry=campaign_group_runtime.items+i;
        if(entry->kind!=RF_GROUP_RUNTIME_TRANSLATION)continue;
        status=rf_group_commit_positions(&entry->translation.motion.flags,&entry->pose,campaign_controller_views+i,campaign_pose_slots,RF_OBJECT_CAPACITY);if(status)return status;
        memcpy(entry->translation.position,entry->pose.position,12);memcpy(entry->translation.pending,entry->pose.pending,12);
    }
    status=rf_geometry_collision_movers_sync(&campaign_movers);if(status)return status;
    for(i=0;i<campaign_movers.count;++i) {
        if(campaign_movers.uids[i]==8543)memcpy(rf_scene_live_door_positions,campaign_movers.poses[i].position,12);
        if(campaign_movers.uids[i]==8544)memcpy(rf_scene_live_door_positions+3,campaign_movers.poses[i].position,12);
    }
    if(campaign_audio_observer)campaign_audio_observer(campaign_audio_observer_context,&campaign_audio_mixer,&campaign_audio_bank,800);
    {uint32_t before[RF_AUDIO_VOICES];
     for(i=0;i<RF_AUDIO_VOICES;i++) {
        const rf_audio_voice *voice=campaign_audio_mixer.voices+i;
        before[i]=(voice->active && voice->loop && campaign_spatial_voices[i].handle==voice->handle)?voice->frame:UINT32_MAX;
     }
     status=rf_audio_mix(&campaign_audio_mixer,campaign_audio_frame,800);if(status)return status;
     for(i=0;i<RF_AUDIO_VOICES;i++)if(before[i]!=UINT32_MAX) {
        ++rf_scene_controller_audio[2];
        rf_scene_controller_audio[3]+=campaign_audio_mixer.voices[i].frame<before[i];
     }}
    for(i=0;i<sizeof(campaign_audio_frame);i++)rf_scene_live_audio[7]=(rf_scene_live_audio[7]^((const uint8_t *)campaign_audio_frame)[i])*16777619u;
    rf_scene_live_audio[6]+=800;
    if(campaign_audio_sink)campaign_audio_sink(campaign_audio_context,campaign_audio_frame,800);
    if(campaign_audio_events.poll)campaign_audio_events.poll(campaign_audio_events_context);
    ++rf_scene_live_motion[0];return RF_OK;
}
static rf_collision_body_mover *campaign_sweep_scratch;
static const rf_geometry **campaign_surface_sources;
static rf_surface_materials *campaign_surface_palette;
rf_geometry_body_hit rf_scene_actor_body_contact;
uint32_t rf_scene_actor_body_sweeps[5]; /* queries, hits, mover hits, status, retained adapter bytes */
uint32_t rf_scene_campaign_movers[3]; /* registered, owned collision bytes, registration bytes */
static uint32_t npc_hash_bytes(uint32_t hash,const void *data,uint32_t bytes)
{
    const unsigned char *p=data;while(bytes--)hash=(hash^*p++)*16777619u;return hash;
}
static void campaign_npc_materials_digest(void)
{
    const rf_entity_materials *owner=&campaign_npc_materials;uint32_t i,j,x,y,h=2166136261u,p=2166136261u,bytes=0;
    memset(rf_scene_npc_materials,0,sizeof(rf_scene_npc_materials));
    rf_scene_npc_materials[0]=owner->count;rf_scene_npc_materials[1]=owner->materials.count;
    rf_scene_npc_materials[2]=owner->materials.textures.count;
    rf_scene_npc_materials[3]=owner->resident_bytes+campaign_appearances.resident_bytes;
    rf_scene_npc_materials[4]=owner->peak_bytes+campaign_appearances.resident_bytes;
    h=npc_hash_bytes(h,owner->offsets,(owner->count+1)*4);
    h=npc_hash_bytes(h,campaign_appearances.actor_indices,campaign_appearances.actor_count*4);
    for(i=0;i<owner->materials.count;++i) {
        const rf_model_material_instance *item=owner->materials.items+i;
        h=npc_hash_bytes(h,item->record.bytes,200);
        for(j=0;j<3;++j)h=npc_hash_bytes(h,item->arrays[j],item->counts[j]*4);
    }
    for(i=0;i<owner->materials.textures.count;++i) {
        const rf_image *image=&owner->materials.textures.items[i].image;
        p=npc_hash_bytes(p,&image->width,4);p=npc_hash_bytes(p,&image->height,4);
        for(y=0;y<image->height;++y)for(x=0;x<image->width;++x)p=npc_hash_bytes(p,rf_image_pixel(image,x,y),4);
        bytes+=image->bytes;
    }
    rf_scene_npc_materials[5]=h;rf_scene_npc_materials[6]=bytes;rf_scene_npc_materials[7]=p;
}
static int campaign_npc_geometry_digest(void)
{
    uint32_t i,j;float prepared[50][12];uint16_t generations[50];int status;
    memset(rf_scene_npc_geometry,0,sizeof(rf_scene_npc_geometry));
    rf_scene_npc_geometry[0]=campaign_render_models.count;rf_scene_npc_geometry[4]=campaign_render_models.resident_bytes;
    rf_scene_npc_geometry[5]=rf_scene_npc_geometry[6]=2166136261u;
    for(i=0;i<campaign_render_models.count;++i) {
        const rf_entity_render_model *model=campaign_render_models.items+i;
        rf_scene_npc_geometry[1]+=model->file.lod_count;
        rf_scene_npc_geometry[5]=npc_hash_bytes(rf_scene_npc_geometry[5],model->stored,model->bone_count*48);
        for(j=0;j<model->file.lod_count;++j) {
            const rf_model_geometry *g=model->lods+j;
            rf_scene_npc_geometry[2]+=g->vertex_count;rf_scene_npc_geometry[3]+=g->triangle_count;
            rf_scene_npc_geometry[5]=npc_hash_bytes(rf_scene_npc_geometry[5],g->batches,g->batch_count*sizeof(*g->batches));
            rf_scene_npc_geometry[5]=npc_hash_bytes(rf_scene_npc_geometry[5],g->vertices,g->vertex_count*sizeof(*g->vertices));
            rf_scene_npc_geometry[5]=npc_hash_bytes(rf_scene_npc_geometry[5],g->triangles,g->triangle_count*sizeof(*g->triangles));
            rf_scene_npc_geometry[5]=npc_hash_bytes(rf_scene_npc_geometry[5],g->reuse,g->vertex_count*sizeof(*g->reuse));
        }
    }
    for(i=0;i<campaign_poses.count;++i)if(campaign_poses.items[i].skeleton!=UINT32_MAX) {
        const rf_entity_pose *p=campaign_poses.items+i;const rf_entity_render_model *m=campaign_render_models.items+p->skeleton;
        if(p->bone_count!=m->bone_count || p->bone_count>50)return RF_RANGE;
        memset(prepared,0,sizeof(prepared));memset(generations,0,sizeof(generations));
        status=rf_model_prepare_skinning(m->stored,p->matrices,p->bone_count,(uint16_t)p->playback.generation,prepared,generations,50);if(status)return status;
        rf_scene_npc_geometry[6]=npc_hash_bytes(rf_scene_npc_geometry[6],prepared,p->bone_count*48);
    }
    return RF_OK;
}
static const float campaign_identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
static rf_movement_descriptor campaign_modes[16];
_Static_assert(sizeof(rf_entity_damage_state)==56,"Damage owner telemetry layout");
typedef struct campaign_npc_body {
    rf_physics_body body;rf_physics_support_contact support;
    rf_collision_contact_extra collision_contact;
    uint32_t collision_material; /* Original object1fc, factory parameter+10. */
    float support_velocity[3]; /* Original422f35..422f3b clears actor8a0. */
    rf_entity_damage_state damage;uint32_t object_flags,field_840;
    struct {int32_t item_82c,requested_83c,action_824,linked_146c,deadline_4b8;uint32_t model_148c;} death;
    uint32_t death_bone_words[2]; /* actor1464/1468 */
    float command_714[3]; /* Constructor422eaf..422ed3 clears this movement vector. */
    float model_radius_78; /* Original489fe0 model-origin radius. */
    float published[3],previous[3];uint32_t movement_slot;
    const float *movement_orientation; /* Original actor85c, borrowed stable matrix. */
    uint32_t trigger_handle; /* Original entity+838; initialized by422360. */
    struct {int32_t ai_timer,animation_lock,cooldown,selected_action;} pain; /*514/744/830/828*/
    struct {int32_t deadline,voice;} pain_sound; /*1458/808; separate from damage.effects.voice(854)*/
    float eye_position[3]; /* Original entity+7d4. */
    rf_movement_settings movement;rf_entity_view view;rf_registered_entity_view registration;
} campaign_npc_body;
static campaign_npc_body *campaign_npc_bodies;
static rf_movement_config *campaign_npc_movement_configs;
static rf_physics_stance_cache *campaign_npc_stances;
typedef struct campaign_npc_eye_class {int32_t tag,parent;float local[12],offsets[6];} campaign_npc_eye_class;
static campaign_npc_eye_class *campaign_npc_eyes;
uint32_t rf_scene_npc_eyes[4]; /* refreshed actors, retained bytes, class hash, position hash */
static uint32_t campaign_npc_body_count;
static int campaign_npc_eye_update(uint32_t actor)
{
    rf_eye_input input={0};float tag[12],placed[12];int status;
    rf_entity_pose *pose;const campaign_npc_eye_class *eye;campaign_npc_body *owner;uint32_t cls;
    if(actor>=campaign_npc_body_count || actor>=campaign_poses.count || !campaign_npc_eyes)return RF_RANGE;
    status=campaign_actor_pose(actor,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    owner=campaign_npc_bodies+actor;cls=campaign_seeds.items[actor].class_index;
    if(cls>=campaign_seeds.class_count)return RF_RANGE;eye=campaign_npc_eyes+cls;
    memcpy(input.position,owner->published,12);
    memcpy(input.orientation,campaign_seeds.records.items[actor].record.orientation,36);
    memcpy(input.standing_offset,eye->offsets,12);memcpy(input.crouching_offset,eye->offsets+3,12);
    input.flags=campaign_seeds.classes[cls].physics.flags2;input.eye_tag=eye->tag;
    input.current_state=pose->controller.current;input.previous_state=pose->controller.next;
    input.transition_duration=pose->controller.duration;input.transition_elapsed=pose->controller.elapsed;
    status=rf_eye_position(&input,owner->eye_position);
    if(status!=RF_NOT_FOUND)return status;
    if(eye->parent<0 || (uint32_t)eye->parent>=pose->bone_count)return RF_RANGE;
    status=rf_model_compose_transform(eye->local,pose->matrices[eye->parent],tag);
    if(!status)status=rf_model_place_tag(tag,input.orientation[0],input.position,placed);
    if(!status)memcpy(owner->eye_position,placed+9,12);return status;
}
uint32_t rf_scene_npc_support[12]; /* sampled, queries, skipped, misses, steep, static, moving, errors, hash, first error UID, first miss UID, corrected */
uint32_t rf_scene_npc_support_first_miss[16];
uint32_t rf_scene_npc_support_deep[8],rf_scene_npc_support_deep_first[20];
static void campaign_npc_support_probe(const rf_geometry_collision_world *world,const campaign_npc_body *owner,
    const rf_entity_physics_config *config,float class_speed,uint32_t uid);
uint32_t rf_scene_npc_damage_owners[3]; /* registered damage records, added owner bytes, state hash */
uint32_t rf_scene_npc_death_owners[3]; /* registered, added bytes, initial state hash */
uint32_t rf_scene_npc_pain_owners[4]; /* registered, added bytes, initial state hash, construction clock */
uint32_t rf_scene_npc_pain_sound_owners[4]; /* registered, added bytes, initial state hash, construction clock */
uint32_t rf_scene_npc_registration[6]; /* registered, view/wrapper bytes, hash, first/last handle, validated */
uint32_t rf_scene_npc_bodies[6]; /* actors, bodies, spheres, resident, peak, content hash */
static void campaign_npc_bodies_close(void)
{
    uint32_t i;for(i=0;i<campaign_npc_body_count;++i) {
        if(campaign_npc_bodies[i].registration.view)
            (void)rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_npc_bodies[i].registration);
        rf_physics_body_close(&campaign_npc_bodies[i].body);
    }
    free(campaign_npc_bodies);campaign_npc_bodies=NULL;campaign_npc_body_count=0;
    free(campaign_npc_movement_configs);campaign_npc_movement_configs=NULL;
    free(campaign_npc_stances);campaign_npc_stances=NULL;
    free(campaign_npc_eyes);campaign_npc_eyes=NULL;
}
static int campaign_npc_bodies_open(const char *tables_path,const rf_geometry_collision_world *world,int32_t now)
{
    const uint32_t budget=512*1024;rf_vpp tables;rf_vpp_entry entity_table,materials;
    uint32_t cls,actor,first,scratch,hash=2166136261u;uint64_t bytes;int status;
    if(campaign_npc_bodies || campaign_npc_body_count || campaign_npc_movement_configs || campaign_npc_stances || campaign_npc_eyes ||
       now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    status=rf_vpp_open(&tables,tables_path);if(status)return status;
    status=rf_vpp_find(&tables,"entity.tbl",&entity_table);if(status)goto done;
    status=rf_vpp_find(&tables,"materials.tbl",&materials);if(status)goto done;
    scratch=entity_table.size>materials.size?entity_table.size:materials.size;
    bytes=(uint64_t)campaign_poses.count*sizeof(*campaign_npc_bodies)+
        (uint64_t)campaign_seeds.class_count*(sizeof(*campaign_npc_movement_configs)+sizeof(*campaign_npc_stances)+sizeof(*campaign_npc_eyes));
    if(bytes+scratch>budget){status=RF_RANGE;goto done;}
    campaign_npc_bodies=calloc(campaign_poses.count,sizeof(*campaign_npc_bodies));
    if(campaign_poses.count && !campaign_npc_bodies){status=RF_RANGE;goto done;}
    campaign_npc_movement_configs=calloc(campaign_seeds.class_count,sizeof(*campaign_npc_movement_configs));
    if(campaign_seeds.class_count && !campaign_npc_movement_configs){status=RF_RANGE;goto done;}
    campaign_npc_stances=calloc(campaign_seeds.class_count,sizeof(*campaign_npc_stances));
    if(campaign_seeds.class_count && !campaign_npc_stances){status=RF_RANGE;goto done;}
    campaign_npc_eyes=calloc(campaign_seeds.class_count,sizeof(*campaign_npc_eyes));
    if(campaign_seeds.class_count && !campaign_npc_eyes){status=RF_RANGE;goto done;}
    memset(rf_scene_npc_eyes,0,sizeof(rf_scene_npc_eyes));
    rf_scene_npc_eyes[1]=campaign_poses.count*12+campaign_seeds.class_count*sizeof(*campaign_npc_eyes);
    campaign_npc_body_count=campaign_poses.count;memset(rf_scene_npc_bodies,0,sizeof(rf_scene_npc_bodies));
    memset(rf_scene_npc_support_deep,0,sizeof(rf_scene_npc_support_deep));rf_scene_npc_support_deep[6]=2166136261u;
    memset(rf_scene_npc_support_deep_first,0,sizeof(rf_scene_npc_support_deep_first));
    memset(rf_scene_npc_support_first_miss,0,sizeof(rf_scene_npc_support_first_miss));
    memset(rf_scene_npc_support,0,sizeof(rf_scene_npc_support));rf_scene_npc_support[8]=2166136261u;
    rf_scene_npc_support[9]=rf_scene_npc_support[10]=UINT32_MAX;
    rf_scene_npc_bodies[0]=campaign_poses.count;rf_scene_npc_bodies[3]=(uint32_t)bytes;
    for(cls=0;cls<campaign_seeds.class_count;++cls) {
        campaign_npc_eye_class *eye_class=campaign_npc_eyes+cls;rf_model_attachment eye={0};uint32_t tag_index;
        rf_entity_physics_config config;rf_physics_sphere spheres[8];uint32_t count=0;
        float model_sphere[4],model_radius;
        rf_entity_movement_values movement_values;rf_movement_config *movement=campaign_npc_movement_configs+cls;
        const rf_entity_pose *pose;const rf_entity_render_model *model;
        for(first=0;first<campaign_poses.count;++first)if(campaign_seeds.items[first].class_index==cls)break;
        if(first==campaign_poses.count){status=RF_FORMAT;goto done;}
        eye_class->tag=eye_class->parent=-1;
        pose=campaign_poses.items+first;if(pose->skeleton==UINT32_MAX)continue;
        if(pose->skeleton>=campaign_render_models.count){status=RF_RANGE;goto done;}
        model=campaign_render_models.items+pose->skeleton;
        status=rf_model_file_bound_sphere(&model->file,model_sphere);if(status)goto done;
        status=rf_model_origin_radius(model_sphere,&model_radius);if(status)goto done;
        if(bytes+scratch>budget){status=RF_RANGE;goto done;}
        if(bytes+scratch>rf_scene_npc_bodies[4])rf_scene_npc_bodies[4]=(uint32_t)bytes+scratch;
        status=rf_entity_physics_config_load(&tables,campaign_seeds.records.items[first].record.class_name,
            budget-(uint32_t)bytes,&config);if(status)goto done;
        status=rf_entity_movement_load(&tables,campaign_seeds.records.items[first].record.class_name,
            budget-(uint32_t)bytes,&movement_values);if(status)goto done;
        movement->flags=config.authored.flags;movement->base_speed=movement_values.speed;
        movement->slow_factor=movement_values.slow_factor;movement->alternate_factor=movement_values.fast_factor;
        movement->response=movement_values.acceleration; /* SP ignores network overrides. */
        /* Shared class geometry comes from the first authored startup actor. */
        status=rf_entity_class_spheres_build(&model->file,pose->matrices,pose->bone_count,&config,spheres,&count);if(status)goto done;
        for(tag_index=0;tag_index<model->file.lods[0].attachment_count;++tag_index) {
            status=rf_model_file_attachment(&model->file,0,tag_index,&eye);if(status)goto done;
            if((eye.name[0]=='e' || eye.name[0]=='E') && (eye.name[1]=='y' || eye.name[1]=='Y') &&
               (eye.name[2]=='e' || eye.name[2]=='E') && !eye.name[3]) {
                float tag[12];
                if(eye.parent<0 || (uint32_t)eye.parent>=pose->bone_count){status=RF_RANGE;goto done;}
                eye_class->tag=(int32_t)tag_index;eye_class->parent=eye.parent;
                status=rf_model_attachment_transform(eye.rotation,eye.position,eye_class->local);if(status)goto done;
                status=rf_model_compose_transform(eye_class->local,pose->matrices[eye.parent],tag);if(status)goto done;
                memcpy(eye_class->offsets,tag+9,12);
                if(config.authored.flags&0x20000u)eye_class->offsets[0]=eye_class->offsets[2]=0;
                break;
            }
        }
        /* Original428010: either crouch state8 or crouch-walk state9 exists. */
        if(campaign_motion_catalog.mappings[cls].states[8]!=-1 || campaign_motion_catalog.mappings[cls].states[9]!=-1) {
            const rf_entity_model_motions *motions=campaign_motion_catalog.models+pose->skeleton;
            const rf_entity_playback_model *playback=campaign_playback_resources.models+pose->skeleton;
            const rf_motion_file **handles;uint32_t i;
            uint64_t temporary=(uint64_t)motions->count*(sizeof(*handles)+sizeof(*playback->resources))+
                (uint64_t)pose->bone_count*48;
            if(motions->count!=playback->count || bytes+temporary>budget){status=RF_RANGE;goto done;}
            if(bytes+temporary>rf_scene_npc_bodies[4])rf_scene_npc_bodies[4]=(uint32_t)(bytes+temporary);
            handles=motions->count?malloc(motions->count*sizeof(*handles)):NULL;
            if(motions->count && !handles){status=RF_IO;goto done;}
            for(i=0;i<motions->count;++i)handles[i]=&motions->items[i].file;
            /* Sample once per class on private playback; never advance a live NPC. */
            status=rf_entity_class_stance_build(&model->file,campaign_skeletons.items[pose->skeleton].bones,
                pose->bone_count,&pose->playback,handles,playback->resources,playback->count,
                campaign_motion_catalog.mappings[cls].states[8],&config,spheres,count,campaign_npc_stances+cls,
                eye_class->tag==-1?NULL:&eye,eye_class->local,eye_class->tag==-1?NULL:eye_class->offsets,
                budget-(uint32_t)bytes-motions->count*(uint32_t)sizeof(*handles));
            free(handles);if(status)goto done;
        }
        for(actor=first;actor<campaign_poses.count;++actor)if(campaign_seeds.items[actor].class_index==cls) {
            const rf_level_entity *record=&campaign_seeds.records.items[actor].record;
            rf_physics_body *body=&campaign_npc_bodies[actor].body;
            if(bytes+(uint64_t)count*sizeof(*spheres)>budget){status=RF_RANGE;goto done;}
            status=rf_entity_body_open(&config,spheres,count,record->position,record->orientation[0],
                campaign_seeds.items[actor].spawn.creation_flags,
                budget-(uint32_t)bytes+(uint32_t)sizeof(*body),body);if(status)goto done;
            {
                campaign_npc_body *owner=campaign_npc_bodies+actor;
                owner->model_radius_78=model_radius;
                owner->collision_material=config.material.index;
                const rf_entity_seed_class *definition=campaign_seeds.classes+cls;
                uint32_t flags=rf_entity_creation_object_flags(campaign_seeds.items[actor].spawn.creation_flags,definition->model_kind);
                /* Generic486da0 factory, then422ba0 class flag and creation vitals.
                 * Later script/AI mutations are not synthesized here. */
                if(flags&0x4000u)flags|=0x8000u;
                flags|=0x06000000u;if(!(config.authored.flags2&1u))flags|=0x20000u;
                rf_entity_creation_vitals_state vitals={0};vitals.object_flags=flags;
                rf_entity_creation_vitals(&vitals,&definition->vitals,0);
                owner->object_flags=vitals.object_flags;owner->field_840=vitals.field_840;
                owner->damage.effects.health=vitals.health;owner->damage.effects.armor=vitals.armor;
                owner->damage.effects.class_health=definition->vitals.health;
                owner->damage.effects.class_armor=definition->vitals.armor;
                owner->damage.effects.class_flags_728=definition->physics.flags2;
                owner->damage.effects.affiliation=campaign_seeds.items[actor].spawn.friendliness;
                owner->damage.effects.voice=UINT32_MAX;owner->damage.responsible_handle=UINT32_MAX;
                owner->damage.burn_source=UINT32_MAX; /* Absent burn; source unused until installed. */
                owner->movement_slot=rf_movement_start(campaign_modes,(int32_t)config.authored.movement_index,&body->state.flags);
                owner->movement_orientation=campaign_identity[0]; /* Constructor422360 installs original73a858. */
                owner->movement.response=body->state.coefficients[1];
                /* Constructor422e19 requests normal speed via427450. */
                status=rf_movement_set_mode(&owner->movement,movement,1,-1,body->state.mass,0);if(status)goto done;
                body->state.coefficients[1]=owner->movement.response;
                memcpy(owner->published,record->position,12);memcpy(owner->previous,record->position,12);
                status=campaign_npc_eye_update(actor);if(status)goto done;
            }
            campaign_npc_support_probe(world,campaign_npc_bodies+actor,&config,movement->base_speed,(uint32_t)record->uid);
            /* Constructor surface1380=0; support handle remains creation-zero.
             * No contact has been queried or accepted for these bodies yet. */
            bytes+=(uint64_t)count*sizeof(*spheres);++rf_scene_npc_bodies[1];rf_scene_npc_bodies[2]+=count;
        }
    }
    /* Register after class sampling so handles follow serialized actor order,
     * not the class-grouped allocation loop. This is the port registry's current
     * setup order; original global factory ordering is still separate. */
    memset(rf_scene_npc_registration,0,sizeof(rf_scene_npc_registration));rf_scene_npc_registration[2]=2166136261u;
    rf_scene_npc_registration[3]=rf_scene_npc_registration[4]=UINT32_MAX;
    rf_scene_npc_registration[1]=campaign_npc_body_count*(sizeof(rf_entity_view)+sizeof(rf_registered_entity_view));
    for(actor=0;actor<campaign_npc_body_count;++actor)if(campaign_poses.items[actor].skeleton!=UINT32_MAX) {
        campaign_npc_body *owner=campaign_npc_bodies+actor;rf_entity_view *view=&owner->view;
        const rf_entity_seed_class *definition=campaign_seeds.classes+campaign_seeds.items[actor].class_index;
        owner->trigger_handle=UINT32_MAX;
        /*423367..423385, constructor40e380 timer, and SP423af2. */
        owner->death.item_82c=owner->death.requested_83c=owner->death.action_824=-1;
        owner->death.linked_146c=owner->death.deadline_4b8=-1;owner->death.model_148c=0;
        /* Original402c33..402d68 and423318..4233a8: expired at creation,
         * not disabled. Later AI/pain code owns changes to these deadlines. */
        status=rf_timer_set(&owner->pain.ai_timer,now,0);if(status)goto done;
        status=rf_timer_set(&owner->pain.animation_lock,now,0);if(status)goto done;
        status=rf_timer_set(&owner->pain.cooldown,now,0);if(status)goto done;
        owner->pain.selected_action=-1;
        status=rf_timer_set(&owner->pain_sound.deadline,now,0);if(status)goto done;
        owner->pain_sound.voice=-1;
        view->handle=-1;view->type=0;view->class_type=(int32_t)definition->physics.use_kind;
        view->flags_7c=owner->object_flags;view->linked_handle=-1;
        view->weapons[0]=view->weapons[1]=-1;view->base_speed=campaign_npc_movement_configs[campaign_seeds.items[actor].class_index].base_speed;
        /* Action/810/7d0 start clear; weapon/attachment owners remain absent in
         * this existing unarmed, unlinked startup projection. */
        status=rf_entity_view_register(&campaign_registry,&campaign_entities,view,&owner->registration);if(status)goto done;
        owner->damage.effects.handle=owner->registration.handle;
        if(!rf_scene_npc_registration[0])rf_scene_npc_registration[3]=owner->registration.handle;
        rf_scene_npc_registration[4]=owner->registration.handle;++rf_scene_npc_registration[0];
        if(rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration ||
           rf_entity_lookup(&campaign_entities,view->handle)!=view ||
           rf_entity_lookup(&campaign_entities,(int32_t)(owner->registration.handle^0x10000u))){status=RF_FORMAT;goto done;}
        ++rf_scene_npc_registration[5];
        rf_scene_npc_registration[2]=npc_hash_bytes(rf_scene_npc_registration[2],&campaign_seeds.records.items[actor].record.uid,4);
        rf_scene_npc_registration[2]=npc_hash_bytes(rf_scene_npc_registration[2],view,44);
        rf_scene_npc_registration[2]=npc_hash_bytes(rf_scene_npc_registration[2],&owner->registration,8);
    }
    rf_scene_npc_death_owners[0]=0;rf_scene_npc_death_owners[1]=campaign_npc_body_count*sizeof(campaign_npc_bodies[0].death);
    rf_scene_npc_death_owners[2]=2166136261u;
    rf_scene_npc_damage_owners[0]=0;rf_scene_npc_damage_owners[1]=campaign_npc_body_count*48;
    rf_scene_npc_damage_owners[2]=2166136261u;
    rf_scene_npc_pain_owners[0]=0;rf_scene_npc_pain_owners[1]=campaign_npc_body_count*sizeof(campaign_npc_bodies[0].pain);
    rf_scene_npc_pain_owners[2]=2166136261u;rf_scene_npc_pain_owners[3]=(uint32_t)now;
    rf_scene_npc_pain_sound_owners[0]=0;rf_scene_npc_pain_sound_owners[1]=campaign_npc_body_count*sizeof(campaign_npc_bodies[0].pain_sound);
    rf_scene_npc_pain_sound_owners[2]=2166136261u;rf_scene_npc_pain_sound_owners[3]=(uint32_t)now;
    for(actor=0;actor<campaign_npc_body_count;++actor)if(campaign_npc_bodies[actor].registration.view) {
        campaign_npc_body *owner=campaign_npc_bodies+actor;
        rf_scene_npc_death_owners[2]=npc_hash_bytes(rf_scene_npc_death_owners[2],&owner->death,sizeof(owner->death));
        ++rf_scene_npc_death_owners[0];
        rf_scene_npc_damage_owners[2]=npc_hash_bytes(rf_scene_npc_damage_owners[2],&owner->damage,sizeof(owner->damage));
        ++rf_scene_npc_damage_owners[0];
        rf_scene_npc_pain_owners[2]=npc_hash_bytes(rf_scene_npc_pain_owners[2],&owner->pain,sizeof(owner->pain));
        ++rf_scene_npc_pain_owners[0];
        rf_scene_npc_pain_sound_owners[2]=npc_hash_bytes(rf_scene_npc_pain_sound_owners[2],&owner->pain_sound,sizeof(owner->pain_sound));
        ++rf_scene_npc_pain_sound_owners[0];
    }
    rf_scene_npc_bodies[3]=(uint32_t)bytes;
    if(bytes>rf_scene_npc_bodies[4])rf_scene_npc_bodies[4]=(uint32_t)bytes;
    for(actor=0;actor<campaign_npc_body_count;++actor) {
        const rf_physics_body *body=&campaign_npc_bodies[actor].body;
        hash=npc_hash_bytes(hash,&body->state,sizeof(body->state));
        hash=npc_hash_bytes(hash,body->spheres.items,body->spheres.count*sizeof(*body->spheres.items));
        hash=npc_hash_bytes(hash,&campaign_npc_bodies[actor].support,sizeof(campaign_npc_bodies[actor].support));
        { /* Preserve the existing constructor-vitals digest across ownership changes. */
            const campaign_npc_body *owner=campaign_npc_bodies+actor;
            rf_entity_creation_vitals_state vitals={owner->damage.effects.health,owner->damage.effects.armor,owner->object_flags,owner->field_840};
            hash=npc_hash_bytes(hash,&vitals,sizeof(vitals));
        }
        hash=npc_hash_bytes(hash,&campaign_npc_bodies[actor].model_radius_78,4);
        hash=npc_hash_bytes(hash,campaign_npc_bodies[actor].published,12);
        hash=npc_hash_bytes(hash,campaign_npc_bodies[actor].previous,12);
        hash=npc_hash_bytes(hash,&campaign_npc_bodies[actor].movement_slot,4);
        hash=npc_hash_bytes(hash,&campaign_npc_bodies[actor].movement,sizeof(campaign_npc_bodies[actor].movement));
    }
    hash=npc_hash_bytes(hash,campaign_npc_movement_configs,campaign_seeds.class_count*sizeof(*campaign_npc_movement_configs));
    hash=npc_hash_bytes(hash,campaign_npc_stances,campaign_seeds.class_count*sizeof(*campaign_npc_stances));
    rf_scene_npc_bodies[5]=hash;status=RF_OK;
    rf_scene_npc_eyes[2]=npc_hash_bytes(2166136261u,campaign_npc_eyes,campaign_seeds.class_count*sizeof(*campaign_npc_eyes));
    rf_scene_npc_eyes[3]=2166136261u;
    for(actor=0;actor<campaign_npc_body_count;++actor)if(campaign_npc_bodies[actor].registration.view) {
        ++rf_scene_npc_eyes[0];rf_scene_npc_eyes[3]=npc_hash_bytes(rf_scene_npc_eyes[3],campaign_npc_bodies[actor].eye_position,12);
    }
done:
    rf_vpp_close(&tables);if(status)campaign_npc_bodies_close();return status;
}
static void campaign_close_movers(void)
{
    campaign_models_close();
    campaign_npc_bodies_close();
    if(campaign_playback_resources.models) {
        uint32_t actor;for(actor=0;actor<campaign_poses.count;++actor)if(campaign_poses.items[actor].skeleton!=UINT32_MAX)
            (void)rf_entity_pose_release(campaign_poses.items+actor,&campaign_playback_resources);
    }
    rf_entity_seeds_close(&campaign_seeds);
    rf_entity_poses_close(&campaign_poses);
    if(campaign_npc_motion_data){uint32_t i;for(i=0;i<campaign_npc_motion_count;++i)free(campaign_npc_motion_data[i]);free(campaign_npc_motion_data);}
    free(campaign_npc_motion_sizes);campaign_npc_motion_sizes=NULL;
    campaign_npc_motion_data=NULL;campaign_npc_motion_count=campaign_npc_motion_bytes=0;
    rf_entity_materials_close(&campaign_npc_materials);
    rf_entity_appearances_close(&campaign_appearances);
    free(campaign_model_query_scratch);campaign_model_query_scratch=NULL;
    rf_entity_collision_models_close(&campaign_collision_models);
    rf_entity_render_models_close(&campaign_render_models);
    rf_entity_playback_resources_close(&campaign_playback_resources);
    rf_entity_motion_catalog_close(&campaign_motion_catalog);
    rf_entity_base_motions_close(&campaign_base_motions);
    rf_entity_skeletons_close(&campaign_skeletons);
    memset(campaign_spatial_voices,0,sizeof(campaign_spatial_voices));
    if(campaign_audio_events.reset)campaign_audio_events.reset(campaign_audio_events_context);
    rf_audio_mixer_init(&campaign_audio_mixer);
    free(campaign_footstep_groups);campaign_footstep_groups=NULL;rf_foley_close(&campaign_foley);
    free(campaign_pain_groups);campaign_pain_groups=NULL;
    free(campaign_impact_groups);campaign_impact_groups=NULL;
    rf_audio_bank_close(&campaign_audio_bank);
    memset(campaign_audio_evictable,0,sizeof(campaign_audio_evictable));
    rf_vpp_close(&campaign_audio_archive);
    rf_sound_metadata_close(&campaign_audio_metadata);
    rf_ambient_instances_close(&campaign_ambient_instances);
    if(campaign_player_object.view)rf_entity_view_unregister(&campaign_registry,&campaign_entities,&campaign_player_object);
    uint32_t i;for(i=0;i<campaign_mover_count;i++)rf_object_registry_remove(&campaign_registry,campaign_mover_wrappers[i].handle);
    rf_group_mover_memberships_close(&campaign_memberships);
    free(campaign_controller_requests);campaign_controller_requests=NULL;
    free(campaign_controller_views);campaign_controller_views=NULL;
    free(campaign_mover_bindings);campaign_mover_bindings=NULL;
    free(campaign_mover_wrappers);free(campaign_mover_objects);
    campaign_mover_wrappers=NULL;campaign_mover_objects=NULL;campaign_mover_count=0;
    rf_geometry_collision_movers_close(&campaign_movers);
    free(campaign_sweep_scratch);campaign_sweep_scratch=NULL;
    free(campaign_surface_sources);campaign_surface_sources=NULL;
    free(campaign_surface_palette);campaign_surface_palette=NULL;
}
static int campaign_open_movers(const rf_geometry_movers *source)
{
    uint32_t i,*handles=NULL;int status=RF_RANGE;
    if(!source || source->count>campaign_registry.count)return RF_RANGE;
    if(!source->count) {memset(rf_scene_campaign_movers,0,sizeof(rf_scene_campaign_movers));return RF_OK;}
    campaign_mover_wrappers=calloc(source->count,sizeof(*campaign_mover_wrappers));
    campaign_mover_objects=calloc(source->count,sizeof(*campaign_mover_objects));
    campaign_mover_bindings=calloc(source->count,sizeof(*campaign_mover_bindings));
    handles=malloc(source->count*sizeof(*handles));
    if(!campaign_mover_wrappers || !campaign_mover_objects || !campaign_mover_bindings || !handles)goto failed;
    for(i=0;i<source->count;i++) {
        rf_group_registered_mover *m=campaign_mover_wrappers+i;m->object_kind=9;
        status=rf_object_registry_insert(&campaign_registry,m,&m->handle);if(status)goto failed;
        ++campaign_mover_count;handles[i]=m->handle;
    }
    status=rf_geometry_collision_movers_open(source,handles,1024*1024,&campaign_movers);if(status)goto failed;
    for(i=0;i<source->count;i++) {
        campaign_mover_wrappers[i].pose=campaign_movers.poses+i;
        campaign_mover_objects[i].uid=(uint32_t)campaign_movers.uids[i];
        campaign_mover_objects[i].handle=handles[i];campaign_mover_objects[i].flags=campaign_movers.poses[i].flags;
        campaign_mover_bindings[i]=(rf_group_object){campaign_movers.uids[i],9,handles[i],UINT32_MAX,campaign_movers.poses[i].flags};
    }
    rf_scene_campaign_movers[0]=campaign_mover_count;rf_scene_campaign_movers[1]=campaign_movers.allocated_bytes;
    rf_scene_campaign_movers[2]=campaign_mover_count*(sizeof(*campaign_mover_wrappers)+sizeof(*campaign_mover_objects)+sizeof(*campaign_mover_bindings));
    free(handles);return RF_OK;
 failed:
    free(handles);campaign_close_movers();return status;
}

static int campaign_bind_movers(void)
{
    uint32_t i,j,*handles=NULL,hash=2166136261u,links=0;int status;
    if(campaign_group_runtime.count) {
        handles=malloc(campaign_group_runtime.count*4);if(!handles)return RF_RANGE;
        for(i=0;i<campaign_group_runtime.count;i++)handles[i]=UINT32_MAX;
    }
    for(i=0;i<campaign_group_registration.count;i++) {
        const rf_group_registered_controller *c=campaign_group_registration.controllers+i;
        handles[c->runtime-campaign_group_runtime.items]=c->handle;
    }
    status=rf_group_mover_memberships_open(&campaign_group_runtime,campaign_mover_bindings,campaign_mover_count,
        handles,0,256*1024,&campaign_memberships);free(handles);if(status)return status;
    for(i=0;i<campaign_mover_count;i++) {
        rf_group_object *o=campaign_mover_bindings+i;uint32_t words[5]={(uint32_t)o->uid,o->type,o->handle,o->parent,o->flags};
        campaign_movers.poses[i].flags=o->flags;campaign_mover_objects[i].flags=o->flags;
        for(j=0;j<5;j++)hash=(hash^words[j])*16777619u;
    }
    for(i=0;i<campaign_memberships.count;i++) {
        const rf_group_mover_membership *m=campaign_memberships.items+i;uint32_t sign;
        memcpy(&sign,&m->rotation_sign,4);hash=(hash^m->count)*16777619u;hash=(hash^sign)*16777619u;
        for(j=0;j<m->count;j++)hash=(hash^m->handles[j])*16777619u;links+=m->count;
    }
    rf_scene_campaign_memberships[0]=campaign_memberships.count;rf_scene_campaign_memberships[1]=links;
    rf_scene_campaign_memberships[2]=campaign_memberships.allocated_bytes;rf_scene_campaign_memberships[3]=campaign_memberships.peak_bytes;
    rf_scene_campaign_memberships[4]=hash;
    campaign_controller_views=calloc(campaign_group_runtime.count?campaign_group_runtime.count:1,sizeof(*campaign_controller_views));
    if(!campaign_controller_views)return RF_RANGE;
    memset(campaign_pose_slots,0,sizeof(campaign_pose_slots));memset(rf_scene_live_motion,0,sizeof(rf_scene_live_motion));
    memset(rf_scene_live_door_positions,0,sizeof(rf_scene_live_door_positions));
    for(i=0;i<campaign_group_runtime.count;++i) {
        campaign_controller_views[i].runtime=&campaign_group_runtime.items[i].translation;
        campaign_controller_views[i].first_key=campaign_group_runtime.items[i].source->keys;
        campaign_controller_views[i].mover_handles=campaign_memberships.items[i].handles;
        campaign_controller_views[i].mover_count=campaign_memberships.items[i].count;
        if(campaign_group_runtime.items[i].kind!=RF_GROUP_RUNTIME_TRANSLATION) {
            if(campaign_controller_views[i].mover_count)++rf_scene_live_motion[6];
            campaign_controller_views[i].mover_count=0; /* Rotation has no valid translation contribution. */
        }
        if(campaign_group_runtime.items[i].source->record.ids_count[0])++rf_scene_live_motion[6];
    }
    for(i=0;i<campaign_mover_count;++i) {
        uint32_t handle=campaign_mover_wrappers[i].handle;
        campaign_pose_slots[handle&0xffffu].handle=handle;campaign_pose_slots[handle&0xffffu].pose=campaign_movers.poses+i;
    }
    return RF_OK;
}
uint32_t rf_scene_campaign_groups[5]; /* controllers, keys, source/runtime/registration bytes */
rf_startup_events_report rf_scene_startup_events;
uint32_t rf_scene_startup_gravity[4];
uint32_t rf_scene_campaign_triggers[2]; /* registered triggers, owner bytes */
uint32_t rf_scene_campaign_links[4]; /* total, resolved, unresolved, ordered target hash */
uint32_t rf_scene_campaign_event_links[4];
uint32_t rf_scene_event_ticks[12]; /* ticks, clock ms, pending other types, cumulative action report */
/* Read-only diagnostic access; no pointer or ownership escapes. */
int rf_scene_npc_backlink_row(uint32_t index,uint32_t row[3])
{
    if(!row || index>=campaign_npc_body_count || !campaign_npc_bodies[index].registration.view)return RF_NOT_FOUND;
    row[0]=campaign_seeds.records.items[index].record.uid;
    row[1]=campaign_npc_bodies[index].registration.handle;row[2]=campaign_npc_bodies[index].trigger_handle;
    return RF_OK;
}
uint32_t rf_scene_npc_backlinks[4]; /* writes, linked actors, hash, retained bytes */
uint32_t rf_scene_npc_links[4]; /* UID objects, temporary bytes, trigger NPC links, event NPC links */
static int campaign_resolve_trigger_links(void)
{
    uint32_t i,j,n=campaign_events.count+campaign_triggers.count+campaign_group_registration.count+campaign_mover_count+rf_scene_npc_registration[0];
    rf_level_uid_object *objects=n?malloc((size_t)n*sizeof(*objects)):NULL;int status;
    if(n && !objects)return RF_RANGE;
    for(i=0;i<campaign_events.count;++i) {
        objects[i].uid=campaign_events.items[i].authored->record.uid;
        objects[i].handle=campaign_events.items[i].handle;objects[i].flags=0;
    }
    for(j=0;j<campaign_triggers.count;++j,++i) {
        objects[i].uid=campaign_triggers.items[j].authored->record.uid;
        objects[i].handle=campaign_triggers.items[j].handle;objects[i].flags=0;
    }
    for(j=0;j<campaign_group_registration.count;++j,++i)
        objects[i]=campaign_group_registration.objects[j];
    for(j=0;j<campaign_mover_count;++j,++i)objects[i]=campaign_mover_objects[j];
    for(j=0;j<campaign_npc_body_count;++j)if(campaign_npc_bodies[j].registration.view) {
        objects[i].uid=campaign_seeds.records.items[j].record.uid;
        objects[i].handle=campaign_npc_bodies[j].registration.handle;
        objects[i].flags=campaign_npc_bodies[j].view.flags_7c;++i;
    }
    if(i!=n){free(objects);return RF_FORMAT;}
    /* Verify UID lookup reaches the exact registered owner, including generation.
     * Duplicate authored IDs must not silently bind a different actor. */
    for(j=0;j<campaign_npc_body_count;++j)if(campaign_npc_bodies[j].registration.view) {
        rf_level_link_target target;
        status=rf_level_link_resolve(campaign_seeds.records.items[j].record.uid,objects,n,
            campaign_group_registration.keys,campaign_group_registration.key_count,&target);
        if(status || target.kind!=1 || target.value!=campaign_npc_bodies[j].registration.handle ||
           rf_entity_lookup(&campaign_entities,(int32_t)target.value)!=&campaign_npc_bodies[j].view) {
            free(objects);return status?status:RF_FORMAT;
        }
    }
    memset(rf_scene_npc_links,0,sizeof(rf_scene_npc_links));
    rf_scene_npc_links[0]=n;rf_scene_npc_links[1]=n*sizeof(*objects);
    /* Port setup order: events, triggers, controllers, movers, skeletal NPCs.
     * Original whole-world factory order and non-skeletal owners remain open. */
    status=rf_runtime_triggers_resolve(&campaign_triggers,objects,n,
        campaign_group_registration.keys,campaign_group_registration.key_count);
    memset(rf_scene_npc_backlinks,0,sizeof(rf_scene_npc_backlinks));
    rf_scene_npc_backlinks[2]=2166136261u;rf_scene_npc_backlinks[3]=campaign_npc_body_count*4;
    /*4611d8..461200: only object links, flag4, and a valid typed entity.
     * Iterate authored trigger/link order so later writes replace earlier ones. */
    if(!status)for(i=0;i<campaign_triggers.count;++i)if(campaign_triggers.items[i].state.flags&4) {
        rf_runtime_trigger *trigger=campaign_triggers.items+i;
        for(j=0;j<trigger->authored->record.link_count;++j)if(trigger->links[j].kind==1) {
            const rf_entity_view *view=rf_entity_lookup(&campaign_entities,(int32_t)trigger->links[j].value);
            uint32_t actor;if(!view)continue;
            for(actor=0;actor<campaign_npc_body_count;++actor)if(view==&campaign_npc_bodies[actor].view) {
                campaign_npc_bodies[actor].trigger_handle=trigger->handle;++rf_scene_npc_backlinks[0];break;
            }
        }
    }
    for(i=0;i<campaign_npc_body_count;++i) {
        uint32_t row[3];if(rf_scene_npc_backlink_row(i,row))continue;
        if(row[2]!=UINT32_MAX)++rf_scene_npc_backlinks[1];
        rf_scene_npc_backlinks[2]=npc_hash_bytes(rf_scene_npc_backlinks[2],row,sizeof(row));
    }
    if(!status)status=rf_runtime_events_resolve(&campaign_events,objects,n,
        campaign_group_registration.keys,campaign_group_registration.key_count);
    free(objects);
    memset(rf_scene_campaign_links,0,sizeof(rf_scene_campaign_links));
    if(status)return status;
    memset(rf_scene_campaign_event_links,0,sizeof(rf_scene_campaign_event_links));
    rf_scene_campaign_event_links[3]=2166136261u;
    for(i=0;i<campaign_events.count;++i)for(j=0;j<campaign_events.items[i].authored->record.link_count;++j) {
        rf_level_link_target *target=campaign_events.items[i].links+j;
        if(target->kind==1 && rf_entity_lookup(&campaign_entities,(int32_t)target->value))++rf_scene_npc_links[3];
        uint32_t k,words[4]={campaign_events.items[i].authored->links[j],target->value,target->kind,target->index};
        for(k=0;k<4;++k)rf_scene_campaign_event_links[3]=(rf_scene_campaign_event_links[3]^words[k])*16777619u;
        ++rf_scene_campaign_event_links[0];++rf_scene_campaign_event_links[target->kind?1:2];
    }
    rf_scene_campaign_links[3]=2166136261u;
    for(i=0;i<campaign_triggers.count;++i)for(j=0;j<campaign_triggers.items[i].authored->record.link_count;++j) {
        rf_level_link_target *target=campaign_triggers.items[i].links+j;
        if(target->kind==1 && rf_entity_lookup(&campaign_entities,(int32_t)target->value))++rf_scene_npc_links[2];
        uint32_t k,words[4]={campaign_triggers.items[i].authored->links[j],target->value,target->kind,target->index};
        for(k=0;k<4;++k)rf_scene_campaign_links[3]=(rf_scene_campaign_links[3]^words[k])*16777619u;
        ++rf_scene_campaign_links[0];
        ++rf_scene_campaign_links[campaign_triggers.items[i].links[j].kind?1:2];
    }
    return RF_OK;
}
uint32_t rf_scene_campaign_events[3]; /* registered events, owner bytes, registry bytes */

typedef struct campaign_damage_context {
    campaign_npc_body *owner;const rf_entity_seed_class *definition;
    rf_damage_object object;const rf_damage_effect_backend *effects;
    uint32_t clock_bits;int status;
} campaign_damage_context;
typedef struct campaign_player_damage_context {
    const rf_damage_effect_backend *effects;rf_damage_object object;
    uint32_t clock_bits;int status;
    int32_t now_ms;rf_random_state *pain_random;
} campaign_player_damage_context;
static rf_damage_object *player_damage_lookup(void *context,uint32_t handle)
{
    campaign_player_damage_context *c=context;
    return rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view?&c->object:NULL;
}
static uint32_t player_damage_gate(void *context,uint32_t stage,uint32_t handle,const rf_damage_object *object)
{
    (void)context;(void)handle;
    if(stage==0)return 1;
    if(stage==1)return rf_entity_armor_immunity(campaign_player_damage.state.effects.armor,
        campaign_player_damage.class_flags,campaign_player_damage.state.effects.flags_814);
    return (object->flags&8)!=0;
}
static uint32_t player_damage_query(void *context,uint32_t predicate,uint32_t handle)
{
    campaign_player_damage_context *c=context;const rf_damage_effect_backend *b=c->effects;
    if(handle==(uint32_t)campaign_player_view.handle) {
        /* This retained local owner is the associated player for this entity.
         * Other player/list families remain with the caller's resolver. */
        if(predicate==RF_DAMAGE_PLAYER || predicate==RF_DAMAGE_OBJECT_PLAYER_FLAG)
            return (campaign_player_view.flags_7c&8)!=0;
        if(predicate==RF_DAMAGE_UNOWNED_PLAYER)return 0;
    }
    return b->predicate(b->context,predicate,handle);
}
static uint32_t player_damage_uid(void *context,int32_t uid)
{const rf_damage_effect_backend *b=((campaign_player_damage_context *)context)->effects;return b->resolve_uid(b->context,uid);}
static int player_damage_source(void *context,uint32_t handle,uint32_t *affiliation)
{const rf_damage_effect_backend *b=((campaign_player_damage_context *)context)->effects;return b->source(b->context,handle,affiliation);}
static uint32_t player_damage_burn(void *context,uint32_t target,uint32_t source)
{const rf_damage_effect_backend *b=((campaign_player_damage_context *)context)->effects;return b->create_burn(b->context,target,source);}
static float player_damage_random(void *context,float minimum,float maximum)
{const rf_damage_effect_backend *b=((campaign_player_damage_context *)context)->effects;return b->random(b->context,minimum,maximum);}
static uint32_t player_damage_playing(void *context,uint32_t voice)
{const rf_damage_effect_backend *b=((campaign_player_damage_context *)context)->effects;return b->playing(b->context,voice);}
static uint32_t player_damage_play(void *context,uint32_t target)
{const rf_damage_effect_backend *b=((campaign_player_damage_context *)context)->effects;return b->play_kind6(b->context,target);}
static void player_damage_notify(void *context,uint32_t kind,uint32_t target,float value,uint32_t source)
{
    campaign_player_damage_context *c=context;const rf_damage_effect_backend *b=c->effects;
    /*428740 exits for an associated mode0 player before all timer/effect work.
     * The current campaign eye profile is the reconstructed first-person path;
     * body/unknown camera profiles must still use the supplied reaction owner. */
    if(kind==RF_DAMAGE_PAIN_ANIMATION && campaign_spawn && rf_scene_actor_eye_enabled &&
        target==(uint32_t)campaign_player_view.handle && (campaign_player_view.flags_7c&8))return;
    if(kind==RF_DAMAGE_PAIN_SOUND && c->pain_random && campaign_spawn && rf_scene_actor_eye_enabled &&
        target==(uint32_t)campaign_player_view.handle && (campaign_player_view.flags_7c&8)) {
        int status=rf_scene_player_pain_sound(target,value,c->now_ms,c->pain_random);
        if(status && !c->status)c->status=status;return;
    }
    if(kind==RF_DAMAGE_PLAYER_FEEDBACK) {int status=rf_scene_player_damage_flash(target);if(status && !c->status)c->status=status;}
    else b->notify(b->context,kind,target,value,source);
}
static float player_damage_effect(void *context,rf_damage_object *object,float amount,uint32_t source,int32_t kind,uint32_t extra)
{
    campaign_player_damage_context *c=context;float result=0;int status;
    rf_damage_effect_backend effects={player_damage_query,player_damage_uid,player_damage_source,player_damage_burn,
        player_damage_random,player_damage_notify,player_damage_playing,player_damage_play,c};
    campaign_player_damage.object_flags=object->flags;campaign_player_view.flags_7c=object->flags;
    status=rf_entity_damage_sp(&campaign_player_damage.state,amount,kind,source,(int32_t)extra,
        kind==-1?1:campaign_player_damage.factors[kind],c->clock_bits,&effects,&result);
    if(status && !c->status)c->status=status;
    object->health=campaign_player_damage.state.effects.health;object->flags=campaign_player_view.flags_7c;
    campaign_player_view.flags_810=campaign_player_damage.state.effects.flags_810;return result;
}
static int campaign_player_damage_apply(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,int32_t now_ms,rf_random_state *random,const rf_damage_effect_backend *effects,float *result)
{
    campaign_player_damage_context c={0};float value;int status;
    rf_damage_backend backend={player_damage_lookup,player_damage_gate,player_damage_effect,&c};
    if(!request || !effects || !result || request->kind < -1 || request->kind>10 ||
        !effects->predicate || !effects->resolve_uid || !effects->source || !effects->create_burn ||
        !effects->random || !effects->notify || !effects->playing || !effects->play_kind6)return RF_RANGE;
    if(!isfinite(request->amount) || !isfinite(difficulty))return RF_FORMAT;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&campaign_player_view ||
        campaign_player_damage.state.effects.handle!=handle){*result=0;return RF_OK;}
    c.effects=effects;c.clock_bits=clock_bits;c.now_ms=now_ms;c.pain_random=random;
    c.object=(rf_damage_object){0,campaign_player_view.flags_7c,campaign_player_damage.state.effects.health};
    campaign_player_damage.state.effects.flags_810=campaign_player_view.flags_810;
    status=rf_damage_dispatch_sp(handle,request,difficulty,&backend,&value);
    campaign_player_damage.state.effects.health=c.object.health;campaign_player_damage.object_flags=c.object.flags;
    campaign_player_view.flags_7c=c.object.flags;
    if(c.status)return c.status;if(status)return status;*result=value;return RF_OK;
}
int rf_scene_player_damage(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,const rf_damage_effect_backend *effects,float *result)
{return campaign_player_damage_apply(handle,request,difficulty,clock_bits,0,NULL,effects,result);}
int rf_scene_player_damage_audio(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,int32_t now_ms,rf_random_state *random,const rf_damage_effect_backend *effects,float *result)
{
    if(!random || now_ms<0 || now_ms>RF_TIMER_PERIOD)return RF_RANGE;
    return campaign_player_damage_apply(handle,request,difficulty,clock_bits,now_ms,random,effects,result);
}
static int campaign_npc_motion_owner(uint32_t handle,campaign_npc_body **result,uint32_t *class_index,rf_entity_pose **pose)
{
    uint32_t i,cls;campaign_npc_body *owner;int status;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view || owner->view.type!=0)return RF_NOT_FOUND;
    if(!campaign_seeds.items || i>=campaign_seeds.records.count)return RF_RANGE;cls=campaign_seeds.items[i].class_index;
    if(cls>=campaign_seeds.class_count || cls>=campaign_motion_catalog.mapping_count)return RF_RANGE;
    status=campaign_actor_pose(i,pose);if(status)return status;if(!*pose)return RF_NOT_FOUND;
    *result=owner;*class_index=cls;return RF_OK;
}
int rf_scene_npc_fall(uint32_t handle)
{
    campaign_npc_body *owner;rf_entity_pose *pose;uint32_t cls;int status;
    status=campaign_npc_motion_owner(handle,&owner,&cls,&pose);if(status)return status;
    if(!campaign_seeds.classes)return RF_RANGE;
    owner->movement_slot=rf_movement_fall(campaign_modes,campaign_seeds.classes[cls].physics.flags,&owner->body.state.flags);
    owner->movement_orientation=campaign_identity[0];return RF_OK;
}
int rf_scene_npc_set_speed(uint32_t handle,int32_t requested,int32_t forced_action)
{
    campaign_npc_body *owner;rf_entity_pose *pose;uint32_t cls;int status;
    status=campaign_npc_motion_owner(handle,&owner,&cls,&pose);if(status)return status;
    if(!campaign_npc_movement_configs)return RF_RANGE;
    status=rf_movement_set_mode(&owner->movement,campaign_npc_movement_configs+cls,requested,forced_action,owner->body.state.mass,0);
    if(!status)owner->body.state.coefficients[1]=owner->movement.response;return status;
}
int rf_scene_npc_request_motion(uint32_t handle,int32_t requested,float duration)
{
    campaign_npc_body *owner;rf_entity_pose *pose;uint32_t cls;int status;
    status=campaign_npc_motion_owner(handle,&owner,&cls,&pose);if(status)return status;
    if(!campaign_motion_catalog.mappings || campaign_motion_catalog.mappings[cls].skeleton!=pose->skeleton)return RF_RANGE;
    return rf_motion_request_state(&pose->controller,campaign_motion_catalog.mappings[cls].states,requested,duration);
}
int rf_scene_npc_collision_view(uint32_t handle,rf_collision_pair_actor_state *result)
{
    campaign_npc_body *owner;rf_entity_pose *pose;rf_collision_pair_actor_state value={0};uint32_t i;int status;
    if(!result)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view || owner->view.type!=0)return RF_NOT_FOUND;
    if(owner->movement_slot>=16 || !campaign_seeds.records.items || i>=campaign_seeds.records.count)return RF_RANGE;
    status=campaign_actor_pose(i,&pose);if(status)return status;
    value.kind=0;value.body_flags=owner->body.state.flags;value.model=pose?i+1:0;
    value.movement_mode=campaign_modes[owner->movement_slot].index;value.handle=handle;
    /* Original object200 is the registry view's linked_handle. */
    value.parent_handle=(uint32_t)owner->view.linked_handle;value.object_flags=owner->object_flags;
    memcpy(value.position,owner->published,12);
    /* Same authored orientation as current NPC model placement/clearance.
     * Moving object orientation publication remains a separate integration. */
    memcpy(value.forward,campaign_seeds.records.items[i].record.orientation[2],12);
    *result=value;return RF_OK;
}

static int collision_body_response(const rf_physics_body *body,const rf_collision_contact_extra *extra,
    uint32_t handle,uint32_t material,uint32_t kind,rf_collision_actor_general_response *result)
{
    rf_collision_actor_general_response value={0};int status;
    if(!body->allocated_bytes || body->spheres.count>INT32_MAX ||
       (body->spheres.count && !body->spheres.items))return RF_RANGE;
    memcpy(value.actor.minimum,body->state.bounds.minimum,12);memcpy(value.actor.maximum,body->state.bounds.maximum,12);
    memcpy(value.actor.position,body->state.position,12);memcpy(value.actor.next_position,body->state.next_position,12);
    memcpy(value.actor.velocity,body->state.velocity,12);value.actor.mass=body->state.mass;
    value.actor.handle=handle;value.actor.material=material;value.actor.body_flags=body->state.flags;
    value.actor.sphere_count=(int32_t)body->spheres.count;value.actor.spheres=body->spheres.items;
    status=rf_collision_contact_read(&body->state,extra,&value.actor.contact);if(status)return status;
    memcpy(value.orientation,body->state.orientation,36);memcpy(value.next_orientation,body->state.next_orientation,36);
    value.extent=body->state.bounds.radius;value.kind=kind;*result=value;return RF_OK;
}
int rf_scene_npc_collision_response(uint32_t handle,rf_collision_actor_general_response *result)
{
    rf_collision_pair_actor_state view;campaign_npc_body *owner;uint32_t i;int status;
    if(!result)return RF_RANGE;
    status=rf_scene_npc_collision_view(handle,&view);if(status)return status;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    return collision_body_response(&owner->body,&owner->collision_contact,handle,owner->collision_material,view.kind,result);
}
int rf_scene_npc_collision_publish(uint32_t handle,uint32_t body_flags,const rf_collision_actor_contact *contact)
{
    rf_collision_pair_actor_state view;campaign_npc_body *owner;uint32_t i;int status;
    if(!contact)return RF_RANGE;
    status=rf_scene_npc_collision_view(handle,&view);if(status)return status;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(!owner->body.allocated_bytes)return RF_RANGE;
    status=rf_collision_contact_write(&owner->body.state,&owner->collision_contact,contact);
    if(!status)owner->body.state.flags=body_flags;return status;
}

int rf_scene_npc_death_entry(uint32_t handle,uint32_t *entered)
{
    campaign_npc_body *owner;rf_entity_death_entry_state state;uint32_t i,cls,falling;
    if(!entered)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view)return RF_NOT_FOUND;
    if(owner->view.flags_810&1u){*entered=0;return RF_OK;}
    if(!campaign_seeds.items || !campaign_seeds.classes || i>=campaign_seeds.records.count || owner->movement_slot>=16)return RF_RANGE;
    cls=campaign_seeds.items[i].class_index;if(cls>=campaign_seeds.class_count)return RF_RANGE;
    falling=rf_entity_falling((int32_t)campaign_modes[owner->movement_slot].index,
        campaign_seeds.classes[cls].physics.use_kind,owner->support.material);
    state.flags_810=owner->view.flags_810;state.flags_1a8=owner->body.state.flags;
    memcpy(state.vector_714,owner->command_714,12);
    /* Embedded physics starts at actor88: bc/c8 map to actor144/150. */
    memcpy(state.vector_144,owner->body.state.velocity,12);memcpy(state.vector_150,owner->body.state.vector_c8,12);
    *entered=rf_entity_death_entry_sp(&state,falling);
    owner->view.flags_810=owner->damage.effects.flags_810=state.flags_810;owner->body.state.flags=state.flags_1a8;
    memcpy(owner->command_714,state.vector_714,12);
    memcpy(owner->body.state.velocity,state.vector_144,12);memcpy(owner->body.state.vector_c8,state.vector_150,12);
    return RF_OK;
}

typedef struct campaign_death_tail_context {
    campaign_npc_body *owner;rf_entity_death_tail_state state;
    const rf_entity_death_tail_backend *resources;
} campaign_death_tail_context;
static void campaign_death_tail_load(campaign_death_tail_context *c)
{
    campaign_npc_body *o=c->owner;
    c->state.action_520=(uint32_t)o->view.action_520;
    c->state.class_flags_728=o->damage.effects.class_flags_728;c->state.radius_78=o->model_radius_78;
    c->state.deadline_4b8=o->death.deadline_4b8;c->state.flags_810=o->view.flags_810;c->state.model_148c=o->death.model_148c;
}
static void campaign_death_tail_publish(campaign_death_tail_context *c)
{
    c->owner->death.deadline_4b8=c->state.deadline_4b8;c->owner->death.model_148c=c->state.model_148c;
    c->owner->damage.effects.flags_810=c->owner->view.flags_810;
}
static void campaign_death_tail_call(void *context,uint32_t operation,uint32_t argument)
{
    campaign_death_tail_context *c=context;
    campaign_death_tail_publish(c);
    c->resources->call(c->resources->context,operation,argument);
    campaign_death_tail_load(c);
}
int rf_scene_npc_death_tail(uint32_t handle,uint32_t name,const rf_entity_death_tail_backend *resources)
{
    const rf_entity_view *view;campaign_death_tail_context c={0};uint32_t i;int status;
    rf_entity_death_tail_backend backend={campaign_death_tail_call,&c,NULL};
    if(!resources || !resources->call || !resources->now_ms)return RF_RANGE;
    view=rf_entity_lookup(&campaign_entities,(int32_t)handle);if(!view)return RF_NOT_FOUND;
    for(i=0;i<campaign_npc_body_count;++i)if(view==&campaign_npc_bodies[i].view)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;
    c.owner=campaign_npc_bodies+i;c.resources=resources;c.state.name=name;backend.now_ms=resources->now_ms;
    campaign_death_tail_load(&c);status=rf_entity_death_tail_sp(&c.state,&backend);
    campaign_death_tail_publish(&c);return status;
}

static rf_damage_object *campaign_damage_lookup(void *context,uint32_t handle)
{
    campaign_damage_context *c=context;
    return rf_entity_lookup(&campaign_entities,(int32_t)handle)==&c->owner->view?&c->object:NULL;
}
static void campaign_damage_flags(campaign_damage_context *c)
{
    c->owner->object_flags=c->object.flags;c->owner->view.flags_7c=c->object.flags;
    c->owner->view.flags_810=c->owner->damage.effects.flags_810;
}
static uint32_t campaign_damage_predicate(void *context,uint32_t stage,uint32_t handle,const rf_damage_object *object)
{
    campaign_damage_context *c=context;const rf_entity_view *view;int32_t player=campaign_player_view.handle;
    (void)object;campaign_damage_flags(c);view=rf_entity_lookup(&campaign_entities,(int32_t)handle);
    if(stage==0)return view!=NULL;
    if(stage==1)return rf_entity_armor_immunity(c->owner->damage.effects.armor,c->definition->physics.flags,c->owner->damage.effects.flags_814);
    /*48aaf0: object player bit or a player's linked actor. Same current
     * single-player list as trigger actor resolution, refreshed after effects. */
    if(view && (view->flags_7c&8))return 1;
    view=rf_entity_lookup(&campaign_entities,player);
    return view && view->linked_handle==(int32_t)handle;
}
static float campaign_damage_effect(void *context,rf_damage_object *object,float amount,uint32_t source,int32_t kind,uint32_t extra)
{
    campaign_damage_context *c=context;float result=0;
    campaign_damage_flags(c);
    c->status=rf_entity_damage_sp(&c->owner->damage,amount,kind,source,(int32_t)extra,
        kind==-1?1:c->definition->damage_factors[kind],c->clock_bits,c->effects,&result);
    object->health=c->owner->damage.effects.health;object->flags=c->owner->object_flags;
    c->owner->view.flags_7c=object->flags;c->owner->view.flags_810=c->owner->damage.effects.flags_810;
    return result;
}
int rf_scene_npc_damage(uint32_t handle,const rf_damage_request *request,float difficulty,
    uint32_t clock_bits,const rf_damage_effect_backend *effects,float *result)
{
    campaign_damage_context c={0};rf_damage_backend backend={campaign_damage_lookup,campaign_damage_predicate,campaign_damage_effect,&c};
    uint32_t i;float value;int status;
    if(!request || !effects || !result || request->kind < -1 || request->kind>10 ||
       !effects->predicate || !effects->resolve_uid || !effects->source || !effects->create_burn ||
       !effects->random || !effects->notify || !effects->playing || !effects->play_kind6)return RF_RANGE;
    if(!isfinite(request->amount) || !isfinite(difficulty))return RF_FORMAT;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view &&
       campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count){*result=0;return RF_OK;}
    c.owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&c.owner->view){*result=0;return RF_OK;}
    c.owner->damage.effects.flags_810=c.owner->view.flags_810;
    c.definition=campaign_seeds.classes+campaign_seeds.items[i].class_index;
    c.object=(rf_damage_object){0,c.owner->object_flags,c.owner->damage.effects.health};c.effects=effects;c.clock_bits=clock_bits;
    status=rf_damage_dispatch_sp(handle,request,difficulty,&backend,&value);
    c.owner->damage.effects.health=c.object.health;campaign_damage_flags(&c);
    if(c.status)return c.status;if(status)return status;*result=value;return RF_OK;
}
uint32_t rf_scene_npc_damage_test_uid=UINT32_MAX,rf_scene_npc_damage_test_words[64];
static int campaign_event_damage_lookup(void *context,uint32_t handle,uint32_t stage,rf_event_damage_target *target)
{
    rf_scene_npc_event_damage_services *c=context;const rf_entity_view *view,*linked;uint32_t i;
    if(c->status)return c->status;
    if(stage>2)return c->status=RF_RANGE;
    memset(target,0,sizeof(*target));view=rf_entity_lookup(&campaign_entities,(int32_t)handle);
    if(!view) {
        /* A missing generation is absent; another live target family needs
         * its actual damage owner, not a pretend successful zero hit. */
        if(rf_object_registry_lookup(&campaign_registry,handle))return c->status=RF_NOT_FOUND;
        return RF_OK;
    }
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view==view &&
        campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count && (view!=&campaign_player_view ||
        campaign_player_damage.state.effects.handle!=handle))return c->status=RF_NOT_FOUND;
    target->present=1;target->entity_handle=handle;
    if(stage==1) {
        linked=rf_entity_lookup(&campaign_entities,view->linked_handle);
        target->exclude_a=linked && linked->class_type==1; /*4290d0*/
        target->exclude_b=(view->flags_810&1u)!=0; /*427020*/
    }
    /*48acf0: retained local type0 owner has a player association; NPCs do not. */
    if(stage==2 && view==&campaign_player_view && view->type==0) {
        target->feedback=1;target->feedback_handle=handle;
    }
    return RF_OK;
}
static void campaign_event_damage_apply(void *context,const rf_event_damage_request *input)
{
    rf_scene_npc_event_damage_services *c=context;rf_damage_request request;
    if(c->status)return;
    request.amount=input->amount;request.source=input->source;request.kind=(int32_t)input->kind;
    request.argument6=input->flags;request.auxiliary_uid=input->other;request.force=input->enabled;
    if(rf_entity_lookup(&campaign_entities,(int32_t)input->target)==&campaign_player_view) {
        if(c->player_pain_random)c->status=rf_scene_player_damage_audio(input->target,&request,c->difficulty,
            c->clock_bits,c->now_ms,c->player_pain_random,c->effects,&c->last_amount);
        else c->status=rf_scene_player_damage(input->target,&request,c->difficulty,c->clock_bits,c->effects,&c->last_amount);
    }
    else c->status=rf_scene_npc_damage(input->target,&request,c->difficulty,c->clock_bits,c->effects,&c->last_amount);
    ++c->dispatches;
}
static void campaign_event_damage_feedback(void *context,uint32_t handle,float first,float second)
{
    rf_scene_event_damage_services *c=context;
    if(!c->status)c->status=rf_scene_player_feedback(handle,first,second,c->now_ms);
}
int rf_scene_event_damage_bind(rf_scene_event_damage_services *services,rf_event_damage_backend *backend)
{
    const rf_damage_effect_backend *e;
    if(!services || !backend || !(e=services->effects) || !isfinite(services->difficulty) ||
       services->now_ms<0 || services->now_ms>RF_TIMER_PERIOD ||
       !e->predicate || !e->resolve_uid || !e->source || !e->create_burn || !e->random ||
       !e->notify || !e->playing || !e->play_kind6)return RF_RANGE;
    *backend=(rf_event_damage_backend){campaign_event_damage_lookup,campaign_event_damage_apply,
        campaign_event_damage_feedback,services};return RF_OK;
}
int rf_scene_npc_event_damage_bind(rf_scene_npc_event_damage_services *services,rf_event_damage_backend *backend)
{return rf_scene_event_damage_bind(services,backend);}
uint32_t rf_scene_npc_pain_test_words[10]; /* Two post-hit pain records plus RNG state. */
typedef struct campaign_pain_context {
    campaign_npc_body *owner;rf_entity_pose *pose;rf_entity_pain_state state;
    const rf_entity_state_set *bindings;rf_entity_playback_model *model;
    rf_random_state *random;const rf_scene_npc_pain_ops *ops;int32_t now;int status;
} campaign_pain_context;
static uint32_t campaign_pain_query(void *context,uint32_t query)
{
    campaign_pain_context *c=context;int value=0,eligible;
    if(c->status)return 0;
    switch(query) {
    case RF_PAIN_PLAYER:case RF_PAIN_PLAYER_MODE:return 0; /* No player association on these NPC owners. */
    case RF_PAIN_COOLDOWN:c->status=rf_timer_expired(c->owner->pain.cooldown,c->now,&value);break;
    case RF_PAIN_EXCLUDED:return (c->owner->view.flags_810&1)!=0;
    case RF_PAIN_AI_ENABLED:return (c->owner->view.flags_7d0&1)!=0;
    case RF_PAIN_AI_BLOCKED:return (c->owner->view.flags_7d0&0x100)!=0;
    case RF_PAIN_AI_TIMER:c->status=rf_timer_pending(c->owner->pain.ai_timer,c->now,&value);break;
    case RF_PAIN_FIRE_PRIMARY:case RF_PAIN_FIRE_SECONDARY:
        c->status=rf_motion_action_active(&c->pose->playback,c->model->resources,c->model->count,
            c->state.motions,query==RF_PAIN_FIRE_PRIMARY?2:3,&value);break;
    case RF_PAIN_COMBAT:c->status=rf_entity_combat_predicates(&campaign_entities,&c->owner->view,NULL,0,&value,&eligible);break;
    default:c->status=RF_RANGE;break;
    }
    return (uint32_t)value;
}
static void campaign_pain_effect(void *context,uint32_t effect,uint32_t first,uint32_t second)
{
    campaign_pain_context *c=context;int32_t sounds[45],sound;uint32_t i;
    if(c->status)return;
    switch(effect) {
    case RF_PAIN_RESET_WEAPON:
        if(second<64)c->status=c->ops && c->ops->reset_weapon?
            c->ops->reset_weapon(c->ops->context,first,(int32_t)second):RF_NOT_FOUND;
        break;
    case RF_PAIN_START:
        c->owner->pain.selected_action=c->state.selected_action;
        c->status=campaign_npc_motion_require(c->pose->skeleton,(uint32_t)c->state.motions[first]);if(c->status)break;
        for(i=0;i<45;++i)sounds[i]=c->bindings->action_sounds[i][0]?(int32_t)i:-1;
        c->status=rf_motion_start_action(&c->pose->playback,c->model->resources,c->model->count,
            c->state.motions,sounds,(int32_t)first,1,0,1,&sound);
        if(!c->status && sound>=0)c->status=c->ops && c->ops->play_sound?
            c->ops->play_sound(c->ops->context,c->owner->registration.handle,c->bindings->action_sounds[sound]):RF_NOT_FOUND;
        break;
    case RF_PAIN_RESET_COOLDOWN:c->status=rf_timer_set_random(&c->owner->pain.cooldown,c->now,(int32_t)first,(int32_t)second,c->random);break;
    case RF_PAIN_SET_LOCK:c->status=rf_timer_set(&c->owner->pain.animation_lock,c->now,(int32_t)first);break;
    default:c->status=RF_RANGE;break;
    }
}
static double campaign_pain_duration(void *context,uint32_t model,int32_t motion)
{
    campaign_pain_context *c=context;const rf_motion_file *file;
    if(c->status)return NAN;
    if(model>=campaign_motion_catalog.model_count || motion<0 || (uint32_t)motion>=campaign_motion_catalog.models[model].count) {
        c->status=RF_RANGE;return NAN;
    }
    file=&campaign_motion_catalog.models[model].items[motion].file;
    return rf_motion_duration((int32_t)file->header[4],(int32_t)file->header[5]);
}
int rf_scene_npc_pain(uint32_t handle,int32_t now,rf_random_state *random,const rf_scene_npc_pain_ops *ops)
{
    uint32_t i,cls;int status;campaign_pain_context c={0};
    rf_entity_pain_backend backend={campaign_pain_query,campaign_pain_effect,campaign_pain_duration,&c};
    if(!random || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;
    c.owner=campaign_npc_bodies+i;status=campaign_actor_pose(i,&c.pose);if(status)return status;if(!c.pose)return RF_NOT_FOUND;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&c.owner->view)return RF_NOT_FOUND;
    cls=campaign_seeds.items[i].class_index;
    if(cls>=campaign_motion_catalog.class_count || cls>=campaign_base_motions.class_count ||
       c.pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
    c.bindings=campaign_base_motions.classes+cls;c.model=campaign_playback_resources.models+c.pose->skeleton;
    c.state.handle=handle;c.state.primary_weapon=c.owner->view.weapons[0];c.state.model=c.pose->skeleton;
    c.state.selected_action=c.owner->pain.selected_action;
    memcpy(c.state.motions,campaign_motion_catalog.mappings[cls].actions,sizeof(c.state.motions));
    c.now=now;c.random=random;c.ops=ops;
    status=rf_entity_pain_react(&c.state,&backend);return c.status?c.status:status;
}
int rf_scene_npc_death_play(uint32_t handle,int32_t action,uint32_t freeze,
    int (*play_sound)(void *,uint32_t,const char *),void *context)
{
    campaign_npc_body *owner;rf_entity_pose *pose;rf_entity_playback_model *model;
    const rf_entity_motion_mapping *mapping;const rf_entity_state_set *bindings;
    uint32_t i,cls;int status;int32_t sounds[45],sound;
    if(action<0 || action>=45)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view)return RF_NOT_FOUND;
    status=campaign_actor_pose(i,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    if(!campaign_seeds.items || i>=campaign_seeds.records.count)return RF_RANGE;
    cls=campaign_seeds.items[i].class_index;
    if(!campaign_motion_catalog.mappings || !campaign_base_motions.classes || !campaign_playback_resources.models ||
       cls>=campaign_motion_catalog.class_count || cls>=campaign_motion_catalog.mapping_count ||
       cls>=campaign_base_motions.class_count || pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
    mapping=campaign_motion_catalog.mappings+cls;bindings=campaign_base_motions.classes+cls;
    if(mapping->weapon!=-1 || owner->view.weapons[0]!=-1 || mapping->skeleton!=pose->skeleton)return RF_NOT_FOUND;
    if(mapping->actions[action]<0)return RF_NOT_FOUND;
    owner->death.action_824=action;
    status=campaign_npc_motion_require(pose->skeleton,(uint32_t)mapping->actions[action]);if(status)return status;
    model=campaign_playback_resources.models+pose->skeleton;
    for(i=0;i<45;++i)sounds[i]=bindings->action_sounds[i][0]?(int32_t)i:-1;
    status=rf_motion_start_action(&pose->playback,model->resources,model->count,mapping->actions,sounds,action,1,(int)(freeze&255u),1,&sound);
    if(status)return status;
    if(sound>=0)return play_sound?play_sound(context,handle,bindings->action_sounds[sound]):RF_NOT_FOUND;
    return RF_OK;
}

typedef struct campaign_death_motion_context {
    campaign_npc_body *owner;rf_entity_death_motion_state state;
    const rf_scene_death_motion_ops *ops;uint32_t slot,handle;int status;
} campaign_death_motion_context;
static void campaign_death_motion_publish(campaign_death_motion_context *c)
{
    c->owner->view.flags_810=c->state.flags_810;c->owner->damage.effects.flags_810=c->state.flags_810;
    c->owner->death.action_824=c->state.action_824;c->owner->death.requested_83c=c->state.requested_83c;
    c->owner->death_bone_words[0]=c->state.word_1464;c->owner->death_bone_words[1]=c->state.word_1468;
}
static void campaign_death_motion_reload(campaign_death_motion_context *c)
{
    c->state.flags_810=c->owner->view.flags_810;c->state.action_824=c->owner->death.action_824;
    c->owner->damage.effects.flags_810=c->owner->view.flags_810;
    c->state.requested_83c=c->owner->death.requested_83c;
    c->state.word_1464=c->owner->death_bone_words[0];c->state.word_1468=c->owner->death_bone_words[1];
}
static uint32_t campaign_death_motion_call(void *context,uint32_t op,uint32_t first,uint32_t second)
{
    campaign_death_motion_context *c=context;uint32_t result=0;int32_t action=-1;
    if(c->status)return 0;campaign_death_motion_publish(c);
    switch(op) {
    case RF_DEATH_MOTION_SELECT:
        c->status=c->ops->select(c->ops->context,c->handle,&action);result=(uint32_t)action;break;
    case RF_DEATH_MOTION_SKELETAL:result=1;break;
    case RF_DEATH_MOTION_RESET:c->status=rf_scene_model_stop_nonlooping(c->slot);break;
    case RF_DEATH_MOTION_POSE:result=c->slot+1;break;
    case RF_DEATH_MOTION_CLEAR_BONE:c->status=rf_scene_model_clear_bone_override(c->slot,second);break;
    case RF_DEATH_MOTION_PLAY:
        c->status=rf_scene_npc_death_play(c->handle,(int32_t)first,second,c->ops?c->ops->sound:NULL,c->ops?c->ops->context:NULL);break;
    default:c->status=RF_RANGE;break;
    }
    campaign_death_motion_reload(c);return result;
}
int rf_scene_npc_death_motion(uint32_t handle,const rf_scene_death_motion_ops *ops)
{
    campaign_death_motion_context c={0};rf_entity_pose *pose;uint32_t i,cls;int status;
    rf_entity_death_motion_backend backend={campaign_death_motion_call,&c};
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;
    c.owner=campaign_npc_bodies+i;c.slot=i;c.handle=handle;c.ops=ops;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&c.owner->view)return RF_NOT_FOUND;
    if(c.owner->view.flags_810&0x80u)return RF_OK;
    if(c.owner->view.weapons[0]!=-1)return RF_NOT_FOUND;
    if(c.owner->death.requested_83c==-1 && (!ops || !ops->select))return RF_NOT_FOUND;
    status=campaign_actor_pose(i,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    if(!campaign_seeds.items || i>=campaign_seeds.records.count)return RF_RANGE;
    cls=campaign_seeds.items[i].class_index;
    if(!campaign_motion_catalog.mappings || cls>=campaign_motion_catalog.mapping_count)return RF_RANGE;
    if(campaign_motion_catalog.mappings[cls].weapon!=-1 || campaign_motion_catalog.mappings[cls].skeleton!=pose->skeleton)return RF_NOT_FOUND;
    status=rf_entity_class_death_bones(&campaign_seeds,&campaign_skeletons,cls,c.state.base_bones);if(status)return status;
    memcpy(c.state.effective_bones,c.state.base_bones,sizeof(c.state.effective_bones));
    memcpy(c.state.motions,campaign_motion_catalog.mappings[cls].actions,sizeof(c.state.motions));
    c.state.model=i+1;c.state.class_flags_724=campaign_seeds.classes[cls].physics.flags;
    campaign_death_motion_reload(&c);status=rf_entity_death_motion_sp(&c.state,0,&backend);
    if(c.status)return c.status;campaign_death_motion_publish(&c);return status;
}

uint32_t rf_scene_npc_pain_audio[9]; /* calls, selections, plays, loads, PCM bytes, last sample, RNG, errors, name hash */
uint32_t rf_scene_npc_pain_sound_test[10]; /* two deadline/voice/sample/RNG/play-count snapshots */
typedef struct campaign_pain_audio_context {rf_random_state *random;int status;uint32_t *telemetry,player;} campaign_pain_audio_context;
static int32_t campaign_pain_audio_resolve(void *context,int32_t group)
{
    campaign_pain_audio_context *c=context;const rf_foley_group *g;
    rf_audio_group_input input={0};rf_audio_group_request request;
    if(group<0 || (uint32_t)group>=campaign_foley.group_count)return -1;
    g=campaign_foley.groups+group;
    if(!g->count || g->first>=campaign_foley.sample_count || g->count>campaign_foley.sample_count-g->first) {
        c->status=RF_FORMAT;return -1;
    }
    input.object_kind=1;
    c->status=rf_audio_group_choose(&input,campaign_foley.samples+g->first,g->count,(int32_t)g->count,c->random,&request);
    if(c->status)return -1;
    ++c->telemetry[1];c->telemetry[5]=(uint32_t)request.sample;
    return request.sample;
}
static int32_t campaign_pain_audio_playing(void *context,int32_t voice)
{
    uint32_t source;(void)context;
    return rf_audio_voice_ids_resolve(&campaign_device_voice_ids,&campaign_audio_mixer,voice,&source)==RF_OK;
}
static void campaign_pain_audio_play(void *context,const float position[3],int32_t sample)
{
    campaign_pain_audio_context *c=context;
    if(c->status)return;
    if(sample==-1)return; /* Original playback rejects the absent sample without a voice. */
    if(sample<0 || (uint32_t)sample>=campaign_audio_bank.count){c->status=RF_RANGE;return;}
    {const unsigned char *name=(const unsigned char *)campaign_audio_bank.samples[sample].name;
     uint32_t hash=2166136261u;for(;*name;++name) {
        uint32_t ch=*name;if(ch>='A' && ch<='Z')ch+='a'-'A';hash=(hash^ch)*16777619u;
     }c->telemetry[8]=hash;}
    if(!rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample)) {
        c->status=campaign_ambient_reload((uint32_t)sample);if(c->status)return;
        if((uint32_t)sample<sizeof(campaign_audio_evictable))campaign_audio_evictable[sample]=1;
        ++rf_scene_sound_bank[1];rf_scene_sound_bank[2]+=campaign_audio_bank.samples[sample].bytes;
        rf_scene_live_audio[1]=campaign_audio_bank.bytes;
        ++c->telemetry[3];c->telemetry[4]+=campaign_audio_bank.samples[sample].bytes;
    }
    if(c->player) {
        rf_player_sound_input input={0};rf_player_sound_request request;int32_t voice;
        input.owner_present=1;input.sound_id=sample;input.volume=1;
        memcpy(input.position,position,12);
        c->status=rf_player_sound_route(&input,&request);
        if(!c->status)c->status=rf_scene_sound_play_request(&request,&voice);
        if(c->status)return;
    } else if(campaign_sound_play(NULL,sample,position,1,0)<0){c->status=RF_IO;return;}
    ++c->telemetry[2];
    /* Original48a9c0 does not store the returned voice in entity+808. */
}
uint32_t rf_scene_npc_action_audio[9];
int rf_scene_npc_death_sound(void *context,uint32_t handle,const char *name)
{
    rf_scene_death_selection_context *input=context;campaign_npc_body *owner;uint32_t i;int status;int32_t group,sample;
    campaign_pain_audio_context audio;
    if(!input || !input->random || !name)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view)return RF_NOT_FOUND;
    status=rf_foley_find(&campaign_foley,name,&group);if(status)return status;
    if(group<0)return RF_OK; /* Original absent group resolves to absent sample. */
    if(!campaign_foley.samples)return RF_RANGE;
    audio.random=input->random;audio.status=0;audio.telemetry=rf_scene_npc_action_audio;audio.player=0;
    ++rf_scene_npc_action_audio[0];sample=campaign_pain_audio_resolve(&audio,group);
    if(!audio.status)campaign_pain_audio_play(&audio,owner->published,sample);
    rf_scene_npc_action_audio[6]=input->random->value;
    if(audio.status)++rf_scene_npc_action_audio[7];return audio.status;
}
uint32_t rf_scene_npc_impact_audio[12]; /* common audio counters + requested body position */
int rf_scene_npc_impact_sound(uint32_t handle,rf_random_state *random)
{
    campaign_npc_body *owner;uint32_t i,cls;int32_t group,sample;campaign_pain_audio_context audio;
    if(!random)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view)return RF_NOT_FOUND;
    if(!campaign_seeds.items || i>=campaign_seeds.records.count || !campaign_impact_groups)return RF_RANGE;
    cls=campaign_seeds.items[i].class_index;if(cls>=campaign_seeds.class_count)return RF_RANGE;
    group=campaign_impact_groups[cls];
    if(group>=0 && !campaign_foley.samples)return RF_RANGE;
    audio.random=random;audio.status=0;audio.telemetry=rf_scene_npc_impact_audio;audio.player=0;
    ++rf_scene_npc_impact_audio[0];sample=campaign_pain_audio_resolve(&audio,group);
    memcpy(rf_scene_npc_impact_audio+9,owner->body.state.position,12);
    if(!audio.status)campaign_pain_audio_play(&audio,owner->body.state.position,sample);
    rf_scene_npc_impact_audio[6]=random->value;
    if(audio.status)++rf_scene_npc_impact_audio[7];return audio.status;
}
int rf_scene_npc_pain_sound(uint32_t handle,float fraction,int32_t now,rf_random_state *random)
{
    uint32_t i,cls;int status;campaign_npc_body *owner;
    rf_entity_damage_sound_state state={0};campaign_pain_audio_context context={random,0,rf_scene_npc_pain_audio,0};
    rf_entity_damage_sound_backend backend={campaign_pain_audio_resolve,campaign_pain_audio_playing,campaign_pain_audio_play,&context};
    if(!random || !isfinite(fraction) || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view)return RF_NOT_FOUND;
    /* Current registered NPCs have no associated player. Death descriptors
     * and player-class overrides require their separate lifecycle owners. */
    if(owner->damage.effects.health<=0)return RF_NOT_FOUND;
    cls=campaign_seeds.items[i].class_index;if(cls>=campaign_seeds.class_count || !campaign_pain_groups)return RF_RANGE;
    state.health=owner->damage.effects.health;state.flags=owner->view.flags_810;
    state.death_descriptor=state.death_class=-1;
    state.light_class=campaign_pain_groups[cls][0];state.heavy_class=campaign_pain_groups[cls][1];
    state.action=owner->view.action_520;state.deadline=owner->pain_sound.deadline;state.voice=owner->pain_sound.voice;
    memcpy(state.position,owner->eye_position,12);++rf_scene_npc_pain_audio[0];
    status=rf_entity_damage_sound(&state,fraction,owner->view.flags_810&1u,0,now,&backend);
    owner->pain_sound.deadline=state.deadline;
    rf_scene_npc_pain_audio[6]=random->value;
    if(context.status || status)++rf_scene_npc_pain_audio[7];
    return context.status?context.status:status;
}
int rf_scene_player_pain_sound(uint32_t handle,float fraction,int32_t now,rf_random_state *random)
{
    rf_entity_damage_sound_state state={0};int status;
    campaign_pain_audio_context context={random,0,rf_scene_player_pain_audio,1};
    rf_entity_damage_sound_backend backend={campaign_pain_audio_resolve,campaign_pain_audio_playing,campaign_pain_audio_play,&context};
    if(!random || !isfinite(fraction) || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!campaign_spawn || !rf_scene_actor_eye_enabled ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&campaign_player_view ||
       campaign_player_damage.state.effects.handle!=handle || !(campaign_player_view.flags_7c&8))return RF_NOT_FOUND;
    state.health=campaign_player_damage.state.effects.health;state.flags=campaign_player_view.flags_810;
    state.death_descriptor=state.death_class=campaign_player_pain_sound.groups[2]; /* Current base/effective class are the same. */
    state.light_class=campaign_player_pain_sound.groups[0];state.heavy_class=campaign_player_pain_sound.groups[1];
    state.action=campaign_player_view.action_520;state.deadline=campaign_player_pain_sound.deadline;state.voice=campaign_player_pain_sound.voice;
    /* Mode0 playback does not consume position; no fabricated entity eye owner. */
    ++rf_scene_player_pain_audio[0];
    status=rf_entity_damage_sound(&state,fraction,campaign_player_view.flags_810&1u,1,now,&backend);
    campaign_player_view.flags_810=state.flags;campaign_player_damage.state.effects.flags_810=state.flags;
    campaign_player_pain_sound.deadline=state.deadline;rf_scene_player_pain_audio[6]=random->value;
    if(context.status || status)++rf_scene_player_pain_audio[7];
    return context.status?context.status:status;
}
int rf_scene_npc_damage_ai(uint32_t handle,uint32_t source)
{
    uint32_t i;const rf_entity_view *view=rf_entity_lookup(&campaign_entities,(int32_t)handle);
    if(!view)return RF_NOT_FOUND;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view==view &&
        campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;
    /* Damage41a350 always passes mode0 to407fb0. Every mode0 route returns
     * before effects when the source is absent or the actor itself. */
    if(source==handle || !rf_object_registry_lookup(&campaign_registry,source))return RF_OK;
    return RF_NOT_FOUND; /* A live different source needs full AI reactions. */
}
static uint32_t campaign_damage_test_predicate(void *c,uint32_t kind,uint32_t handle)
{(void)c;(void)kind;(void)handle;return 0;}
static uint32_t campaign_damage_test_uid(void *c,int32_t uid)
{(void)c;(void)uid;rf_scene_npc_damage_test_words[63]++;return UINT32_MAX;}
static int campaign_damage_test_source(void *c,uint32_t handle,uint32_t *affiliation)
{(void)c;(void)handle;*affiliation=0;rf_scene_npc_damage_test_words[63]++;return 0;}
static uint32_t campaign_damage_test_burn(void *c,uint32_t target,uint32_t source)
{(void)c;(void)target;(void)source;rf_scene_npc_damage_test_words[63]++;return 0;}
static float campaign_damage_test_random(void *c,float low,float high)
{(void)c;(void)low;(void)high;rf_scene_npc_damage_test_words[63]++;return 0;}
static void campaign_damage_test_notify(void *c,uint32_t kind,uint32_t target,float value,uint32_t source)
{
    (void)value;(void)source;
    if(kind==RF_DAMAGE_PAIN_ANIMATION && rf_scene_npc_pain(target,1000,c,NULL))rf_scene_npc_damage_test_words[63]++;
    if(kind==RF_DAMAGE_PAIN_SOUND && rf_scene_npc_pain_sound(target,value,1000,c))rf_scene_npc_damage_test_words[63]++;
    if(kind==RF_DAMAGE_AI_REACTION && rf_scene_npc_damage_ai(target,source))rf_scene_npc_damage_test_words[63]++;
    if(kind!=RF_DAMAGE_PAIN_ANIMATION && kind!=RF_DAMAGE_PAIN_SOUND && kind!=RF_DAMAGE_AI_REACTION)rf_scene_npc_damage_test_words[63]++;
    ++rf_scene_npc_damage_test_words[3];
}
static uint32_t campaign_damage_test_playing(void *c,uint32_t voice)
{(void)c;(void)voice;rf_scene_npc_damage_test_words[63]++;return 0;}
static uint32_t campaign_damage_test_play(void *c,uint32_t target)
{(void)c;(void)target;rf_scene_npc_damage_test_words[63]++;return UINT32_MAX;}
static int campaign_npc_damage_fixture(void)
{
    uint32_t i,pass;campaign_npc_body *owner;const rf_entity_seed_class *definition;rf_random_state random={1};
    rf_runtime_event event={0};rf_level_owned_event authored={0};rf_level_link_target link;
    rf_runtime_triggers triggers=campaign_triggers;rf_runtime_damage_backend damage_backend={0};int status=RF_OK;
    rf_startup_events_report report;
    rf_damage_effect_backend effects={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
        campaign_damage_test_burn,campaign_damage_test_random,campaign_damage_test_notify,campaign_damage_test_playing,campaign_damage_test_play,&random};
    memset(rf_scene_npc_damage_test_words,0,sizeof(rf_scene_npc_damage_test_words));
    memset(rf_scene_npc_pain_test_words,0,sizeof(rf_scene_npc_pain_test_words));
    memset(rf_scene_npc_pain_sound_test,0,sizeof(rf_scene_npc_pain_sound_test));
    memset(rf_scene_npc_impact_audio,0,sizeof(rf_scene_npc_impact_audio));rf_scene_npc_impact_audio[5]=UINT32_MAX;
    memset(rf_scene_npc_pain_audio,0,sizeof(rf_scene_npc_pain_audio));rf_scene_npc_pain_audio[5]=UINT32_MAX;
    if(rf_scene_npc_damage_test_uid==UINT32_MAX)return RF_OK;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view &&
        (uint32_t)campaign_seeds.records.items[i].record.uid==rf_scene_npc_damage_test_uid)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;
    owner=campaign_npc_bodies+i;definition=campaign_seeds.classes+campaign_seeds.items[i].class_index;
    rf_scene_npc_damage_test_words[1]=rf_scene_npc_damage_test_uid;rf_scene_npc_damage_test_words[2]=owner->registration.handle;
    memcpy(rf_scene_npc_damage_test_words+4,&owner->damage,56);
    rf_scene_npc_damage_test_words[18]=owner->object_flags;rf_scene_npc_damage_test_words[19]=definition->physics.flags;
    memcpy(rf_scene_npc_damage_test_words+20,definition->damage_factors,44);
    { /* Stale generation must neither hit nor rewrite the retained owner. */
        rf_entity_damage_state saved=owner->damage;uint32_t flags=owner->object_flags;
        rf_damage_request request={10,UINT32_MAX,2,0,UINT32_MAX,0};float result=1;
        int status=rf_scene_npc_damage(owner->registration.handle^0x10000u,&request,1,0x3f800000,&effects,&result);
        if(status || result!=0 || flags!=owner->object_flags || memcmp(&saved,&owner->damage,sizeof(saved)))return RF_FORMAT;
    }
    /* Explicit diagnostic event, not an authored level event. Exercise the
     * registered type17 runtime path before broad campaign backend attachment. */
    rf_scene_npc_event_damage_services services={&effects,1,0x3f800000,0,0,0,1000,NULL};
    status=rf_scene_npc_event_damage_bind(&services,&damage_backend.effects);if(status)return status;
    damage_backend.frame_seconds=.25f;triggers.damage_backend=&damage_backend;
    event.object_kind=6;event.authored=&authored;event.links=&link;event.state.type=17;event.state.deadline=-1;
    authored.record.words[0]=40;authored.record.link_count=1;link=(rf_level_link_target){owner->registration.handle,1,0};
    status=rf_object_registry_insert(&campaign_registry,&event,&event.handle);if(status)return status;
    for(pass=0;pass<2;++pass) {
        float result;services.status=0;services.dispatches=0;authored.record.words[1]=pass?UINT32_MAX:2;
        status=rf_runtime_event_fire(&triggers,event.handle,UINT32_MAX,UINT32_MAX,1000,&scene_gravity,NULL,NULL,&report);
        if(!status)status=services.status;
        if(!status && (services.dispatches!=1 || report.unsupported_actions))status=RF_FORMAT;
        result=services.last_amount;
        rf_scene_npc_damage_test_words[0]=(uint32_t)status;if(status)break;
        memcpy(rf_scene_npc_damage_test_words+(pass?47:31),&owner->damage,56);
        rf_scene_npc_damage_test_words[pass?61:45]=owner->object_flags;
        memcpy(rf_scene_npc_damage_test_words+(pass?62:46),&result,4);
        memcpy(rf_scene_npc_pain_test_words+pass*5,&owner->pain,16);
        rf_scene_npc_pain_test_words[pass*5+4]=random.value;
        memcpy(rf_scene_npc_pain_sound_test+pass*5,&owner->pain_sound,8);
        rf_scene_npc_pain_sound_test[pass*5+2]=rf_scene_npc_pain_audio[5];
        rf_scene_npc_pain_sound_test[pass*5+3]=random.value;
        rf_scene_npc_pain_sound_test[pass*5+4]=rf_scene_npc_pain_audio[2];
    }
    {int removed=rf_object_registry_remove(&campaign_registry,event.handle);if(!status)status=removed;}
    if(!status) {
        /* Explicit audio fixture, not a lethal gameplay impact. */
        rf_random_state impact_random={1},before=impact_random;float position[3];uint32_t kept[12];
        memcpy(position,owner->body.state.position,12);memcpy(kept,rf_scene_npc_impact_audio,sizeof(kept));
        if(rf_scene_npc_impact_sound(owner->registration.handle^0x10000,&impact_random)!=RF_NOT_FOUND ||
           memcmp(&before,&impact_random,sizeof(before)) || memcmp(kept,rf_scene_npc_impact_audio,sizeof(kept)))status=RF_FORMAT;
        owner->body.state.position[1]+=2;
        if(!status)status=rf_scene_npc_impact_sound(owner->registration.handle,&impact_random);
        if(!status && memcmp(rf_scene_npc_impact_audio+9,owner->body.state.position,12))status=RF_FORMAT;
        memcpy(owner->body.state.position,position,12);
    }
    return status?status:rf_scene_npc_damage_test_words[63]?RF_FORMAT:RF_OK;
}

static float campaign_jump_strength;
uint32_t rf_scene_player_pain_test[21]; /* three deadline/voice/sample/RNG/play/group/flat-voice snapshots */
uint32_t rf_scene_player_damage_audio_test[18]; /* health/armor/amount/flash/dispatches/unexpected notifications */
uint32_t rf_scene_player_death_audio_test[18]; /* two health/armor/amount/flags/flash/plays/RNG/deadline/voice snapshots */
static uint32_t campaign_player_fixture_notifications;
static void campaign_player_fixture_notify(void *context,uint32_t kind,uint32_t target,float value,uint32_t source)
{(void)context;(void)kind;(void)target;(void)value;(void)source;++campaign_player_fixture_notifications;}
static int campaign_player_pain_fixture(void)
{
    rf_random_state random={1};uint32_t pass,i;int status=RF_OK;
    rf_runtime_event event={0};rf_level_owned_event authored={0};rf_level_link_target link;
    rf_runtime_triggers triggers=campaign_triggers;rf_runtime_damage_backend damage_backend={0};
    rf_damage_effect_backend effects={campaign_damage_test_predicate,campaign_damage_test_uid,campaign_damage_test_source,
        campaign_damage_test_burn,campaign_damage_test_random,campaign_player_fixture_notify,campaign_damage_test_playing,campaign_damage_test_play,&random};
    rf_scene_event_damage_services services={&effects,1,0x3f800000,0,0,0,1000,&random};
    rf_startup_events_report report;
    memset(rf_scene_player_pain_test,0,sizeof(rf_scene_player_pain_test));
    memset(rf_scene_player_damage_audio_test,0,sizeof(rf_scene_player_damage_audio_test));campaign_player_fixture_notifications=0;
    memset(rf_scene_player_death_audio_test,0,sizeof(rf_scene_player_death_audio_test));
    if(rf_scene_npc_damage_test_uid==UINT32_MAX)return RF_OK;
    status=rf_scene_event_damage_bind(&services,&damage_backend.effects);if(status)return status;
    damage_backend.frame_seconds=.25f;triggers.damage_backend=&damage_backend;
    event.object_kind=6;event.authored=&authored;event.links=&link;event.state.type=17;event.state.deadline=-1;
    authored.record.link_count=1;authored.record.words[1]=UINT32_MAX;
    link=(rf_level_link_target){(uint32_t)campaign_player_view.handle,1,0};
    status=rf_object_registry_insert(&campaign_registry,&event,&event.handle);if(status)return status;
    for(pass=0;pass<3;++pass) {
        uint32_t *words=rf_scene_player_pain_test+pass*7,*damage=rf_scene_player_damage_audio_test+pass*6;
        services.now_ms=pass==2?2000:1000;services.clock_bits=pass==2?0x40000000:0x3f800000;
        services.dispatches=0;authored.record.words[0]=pass==2?160:40;
        status=rf_runtime_event_fire(&triggers,event.handle,UINT32_MAX,UINT32_MAX,services.now_ms,&scene_gravity,NULL,NULL,&report);
        if(!status)status=services.status;if(status)break;
        memcpy(damage,&campaign_player_damage.state.effects.health,8);memcpy(damage+2,&services.last_amount,4);
        damage[3]=campaign_player_flash.alpha;damage[4]=services.dispatches;damage[5]=campaign_player_fixture_notifications;
        words[0]=(uint32_t)campaign_player_pain_sound.deadline;words[1]=(uint32_t)campaign_player_pain_sound.voice;
        words[2]=rf_scene_player_pain_audio[5];words[3]=random.value;words[4]=rf_scene_player_pain_audio[2];
        words[5]=(uint32_t)campaign_player_pain_sound.groups[pass==2?1:0];
        for(i=0;i<RF_AUDIO_VOICES;++i)if(campaign_audio_mixer.voices[i].active &&
            campaign_spatial_voices[i].handle && !campaign_spatial_voices[i].positional)++words[6];
    }
    for(pass=0;!status && pass<2;++pass) {
        uint32_t *death=rf_scene_player_death_audio_test+pass*9;
        services.now_ms=pass?4000:3000;services.clock_bits=pass?0x40800000:0x40400000;
        services.dispatches=0;authored.record.words[0]=1200;
        status=rf_screen_flash_reset(&campaign_player_flash);if(status)break;
        status=rf_runtime_event_fire(&triggers,event.handle,UINT32_MAX,UINT32_MAX,services.now_ms,&scene_gravity,NULL,NULL,&report);
        if(!status)status=services.status;if(status)break;
        memcpy(death,&campaign_player_damage.state.effects.health,8);memcpy(death+2,&services.last_amount,4);
        death[3]=campaign_player_view.flags_810;death[4]=campaign_player_flash.alpha;
        death[5]=rf_scene_player_pain_audio[2];death[6]=random.value;
        death[7]=(uint32_t)campaign_player_pain_sound.deadline;death[8]=(uint32_t)campaign_player_pain_sound.voice;
    }
    {int removed=rf_object_registry_remove(&campaign_registry,event.handle);if(!status)status=removed;}
    /* Record damage flash above, then keep the existing movement replay's
     * presentation schedule independent of this pre-frame fixture. */
    if(!status)status=rf_screen_flash_reset(&campaign_player_flash);
    return status?status:campaign_player_fixture_notifications?RF_FORMAT:RF_OK;
}
static rf_player_climb_state campaign_climb;
uint32_t rf_scene_player_climb_frames[128][9]; /* frame, region, mode, position XYZ, velocity XYZ before motion */
uint32_t rf_scene_player_climb[8]; /* queries, enters, exits, region, mode, bytes, sounds, sound ID */

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
int rf_scene_death_clearance(const rf_geometry_collision_world *world,uint32_t handle,uint32_t direction,
    rf_entity_death_obstacle *scratch,uint32_t capacity,uint32_t *allowed)
{
    rf_entity_death_clearance_state target={0};uint32_t slot,count=0,found=0;
    if(!world || !allowed || (!scratch && capacity))return RF_RANGE;
    if(!campaign_spawn || !rf_entity_lookup(&campaign_entities,(int32_t)handle))return RF_NOT_FOUND;
    for(slot=0;slot<RF_OBJECT_SLOTS;slot++)if(campaign_entities.slots[slot]) {
        const rf_entity_view *view=campaign_entities.slots[slot];
        rf_entity_death_clearance_state state={0};uint32_t word,actor;
        if(rf_entity_lookup(&campaign_entities,view->handle)!=view)return RF_FORMAT;
        if(count==capacity)return RF_RANGE;
        if(view==&campaign_player_view) {
            memcpy(state.position,rf_scene_actor_pose.public_position,12);memcpy(state.matrix,scene_actor_body.state.orientation,36);
            state.model_radius_78=campaign_player_geometry.model_radius;state.extent_180=scene_actor_body.state.bounds.radius;
            memcpy(&word,&campaign_player_geometry.eye_limits.minimum[2],4);
        } else {
            for(actor=0;actor<campaign_npc_body_count;actor++)if(view==&campaign_npc_bodies[actor].view)break;
            if(actor==campaign_npc_body_count || actor>=campaign_seeds.records.count)return RF_NOT_FOUND;
            if(campaign_seeds.items[actor].class_index>=campaign_seeds.class_count)return RF_FORMAT;
            memcpy(state.position,campaign_npc_bodies[actor].published,12);
            memcpy(state.matrix,campaign_seeds.records.items[actor].record.orientation,36);
            state.model_radius_78=campaign_npc_bodies[actor].model_radius_78;state.extent_180=campaign_npc_bodies[actor].body.state.bounds.radius;
            memcpy(&word,&campaign_seeds.classes[campaign_seeds.items[actor].class_index].eye_limits.minimum[2],4);
        }
        memcpy(scratch[count].position,state.position,12);scratch[count].extent_180=state.extent_180;scratch[count].class_word_74=word;++count;
        if((uint32_t)view->handle==handle){target=state;found=1;}
    }
    if(!found)return RF_NOT_FOUND;
    return rf_geometry_death_clearance(world,&campaign_movers,&target,direction,scratch,count,allowed);
}

uint32_t rf_scene_actor_initial_world[8],rf_scene_actor_initial_fall[8];
typedef struct campaign_death_selection_query {
    rf_scene_death_selection_context *input;uint32_t handle;int status;
} campaign_death_selection_query;
static uint32_t campaign_death_selection_clearance(void *context,uint32_t direction)
{
    campaign_death_selection_query *q=context;uint32_t allowed=0;
    if(!q->status)q->status=rf_scene_death_clearance(q->input->world,q->handle,direction,q->input->scratch,q->input->capacity,&allowed);
    return allowed;
}
int rf_scene_npc_death_select(void *context,uint32_t handle,int32_t *action)
{
    rf_scene_death_selection_context *input=context;campaign_death_selection_query query={input,handle,0};
    rf_entity_death_selection selection;rf_entity_pose *pose;campaign_npc_body *owner;
    rf_random_state random;int32_t result;uint32_t i,cls;int status;
    if(!input || !input->world || !input->random || !action)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view || owner->view.weapons[0]!=-1)return RF_NOT_FOUND;
    status=campaign_actor_pose(i,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
    if(!campaign_seeds.items || i>=campaign_seeds.records.count)return RF_RANGE;cls=campaign_seeds.items[i].class_index;
    if(!campaign_motion_catalog.mappings || cls>=campaign_motion_catalog.mapping_count)return RF_RANGE;
    if(campaign_motion_catalog.mappings[cls].weapon!=-1 || campaign_motion_catalog.mappings[cls].skeleton!=pose->skeleton)return RF_NOT_FOUND;
    selection.flags_810=owner->view.flags_810;selection.current_138c=pose->controller.current;selection.next_1390=pose->controller.next;
    selection.action_824=owner->death.action_824;memcpy(selection.motions,campaign_motion_catalog.mappings[cls].actions,sizeof(selection.motions));
    random=*input->random;status=rf_entity_death_select(&selection,campaign_death_selection_clearance,&query,&random,&result);
    if(query.status)return query.status;if(status)return status;
    *input->random=random;*action=result;return RF_OK;
}
uint32_t rf_scene_death_animation_test_enabled,rf_scene_death_animation_test[8];
static int campaign_death_animation_fixture(const rf_geometry_collision_world *world,uint32_t frame,rf_random_state *random)
{
    uint32_t i,count=0;campaign_npc_body *owner;rf_entity_death_obstacle *scratch;int status;
    rf_scene_death_selection_context context;rf_scene_death_motion_ops ops={rf_scene_npc_death_select,rf_scene_npc_death_sound,&context};
    if(!frame){memset(rf_scene_death_animation_test,0,sizeof(rf_scene_death_animation_test));memset(rf_scene_npc_action_audio,0,sizeof(rf_scene_npc_action_audio));}
    if(!rf_scene_death_animation_test_enabled || frame!=120)return RF_OK;
    if(!random)return RF_NOT_FOUND;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && (uint32_t)campaign_seeds.records.items[i].record.uid==rf_scene_npc_damage_test_uid)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    for(i=0;i<RF_OBJECT_SLOTS;++i)if(campaign_entities.slots[i])++count;
    scratch=malloc(count*sizeof(*scratch));if(!scratch)return RF_IO;
    context.world=world;context.scratch=scratch;context.capacity=count;context.random=random;
    owner->death.requested_83c=owner->death.action_824=-1;
    rf_scene_death_animation_test[0]=frame;rf_scene_death_animation_test[1]=owner->registration.handle;
    rf_scene_death_animation_test[2]=random->value;
    status=rf_scene_npc_death_motion(owner->registration.handle,&ops);
    rf_scene_death_animation_test[3]=random->value;rf_scene_death_animation_test[4]=(uint32_t)owner->death.action_824;
    rf_scene_death_animation_test[5]=owner->view.flags_810;rf_scene_death_animation_test[6]=(uint32_t)status;
    rf_scene_death_animation_test[7]=count*sizeof(*scratch);free(scratch);return status;
}
uint32_t rf_scene_death_clearance_test[8]; /* passes, queries, allowed, blocked, candidates, hash, status, scratch bytes */
static int campaign_death_clearance_fixture(const rf_geometry_collision_world *world,uint32_t frame)
{
    rf_entity_death_obstacle *scratch;uint32_t slot,count=0,direction,allowed;int status=RF_OK;
    if(!frame) {memset(rf_scene_death_clearance_test,0,sizeof(rf_scene_death_clearance_test));rf_scene_death_clearance_test[5]=2166136261u;}
    if(!campaign_spawn || rf_scene_npc_damage_test_uid==UINT32_MAX || (frame!=1 && frame!=90))return RF_OK;
    for(slot=0;slot<RF_OBJECT_SLOTS;slot++)if(campaign_entities.slots[slot])++count;
    if(!count || count>RF_OBJECT_SLOTS)return RF_FORMAT;
    scratch=malloc(count*sizeof(*scratch));if(!scratch)return RF_IO;
    rf_scene_death_clearance_test[4]=count;rf_scene_death_clearance_test[7]=count*sizeof(*scratch);
    for(slot=0;slot<RF_OBJECT_SLOTS && !status;slot++)if(campaign_entities.slots[slot]) {
        uint32_t handle=(uint32_t)campaign_entities.slots[slot]->handle;
        for(direction=0;direction<2 && !status;direction++) {
            uint32_t row[4]={frame,handle,direction,0};
            status=rf_scene_death_clearance(world,handle,direction,scratch,count,&allowed);if(status)break;
            row[3]=allowed;++rf_scene_death_clearance_test[1];++rf_scene_death_clearance_test[allowed?2:3];
            rf_scene_death_clearance_test[5]=npc_hash_bytes(rf_scene_death_clearance_test[5],row,sizeof(row));
            rf_scene_death_clearance_test[5]=npc_hash_bytes(rf_scene_death_clearance_test[5],scratch,count*sizeof(*scratch));
        }
    }
    free(scratch);rf_scene_death_clearance_test[6]=(uint32_t)status;if(!status)++rf_scene_death_clearance_test[0];return status;
}
uint32_t rf_scene_actor_render_frames[64][5]; /* vertices, hash, body position bits */
typedef struct actor_ground_record {
    rf_physics_ground_probe probe;
    rf_geometry_world_sweep_hit hit;
    uint32_t matched;
} actor_ground_record;
_Static_assert(sizeof(actor_ground_record)==132,"Guest ground record layout");
actor_ground_record rf_scene_actor_ground_records[64];
rf_geometry_body_hit rf_scene_actor_ground_contacts[64];
static float campaign_support_velocity[3];
static uint32_t campaign_support_handle;
static rf_collision_contact_extra campaign_player_contact;
static uint32_t campaign_player_material;
const float *rf_scene_collision_extra_velocity(void *context,uint32_t handle)
{
    const rf_entity_view *view;uint32_t i;(void)context;
    view=rf_entity_lookup(&campaign_entities,(int32_t)handle);if(!view || view->type!=0)return NULL;
    if(campaign_spawn && campaign_player_object.view==view && campaign_player_object.handle==handle &&
       view==&campaign_player_view)return campaign_support_velocity;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view==view &&
       campaign_npc_bodies[i].registration.handle==handle)return campaign_npc_bodies[i].support_velocity;
    return NULL;
}

/*41e370 reads support object144, distinct from actor extra velocity8a0. */
static const float *campaign_object_velocity(uint32_t handle)
{
    void *object=rf_object_registry_lookup(&campaign_registry,handle);uint32_t i;
    if(!object)return NULL;
    if(campaign_spawn && object==&campaign_player_object && campaign_player_object.handle==handle &&
        campaign_player_object.view==&campaign_player_view)return scene_actor_body.state.velocity;
    for(i=0;i<campaign_npc_body_count;++i)if(object==&campaign_npc_bodies[i].registration &&
        campaign_npc_bodies[i].registration.handle==handle && campaign_npc_bodies[i].registration.view==&campaign_npc_bodies[i].view)
        return campaign_npc_bodies[i].body.state.velocity;
    for(i=0;i<campaign_mover_count;++i)if(object==campaign_mover_wrappers+i && campaign_mover_wrappers[i].handle==handle &&
        campaign_mover_wrappers[i].pose)return campaign_mover_wrappers[i].pose->velocity;
    return NULL;
}
uint32_t rf_scene_npc_support_refresh[6]; /* ticks,actors,resolved,fixture cases/hash,errors */
int rf_scene_npc_refresh_support(uint32_t handle)
{
    uint32_t i,mode;campaign_npc_body *owner;const float *velocity;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view &&
        campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view || owner->view.type!=0)return RF_NOT_FOUND;
    if(owner->movement_slot>=16)return RF_RANGE;
    mode=campaign_modes[owner->movement_slot].index;
    velocity=(mode==1 || mode==3)?campaign_object_velocity(owner->support.handle):NULL;
    rf_physics_support_refresh(mode,velocity,owner->support_velocity,&owner->body.state.flags,&owner->object_flags);
    owner->view.flags_7c=owner->object_flags;
    ++rf_scene_npc_support_refresh[1];if(velocity)++rf_scene_npc_support_refresh[2];return RF_OK;
}
static int campaign_npc_refresh_support_tick(void)
{
    uint32_t i;int status;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view) {
        status=rf_scene_npc_refresh_support(campaign_npc_bodies[i].registration.handle);
        if(status){++rf_scene_npc_support_refresh[5];return status;}
    }
    ++rf_scene_npc_support_refresh[0];return RF_OK;
}
extern uint32_t rf_scene_actor_pair_test_enabled;
static int campaign_npc_refresh_support_fixture(uint32_t frame)
{
    campaign_npc_body *owner=NULL,saved;float *sources[3],original[3][3];uint32_t handles[5],i,slot,source;int status=RF_OK;
    if(frame)return RF_OK;memset(rf_scene_npc_support_refresh,0,sizeof(rf_scene_npc_support_refresh));
    if(!rf_scene_actor_pair_test_enabled)return RF_OK;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view){owner=campaign_npc_bodies+i;break;}
    if(!owner || !campaign_mover_count || !campaign_player_object.view)return RF_NOT_FOUND;
    saved=*owner;handles[0]=0;handles[1]=owner->registration.handle^0x10000;
    handles[2]=owner->registration.handle;handles[3]=campaign_player_object.handle;handles[4]=campaign_mover_wrappers[0].handle;
    sources[0]=owner->body.state.velocity;sources[1]=scene_actor_body.state.velocity;sources[2]=campaign_mover_wrappers[0].pose->velocity;
    for(i=0;i<3;++i){memcpy(original[i],sources[i],12);sources[i][0]=3+i;sources[i][1]=-7-(float)i;sources[i][2]=11+i;}
    rf_scene_npc_support_refresh[4]=2166136261u;
    for(slot=0;slot<16 && !status;++slot)for(source=0;source<5 && !status;++source) {
        float expected[3]={-1,2,-3};uint32_t body_flags=0,object_flags=0,mode=campaign_modes[slot].index;
        owner->movement_slot=slot;owner->support.handle=handles[source];owner->body.state.flags=0;owner->object_flags=owner->view.flags_7c=0;
        memcpy(owner->support_velocity,expected,12);
        rf_physics_support_refresh(mode,source>=2?sources[source-2]:NULL,expected,&body_flags,&object_flags);
        status=rf_scene_npc_refresh_support(owner->registration.handle);
        if(!status && (memcmp(expected,owner->support_velocity,12) || body_flags!=owner->body.state.flags ||
            object_flags!=owner->object_flags || object_flags!=owner->view.flags_7c))status=RF_FORMAT;
        if(status)break;
        ++rf_scene_npc_support_refresh[3];rf_scene_npc_support_refresh[4]=npc_hash_bytes(rf_scene_npc_support_refresh[4],expected,12);
        rf_scene_npc_support_refresh[4]=npc_hash_bytes(rf_scene_npc_support_refresh[4],&body_flags,4);
        rf_scene_npc_support_refresh[4]=npc_hash_bytes(rf_scene_npc_support_refresh[4],&object_flags,4);
    }
    for(i=0;i<3;++i)memcpy(sources[i],original[i],12);*owner=saved;
    if(status)++rf_scene_npc_support_refresh[5];return status;
}
uint32_t rf_scene_actor_ground_queries[4]; /* queries, hits, mover hits, status */
uint32_t rf_scene_actor_ground_stats[8]; /* magic, records, hits, walkable, first walkable frame, hash, stride, status */
uint32_t rf_scene_actor_landing[8]; /* magic, descriptor index, frame, landings, grounded ticks, status, support commits, support losses */
int rf_scene_player_collision_view(uint32_t handle,rf_collision_pair_actor_state *result)
{
    rf_collision_pair_actor_state value={0};uint32_t slot=rf_scene_actor_landing[1];
    if(!result)return RF_RANGE;
    if(!campaign_spawn || !campaign_player_object.view || campaign_player_object.handle!=handle ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&campaign_player_view ||
       campaign_player_view.type!=0 || !(campaign_player_view.flags_7c&8))return RF_NOT_FOUND;
    if(slot>=16 || !scene_actor_body.allocated_bytes || !campaign_player_model ||
       !campaign_player_model->model || !campaign_player_model->bones || !campaign_player_model->bone_count ||
       !campaign_player_model->matrices || !campaign_player_model->playback)return RF_RANGE;
    value.body_flags=scene_actor_body.state.flags;value.model=RF_SCENE_PLAYER_MODEL;
    value.movement_mode=campaign_modes[slot].index;value.handle=handle;
    value.parent_handle=(uint32_t)campaign_player_view.linked_handle;value.object_flags=campaign_player_view.flags_7c;
    memcpy(value.position,rf_scene_actor_pose.public_position,12);
    memcpy(value.forward,scene_actor_body.state.orientation+6,12);
    *result=value;return RF_OK;
}
int rf_scene_player_collision_response(uint32_t handle,rf_collision_actor_general_response *result)
{
    rf_collision_pair_actor_state view;int status;
    if(!result)return RF_RANGE;
    status=rf_scene_player_collision_view(handle,&view);if(status)return status;
    return collision_body_response(&scene_actor_body,&campaign_player_contact,handle,campaign_player_material,view.kind,result);
}
int rf_scene_player_collision_publish(uint32_t handle,uint32_t body_flags,const rf_collision_actor_contact *contact)
{
    rf_collision_pair_actor_state view;int status;
    if(!contact)return RF_RANGE;
    status=rf_scene_player_collision_view(handle,&view);if(status)return status;
    status=rf_collision_contact_write(&scene_actor_body.state,&campaign_player_contact,contact);
    if(!status)scene_actor_body.state.flags=body_flags;return status;
}
uint32_t rf_scene_collision_views[8]; /* frames, player hash, NPC hash/count, player model, NPC models, status, last frame */
int rf_scene_actor_pair_response(uint32_t first,uint32_t second,uint32_t normal_mode,uint32_t *changed)
{
    rf_collision_actor_general_response actors[2];uint32_t handles[2]={first,second},player[2],i,value;int status;
    if(!changed || first==second || normal_mode>1)return RF_RANGE;
    /* Validate both owners before any response mutation. The only callback is
     * our read-only actor velocity resolver; owners stay stable through commit. */
    for(i=0;i<2;++i) {
        player[i]=campaign_spawn && campaign_player_object.view && campaign_player_object.handle==handles[i];
        status=player[i]?rf_scene_player_collision_response(handles[i],actors+i):rf_scene_npc_collision_response(handles[i],actors+i);
        if(status)return status;
    }
    value=normal_mode?rf_collision_actors_normal_response(&actors[0].actor,&actors[1].actor,rf_scene_collision_extra_velocity,NULL):
        rf_collision_actors_general_response(actors,actors+1,rf_scene_collision_extra_velocity,NULL);
    for(i=0;i<2;++i) {
        status=player[i]?rf_scene_player_collision_publish(handles[i],actors[i].actor.body_flags,&actors[i].actor.contact):
            rf_scene_npc_collision_publish(handles[i],actors[i].actor.body_flags,&actors[i].actor.contact);
        if(status)return status;
    }
    *changed=value;return RF_OK;
}
typedef struct scene_model_response_context {
    uint32_t slot,handle;rf_collision_model_target target;rf_entity_pose *pose;int status;
} scene_model_response_context;
static const rf_collision_model_target *scene_model_response_target(void *context,uint32_t handle)
{
    scene_model_response_context *c=context;
    if(handle!=c->handle){c->status=RF_RANGE;return NULL;}return &c->target;
}
static uint32_t scene_model_response_geometry(void *context,uint32_t operation,const void *geometry,
    const void *pose,int32_t part,const void *input,rf_collision_model_response_hit *hit,uint32_t reset)
{
    scene_model_response_context *c=context;rf_collision_model_part_query query={0};uint32_t accepted=0;
    if(c->status)return 0;
    if(operation!=RF_MODEL_QUERY_POSE || pose!=c->pose || part!=-1 ||
        geometry!=campaign_collision_models.items+c->pose->skeleton){c->status=RF_RANGE;return 0;}
    memcpy(&query.input,input,sizeof(query.input));
    c->status=rf_scene_model_collision_query(c->slot,&query,hit,reset,&accepted);return c->status?0:accepted;
}
static uint32_t scene_model_response_query(void *context,uint32_t model,const rf_collision_solid_response_query *query,
    rf_collision_model_response_hit *hit)
{
    scene_model_response_context *c=context;rf_collision_model_query_view view;
    rf_collision_model_query_backend backend={scene_model_response_geometry,c};
    if(c->status)return 0;if(model!=c->slot+1){c->status=RF_RANGE;return 0;}
    view.kind=2;view.geometry=campaign_collision_models.items+c->pose->skeleton;
    /* One published pose: no original148-byte record arithmetic is needed. */
    view.pose_records=(const uint8_t *)c->pose;view.pose_count=1;
    return rf_collision_model_query_all(&view,query,hit,0,&backend);
}
int rf_scene_actor_model_response(uint32_t first,uint32_t target,uint32_t *changed)
{
    rf_collision_actor_general_response actors[2];rf_collision_pair_actor_state target_view;
    scene_model_response_context context={0};rf_collision_model_response_backend backend={scene_model_response_target,scene_model_response_query,&context};
    uint32_t player,cls,value;campaign_npc_body *owner;int status;
    if(!changed || first==target)return RF_RANGE;
    player=campaign_spawn && campaign_player_object.view && campaign_player_object.handle==first;
    status=player?rf_scene_player_collision_response(first,actors):rf_scene_npc_collision_response(first,actors);if(status)return status;
    status=rf_scene_npc_collision_view(target,&target_view);if(status)return status;
    if(!target_view.model)return RF_NOT_FOUND;context.slot=target_view.model-1;context.handle=target;
    status=rf_scene_npc_collision_response(target,actors+1);if(status)return status;
    status=campaign_actor_pose(context.slot,&context.pose);if(status)return status;
    if(!context.pose || !campaign_collision_models.items || context.pose->skeleton>=campaign_collision_models.count)return RF_RANGE;
    owner=campaign_npc_bodies+context.slot;cls=campaign_seeds.items[context.slot].class_index;
    if(cls>=campaign_seeds.class_count)return RF_RANGE;
    context.target.armor=owner->damage.effects.armor;context.target.class_flags_724=campaign_seeds.classes[cls].physics.flags;
    context.target.flags_814=owner->damage.effects.flags_814;
    value=rf_collision_actor_model_response(actors,actors+1,target_view.model,campaign_seeds.classes[cls].physics.use_kind,&backend);
    if(context.status)return context.status;
    status=player?rf_scene_player_collision_publish(first,actors[0].actor.body_flags,&actors[0].actor.contact):
        rf_scene_npc_collision_publish(first,actors[0].actor.body_flags,&actors[0].actor.contact);if(status)return status;
    status=rf_scene_npc_collision_publish(target,actors[1].actor.body_flags,&actors[1].actor.contact);if(status)return status;
    *changed=value;return RF_OK;
}
typedef struct scene_pair_process_context {
    const void *identities[2];rf_collision_pair_actor_state views[2],empty;uint32_t next,hits;int status;
} scene_pair_process_context;
uint32_t rf_scene_pair_dispatch[5]; /* calls, normal, general, model, errors */
static int scene_pair_view(const void *identity,rf_collision_pair_actor_state *view)
{
    uint32_t i;
    if(campaign_spawn && campaign_player_object.view && identity==campaign_player_object.view)
        return rf_scene_player_collision_view(campaign_player_object.handle,view);
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view && identity==campaign_npc_bodies[i].registration.view)
        return rf_scene_npc_collision_view(campaign_npc_bodies[i].registration.handle,view);
    return RF_NOT_FOUND;
}
static const rf_collision_pair_actor_state *scene_pair_actor(void *context,const void *identity)
{
    scene_pair_process_context *c=context;uint32_t slot;
    if(c->status)return &c->empty;
    slot=c->identities[0]==identity?0:c->identities[1]==identity?1:c->next;
    c->status=scene_pair_view(identity,c->views+slot);if(c->status)return &c->empty;
    c->identities[slot]=identity;c->next=slot^1u;return c->views+slot;
}
static uint32_t scene_pair_response(void *context,uint32_t operation,const void *first,const void *second)
{
    scene_pair_process_context *c=context;rf_collision_pair_actor_state a,b;uint32_t changed=0;
    if(c->status)return 0;
    c->status=scene_pair_view(first,&a);if(!c->status)c->status=scene_pair_view(second,&b);if(c->status)return 0;
    if(operation==RF_PAIR_RESPONSE_MODEL)c->status=rf_scene_actor_model_response(a.handle,b.handle,&changed);
    else if(operation==RF_PAIR_RESPONSE_MODES1 || operation==RF_PAIR_RESPONSE_GENERAL)
        c->status=rf_scene_actor_pair_response(a.handle,b.handle,operation==RF_PAIR_RESPONSE_MODES1,&changed);
    else c->status=RF_NOT_FOUND;
    ++rf_scene_pair_dispatch[0];
    if(c->status){++rf_scene_pair_dispatch[4];return 0;}
    ++rf_scene_pair_dispatch[operation==RF_PAIR_RESPONSE_MODES1?1:operation==RF_PAIR_RESPONSE_GENERAL?2:3];
    c->hits+=changed!=0;return changed;
}
int rf_scene_actor_pairs_process(rf_collision_pair_list *pairs,uint32_t *hits)
{
    scene_pair_process_context context={0};rf_collision_pair_list available={0};rf_collision_pair *node;
    rf_collision_pair_process_backend backend={scene_pair_actor,scene_pair_response,&context};uint32_t i;int status;
    if(!pairs || !hits || pairs->count>8192)return RF_RANGE;
    /* Validate the bounded actor-only list before publishing any response.
     * Identities are registered entity-view pointers; never dereference an
     * unvalidated identity. Actor-only pairs cannot expire or enter triggers. */
    node=pairs->head;
    for(i=0;i<pairs->count;++i) {
        rf_collision_pair_actor_state a,b;
        if(!node || node->first==node->second)return RF_RANGE;
        status=scene_pair_view(node->first,&a);if(!status)status=scene_pair_view(node->second,&b);if(status)return status;
        node=node->next;
    }
    if(node)return RF_RANGE;
    rf_collision_pairs_process(pairs,&available,&backend);
    if(context.status)return context.status;
    if(available.count)return RF_FORMAT;
    *hits=context.hits;return RF_OK;
}
uint32_t rf_scene_collision_responses[6]; /* frames, player hash, NPC hash/count, status, last frame */
uint32_t rf_scene_actor_pair_test_enabled,rf_scene_actor_pair_test[8];
static int campaign_actor_pair_fixture(uint32_t frame)
{
    rf_physics_body *bodies[3],saved[3];rf_collision_contact_extra *extras[3],saved_extra[3];
    rf_physics_sphere spheres[3]={{0}};uint32_t handles[3],found=0,i,route,normal,scenario;int status=RF_OK;
    if(frame)return RF_OK;memset(rf_scene_actor_pair_test,0,sizeof(rf_scene_actor_pair_test));
    if(!rf_scene_actor_pair_test_enabled)return RF_OK;
    for(i=0;i<campaign_npc_body_count && found<2;++i)if(campaign_npc_bodies[i].registration.view) {
        bodies[found]=&campaign_npc_bodies[i].body;extras[found]=&campaign_npc_bodies[i].collision_contact;
        handles[found++]=campaign_npc_bodies[i].registration.handle;
    }
    if(found!=2){rf_scene_actor_pair_test[5]=(uint32_t)RF_NOT_FOUND;return RF_NOT_FOUND;}
    bodies[2]=&scene_actor_body;extras[2]=&campaign_player_contact;handles[2]=campaign_player_object.handle;
    for(i=0;i<3;++i){saved[i]=*bodies[i];saved_extra[i]=*extras[i];spheres[i].radius=1;}
    rf_scene_actor_pair_test[4]=2166136261u;
    for(route=0;route<3;++route)for(normal=0;normal<2;++normal)for(scenario=0;scenario<3;++scenario) {
        uint32_t ids[2]={route==1?2:0,route==2?2:1},changed,expected_changed;
        rf_collision_actor_general_response expected[2],actual;
        for(i=0;i<2;++i) {
            uint32_t id=ids[i],k;rf_physics_body *body=bodies[id];
            *body=saved[id];memset(&body->state,0,sizeof(body->state));memset(extras[id],0x35,sizeof(*extras[id]));
            body->spheres.items=spheres+id;body->spheres.count=1;
            body->state.mass=2+i;body->state.bounds.radius=1;body->state.scalar_144=(scenario==2 && i==1)?0:1;
            body->state.flags=(scenario==2 && i==0)?0x40000000:0;
            for(k=0;k<3;++k){body->state.bounds.minimum[k]=-10;body->state.bounds.maximum[k]=10;body->state.orientation[k*4]=body->state.next_orientation[k*4]=1;}
            body->state.position[2]=i?2.5f:0;body->state.next_position[2]=i?-1.5f:0;
            if(scenario==1 && i==1)body->state.bounds.minimum[0]=10;
            status=id==2?rf_scene_player_collision_response(handles[id],expected+i):rf_scene_npc_collision_response(handles[id],expected+i);if(status)goto done;
        }
        expected_changed=normal?rf_collision_actors_normal_response(&expected[0].actor,&expected[1].actor,rf_scene_collision_extra_velocity,NULL):
            rf_collision_actors_general_response(expected,expected+1,rf_scene_collision_extra_velocity,NULL);
        status=rf_scene_actor_pair_response(handles[ids[0]],handles[ids[1]],normal,&changed);if(status)goto done;
        if(changed!=expected_changed){status=RF_FORMAT;goto done;}
        for(i=0;i<2;++i) {
            status=ids[i]==2?rf_scene_player_collision_response(handles[ids[i]],&actual):rf_scene_npc_collision_response(handles[ids[i]],&actual);if(status)goto done;
            if(memcmp(&actual,expected+i,sizeof(actual))){status=RF_FORMAT;goto done;}
            rf_scene_actor_pair_test[4]=npc_hash_bytes(rf_scene_actor_pair_test[4],&actual.actor.contact,68);
            rf_scene_actor_pair_test[4]=npc_hash_bytes(rf_scene_actor_pair_test[4],&actual.actor.body_flags,4);
        }
        if((scenario==0 && changed!=1) || (scenario==1 && changed!=0)){status=RF_FORMAT;goto done;}
        if(scenario==2 && !normal) {
            if(changed || bodies[ids[0]]->state.flags!=0x60000000 || bodies[ids[0]]->state.scalar_144!=0){status=RF_FORMAT;goto done;}
            ++rf_scene_actor_pair_test[3];
        }
        ++rf_scene_actor_pair_test[0];++rf_scene_actor_pair_test[changed?1:2];
    }
done:
    for(i=0;i<3;++i){*bodies[i]=saved[i];*extras[i]=saved_extra[i];++rf_scene_actor_pair_test[6];}
    rf_scene_actor_pair_test[5]=(uint32_t)status;rf_scene_actor_pair_test[7]=frame;return status;
}
uint32_t rf_scene_actor_model_test[6]; /* cases, hits, misses, contact hash, status, restored */
static int campaign_actor_model_fixture(uint32_t frame)
{
    rf_physics_body *bodies[3],saved[3];rf_collision_contact_extra *extras[3],saved_extra[3];
    uint32_t handles[3],slots[2],found=0,i,route,axis,direction,miss;float sphere[4];int status=RF_OK;
    if(frame)return RF_OK;memset(rf_scene_actor_model_test,0,sizeof(rf_scene_actor_model_test));
    if(!rf_scene_actor_pair_test_enabled)return RF_OK;
    for(i=0;i<campaign_npc_body_count && found<2;++i)if(campaign_npc_bodies[i].registration.view) {
        slots[found]=i;handles[found]=campaign_npc_bodies[i].registration.handle;
        bodies[found]=&campaign_npc_bodies[i].body;extras[found]=&campaign_npc_bodies[i].collision_contact;++found;
    }
    if(found!=2)return RF_NOT_FOUND;
    bodies[2]=&scene_actor_body;extras[2]=&campaign_player_contact;handles[2]=campaign_player_object.handle;
    for(i=0;i<3;++i){saved[i]=*bodies[i];saved_extra[i]=*extras[i];}
    status=rf_model_file_bound_sphere(&campaign_render_models.items[campaign_model_owners[slots[0]].pose->skeleton].file,sphere);if(status)goto done;
    rf_scene_actor_model_test[3]=2166136261u;
    for(route=0;route<2;++route)for(axis=0;axis<3;++axis)for(direction=0;direction<2;++direction)for(miss=0;miss<2;++miss) {
        uint32_t first=route?1:2,changed=99,k;rf_physics_sphere probes[2]={{0}};rf_collision_actor_general_response actual[2];
        probes[0].radius=.05f;probes[1].radius=.25f;probes[1].center[(axis+1)%3]=.1f;
        for(i=0;i<2;++i) {
            uint32_t id=i?0:first;rf_physics_body *body=bodies[id];
            *body=saved[id];memset(&body->state,0,sizeof(body->state));memset(extras[id],0,sizeof(*extras[id]));
            body->state.mass=i?100:80;body->state.scalar_144=1;body->state.bounds.radius=1;
            for(k=0;k<3;++k){body->state.bounds.minimum[k]=-1000;body->state.bounds.maximum[k]=1000;body->state.orientation[k*4]=body->state.next_orientation[k*4]=1;}
            if(!i) {
                body->spheres.items=probes;body->spheres.count=2;memcpy(body->state.position,sphere,12);memcpy(body->state.next_position,sphere,12);
                body->state.position[axis]+=(direction?1:-1)*(sphere[3]+1);body->state.next_position[axis]-=(direction?1:-1)*(sphere[3]+1);
                if(miss){body->state.position[(axis+1)%3]+=100;body->state.next_position[(axis+1)%3]+=100;}
            }
        }
        {
            rf_collision_pair_record record={0};rf_collision_pair_list list={&record.pair,1};
            const void *source=first==2?(const void *)campaign_player_object.view:(const void *)campaign_npc_bodies[slots[1]].registration.view;
            const void *target=campaign_npc_bodies[slots[0]].registration.view;
            bodies[first]->state.flags|=0x40000000u;
            record.pair.first=direction?target:source;record.pair.second=direction?source:target;record.flags=direction?2:4;
            status=rf_scene_actor_pairs_process(&list,&changed);if(status)goto done;
            if(list.count!=1 || list.head!=&record.pair || record.pair.next){status=RF_FORMAT;goto done;}
        }
        status=first==2?rf_scene_player_collision_response(handles[first],actual):rf_scene_npc_collision_response(handles[first],actual);if(status)goto done;
        status=rf_scene_npc_collision_response(handles[0],actual+1);if(status)goto done;
        if(miss && changed){status=RF_FORMAT;goto done;}
        if(changed && (!(actual[0].actor.contact.time<1) || actual[0].actor.contact.handle!=handles[0] ||
            actual[1].actor.contact.handle!=handles[first] || actual[1].actor.contact.time!=actual[0].actor.contact.time)){status=RF_FORMAT;goto done;}
        for(i=0;i<2;++i)rf_scene_actor_model_test[3]=npc_hash_bytes(rf_scene_actor_model_test[3],&actual[i].actor.contact,68);
        ++rf_scene_actor_model_test[0];++rf_scene_actor_model_test[changed?1:2];
    }
 done:
    for(i=0;i<3;++i){*bodies[i]=saved[i];*extras[i]=saved_extra[i];++rf_scene_actor_model_test[5];}
    rf_scene_actor_model_test[4]=(uint32_t)status;return status;
}
static int campaign_pair_dispatch_fixture(uint32_t frame)
{
    uint32_t slot,i,mode,saved_modes[16],hits;rf_physics_body saved[2];rf_collision_contact_extra extra[2];int status=RF_OK;
    rf_physics_body *body[2];rf_collision_contact_extra *contact[2];const void *identity[2];uint32_t handles[2];rf_physics_sphere spheres[2]={{0}};
    if(frame || !rf_scene_actor_pair_test_enabled)return RF_OK;
    for(slot=0;slot<campaign_npc_body_count;++slot)if(campaign_npc_bodies[slot].registration.view)break;
    if(slot==campaign_npc_body_count)return RF_NOT_FOUND;
    body[0]=&scene_actor_body;body[1]=&campaign_npc_bodies[slot].body;contact[0]=&campaign_player_contact;contact[1]=&campaign_npc_bodies[slot].collision_contact;
    identity[0]=campaign_player_object.view;identity[1]=campaign_npc_bodies[slot].registration.view;handles[0]=campaign_player_object.handle;handles[1]=campaign_npc_bodies[slot].registration.handle;
    for(i=0;i<2;++i){saved[i]=*body[i];extra[i]=*contact[i];spheres[i].radius=1;}
    for(i=0;i<16;++i)saved_modes[i]=campaign_modes[i].index;
    for(mode=0;mode<3;++mode) {
        rf_collision_actor_general_response expected[2],actual;uint32_t changed;
        rf_collision_pair_record record={0};rf_collision_pair_list list={&record.pair,1};
        for(i=0;i<16;++i)campaign_modes[i].index=mode==1?1:2;
        for(i=0;i<2;++i) {
            uint32_t k;*body[i]=saved[i];memset(&body[i]->state,0,sizeof(body[i]->state));memset(contact[i],0,sizeof(*contact[i]));
            body[i]->spheres.items=spheres+i;body[i]->spheres.count=1;body[i]->state.mass=2+i;body[i]->state.scalar_144=1;body[i]->state.bounds.radius=1;
            body[i]->state.flags=i?0:0x40000000u;body[i]->state.position[2]=i?2.5f:0;body[i]->state.next_position[2]=i?-1.5f:0;
            for(k=0;k<3;++k){body[i]->state.bounds.minimum[k]=-10;body[i]->state.bounds.maximum[k]=10;body[i]->state.orientation[k*4]=body[i]->state.next_orientation[k*4]=1;}
            status=i?rf_scene_npc_collision_response(handles[i],expected+i):rf_scene_player_collision_response(handles[i],expected+i);if(status)goto done;
        }
        changed=mode==1?rf_collision_actors_normal_response(&expected[0].actor,&expected[1].actor,rf_scene_collision_extra_velocity,NULL):
            rf_collision_actors_general_response(expected,expected+1,rf_scene_collision_extra_velocity,NULL);
        record.pair.first=identity[0];record.pair.second=identity[1];record.flags=mode?0x20:0;
        status=rf_scene_actor_pairs_process(&list,&hits);if(status)goto done;
        if(hits!=(changed!=0)){status=RF_FORMAT;goto done;}
        for(i=0;i<2;++i) {
            status=i?rf_scene_npc_collision_response(handles[i],&actual):rf_scene_player_collision_response(handles[i],&actual);if(status)goto done;
            if(memcmp(&actual,expected+i,sizeof(actual))){status=RF_FORMAT;goto done;}
        }
    }
 done:
    for(i=0;i<2;++i){*body[i]=saved[i];*contact[i]=extra[i];}
    for(i=0;i<16;++i)campaign_modes[i].index=saved_modes[i];return status;
}
static uint32_t collision_response_hash(uint32_t hash,const rf_collision_actor_general_response *view,const float *extra_velocity)
{
    uint32_t words[57],i;
    _Static_assert(offsetof(rf_collision_actor_response,spheres)==80,"response scalar prefix");
    memcpy(words,&view->actor,80);memcpy(words+20,&view->actor.contact,68);
    memcpy(words+37,view->orientation,36);memcpy(words+46,view->next_orientation,36);
    memcpy(words+55,&view->extent,4);words[56]=view->kind;
    hash=npc_hash_bytes(hash,words,sizeof(words));
    /* Addresses and sphere opaque_14 are not semantic portable state. */
    for(i=0;i<(uint32_t)view->actor.sphere_count;++i)hash=npc_hash_bytes(hash,view->actor.spheres+i,20);
    hash=npc_hash_bytes(hash,extra_velocity,12);
    return hash;
}
static uint32_t collision_view_hash(uint32_t hash,const rf_collision_pair_actor_state *view)
{
    uint32_t words[16]={view->kind,view->body_flags,view->model,view->movement_mode,view->handle,
        view->parent_handle,view->object_flags,view->trigger_filter,(uint32_t)view->allowed_count,0};
    /* Pointer-free wire layout: actor views have no trigger handle array. */
    memcpy(words+10,view->position,12);memcpy(words+13,view->forward,12);
    return npc_hash_bytes(hash,words,sizeof(words));
}
static int campaign_collision_views_check(uint32_t frame)
{
    rf_collision_pair_actor_state view;rf_collision_actor_general_response response;const float *extra;uint32_t i,models=0;int status;
    if(!frame)memset(rf_scene_pair_dispatch,0,sizeof(rf_scene_pair_dispatch));
    status=campaign_actor_pair_fixture(frame);if(status)return status;
    status=campaign_actor_model_fixture(frame);if(status)return status;
    status=campaign_pair_dispatch_fixture(frame);if(status)return status;
    if(!frame){memset(rf_scene_collision_views,0,sizeof(rf_scene_collision_views));rf_scene_collision_views[1]=rf_scene_collision_views[2]=2166136261u;}
    if(!frame){memset(rf_scene_collision_responses,0,sizeof(rf_scene_collision_responses));rf_scene_collision_responses[1]=rf_scene_collision_responses[2]=2166136261u;}
    status=rf_scene_player_collision_view(campaign_player_object.handle,&view);if(status)goto done;
    rf_scene_collision_views[1]=collision_view_hash(rf_scene_collision_views[1],&view);rf_scene_collision_views[4]=view.model;
    status=rf_scene_player_collision_response(campaign_player_object.handle,&response);if(status)goto done;
    extra=rf_scene_collision_extra_velocity(NULL,campaign_player_object.handle);if(!extra){status=RF_NOT_FOUND;goto done;}
    rf_scene_collision_responses[1]=collision_response_hash(rf_scene_collision_responses[1],&response,extra);
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view) {
        status=rf_scene_npc_collision_view(campaign_npc_bodies[i].registration.handle,&view);if(status)goto done;
        rf_scene_collision_views[2]=collision_view_hash(rf_scene_collision_views[2],&view);
        ++rf_scene_collision_views[3];models+=view.model!=0;
        status=rf_scene_npc_collision_response(campaign_npc_bodies[i].registration.handle,&response);if(status)goto done;
        extra=rf_scene_collision_extra_velocity(NULL,campaign_npc_bodies[i].registration.handle);if(!extra){status=RF_NOT_FOUND;goto done;}
        rf_scene_collision_responses[2]=collision_response_hash(rf_scene_collision_responses[2],&response,extra);++rf_scene_collision_responses[3];
    }
    ++rf_scene_collision_views[0];rf_scene_collision_views[5]=models;
    ++rf_scene_collision_responses[0];
done:
    rf_scene_collision_responses[4]=(uint32_t)status;rf_scene_collision_responses[5]=frame;
    rf_scene_collision_views[6]=(uint32_t)status;rf_scene_collision_views[7]=frame;return status;
}
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
    if(!status && campaign_spawn) {free(campaign_surface_palette);campaign_surface_palette=palette;palette=NULL;}
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
    movement.mode=frame?(int32_t)rf_scene_actor_landing[1]:(campaign_spawn?1:3);
    if(campaign_spawn && !frame)memset(movement.vector,0,sizeof(movement.vector));
    movement.direction=rf_scene_actor_movement_settings.mode;
    /* Ordinary unarmed candidates from 41f6ee..41f729. Priority/AI candidate
     * selection remains outside this miner fixture. */
    movement.idle_state=0;movement.move_state=2;movement.alternate_state=4;
    /* 41f270 creation-field audit: miner1's default handgun and player flag
     * select armed candidates. Inventory/weapon view ownership remains open. */
    if(campaign_spawn){movement.idle_state=1;movement.move_state=3;movement.alternate_state=5;}
    if(campaign_spawn && frame) {
        rf_player_motion_input input={0};int32_t selected;
        memcpy(input.direction,movement.vector,12);input.entity_present=1;input.parent_kind=-1;
        input.crouched=campaign_crouched;input.free_motion=movement.mode==3 || movement.mode==8;
        input.swim_motion=movement.mode==4 || movement.mode==7;input.attachment_75c=-1;
        input.primary_weapon=0; /* Present default handgun; inventory owner remains separate. */
        status=rf_player_motion_choose(&input,&selected);if(status)return status;
        if(selected>=0 && !rf_motion_has_state(controller,selected)) {
            status=rf_motion_request_state(controller,motions,selected,.25f);if(status)return status;
        }
        movement.idle_state=selected;movement.move_state=movement.alternate_state=-1;
    } else {status=rf_motion_select_movement(controller,motions,&movement);if(status)return status;}
    record=rf_scene_actor_locomotion_frames[frame%64];record[0]=campaign_spawn && frame?2:1;
    record[1]=(uint32_t)movement.mode;record[2]=(uint32_t)movement.direction;
    memcpy(record+3,movement.vector,12);record[6]=movement.idle_state;record[7]=movement.move_state;record[8]=movement.alternate_state;
    record[9]=(uint32_t)controller->current;record[10]=(uint32_t)controller->next;
    memcpy(record+11,&controller->duration,4);return RF_OK;
}
static int campaign_body_query(const rf_geometry_collision_world *world,const rf_collision_body_query *query,
    rf_geometry_body_hit *contact,uint32_t *matched)
{
    rf_geometry_materials mapping={0};rf_geometry_body_surfaces surfaces;
    if(!actor_follow_world || !campaign_surface_palette || !campaign_surface_sources)return RF_RANGE;
    mapping.offsets=actor_follow_world->offsets;mapping.slots=actor_follow_world->slots;
    mapping.count=actor_follow_world->geometry_count;mapping.textures.count=actor_follow_world->material_count;
    surfaces.geometries=campaign_surface_sources;surfaces.count=mapping.count;
    surfaces.mapping=&mapping;surfaces.palette=campaign_surface_palette;
    return rf_geometry_collision_body_sweep(world,&campaign_movers,query,campaign_sweep_scratch,
        campaign_movers.count,rf_geometry_body_surface,&surfaces,contact,matched);
}
int rf_scene_npc_ground_query(const rf_geometry_collision_world *world,uint32_t handle,float elapsed,
    rf_physics_ground_probe *probe,rf_collision_actor_contact *contact,uint32_t *matched)
{
    campaign_npc_body *owner;rf_entity_pose *pose;uint32_t cls,falling,found,i;int status;
    rf_physics_ground_probe prepared;rf_collision_body_sphere sphere;rf_collision_body_query query={0};
    rf_geometry_body_hit hit={0};rf_collision_actor_contact candidate;
    if(!world || !probe || !contact || !matched)return RF_RANGE;
    status=campaign_npc_motion_owner(handle,&owner,&cls,&pose);if(status)return status;
    if(owner->movement_slot>=16 || !campaign_npc_movement_configs || !campaign_seeds.classes)return RF_RANGE;
    /* NPC path: player-only49b900 prequery remains in the player adapter. */
    if(owner->object_flags&8)return RF_RANGE;
    falling=rf_entity_falling((int32_t)campaign_modes[owner->movement_slot].index,campaign_seeds.classes[cls].physics.use_kind,owner->support.material);
    status=rf_physics_ground_prepare(owner->body.spheres.items,owner->body.spheres.count,owner->body.state.next_position,
        owner->body.state.state_124,(int)falling,elapsed,campaign_npc_movement_configs[cls].base_speed,owner->support_velocity[1],&prepared);if(status)return status;
    memcpy(sphere.center,prepared.sphere.center,12);sphere.radius=prepared.sphere.radius;
    memcpy(query.start,prepared.start,12);memcpy(query.end,prepared.end,12);for(i=0;i<3;++i)query.matrix[i][i]=1;
    query.radius=prepared.bounds.radius;query.flags=prepared.query_flags;query.spheres=&sphere;query.count=1;query.limit=1;
    candidate=*contact;candidate.time=1;candidate.reserved_1ec=0;
    status=campaign_body_query(world,&query,&hit,&found);if(status)return status;
    _Static_assert(sizeof(hit.contact)==sizeof(candidate),"original contact payload wire");
    if(found)memcpy(&candidate,&hit.contact,sizeof(candidate));
    *probe=prepared;*contact=candidate;*matched=found;return RF_OK;
}
/* Caller owns conversion scratch; no per-query allocation or player globals. */
static int campaign_physics_body_sweep(const rf_geometry_collision_world *world,
    const rf_physics_body_state *state,const rf_physics_spheres *source,uint32_t flags,
    rf_collision_body_sphere *scratch,uint32_t capacity,rf_geometry_body_hit *hit,uint32_t *matched)
{
    rf_collision_body_query query={0};rf_geometry_body_hit value={0};uint32_t i,found;int status;
    if(!world || !state || !source || !hit || !matched || source->count>capacity ||
        (source->count && (!source->items || !scratch)))return RF_RANGE;
    if(state->position[0]==state->next_position[0] && state->position[1]==state->next_position[1] &&
        state->position[2]==state->next_position[2]){*matched=0;return RF_OK;}
    for(i=0;i<source->count;++i){memcpy(scratch[i].center,source->items[i].center,12);scratch[i].radius=source->items[i].radius;}
    memcpy(query.start,state->position,12);memcpy(query.end,state->next_position,12);
    memcpy(query.matrix,state->orientation,36);query.radius=state->bounds.radius;
    query.flags=flags;query.spheres=scratch;query.count=source->count;query.limit=1;
    status=campaign_body_query(world,&query,&value,&found);if(status)return status;
    if(found)*hit=value;*matched=found;return RF_OK;
}
int rf_scene_npc_body_sweep(const rf_geometry_collision_world *world,uint32_t handle,
    const rf_physics_body_state *proposal,uint32_t flags,rf_collision_body_sphere *scratch,
    uint32_t capacity,rf_geometry_body_hit *hit,uint32_t *matched)
{
    uint32_t i;campaign_npc_body *owner;
    if(!proposal || !hit || !matched)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view &&
        campaign_npc_bodies[i].registration.handle==handle)break;
    if(i==campaign_npc_body_count)return RF_NOT_FOUND;owner=campaign_npc_bodies+i;
    if(rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view || owner->view.type!=0)return RF_NOT_FOUND;
    return campaign_physics_body_sweep(world,proposal,&owner->body.spheres,flags,scratch,capacity,hit,matched);
}
/* Diagnostic follow-up to short misses, never a substitute for4a0840 depth.
 * Extend only the endpoint by16 units and propose contact on private storage. */
static void campaign_npc_deep_probe(const rf_geometry_collision_world *world,const campaign_npc_body *owner,
    const rf_physics_ground_probe *original,uint32_t uid)
{
    rf_physics_ground_probe probe=*original;rf_collision_body_query query={0};rf_collision_body_sphere sphere;
    rf_geometry_body_hit hit={0};rf_physics_body_state state=owner->body.state;
    rf_physics_support_contact support=owner->support;float published[3];uint32_t record[20]={0},matched=0,i;int status;
    record[0]=uid;memcpy(record+6,probe.sphere.center,12);memcpy(record+9,&probe.sphere.radius,4);
    memcpy(published,owner->published,12);probe.end[1]-=16;
    memcpy(record+10,&probe.start[1],4);memcpy(record+11,&probe.end[1],4);
    memcpy(sphere.center,probe.sphere.center,12);sphere.radius=probe.sphere.radius;
    memcpy(query.start,probe.start,12);memcpy(query.end,probe.end,12);
    for(i=0;i<3;++i)query.matrix[i][i]=1;
    query.radius=probe.bounds.radius;query.flags=probe.query_flags;query.spheres=&sphere;query.count=1;query.limit=1;
    status=campaign_body_query(world,&query,&hit,&matched);if(status)goto done;
    record[1]=matched;if(!matched)goto done;
    ++rf_scene_npc_support_deep[1];record[3]=hit.solid;record[4]=hit.face;record[5]=hit.contact.material;
    memcpy(record+12,&hit.contact.fraction,4);memcpy(record+13,hit.contact.normal,12);memcpy(record+16,hit.contact.point,12);
    if(hit.contact.fraction<1 && hit.contact.normal[1]>=.5f) {
        ++rf_scene_npc_support_deep[2];
        status=rf_physics_support_accept(&state,&probe,hit.contact.fraction,hit.solid!=UINT32_MAX,
            hit.contact.velocity[1],hit.contact.object_id,(int32_t)hit.contact.material,&support,published);
        if(!status){memcpy(record+19,&state.position[1],4);if(memcmp(state.position,owner->body.state.position,12))++rf_scene_npc_support_deep[7];}
    }
done:
    record[2]=(uint32_t)status;
    if(status){++rf_scene_npc_support_deep[3];if(rf_scene_npc_support_deep[3]==1){rf_scene_npc_support_deep[4]=uid;rf_scene_npc_support_deep[5]=(uint32_t)status;}}
    if(!rf_scene_npc_support_deep[0])memcpy(rf_scene_npc_support_deep_first,record,sizeof(record));
    ++rf_scene_npc_support_deep[0];rf_scene_npc_support_deep[6]=npc_hash_bytes(rf_scene_npc_support_deep[6],record,sizeof(record));
}
/* Startup diagnostic only: query actual NPC shapes, commit only to a private
 * body copy. The unlinked/cleared actor intent is the existing startup fixture;
 * this is not the post-physics487e00 phase or an AI/landing implementation. */
static void campaign_npc_support_probe(const rf_geometry_collision_world *world,const campaign_npc_body *owner,
    const rf_entity_physics_config *config,float class_speed,uint32_t uid)
{
    rf_physics_body_state state=owner->body.state;rf_physics_support_contact support=owner->support;
    rf_physics_ground_probe probe;rf_collision_body_query query={0};rf_collision_body_sphere sphere;
    rf_geometry_body_hit hit={0};rf_entity_support_gate gate={0};float published[3];
    uint32_t record[16]={0},matched=0,i,flags=owner->object_flags;int status;
    memcpy(published,owner->published,12);record[0]=uid;++rf_scene_npc_support[0];
    gate.movement_mode=campaign_modes[owner->movement_slot].index;gate.linked_handle=-1;
    gate.falling=rf_entity_falling(gate.movement_mode,config->authored.use_kind,support.material);
    gate.moved=rf_entity_support_moved(&flags,owner->previous,state.position);
    gate.body_flags=state.flags;gate.special=(flags&8)!=0;
    record[1]=(uint32_t)rf_entity_support_route(&gate);record[2]=gate.falling;
    if(record[1]!=RF_ENTITY_SUPPORT_QUERY){++rf_scene_npc_support[2];goto done;}
    ++rf_scene_npc_support[1];
    status=rf_physics_ground_prepare(owner->body.spheres.items,owner->body.spheres.count,state.next_position,
        state.state_124,(int)gate.falling,1.0f/30.0f,class_speed,0,&probe);
    if(status)goto failed;
    memcpy(sphere.center,probe.sphere.center,12);sphere.radius=probe.sphere.radius;
    memcpy(query.start,probe.start,12);memcpy(query.end,probe.end,12);
    for(i=0;i<3;++i)query.matrix[i][i]=1;
    query.radius=probe.bounds.radius;query.flags=probe.query_flags;query.spheres=&sphere;query.count=1;query.limit=1;
    status=campaign_body_query(world,&query,&hit,&matched);if(status)goto failed;
    record[3]=matched;record[4]=hit.solid;record[5]=hit.face;record[6]=hit.contact.material;
    memcpy(record+8,&hit.contact.fraction,4);memcpy(record+9,hit.contact.normal,12);
    memcpy(record+12,&probe.start[1],4);memcpy(record+13,&probe.end[1],4);memcpy(record+14,&state.position[1],4);
    if(!matched || hit.contact.fraction>=1) {
        if(!rf_scene_npc_support[3]) {rf_scene_npc_support[10]=uid;memcpy(rf_scene_npc_support_first_miss,record,sizeof(record));}
        ++rf_scene_npc_support[3];campaign_npc_deep_probe(world,owner,&probe,uid);goto done;
    }
    if(!(hit.contact.normal[1]>=.5f)){++rf_scene_npc_support[4];goto done;}
    ++rf_scene_npc_support[hit.solid==UINT32_MAX?5:6];
    /* Numeric proposal only. Moving-object acceptance, impact/landing and
     * actor publication are not applied to the live owner by this diagnostic. */
    status=rf_physics_support_accept(&state,&probe,hit.contact.fraction,hit.solid!=UINT32_MAX,
        hit.contact.velocity[1],hit.contact.object_id,(int32_t)hit.contact.material,&support,published);
    if(status)goto failed;
    memcpy(record+15,&state.position[1],4);if(memcmp(state.position,owner->body.state.position,12))++rf_scene_npc_support[11];
    goto done;
failed:
    record[7]=(uint32_t)status;if(!rf_scene_npc_support[7])rf_scene_npc_support[9]=uid;++rf_scene_npc_support[7];
done:
    rf_scene_npc_support[8]=npc_hash_bytes(rf_scene_npc_support[8],record,sizeof(record));
}
static int actor_ground_query_state(const rf_geometry_collision_world *world,const rf_physics_body_state *state,
    actor_ground_record *r,rf_geometry_body_hit *contact)
{
    float start[3],delta[3];uint32_t k;int status;
    memset(r,0,sizeof(*r));
    memset(contact,0,sizeof(*contact));contact->solid=UINT32_MAX;
    /* Original falling/grounded depths and retained support velocity. Queries
     * are retained every frame; commits obey the grounded movement gate. */
    status=rf_physics_ground_prepare(scene_actor_body.spheres.items,scene_actor_body.spheres.count,
        state->position,state->state_124,rf_scene_actor_landing[1]==3,
        scene_step_seconds,rf_scene_actor_movement_values.speed,campaign_spawn?campaign_support_velocity[1]:0,&r->probe);if(status)return status;
    if(campaign_spawn) {
        rf_collision_body_sphere sphere;rf_collision_body_query query={0};
        memcpy(sphere.center,r->probe.sphere.center,12);sphere.radius=r->probe.sphere.radius;
        memcpy(query.start,r->probe.start,12);memcpy(query.end,r->probe.end,12);
        for(k=0;k<3;k++)query.matrix[k][k]=1;
        query.radius=r->probe.bounds.radius;query.flags=r->probe.query_flags;query.spheres=&sphere;query.count=1;query.limit=1;
        status=campaign_body_query(world,&query,contact,&r->matched);
        ++rf_scene_actor_ground_queries[0];rf_scene_actor_ground_queries[3]=(uint32_t)status;
        if(status)return status;
        if(r->matched) {
            ++rf_scene_actor_ground_queries[1];if(contact->solid!=UINT32_MAX)++rf_scene_actor_ground_queries[2];
            r->hit.hit.fraction=contact->contact.fraction;memcpy(r->hit.hit.point,contact->contact.point,12);
            memcpy(r->hit.hit.normal,contact->contact.normal,12);r->hit.face=contact->face;
            r->hit.room=contact->room;r->hit.hits=contact->hits;r->hit.edge=contact->edge;
        }
        return RF_OK;
    }
    for(k=0;k<3;++k) {
        start[k]=(float)((double)r->probe.start[k]+r->probe.sphere.center[k]);
        delta[k]=(float)((double)r->probe.end[k]-r->probe.start[k]);
    }
    status=rf_geometry_collision_world_sweep(world,r->probe.query_flags,start,delta,r->probe.sphere.radius,1,&r->hit,&r->matched);
    if(status)return status;
    return RF_OK;
}
static int actor_ground_query(const rf_geometry_collision_world *world,actor_ground_record *r,rf_geometry_body_hit *contact)
{return actor_ground_query_state(world,&scene_actor_body.state,r,contact);}
static int actor_support_commit(rf_physics_body_state *state,const actor_ground_record *ground,
    const rf_geometry_body_hit *contact,uint32_t landing)
{
    rf_physics_body_state next=*state;uint32_t handle;int status;
    if(!campaign_spawn)return landing?rf_physics_static_land(state,&ground->probe,ground->hit.hit.fraction):
        rf_physics_static_support(state,&ground->probe,ground->hit.hit.fraction);
    status=rf_physics_support_commit(&next,&ground->probe,ground->hit.hit.fraction,contact->solid!=UINT32_MAX,
        contact->contact.velocity[1],contact->contact.object_id,&handle);if(status)return status;
    if(landing) {
        status=rf_physics_landing_velocity(next.velocity,campaign_support_velocity,contact->contact.velocity,next.velocity);if(status)return status;
        next.velocity[1]=0;next.flags&=~0x200000u; /* Existing ordinary run transition. */
    }
    *state=next;campaign_support_handle=handle;memcpy(campaign_support_velocity,contact->contact.velocity,12);return RF_OK;
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
    status=actor_ground_query(world,r,rf_scene_actor_ground_contacts+(frame%64));if(status)return status;
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
    if(campaign_spawn) {
        rf_collision_body_sphere spheres[8];int status;
        status=campaign_physics_body_sweep(world,state,&scene_actor_body.spheres,query_flags,
            spheres,8,&rf_scene_actor_body_contact,&matched);
        ++rf_scene_actor_body_sweeps[0];rf_scene_actor_body_sweeps[3]=(uint32_t)status;
        if(status)return status;
        if(matched) {
            ++rf_scene_actor_body_sweeps[1];if(rf_scene_actor_body_contact.solid!=UINT32_MAX)++rf_scene_actor_body_sweeps[2];
            *sphere=rf_scene_actor_body_contact.sphere;*fraction=rf_scene_actor_body_contact.contact.fraction;
            memcpy(normal,rf_scene_actor_body_contact.contact.normal,12);
        }
        return RF_OK;
    }
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
    actor_ground_record *r=rf_scene_actor_stance_ground+(frame%64);rf_geometry_body_hit contact;
    uint32_t *d=rf_scene_actor_stance_support[frame%64];int status,walkable;
    d[0]=1;d[1]=rf_scene_actor_landing[1];memcpy(d+3,scene_actor_body.state.position,12);
    status=actor_ground_query(world,r,&contact);if(status)return status;
    walkable=r->matched && r->hit.hit.fraction<1 && r->hit.hit.normal[1]>=.5f;
    if(walkable) {
        if(rf_scene_actor_landing[1]==3) {
            status=actor_support_commit(&scene_actor_body.state,r,&contact,1);if(status)return status;
            rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=frame;
            ++rf_scene_actor_landing[3];rf_scene_actor_landing[5]=1;
        } else {
            status=actor_support_commit(&scene_actor_body.state,r,&contact,0);if(status)return status;
            ++rf_scene_actor_landing[6];
        }
    } else if(rf_scene_actor_landing[1]==1) {
        scene_actor_body.state.flags|=1;rf_scene_actor_landing[1]=3;++rf_scene_actor_landing[7];
    }
    status=rf_group_pose_set_position(&rf_scene_actor_pose,scene_actor_body.state.position);if(status)return status;
    d[2]=rf_scene_actor_landing[1];memcpy(d+6,scene_actor_body.state.position,12);return RF_OK;
}
typedef struct actor_clearance_context {const rf_geometry_collision_world *world;int frame;} actor_clearance_context;
static int actor_stand_clearance(void *context,const float start[3],const float end[3],uint32_t *blocked)
{
    actor_clearance_context *c=context;rf_physics_body_state probe=scene_actor_body.state;
    float normal[3],fraction;uint32_t sphere;int status;
    memcpy(probe.position,start,12);memcpy(probe.next_position,end,12);
    status=actor_sweep(c->world,&probe,normal,&fraction,&sphere,probe.state_124|4);
    if(!status)*blocked=sphere!=UINT32_MAX;return status;
}
static int actor_stand_ground(void *context)
{
    actor_clearance_context *c=context;
    return c->frame<0?RF_OK:actor_stance_ground_commit(c->world,(uint32_t)c->frame);
}
static int actor_stance_update(const rf_geometry_collision_world *world,uint32_t request,int *blocked,int live_frame)
{
    int status;*blocked=0;
    if(request>1)return RF_RANGE;
    int crouched=(rf_scene_actor_stance_flags&0x400)!=0;
    if(crouched!=(int)request) {
        if(!request) {
            const rf_physics_stand_ops ops={actor_stand_clearance,NULL,actor_stand_ground};
            actor_clearance_context context={world,live_frame};int stood;
            /* This fixture has no separate published position or player byte owner. */
            status=rf_physics_try_stand(&scene_actor_body.spheres,&rf_scene_actor_stance_cache,
                scene_actor_body.state.position,&rf_scene_actor_stance_flags,&ops,&context,&stood);if(status)return status;
            *blocked=!stood;
        } else {
            status=rf_physics_stance_centers(&scene_actor_body.spheres,rf_scene_actor_stance_cache.centers[1],
                rf_scene_actor_stance_cache.count,&rf_scene_actor_stance_flags,1);if(status)return status;
            if(live_frame>=0) {status=actor_stance_ground_commit(world,(uint32_t)live_frame);if(status)return status;}
        }
        if(!*blocked) {status=actor_set_speed_mode(request!=0);if(status)return status;}
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
typedef struct actor_stand_context {const rf_geometry_collision_world *world;uint32_t frame;} actor_stand_context;
static int campaign_try_stand(void *context,uint32_t *stood)
{
    actor_stand_context *c=context;int blocked,status;
    status=actor_stance_update(c->world,0,&blocked,(int)c->frame);if(status)return status;
    *stood=!blocked;campaign_crouched=(rf_scene_actor_stance_flags&0x400)!=0;return RF_OK;
}
static void campaign_climb_sound(void *context,const rf_player_climb_state *state,const rf_player_sound_request *request)
{
    (void)context;(void)state; /* Audio backend remains open; retain the request ID. */
    ++rf_scene_player_climb[6];rf_scene_player_climb[7]=(uint32_t)request->sound_id;
}
static int campaign_climb_update(scene_stream *stream,uint32_t frame)
{
    uint32_t index,mode=rf_scene_actor_landing[1],selected=mode;int status;
    const rf_player_movement_region *region;
    status=rf_player_movement_region_find(campaign_regions.items,campaign_regions.count,scene_actor_body.state.position,&index);
    if(status)return status;region=index==UINT32_MAX?NULL:campaign_regions.items+index;
    ++rf_scene_player_climb[0];rf_scene_player_climb[3]=index;
    campaign_climb.speed=rf_scene_actor_movement_settings;
    campaign_climb.movement=campaign_modes+mode;
    campaign_climb.vertical_velocity=scene_actor_body.state.velocity[1];
    if(region && mode!=2 && (mode==1 || region!=campaign_climb.previous_region)) {
        rf_player_climb_input input={0};
        input.region=region;input.descriptors=campaign_modes;input.config=&rf_scene_actor_movement_config;
        input.forced_action=-1;input.entity_scale=scene_actor_body.state.mass;
        input.free_motion=mode==3 || mode==8;input.sound.owner_present=1;
        memcpy(input.sound.position,scene_actor_body.state.position,12);
        status=rf_player_climb_enter(&campaign_climb,&input,&selected,campaign_climb_sound,NULL);if(status)return status;
        if(rf_scene_actor_movement_config.flags&4){mode=selected;++rf_scene_player_climb[1];}
    } else if(!region && mode==2) {
        rf_player_climb_exit_input input={0};actor_stand_context context={stream->collision,frame};
        input.config=&rf_scene_actor_movement_config;input.descriptors=campaign_modes;input.identity=campaign_identity;
        input.default_index=1;input.forced_action=-1;input.entity_scale=scene_actor_body.state.mass;
        input.crouched=campaign_crouched;
        status=rf_player_climb_exit(&campaign_climb,&input,&selected,campaign_try_stand,&context);if(status)return status;
        if(selected!=2){mode=selected;++rf_scene_player_climb[2];}
    }
    rf_scene_actor_movement_settings=campaign_climb.speed;
    rf_scene_actor_landing[1]=mode;scene_actor_body.state.velocity[1]=campaign_climb.vertical_velocity;
    rf_scene_player_climb[4]=mode;
    {uint32_t *record=rf_scene_player_climb_frames[frame%128];record[0]=frame;record[1]=index;record[2]=mode;
     memcpy(record+3,scene_actor_body.state.position,12);memcpy(record+6,scene_actor_body.state.velocity,12);}
    return RF_OK;
}
static void campaign_jump_sound(void *context,const rf_player_jump_state *state,int32_t sound)
{(void)context;(void)state;(void)sound;++rf_scene_player_jump[2]; /* Asset resolution/playback pending. */}
static int campaign_jump_update(uint32_t frame)
{
    uint32_t held=player_poll?player_input.jump:0,pressed,selected=rf_scene_actor_landing[1],accepted=0;
    uint32_t *record=rf_scene_player_jump_frames[frame%128];int status;
    if(!frame){campaign_jump_held=0;memset(rf_scene_player_jump,0,sizeof(rf_scene_player_jump));memset(rf_scene_player_jump_frames,0,sizeof(rf_scene_player_jump_frames));}
    pressed=held && !campaign_jump_held;campaign_jump_held=held;
    if(pressed) {
        rf_player_jump_gate gate={1,0,0,-1,-1,rf_scene_actor_stance_flags,0};
        ++rf_scene_player_jump[0];
        if(rf_player_jump_enabled(&gate)) {
            rf_player_jump_state state={rf_scene_actor_stance_flags,scene_actor_body.state.flags,
                scene_actor_body.state.velocity[1],campaign_modes+selected,campaign_identity,0};
            /* Configured impulse uses the same initialized gravity state as falling.
             * Runtime gravity events do not recompute it; audio remains open. */
            rf_player_jump_input input={campaign_modes,campaign_identity,
                campaign_jump_strength,scene_step_seconds,0,0,-1,0};
            float now=(float)((double)frame*scene_step_seconds);uint32_t sounds=rf_scene_player_jump[2];
            memcpy(&input.now,&now,4);
            status=rf_player_jump(&state,&input,&selected,campaign_jump_sound,NULL);if(status)return status;
            accepted=rf_scene_player_jump[2]!=sounds;
            rf_scene_actor_stance_flags=state.actor_flags;scene_actor_body.state.flags=state.physics_flags;
            scene_actor_body.state.velocity[1]=state.vertical_velocity;rf_scene_actor_landing[1]=selected;
            if(accepted){++rf_scene_player_jump[1];rf_scene_player_jump[3]=frame;}
        }
    }
    record[0]=frame;record[1]=held;record[2]=pressed;record[3]=accepted;record[4]=rf_scene_actor_landing[1];
    memcpy(record+5,scene_actor_body.state.position+1,4);memcpy(record+6,scene_actor_body.state.velocity+1,4);record[7]=rf_scene_actor_stance_flags;
    return RF_OK;
}
static int actor_player_stance(void *context,uint32_t frame,rf_motion_controller *controller,const int32_t motions[23])
{
    int update_status=campaign_climb_update((scene_stream*)context,frame);if(update_status)return update_status;
    rf_motion_stance_decision decision={0,RF_MOTION_STANCE_NONE};
    rf_player_crouch_input eligibility={1,-1,-1,-1,(int32_t)rf_scene_actor_landing[1]};
    /* Ownership/environment/locks are fixture defaults until the player
     * registry and environment lifecycle supply their resolved values. */
    rf_player_stance_gate gate={1,rf_scene_player_climb[3]!=UINT32_MAX,(int32_t)rf_scene_actor_landing[1],
        (int32_t)rf_scene_actor_movement_settings.mode,0,-1,0,0};
    uint32_t request=player_poll?player_input.crouch:0;int status;
    /* Ordinary unattached player fixture. 430c70 owns immediate collision
     * effects before its state-9 request; input locks/vehicles remain separate. */
    if(rf_player_stance_enabled(&gate)) {
        if(request && !campaign_crouched && rf_player_can_crouch(&eligibility))decision.effect=RF_MOTION_STANCE_CROUCH;
        else if(!request && campaign_crouched)decision.effect=RF_MOTION_STANCE_STAND;
    }
    status=actor_selector_effect(context,frame,&decision,controller);if(status)return status;
    campaign_crouched=(rf_scene_actor_stance_flags&0x400)!=0;
    if(decision.effect==RF_MOTION_STANCE_CROUCH && campaign_crouched) {
        status=rf_motion_request_state(controller,motions,9,.25f);if(status)return status;
    }
    /* 430c70 resolves immediate stance before its action loop reaches jump. */
    return campaign_jump_update(frame);
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
static int actor_contact(rf_physics_body_state *state,const float normal[3],const float support[3],
    const float direction[3],uint32_t mode,float *impact)
{
    const float *contact=campaign_spawn?rf_scene_actor_body_contact.contact.velocity:support;
    /* 4a6060 -> 4307a0 writes the resolved command to entity +714. Contact
     * applies the body transform itself. This fixture has no rotating actor;
     * 42a020 is true only for falling/free modes 3 and 8. */
    if(state->flags&0x80)return rf_physics_player_contact(state,normal,support,contact,direction,
        mode,mode==3 || mode==8,impact);
    return rf_physics_static_contact(state,normal,support,contact,impact);
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
        {int status=rf_physics_fall_propose(&proposal,1.0f/60,scene_gravity.acceleration,support);if(status)return status;}
        {int status=actor_sweep(world,&proposal,normal,&fraction,&sphere,0x460);if(status)return status;}
        if(sphere!=UINT32_MAX) {
            /* Stationary non-liquid floor, no rotating actor predicate.
             * Continued substeps and actor pose/room commit remain separate. */
            int status=rf_physics_contact_advance(&proposal,1.0f/60,fraction,rf_scene_actor_contact_time+1);if(status)return status;
            rf_scene_actor_contact_time[0]=proposal.scalar_144;
            status=actor_contact(&proposal,normal,support,support,3,rf_scene_actor_contact);if(status)return status;
            memcpy(rf_scene_actor_contact+1,normal,sizeof(normal));
            memcpy(rf_scene_actor_contact+4,proposal.velocity,sizeof(proposal.velocity));
            current=proposal;
            {
                float remaining=rf_scene_actor_contact_time[1];uint32_t pass=1;
                current.flags|=0x1000000;
                while(remaining>0) {
                    float hit_fraction,impact;uint32_t hit_sphere;
                    status=rf_physics_fall_propose(&current,remaining,scene_gravity.acceleration,support);if(status)return status;
                    status=actor_sweep(world,&current,normal,&hit_fraction,&hit_sphere,0x460);if(status)return status;
                    if(hit_sphere==UINT32_MAX) {
                        memcpy(current.position,current.next_position,sizeof(current.position));current.scalar_144=1;remaining=0;
                    } else {
                        status=rf_physics_contact_advance(&current,remaining,hit_fraction,&remaining);if(status)return status;
                        status=actor_contact(&current,normal,support,support,3,&impact);if(status)return status;
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
static void campaign_force_sound(void *context,const rf_player_force_state *state,const float position[3],uint32_t slot)
{
    const rf_wave_pcm *pcm=rf_audio_bank_sample(&campaign_audio_bank,slot);uint32_t handle;
    (void)context;(void)state;(void)position;
    ++rf_scene_force_ticks[7];
    /* Owned first-person player uses the local 505560 route. The diagnostic
     * mixer/device backend retains its existing unity-gain policy. */
    if(!pcm || rf_audio_voice_start(&campaign_audio_mixer,pcm,32768,32768,0,&handle)) {
        ++rf_scene_live_audio[5];return;
    }
    ++rf_scene_live_audio[4];
    if(campaign_audio_events.play)campaign_audio_events.play(campaign_audio_events_context,handle,pcm,1,1);
}
static int campaign_force_tick(rf_physics_body_state *body,rf_level_particles *particles,int32_t now)
{
    uint32_t index;rf_physics_force_region *region;rf_physics_force_influence influence;int status;float amplitude;
    ++rf_scene_force_ticks[0];
    if(!(body->flags&8))return RF_OK;
    status=rf_physics_force_region_select(campaign_forces.items,campaign_forces.count,rf_scene_actor_pose.public_position,&index);
    if(status)return status;if(index==UINT32_MAX)return RF_OK;
    ++rf_scene_force_ticks[1];region=campaign_forces.items+index;rf_scene_force_ticks[8]=region->uid;
    /* This owner is the registered type-0 local player (object flag8), with no
     * parent/attachment. General actor and player-list ownership remain external. */
    if(!rf_physics_force_eligible(body->flags,1,region->flags,1,1,rf_scene_actor_landing[1]))return RF_OK;
    ++rf_scene_force_ticks[2];
    status=rf_physics_force_region_influence(region,body->position,body->bounds.radius,body->mass,&influence);if(status)return status;
    if(region->flags&0xf0000) {
        if(!particles || !particles->state)return RF_FORMAT;
        status=rf_physics_force_turbulence(&influence,region->flags,scene_step_seconds,&particles->state->random,&amplitude);if(status)return status;
        ++rf_scene_force_ticks[5];rf_scene_force_ticks[9]=particles->state->random.value;
        status=rf_scene_player_feedback((uint32_t)campaign_player_view.handle,amplitude,.05f,now);if(status)return status;
        ++rf_scene_force_ticks[6];
    }
    if(region->flags&0x40) {
        uint32_t selected;rf_player_force_state value={0};rf_player_force_input input={0};
        memcpy(value.velocity,body->velocity,12);value.physics_flags=body->flags;value.alternate_cap=campaign_force_air_limit;
        value.movement=campaign_modes+rf_scene_actor_landing[1];value.orientation=campaign_identity;
        input.influence=influence;memcpy(input.position,body->position,12);input.class_speed=rf_scene_actor_movement_values.speed;
        input.class_flags=campaign_force_class_flags;input.descriptors=campaign_modes;input.identity=campaign_identity;
        status=rf_player_force_replace(&value,&input,&selected,campaign_force_sound,NULL);if(status)return status;
        memcpy(body->velocity,value.velocity,12);body->flags=value.physics_flags;campaign_force_air_limit=value.alternate_cap;
        rf_scene_actor_landing[1]=selected;memcpy(rf_scene_force_ticks+10,&campaign_force_air_limit,4);++rf_scene_force_ticks[4];
    } else {
        status=rf_physics_force_actor_carry(campaign_support_velocity,&body->flags,&influence,
            rf_scene_actor_landing[1],campaign_force_class_kind,-1);if(status)return status;
        ++rf_scene_force_ticks[3];
    }
    return RF_OK;
}
static int actor_trace_contacts=1;
static int actor_tick(const rf_geometry_collision_world *world,rf_physics_body_state *state,const float command[3],const float ground_normal[3])
{
    float remaining=scene_step_seconds,support[3]={0},normal[3];uint32_t pass=0,contacts=0;int status;
    int grounded=rf_scene_actor_landing[1]==1;
    if(campaign_spawn)memcpy(support,campaign_support_velocity,12);
    if(grounded)++rf_scene_actor_landing[4];
    state->flags&=~0x1000000u;
    do {
        float fraction,impact;uint32_t sphere;
        if(campaign_spawn && rf_scene_actor_landing[1]==2) {
            float input[3];
            status=rf_movement_transform(campaign_modes[2].translation,command,
                actor_look.eye_orientation,state->orientation,(const float*)campaign_climb.orientation,input);if(status)return status;
            status=rf_physics_climb_propose(state,remaining,rf_scene_actor_movement_settings.speed,
                rf_scene_actor_movement_values.acceleration,input,support);
        } else if(grounded) {
            float input[3];
            status=rf_movement_transform(rf_scene_actor_movement[0].translation,command,
                state->orientation,state->orientation,state->orientation,input);if(status)return status;
            status=rf_physics_run_propose(state,remaining,rf_scene_actor_movement_settings.speed,
                rf_scene_actor_movement_values.acceleration,rf_scene_actor_run_traction,input,ground_normal,support);
        } else {
            if(campaign_spawn && !(state->flags&0x1000000)) {
                float scaled[3],input[3];uint32_t axis;
                /* 49e780..49e8b7: class acceleration precedes the transform. */
                for(axis=0;axis<3;axis++)scaled[axis]=(float)((double)command[axis]*rf_scene_actor_movement_values.acceleration);
                status=rf_movement_transform(campaign_modes[rf_scene_actor_landing[1]].translation,scaled,
                    actor_look.eye_orientation,state->next_orientation,(const float *)campaign_identity,input);if(status)return status;
                /* Original initialized air-control scalar at 5a00e0 is .5. */
                status=rf_physics_air_steer(state,remaining,.5f,rf_scene_actor_movement_values.acceleration,
                    (state->flags&0x200000)?campaign_force_air_limit:rf_scene_actor_movement_values.speed,input);if(status)return status;
            }
            status=rf_physics_fall_propose(state,remaining,scene_gravity.acceleration,support);
        }
        if(status)return status;
        status=rf_physics_body_prepare_sweep(state);if(status)return status;
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
            memcpy(record+12,support,12);
            memcpy(record+15,campaign_spawn?rf_scene_actor_body_contact.contact.velocity:support,12);
            ++contacts;
            status=rf_physics_contact_advance(state,remaining,fraction,&remaining);if(status)return status;
            status=actor_contact(state,normal,support,command,rf_scene_actor_landing[1],&impact);if(status)return status;
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
    float traction=rf_scene_actor_run_traction,saved_support[3];uint32_t saved_handle=campaign_support_handle;
    memcpy(saved_support,campaign_support_velocity,12);
    memcpy(landing,rf_scene_actor_landing,sizeof(landing));memcpy(ticks,rf_scene_actor_tick_stats,sizeof(ticks));
    memset(rf_scene_actor_routes,0,sizeof(rf_scene_actor_routes));actor_trace_contacts=0;
    for(route=0;route<8;++route) {
        uint32_t *out=rf_scene_actor_routes[route],hash=2166136261u;int status=RF_OK;float previous[3];
        memcpy(previous,saved.position,12);
        memcpy(campaign_support_velocity,saved_support,12);campaign_support_handle=saved_handle;
        scene_actor_body.state=saved;memcpy(rf_scene_actor_landing,landing,sizeof(landing));
        memset(rf_scene_actor_tick_stats,0,sizeof(rf_scene_actor_tick_stats));
        rf_scene_actor_run_traction=traction;rf_scene_actor_ground_material=material;out[14]=UINT32_MAX;
        for(step=0;step<600;++step) {
            actor_ground_record ground;rf_geometry_body_hit contact;rf_physics_body_state next=scene_actor_body.state;rf_group_attached_pose pose=rf_scene_actor_pose;
            int walkable,moved=memcmp(previous,next.position,12)!=0;uint32_t before=rf_scene_actor_landing[1],i;
            memcpy(previous,next.position,12);
            status=actor_ground_query(stream->collision,&ground,&contact);if(status)break;
            walkable=ground.matched && ground.hit.hit.fraction<1 && ground.hit.hit.normal[1]>=.5f;
            if(walkable) {
                rf_geometry_face face;
                if(campaign_spawn)rf_scene_actor_ground_material=contact.contact.material;
                else {
                status=rf_geometry_get_face(stream->geometry,ground.hit.face,&face);if(status)break;
                if(face.texture>=stream->geometry->textures) {status=RF_FORMAT;break;}
                rf_scene_actor_ground_material=stream->surface_indices[face.texture];
                }
                rf_scene_actor_run_traction=rf_scene_actor_surface_values[rf_scene_actor_ground_material].traction;
                if(before==3) {status=actor_support_commit(&next,&ground,&contact,1);
                    rf_scene_actor_landing[1]=1;++out[2];out[15]=step;
                } else if(moved)status=actor_support_commit(&next,&ground,&contact,0);
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
    memcpy(campaign_support_velocity,saved_support,12);campaign_support_handle=saved_handle;
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
/* Compute the gameplay eye pose before rendering or diagnostic view overrides.
 * This advances look state, so call exactly once per frame. */
static int actor_listener_pose(scene_stream *stream,uint32_t frame,
    const rf_motion_controller *controller,float position[3],float orientation[3][3])
{
    static const float default_orientation[3][3]={{-1,0,0},{0,1,0},{0,0,-1}};
    int status;memcpy(orientation,default_orientation,sizeof(default_orientation));
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
    return RF_OK;
}
static int actor_follow_view(void *context,uint32_t frame,const rf_motion_controller *controller,rf_model_projection *view)
{
    scene_stream *stream=context;float position[3],orientation[3][3];
    uint32_t *r=rf_scene_actor_follow_frames[frame%64];int status;
    profile_mark(1);
    status=actor_listener_pose(stream,frame,controller,position,orientation);if(status)return status;
    if(campaign_spawn && stream->particles.state) {
        uint32_t active;uint64_t elapsed=(uint64_t)frame*1000/60;
        int32_t now=(int32_t)(elapsed?((elapsed-1)%RF_TIMER_PERIOD)+1:0);
        status=rf_camera_effect_apply_random(&campaign_camera_effect,now,&stream->particles.state->random,(float *)orientation,&active);
        if(status)return status;
    }
    if(campaign_spawn) {
        campaign_audio_listener(position,orientation[0]);
        /* Original480ef7 follows listener refresh. Use the owned replay clock;
         * rendering the same frame again must not add a simulation tick. */
        if(campaign_ambient_frame!=frame) {
            uint64_t elapsed=(uint64_t)frame*1000/60;
            int32_t now=(int32_t)(elapsed?((elapsed-1)%RF_TIMER_PERIOD)+1:0);
            campaign_ambient_process();
            status=campaign_ambient_schedule(now,0);if(status)return status;
            campaign_ambient_frame=frame;
        }
    }
    if(rf_scene_particle_view_enabled && frame<400 && stream->particles.state && stream->particles.materials.count) {
        memcpy(position,stream->particles.state->slots[0].runtime.emitter.position,12);
        if(rf_scene_particle_view_back) {
            const float inspection_basis[3][3]={{1,0,0},{0,.7071067811865475f,.7071067811865475f},{0,-.7071067811865475f,.7071067811865475f}};
            position[1]+=4;position[2]-=4;memcpy(orientation,inspection_basis,sizeof(inspection_basis));
        }
    }
    if(stream->visibility.storage) {
        rf_collision_room_location room;rf_visibility_camera camera={0};
        /* Match the preview's fixed 4:3, x/z projection and 1000-unit far
         * distance. FOV=1 supplies an exact unit projection factor. */
        rf_visibility_camera_parameters parameters={{640,480,0,0,1,1,1000,1},{0},{0},.1f,1,1,1,0};
        uint32_t i,cached=0,hash=2166136261u,*record=rf_scene_visibility_frames[frame%64];
        memcpy(parameters.origin,position,12);memcpy(parameters.basis,orientation,36);
        status=rf_visibility_camera_setup(&parameters,&camera);if(status)return status;
        stream->particle_camera=camera;stream->particle_frame=frame;
        status=rf_geometry_collision_world_locate(stream->collision,position,&room);if(status)return status;
        status=rf_level_visibility_begin_render(&stream->visibility);if(status)return status;
        status=rf_level_visibility_view(&stream->visibility,&camera,640,480,room.room,UINT32_MAX,0,1);if(status)return status;
        for(i=0;i<stream->visibility.state.count;i++)hash=(hash^stream->visibility.state.rooms[i].visible)*16777619u;
        for(i=0;i<stream->visibility.graph.count;i++)cached+=stream->visibility.cache[i].valid!=0;
        record[0]=frame;record[1]=room.room;record[2]=stream->visibility.state.visible_count;record[3]=cached;record[4]=hash;
        memcpy(record+5,position,12);memcpy(record+8,orientation,36);
        rf_scene_visibility_summary[0]=frame+1;rf_scene_visibility_summary[1]=stream->visibility.resident_bytes;
        rf_scene_visibility_summary[2]=stream->visibility.state.count;rf_scene_visibility_summary[3]=stream->visibility.graph.count;
        rf_scene_visibility_summary[4]=record[2];rf_scene_visibility_summary[5]=room.room;
    }
    /* The actor portion is idle until animation emits this tick's model. Use
     * it for transactional world projection before the actor is appended. */
    {uint32_t world_capacity=(stream->capacity/2)/sizeof(rf_preview_vertex)*sizeof(rf_preview_vertex);
     rf_preview_failure[0]=0;
     if(rf_scene_actor_eye_enabled && stream->mesh->bytes>world_capacity)
        status=rf_scene_world_update_camera(actor_follow_world,campaign_movers.poses,campaign_movers.count,position,orientation,stream->mesh,stream->capacity-(campaign_spawn?1024*1024:0));
     else {
        status=rf_scene_world_update_camera_staged(actor_follow_world,campaign_movers.poses,campaign_movers.count,position,orientation,
        stream->mesh,world_capacity,stream->mesh->vertices+world_capacity/sizeof(rf_preview_vertex),
        stream->capacity-world_capacity);
        /* First-person rendering has no visible actor prefix to reserve. A
         * large world may use the whole allocation through the transactional
         * two-pass path instead of terminating at the staging-half boundary. */
        if(status==RF_RANGE && rf_scene_actor_eye_enabled && rf_preview_failure[0])
            status=rf_scene_world_update_camera(actor_follow_world,campaign_movers.poses,campaign_movers.count,position,orientation,stream->mesh,stream->capacity-(campaign_spawn?1024*1024:0));
     }
     if(status)return status;}
    profile_mark(2);
    stream->world=stream->mesh->count;
    memcpy(view->camera,position,12);memcpy(view->rotation,orientation,36);
    {uint32_t axis;for(axis=3;axis<6;++axis)view->rotation[axis]*=4.0f/3.0f;}
    stream->npc_view=*view;
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
uint32_t rf_scene_npc_playback[7]; /* ticks, actors, bones, state hash, pose hash, sticky marker bits, clip bytes */
/* Advance the existing startup selection once per simulation step. AI/state
 * reselection and weapon overlays remain external; do not tick from drawing. */
uint32_t rf_scene_npc_gate[4]; /* cumulative considered, advanced, skipped; last decision hash */
uint32_t rf_scene_npc_fall_test[6]; /* cases,normal,alternate,fallback,hash,errors */
static int campaign_npc_fall_fixture(uint32_t frame)
{
    uint32_t i,j;int status=RF_OK;
    if(frame)return RF_OK;memset(rf_scene_npc_fall_test,0,sizeof(rf_scene_npc_fall_test));
    if(!rf_scene_actor_pair_test_enabled)return RF_OK;rf_scene_npc_fall_test[4]=2166136261u;
    for(i=0;i<campaign_npc_body_count && !status;++i)if(campaign_npc_bodies[i].registration.view) {
        campaign_npc_body *owner=campaign_npc_bodies+i,saved=*owner,expected;uint32_t cls=campaign_seeds.items[i].class_index;
        uint32_t flags=campaign_seeds.classes[cls].physics.flags,enabled3=campaign_modes[3].enabled,enabled8=campaign_modes[8].enabled;
        for(j=0;j<4 && !status;++j) {
            uint32_t selected=j&1?8:3,actual,record[3];
            *owner=saved;owner->movement_orientation=owner->body.state.orientation;
            owner->body.state.flags&=~1u;expected=*owner;
            campaign_seeds.classes[cls].physics.flags=(flags&~0x400u)|(j&1?0x400u:0);
            campaign_modes[selected].enabled=j<2?0x101:0x100;
            actual=j<2?selected:0;expected.movement_slot=actual;expected.body.state.flags|=1;
            expected.movement_orientation=campaign_identity[0];
            status=rf_scene_npc_fall(owner->registration.handle);if(status)break;
            if(memcmp(owner,&expected,sizeof(expected))){status=RF_FORMAT;break;}
            ++rf_scene_npc_fall_test[0];++rf_scene_npc_fall_test[j>=2?3:j==0?1:2];
            record[0]=owner->movement_slot;record[1]=owner->body.state.flags;record[2]=owner->movement_orientation==campaign_identity[0];
            rf_scene_npc_fall_test[4]=npc_hash_bytes(rf_scene_npc_fall_test[4],record,sizeof(record));
            if(rf_scene_npc_fall(owner->registration.handle^0x10000)!=RF_NOT_FOUND || memcmp(owner,&expected,sizeof(expected)))status=RF_FORMAT;
        }
        *owner=saved;campaign_seeds.classes[cls].physics.flags=flags;
        campaign_modes[3].enabled=enabled3;campaign_modes[8].enabled=enabled8;
    }
    if(status)++rf_scene_npc_fall_test[5];return status;
}
uint32_t rf_scene_npc_ground_query_test[6]; /* cases,hits,misses,hash,errors,rejected empty bodies */
static int campaign_npc_ground_query_fixture(const rf_geometry_collision_world *world,uint32_t frame)
{
    uint32_t i,j;int status=RF_OK;
    if(frame)return RF_OK;memset(rf_scene_npc_ground_query_test,0,sizeof(rf_scene_npc_ground_query_test));
    if(!rf_scene_actor_pair_test_enabled)return RF_OK;rf_scene_npc_ground_query_test[3]=2166136261u;
    for(i=0;i<campaign_npc_body_count && !status;++i)if(campaign_npc_bodies[i].registration.view) {
        campaign_npc_body *owner=campaign_npc_bodies+i;uint32_t cls=campaign_seeds.items[i].class_index;
        float support_y=owner->support_velocity[1];uint32_t slot=owner->movement_slot;
        for(j=0;j<3 && !status;++j) {
            rf_physics_ground_probe probe={0},expected={0};rf_collision_actor_contact contact,expected_contact;
            rf_collision_body_sphere sphere;rf_collision_body_query query={0};rf_geometry_body_hit hit={0};uint32_t found=99,want=0,k,falling;
            float elapsed=j==0?0:j==1?1.0f/60:1.0f/30;
            owner->movement_slot=j==0?slot:j==1?3:1;owner->support_velocity[1]=j==2?2:support_y;
            memset(&contact,0xa5,sizeof(contact));expected_contact=contact;expected_contact.time=1;expected_contact.reserved_1ec=0;
            /* Authored non-colliding actors have no lowest sphere. Verify the
             * rejected query without inventing a collision shape for them. */
            if(!owner->body.spheres.count) {
                rf_collision_actor_contact saved=contact;
                if(rf_scene_npc_ground_query(world,owner->registration.handle,elapsed,&probe,&contact,&found)!=RF_RANGE ||
                    memcmp(&probe,&expected,sizeof(probe)) || memcmp(&contact,&saved,sizeof(contact)) || found!=99)status=RF_FORMAT;
                else ++rf_scene_npc_ground_query_test[5];
                continue;
            }
            falling=rf_entity_falling((int32_t)campaign_modes[owner->movement_slot].index,campaign_seeds.classes[cls].physics.use_kind,owner->support.material);
            status=rf_physics_ground_prepare(owner->body.spheres.items,owner->body.spheres.count,owner->body.state.next_position,
                owner->body.state.state_124,(int)falling,elapsed,campaign_npc_movement_configs[cls].base_speed,owner->support_velocity[1],&expected);if(status)break;
            status=rf_scene_npc_ground_query(world,owner->registration.handle,elapsed,&probe,&contact,&found);if(status)break;
            memcpy(sphere.center,expected.sphere.center,12);sphere.radius=expected.sphere.radius;
            memcpy(query.start,expected.start,12);memcpy(query.end,expected.end,12);for(k=0;k<3;++k)query.matrix[k][k]=1;
            query.radius=expected.bounds.radius;query.flags=expected.query_flags;query.spheres=&sphere;query.count=1;query.limit=1;
            status=campaign_body_query(world,&query,&hit,&want);if(status)break;if(want)memcpy(&expected_contact,&hit.contact,68);
            if(found!=want || memcmp(&probe,&expected,sizeof(probe)) || memcmp(&contact,&expected_contact,68)){status=RF_FORMAT;break;}
            ++rf_scene_npc_ground_query_test[0];++rf_scene_npc_ground_query_test[found?1:2];
            rf_scene_npc_ground_query_test[3]=npc_hash_bytes(rf_scene_npc_ground_query_test[3],&probe,sizeof(probe));
            rf_scene_npc_ground_query_test[3]=npc_hash_bytes(rf_scene_npc_ground_query_test[3],&contact,sizeof(contact));
            {rf_physics_ground_probe saved=probe;rf_collision_actor_contact kept=contact;uint32_t sentinel=99;
             if(rf_scene_npc_ground_query(world,owner->registration.handle^0x10000,elapsed,&probe,&contact,&sentinel)!=RF_NOT_FOUND ||
                memcmp(&probe,&saved,sizeof(probe)) || memcmp(&contact,&kept,sizeof(contact)) || sentinel!=99)status=RF_FORMAT;}
        }
        owner->support_velocity[1]=support_y;owner->movement_slot=slot;
    }
    if(status)++rf_scene_npc_ground_query_test[4];return status;
}
uint32_t rf_scene_npc_motion_request_test[5]; /* owners,speed cases,motion cases,hash,errors */
static int campaign_npc_motion_request_fixture(uint32_t frame)
{
    static const int32_t speeds[4]={0,1,2,-7},motions[4]={0,9,23,-1};uint32_t i,j,k;int status=RF_OK;
    if(frame)return RF_OK;memset(rf_scene_npc_motion_request_test,0,sizeof(rf_scene_npc_motion_request_test));
    if(!rf_scene_actor_pair_test_enabled)return RF_OK;rf_scene_npc_motion_request_test[3]=2166136261u;
    for(i=0;i<campaign_npc_body_count && !status;++i)if(campaign_npc_bodies[i].registration.view) {
        campaign_npc_body *owner;rf_entity_pose *pose;uint32_t cls;rf_movement_settings saved,expected;rf_motion_controller controller,wanted;float response;
        status=campaign_npc_motion_owner(campaign_npc_bodies[i].registration.handle,&owner,&cls,&pose);if(status)break;
        saved=owner->movement;response=owner->body.state.coefficients[1];controller=pose->controller;
        if(rf_scene_npc_set_speed(owner->registration.handle^0x10000,0,-1)!=RF_NOT_FOUND ||
            rf_scene_npc_request_motion(owner->registration.handle^0x10000,9,.25f)!=RF_NOT_FOUND ||
            rf_scene_npc_request_motion(owner->registration.handle,9,-1)!=RF_FORMAT ||
            memcmp(&controller,&pose->controller,sizeof(controller)) || memcmp(&saved,&owner->movement,sizeof(saved)))status=RF_FORMAT;

        for(j=0;j<4 && !status;++j)for(k=0;k<2 && !status;++k) {
            expected=owner->movement;
            status=rf_movement_set_mode(&expected,campaign_npc_movement_configs+cls,speeds[j],k?42:-1,owner->body.state.mass,0);if(status)break;
            status=rf_scene_npc_set_speed(owner->registration.handle,speeds[j],k?42:-1);if(status)break;
            if(memcmp(&expected,&owner->movement,sizeof(expected)) || memcmp(&expected.response,&owner->body.state.coefficients[1],4)){status=RF_FORMAT;break;}
            ++rf_scene_npc_motion_request_test[1];rf_scene_npc_motion_request_test[3]=npc_hash_bytes(rf_scene_npc_motion_request_test[3],&owner->movement,sizeof(owner->movement));
        }
        for(j=0;j<4 && !status;++j) {
            wanted=pose->controller;status=rf_motion_request_state(&wanted,campaign_motion_catalog.mappings[cls].states,motions[j],.25f);if(status)break;
            status=rf_scene_npc_request_motion(owner->registration.handle,motions[j],.25f);if(status)break;
            if(memcmp(&wanted,&pose->controller,sizeof(wanted))){status=RF_FORMAT;break;}
            ++rf_scene_npc_motion_request_test[2];rf_scene_npc_motion_request_test[3]=npc_hash_bytes(rf_scene_npc_motion_request_test[3],&pose->controller,sizeof(pose->controller));
        }
        owner->movement=saved;owner->body.state.coefficients[1]=response;pose->controller=controller;++rf_scene_npc_motion_request_test[0];
    }
    if(status)++rf_scene_npc_motion_request_test[4];return status;
}
uint32_t rf_scene_npc_body_sweep_test[5]; /* cases,hits,misses,hash,errors */
static int campaign_npc_body_sweep_fixture(const rf_geometry_collision_world *world,uint32_t frame)
{
    uint32_t i,axis,direction,capacity=0;rf_collision_body_sphere *scratch;int status=RF_OK;
    if(frame)return RF_OK;memset(rf_scene_npc_body_sweep_test,0,sizeof(rf_scene_npc_body_sweep_test));
    if(!rf_scene_actor_pair_test_enabled)return RF_OK;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].body.spheres.count>capacity)capacity=campaign_npc_bodies[i].body.spheres.count;
    if(!capacity || capacity>4096)return RF_RANGE;scratch=malloc(capacity*sizeof(*scratch));if(!scratch)return RF_IO;
    rf_scene_npc_body_sweep_test[3]=2166136261u;
    for(i=0;i<campaign_npc_body_count && !status;++i)if(campaign_npc_bodies[i].registration.view) {
        campaign_npc_body *owner=campaign_npc_bodies+i;
        for(axis=0;axis<3 && !status;++axis)for(direction=0;direction<3 && !status;++direction) {
            rf_physics_body_state proposal=owner->body.state;rf_collision_body_query query={0};
            rf_geometry_body_hit hit={0},expected={0};uint32_t matched=99,want=0,k;
            memcpy(proposal.next_position,proposal.position,12);
            proposal.next_position[axis]+=direction==0?-16.0f:direction==1?16.0f:0;
            status=rf_physics_body_prepare_sweep(&proposal);if(status)break;
            status=rf_scene_npc_body_sweep(world,owner->registration.handle,&proposal,0x460,scratch,capacity,&hit,&matched);if(status)break;
            /* Independent query assembly checks owner and transform selection. */
            for(k=0;k<owner->body.spheres.count;++k){memcpy(scratch[k].center,owner->body.spheres.items[k].center,12);scratch[k].radius=owner->body.spheres.items[k].radius;}
            memcpy(query.start,proposal.position,12);memcpy(query.end,proposal.next_position,12);
            memcpy(query.matrix,proposal.orientation,36);query.radius=proposal.bounds.radius;
            query.flags=0x460;query.spheres=scratch;query.count=owner->body.spheres.count;query.limit=1;
            if(direction!=2)status=campaign_body_query(world,&query,&expected,&want);
            if(!status && (matched!=want || (matched && memcmp(&hit,&expected,sizeof(hit)))))status=RF_FORMAT;
            if(status)break;
            ++rf_scene_npc_body_sweep_test[0];++rf_scene_npc_body_sweep_test[matched?1:2];
            rf_scene_npc_body_sweep_test[3]=npc_hash_bytes(rf_scene_npc_body_sweep_test[3],&matched,4);
            if(matched)rf_scene_npc_body_sweep_test[3]=npc_hash_bytes(rf_scene_npc_body_sweep_test[3],&hit,sizeof(hit));
        }
    }
    free(scratch);if(status)++rf_scene_npc_body_sweep_test[4];return status;
}
static int campaign_model_query_fixture(uint32_t frame)
{
    uint32_t i,axis,r,tested=0;int status;
    if(!rf_scene_actor_pair_test_enabled || frame%30)return RF_OK;
    for(i=0;i<campaign_model_owner_count;++i) {
        rf_entity_pose *pose;float sphere[4];status=campaign_actor_pose(i,&pose);if(status)return status;if(!pose)continue;
        status=rf_model_file_bound_sphere(&campaign_render_models.items[pose->skeleton].file,sphere);if(status)return status;
        for(axis=0;axis<3;++axis)for(r=0;r<2;++r) {
            rf_collision_model_part_query query={0};rf_collision_model_response_hit hit={0};uint32_t accepted;
            query.input.flags=2+r;query.input.radius=r?.25f:0;
            memcpy(query.input.start,sphere,12);query.input.start[axis]-=sphere[3]+1;
            query.input.displacement[axis]=2*(sphere[3]+1);
            if(!tested) {
                uint32_t n=pose->bone_count*48,k,playback_hash=npc_hash_bytes(2166136261u,&pose->playback,sizeof(pose->playback));
                unsigned char *saved=malloc(n+pose->bone_count*2);if(!saved)return RF_IO;
                memcpy(saved,pose->matrices,n);memcpy(saved+n,pose->generations,pose->bone_count*2);
                memset(pose->matrices,0xa5,n);for(k=0;k<pose->bone_count;++k)pose->generations[k]=(uint16_t)(pose->playback.generation-1u);
                status=rf_scene_model_collision_query(i,&query,&hit,1,&accepted);
                if(!status && (memcmp(saved,pose->matrices,n) || memcmp(saved+n,pose->generations,pose->bone_count*2) ||
                    playback_hash!=npc_hash_bytes(2166136261u,&pose->playback,sizeof(pose->playback))))status=RF_FORMAT;
                if(status){memcpy(pose->matrices,saved,n);memcpy(pose->generations,saved+n,pose->bone_count*2);++rf_scene_npc_pose_demand[4];}
                free(saved);++rf_scene_npc_pose_demand[3];tested=1;
            } else status=rf_scene_model_collision_query(i,&query,&hit,1,&accepted);
            if(status)return status;
        }
    }
    return RF_OK;
}
static int campaign_npc_playback_tick(scene_stream *stream,float elapsed)
{
    uint32_t i,h=2166136261u,p=2166136261u,actors=0,bones=0,markers=0,g=2166136261u;int status;
    if(campaign_npc_body_count!=campaign_poses.count)return RF_RANGE;
    /* Original snapshot list completes before any model/controller update. */
    for(i=0;i<campaign_npc_body_count;++i) {
        rf_entity_pose *pose;status=campaign_actor_pose(i,&pose);if(status)return status;if(!pose)continue;
        campaign_npc_body *owner=campaign_npc_bodies+i;
        rf_entity_position_snapshot(&owner->object_flags,owner->previous,owner->published);
        owner->view.flags_7c=owner->object_flags;
        memcpy(campaign_model_owners[i].position,owner->published,12);
    }
    for(i=0;i<campaign_poses.count;++i) {
        rf_entity_pose *pose;const rf_entity_motion_mapping *map;
        rf_entity_playback_model *model;float displacement[3]={0};uint32_t class_index;
        status=campaign_actor_pose(i,&pose);if(status)return status;if(!pose)continue;
        class_index=campaign_seeds.items[i].class_index;
        if(class_index>=campaign_motion_catalog.class_count || pose->skeleton>=campaign_playback_resources.model_count)return RF_RANGE;
        map=campaign_motion_catalog.mappings+class_index;model=campaign_playback_resources.models+pose->skeleton;
        if(map->skeleton!=pose->skeleton || map->weapon!=-1)return RF_FORMAT;
        status=rf_motion_apply_controller(&pose->controller,map->states,elapsed,&pose->playback,model->resources,model->count);if(status)return status;
        status=campaign_npc_pose_residency(i);if(status)return status;
        {
            rf_entity_animation_gate gate={0};uint32_t room;int advance;
            const rf_entity_seed_class *cls=campaign_seeds.classes+class_index;
            const rf_visibility_view *camera=&stream->particle_camera.view;
            if(!stream->visibility.storage)return RF_RANGE;
            room=campaign_model_owners[i].room;gate.model_present=1;gate.model_kind=cls->model_kind;
            gate.descriptor_present=room<stream->visibility.state.count;
            if(gate.descriptor_present)gate.descriptor_flag=stream->visibility.state.rooms[room].visible;
            /* Fixed startup actors:402d68 clears action520;422360 clears810/814.
             * Action now comes from the registered owner;814 and the extra predicate
             * still use startup values until AI/death ownership is connected. */
            gate.action_520=campaign_npc_bodies[i].view.action_520;gate.flags=0;gate.predicate=0;
            gate.lod_distance_count=(int32_t)cls->lod.count;
            status=rf_model_lod_metric(0x66,campaign_model_owners[i].position,
                camera->origin,camera->scale[2],camera->scale[0],&gate.distance);if(status)return status;
            advance=rf_entity_animation_should_advance(&gate);
            ++rf_scene_npc_gate[0];++rf_scene_npc_gate[advance?1:2];
            g=(g^(uint32_t)advance)*16777619u;
            if(advance) {
                status=rf_entity_pose_advance(pose,&campaign_skeletons,&campaign_motion_catalog,&campaign_playback_resources,elapsed,displacement);if(status)return status;
                status=campaign_model_collision_refresh(i);if(status)return status;
            }
        }
        status=campaign_npc_eye_update(i);if(status)return status;
        ++actors;bones+=pose->bone_count;
        h=npc_hash_bytes(h,&pose->playback,sizeof(pose->playback));
        p=npc_hash_bytes(p,pose->matrices,pose->bone_count*48);p=npc_hash_bytes(p,pose->generations,pose->bone_count*2);
        markers|=pose->playback.event_mask;
    }
    rf_scene_npc_gate[3]=g;
    rf_scene_npc_eyes[3]=2166136261u;rf_scene_npc_eyes[0]=0;
    for(i=0;i<campaign_npc_body_count;++i)if(campaign_npc_bodies[i].registration.view) {
        ++rf_scene_npc_eyes[0];rf_scene_npc_eyes[3]=npc_hash_bytes(rf_scene_npc_eyes[3],campaign_npc_bodies[i].eye_position,12);
    }
    status=campaign_npc_fall_fixture(rf_scene_npc_playback[0]);if(status)return status;
    status=campaign_npc_ground_query_fixture(stream->collision,rf_scene_npc_playback[0]);if(status)return status;
    status=campaign_npc_motion_request_fixture(rf_scene_npc_playback[0]);if(status)return status;
    status=campaign_npc_body_sweep_fixture(stream->collision,rf_scene_npc_playback[0]);if(status)return status;
    status=campaign_model_query_fixture(rf_scene_npc_playback[0]);if(status)return status;
    ++rf_scene_npc_playback[0];rf_scene_npc_playback[1]=actors;rf_scene_npc_playback[2]=bones;
    rf_scene_npc_playback[3]=h;rf_scene_npc_playback[4]=p;rf_scene_npc_playback[5]=markers;rf_scene_npc_playback[6]=campaign_npc_motion_bytes;return RF_OK;
}
/* Diagnostic submission. Highest-detail LOD per SUBM until the
 * original distance/actor draw gates are connected. Uses portal-room visibility.
 * Room membership is cached for these stationary actors. No AI tick. */
uint32_t rf_scene_npc_draw_detail[6];
uint32_t rf_scene_npc_draw[5]; /* frame, visible actors, vertices, vertex hash, scratch bytes */
static int scene_npc_draw(scene_stream *stream,uint32_t frame)
{
    rf_model_render_buffers buffers;rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};
    uint32_t actor,lod,batch,k,start_all=stream->mesh->count;int status;
    if(!stream->npc_memory)return RF_OK;
    memset(rf_scene_npc_draw,0,sizeof(rf_scene_npc_draw));rf_scene_npc_draw[0]=frame+1;
    rf_scene_npc_draw[4]=4096*96+24576*sizeof(uint16_t)+sizeof(*stream->npc_pool);
    buffers.cache=stream->npc_memory;buffers.clip=(float(*)[3])((uint8_t*)stream->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t*)stream->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t*)stream->npc_memory+4096*56);buffers.capacity=4096;
    planes.near_depth=.1f;planes.far_depth=1000;
    projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=40;lights.ambient[1]=50;lights.ambient[2]=60;
    for(actor=0;actor<campaign_poses.count;++actor) {
        rf_entity_pose *pose;const rf_entity_render_model *model;
        const campaign_model_owner *owner=campaign_model_owners+actor;rf_model_projection view;float prepared[50][12];uint16_t generations[50];
        uint32_t appearance,first,last,start_actor=stream->mesh->count;
        rf_scene_npc_draw_detail[0]=actor;
        status=campaign_model_pose(actor,&pose);if(status)return status;if(!pose)continue;
        if(owner->room<stream->visibility.state.count &&
            !stream->visibility.state.rooms[owner->room].visible)continue;
        appearance=owner->appearance;if(appearance>=campaign_npc_materials.count)return RF_FORMAT;
        first=campaign_npc_materials.offsets[appearance];last=campaign_npc_materials.offsets[appearance+1];
        model=campaign_render_models.items+pose->skeleton;
        memset(prepared,0,sizeof(prepared));memset(generations,0,sizeof(generations));
        status=rf_model_prepare_skinning(model->stored,pose->matrices,pose->bone_count,(uint16_t)pose->playback.generation,prepared,generations,50);if(status)return status;
        status=rf_model_local_view(&stream->npc_view,owner->position,owner->basis,&view);if(status)return status;
        for(lod=0;lod<model->file.lod_count;++lod) {
            const rf_model_geometry *geometry=model->lods+lod;uint32_t previous;
            for(previous=0;previous<lod;++previous)if(model->file.lods[previous].section_index==model->file.lods[lod].section_index)break;
            if(previous<lod)continue;
            for(batch=0;batch<geometry->batch_count;++batch) {
                uint32_t start=stream->mesh->count,emitted,slot,material=geometry->batches[batch].material;
                if(material==UINT32_MAX)continue;if(material>=last-first)return RF_FORMAT;
                memcpy(&slot,campaign_npc_materials.materials.items[first+material].record.bytes+0x10,4);
                if(slot>=stream->npc_textures)return RF_FORMAT;
                rf_scene_npc_draw_detail[1]=lod;rf_scene_npc_draw_detail[2]=batch;rf_scene_npc_draw_detail[3]=1;
                rf_scene_npc_draw_detail[4]=stream->mesh->bytes;rf_scene_npc_draw_detail[5]=stream->capacity;
                memset(stream->npc_memory,0xa5,4096*96);
                status=rf_model_geometry_render_batch(geometry,batch,prepared,pose->bone_count,&view,&lights,&attributes,&buffers);if(status)return status;
                rf_scene_npc_draw_detail[3]=2;
                status=rf_preview_model_emit(geometry,batch,&buffers,stream->npc_indices,stream->npc_pool,&view,&planes,&projection,
                    &attributes,stream->mesh,stream->capacity,&emitted);if(status)return status;
                for(k=start;k<stream->mesh->count;++k)stream->mesh->vertices[k].material=stream->npc_base+slot;
            }
        }
        if(stream->mesh->count>start_actor)++rf_scene_npc_draw[1];
    }
    rf_scene_npc_draw[2]=stream->mesh->count-start_all;
    rf_scene_npc_draw[3]=npc_hash_bytes(2166136261u,stream->mesh->vertices+start_all,rf_scene_npc_draw[2]*sizeof(rf_preview_vertex));
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
        status=campaign_death_clearance_fixture(stream->collision,frame);if(status)return status;
        status=campaign_death_animation_fixture(stream->collision,frame,stream->particles.state?&stream->particles.state->random:NULL);if(status)return status;
        status=actor_ground_check(stream->collision,frame);if(status)return status;
        {
            const actor_ground_record *ground=rf_scene_actor_ground_records+(frame%64);
            if(ground->matched && ground->hit.hit.fraction<1 && ground->hit.hit.normal[1]>=.5f) {
                if(campaign_spawn)rf_scene_actor_ground_material=rf_scene_actor_ground_contacts[frame%64].contact.material;
                else {
                    rf_geometry_face face;
                    status=rf_geometry_get_face(stream->geometry,ground->hit.face,&face);if(status)return status;
                    if(face.texture>=stream->geometry->textures)return RF_FORMAT;
                    rf_scene_actor_ground_material=stream->surface_indices[face.texture];
                }
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
        status=scene_npc_draw(stream,frame);if(status)return status;
        particle_draw_stream=stream;
        status=stream->sink(stream->context,frame,stream->mesh,stream->materials,stream->world);
        particle_draw_stream=NULL;if(status)return status;
        profile_mark(6);
        if(stream->collision && frame+1<rf_scene_actor_frame_count) {
            uint64_t particle_elapsed=((uint64_t)frame+1)*1000/60;
            int32_t particle_now=(int32_t)(particle_elapsed?((particle_elapsed-1)%RF_TIMER_PERIOD)+1:0);
            rf_physics_body_state next=scene_actor_body.state;
            const actor_ground_record *ground=rf_scene_actor_ground_records+(frame%64);
            int walkable=ground->matched && ground->hit.hit.fraction<1 && ground->hit.hit.normal[1]>=.5f;
            actor_ground_record post_ground;rf_geometry_body_hit post_contact;
            const rf_geometry_body_hit *contact=rf_scene_actor_ground_contacts+(frame%64);uint32_t route=RF_PLAYER_SUPPORT_QUERY;
            int moved=0;uint32_t axis;
            if(stream->particles.state) {
                status=rf_level_particles_emit_pass(&stream->particles,&stream->visibility.state,1,scene_step_seconds,
                    particle_now,NULL,NULL,&stream->particle_first);if(status)return status;
            }
            if(campaign_spawn) {
                /* 433520 -> 433260: input, physics, then 487e00 support.
                 * The owned local-player fixture has object bit 8 and no parent. */
                rf_group_registered_mover *support;
                status=campaign_force_tick(&next,&stream->particles,particle_now);
                rf_scene_force_ticks[11]=(uint32_t)status;if(status)return status;
                status=campaign_controller_tick(particle_now,&stream->particles,next.position);
                rf_scene_live_motion[7]=(uint32_t)status;if(status)return status;
                status=campaign_npc_refresh_support_fixture(frame);if(status)return status;
                status=campaign_npc_refresh_support_tick();if(status)return status;
                support=rf_object_registry_lookup(&campaign_registry,campaign_support_handle);
                rf_physics_support_refresh(rf_scene_actor_landing[1],
                    support && support->object_kind==9?support->pose->velocity:NULL,
                    campaign_support_velocity,&next.flags,&rf_scene_actor_pose.flags);
                status=actor_tick(stream->collision,&next,rf_scene_actor_input_frames[frame%64],ground->hit.hit.normal);if(status)return status;
                status=campaign_controller_commit();
                rf_scene_live_motion[7]=(uint32_t)status;if(status)return status;
                moved=memcmp(next.position,scene_actor_body.state.position,12)!=0;
                {rf_player_support_input input={rf_scene_actor_landing[1],rf_scene_actor_stance_flags,
                    0,-1,-1,(uint32_t)moved,next.flags,8};route=rf_player_support_route(&input);}
                if(route==RF_PLAYER_SUPPORT_QUERY) {
                    status=actor_ground_query_state(stream->collision,&next,&post_ground,&post_contact);if(status)return status;
                    ground=&post_ground;contact=&post_contact;walkable=ground->matched && ground->hit.hit.fraction<1 && ground->hit.hit.normal[1]>=.5f;
                }
            } else if(frame)for(axis=0;axis<3;++axis) {
                float previous;memcpy(&previous,rf_scene_actor_render_frames[(frame-1)%64]+2+axis,4);
                if(previous!=next.position[axis])moved=1;
            }
            if(route==RF_PLAYER_SUPPORT_FALL) {
                next.flags|=1;rf_scene_actor_landing[1]=3;
            } else if(route==RF_PLAYER_SUPPORT_QUERY) {
                if(rf_scene_actor_landing[1]==3 && walkable) {
                    status=actor_support_commit(&next,ground,contact,1);if(status)return status;
                    rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=frame;
                    ++rf_scene_actor_landing[3];rf_scene_actor_landing[5]=1;
                } else if(rf_scene_actor_landing[1]==1 && (campaign_spawn || moved)) {
                    if(walkable) {
                        status=actor_support_commit(&next,ground,contact,0);if(status)return status;
                        ++rf_scene_actor_landing[6];
                    } else {
                        next.flags|=1;rf_scene_actor_landing[1]=3;++rf_scene_actor_landing[7];
                    }
                }
            }
            if(campaign_spawn) {
                /* 4aa6d0 consumes this after support; its optional first-person
                 * motion request remains part of the unfinished weapon lifecycle. */
                rf_scene_actor_stance_flags&=~2u;
            } else {
                status=actor_tick(stream->collision,&next,rf_scene_actor_input_frames[frame%64],ground->hit.hit.normal);if(status)return status;
            }
            status=rf_group_pose_set_position(&rf_scene_actor_pose,next.position);if(status)return status;
            memcpy(next.position,rf_scene_actor_pose.position,12);memcpy(next.next_position,rf_scene_actor_pose.pending,12);
            memcpy(next.bounds.minimum,rf_scene_actor_pose.minimum,12);memcpy(next.bounds.maximum,rf_scene_actor_pose.maximum,12);
            scene_actor_body.state=next;
            if(campaign_spawn) {
                rf_startup_events_report tick_report;uint32_t pending,j,words[9];
                uint64_t elapsed=((uint64_t)frame+1)*1000/60;
                int32_t now=(int32_t)(elapsed?((elapsed-1)%RF_TIMER_PERIOD)+1:0);
                /* Owned 60-Hz replay clock. Original 4333ea calls event tick
                 * after physics; full wall-clock/whole-frame parity is open. */
                status=campaign_trigger_contacts(&rf_scene_actor_pose,now,frame,&stream->particles,player_poll?player_input.use:0);if(status)return status;
                status=rf_runtime_events_tick(&campaign_events,&campaign_triggers,&scene_gravity,now,&stream->particles, &campaign_forces,&tick_report,&pending);
                if(status)return status;
                campaign_force_snapshot();campaign_switch_snapshot();
                ++rf_scene_event_ticks[0];rf_scene_event_ticks[1]=(uint32_t)now;rf_scene_event_ticks[2]=pending;
                memcpy(words,&tick_report,sizeof(words));
                for(j=0;j<9;++j)rf_scene_event_ticks[3+j]+=words[j];
            }
            if(stream->particles.state) {
                rf_level_particle_tick_result step,last;uint32_t particle_index,byte_index,hash=2166136261u;
                uint32_t *record=rf_scene_particles_frames[(frame+1)%64],*summary=rf_scene_particles_summary;
                status=rf_level_particles_simulate(&stream->particles,&stream->visibility.state,1,scene_step_seconds,NULL,NULL,&step);
                if(status)return status;
                status=rf_level_particles_emit_pass(&stream->particles,&stream->visibility.state,1,scene_step_seconds,
                    particle_now,NULL,NULL,&last);if(status)return status;
                for(particle_index=0;particle_index<RF_PARTICLE_CAPACITY;particle_index++)if(stream->particles.state->records[particle_index].flags&1u) {
                    const unsigned char *payload=(const unsigned char*)(stream->particles.state->records+particle_index);
                    for(byte_index=0;byte_index<sizeof(rf_particle);byte_index++)hash=(hash^payload[byte_index])*16777619u;
                }
                record[0]=frame+1;record[1]=stream->particle_first.created;record[2]=step.stepped;record[3]=step.expired;
                record[4]=last.created;record[5]=stream->particle_first.emitter_updates;record[6]=last.emitter_updates;
                record[7]=stream->particles.state->particles.live[0];record[8]=stream->particles.state->particles.live[1];
                record[9]=stream->particles.state->random.value;record[10]=hash;record[11]=0;
                ++summary[0];summary[3]+=record[1]+record[4];summary[4]+=record[3];
                summary[5]=record[7];summary[6]=record[8];summary[7]=record[9];
            }
            if(campaign_spawn){status=campaign_npc_playback_tick(stream,scene_step_seconds);if(status)return status;}
        }
        if(campaign_spawn && stream->collision) {int status=campaign_collision_views_check(frame);if(status)return status;}
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
        if(!status && campaign_spawn) {
            campaign_force_class_flags=physics_config.authored.flags;campaign_force_class_kind=physics_config.authored.use_kind;
            campaign_force_air_limit=0;memset(rf_scene_force_ticks,0,sizeof(rf_scene_force_ticks));
            status=rf_camera_effect_reset(&campaign_camera_effect,0);
            if(!status)status=rf_screen_flash_reset(&campaign_player_flash);
            if(!status)status=campaign_player_damage_open(&tables,binding.entity.class_name,&physics_config.authored);
        }
        if(!status && collision)status=rf_movement_descriptor_load(&tables,physics_config.authored.movement_index,65536,rf_scene_actor_movement);
        if(!status && collision)status=rf_movement_descriptor_load(&tables,3,65536,rf_scene_actor_movement+1);
        if(!status && collision)status=rf_entity_movement_load(&tables,binding.entity.class_name,512*1024,&rf_scene_actor_movement_values);
        if(!status && collision)status=scene_surface_open(&tables,&stream);
        if(!status && collision && campaign_spawn) {
            uint32_t mode;float height;
            status=rf_game_jump_height_load(&tables,65536,&height);
            if(!status)campaign_jump_strength=(float)sqrt(2.0*(double)scene_gravity.acceleration*(double)height);
            for(mode=0;mode<16 && !status;++mode)status=rf_movement_descriptor_load(&tables,mode,65536,campaign_modes+mode);
        }
        if(!status && collision && campaign_spawn)status=rf_entity_seeds_open(level,&tables,1024*1024,&campaign_seeds);
        if(!status && collision && campaign_spawn)status=rf_entity_skeletons_open(&campaign_seeds,&archive,256*1024,&campaign_skeletons);
        if(!status && collision && campaign_spawn)status=rf_entity_poses_open(&campaign_seeds,&campaign_skeletons,1024*1024,&campaign_poses);
        if(!status && collision && campaign_spawn)status=rf_entity_render_models_open(&campaign_skeletons,&archive,1024*1024,&campaign_render_models);
        if(!status && collision && campaign_spawn)status=campaign_model_geometry_open();
        if(!status && collision && campaign_spawn)status=rf_entity_appearances_open(&campaign_seeds,&campaign_skeletons,&tables,1024*1024,&campaign_appearances);
        if(!status && collision && campaign_spawn)status=rf_entity_materials_open(&campaign_npc_materials,&campaign_appearances,&campaign_render_models,maps,map_count,4*1024*1024);
        if(!status && collision && campaign_spawn)campaign_npc_materials_digest();
        rf_vpp_close(&tables);if(status)goto done;
        if(campaign_spawn && collision) {
            rf_object_registry_init(&campaign_registry);
            status=rf_runtime_events_open(level,&campaign_registry,1024*1024,&campaign_events);
            if(status)goto done;
            status=rf_level_owned_ambient_open(level,65536,&campaign_ambient);
            if(status==RF_NOT_FOUND)status=RF_OK;if(status)goto done;
            rf_scene_ambient_records[0]=campaign_ambient.count;
            rf_scene_ambient_records[1]=campaign_ambient.allocated_bytes;
            rf_scene_ambient_records[2]=2166136261u;
            for(i=0;i<campaign_ambient.count*sizeof(*campaign_ambient.items);++i)
                rf_scene_ambient_records[2]=(rf_scene_ambient_records[2]^((unsigned char *)campaign_ambient.items)[i])*16777619u;
            rf_scene_campaign_events[0]=campaign_events.count;
            rf_scene_campaign_events[1]=campaign_events.allocated_bytes;
            rf_scene_campaign_events[2]=sizeof(campaign_registry);
            status=rf_runtime_triggers_open(level,&campaign_registry,1024*1024,0,&campaign_triggers);
            if(status)goto done;
            rf_scene_campaign_triggers[0]=campaign_triggers.count;
            rf_scene_campaign_triggers[1]=campaign_triggers.allocated_bytes;
            status=rf_level_owned_groups_open(level,1024*1024,&campaign_groups);
            if(status==RF_NOT_FOUND)status=RF_OK;if(status)goto done;
            status=rf_physics_forces_open(level,65536,&campaign_forces);if(status)goto done;
            rf_scene_campaign_forces[0]=campaign_forces.count;
            rf_scene_campaign_forces[1]=campaign_forces.allocated_bytes;
            rf_scene_campaign_forces[2]=2166136261u;
            for(i=0;i<campaign_forces.count*sizeof(*campaign_forces.items);++i)
                rf_scene_campaign_forces[2]=(rf_scene_campaign_forces[2]^((unsigned char *)campaign_forces.items)[i])*16777619u;
            if(status==RF_NOT_FOUND)status=RF_OK;if(status)goto done;
            status=rf_group_runtime_open(&campaign_groups,0,256*1024,&campaign_group_runtime);
            if(status)goto done;
            status=rf_group_registration_open(&campaign_group_runtime,&campaign_registry,65536,&campaign_group_registration);
            if(status)goto done;
            rf_scene_campaign_groups[0]=campaign_group_registration.count;
            rf_scene_campaign_groups[1]=campaign_group_registration.key_count;
            rf_scene_campaign_groups[2]=campaign_groups.allocated_bytes;
            rf_scene_campaign_groups[3]=campaign_group_runtime.allocated_bytes;
            rf_scene_campaign_groups[4]=campaign_group_registration.allocated_bytes;
            if(!actor_follow_world) {status=RF_RANGE;goto done;}
            status=campaign_open_movers(&actor_follow_world->movers);if(status)goto done;
            campaign_sweep_scratch=calloc(campaign_movers.count?campaign_movers.count:1,sizeof(*campaign_sweep_scratch));
            campaign_surface_sources=calloc((size_t)campaign_movers.count+1,sizeof(*campaign_surface_sources));
            if(!campaign_sweep_scratch || !campaign_surface_sources){status=RF_RANGE;goto done;}
            campaign_surface_sources[0]=geometry;
            for(i=0;i<campaign_movers.count;i++)campaign_surface_sources[i+1]=&actor_follow_world->movers.items[i].geometry;
            campaign_controller_requests=calloc(campaign_group_runtime.count?campaign_group_runtime.count:1,sizeof(*campaign_controller_requests));
            if(!campaign_controller_requests){status=RF_RANGE;goto done;}
            status=campaign_audio_open(tables_path,level->entry.name,binding.entity.class_name);if(status)goto done;
            memset(rf_scene_live_activation,0,sizeof(rf_scene_live_activation));campaign_actor_controller=UINT32_MAX;
            memset(rf_scene_trigger_contacts,0,sizeof(rf_scene_trigger_contacts));
            memset(&campaign_entities,0,sizeof(campaign_entities));memset(&campaign_player_view,0,sizeof(campaign_player_view));
            campaign_player_view.handle=-1;campaign_player_view.type=0;campaign_player_view.class_type=-1;
            campaign_player_view.flags_7c=8; /* Confirmed local-player object bit; full factory flags remain separate. */
            campaign_player_view.linked_handle=-1;campaign_player_view.weapons[0]=campaign_player_view.weapons[1]=-1;
            campaign_player_view.action_520=-1;campaign_player_view.base_speed=rf_scene_actor_movement_values.speed;
            status=rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object);if(status)goto done;
            campaign_player_damage.state.effects.handle=campaign_player_object.handle;
            rf_scene_campaign_player[0]=campaign_player_object.handle;rf_scene_campaign_player[1]=campaign_player_object.object_kind;
            rf_scene_campaign_player[2]=campaign_player_view.flags_7c;
            rf_scene_campaign_player[3]=sizeof(campaign_entities)+sizeof(campaign_player_view)+sizeof(campaign_player_object)+sizeof(campaign_player_flash)+sizeof(campaign_player_damage)+sizeof(campaign_player_pain_sound)+sizeof(campaign_player_geometry);
            memset(rf_scene_actor_body_sweeps,0,sizeof(rf_scene_actor_body_sweeps));
            memset(rf_scene_actor_ground_queries,0,sizeof(rf_scene_actor_ground_queries));
            memset(rf_scene_actor_ground_contacts,0,sizeof(rf_scene_actor_ground_contacts));
            memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));campaign_support_handle=0;
            memset(&rf_scene_actor_body_contact,0,sizeof(rf_scene_actor_body_contact));
            rf_scene_actor_body_sweeps[4]=(campaign_movers.count?campaign_movers.count:1)*sizeof(*campaign_sweep_scratch)+
                (campaign_movers.count+1)*sizeof(*campaign_surface_sources)+sizeof(*campaign_surface_palette);
            status=campaign_bind_movers();if(status)goto done;
            status=rf_level_owned_regions_open(level,65536,&campaign_regions);
            if(status==RF_NOT_FOUND)status=RF_OK;if(status)goto done;
            memset(&campaign_climb,0,sizeof(campaign_climb));memset(rf_scene_player_climb,0,sizeof(rf_scene_player_climb));
            memset(rf_scene_player_climb_frames,0,sizeof(rf_scene_player_climb_frames));
            rf_scene_player_climb[3]=UINT32_MAX;rf_scene_player_climb[5]=campaign_regions.allocated_bytes;
        }
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
        /* Port-zeroed inactive payload;49f010 itself leaves these fields untouched. */
        memset(&campaign_player_contact,0,sizeof(campaign_player_contact));campaign_player_material=physics_config.material.index;
        placement.campaign_player=campaign_spawn;
        placement.published_model=campaign_spawn?&campaign_player_model:NULL;
        campaign_crouched=0;placement.player_stance=campaign_spawn?actor_player_stance:NULL;
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
    if(state_mode || campaign_spawn) {
        status=rf_vpp_open(&motions,motions_path);if(status)goto done;motions_opened=1;
        if(campaign_spawn) {
            rf_vpp tables;status=rf_vpp_open(&tables,tables_path);if(status)goto done;
            status=rf_entity_base_motions_open(&campaign_seeds,&tables,&motions,1024*1024,&campaign_base_motions);
            rf_vpp_close(&tables);if(status)goto done;
            status=rf_entity_motion_catalog_open(&campaign_skeletons,&campaign_base_motions,512*1024,&campaign_motion_catalog);
            if(status)goto done;
            status=rf_entity_playback_resources_open(&campaign_motion_catalog,256*1024,&campaign_playback_resources);if(status)goto done;
            /* Diagnostic startup delta matches the existing first-class pose query.
             * Subsequent live selector scheduling/geometry submission is separate. */
            status=rf_entity_poses_start_initial(&campaign_seeds,&campaign_skeletons,&campaign_motion_catalog,
                &campaign_playback_resources,&campaign_poses,campaign_modes,1.0f/30.0f);if(status)goto done;
            status=campaign_models_open();if(status)goto done;
            /* This diagnostic begins the simulation clock at zero. */
            status=campaign_npc_bodies_open(tables_path,collision,0);if(status)goto done;
            status=campaign_resolve_trigger_links();if(status)goto done;
            status=campaign_npc_motion_residency();if(status)goto done;
            status=campaign_npc_damage_fixture();if(status)goto done;
            status=campaign_player_pain_fixture();if(status)goto done;
            memset(rf_scene_npc_playback,0,sizeof(rf_scene_npc_playback));
            memset(rf_scene_npc_gate,0,sizeof(rf_scene_npc_gate));
            status=campaign_npc_geometry_digest();if(status)goto done;
            {
                uint32_t actor,k;rf_scene_npc_startup[0]=rf_scene_npc_startup[1]=0;
                rf_scene_npc_startup[2]=rf_scene_npc_startup[3]=2166136261u;
                for(actor=0;actor<campaign_poses.count;++actor)if(campaign_poses.items[actor].skeleton!=UINT32_MAX) {
                    const rf_entity_pose *p=campaign_poses.items+actor;const unsigned char *bytes=(const unsigned char*)&p->playback;
                    ++rf_scene_npc_startup[0];rf_scene_npc_startup[1]+=p->bone_count;
                    for(k=0;k<sizeof(p->playback);++k)rf_scene_npc_startup[2]=(rf_scene_npc_startup[2]^bytes[k])*16777619u;
                    bytes=(const unsigned char*)p->matrices;
                    for(k=0;k<p->bone_count*48;++k)rf_scene_npc_startup[3]=(rf_scene_npc_startup[3]^bytes[k])*16777619u;
                    bytes=(const unsigned char*)p->generations;
                    for(k=0;k<p->bone_count*2;++k)rf_scene_npc_startup[3]=(rf_scene_npc_startup[3]^bytes[k])*16777619u;
                }
            }
        }
    }
    if(state_mode) {
        states=malloc(sizeof(*states));if(!states) {status=RF_IO;goto done;}
        status=rf_entity_state_set_open(tables_path,binding.entity.class_name,"",&motions,512*1024,states);if(status)goto done;
    } else {
        status=rf_animation_preview_placed(meshes_path,motions_path,&placement,0,&actor,1024*1024);if(status)goto done;
    }
    status=rf_model_file_open(&model,&archive,binding.mesh.name);if(status)goto done;
    if(campaign_spawn) {float sphere[4];status=rf_model_file_bound_sphere(&model,sphere);
        if(!status)status=rf_model_origin_radius(sphere,&campaign_player_geometry.model_radius);if(status)goto done;}
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
    if(sink && collision && campaign_spawn) {
        rf_materials *textures=&campaign_npc_materials.materials.textures;rf_material *combined;
        stream.npc_base=materials->count;stream.npc_textures=textures->count;
        if((uint64_t)materials->count+textures->count>256){status=RF_RANGE;goto done;}
        combined=malloc((materials->count+textures->count)*sizeof(*combined));if(!combined){status=RF_IO;goto done;}
        memcpy(combined,materials->items,materials->count*sizeof(*combined));
        memcpy(combined+materials->count,textures->items,textures->count*sizeof(*combined));
        free(materials->items);materials->items=combined;materials->count+=textures->count;
        materials->loaded+=textures->loaded;materials->missing+=textures->missing;materials->allocated_bytes+=textures->allocated_bytes;
        free(textures->items);memset(textures,0,sizeof(*textures)); /* Transfer pixels to renderer owner. */
        stream.npc_memory=malloc(4096*96);stream.npc_indices=malloc(24576*sizeof(uint16_t));stream.npc_pool=calloc(1,sizeof(*stream.npc_pool));
        if(!stream.npc_memory || !stream.npc_indices || !stream.npc_pool){status=RF_IO;goto done;}
        for(i=0;i<campaign_poses.count;++i) {
            rf_collision_room_location location;
            status=rf_geometry_collision_world_locate(collision,campaign_seeds.records.items[i].record.position,&location);if(status)goto done;
            status=campaign_model_place(i,campaign_npc_bodies[i].published,campaign_seeds.records.items[i].record.orientation[0],
                campaign_appearances.actor_indices[i],location.room);if(status)goto done;
        }
    }
    if(sink) {
        rf_preview_close(&actor);
        stream.mesh=mesh;stream.materials=materials;stream.bundle=&bundle;
        if(actor_follow_world) {
            status=rf_level_visibility_open(geometry,64*1024,&stream.visibility);if(status)goto done;
            status=rf_level_particles_open(&stream.particles,level,collision,maps,map_count,1,0,512*1024);if(status)goto done;
            stream.particle_workspace=calloc(1,sizeof(*stream.particle_workspace));if(!stream.particle_workspace){status=RF_IO;goto done;}
            memset(rf_scene_particle_draw_summary,0,sizeof(rf_scene_particle_draw_summary));
            memset(rf_scene_particle_draw_frames,0,sizeof(rf_scene_particle_draw_frames));
            rf_scene_particle_draw_summary[5]=2166136261u;
            rf_scene_particle_draw_summary[6]=sizeof(*stream.particle_workspace);
            memset(rf_scene_visibility_summary,0,sizeof(rf_scene_visibility_summary));
            memset(rf_scene_visibility_frames,0,sizeof(rf_scene_visibility_frames));
            memset(rf_scene_particles_summary,0,sizeof(rf_scene_particles_summary));
            memset(rf_scene_particles_frames,0,sizeof(rf_scene_particles_frames));
            rf_scene_particles_summary[1]=stream.particles.materials.count;
            rf_scene_particles_summary[2]=stream.particles.resident_bytes;
            placement.prepare_view=actor_follow_view;placement.view_context=&stream;
        }
        stream.capacity=(uint32_t)capacity;stream.sink=sink;stream.context=context;stream.collision=collision;
        if(campaign_spawn && collision) {
            memset(rf_scene_event_ticks,0,sizeof(rf_scene_event_ticks));
            memset(campaign_ambient_slots,0,sizeof(campaign_ambient_slots));
            for(i=0;i<RF_AMBIENT_SLOTS;i++)campaign_ambient_slots[i].sample=campaign_ambient_slots[i].voice=-1;
            memset(rf_scene_ambient_schedule,0,sizeof(rf_scene_ambient_schedule));campaign_ambient_frame=UINT32_MAX;
            /* Original level startup435df0 calls45ade0 before levelstart.vcs. */
            status=campaign_ambient_schedule(0,1);if(status)goto done;
            status=rf_runtime_startup_events(&campaign_triggers,&scene_gravity,0,0,&stream.particles, &campaign_forces,&rf_scene_startup_events);
            if(status)goto done;
            campaign_force_snapshot();campaign_switch_snapshot();
            memcpy(rf_scene_startup_gravity,&scene_gravity,sizeof(scene_gravity));
        }
        if(state_mode)status=rf_animation_stream_states(meshes_path,motions_path,1024*1024,&placement,states,scene_frame,&stream);
        else status=rf_animation_stream_placed(meshes_path,motions_path,1024*1024,&placement,scene_frame,&stream);
        if(!status && collision && rf_scene_actor_route_enabled && !rf_scene_actor_live_enabled)status=actor_routes(&stream);
    }
done:
    free(stream.npc_memory);free(stream.npc_indices);free(stream.npc_pool);
    rf_level_visibility_close(&stream.visibility);
    rf_level_particles_close(&stream.particles);
    free(stream.particle_workspace);particle_draw_stream=NULL;
    campaign_close_movers();
    rf_physics_forces_close(&campaign_forces);
    rf_group_registration_close(&campaign_group_registration);
    rf_group_runtime_close(&campaign_group_runtime);
    rf_level_owned_groups_close(&campaign_groups);
    rf_runtime_triggers_close(&campaign_triggers);
    rf_runtime_events_close(&campaign_events);
    rf_level_owned_ambient_close(&campaign_ambient);
    memset(&campaign_climb,0,sizeof(campaign_climb));rf_level_owned_regions_close(&campaign_regions);
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
