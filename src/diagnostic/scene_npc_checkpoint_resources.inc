#ifndef RF_SCENE_NPC_CHECKPOINT_RESOURCES_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_RESOURCES_INCLUDED
#include "rf/npc_checkpoint.h"
/* Include after NPC owners/catalog globals and campaign_npc_motion_require.
 * Input rows must already pass RFNC identity/catalog/semantic decode. This is
 * cache demand only: no actor selection, controller, playback or reference
 * count is changed. Call while gameplay is paused, before restore_prepare.
 * Keep the scene alive on failure: successful earlier loads remain ordinary
 * cache entries and are released by existing scene/cache cleanup. */
typedef struct scene_npc_checkpoint_resource_request {uint32_t skeleton,id,cache;} scene_npc_checkpoint_resource_request;
static int scene_npc_checkpoint_resource_add(scene_npc_checkpoint_resource_request *plan,uint32_t *count,
    uint32_t capacity,uint32_t skeleton,int32_t id)
{
    const rf_entity_playback_model *model;const rf_motion_file *file;uint32_t i,cache;
    if(id<0)return id==-1?RF_OK:RF_FORMAT;
    if(!campaign_playback_resources.models||!campaign_motion_catalog.models||
       skeleton>=campaign_playback_resources.model_count||skeleton>=campaign_motion_catalog.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+skeleton;
    if(!model->resources||!model->cache_ids||!campaign_motion_catalog.models[skeleton].items||
       (uint32_t)id>=model->count||(uint32_t)id>=campaign_motion_catalog.models[skeleton].count)return RF_FORMAT;
    cache=model->cache_ids[id];file=&campaign_motion_catalog.models[skeleton].items[id].file;
    if(cache>=campaign_npc_motion_count||!file->archive||!file->entry.size||model->resources[id].references<0)return RF_FORMAT;
    for(i=0;i<*count;i++)if(plan[i].skeleton==skeleton&&plan[i].id==(uint32_t)id)return RF_OK;
    if(*count>=capacity)return RF_RANGE;
    plan[*count]=(scene_npc_checkpoint_resource_request){skeleton,(uint32_t)id,cache};++*count;return RF_OK;
}
static inline int scene_npc_checkpoint_resources_require(const rf_npc_checkpoint_record *rows,uint32_t count,
    uint32_t scratch_budget,uint32_t *added_bytes)
{
    scene_npc_checkpoint_resource_request *plan=NULL;uint32_t capacity,requests=0,i,j,k,slot,before;
    uint64_t scratch,missing=0;int status=RF_OK;
    if(!added_bytes||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||
       campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS)return RF_RANGE;
    if(!count){*added_bytes=0;return RF_OK;}
    if(!campaign_npc_bodies||!campaign_seeds.items||!campaign_seeds.records.items||!campaign_seeds.classes||
       campaign_seeds.records.count<campaign_npc_body_count||!campaign_npc_motion_data||!campaign_npc_motion_sizes)return RF_RANGE;
    /* 16 active clips + current/next/override + independent scripted clip.
     * Unique requests share entries, but allocation stays bounded before use. */
    capacity=count*20u;scratch=(uint64_t)capacity*sizeof(*plan);
    if(scratch>scratch_budget||scratch>SIZE_MAX)return RF_RANGE;
    plan=malloc((size_t)scratch);if(!plan)return RF_IO;
    for(i=0;i<count;i++){
        const rf_npc_checkpoint_record *r=rows+i;rf_entity_motion_selection selection;uint32_t found=0,skeleton;
        if(r->uid==UINT32_MAX||r->retired>1||r->animation_present>1||(i&&rows[i-1].uid>=r->uid)){status=RF_FORMAT;goto done;}
        slot=0;
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==r->uid){slot=j;++found;}
        if(found!=1||!campaign_npc_bodies[slot].persistence_registered||
           r->class_id!=campaign_seeds.items[slot].class_index||r->class_id>=campaign_seeds.class_count){status=RF_FORMAT;goto done;}
        if(r->retired)continue;
        if(!campaign_npc_bodies[slot].registration.view||!campaign_poses.items||slot>=campaign_poses.count){status=RF_RANGE;goto done;}
        status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,r->class_id,&selection);if(status)goto done;
        if(r->primary>=0){status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,r->class_id,r->primary,&selection);if(status)goto done;}
        skeleton=selection.mapping.skeleton;
        if(skeleton!=campaign_poses.items[slot].skeleton){status=RF_FORMAT;goto done;}
#define NPC_RESOURCE_ADD(id) do{status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,skeleton,(id));if(status)goto done;}while(0)
        if(!r->animation_present){NPC_RESOURCE_ADD(selection.mapping.states[0]);}
        else{
            if(r->playback.completion.active.count>16||r->script_animation.active>1||r->controller.override_enabled>1){status=RF_FORMAT;goto done;}
            for(k=0;k<3;k++){
                int32_t state=k==0?r->controller.current:k==1?r->controller.next:
                    r->controller.override_enabled?r->controller.override_state:-1;
                if(state< -1||state>=23){status=RF_FORMAT;goto done;}
                if(state>=0){NPC_RESOURCE_ADD(selection.mapping.states[state]);}
            }
            if(r->script_animation.active){NPC_RESOURCE_ADD(r->script_animation.motion);}
            for(k=0;k<r->playback.completion.active.count;k++){
                int32_t id=r->playback.completion.active.slots[k].motion;
                if(id<0){status=RF_FORMAT;goto done;}NPC_RESOURCE_ADD(id);
            }
        }
#undef NPC_RESOURCE_ADD
    }
    /* campaign_npc_motion_require may evict zero-reference payloads. Pending
     * restore clips intentionally have no references yet: preflight enough
     * free space for the whole set so loading cannot evict earlier requests.
     * This conservative first-pass policy does not expand the shared1MiB cap. */
    for(i=0;i<requests;i++){
        const scene_npc_checkpoint_resource_request *r=plan+i;
        const rf_motion_file *file=&campaign_motion_catalog.models[r->skeleton].items[r->id].file;
        for(j=0;j<i;j++)if(plan[j].cache==r->cache)break;
        if(j<i){
            const rf_motion_file *other=&campaign_motion_catalog.models[plan[j].skeleton].items[plan[j].id].file;
            if(other->entry.size!=file->entry.size){status=RF_FORMAT;goto done;}
        }else if(!campaign_npc_motion_data[r->cache])missing+=file->entry.size;
        else if(campaign_npc_motion_sizes[r->cache]!=file->entry.size){status=RF_FORMAT;goto done;}
    }
    if((uint64_t)campaign_npc_motion_bytes+missing>1024u*1024u){status=RF_RANGE;goto done;}
    before=campaign_npc_motion_bytes;
    for(i=0;i<requests;i++){
        status=campaign_npc_motion_require(plan[i].skeleton,plan[i].id);if(status)goto done;
    }
    *added_bytes=campaign_npc_motion_bytes-before;
done:free(plan);return status;
}
#endif
