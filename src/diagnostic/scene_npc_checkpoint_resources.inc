#ifndef RF_SCENE_NPC_CHECKPOINT_RESOURCES_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_RESOURCES_INCLUDED
#include "rf/npc_checkpoint.h"
#include "scene_npc_return_resource_plan.inc"
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
/* Read-only future action40 registration. Incoming standalone eligibility is
 * provisional here and must still pass the composed seat/owner join before
 * publication; a plan is resource demand, never live admission. */
static int scene_pursuit_return_recovery_clip(const rf_entity_motion_selection *selection,
    int32_t primary,int32_t linked,int32_t *id)
{
    scene_npc_return_resource_plan future;uint32_t skeleton;int status;
    if(!selection||!id)return RF_RANGE;
    skeleton=selection->mapping.skeleton;
    if(!campaign_playback_resources.models||!campaign_motion_catalog.models||
       skeleton>=campaign_playback_resources.model_count||skeleton>=campaign_motion_catalog.model_count)return RF_FORMAT;
    status=scene_npc_return_resources_plan(&selection->mapping,selection->mapping.class_index,
        skeleton,campaign_playback_resources.models[skeleton].count,primary,
        selection->mapping.weapon,linked,1,0,&future);if(status)return status;
    if(future.count>1)return RF_FORMAT;
    if(future.count&&future.clips[0].motion>=campaign_motion_catalog.models[skeleton].count)return RF_FORMAT;
    *id=future.count?(int32_t)future.clips[0].motion:-1;return RF_OK;
}
/* Optional synchronous accounting for this ONE temporary plan allocation.
 * Acquire runs before malloc; release runs after free, including malloc failure.
 * The owner token is the pinned address of the local plan variable. Callbacks
 * may update only caller-owned accounting, never demand/mutate scene resources.
 * Successful admission remains cumulatively charged after release or failure.
 * This interface has no dependency on the later world/return-driver includes. */
typedef int (*scene_npc_resource_plan_account)(void *context,const void *owner,
    size_t request_bytes,uint32_t acquire);
static inline int scene_npc_checkpoint_resources_require_memory(const rf_npc_checkpoint_record *rows,uint32_t count,
    uint32_t scratch_budget,uint32_t *added_bytes,scene_npc_resource_plan_account account,void *account_context,
    const rf_prepare_memory *payload_memory)
{
    scene_npc_checkpoint_resource_request *plan=NULL;uint32_t capacity,requests=0,i,j,k,slot,before;
    uint64_t scratch,missing=0;uint32_t live_present,new_present,guard_present,alarm_present,crouch_live=0,crouch_new=0;int status=RF_OK;
    if((!account&&account_context)||!added_bytes||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||
       campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&!campaign_npc_bodies))return RF_RANGE;
    status=scene_npc_reload_live_present(combat_frame==UINT32_MAX?0:combat_frame,&live_present);if(status)return status;
    status=scene_guard_retreat_live_present(combat_frame==UINT32_MAX?0:combat_frame,&guard_present);if(status)return status;
    status=scene_alarm_runner_live_present(&alarm_present);if(status)return status;
    new_present=live_present|guard_present|alarm_present;live_present|=guard_present|alarm_present;
    {uint32_t noise_present;status=scene_noise_live_present(&noise_present);if(status)return status;
     new_present|=noise_present;live_present|=noise_present;}
    for(i=0;i<campaign_npc_body_count;i++)if(scene_patrol_future_owner(campaign_npc_bodies+i)||campaign_npc_bodies[i].tactical.phase||campaign_npc_bodies[i].pursuit_return.phase)new_present=live_present=1;
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].flight.qualified==1&&campaign_npc_bodies[i].flight.entered)
        new_present=live_present=1;
    for(i=0;i<count;i++){
        if(rows[i].reload_present>1||(!rows[i].reload_present&&(rows[i].reload_weapon||rows[i].reload_remaining)))return RF_FORMAT;
        new_present|=(rows[i].ai_mode==6)|rows[i].reload_present|rows[i].guard_retreat.present|rows[i].flight.entered|rows[i].alarm.present|(rows[i].route.patrol&1)|!!rows[i].tactical.phase|!!rows[i].pursuit_return_phase;
    }
    /* Authenticate retained ownership before any demand or eviction. Count
     * only actual crouch rows, leaving legacy scratch requirements unchanged. */
    {uint32_t present;
     status=scene_crouch_live_present(&present);if(status)return status;
     status=scene_crouch_incoming_present(rows,count,&present);if(status)return status;}
    for(i=0;i<campaign_npc_body_count;i++)crouch_live+=!!(campaign_npc_bodies[i].crouch.flags&SCENE_CROUCH_PRESENT);
    for(i=0;i<count;i++)crouch_new+=!!(rows[i].crouch.flags&1u);
    if(!count&&!live_present&&!crouch_live){*added_bytes=0;return RF_OK;}
    if(!campaign_npc_bodies||!campaign_seeds.items||!campaign_seeds.records.items||!campaign_seeds.classes||
       campaign_seeds.records.count<campaign_npc_body_count||!campaign_npc_motion_data||!campaign_npc_motion_sizes)return RF_RANGE;
    /* Existing capacity:20 incoming slot/controller/script requests plus8
     * future-owner requests per incoming/outgoing actor. A flight episode
     * needs5 and cannot overlap guard7/reload ownership. Crouch adds at
     * most7 requests per retained side, charged to the same scratch budget.
     * Authenticate old pending before demand can evict zero-ref payloads. */
    capacity=count*20u+(new_present?8u*(count+campaign_npc_body_count):0u)+7u*(crouch_live+crouch_new);scratch=(uint64_t)capacity*sizeof(*plan);
    if(scratch>scratch_budget||scratch>SIZE_MAX)return RF_RANGE;
    if(account){status=account(account_context,&plan,(size_t)scratch,1);if(status)return status;}
    plan=malloc((size_t)scratch);if(!plan){status=RF_IO;goto done;}
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].crouch.flags&SCENE_CROUCH_PRESENT){
        const campaign_npc_body *o=campaign_npc_bodies+i;int32_t id;
        status=scene_crouch_live_shape(i,o);if(status)goto done;
        /* Dead retained geometry has no future locomotion obligation; any
         * residual active action4 is covered by ordinary playback requests. */
        if(o->damage.effects.health<=0||(o->view.flags_810&1u))continue;
        for(k=0;k<7;k++){
            status=scene_crouch_future_clip(&o->selection,k,&id);
            if(status==RF_NOT_FOUND)continue;if(status)goto done;
            status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,o->selection.mapping.skeleton,id);if(status)goto done;
        }
    }
    for(i=0;i<campaign_npc_body_count;i++)if(scene_patrol_future_owner(campaign_npc_bodies+i)){
        const rf_entity_motion_selection *sel=&campaign_npc_bodies[i].selection;
        status=scene_patrol_resources(i,sel,0);if(status)goto done;
        status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,sel->mapping.states[0]);if(status)goto done;
        status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,sel->mapping.states[2]);if(status)goto done;
    }
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].tactical.phase||campaign_npc_bodies[i].pursuit_return.phase){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        const rf_entity_motion_selection *sel=&o->selection;
        status=scene_tactical_owner(i);if(status)goto done;
        if(o->pursuit_return.phase){
            status=scene_pursuit_source(i);if(status)goto done;
            status=scene_pursuit_return_tactical_admit(&o->pursuit_return,&o->tactical);if(status)goto done;
            if(o->pursuit_return.phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE){
                int32_t id;
                status=scene_pursuit_return_recovery_clip(sel,o->view.weapons[0],o->view.linked_handle,&id);if(status)goto done;
                status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,id);if(status)goto done;
            }
        }
        status=scene_patrol_resources(i,sel,0);if(status)goto done;
        status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,sel->mapping.states[0]);if(status)goto done;
        status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,sel->mapping.states[2]);if(status)goto done;
    }
    for(i=0;i<campaign_npc_body_count;i++)if(scene_noise_active(campaign_npc_bodies+i)){
        const rf_entity_motion_selection *sel=&campaign_npc_bodies[i].selection;
        status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,sel->mapping.states[0]);if(status)goto done;
        status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,sel->mapping.states[2]);if(status)goto done;
    }
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].combat_reload_pending){
        const campaign_npc_body *o=campaign_npc_bodies+i;
        status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,
            o->selection.mapping.skeleton,o->selection.mapping.actions[39]);if(status)goto done;
    }
    for(i=0;i<campaign_npc_body_count;i++)if(scene_guard_retreat_active(campaign_npc_bodies+i)){
        const rf_entity_motion_selection *sel=&campaign_npc_bodies[i].selection;
        const int32_t ids[7]={sel->mapping.states[0],sel->mapping.states[2],sel->mapping.states[4],
            sel->mapping.states[6],sel->mapping.states[13],sel->mapping.actions[2],sel->mapping.actions[39]};
        for(k=0;k<7;k++){
            if(ids[k]<0){status=RF_FORMAT;goto done;}
            status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,ids[k]);if(status)goto done;
        }
    }
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].alarm.present){
        const rf_entity_motion_selection *sel=&campaign_npc_bodies[i].selection;rf_npc_checkpoint_alarm receipt;
        const int32_t ids[4]={sel->mapping.states[0],sel->mapping.states[2],sel->mapping.states[4],sel->mapping.actions[44]};
        status=scene_alarm_runner_capture(i,&receipt);if(status)goto done;
        status=scene_alarm_runner_resources(i,sel,0);if(status)goto done;
        for(k=0;k<4;k++){
            status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,ids[k]);if(status)goto done;
        }
    }
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].flight.qualified==1&&campaign_npc_bodies[i].flight.entered){
        static const uint32_t states[5]={0,2,4,6,13};
        const rf_entity_motion_selection *sel=&campaign_npc_bodies[i].selection;
        status=scene_civilian_flight_mapping(i,sel);if(status)goto done;
        for(k=0;k<5;k++){
            status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,sel->mapping.skeleton,sel->mapping.states[states[k]]);
            if(status)goto done;
        }
    }
    for(i=0;i<count;i++){
        const rf_npc_checkpoint_record *r=rows+i;rf_entity_motion_selection selection;uint32_t found=0,skeleton;
        if(r->uid==UINT32_MAX||r->retired>1||r->animation_present>1||r->reload_present>1||
           (!r->reload_present&&(r->reload_weapon||r->reload_remaining))||(i&&rows[i-1].uid>=r->uid)){status=RF_FORMAT;goto done;}
        slot=0;
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==r->uid){slot=j;++found;}
        if(found!=1||!campaign_npc_bodies[slot].persistence_registered||
           r->class_id!=campaign_seeds.items[slot].class_index||r->class_id>=campaign_seeds.class_count){status=RF_FORMAT;goto done;}
        if(r->dead_pose==2){
            /* RFAP13 owns retirement; never demand an idle/death animation. */
            if(scene_robot_death_profile_at(slot)<0){status=RF_FORMAT;goto done;}
            continue;
        }
        if(r->retired){if(r->reload_present){status=RF_FORMAT;goto done;}continue;}
        if(!campaign_npc_bodies[slot].registration.view||!campaign_poses.items||slot>=campaign_poses.count){status=RF_RANGE;goto done;}
        status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,r->class_id,&selection);if(status)goto done;
        if(r->primary>=0){status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,r->class_id,r->primary,&selection);if(status)goto done;}
        skeleton=selection.mapping.skeleton;
        if(skeleton!=campaign_poses.items[slot].skeleton){status=RF_FORMAT;goto done;}
#define NPC_RESOURCE_ADD(id) do{status=scene_npc_checkpoint_resource_add(plan,&requests,capacity,skeleton,(id));if(status)goto done;}while(0)
        if((r->crouch.flags&1u)&&!r->dead_pose&&r->health>0){
            int32_t id;
            for(k=0;k<7;k++){
                status=scene_crouch_future_clip(&selection,k,&id);
                if(status==RF_NOT_FOUND)continue;if(status)goto done;
                NPC_RESOURCE_ADD(id);
            }
        }
        /* A queued return can have no tactical phase yet. Retain its common
         * normal/locomotion pair on both sides, within the existing eight
         * future-owner request allowance. Queued prelude adds source action40
         * independently; complete normal/standing resource closure is pending. */
        if(r->tactical.phase||r->pursuit_return_phase){
            status=scene_tactical_source(slot);if(status)goto done;
            if(r->pursuit_return_phase){
                if(r->pursuit_return_profile!=1||r->pursuit_return_phase>3){status=RF_FORMAT;goto done;}
                status=scene_pursuit_source(slot);if(status)goto done;
                status=scene_pursuit_return_saved_owner(r,r->pursuit_return_phase);if(status)goto done;
                if(r->pursuit_return_phase==SCENE_PURSUIT_RETURN_PENDING_PRELUDE){
                    int32_t id;
                    status=scene_pursuit_return_recovery_clip(&selection,r->primary,-1,&id);if(status)goto done;
                    NPC_RESOURCE_ADD(id);
                }
            }
            NPC_RESOURCE_ADD(selection.mapping.states[0]);NPC_RESOURCE_ADD(selection.mapping.states[2]);
        }
        if(r->ai_mode==6){
            status=scene_noise_candidate(slot,r,&selection);if(status)goto done;
            NPC_RESOURCE_ADD(selection.mapping.states[0]);NPC_RESOURCE_ADD(selection.mapping.states[2]);
        }
        if((r->route.patrol&1)&&!r->dead_pose&&r->health>0){
            status=scene_patrol_candidate(slot,r,&selection);if(status)goto done;
            NPC_RESOURCE_ADD(selection.mapping.states[0]);NPC_RESOURCE_ADD(selection.mapping.states[2]);
        }
        if(r->alarm.present){
            const int32_t ids[4]={selection.mapping.states[0],selection.mapping.states[2],selection.mapping.states[4],selection.mapping.actions[44]};
            status=scene_alarm_runner_candidate(slot,r,&selection);if(status)goto done;
            for(k=0;k<4;k++)NPC_RESOURCE_ADD(ids[k]);
        }
        if(r->guard_retreat.present){
            status=scene_guard_retreat_binding(slot,r->primary,r->secondary,&r->inventory,&selection,0);if(status)goto done;
            const int32_t ids[7]={selection.mapping.states[0],selection.mapping.states[2],selection.mapping.states[4],
                selection.mapping.states[6],selection.mapping.states[13],selection.mapping.actions[2],selection.mapping.actions[39]};
            for(k=0;k<7;k++){if(ids[k]<0){status=RF_FORMAT;goto done;}NPC_RESOURCE_ADD(ids[k]);}
        }
        if(r->flight.entered){
            static const uint32_t states[5]={0,2,4,6,13};
            if(r->primary!=-1||r->secondary!=-1){status=RF_FORMAT;goto done;}
            status=scene_civilian_flight_mapping(slot,&selection);if(status)goto done;
            for(k=0;k<5;k++)NPC_RESOURCE_ADD(selection.mapping.states[states[k]]);
        }
        if(r->reload_present){
            status=scene_npc_reload_saved(slot,r,&selection,0,NULL);if(status)goto done;
            NPC_RESOURCE_ADD(selection.mapping.actions[39]);
        }
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
     * Use the same bounded cache ceiling as the live motion allocator. */
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
    if((uint64_t)campaign_npc_motion_bytes+missing>SCENE_NPC_MOTION_BUDGET){status=RF_RANGE;goto done;}
    before=campaign_npc_motion_bytes;
    for(i=0;i<requests;i++){
        status=campaign_npc_motion_require_accounted(plan[i].skeleton,plan[i].id,payload_memory);if(status)goto done;
    }
done:
    free(plan);
    if(account){int released=account(account_context,&plan,(size_t)scratch,0);
        if(!status)status=released; /* Preserve the original demand/malloc error. */}
    if(!status)*added_bytes=campaign_npc_motion_bytes-before;
    return status;
}
static inline int scene_npc_checkpoint_resources_require_accounted(const rf_npc_checkpoint_record *rows,uint32_t count,
    uint32_t scratch_budget,uint32_t *added_bytes,scene_npc_resource_plan_account account,void *account_context)
{return scene_npc_checkpoint_resources_require_memory(rows,count,scratch_budget,added_bytes,account,account_context,NULL);}
static inline int scene_npc_checkpoint_resources_require(const rf_npc_checkpoint_record *rows,uint32_t count,
    uint32_t scratch_budget,uint32_t *added_bytes)
{return scene_npc_checkpoint_resources_require_accounted(rows,count,scratch_budget,added_bytes,NULL,NULL);}
#endif
