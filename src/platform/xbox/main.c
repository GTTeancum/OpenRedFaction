#include "rf/resource_budget.h"
#include "rf/vpp.h"
#include "rf/object_registry.h"
#include "rf/checksum.h"
#include "rf/level.h"
#include "rf/geometry.h"
#include "rf/material.h"
#include "rf/lightmap.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include "rf/scene_preview.h"
#include "rf/frame_clock.h"
#include "renderer.h"
#include "input.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <hal/debug.h>
#include <hal/video.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

/* Read-only monitor evidence. Resolve its VA from the matching linker map. */
volatile uint32_t rf_diagnostic[58] = {0x52464447u, 9u, 0};
static rf_geometry resident_geometry;
static rf_scene_world_geometry resident_render_geometry;
static int door_render_frame(void);
static rf_preview_mesh door_mesh;
static uint32_t door_capacity;
rf_frame_clock rf_player_frame_clock;
static uint32_t player_pacing,scene_simulation_frames;
static FILE *player_replay;
static uint32_t player_replay_size;
uint32_t rf_player_replay_diagnostic[4]; /* active, records, consumed, read status */
static void player_input_close(void)
{
    if(player_replay){fclose(player_replay);player_replay=NULL;}
    rf_player_replay_diagnostic[0]=0;rf_xbox_input_close();
}
static uint32_t profile_milliseconds(void){return GetTickCount();}
static int player_poll_paced(void *context,uint32_t frame,rf_scene_input *input)
{
    if(player_replay) {
        memset(input,0,sizeof(*input));
        int status=frame!=rf_player_replay_diagnostic[2]?RF_FORMAT:
            fread(input,player_replay_size,1,player_replay)!=1?RF_IO:RF_OK;
        rf_player_replay_diagnostic[3]=(uint32_t)status;
        if(!status)++rf_player_replay_diagnostic[2];return status;
    }
    if(player_pacing) {
        uint32_t wait;
        while((wait=rf_frame_clock_step(&rf_player_frame_clock,GetTickCount()))!=0)Sleep(wait);
    }
    return rf_xbox_input_poll(context,frame,input);
}
volatile uint32_t rf_door_render_diagnostic[10]={0x52464452u};
static rf_geometry_collision_world resident_collision;
volatile uint32_t rf_collision_diagnostic[9]={0x52464357u};
volatile uint32_t rf_sweep_diagnostic[8]={0x52465357u};
static rf_group_mover_memberships resident_memberships;
static rf_group_object *resident_mover_objects;
static uint32_t *resident_controller_handles;
static rf_object_registry resident_registry;
volatile uint32_t rf_registry_diagnostic[6]={0x52465247u};
volatile uint32_t rf_group_membership_diagnostic[11]={0x5246474du};
static int membership_check(void);
static rf_group_runtime_collection resident_group_runtime;
volatile uint32_t rf_group_runtime_diagnostic[8]={0x52464752u};
static uint32_t group_runtime_hash(void)
{
    uint32_t i,j,part,hash=2166136261u;
    for(i=0;i<resident_group_runtime.count;i++) {
        const rf_group_runtime_entry *e=resident_group_runtime.items+i;
        const void *data[4]={&e->kind,&e->initial_flags,&e->translation,&e->pose};uint32_t sizes[4]={4,4,76,236};
        for(part=0;part<4;part++)for(j=0;j<sizes[part];j++)hash=(hash^((const unsigned char *)data[part])[j])*16777619u;
    }
    return hash;
}
static rf_level_owned_triggers resident_triggers;
static rf_level_owned_events resident_events;
static rf_level_owned_entities resident_entities;
rf_entity_physics_config resident_miner_config;
volatile uint32_t rf_actor_creation_diagnostic[6]={0x52464143u};
uint32_t rf_actor_world_diagnostic[8];
uint32_t rf_actor_fall_diagnostic[8];
static int actor_body_preview,actor_follow_preview;
extern uint32_t rf_scene_actor_live_enabled;
extern uint32_t rf_scene_actor_initial_world[8],rf_scene_actor_initial_fall[8];
volatile uint32_t rf_level_logic_diagnostic[12]={0x52464c47u};
static uint32_t logic_hash_part(uint32_t hash,const void *data,uint32_t bytes)
{
    uint32_t i;for(i=0;i<bytes;++i)hash=(hash^((const unsigned char *)data)[i])*16777619u;
    return hash;
}
static uint32_t logic_storage_hash(void)
{
    uint32_t i,hash=2166136261u;
    for(i=0;i<resident_triggers.count;++i) {
        const rf_level_owned_trigger *v=resident_triggers.items+i;
        hash=logic_hash_part(hash,&v->record,sizeof(v->record));
        hash=logic_hash_part(hash,v->links,v->record.link_count*4);
    }
    for(i=0;i<resident_events.count;++i) {
        const rf_level_owned_event *v=resident_events.items+i;
        hash=logic_hash_part(hash,&v->record,sizeof(v->record));
        hash=logic_hash_part(hash,v->links,v->record.link_count*4);
    }
    for(i=0;i<resident_entities.count;++i) {
        const rf_level_owned_entity *v=resident_entities.items+i;
        hash=logic_hash_part(hash,&v->record,sizeof(v->record));
        hash=logic_hash_part(hash,v->raw,v->record.bytes);
    }
    return hash;
}
static int logic_storage_check(void)
{
    uint32_t hash=logic_storage_hash();rf_level_logic_diagnostic[8]=hash;rf_level_logic_diagnostic[9]++;
    if(hash!=rf_level_logic_diagnostic[7]) {rf_level_logic_diagnostic[1]=(uint32_t)RF_FORMAT;return RF_FORMAT;}
    return RF_OK;
}
static int logic_storage_open(const rf_level *level)
{
    uint32_t i;int status=rf_level_owned_triggers_open(level,512u*1024u,&resident_triggers);
    if(!status)status=rf_level_owned_events_open(level,512u*1024u-resident_triggers.allocated_bytes,&resident_events);
    if(!status)status=rf_level_owned_entities_open(level,512u*1024u-resident_triggers.allocated_bytes-resident_events.allocated_bytes,&resident_entities);
    if(status) {
        rf_level_owned_triggers_close(&resident_triggers);rf_level_owned_events_close(&resident_events);
        rf_level_owned_entities_close(&resident_entities);
        rf_level_logic_diagnostic[1]=(uint32_t)status;return status;
    }
    rf_level_logic_diagnostic[1]=1;rf_level_logic_diagnostic[2]=resident_triggers.count;
    rf_level_logic_diagnostic[3]=resident_events.count;
    rf_level_logic_diagnostic[4]=resident_triggers.allocated_bytes+resident_events.allocated_bytes+resident_entities.allocated_bytes;
    rf_level_logic_diagnostic[10]=resident_entities.count;rf_level_logic_diagnostic[11]=resident_entities.allocated_bytes;
    for(i=0;i<resident_triggers.count;++i)rf_level_logic_diagnostic[5]+=resident_triggers.items[i].record.link_count;
    for(i=0;i<resident_events.count;++i)rf_level_logic_diagnostic[6]+=resident_events.items[i].record.link_count;
    rf_level_logic_diagnostic[7]=logic_storage_hash();
    for(i=0;i<resident_entities.count;++i)if(resident_entities.items[i].record.uid==9858) {
        rf_vpp tables;
        rf_actor_creation_diagnostic[1]=2;rf_actor_creation_diagnostic[2]=9858;
        status=rf_vpp_open(&tables,"D:\\tables.vpp");
        if(!status) {
            status=rf_entity_physics_config_load(&tables,resident_entities.items[i].record.class_name,
                512u*1024u,&resident_miner_config);
            rf_vpp_close(&tables);
        }
        rf_actor_creation_diagnostic[1]=status?(uint32_t)status:1;
        rf_actor_creation_diagnostic[2]=9858;
        rf_actor_creation_diagnostic[3]=sizeof(resident_miner_config);
        if(!status) {
            rf_actor_creation_diagnostic[4]=logic_hash_part(2166136261u,&resident_miner_config,sizeof(resident_miner_config));
            rf_actor_creation_diagnostic[5]=resident_miner_config.spheres.count;
        }
        if(status)return status;break;
    }
    return RF_OK;
}
static rf_level_owned_groups resident_groups;
volatile uint32_t rf_group_storage_diagnostic[10]={0x52464753u};
static uint32_t group_storage_hash(void)
{
    uint32_t i,j,part,hash=2166136261u;
    for(i=0;i<resident_groups.count;i++) {
        const rf_level_owned_group *g=resident_groups.groups+i;
        const void *data[5]={&g->record,g->keys,g->legacy,g->ids[0],g->ids[1]};
        uint32_t sizes[5]={sizeof(g->record),g->record.key_count*sizeof(*g->keys),g->record.legacy_count*sizeof(*g->legacy),g->record.ids_count[0]*4,g->record.ids_count[1]*4};
        for(part=0;part<5;part++)for(j=0;j<sizes[part];j++)hash=(hash^((const unsigned char *)data[part])[j])*16777619u;
    }
    return hash;
}
static int group_storage_check(void)
{
    uint32_t hash;if(logic_storage_check())return RF_FORMAT;
    hash=group_storage_hash();rf_group_storage_diagnostic[8]=hash;rf_group_storage_diagnostic[9]++;
    if(hash!=rf_group_storage_diagnostic[7]) {rf_group_storage_diagnostic[1]=(uint32_t)RF_FORMAT;return RF_FORMAT;}
    hash=group_runtime_hash();rf_group_runtime_diagnostic[7]=hash;
    if(hash!=rf_group_runtime_diagnostic[6]) {rf_group_runtime_diagnostic[1]=(uint32_t)RF_FORMAT;return RF_FORMAT;}
    return rf_group_membership_diagnostic[1]==1?membership_check():RF_OK;
}
static int group_storage_open(const rf_level *level)
{
    uint32_t i;int status=logic_storage_open(level);if(status)return status;
    status=rf_level_owned_groups_open(level,256u*1024u,&resident_groups);
    /* Authored levels may omit the optional group section entirely. */
    if(status==RF_NOT_FOUND) {resident_groups.allocated_bytes=sizeof(resident_groups);status=RF_OK;}
    rf_group_storage_diagnostic[1]=status?(uint32_t)status:1;if(status)return status;
    rf_group_storage_diagnostic[2]=resident_groups.count;rf_group_storage_diagnostic[3]=resident_groups.allocated_bytes;
    for(i=0;i<resident_groups.count;i++) {
        const rf_level_group *g=&resident_groups.groups[i].record;
        rf_group_storage_diagnostic[4]+=g->key_count;rf_group_storage_diagnostic[5]+=g->ids_count[0]+g->ids_count[1];rf_group_storage_diagnostic[6]+=g->legacy_count;
    }
    status=rf_group_runtime_open(&resident_groups,0,64u*1024u,&resident_group_runtime);
    rf_group_runtime_diagnostic[1]=status?(uint32_t)status:1;if(status)return status;
    rf_group_runtime_diagnostic[2]=resident_group_runtime.count;rf_group_runtime_diagnostic[3]=resident_group_runtime.allocated_bytes;
    for(i=0;i<resident_group_runtime.count;i++) {
        rf_group_runtime_diagnostic[4]+=resident_group_runtime.items[i].kind==RF_GROUP_RUNTIME_TRANSLATION;
        rf_group_runtime_diagnostic[5]+=resident_group_runtime.items[i].kind==RF_GROUP_RUNTIME_ROTATION_PENDING;
    }
    rf_group_runtime_diagnostic[6]=group_runtime_hash();
    rf_group_storage_diagnostic[7]=group_storage_hash();return group_storage_check();
}
static rf_geometry_collision_movers resident_movers;
volatile uint32_t rf_mover_diagnostic[12]={0x52464d56u};
static uint32_t membership_hash(void)
{
    uint32_t i,j,part,hash=2166136261u;const unsigned char *objects=(const unsigned char *)resident_mover_objects;
    for(j=0;j<resident_movers.count*sizeof(*resident_mover_objects);j++)hash=(hash^objects[j])*16777619u;
    for(i=0;i<resident_memberships.count;i++) {
        const rf_group_mover_membership *m=resident_memberships.items+i;
        const void *data[3]={&m->count,&m->rotation_sign,m->handles};uint32_t sizes[3]={4,4,m->count*4};
        for(part=0;part<3;part++)for(j=0;j<sizes[part];j++)hash=(hash^((const unsigned char *)data[part])[j])*16777619u;
    }
    return hash;
}
static int membership_check(void)
{
    uint32_t i,hash=membership_hash();rf_group_membership_diagnostic[8]=hash;rf_group_membership_diagnostic[9]++;
    for(i=0;i<resident_movers.count;i++)if(resident_movers.poses[i].flags!=resident_mover_objects[i].flags ||
        resident_movers.views[i].object_id!=resident_mover_objects[i].handle)goto failed;
    for(i=0;i<resident_movers.count;i++)if(rf_object_registry_lookup(&resident_registry,resident_mover_objects[i].handle)!=resident_mover_objects+i)goto failed;
    for(i=0;i<resident_group_runtime.count;i++)if(rf_object_registry_lookup(&resident_registry,resident_controller_handles[i])!=resident_group_runtime.items+i)goto failed;
    rf_registry_diagnostic[5]++;
    if(hash==rf_group_membership_diagnostic[7])return RF_OK;
 failed:
    rf_registry_diagnostic[1]=(uint32_t)RF_FORMAT;rf_group_membership_diagnostic[1]=(uint32_t)RF_FORMAT;return RF_FORMAT;
}
static int membership_open(void)
{
    uint32_t i;int status;
    if((uint64_t)resident_movers.count+resident_group_runtime.count>1024)return RF_RANGE;
    resident_mover_objects=calloc(resident_movers.count?resident_movers.count:1,sizeof(*resident_mover_objects));
    resident_controller_handles=calloc(resident_group_runtime.count?resident_group_runtime.count:1,4);
    if(!resident_mover_objects || !resident_controller_handles) {status=RF_RANGE;goto failed;}
    rf_object_registry_init(&resident_registry);
    for(i=0;i<resident_movers.count;i++) {
        rf_group_object *o=resident_mover_objects+i;o->uid=resident_movers.uids[i];o->type=9;
        o->parent=UINT32_MAX;o->flags=resident_movers.poses[i].flags;
        status=rf_object_registry_insert(&resident_registry,o,&o->handle);if(status)goto failed;
        resident_movers.views[i].object_id=o->handle;
    }
    for(i=0;i<resident_group_runtime.count;i++) {
        status=rf_object_registry_insert(&resident_registry,resident_group_runtime.items+i,resident_controller_handles+i);if(status)goto failed;
    }
    rf_registry_diagnostic[1]=1;rf_registry_diagnostic[2]=resident_movers.count;
    rf_registry_diagnostic[3]=resident_group_runtime.count;rf_registry_diagnostic[4]=sizeof(resident_registry);
    status=rf_group_mover_memberships_open(&resident_group_runtime,resident_mover_objects,resident_movers.count,
        resident_controller_handles,0,64u*1024u,&resident_memberships);if(status)goto failed;
    for(i=0;i<resident_movers.count;i++)resident_movers.poses[i].flags=resident_mover_objects[i].flags;
    rf_group_membership_diagnostic[1]=1;rf_group_membership_diagnostic[2]=resident_memberships.count;rf_group_membership_diagnostic[3]=resident_movers.count;
    rf_group_membership_diagnostic[4]=resident_memberships.allocated_bytes;rf_group_membership_diagnostic[5]=resident_memberships.peak_bytes;
    for(i=0;i<resident_memberships.count;i++)rf_group_membership_diagnostic[6]+=resident_memberships.items[i].count;
    rf_group_membership_diagnostic[7]=membership_hash();
    rf_group_membership_diagnostic[10]=resident_movers.count*sizeof(*resident_mover_objects)+resident_group_runtime.count*4;
    return membership_check();
 failed:
    rf_object_registry_init(&resident_registry);rf_registry_diagnostic[1]=(uint32_t)status;
    free(resident_mover_objects);resident_mover_objects=NULL;free(resident_controller_handles);resident_controller_handles=NULL;
    rf_group_mover_memberships_close(&resident_memberships);rf_group_membership_diagnostic[1]=(uint32_t)status;return status;
}

volatile uint32_t rf_door_motion_diagnostic[8]={0x5246444du};
static int door_motion_check(void)
{
    rf_group_pose_slot *slots=NULL;uint32_t i,j,k,frame,step,hash=2166136261u,view_hash=2166136261u;int status=RF_OK;MM_STATISTICS memory={0};
    float dt=.25f;uint32_t render_steps=600;
    FILE *render_flag=fopen("D:\\door-motion.flag","rb");
    if(render_flag) {
        fclose(render_flag);
        if(rf_diagnostic[31]!=5 || !resident_render_geometry.world){status=RF_RANGE;goto done;}
        door_capacity=rf_diagnostic[46];door_mesh.vertices=malloc(door_capacity);
        if(!door_mesh.vertices){status=RF_RANGE;goto done;}
        rf_door_render_diagnostic[3]=resident_render_geometry.allocated_bytes;
        rf_door_render_diagnostic[4]=door_capacity;rf_door_render_diagnostic[5]=2166136261u;
        dt=1.0f/60.0f;
        render_flag=fopen("D:\\door-motion-frames.txt","rb");
        if(render_flag){int parsed=fscanf(render_flag,"%u",&render_steps);fclose(render_flag);if(parsed!=1 || !render_steps || render_steps>600){status=RF_RANGE;goto done;}}
        for(i=0;i<resident_movers.count;++i)if(i>=resident_render_geometry.movers.count ||
            resident_movers.uids[i]!=resident_render_geometry.movers.items[i].uid){status=RF_FORMAT;goto done;}
    }
    slots=calloc(resident_movers.count?resident_movers.count:1,sizeof(*slots));if(!slots) {status=RF_RANGE;goto done;}
    for(i=0;i<resident_movers.count;i++) {slots[i].handle=resident_movers.views[i].object_id;slots[i].pose=resident_movers.poses+i;}
    rf_door_motion_diagnostic[7]=resident_movers.count*sizeof(*slots);
    for(step=0;step<(door_mesh.vertices?render_steps:resident_group_runtime.count);++step) {
    for(i=door_mesh.vertices?0:step;i<(door_mesh.vertices?resident_group_runtime.count:step+1);i++) {
        rf_group_runtime_entry *entry=resident_group_runtime.items+i;const rf_level_owned_group *g=entry->source;
        const rf_group_mover_membership *m=resident_memberships.items+i;rf_group_controller_view binding={0};rf_group_attached_pose *mover;uint32_t index;
        if(entry->kind!=RF_GROUP_RUNTIME_TRANSLATION)continue;
        /* Diagnostic fixture: real two-key doors, unobstructed gates, absent events. */
        if(g->record.key_count!=2 || g->record.ids_count[0] || m->count!=1) {status=RF_RANGE;goto done;}
        for(j=0;j<2;j++)for(k=0;k<3;k++)if(g->keys[j].links[k]!=UINT32_MAX) {status=RF_RANGE;goto done;}
        index=m->handles[0]&0xffffu;if(index>=resident_movers.count || slots[index].handle!=m->handles[0]) {status=RF_RANGE;goto done;}
        mover=slots[index].pose;binding.runtime=&entry->translation;binding.first_key=g->keys;binding.mover_handles=m->handles;binding.mover_count=m->count;
        if(!door_mesh.vertices || !step) {
            status=rf_group_motion_activate(&entry->translation.motion,2);if(status)goto done;
            rf_door_motion_diagnostic[2]++;
        }
        for(frame=door_mesh.vertices?step:0;frame<(door_mesh.vertices?step+1:40u);frame++) {
            rf_group_translation_frame tick;uint32_t sounds=0;const void *data[4]={&status,&entry->translation,&entry->pose,mover};uint32_t sizes[4]={4,76,236,236};
            status=rf_group_translation_tick_begin(&entry->translation,g->keys,2,dt,(int32_t)(door_mesh.vertices?frame*1000/60:frame*250),&tick);if(status)goto done;
            if(tick.stage==RF_GROUP_TICK_GATES) {status=rf_group_translation_tick_move(&entry->translation,&tick);if(status)goto done;}
            if(tick.stage==RF_GROUP_TICK_ARRIVAL) {status=rf_group_translation_tick_finish(&entry->translation,&tick,2,&sounds);if(status)goto done;}
            entry->pose.flags=entry->translation.object_flags;memcpy(entry->pose.pending,entry->translation.pending,12);memcpy(entry->pose.velocity,entry->translation.velocity,12);
            status=rf_group_translation_bind_pose(mover,m->handles[0],&binding,1,dt,0);if(status)goto done;
            status=rf_group_commit_positions(&entry->translation.motion.flags,&entry->pose,&binding,slots,resident_movers.count);if(status)goto done;
            memcpy(entry->translation.position,entry->pose.position,12);entry->translation.object_flags=entry->pose.flags;
            status=rf_geometry_collision_movers_sync(&resident_movers);if(status)goto done;
            for(j=0;j<4;j++)for(k=0;k<sizes[j];k++)hash=(hash^((const unsigned char *)data[j])[k])*16777619u;
            rf_door_motion_diagnostic[3]++;
        }
    }
    if(door_mesh.vertices){status=door_render_frame();if(status)goto done;}
    }
    for(i=0;i<resident_movers.count;i++)for(j=0;j<120;j++)view_hash=(view_hash^((const unsigned char *)resident_movers.views[i].minimum)[j])*16777619u;
    rf_door_motion_diagnostic[4]=hash;rf_door_motion_diagnostic[5]=view_hash;
    rf_group_runtime_diagnostic[7]=group_runtime_hash();memory.Length=sizeof(memory);
    if(NT_SUCCESS(MmQueryStatistics(&memory)))rf_door_motion_diagnostic[6]=memory.AvailablePages;
 done:
    if(door_mesh.vertices) {
        rf_door_render_diagnostic[8]=(uint32_t)door_mesh.vertices;rf_door_render_diagnostic[9]=door_mesh.bytes;
        rf_door_render_diagnostic[1]=status?(uint32_t)status:1;
        memory.Length=sizeof(memory);if(NT_SUCCESS(MmQueryStatistics(&memory)))rf_diagnostic[47]=memory.AvailablePages;
    }
    free(slots);rf_door_motion_diagnostic[1]=status?(uint32_t)status:1;return status;
}
static int mover_check(const rf_level *level)
{
    rf_geometry_movers source={0};uint32_t *ids=NULL,i,j,k,v,group,hash=2166136261u;
    int status;MM_STATISTICS memory={0};
    status=group_storage_open(level);if(status)goto done;
    status=rf_geometry_movers_open(level,1024u*1024u,&source);
    if(status==RF_NOT_FOUND) {source.allocated_bytes=sizeof(source);status=RF_OK;}
    if(status)goto done;
    ids=(uint32_t *)malloc(source.count?source.count*4:4);if(!ids) {status=RF_RANGE;goto done;}
    for(i=0;i<source.count;i++)ids[i]=UINT32_MAX; /* Replaced by registration before queries or attachments. */
    status=rf_geometry_collision_movers_open(&source,ids,1024u*1024u,&resident_movers);
    free(ids);ids=NULL;rf_geometry_movers_close(&source);if(status)goto done;
    status=membership_open();if(status)goto done;
    rf_mover_diagnostic[2]=resident_movers.count;rf_mover_diagnostic[3]=resident_movers.allocated_bytes;
    rf_mover_diagnostic[4]=resident_movers.peak_bytes;
    for(group=0;group<2;group++)for(i=0;i<(group?resident_movers.count:resident_collision.room_count);i++) {
        const rf_collision_face *faces=group?resident_movers.owned[i].faces:resident_collision.rooms[i].tree.faces;
        uint32_t count=group?resident_movers.owned[i].count:(resident_collision.rooms[i].tree.face_count?1:0);
        for(j=0;j<count;j++) {
            const rf_collision_face *face=faces+j;float start[3],end[3];uint32_t visible;
            rf_collision_ray_hit local={0},global;
            struct {int32_t status;uint32_t matched;rf_collision_solid_hit hit;} out;
            const unsigned char *raw=(const unsigned char *)&out;
            for(v=0;v<face->count;v++)for(k=0;k<3;k++)local.point[k]+=face->vertices[v][k];
            for(k=0;k<3;k++) {local.point[k]/=face->count;local.normal[k]=face->plane[k];}
            global=local;
            if(group) {status=rf_collision_contact_world(&local,resident_movers.views[i].output_origin,resident_movers.views[i].output_matrix,&global);if(status)goto done;}
            for(k=0;k<3;k++) {start[k]=global.point[k]+global.normal[k]+.0037f*(k+1);end[k]=global.point[k]-global.normal[k]+.005f*(k+1);}
            memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_ray(&resident_collision,&resident_movers,start,end,0x26,&out.hit,&out.matched);
            rf_mover_diagnostic[5]++;
            if(out.status) {status=out.status;goto done;}
            status=rf_geometry_collision_ray(&resident_collision,&resident_movers,start,end,0x26,NULL,&visible);
            if(status || visible!=out.matched) {status=RF_FORMAT;goto done;}
            rf_mover_diagnostic[6]+=out.matched;
            if(out.matched)rf_mover_diagnostic[out.hit.solid_index==UINT32_MAX?8:7]++;
            for(k=0;k<sizeof(out);k++)hash=(hash^raw[k])*16777619u;
        }
    }
    rf_mover_diagnostic[9]=hash;memory.Length=sizeof(memory);
    if(NT_SUCCESS(MmQueryStatistics(&memory)))rf_mover_diagnostic[10]=memory.AvailablePages;
 done:
    free(ids);rf_geometry_movers_close(&source);rf_mover_diagnostic[1]=status?(uint32_t)status:1;
    if(status)rf_mover_diagnostic[11]++;
    return status;
}
static int sweep_check(void)
{
    uint32_t i,j,k,q,hash=2166136261u;MM_STATISTICS memory={0};
    for(i=0;i<resident_collision.room_count;i++)for(q=0;q<3;q++) {
        const rf_collision_tree *tree=&resident_collision.rooms[i].tree;const rf_collision_face *face;
        float start[3]={0},delta[3];struct {int32_t status;uint32_t matched;rf_geometry_world_sweep_hit hit;} out;
        const unsigned char *bytes=(const unsigned char*)&out;
        if(!tree->face_count)continue;face=tree->faces;
        for(j=0;j<face->count;j++)for(k=0;k<3;k++)start[k]+=face->vertices[j][k];
        for(k=0;k<3;k++) {
            float anchor=q==0?start[k]/face->count:q==1?face->vertices[0][k]:(face->vertices[0][k]+face->vertices[1%face->count][k])*.5f;
            start[k]=anchor+face->plane[k]+.0037f*(k+1);delta[k]=-2*face->plane[k]+.0013f*(k+1);
        }
        memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_world_sweep(&resident_collision,0x460,start,delta,.25f*(q+1),1,&out.hit,&out.matched);
        rf_sweep_diagnostic[2]++;
        if(out.status)rf_sweep_diagnostic[5]++;
        else {rf_sweep_diagnostic[3]+=out.matched;if(out.matched)rf_sweep_diagnostic[4]+=out.hit.edge!=0;}
        for(j=0;j<sizeof(out);j++)hash=(hash^bytes[j])*16777619u;
    }
    rf_sweep_diagnostic[6]=hash;memory.Length=sizeof(memory);
    if(NT_SUCCESS(MmQueryStatistics(&memory)))rf_sweep_diagnostic[7]=memory.AvailablePages;
    rf_sweep_diagnostic[1]=rf_sweep_diagnostic[5]?(uint32_t)RF_FORMAT:1;
    return rf_sweep_diagnostic[5]?RF_FORMAT:RF_OK;
}
static int collision_check(const rf_level *level)
{
    uint32_t i,j,k,hash=2166136261u;MM_STATISTICS memory={0};int status;
    status=rf_geometry_collision_world_open(&resident_geometry,8u*1024u*1024u,&resident_collision);
    if(status) {rf_collision_diagnostic[1]=(uint32_t)status;return status;}
    rf_collision_diagnostic[2]=resident_collision.allocated_bytes;rf_collision_diagnostic[3]=resident_collision.peak_bytes;
    for(i=0;i<resident_collision.room_count;i++) {
        const rf_collision_tree *tree=&resident_collision.rooms[i].tree;const rf_collision_face *face;
        float start[3]={0},delta[3];struct {int32_t status;uint32_t matched;rf_geometry_world_hit hit;} out;
        const unsigned char *bytes=(const unsigned char*)&out;
        if(!tree->face_count)continue;face=tree->faces;
        for(j=0;j<face->count;j++)for(k=0;k<3;k++)start[k]+=face->vertices[j][k];
        for(k=0;k<3;k++) {start[k]=start[k]/face->count+face->plane[k]+.0037f*(k+1);delta[k]=-2*face->plane[k]+.0013f*(k+1);}
        memset(&out,0xa5,sizeof(out));out.status=rf_geometry_collision_world_ray(&resident_collision,0x460,start,delta,1,&out.hit,&out.matched);
        rf_collision_diagnostic[4]++;if(out.status)rf_collision_diagnostic[6]++;else rf_collision_diagnostic[5]+=out.matched;
        for(j=0;j<sizeof(out);j++)hash=(hash^bytes[j])*16777619u;
    }
    rf_collision_diagnostic[7]=hash;memory.Length=sizeof(memory);
    if(NT_SUCCESS(MmQueryStatistics(&memory)))rf_collision_diagnostic[8]=memory.AvailablePages;
    rf_collision_diagnostic[1]=rf_collision_diagnostic[6]?(uint32_t)RF_FORMAT:1;
    if(rf_collision_diagnostic[6])return RF_FORMAT;
    status=sweep_check();return status?status:mover_check(level);
}
static rf_materials resident_materials;
static rf_lightmaps resident_lightmaps;
static int door_render_frame(void)
{
    uint32_t i,hash=2166136261u,pair[2];int status;
    status=rf_scene_world_update(&resident_render_geometry,resident_movers.poses,resident_movers.count,&door_mesh,door_capacity);
    if(status)return status;
    for(i=0;i<door_mesh.bytes;++i)hash=(hash^((const unsigned char *)door_mesh.vertices)[i])*16777619u;
    pair[0]=door_mesh.count;pair[1]=hash;
    for(i=0;i<sizeof(pair);++i)rf_door_render_diagnostic[5]=(rf_door_render_diagnostic[5]^((const unsigned char *)pair)[i])*16777619u;
    rf_door_render_diagnostic[6]=door_mesh.count;rf_door_render_diagnostic[7]=hash;
    rf_diagnostic[57]=door_mesh.count;
    status=rf_xbox_scene_stream_frame(&door_mesh,&resident_materials,&resident_lightmaps,door_mesh.count,&rf_diagnostic[32],&rf_diagnostic[44]);
    if(!status)rf_door_render_diagnostic[2]++;
    return status;
}
static int scene_frame(void *context,uint32_t frame,const rf_preview_mesh *mesh,
    const rf_materials *materials,uint32_t world)
{
    (void)context;
    if(scene_simulation_frames!=frame)return RF_FORMAT;
    ++scene_simulation_frames;
    if(frame==(rf_scene_actor_live_enabled?663u:63u) && actor_body_preview) {
        memcpy(rf_actor_world_diagnostic,rf_scene_actor_initial_world,sizeof(rf_actor_world_diagnostic));
        memcpy(rf_actor_fall_diagnostic,rf_scene_actor_initial_fall,sizeof(rf_actor_fall_diagnostic));
    } else if(frame==63 && !actor_body_preview) {
        int status=rf_scene_actor_world_check(&resident_collision,rf_actor_world_diagnostic);if(status)return status;
        status=rf_scene_actor_fall_check(&resident_collision,rf_actor_fall_diagnostic);if(status)return status;
    }
    rf_diagnostic[57]=world;
    if(player_pacing && !rf_frame_clock_present(&rf_player_frame_clock,GetTickCount()))return group_storage_check();
    {int status=actor_follow_preview?rf_xbox_scene_stream_frame_sized(mesh,materials,&resident_lightmaps,world,&rf_diagnostic[32],&rf_diagnostic[44],RF_SCENE_FOLLOW_CAPACITY):rf_xbox_scene_stream_frame(mesh,materials,&resident_lightmaps,world,&rf_diagnostic[32],&rf_diagnostic[44]);return status?status:group_storage_check();}
}
static int scene_preview(rf_level *level,rf_preview_mesh *mesh)
{
    static const char *paths[]={"D:\\maps1.vpp","D:\\maps2.vpp","D:\\maps3.vpp","D:\\maps4.vpp","D:\\maps_en.vpp"};
    rf_vpp maps[5];uint32_t opened=0,world;int status,player_controls=0;FILE *stream_flag;
    player_pacing=0;scene_simulation_frames=0;memset(&rf_player_frame_clock,0,sizeof(rf_player_frame_clock));
    rf_scene_set_profile(NULL);
    stream_flag=fopen("D:\\campaign-spawn.flag","rb");
    if(stream_flag){fclose(stream_flag);status=rf_scene_set_campaign_spawn(level);}
    else status=rf_scene_preview_camera(level,9858);
    if(status)return status;
    actor_body_preview=0;stream_flag=fopen("D:\\actor-body.flag","rb");
    if(stream_flag){fclose(stream_flag);actor_body_preview=1;}
    stream_flag=fopen("D:\\actor-drive.flag","rb");rf_scene_actor_drive(stream_flag!=NULL);
    if(stream_flag){fclose(stream_flag);actor_body_preview=1;}
    stream_flag=fopen("D:\\actor-contact.flag","rb");
    if(stream_flag){fclose(stream_flag);rf_scene_actor_drive(2);actor_body_preview=1;}
    {extern uint32_t rf_scene_actor_route_enabled;
     stream_flag=fopen("D:\\actor-routes.flag","rb");rf_scene_actor_route_enabled=stream_flag!=NULL;
     if(stream_flag){fclose(stream_flag);rf_scene_actor_drive(1);actor_body_preview=1;}}
    {extern uint32_t rf_scene_actor_live_enabled;
     stream_flag=fopen("D:\\actor-live.flag","rb");rf_scene_actor_live_enabled=stream_flag!=NULL;
     if(stream_flag){fclose(stream_flag);rf_scene_actor_drive(1);actor_body_preview=1;}}
    stream_flag=fopen("D:\\actor-follow.flag","rb");actor_follow_preview=stream_flag!=NULL;
    if(stream_flag){fclose(stream_flag);rf_scene_actor_live_enabled=1;rf_scene_actor_drive(1);actor_body_preview=1;}
    stream_flag=fopen("D:\\actor-eye.flag","rb");rf_scene_actor_eye_enabled=stream_flag!=NULL;
    if(stream_flag){fclose(stream_flag);actor_follow_preview=1;rf_scene_actor_live_enabled=1;rf_scene_actor_drive(1);actor_body_preview=1;}
    stream_flag=fopen("D:\\actor-look.flag","rb");rf_scene_actor_look_enabled=stream_flag!=NULL;
    if(stream_flag){fclose(stream_flag);rf_scene_actor_eye_enabled=1;actor_follow_preview=1;rf_scene_actor_live_enabled=1;rf_scene_actor_drive(1);actor_body_preview=1;}
    stream_flag=fopen("D:\\actor-turn.flag","rb");rf_scene_actor_turn_enabled=stream_flag!=NULL;
    if(stream_flag){fclose(stream_flag);rf_scene_actor_look_enabled=1;rf_scene_actor_eye_enabled=1;actor_follow_preview=1;rf_scene_actor_live_enabled=1;rf_scene_actor_drive(1);actor_body_preview=1;}
    stream_flag=fopen("D:\\player-control.flag","rb");
    if(stream_flag) {
        uint32_t limit=0;FILE *frames;fclose(stream_flag);
        frames=fopen("D:\\player-control-frames.txt","rb");
        if(frames){int scanned=fscanf(frames,"%u",&limit);fclose(frames);if(scanned!=1 || limit>60000)return RF_FORMAT;}
        player_replay=fopen("D:\\player-replay.bin","rb");
        memset(rf_player_replay_diagnostic,0,sizeof(rf_player_replay_diagnostic));
        if(player_replay) {
            if(rf_scene_replay_header(player_replay,&limit,&player_replay_size)){player_input_close();return RF_FORMAT;}
            rf_player_replay_diagnostic[0]=1;rf_player_replay_diagnostic[1]=limit;
        } else {status=rf_xbox_input_open();if(status)return status;}
        player_controls=1;player_pacing=!player_replay && limit==0;
        if(player_pacing)rf_scene_set_profile(profile_milliseconds);
        rf_scene_set_input(player_poll_paced,NULL,limit);
        rf_scene_actor_turn_enabled=rf_scene_actor_look_enabled=rf_scene_actor_eye_enabled=1;
        actor_follow_preview=rf_scene_actor_live_enabled=actor_body_preview=1;rf_scene_actor_drive(1);
    }
    if(rf_scene_actor_live_enabled) {
        stream_flag=fopen("D:\\campaign-spawn.flag","rb");
        if(stream_flag){fclose(stream_flag);status=rf_scene_set_campaign_spawn(level);}
        else {rf_scene_set_campaign_spawn(NULL);status=rf_scene_preview_route_camera(level,9858);}
        if(status){if(player_controls){rf_scene_set_input(NULL,NULL,0);player_input_close();}return status;}
    }
    stream_flag=fopen("D:\\door-view.flag","rb");
    if(stream_flag){fclose(stream_flag);status=rf_scene_preview_mover_camera(level,8544,6.0f);if(status){if(player_controls){rf_scene_set_input(NULL,NULL,0);player_input_close();}return status;}}
    stream_flag=fopen("D:\\showcase.flag","rb");rf_scene_showcase_enabled=stream_flag!=NULL;
    if(stream_flag){fclose(stream_flag);status=rf_scene_showcase_camera(level);if(status){if(player_controls){rf_scene_set_input(NULL,NULL,0);player_input_close();}return status;}}
    rf_preview_close(mesh);rf_materials_close(&resident_materials);
    while(!status && opened<5) {status=rf_vpp_open(maps+opened,paths[opened]);if(!status)++opened;}
    if(!status)status=rf_scene_world_open_retained(level,&resident_geometry,maps,opened,mesh,&resident_materials,8*1024*1024,RF_CAMPAIGN_MATERIAL_BUDGET,&resident_render_geometry);
    if(!status && actor_follow_preview)rf_scene_actor_follow(&resident_render_geometry);
    world=mesh->count;
    stream_flag=fopen("D:\\scene-stream.flag","rb");
    if(stream_flag || player_controls) {
        if(stream_flag)fclose(stream_flag);rf_diagnostic[31]=4;rf_diagnostic[56]=9858;
        stream_flag=fopen("D:\\scene-states.flag","rb");
        if(stream_flag || player_controls) {
            if(stream_flag)fclose(stream_flag);rf_diagnostic[31]=5;
            if(!status && actor_body_preview)status=rf_scene_stream_miner_body(level,9858,"D:\\meshes.vpp","D:\\motions.vpp","D:\\tables.vpp",
                maps,opened,mesh,&resident_materials,8*1024*1024,RF_CAMPAIGN_MATERIAL_BUDGET,scene_frame,NULL,&resident_collision,&resident_geometry);
            else if(!status)status=rf_scene_stream_miner_states(level,9858,"D:\\meshes.vpp","D:\\motions.vpp","D:\\tables.vpp",
                maps,opened,mesh,&resident_materials,8*1024*1024,RF_CAMPAIGN_MATERIAL_BUDGET,scene_frame,NULL);
        } else if(!status)status=rf_scene_stream_miner(level,9858,"D:\\meshes.vpp","D:\\motions.vpp","D:\\tables.vpp",
            maps,opened,mesh,&resident_materials,8*1024*1024,RF_CAMPAIGN_MATERIAL_BUDGET,scene_frame,NULL);
        while(opened)rf_vpp_close(maps+--opened);
        if(player_controls){rf_scene_set_input(NULL,NULL,0);player_input_close();}
        return status;
    }
    if(!status)status=rf_scene_preview_miner(level,9858,"D:\\meshes.vpp","D:\\motions.vpp","D:\\tables.vpp",
        maps,opened,mesh,&resident_materials,8*1024*1024,RF_CAMPAIGN_MATERIAL_BUDGET);
    while(opened)rf_vpp_close(maps+--opened);
    if(!status) {
        rf_diagnostic[31]=3;rf_diagnostic[56]=9858;rf_diagnostic[57]=world;
        status=rf_xbox_scene_preview(mesh,&resident_materials,&resident_lightmaps,world,&rf_diagnostic[32],&rf_diagnostic[44]);
    }
    return status;
}
static int model_frame(void *context,uint32_t frame,rf_preview_mesh *mesh)
{
    rf_model_materials *bundle=context;uint32_t i;
    if(rf_diagnostic[37]!=frame)return RF_FORMAT;
    for(i=0;i<mesh->count;++i) {
        uint32_t material=mesh->vertices[i].material;
        if(material>=bundle->count)return RF_FORMAT;
        memcpy(&mesh->vertices[i].material,bundle->items[material].record.bytes+0x10,4);
    }
    return rf_xbox_model_stream_frame(mesh,&bundle->textures,&rf_diagnostic[32],&rf_diagnostic[44]);
}
static int model_preview(void)
{
    static const char *paths[]={"D:\\maps1.vpp","D:\\maps2.vpp","D:\\maps3.vpp","D:\\maps4.vpp","D:\\maps_en.vpp"};
    rf_vpp meshes,archives[5];rf_model_file model;rf_model_materials bundle={0};rf_preview_mesh mesh={0};
    uint32_t opened=0,i;int status;FILE *stream_flag;
    static rf_entity_assets skin_assets;const char *skin_names[64];
    memset(&skin_assets,0,sizeof(skin_assets));
    FILE *skin_file=fopen("D:\\model-skin.txt","rb");
    if(skin_file) {
        char skin[64],compiled[64];size_t size=fread(skin,1,sizeof(skin),skin_file);
        int failed=ferror(skin_file);fclose(skin_file);
        if(failed || size==sizeof(skin))return RF_RANGE;
        while(size && (skin[size-1]=='\r' || skin[size-1]=='\n'))--size;
        if(!size || memchr(skin,0,size))return RF_FORMAT;skin[size]=0;
        status=rf_entity_assets_load("D:\\tables.vpp","miner1",skin,&skin_assets,512*1024);
        if(status)return status;
        status=rf_entity_skeletal_filename(skin_assets.model,compiled);
        if(status || strcmp(compiled,"miner.v3c"))return RF_FORMAT;
        for(i=0;i<skin_assets.texture_count;++i)skin_names[i]=skin_assets.textures[i];
        rf_diagnostic[56]=rf_filename_checksum(skin);rf_diagnostic[57]=skin_assets.texture_count;
    }
    status=rf_vpp_open(&meshes,"D:\\meshes.vpp");if(status)return status;
    status=rf_model_file_open(&model,&meshes,"miner.v3c");
    while(!status && opened<5) {status=rf_vpp_open(archives+opened,paths[opened]);if(!status)++opened;}
    if(!status)status=rf_model_materials_open_skin(&bundle,&model,skin_names,skin_assets.texture_count,archives,opened,4*1024*1024);
    stream_flag=fopen("D:\\model-stream.flag","rb");
    if(stream_flag) {
        rf_animation_placement placement={0};
        fclose(stream_flag);rf_diagnostic[31]=2;
        /* Equivalent inspection framing through a translated quarter-turn
         * entity, exercising model-local view conversion on the Xbox. */
        placement.world_view.camera[0]=2.2f;placement.world_view.camera[1]=8;placement.world_view.camera[2]=16;
        placement.world_view.rotation[2]=1;placement.world_view.rotation[4]=1;placement.world_view.rotation[6]=-1;
        placement.world_view.perspective=placement.world_view.compute_clip=placement.world_view.clipping=1;
        placement.world_view.screen[0]=320;placement.world_view.screen[1]=-240;placement.world_view.screen[2]=320;placement.world_view.screen[3]=240;
        placement.position[1]=8;placement.position[2]=16;
        placement.orientation[2]=-1;placement.orientation[4]=placement.orientation[6]=1;
        placement.clip_projection.scale[0]=320;placement.clip_projection.scale[1]=240;placement.clip_projection.clamp=1;
        if(!status)status=rf_animation_stream_placed("D:\\meshes.vpp","D:\\motions.vpp",1024*1024,&placement,model_frame,&bundle);
    } else {
    if(!status)status=rf_animation_preview("D:\\meshes.vpp","D:\\motions.vpp",0,&mesh,1024*1024);
    if(!status)for(i=0;i<mesh.count;++i) {
        uint32_t material=mesh.vertices[i].material;
        if(material>=bundle.count) {status=RF_FORMAT;break;}
        memcpy(&mesh.vertices[i].material,bundle.items[material].record.bytes+0x10,4);
    }
    if(!status) {rf_diagnostic[31]=1;status=rf_xbox_model_preview(&mesh,&bundle.textures,&rf_diagnostic[32],&rf_diagnostic[44]);}
    }
    rf_preview_close(&mesh);rf_model_materials_close(&bundle);
    while(opened)rf_vpp_close(archives+--opened);
    rf_vpp_close(&meshes);return status;
}
static int load_materials(void)
{
    static const char *paths[] = {"D:\\maps1.vpp", "D:\\maps2.vpp", "D:\\maps3.vpp", "D:\\maps4.vpp", "D:\\maps_en.vpp"};
    rf_vpp archives[5];
    uint32_t i, opened = 0, checksum = 0;
    int result = RF_OK;
    for (i = 0; i < 5; ++i) {
        result = rf_vpp_open(archives + i, paths[i]);
        if (result) break;
        ++opened;
    }
    if (!result) result = rf_materials_open(&resident_materials, &resident_geometry, archives, 5, RF_CAMPAIGN_MATERIAL_BUDGET);
    while (opened) rf_vpp_close(archives + --opened);
    if (result) return result;
    for (i = 0; i < resident_materials.count; ++i) {
        const rf_image *image = &resident_materials.items[i].image;
        uint32_t j, hash = 2166136261u;
        if (!image->rgba) continue;
        for (j = 0; j < image->bytes; ++j) hash = (hash ^ rf_image_pixel(image,(j/4)%image->width,(j/4)/image->width)[j%4]) * 16777619u;
        checksum ^= hash;
    }
    rf_diagnostic[38] = resident_materials.loaded;
    rf_diagnostic[39] = resident_materials.allocated_bytes;
    rf_diagnostic[40] = checksum; rf_diagnostic[41] = resident_materials.missing;
    return RF_OK;
}

void WinMainCRTStartup(void);
void __attribute__((no_stack_protector)) rf_diagnostic_start(void)
{
    rf_diagnostic[2] = 10;
    WinMainCRTStartup();
}

volatile uint32_t rf_explosion_loading_diagnostic[6]; /* status, recipes, hash, peak, vclip hash, resolved mask */
static int explosion_loading_check(void)
{
    static const char *names[]={"generic","space","geomod","shoulder mounted geomod","rocket hit","flamethrower-alt","FGatE","FGatE_lilspark","FGatE_bigspark"};
    rf_vclip_definition vclip;rf_explosion_definition definition;rf_vpp tables;
    unsigned i;uint32_t hash=2166136261u;int status;
    rf_explosion_loading_diagnostic[0]=2;
    for(i=0;i<9;++i) {
        status=rf_vpp_open(&tables,"D:\\tables.vpp");if(status)goto fail;
        status=rf_explosion_definition_load(&tables,names[i],65536,&definition);rf_vpp_close(&tables);
        if(status)goto fail;
        hash=logic_hash_part(hash,&definition,sizeof(definition));
        if(definition.peak_bytes>rf_explosion_loading_diagnostic[3])rf_explosion_loading_diagnostic[3]=definition.peak_bytes;
        ++rf_explosion_loading_diagnostic[1];
    }
    status=rf_vpp_open(&tables,"D:\\tables.vpp");if(status)goto fail;
    status=rf_vclip_definition_load(&tables,"charge_explode",65536,&vclip);
    if(!status)status=rf_explosion_definition_load(&tables,vclip.explosion,65536,&definition);
    rf_vpp_close(&tables);if(status)goto fail;
    rf_explosion_loading_diagnostic[2]=hash;
    rf_explosion_loading_diagnostic[4]=logic_hash_part(2166136261u,&vclip,sizeof(vclip));
    rf_explosion_loading_diagnostic[5]=definition.resolved;
    rf_explosion_loading_diagnostic[0]=1;return RF_OK;
fail:
    rf_explosion_loading_diagnostic[0]=(uint32_t)status;return status;
}
/* Replay-only resource lifetime check; no host input or rendering. */
volatile uint32_t rf_particle_resource_diagnostic[7]; /* status, loads, hash, peak bytes, pages before/min/after */
static void particle_resource_check(void)
{
    FILE *flag=fopen("D:\\player-replay.bin","rb");rf_vpp maps;int status;unsigned round,i;
    MM_STATISTICS memory={0};uint32_t hash=2166136261u;
    if(!flag)return;fclose(flag);rf_particle_resource_diagnostic[0]=2;
    status=explosion_loading_check();if(status){rf_particle_resource_diagnostic[0]=(uint32_t)status;return;}
    status=rf_vpp_open(&maps,"D:\\maps2.vpp");if(status){rf_particle_resource_diagnostic[0]=(uint32_t)status;return;}
    memory.Length=sizeof(memory);
    for(round=0;round<3;++round) {
        for(i=0;i<3;++i) {
            rf_particle_definition definition={0};rf_particle_bitmap bitmap={0};uint32_t x,y;
            strcpy(definition.bitmap,i?"boom01.vbm":"LightCorona01.tga");
            status=rf_particle_bitmap_open(&bitmap,&definition,&maps,1,i==2?15:0,65536);
            if(status)goto done;
            if(round) {
                if(bitmap.resident_bytes>rf_particle_resource_diagnostic[3])rf_particle_resource_diagnostic[3]=bitmap.resident_bytes;
                if(NT_SUCCESS(MmQueryStatistics(&memory)) && memory.AvailablePages<rf_particle_resource_diagnostic[5])rf_particle_resource_diagnostic[5]=memory.AvailablePages;
                for(y=0;y<bitmap.image.height;++y)for(x=0;x<bitmap.image.width;++x) {
                    unsigned k;unsigned char *pixel=rf_image_pixel(&bitmap.image,x,y);
                    for(k=0;k<4;++k)hash=(hash^pixel[k])*16777619u;
                }
                ++rf_particle_resource_diagnostic[1];
            }
            rf_particle_bitmap_close(&bitmap);
        }
        if(!round && NT_SUCCESS(MmQueryStatistics(&memory)))rf_particle_resource_diagnostic[4]=rf_particle_resource_diagnostic[5]=memory.AvailablePages;
    }
    if(NT_SUCCESS(MmQueryStatistics(&memory)))rf_particle_resource_diagnostic[6]=memory.AvailablePages;
    rf_particle_resource_diagnostic[2]=hash;
done:
    rf_vpp_close(&maps);rf_particle_resource_diagnostic[0]=status?(uint32_t)status:1;
}
int main(void)
{
    rf_vpp archive;
    MM_STATISTICS memory = {0};
    int live_mines_door_fixture=1;
    int result;
    rf_diagnostic[2] = 1;
    XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
    memory.Length = sizeof(memory);
    debugPrint("Red Faction reconstruction - archive diagnostic\n");
    debugPrint("Not a playable game. Stock 64 MiB target.\n");
    if (NT_SUCCESS(MmQueryStatistics(&memory))) {
        rf_diagnostic[3] = memory.TotalPhysicalPages;
        rf_diagnostic[4] = memory.AvailablePages;
        debugPrint("Physical pages: %lu; available: %lu\n", memory.TotalPhysicalPages, memory.AvailablePages);
    }
    OutputDebugStringA("RF_DIAGNOSTIC_BOOT\n");
    particle_resource_check();
    result = rf_vpp_open(&archive, "D:\\tables.vpp");
    if (result == RF_OK) {
        rf_diagnostic[5] = archive.count;
        rf_diagnostic[6] = archive.length;
        rf_diagnostic[7] = rf_filename_checksum("tables.vpp");
        debugPrint("tables.vpp: %u files, %u bytes\n", archive.count, archive.length);
        OutputDebugStringA("RF_VPP_VALIDATED\n");
        rf_vpp_close(&archive);
        rf_diagnostic[2] = 2;
    } else {
        debugPrint("tables.vpp unavailable or invalid: %d\n", result);
        OutputDebugStringA("RF_VPP_FAILED\n");
        rf_diagnostic[2] = 0x80000000u | (uint32_t)(-result);
    }
    if (result == RF_OK) {
        uint32_t animation[8], i;
        result=rf_animation_check("D:\\meshes.vpp","D:\\motions.vpp",animation);
        for (i=0;i<8;++i) rf_diagnostic[48+i]=animation[i];
        if (result!=RF_OK) rf_diagnostic[2]=0x80000200u | (uint32_t)(-result);
    }
    if (result == RF_OK) {
        char selection[128]={0},archive_path[80];int selected=0;
        FILE *selection_file=fopen("D:\\campaign-level.bin","rb");
        if(selection_file) {
            selected=1;
            if(fread(selection,1,sizeof(selection),selection_file)!=sizeof(selection) ||
               fgetc(selection_file)!=EOF || !memchr(selection,0,64) || !memchr(selection+64,0,64) ||
               !selection[64] || (strcmp(selection,"levels1.vpp") && strcmp(selection,"levels2.vpp") &&
               strcmp(selection,"levels3.vpp") && strcmp(selection,"levelsm.vpp")))result=RF_FORMAT;
            fclose(selection_file);
        }
        if(result==RF_OK) {
            snprintf(archive_path,sizeof(archive_path),"D:\\%s",selected?selection:"levels1.vpp");
            result = rf_vpp_open(&archive, archive_path);
        }
        if (result == RF_OK) {
            rf_level level;
            FILE *climb_flag=fopen("D:\\campaign-climb.flag","rb");
            int staged_climb=climb_flag!=NULL;
            live_mines_door_fixture=!staged_climb && !selected;
            int climb_mode=climb_flag && fgetc(climb_flag)=='2'?2:1;
            if(climb_flag)fclose(climb_flag);
            result = selected && staged_climb?RF_FORMAT:
                rf_level_open(&level, &archive, selected?selection+64:staged_climb?"L1S2.rfl":"L1S1.rfl");
            if(result==RF_OK && staged_climb) {
                result=rf_scene_stage_climb(&level,(uint32_t)climb_mode);
            }
            if (result == RF_OK) {
                const rf_level_section *geometry = rf_level_find(&level, 0x100);
                const rf_level_section *lightmaps = rf_level_find(&level, 0x1200);
                uint32_t i;
                rf_diagnostic[8] = level.version;
                rf_diagnostic[9] = level.section_count;
                rf_diagnostic[10] = level.entry.size;
                rf_diagnostic[11] = geometry ? geometry->size : 0;
                rf_diagnostic[12] = lightmaps ? lightmaps->size : 0;
                for (i = 0; i < 3; ++i) {
                    uint32_t bits;
                    memcpy(&bits, &level.player_position[i], sizeof(bits));
                    rf_diagnostic[13 + i] = bits;
                }
                debugPrint("%s: %u sections, %u bytes\n", level.name, level.section_count, level.entry.size);
                OutputDebugStringA("RF_LEVEL_DIRECTORY_VALIDATED\n");
                result = rf_geometry_open(&resident_geometry, &level, 8u * 1024u * 1024u);
                if (result == RF_OK) {
                    float vertex[3];
                    rf_diagnostic[16] = resident_geometry.textures;
                    rf_diagnostic[17] = resident_geometry.rooms;
                    rf_diagnostic[18] = resident_geometry.vertices;
                    rf_diagnostic[19] = resident_geometry.faces;
                    rf_diagnostic[20] = resident_geometry.corners;
                    rf_diagnostic[21] = resident_geometry.mappings;
                    rf_diagnostic[22] = resident_geometry.allocated_bytes;
                    if (NT_SUCCESS(MmQueryStatistics(&memory))) rf_diagnostic[23] = memory.AvailablePages;
                    rf_geometry_vertex(&resident_geometry, 0, vertex);
                    for (i = 0; i < 3; ++i) { uint32_t bits; memcpy(&bits, &vertex[i], 4); rf_diagnostic[24 + i] = bits; }
                    rf_geometry_vertex(&resident_geometry, resident_geometry.vertices - 1, vertex);
                    for (i = 0; i < 3; ++i) { uint32_t bits; memcpy(&bits, &vertex[i], 4); rf_diagnostic[27 + i] = bits; }
                    rf_diagnostic[30] = resident_geometry.bytes - resident_geometry.tail_offset - 4;
                    debugPrint("Geometry: %u vertices, %u faces, %u bytes\n", resident_geometry.vertices, resident_geometry.faces, resident_geometry.allocated_bytes);
                    {
                        rf_preview_mesh mesh;
                        result = collision_check(&level);
                        if(result == RF_OK) result = rf_lightmaps_open(&resident_lightmaps, &level, RF_CAMPAIGN_LIGHTMAP_BUDGET);
                        if (result == RF_OK) {
                            uint32_t mapping, image;
                            for (mapping = 0; mapping < resident_geometry.mappings && result == RF_OK; ++mapping)
                                result = rf_geometry_lightmap(&resident_geometry, mapping, resident_lightmaps.count, &image);
                            rf_diagnostic[43] = resident_lightmaps.count;
                        }
                        if (result == RF_OK) result = load_materials();
                        if (NT_SUCCESS(MmQueryStatistics(&memory))) rf_diagnostic[42] = memory.AvailablePages;
                        if (result == RF_OK) result = rf_preview_build(&mesh, &resident_geometry, &level, 8u*1024u*1024u);
                        if (result == RF_OK) {
                            FILE *scene_flag=fopen("D:\\scene-preview.flag","rb");
                            if(scene_flag) {fclose(scene_flag);result=scene_preview(&level,&mesh);}
                            else {
                                FILE *model_flag=fopen("D:\\model-preview.flag","rb");
                                if(model_flag) {fclose(model_flag);result=model_preview();}
                                else result = rf_xbox_preview(&mesh, &resident_materials, &resident_lightmaps, &rf_diagnostic[32], &rf_diagnostic[44]);
                            }
                            rf_preview_close(&mesh);
                            if (NT_SUCCESS(MmQueryStatistics(&memory))) rf_diagnostic[47] = memory.AvailablePages;
                        }
                    }
                }
            }
            rf_vpp_close(&archive);
        }
        if(result==RF_OK)result=group_storage_check(); /* Level archive is closed; owned data remains resident. */
        if(result==RF_OK && live_mines_door_fixture)result=door_motion_check(); /* Two-key L1S1 fixture, not campaign simulation. */
        rf_diagnostic[2] = result == RF_OK ? 5u : 0x80000100u | (uint32_t)(-result);
    }
    for (;;) Sleep(1000);
    return 0;
}
