#ifndef RF_SCENE_MOVER_CHECKPOINT_INC
#define RF_SCENE_MOVER_CHECKPOINT_INC
#include "rf/mover_checkpoint.h"
#include "rf/geometry.h"
#include <stdlib.h>
#include <string.h>
/* Owned candidate state; source keys, member-handle arrays and collision
 * geometry remain borrowed from the unchanged live level until commit/close. */
typedef struct scene_mover_checkpoint_stage {
    rf_group_runtime_entry *entries;rf_group_controller_view *bindings;
    rf_group_attached_pose *poses;rf_collision_solid_view *views;
    rf_group_pose_slot *slots;
    uint32_t *member_handles,*controller_handles,*controller_uids,*controller_key_counts;
    const rf_group_runtime_entry *original_entries;
    const rf_group_attached_pose *original_poses;
    const rf_collision_solid_view *original_views;
    const rf_group_registered_controller *original_controllers;
    const rf_object_registry *original_registry;
    uint32_t controller_count,mover_count,allocated_bytes;
} scene_mover_checkpoint_stage;
static void scene_mover_checkpoint_close(scene_mover_checkpoint_stage *s)
{
    if(!s)return;free(s->entries);free(s->bindings);free(s->poses);free(s->views);free(s->slots);
    free(s->member_handles);free(s->controller_handles);free(s->controller_uids);free(s->controller_key_counts);memset(s,0,sizeof(*s));
}
/* Pure preparation: output must be empty, every live controller needs one row.
 * Call at a settled boundary; records with pending != position are rejected.
 * This service validates UID/registration/attachment identity and reconstructs
 * geometry, not world clearance, authored flag policy, queued effects or audio.
 * General-object/actor attachments require a separate owner and are rejected.
 * Failure leaves output and all live state unchanged. No registry mutations. */
static inline int scene_mover_checkpoint_prepare(const rf_group_runtime_collection *runtime,
    const rf_group_registration *registration,const rf_group_controller_view *bindings,
    const rf_geometry_collision_movers *movers,const rf_mover_checkpoint_record *rows,
    uint32_t count,int32_t now,uint32_t budget,scene_mover_checkpoint_stage *out)
{
    scene_mover_checkpoint_stage s={0};rf_geometry_collision_movers geometry;uint64_t bytes;
    uint32_t i,j,k,active=0,members=0,at=0;int status=RF_FORMAT;
    if(!runtime||!registration||!movers||!out||(count&&!rows)||count>RF_MOVER_CHECKPOINT_MAX_COUNT||now<0||now>RF_TIMER_PERIOD||
       runtime->count>RF_OBJECT_CAPACITY||movers->count>RF_OBJECT_CAPACITY||
       (runtime->count&&(!runtime->items||!bindings))||
       (movers->count&&(!movers->poses||!movers->views))||
       !registration->registry||(registration->count&&!registration->controllers)||
       out->entries||out->bindings||out->poses||out->views||out->slots||out->member_handles||out->controller_handles||out->controller_uids||out->controller_key_counts||out->controller_count||out->mover_count||out->allocated_bytes)return RF_RANGE;
    for(i=0;i<count;i++)if(i&&rows[i-1].uid>=rows[i].uid)return RF_FORMAT;
    for(i=0;i<runtime->count;i++)active+=runtime->items[i].kind!=RF_GROUP_RUNTIME_EMPTY;
    if(count!=active||registration->count!=active)return RF_FORMAT;
    for(i=0;i<runtime->count;i++){
        if(bindings[i].mover_count>RF_OBJECT_CAPACITY||(bindings[i].mover_count&&!bindings[i].mover_handles))return RF_RANGE;
        members+=bindings[i].mover_count;
    }
    bytes=sizeof(s)+(uint64_t)runtime->count*(sizeof(*s.entries)+sizeof(*s.bindings)+12)+
        (uint64_t)movers->count*(sizeof(*s.poses)+sizeof(*s.views))+RF_OBJECT_CAPACITY*sizeof(*s.slots)+(uint64_t)members*4;
    if(bytes>budget||bytes>UINT32_MAX)return RF_RANGE;
    s.controller_count=runtime->count;s.mover_count=movers->count;s.allocated_bytes=(uint32_t)bytes;
    if(runtime->count){s.entries=malloc(runtime->count*sizeof(*s.entries));s.bindings=malloc(runtime->count*sizeof(*s.bindings));s.controller_handles=calloc(runtime->count,4);s.controller_uids=calloc(runtime->count,4);s.controller_key_counts=calloc(runtime->count,4);}
    if(members)s.member_handles=malloc(members*4);
    if(movers->count){s.poses=malloc(movers->count*sizeof(*s.poses));s.views=malloc(movers->count*sizeof(*s.views));}
    s.slots=calloc(RF_OBJECT_CAPACITY,sizeof(*s.slots));
    if((runtime->count&&(!s.entries||!s.bindings||!s.controller_handles||!s.controller_uids||!s.controller_key_counts))||(members&&!s.member_handles)||(movers->count&&(!s.poses||!s.views))||!s.slots){status=RF_RANGE;goto fail;}
    s.original_entries=runtime->items;s.original_poses=movers->poses;s.original_views=movers->views;
    s.original_controllers=registration->controllers;s.original_registry=registration->registry;
    if(runtime->count){memcpy(s.entries,runtime->items,runtime->count*sizeof(*s.entries));memcpy(s.bindings,bindings,runtime->count*sizeof(*s.bindings));}
    if(movers->count){memcpy(s.poses,movers->poses,movers->count*sizeof(*s.poses));memcpy(s.views,movers->views,movers->count*sizeof(*s.views));}
    for(i=0;i<movers->count;i++){
        uint32_t h=movers->views[i].object_id,slot=h&0xffffu;const rf_group_registered_mover *registered;
        if(h==UINT32_MAX||slot>=RF_OBJECT_CAPACITY||s.slots[slot].pose){status=RF_FORMAT;goto fail;}
        registered=rf_object_registry_lookup(registration->registry,h);
        if(!registered||registered->object_kind!=9||registered->handle!=h||registered->pose!=movers->poses+i){status=RF_FORMAT;goto fail;}
        s.slots[slot].handle=h;s.slots[slot].pose=s.poses+i;
    }
    for(i=0;i<runtime->count;i++){
        rf_group_runtime_entry *entry=s.entries+i;const rf_group_runtime_entry *live=runtime->items+i;uint32_t found=0;
        s.bindings[i].runtime=&entry->translation;
        if(bindings[i].mover_count){memcpy(s.member_handles+at,bindings[i].mover_handles,bindings[i].mover_count*4);s.bindings[i].mover_handles=s.member_handles+at;at+=bindings[i].mover_count;}
        if(entry->kind==RF_GROUP_RUNTIME_EMPTY){
            if(bindings[i].mover_count||bindings[i].general_count){status=RF_FORMAT;goto fail;}continue;
        }
        if(!entry->source||!entry->source->keys||!entry->source->record.key_count||
           bindings[i].runtime!=&live->translation||bindings[i].first_key!=entry->source->keys||bindings[i].general_count||
           (bindings[i].mover_count&&!bindings[i].mover_handles)||
           (entry->kind==RF_GROUP_RUNTIME_ROTATION_PENDING&&bindings[i].rotation_sign!=1&&bindings[i].rotation_sign!=-1)){status=RF_FORMAT;goto fail;}
        for(j=0;j<registration->count;j++)if(registration->controllers[j].runtime==live){
            const rf_group_registered_controller *c=registration->controllers+j;
            if(c->object_kind!=8||rf_object_registry_lookup(registration->registry,c->handle)!=c){status=RF_FORMAT;goto fail;}
            s.controller_handles[i]=c->handle;++found;
        }
        if(found!=1){status=RF_FORMAT;goto fail;}
        s.controller_uids[i]=entry->source->keys[0].uid;
        s.controller_key_counts[i]=entry->source->record.key_count;
        for(j=0;j<count;j++)if(rows[j].uid==entry->source->keys[0].uid)break;
        if(j==count||rows[j].mode!=live->translation.motion.mode||memcmp(rows[j].position,rows[j].pending,12)){status=RF_FORMAT;goto fail;}
        /* Unique runtime identity prevents two controllers consuming one row. */
        for(k=0;k<i;k++)if(s.entries[k].kind!=RF_GROUP_RUNTIME_EMPTY&&s.entries[k].source->keys[0].uid==rows[j].uid){status=RF_FORMAT;goto fail;}
        status=rf_mover_checkpoint_prepare(rows+j,live,now,&entry->translation);if(status)goto fail;
        for(k=0;k<bindings[i].mover_count;k++){
            uint32_t h=bindings[i].mover_handles[k],slot=h&0xffffu;
            if(slot>=RF_OBJECT_CAPACITY||!s.slots[slot].pose||s.slots[slot].handle!=h){status=RF_FORMAT;goto fail;}
        }
        memcpy(entry->pose.pending,entry->translation.pending,12);entry->pose.flags=entry->translation.object_flags;
        entry->translation.motion.flags|=8; /* Force only staged commit; cleared by core. */
    }
    geometry=*movers;geometry.poses=s.poses;geometry.views=s.views;
    status=rf_geometry_collision_movers_propagate(&geometry,s.bindings,runtime->count,1.0f/60,1);if(status)goto fail;
    for(i=0;i<runtime->count;i++)if(s.entries[i].kind!=RF_GROUP_RUNTIME_EMPTY){
        rf_group_runtime_entry *entry=s.entries+i;
        status=rf_group_commit_positions(&entry->translation.motion.flags,&entry->pose,s.bindings+i,s.slots,RF_OBJECT_CAPACITY);if(status)goto fail;
        memcpy(entry->translation.position,entry->pose.position,12);memcpy(entry->translation.pending,entry->pose.pending,12);
    }
    status=rf_geometry_collision_movers_sync(&geometry);if(status)goto fail;
    *out=s;return RF_OK;
fail:scene_mover_checkpoint_close(&s);return status;
}
/* Preflight every original identity before the first write. Caller must keep
 * simulation paused between preparation and commit; this checks identity and
 * topology, not an optimistic concurrency version of mutable gameplay state.
 * No allocation/callback/lookup capable of failure remains after validation. */
static inline int scene_mover_checkpoint_validate(rf_group_runtime_collection *runtime,
    const rf_group_registration *registration,const rf_group_controller_view *bindings,
    rf_geometry_collision_movers *movers,const scene_mover_checkpoint_stage *s)
{
    uint32_t i,j,active=0;
    if(!runtime||!registration||!movers||!s||!s->slots||
       runtime->count!=s->controller_count||movers->count!=s->mover_count||registration->count>runtime->count||
       runtime->items!=s->original_entries||movers->poses!=s->original_poses||movers->views!=s->original_views||
       registration->controllers!=s->original_controllers||registration->registry!=s->original_registry||
       (runtime->count&&(!bindings||!s->entries||!s->bindings||!s->controller_handles||!s->controller_uids||!s->controller_key_counts))||
       (movers->count&&(!s->poses||!s->views)))return RF_RANGE;
    for(i=0;i<runtime->count;i++){
        const rf_group_runtime_entry *live=runtime->items+i,*candidate=s->entries+i;
        const rf_group_controller_view *b=bindings+i,*saved=s->bindings+i;
        if(live->kind!=candidate->kind||live->source!=candidate->source||live->initial_flags!=candidate->initial_flags)return RF_FORMAT;
        if(live->kind==RF_GROUP_RUNTIME_EMPTY){if(b->mover_count||b->general_count)return RF_FORMAT;continue;}
        ++active;
        if(!live->source||!live->source->keys||live->source->record.key_count!=s->controller_key_counts[i]||live->source->keys[0].uid!=s->controller_uids[i]||
           b->runtime!=&live->translation||b->first_key!=saved->first_key||b->first_key!=live->source->keys||b->general_count||
           b->mover_count!=saved->mover_count||b->rotation_sign!=saved->rotation_sign||
           (b->mover_count&&(!b->mover_handles||memcmp(b->mover_handles,saved->mover_handles,b->mover_count*4))))return RF_FORMAT;
        for(j=0;j<registration->count;j++)if(registration->controllers[j].handle==s->controller_handles[i])break;
        if(j==registration->count||registration->controllers[j].runtime!=live||registration->controllers[j].object_kind!=8||
           rf_object_registry_lookup(registration->registry,s->controller_handles[i])!=registration->controllers+j)return RF_FORMAT;
    }
    if(active!=registration->count)return RF_FORMAT;
    for(i=0;i<movers->count;i++){
        const rf_group_registered_mover *registered;rf_collision_solid_view check=movers->views[i];
        uint32_t h=s->views[i].object_id;
        if(movers->views[i].object_id!=h)return RF_FORMAT;
        registered=rf_object_registry_lookup(registration->registry,h);
        if(!registered||registered->object_kind!=9||registered->handle!=h||registered->pose!=movers->poses+i)return RF_FORMAT;
        if(movers->poses[i].radius!=s->poses[i].radius||memcmp(movers->poses[i].base_position,s->poses[i].base_position,12)||
           memcmp(movers->poses[i].base_matrix,s->poses[i].base_matrix,36))return RF_FORMAT;
        /* Ignore current pose, but forbid replacing collision geometry owners. */
        memcpy(check.minimum,s->views[i].minimum,12);memcpy(check.maximum,s->views[i].maximum,12);
        memcpy(check.input_origin,s->views[i].input_origin,12);memcpy(check.output_origin,s->views[i].output_origin,12);
        memcpy(check.input_matrix,s->views[i].input_matrix,36);memcpy(check.output_matrix,s->views[i].output_matrix,36);
        if(memcmp(&check,s->views+i,sizeof(check)))return RF_FORMAT;
    }
    return RF_OK;
}
static inline void scene_mover_checkpoint_assign(rf_group_runtime_collection *runtime,
    rf_geometry_collision_movers *movers,const scene_mover_checkpoint_stage *s)
{
    uint32_t i;
    for(i=0;i<runtime->count;i++)if(runtime->items[i].kind!=RF_GROUP_RUNTIME_EMPTY){
        runtime->items[i].translation=s->entries[i].translation;runtime->items[i].pose=s->entries[i].pose;
    }
    if(movers->count){memcpy(movers->poses,s->poses,movers->count*sizeof(*s->poses));memcpy(movers->views,s->views,movers->count*sizeof(*s->views));}
}
static inline int scene_mover_checkpoint_commit(rf_group_runtime_collection *runtime,
    const rf_group_registration *registration,const rf_group_controller_view *bindings,
    rf_geometry_collision_movers *movers,const scene_mover_checkpoint_stage *s)
{
    int status=scene_mover_checkpoint_validate(runtime,registration,bindings,movers,s);if(status)return status;
    scene_mover_checkpoint_assign(runtime,movers,s);return RF_OK;
}
/* Commit contract: after ALL checkpoint components pass, the helper copies each nonempty
 * entry's translation and pose, and mover poses/views, back into existing live
 * allocations. Never replace runtime entries/registrations or copy bindings
 * (their runtime pointers target this stage). Preserve live source pointers,
 * registry identities and geometry storage. Reset interval/contact caches,
 * refresh visibility/rooms and rebuild/restart audio separately. Then close.
 * force propagation resets mover velocity to zero to avoid a teleport impulse. */
#endif
