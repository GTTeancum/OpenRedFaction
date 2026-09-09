#include "rf/scene_preview.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
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
typedef struct scene_stream {
    rf_preview_mesh *mesh;rf_materials *materials;const rf_model_materials *bundle;
    uint32_t world,base,capacity;rf_scene_frame_sink sink;void *context;
    const rf_geometry_collision_world *collision;
} scene_stream;
rf_physics_body scene_actor_body;
uint32_t rf_scene_actor_physics_diagnostic[8];
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
uint32_t rf_scene_actor_landing[8]; /* magic, descriptor index, frame, transitions, idle ticks, status, reserved */
static int actor_ground_check(const rf_geometry_collision_world *world,uint32_t frame)
{
    actor_ground_record *r=rf_scene_actor_ground_records+frame;float start[3],delta[3];uint32_t k;int status;
    if(frame==0) {
        memset(rf_scene_actor_ground_stats,0,sizeof(rf_scene_actor_ground_stats));
        rf_scene_actor_ground_stats[0]=0x52464750;rf_scene_actor_ground_stats[4]=UINT32_MAX;
        rf_scene_actor_ground_stats[5]=2166136261u;rf_scene_actor_ground_stats[6]=sizeof(*r);
    }
    memset(r,0,sizeof(*r));
    /* Prepared falling mode, zero support velocity. Observe support without
     * changing movement mode; the complete landing transition remains open. */
    status=rf_physics_ground_prepare(scene_actor_body.spheres.items,scene_actor_body.spheres.count,
        scene_actor_body.state.position,scene_actor_body.state.state_124,1,1.0f/60,0,0,&r->probe);if(status)return status;
    for(k=0;k<3;++k) {
        start[k]=(float)((double)r->probe.start[k]+r->probe.sphere.center[k]);
        delta[k]=(float)((double)r->probe.end[k]-r->probe.start[k]);
    }
    status=rf_geometry_collision_world_sweep(world,r->probe.query_flags,start,delta,r->probe.sphere.radius,1,&r->hit,&r->matched);
    if(status)return status;
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
    float normal[3],float *fraction,uint32_t *sphere)
{
    float delta[3],start[3];uint32_t i,k,matched;
    *fraction=1;*sphere=UINT32_MAX;
    for(k=0;k<3;++k)delta[k]=state->next_position[k]-state->position[k];
    if(delta[0]==0 && delta[1]==0 && delta[2]==0)return RF_OK; /* 4df1c0 zero-displacement exit */
    for(i=0;i<scene_actor_body.spheres.count;++i) {
        const rf_physics_sphere *s=scene_actor_body.spheres.items+i;rf_geometry_world_sweep_hit hit;int status;
        for(k=0;k<3;++k)start[k]=(float)((double)state->position[k]+(double)s->center[0]*state->orientation[k]+
            (double)s->center[1]*state->orientation[3+k]+(double)s->center[2]*state->orientation[6+k]);
        status=rf_geometry_collision_world_sweep(world,0x460,start,delta,s->radius,1,&hit,&matched);if(status)return status;
        if(matched && hit.hit.fraction<*fraction) {*sphere=i;*fraction=hit.hit.fraction;memcpy(normal,hit.hit.normal,12);}
    }
    return RF_OK;
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
        {int status=actor_sweep(world,&proposal,normal,&fraction,&sphere);if(status)return status;}
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
                    status=actor_sweep(world,&current,normal,&hit_fraction,&hit_sphere);if(status)return status;
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
static int actor_tick(const rf_geometry_collision_world *world,rf_physics_body_state *state)
{
    float remaining=1.0f/60,support[3]={0},normal[3];uint32_t pass=0,contacts=0;int status;
    int grounded=rf_scene_actor_landing[1]==1;
    if(grounded)++rf_scene_actor_landing[4];
    state->flags&=~0x1000000u;
    do {
        float fraction,impact;uint32_t sphere;
        if(grounded) {
            /* Ordinary run drag selection at 49f79a; steering remains zero in
             * this passive scene. Shared proposal now handles nonzero velocity
             * and force instead of an idle-only assignment. */
            float drag=fmaxf(.5f,(float)((double)state->coefficients[1]/state->mass));
            status=rf_physics_ground_propose(state,remaining,drag,support,support);
        } else status=rf_physics_fall_propose(state,remaining,9.8f,support);
        if(status)return status;
        memset(state->vector_e0,0,sizeof(state->vector_e0)); /* full 49f3c0 clears force after proposal */
        state->flags|=0x1000000;
        status=actor_sweep(world,state,normal,&fraction,&sphere);if(status)return status;
        if(sphere==UINT32_MAX) {
            memcpy(state->position,state->next_position,sizeof(state->position));state->scalar_144=1;remaining=0;
        } else {
            ++contacts;
            status=rf_physics_contact_advance(state,remaining,fraction,&remaining);if(status)return status;
            status=rf_physics_static_contact(state,normal,support,support,&impact);if(status)return status;
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
static int scene_frame(void *context,uint32_t frame,rf_preview_mesh *actor)
{
    scene_stream *stream=context;uint32_t i,slot;
    if(stream->collision && frame==0) {
        int status=rf_scene_actor_world_check(stream->collision,rf_scene_actor_initial_world);if(status)return status;
        status=rf_scene_actor_fall_check(stream->collision,rf_scene_actor_initial_fall);if(status)return status;
        memset(&rf_scene_actor_pose,0,sizeof(rf_scene_actor_pose));
        rf_scene_actor_pose.radius=scene_actor_body.state.bounds.radius;
        status=rf_group_pose_set_position(&rf_scene_actor_pose,scene_actor_body.state.position);if(status)return status;
        memset(rf_scene_actor_tick_stats,0,sizeof(rf_scene_actor_tick_stats));rf_scene_actor_tick_stats[0]=0x5246544b;
        memset(rf_scene_actor_landing,0,sizeof(rf_scene_actor_landing));
        rf_scene_actor_landing[0]=0x52464c44;rf_scene_actor_landing[1]=3;rf_scene_actor_landing[2]=UINT32_MAX;
    }
    uint64_t bytes=(uint64_t)stream->world*sizeof(rf_preview_vertex)+actor->bytes;
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
    memcpy(stream->mesh->vertices+stream->world,actor->vertices,actor->bytes);
    if(stream->collision) {
        uint32_t hash=2166136261u;
        int status=actor_ground_check(stream->collision,frame);if(status)return status;
        for(i=0;i<actor->bytes;++i)hash=(hash^((const unsigned char*)actor->vertices)[i])*16777619u;
        rf_scene_actor_render_frames[frame][0]=actor->count;rf_scene_actor_render_frames[frame][1]=hash;
        memcpy(rf_scene_actor_render_frames[frame]+2,scene_actor_body.state.position,12);
    }
    stream->mesh->count=stream->world+actor->count;stream->mesh->bytes=(uint32_t)bytes;
    {
        int status=stream->sink(stream->context,frame,stream->mesh,stream->materials,stream->world);if(status)return status;
        if(stream->collision && frame<63) {
            rf_physics_body_state next=scene_actor_body.state;
            const actor_ground_record *ground=rf_scene_actor_ground_records+frame;
            if(rf_scene_actor_landing[1]==3 && ground->matched && ground->hit.hit.fraction<1 && ground->hit.hit.normal[1]>=.5f) {
                status=rf_physics_static_land(&next,&ground->probe,ground->hit.hit.fraction);if(status)return status;
                rf_scene_actor_landing[1]=1;rf_scene_actor_landing[2]=frame;
                ++rf_scene_actor_landing[3];rf_scene_actor_landing[5]=1;
            }
            status=actor_tick(stream->collision,&next);if(status)return status;
            status=rf_group_pose_set_position(&rf_scene_actor_pose,next.position);if(status)return status;
            memcpy(next.position,rf_scene_actor_pose.position,12);memcpy(next.next_position,rf_scene_actor_pose.pending,12);
            memcpy(next.bounds.minimum,rf_scene_actor_pose.minimum,12);memcpy(next.bounds.maximum,rf_scene_actor_pose.maximum,12);
            scene_actor_body.state=next;
        }
        return RF_OK;
    }
}
static int scene_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context,int state_mode,const rf_geometry_collision_world *collision)
{
    rf_vpp archive,motions;rf_model_file model;rf_level_actor_assets binding;rf_entity_physics_config physics_config;
    rf_entity_state_set *states=NULL;int motions_opened=0;
    rf_animation_placement placement;rf_preview_mesh actor={0};rf_model_materials bundle={0};
    rf_preview_vertex *vertices=NULL;rf_material *items=NULL;const char *names[64];
    uint64_t bytes,count,capacity;uint32_t i;int status;scene_stream stream={0};
    if(!level || !mesh || !materials || !mesh->vertices || !materials->items ||
       mesh->count%3 || mesh->bytes!=(uint64_t)mesh->count*sizeof(*mesh->vertices) ||
       materials->allocated_bytes>=material_budget)return RF_RANGE;
    stream.world=mesh->count;stream.base=materials->count;
    if(sink && (uint64_t)mesh->bytes+1024*1024>mesh_budget)return RF_RANGE;
    status=rf_vpp_open(&archive,meshes_path);if(status)return status;
    status=rf_level_actor_assets_load(level,uid,tables_path,&archive,512*1024,&binding);if(status)goto done;
    if(strcmp(binding.mesh.name,"miner.v3c")) {status=RF_FORMAT;goto done;}
    status=rf_animation_placement_from_level(level,&binding.entity,&placement);if(status)goto done;
    if(sink) {
        rf_vpp tables;status=rf_vpp_open(&tables,tables_path);if(status)goto done;
        status=rf_entity_physics_config_load(&tables,binding.entity.class_name,512*1024,&physics_config);
        rf_vpp_close(&tables);if(status)goto done;
        /* Live landing currently implements the ordinary class-run branch.
         * Reject other descriptors/special landing classes rather than silently
         * treating them as this passive miner fixture. */
        if(collision && (physics_config.authored.movement_index!=1 ||
           !(physics_config.authored.flags&1) || (physics_config.authored.flags&0x2000000))) {status=RF_FORMAT;goto done;}
        rf_physics_body_close(&scene_actor_body);memset(rf_scene_actor_physics_diagnostic,0,sizeof(rf_scene_actor_physics_diagnostic));
        placement.physics_config=&physics_config;placement.physics_body=&scene_actor_body;
        placement.physics_diagnostic=rf_scene_actor_physics_diagnostic;
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
    capacity=sink?(uint64_t)mesh->bytes+1024*1024:bytes;
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
        stream.capacity=(uint32_t)capacity;stream.sink=sink;stream.context=context;stream.collision=collision;
        if(state_mode)status=rf_animation_stream_states(meshes_path,motions_path,1024*1024,&placement,states,scene_frame,&stream);
        else status=rf_animation_stream_placed(meshes_path,motions_path,1024*1024,&placement,scene_frame,&stream);
    }
done:
    free(states);if(motions_opened)rf_vpp_close(&motions);
    free(vertices);free(items);rf_preview_close(&actor);rf_model_materials_close(&bundle);
    rf_vpp_close(&archive);return status;
}
int rf_scene_preview_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget)
{
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,NULL,NULL,0,NULL);
}
int rf_scene_stream_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context)
{
    if(!sink)return RF_RANGE;
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,sink,context,0,NULL);
}
int rf_scene_stream_miner_states(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context)
{
    if(!sink)return RF_RANGE;
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,sink,context,1,NULL);
}
int rf_scene_stream_miner_body(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget,
    rf_scene_frame_sink sink,void *context,const rf_geometry_collision_world *collision)
{
    if(!sink || !collision)return RF_RANGE;
    return scene_miner(level,uid,meshes_path,motions_path,tables_path,maps,map_count,
        mesh,materials,mesh_budget,material_budget,sink,context,1,collision);
}
