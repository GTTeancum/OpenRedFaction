#include "rf/eye.h"
#include "rf/scene_preview.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
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
    uint32_t world,base,capacity;rf_scene_frame_sink sink;void *context;
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
static rf_camera_effect_state campaign_force_shake;
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
static rf_entity_view campaign_player_view;
static rf_registered_entity_view campaign_player_object;
uint32_t rf_scene_campaign_player[4]; /* registered handle, kind, initial object flags, adapter bytes */
uint32_t rf_scene_trigger_contacts[6]; /* polls, ready, last ready UID, lower door ready, unsupported, status */
typedef struct campaign_controller_effects {
    uint32_t source,actor,pending,starts,start_frame;
    rf_group_sound_state sounds;
} campaign_controller_effects;
static campaign_controller_effects *campaign_controller_requests;
static rf_audio_bank campaign_audio_bank;
static rf_audio_mixer campaign_audio_mixer;
static int16_t campaign_audio_frame[1600];
static rf_scene_audio_sink campaign_audio_sink;
static void *campaign_audio_context;
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
typedef struct campaign_spatial_voice {
    uint32_t handle,sample;float position[3],volume;int32_t last_volume,last_pan;
} campaign_spatial_voice;
static campaign_spatial_voice campaign_spatial_voices[RF_AUDIO_VOICES];
static float campaign_listener_position[3],campaign_listener_right[3];
/* Initial updates, refresh updates, integer gain/pan hash, noncenter updates,
 * changed settings, fixed tracking bytes. Unity PCM diagnostic is separate. */
uint32_t rf_scene_spatial_audio[6];
static void campaign_spatial_update(campaign_spatial_voice *voice,int initial)
{
    const rf_audio_parameters *parameters=rf_audio_bank_parameters(&campaign_audio_bank,voice->sample);
    float spatial[2],gains[2],volume;int32_t device_volume,pan;
    if(!parameters || !voice->handle)return;
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
static int campaign_audio_open(const char *tables_path,const char *level_name)
{
    char path[1024];size_t prefix=0,n;uint32_t i,j,index,capacity;
    rf_vpp archive={0},tables={0};rf_audio_declaration *declarations=NULL;uint32_t declared=0;int status;
    memset(campaign_spatial_voices,0,sizeof(campaign_spatial_voices));
    memset(campaign_listener_position,0,sizeof(campaign_listener_position));
    memset(campaign_listener_right,0,sizeof(campaign_listener_right));campaign_listener_right[0]=-1;
    memset(rf_scene_spatial_audio,0,sizeof(rf_scene_spatial_audio));rf_scene_spatial_audio[2]=2166136261u;
    rf_scene_spatial_audio[5]=sizeof(campaign_spatial_voices)+sizeof(campaign_listener_position)+sizeof(campaign_listener_right);
    memset(rf_scene_live_audio,0,sizeof(rf_scene_live_audio));rf_scene_live_audio[7]=2166136261u;
    memset(rf_scene_sound_bank,0,sizeof(rf_scene_sound_bank));
    rf_audio_mixer_init(&campaign_audio_mixer);
    for(i=0;i<campaign_group_runtime.count;i++)for(j=0;j<4;j++) {
        campaign_controller_requests[i].sounds.samples[j]=-1;
        campaign_controller_requests[i].sounds.handles[j]=-1;
    }
    for(n=0;tables_path[n];n++)if(tables_path[n]=='/' || tables_path[n]=='\\')prefix=n+1;
    if(prefix+sizeof("audio.vpp")>sizeof(path))return RF_RANGE;
    memcpy(path,tables_path,prefix);memcpy(path+prefix,"audio.vpp",sizeof("audio.vpp"));
    status=rf_vpp_open(&archive,path);if(status)return status;
    status=rf_vpp_open(&tables,tables_path);if(status)goto audio_done;
    status=rf_sound_table_load(&tables,65536,NULL,0,&declared);if(status)goto audio_done;
    if(declared) {
        declarations=malloc((size_t)declared*sizeof(*declarations));if(!declarations){status=RF_RANGE;goto audio_done;}
        status=rf_sound_table_load(&tables,65536,declarations,declared,&declared);if(status)goto audio_done;
    }
    capacity=declared+campaign_group_runtime.count*4;if(!capacity)capacity=1;if(capacity>2600)capacity=2600;
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
    free(declarations);declarations=NULL;rf_vpp_close(&tables);
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
audio_done:
    free(declarations);rf_vpp_close(&tables);
    rf_vpp_close(&archive);campaign_audio_bank.archive=NULL; /* All loading is complete. */
    return status;
}
static int32_t campaign_sound_play(void *context,int32_t sample,const float position[3],float volume,uint32_t flags)
{
    const rf_wave_pcm *pcm;uint32_t handle;
    (void)context;(void)flags;
    /* The deterministic PCM diagnostic remains unity/nonspatial. Device
     * output receives the separate listener-driven gain path below. */
    pcm=sample<0?NULL:rf_audio_bank_sample(&campaign_audio_bank,(uint32_t)sample);
    if(!pcm || rf_audio_voice_start(&campaign_audio_mixer,pcm,32768,32768,0,&handle)) {
        ++rf_scene_live_audio[5];return -1;
    }
    ++rf_scene_live_audio[4];
    {campaign_spatial_voice *voice=campaign_spatial_voices+(handle&0xffff);
     voice->handle=handle;voice->sample=(uint32_t)sample;voice->volume=volume;
     memcpy(voice->position,position,12);voice->last_volume=INT32_MIN;voice->last_pan=INT32_MIN;
     float gains[2];campaign_spatial_update(voice,1);
     if(!rf_audio_device_gains(voice->last_volume,voice->last_pan,gains) && campaign_audio_events.play)
        campaign_audio_events.play(campaign_audio_events_context,handle,pcm,gains[0],gains[1]);}
    return (int32_t)handle;
}
static void campaign_sound_request(rf_group_runtime_entry *entry,campaign_controller_effects *request,uint32_t effects)
{
    if(effects&RF_GROUP_SOUND_START)rf_group_sound_start(&request->sounds,entry->translation.motion.flags,
        entry->translation.motion.next_key,entry->pose.public_position,campaign_sound_play,NULL);
    if(effects&RF_GROUP_SOUND_END) {
        /* Original 46a0d0: stop retained moving voice, then play arrival slot. */
        if(request->sounds.handles[1]!=-1) {
            rf_audio_voice_stop(&campaign_audio_mixer,(uint32_t)request->sounds.handles[1]);
            if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,(uint32_t)request->sounds.handles[1]);
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
        if(record->value_byte || record->fields[2]!=UINT32_MAX || record->fields[0]!=UINT32_MAX ||
           record->fields[1]!=UINT32_MAX || record->script[0] ||
           (trigger->state.flags&(2u|128u))) {++rf_scene_trigger_contacts[4];continue;}
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
    status=rf_audio_mix(&campaign_audio_mixer,campaign_audio_frame,800);if(status)return status;
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
static void campaign_close_movers(void)
{
    memset(campaign_spatial_voices,0,sizeof(campaign_spatial_voices));
    if(campaign_audio_events.reset)campaign_audio_events.reset(campaign_audio_events_context);
    rf_audio_mixer_init(&campaign_audio_mixer);rf_audio_bank_close(&campaign_audio_bank);
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
static int campaign_resolve_trigger_links(void)
{
    uint32_t i,j,n=campaign_events.count+campaign_triggers.count+campaign_group_registration.count+campaign_mover_count;
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
    /* Fixture order: events, triggers, controllers, then movers. Entity registration
     * and original whole-world handle order remain incomplete. */
    status=rf_runtime_triggers_resolve(&campaign_triggers,objects,n,
        campaign_group_registration.keys,campaign_group_registration.key_count);
    if(!status)status=rf_runtime_events_resolve(&campaign_events,objects,n,
        campaign_group_registration.keys,campaign_group_registration.key_count);
    free(objects);
    memset(rf_scene_campaign_links,0,sizeof(rf_scene_campaign_links));
    if(status)return status;
    memset(rf_scene_campaign_event_links,0,sizeof(rf_scene_campaign_event_links));
    rf_scene_campaign_event_links[3]=2166136261u;
    for(i=0;i<campaign_events.count;++i)for(j=0;j<campaign_events.items[i].authored->record.link_count;++j) {
        rf_level_link_target *target=campaign_events.items[i].links+j;
        uint32_t k,words[4]={campaign_events.items[i].authored->links[j],target->value,target->kind,target->index};
        for(k=0;k<4;++k)rf_scene_campaign_event_links[3]=(rf_scene_campaign_event_links[3]^words[k])*16777619u;
        ++rf_scene_campaign_event_links[0];++rf_scene_campaign_event_links[target->kind?1:2];
    }
    rf_scene_campaign_links[3]=2166136261u;
    for(i=0;i<campaign_triggers.count;++i)for(j=0;j<campaign_triggers.items[i].authored->record.link_count;++j) {
        rf_level_link_target *target=campaign_triggers.items[i].links+j;
        uint32_t k,words[4]={campaign_triggers.items[i].authored->links[j],target->value,target->kind,target->index};
        for(k=0;k<4;++k)rf_scene_campaign_links[3]=(rf_scene_campaign_links[3]^words[k])*16777619u;
        ++rf_scene_campaign_links[0];
        ++rf_scene_campaign_links[campaign_triggers.items[i].links[j].kind?1:2];
    }
    return RF_OK;
}
uint32_t rf_scene_campaign_events[3]; /* registered events, owner bytes, registry bytes */
static rf_movement_descriptor campaign_modes[16];
static float campaign_jump_strength;
static rf_player_climb_state campaign_climb;
static const float campaign_identity[3][3]={{1,0,0},{0,1,0},{0,0,1}};
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
uint32_t rf_scene_actor_initial_world[8],rf_scene_actor_initial_fall[8];
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

uint32_t rf_scene_actor_ground_queries[4]; /* queries, hits, mover hits, status */
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
        rf_collision_body_sphere spheres[8];rf_collision_body_query query={0};
        int status;
        if(!actor_follow_world || !campaign_surface_palette || !campaign_surface_sources || scene_actor_body.spheres.count>8)return RF_RANGE;
        for(i=0;i<scene_actor_body.spheres.count;i++) {
            memcpy(spheres[i].center,scene_actor_body.spheres.items[i].center,12);
            spheres[i].radius=scene_actor_body.spheres.items[i].radius;
        }
        memcpy(query.start,state->position,12);memcpy(query.end,state->next_position,12);
        memcpy(query.matrix,state->orientation,36);query.radius=state->bounds.radius;
        query.flags=query_flags;query.spheres=spheres;query.count=scene_actor_body.spheres.count;query.limit=1;
        status=campaign_body_query(world,&query,&rf_scene_actor_body_contact,&matched);
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
        status=rf_camera_effect_start(&campaign_force_shake,amplitude,.05f,now);if(status)return status;
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
        status=rf_camera_effect_apply_random(&campaign_force_shake,now,&stream->particles.state->random,(float *)orientation,&active);
        if(status)return status;
    }
    if(campaign_spawn)campaign_audio_listener(position,orientation[0]);
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
    {uint32_t world_capacity=(stream->capacity-1024*1024)/sizeof(rf_preview_vertex)*sizeof(rf_preview_vertex);
     rf_preview_failure[0]=0;
     if(rf_scene_actor_eye_enabled && stream->mesh->bytes>world_capacity)
        status=rf_scene_world_update_camera(actor_follow_world,campaign_movers.poses,campaign_movers.count,position,orientation,stream->mesh,stream->capacity);
     else {
        status=rf_scene_world_update_camera_staged(actor_follow_world,campaign_movers.poses,campaign_movers.count,position,orientation,
        stream->mesh,world_capacity,stream->mesh->vertices+world_capacity/sizeof(rf_preview_vertex),
        stream->capacity-world_capacity);
        /* First-person rendering has no visible actor prefix to reserve. A
         * large world may use the whole allocation through the transactional
         * two-pass path instead of terminating at the staging-half boundary. */
        if(status==RF_RANGE && rf_scene_actor_eye_enabled && rf_preview_failure[0])
            status=rf_scene_world_update_camera(actor_follow_world,campaign_movers.poses,campaign_movers.count,position,orientation,stream->mesh,stream->capacity);
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
        if(!status && campaign_spawn) {
            campaign_force_class_flags=physics_config.authored.flags;campaign_force_class_kind=physics_config.authored.use_kind;
            campaign_force_air_limit=0;memset(rf_scene_force_ticks,0,sizeof(rf_scene_force_ticks));
            status=rf_camera_effect_reset(&campaign_force_shake,0);
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
        rf_vpp_close(&tables);if(status)goto done;
        if(campaign_spawn && collision) {
            rf_object_registry_init(&campaign_registry);
            status=rf_runtime_events_open(level,&campaign_registry,1024*1024,&campaign_events);
            if(status)goto done;
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
            status=campaign_audio_open(tables_path,level->entry.name);if(status)goto done;
            memset(rf_scene_live_activation,0,sizeof(rf_scene_live_activation));campaign_actor_controller=UINT32_MAX;
            memset(rf_scene_trigger_contacts,0,sizeof(rf_scene_trigger_contacts));
            memset(&campaign_entities,0,sizeof(campaign_entities));memset(&campaign_player_view,0,sizeof(campaign_player_view));
            campaign_player_view.handle=-1;campaign_player_view.type=0;campaign_player_view.class_type=-1;
            campaign_player_view.flags_7c=8; /* Confirmed local-player object bit; full factory flags remain separate. */
            campaign_player_view.linked_handle=-1;campaign_player_view.weapons[0]=campaign_player_view.weapons[1]=-1;
            campaign_player_view.action_520=-1;campaign_player_view.base_speed=rf_scene_actor_movement_values.speed;
            status=rf_entity_view_register(&campaign_registry,&campaign_entities,&campaign_player_view,&campaign_player_object);if(status)goto done;
            rf_scene_campaign_player[0]=campaign_player_object.handle;rf_scene_campaign_player[1]=campaign_player_object.object_kind;
            rf_scene_campaign_player[2]=campaign_player_view.flags_7c;
            rf_scene_campaign_player[3]=sizeof(campaign_entities)+sizeof(campaign_player_view)+sizeof(campaign_player_object);
            memset(rf_scene_actor_body_sweeps,0,sizeof(rf_scene_actor_body_sweeps));
            memset(rf_scene_actor_ground_queries,0,sizeof(rf_scene_actor_ground_queries));
            memset(rf_scene_actor_ground_contacts,0,sizeof(rf_scene_actor_ground_contacts));
            memset(campaign_support_velocity,0,sizeof(campaign_support_velocity));campaign_support_handle=0;
            memset(&rf_scene_actor_body_contact,0,sizeof(rf_scene_actor_body_contact));
            rf_scene_actor_body_sweeps[4]=(campaign_movers.count?campaign_movers.count:1)*sizeof(*campaign_sweep_scratch)+
                (campaign_movers.count+1)*sizeof(*campaign_surface_sources)+sizeof(*campaign_surface_palette);
            status=campaign_bind_movers();if(status)goto done;
            status=campaign_resolve_trigger_links();if(status)goto done;
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
        placement.campaign_player=campaign_spawn;
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
