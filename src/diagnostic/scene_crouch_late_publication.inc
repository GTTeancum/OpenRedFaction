#ifndef RF_SCENE_CROUCH_LATE_PUBLICATION_INCLUDED
#define RF_SCENE_CROUCH_LATE_PUBLICATION_INCLUDED
#include "rf/prepare_memory.h"
/* Proposed crouch-only late-close receipt. Existing reload owners keep their
 * early-close lane. Allocate before callbacks; capture the authenticated NPC
 * assignment projection before occupied-vehicle publication; seal after all
 * fallible publication and before common storage close. No gameplay repair,
 * resource demand, or callback is performed by these operations. Return
 * candidate fields are sealed for these crouch-selected rows only; this does
 * not supply the still-required independent return-only late-close lane. */
typedef struct scene_crouch_late_row {
    uint32_t row,slot,handle,uid,class_id,registered,retired,shape_present,profile;
    uint32_t intent,stamp,retire_pending;
    uint32_t return_profile,return_phase,return_retire_pending;
    scene_pursuit_return_receipt return_prepared;
    rf_npc_checkpoint_crouch saved;
    scene_crouch_receipt prepared;
    scene_crouch_shape_seal shape;
    campaign_npc_body assigned;
    rf_entity_pose pose;
    float matrices[50][12];uint16_t generations[50];
} scene_crouch_late_row;
typedef struct scene_crouch_late_receipt {
    uint64_t epoch;uint32_t bytes,count,captured;
    const scene_npc_checkpoint_restore_stage *stage;
    scene_npc_reload_publication_receipt *publication;
    const rf_physics_stance_cache *stances;
    scene_crouch_late_row rows[];
} scene_crouch_late_receipt;
_Static_assert(sizeof(void*)!=4||sizeof(scene_crouch_late_row)<=6820,"Return receipt enlarged crouch late row exceeds proposed Xbox32 bound");
_Static_assert(sizeof(void*)!=4||sizeof(scene_crouch_late_receipt)<=40,"Crouch late header exceeds reviewed Xbox32 bound");
static void scene_crouch_late_close(scene_crouch_late_receipt **r)
{if(r&&*r){free(*r);*r=NULL;}}
static int scene_crouch_late_candidate(const scene_npc_checkpoint_restore_entry *e,
    const scene_crouch_late_row *r)
{
    if(!e||!r||e->slot!=r->slot||e->handle!=r->handle||e->saved.uid!=r->uid||
       e->saved.class_id!=r->class_id||e->registered!=r->registered||e->saved.retired!=r->retired||
       e->crouch_shape_present!=r->shape_present||e->saved.crouch_profile!=r->profile||
       e->crouch_intent!=r->intent||e->crouch_stamp!=r->stamp||e->crouch_retire_pending!=r->retire_pending||
       e->pursuit_return_retire_pending!=r->return_retire_pending||
       e->saved.pursuit_return_profile!=r->return_profile||
       e->saved.pursuit_return_phase!=r->return_phase||
       memcmp(&e->pursuit_return_prepared,&r->return_prepared,sizeof(r->return_prepared))||
       memcmp(&e->saved.crouch,&r->saved,sizeof(r->saved))||
       memcmp(&e->crouch_prepared,&r->prepared,sizeof(r->prepared))||
       memcmp(&e->crouch_shape,&r->shape,sizeof(r->shape)))return RF_FORMAT;
    return RF_OK;
}
static int scene_crouch_late_lifetime(const scene_npc_checkpoint_restore_stage *stage,
    const scene_crouch_late_receipt *r)
{
    if(!r||!stage||r->stage!=stage||r->epoch!=scene_robot_death_epoch||
       r->stances!=campaign_npc_stances||!r->publication||
       r->captured>1||r->count>RF_NPC_CHECKPOINT_MAX_COUNT||
       r->bytes!=sizeof(*r)+(uint64_t)r->count*sizeof(*r->rows))return RF_FORMAT;
    {int status=scene_npc_reload_publication_lifetime(stage,r->publication);if(status)return status;}
    /* Owning container headers alone do not authenticate per-model borrowed
     * arrays. Check their captured headers before any clip/cache dereference. */
    for(uint32_t i=0;i<r->publication->count;i++){
        const scene_npc_reload_publication_row *v=r->publication->rows+i;
        uint32_t skeleton=v->skeleton_id;
        if(!v->registered||skeleton==UINT32_MAX)continue;
        if(skeleton>=campaign_playback_resources.model_count||skeleton>=campaign_motion_catalog.model_count||
           skeleton>=campaign_skeletons.count||
           memcmp(campaign_playback_resources.models+skeleton,&v->resources,sizeof(v->resources))||
           memcmp(campaign_motion_catalog.models+skeleton,&v->motions,sizeof(v->motions))||
           memcmp(campaign_skeletons.items+skeleton,&v->skeleton,sizeof(v->skeleton)))return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_crouch_late_begin_accounted(const scene_npc_checkpoint_restore_stage *stage,
    scene_npc_reload_publication_receipt *publication,uint32_t budget,scene_crouch_late_receipt **out,const rf_prepare_memory *memory)
{
    scene_crouch_late_receipt *r;uint32_t count=0,at=0;uint64_t bytes;int status;
    if(!out||*out||!stage||!publication||!stage->crouch_joined)return RF_RANGE;
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    for(uint32_t i=0;i<stage->count;i++)count+=!!stage->entries[i].crouch_shape_present;
    bytes=sizeof(*r)+(uint64_t)count*sizeof(*r->rows);
    if(bytes>budget||bytes>UINT32_MAX||bytes>SIZE_MAX)return RF_RANGE;
    status=rf_prepare_memory_admit(memory,NULL,(size_t)bytes);if(status)return status;
    r=calloc(1,(size_t)bytes);if(!r)return RF_IO;
    r->epoch=scene_robot_death_epoch;r->bytes=(uint32_t)bytes;r->count=count;
    r->stage=stage;r->publication=publication;r->stances=campaign_npc_stances;
    for(uint32_t i=0;i<stage->count;i++)if(stage->entries[i].crouch_shape_present){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;scene_crouch_late_row *v=r->rows+at++;
        v->row=i;v->slot=e->slot;v->handle=e->handle;v->uid=e->saved.uid;v->class_id=e->saved.class_id;
        v->registered=e->registered;v->retired=e->saved.retired;v->shape_present=e->crouch_shape_present;
        v->profile=e->saved.crouch_profile;v->intent=e->crouch_intent;v->stamp=e->crouch_stamp;
        v->return_profile=e->saved.pursuit_return_profile;v->return_phase=e->saved.pursuit_return_phase;
        v->return_prepared=e->pursuit_return_prepared;
        v->return_retire_pending=e->pursuit_return_retire_pending;
        v->saved=e->saved.crouch;v->prepared=e->crouch_prepared;v->shape=e->crouch_shape;v->retire_pending=e->crouch_retire_pending;
    }
    *out=r;return RF_OK;
}
static inline int scene_crouch_late_begin(const scene_npc_checkpoint_restore_stage *stage,
    scene_npc_reload_publication_receipt *publication,uint32_t budget,scene_crouch_late_receipt **out)
{return scene_crouch_late_begin_accounted(stage,publication,budget,out,NULL);}
/* Rebase only the reference counter, using the exact checked old/new slots.
 * All other catalog/payload identity must remain byte-for-byte unchanged. */
static int scene_crouch_late_resources(const scene_npc_checkpoint_restore_stage *stage,
    scene_npc_reload_publication_receipt *publication,uint32_t assign)
{
    for(uint32_t i=0;i<publication->resource_count;i++){
        scene_npc_reload_publication_resource *old=publication->clips+i,current={0};
        int64_t expected=old->value.references;int status;
        if(assign)for(uint32_t j=0;j<stage->count;j++)if(stage->entries[j].registered){
            const scene_npc_checkpoint_restore_entry *e=stage->entries+j;
            if(e->pose_before.skeleton!=old->skeleton)continue;
            if(e->pose_before.playback.completion.active.count>16||e->playback.completion.active.count>16)return RF_FORMAT;
            for(uint32_t k=0;k<e->pose_before.playback.completion.active.count;k++)
                expected-=e->pose_before.playback.completion.active.slots[k].motion==(int32_t)old->id;
            if(expected<0)return RF_FORMAT;
            for(uint32_t k=0;k<e->playback.completion.active.count;k++)
                expected+=e->playback.completion.active.slots[k].motion==(int32_t)old->id;
            if(expected>INT32_MAX)return RF_FORMAT;
        }
        status=scene_npc_reload_publication_clip(old->skeleton,(int32_t)old->id,old->payload!=NULL,&current);if(status)return status;
        if(current.value.references!=expected)return RF_FORMAT;
        current.value.references=old->value.references;
        if(memcmp(&current,old,sizeof(current)))return RF_FORMAT;
    }
    /* Commit only after EVERY clip validated. Stable synchronous ownership
     * excludes mutation here; these are the exact counters just compared.
     * This changes the private receipt, never live references or payloads. */
    if(assign)for(uint32_t i=0;i<publication->resource_count;i++)
        publication->clips[i].value.references=publication->clips[i].address->references;
    return RF_OK;
}
/* One loader-owned state for the shared publication, regardless of how many
 * late adapters use it: 0 prepared, 1 rebased, 2 failed, 3 finally sealed.
 * This operation is forbidden in the early-close lane. Root allocations and
 * immutable publication layout remain parent-authenticated, as at begin.
 * No rollback of counters: NPC assignment already changed live references.
 * On failure preserve the existing scene rejection/abort/discard policy. */
static int scene_npc_late_resources(const scene_npc_checkpoint_restore_stage *stage,
    scene_npc_reload_publication_receipt *publication,uint32_t *state,uint32_t assign)
{
    int status;
    if(!state)return RF_RANGE;
    if(assign>1||*state!=(assign?0u:1u)){*state=2;return RF_FORMAT;}
    *state=2;
    status=scene_npc_reload_publication_lifetime(stage,publication);if(status)return status;
    /* Authenticate every retained model header, including models owned only
     * by another late participant, before resource lookup follows its arrays. */
    for(uint32_t i=0;i<publication->count;i++){
        const scene_npc_reload_publication_row *v=publication->rows+i;
        uint32_t skeleton=v->skeleton_id;
        if(!v->registered||skeleton==UINT32_MAX)continue;
        if(skeleton>=campaign_playback_resources.model_count||
           skeleton>=campaign_motion_catalog.model_count||
           skeleton>=campaign_skeletons.count||
           !campaign_playback_resources.models||!campaign_motion_catalog.models||
           !campaign_skeletons.items||
           memcmp(campaign_playback_resources.models+skeleton,&v->resources,sizeof(v->resources))||
           memcmp(campaign_motion_catalog.models+skeleton,&v->motions,sizeof(v->motions))||
           memcmp(campaign_skeletons.items+skeleton,&v->skeleton,sizeof(v->skeleton)))return RF_FORMAT;
    }
    status=scene_crouch_late_resources(stage,publication,assign);if(status)return status;
    *state=assign?1u:3u;return RF_OK;
}
/* Only after a successful pre-assignment publication seal and the ordinary
 * callback-free NPC assignment block. Never call in the early-close lane. */
static int scene_crouch_late_capture(const scene_npc_checkpoint_restore_stage *stage,
    scene_crouch_late_receipt *r)
{
    int status=scene_crouch_late_lifetime(stage,r);if(status)return status;
    if(r->captured)return RF_FORMAT;
    for(uint32_t i=0;i<r->count;i++){
        scene_crouch_late_row *v=r->rows+i;const scene_npc_checkpoint_restore_entry *e;const rf_entity_pose *p;
        if(v->row>=stage->count)return RF_FORMAT;e=stage->entries+v->row;
        status=scene_crouch_late_candidate(e,v);if(status)return status;
        status=scene_crouch_assigned(stage,e);if(status)return status;
        p=campaign_poses.items+e->slot;
        if(p->bone_count>50||!p->matrices||!p->generations)return RF_FORMAT;
        v->assigned=campaign_npc_bodies[e->slot];v->pose=*p;
        memcpy(v->matrices,p->matrices,(size_t)p->bone_count*48);
        memcpy(v->generations,p->generations,(size_t)p->bone_count*2);
    }
    /* Shared resource rebase belongs to the loader after ALL captures. */
    r->captured=1;return RF_OK;
}
static int scene_crouch_late_seal(const scene_npc_checkpoint_restore_stage *stage,
    const scene_crouch_late_receipt *r)
{
    int status=scene_crouch_late_lifetime(stage,r);if(status)return status;
    if(r->captured!=1)return RF_FORMAT;
    for(uint32_t i=0;i<r->count;i++){
        const scene_crouch_late_row *v=r->rows+i;const scene_npc_checkpoint_restore_entry *e;const rf_entity_pose *p;
        if(v->row>=stage->count)return RF_FORMAT;e=stage->entries+v->row;
        status=scene_crouch_late_candidate(e,v);if(status)return status;
        if(v->slot>=campaign_npc_body_count||v->slot>=campaign_poses.count)return RF_FORMAT;
        p=campaign_poses.items+v->slot;
        if(memcmp(campaign_npc_bodies+v->slot,&v->assigned,sizeof(v->assigned))||
           memcmp(p,&v->pose,sizeof(v->pose)))return RF_FORMAT;
        status=scene_crouch_assigned(stage,e);if(status)return status;
        if(memcmp(p->matrices,v->matrices,(size_t)p->bone_count*48)||
           memcmp(p->generations,v->generations,(size_t)p->bone_count*2))return RF_FORMAT;
    }
    /* Loader seals shared resources once after ALL late adapters seal. */
    return RF_OK;
}
#endif
