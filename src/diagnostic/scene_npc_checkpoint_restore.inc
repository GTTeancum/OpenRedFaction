#ifndef RF_SCENE_NPC_CHECKPOINT_RESTORE_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_RESTORE_INCLUDED
#include "rf/prepare_memory.h"
#include "rf/prepare_storage.h"
#include "rf/npc_checkpoint.h"
#include "scene_npc_checkpoint_pair_fit.inc"
/* Read-only candidate-world placement/support and room resolver. Must fill a
 * fresh room state with query_position equal to placement.position. No gameplay
 * callbacks, allocation of live resources or actor publication are permitted. */
/* Explicit identity travels with private candidate spheres. A copied shape
 * must never lose class/support/prop admission because its address differs
 * from the live owner's allocation. Full handle includes the generation. */
typedef int (*scene_npc_checkpoint_place)(void *,uint32_t,uint32_t,uint32_t,
    const rf_checkpoint_placement *,rf_entity_room_state *,rf_physics_support_contact *);
typedef struct scene_npc_checkpoint_restore_entry {
    uint32_t slot,handle,registered,flags,controller_handle,attack_target_handle;
    uint32_t reload_deadline,dwell_deadline;
    uint32_t blind_target,blind_last_seen; /* Prepared actual-generation receipt; final-close only. */
    scene_pursuit_delay_receipt pursuit_delay_prepared;
    scene_pursuit_return_receipt pursuit_return_prepared;
    uint32_t pursuit_return_retire_pending;
    uint32_t tactical_target;int32_t tactical_phase_due,tactical_recovery_due;
    uint32_t crouch_shape_present,crouch_intent,crouch_stamp,crouch_retire_pending;
    scene_crouch_receipt crouch_prepared;
    scene_crouch_shape_seal crouch_shape; /* Preparation snapshot; pre-demand admission is separate. */
    uint32_t unowned_motion_mask; /* Low16: burn pain; bit31: RFNC29 finalizer obligation. */
    uint32_t item_attempted_before;int32_t item_fault_before;rf_campaign_item_drop item_drop_before;
    rf_npc_checkpoint_record saved,before;
    scene_medic_flight_snapshot flight;
    scene_guard_retreat_checkpoint_stage guard_retreat;
    scene_medic_syringe_candidate medic_child;
    rf_campaign_actor_route route_before;uint32_t route_owner_snapshot;
    uint32_t medic_owner_snapshot; /* Exact old pending target/deadline/clip owner. */
    int32_t effective_ai_mode; /* Raw saved mode stays immutable during legacy initialization. */
    campaign_npc_body owner_before;rf_entity_pose pose_before;
    rf_physics_body_state body;rf_entity_room_state room;rf_physics_support_contact support;
    rf_movement_settings movement;uint32_t movement_slot,movement_resolved;float base_speed;
    rf_entity_motion_selection selection;rf_motion_playback_state playback;
    rf_level_waypoint_path move_path;rf_entity_navigation_candidate *move_nodes[4];
    float basis[9],look_basis[9],published[3],eye[3];int32_t idle,pain_deadlines[3];
    uint32_t bone_count;float (*matrices)[12];uint16_t *generations;
    float (*original_matrices)[12];uint16_t *original_generations;
} scene_npc_checkpoint_restore_entry;
typedef struct scene_npc_checkpoint_restore_stage {
    uint32_t count,bytes,fresh_boot;int32_t now;
    scene_medic_flight_rng flight_rng,flight_rng_before;
    uint32_t flight_profile,flight_joined,flight_player,alarm_joined;
    uint32_t patrol_events_joined,patrol_binding_generation;
    uint32_t blind_joined; /* No publication of unresolved on-foot candidates. */
    uint32_t tactical_cursor,tactical_joined,crouch_joined,pursuit_return_joined;
    scene_npc_checkpoint_restore_entry entries[];
} scene_npc_checkpoint_restore_stage;
/* Previous Xbox32 entry11556/header80. Proposed return growth44 includes
 * two decoded-row additions16, owner_before allowance16 and prepared receipt12;
 * return retirement marker adds4 with conservative8-byte alignment allowance;
 * header grows4. RFNC42 adds another8 across the two decoded records; actual
 * allocation charges sizeof(entry) inside the unchanged cumulative budget.
 * owner_before adds4 for speech with up to4 bytes of ABI alignment padding. */
_Static_assert(sizeof(void*)!=4||sizeof(scene_npc_checkpoint_restore_entry)<=11624,"Decoded speech field candidate growth exceeds reviewed allowance");
_Static_assert(sizeof(void*)!=4||sizeof(scene_npc_checkpoint_restore_stage)<=84,"Pursuit return restore header exceeds proposed allowance");
/* Read-only native failure detail: phase FNV1a, row index, UID, status,
 * expected resident row count and supplied row count. UINT32_MAX identifies
 * a non-row failure. This is separate from capture admission diagnostics. */
uint32_t rf_scene_npc_restore_reject[6];
static int scene_npc_checkpoint_restore_failure(const char *detail,uint32_t index,uint32_t uid,int status)
{
    uint32_t hash=2166136261u;const unsigned char *p=(const unsigned char *)detail;
    while(*p){hash^=*p++;hash*=16777619u;}
    rf_scene_npc_restore_reject[0]=hash;rf_scene_npc_restore_reject[1]=index;
    rf_scene_npc_restore_reject[2]=uid;rf_scene_npc_restore_reject[3]=(uint32_t)status;
    return status;
}
static int scene_npc_checkpoint_living_npc_handle(uint32_t uid,
    const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t *handle)
{
    uint32_t i,j;
    if(!handle||(count&&!rows))return RF_RANGE;
    if(!uid||uid==UINT32_MAX)return RF_NOT_FOUND;
    for(i=0;i<count;i++)if(rows[i].uid==uid){
        if(rows[i].retired||rows[i].dead_pose||rows[i].health<=0)return RF_FORMAT;
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==uid){
            campaign_npc_body *target=campaign_npc_bodies+j;
            if(!target->registration.view||
               rf_object_registry_lookup(&campaign_registry,target->registration.handle)!=&target->registration)return RF_FORMAT;
            *handle=target->registration.handle;return RF_OK;
        }
        return RF_FORMAT;
    }
    return RF_NOT_FOUND;
}
static int scene_npc_checkpoint_attack_target_handle(uint32_t uid,
    const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t *handle)
{
    uint32_t i;int status;
    if(!handle||(count&&!rows))return RF_RANGE;
    if(!uid){if(!campaign_player_object.handle)return RF_FORMAT;
        *handle=campaign_player_object.handle;return RF_OK;}
    if(campaign_authored_vehicle_uid && uid==(uint32_t)campaign_authored_vehicle_uid &&
       campaign_authored_vehicle_handle){*handle=campaign_authored_vehicle_handle;return RF_OK;}
    for(i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *target=campaign_passive_vehicles+i;
        if(target->uid==uid&&!target->damage.destroyed&&
           target->damage.state.effects.health>0&&
           rf_object_registry_lookup(&campaign_registry,target->handle)==target){
            *handle=target->handle;return RF_OK;
        }
    }
    status=scene_npc_checkpoint_living_npc_handle(uid,rows,count,handle);if(status!=RF_NOT_FOUND)return status;
    return scene_turret_attack_checkpoint_handle(uid,handle);
}
/* All candidate placement consumers share one shape selector. Original
 * overlap exceptions deliberately retain live source spheres separately. */
static int scene_npc_checkpoint_restore_spheres(const scene_npc_checkpoint_restore_entry *e,
    const rf_physics_sphere **spheres,uint32_t *count)
{
    const rf_physics_spheres *live;
    if(!e||!spheres||!count||e->slot>=campaign_npc_body_count||!campaign_npc_bodies||
       e->crouch_shape_present>1)return RF_RANGE;
    live=&campaign_npc_bodies[e->slot].body.spheres;
    if(e->crouch_shape_present){
        if(!e->crouch_shape.prepared.count||e->crouch_shape.prepared.count>8||e->crouch_shape.prepared.count!=live->count)
            return RF_FORMAT;
        *spheres=e->crouch_shape.prepared.spheres;*count=e->crouch_shape.prepared.count;
    }else{*spheres=live->items;*count=live->count;}
    return RF_OK;
}
/* Pure private shape preparation. This does not enable live stance policy.
 * Both sides matter: a legacy standing row must not inherit a destination
 * actor's crouched geometry. Dead but retained bodies keep their real shape;
 * retired allocations are discarded by the existing retirement publisher. */
static int scene_npc_checkpoint_restore_crouch(scene_npc_checkpoint_restore_stage *stage,
    scene_npc_checkpoint_restore_entry *e)
{
    campaign_npc_body *o;scene_crouch_shape_candidate shape;
    uint32_t physical,frame=combat_frame==UINT32_MAX?0:combat_frame;int status;
    if(!stage||!e||!campaign_npc_bodies||e->slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+e->slot;
    status=scene_crouch_receipt_prepare(&e->saved.crouch,e->saved.crouch_profile,
        stage->now,frame,&e->crouch_prepared,&e->crouch_intent,&physical);if(status)return status;
    status=scene_crouch_stamp_prepare(&e->saved.crouch,(double)frame*scene_step_seconds,
        scene_step_seconds,&e->crouch_stamp);if(status)return status;
    if(e->saved.retired){
        if(e->saved.crouch.flags)return RF_FORMAT;
        if(!(o->crouch.flags&SCENE_CROUCH_PRESENT))return RF_OK;
        /* Keep the outgoing shape allocation through the final close. The
         * retired row publishes no replacement shape or policy. */
        e->crouch_retire_pending=1;physical=!!(o->view.flags_810&0x400u);
        status=scene_crouch_live_shape(e->slot,o);if(status)return status;
    }
    if(!e->saved.crouch.flags&&!o->crouch.flags&&!(o->view.flags_810&0x400u))return RF_OK;
    if(!e->registered||scene_npc_reload_identity(e->slot,o)||!scene_npc_reload_standalone(o)||
       !campaign_npc_stances||e->saved.class_id>=campaign_seeds.class_count||
       !campaign_poses.items||e->slot>=campaign_poses.count)return RF_FORMAT;
    status=scene_tactical_source(e->slot);if(status)return status;
    status=scene_crouch_shape_prepare(&o->body.spheres,campaign_npc_stances+e->saved.class_id,
        e->saved.position,o->view.flags_810,physical,&shape);if(status)return status;
    status=scene_crouch_shape_seal_prepare(&o->body.spheres,&shape,&e->crouch_shape);if(status)return status;
    e->crouch_shape_present=1;e->owner_before=*o;e->pose_before=campaign_poses.items[e->slot];
    stage->crouch_joined=0;
    return RF_OK;
}
static int scene_npc_checkpoint_restore_placement(void *context,uint32_t index,
    rf_checkpoint_placement *p,uint32_t *retired)
{
    const scene_npc_checkpoint_restore_stage *stage=context;
    const scene_npc_checkpoint_restore_entry *e;
    if(!stage||index>=stage->count)return RF_RANGE;e=stage->entries+index;
    /* Hidden actors retain their saved pose but do not participate in live
     * body contacts; pair clearance only applies to active solid actors. */
    *retired=e->saved.retired||e->saved.dead_pose==2||!!(e->saved.flags&0x4000u);if(*retired)return RF_OK;
    if(e->slot>=campaign_npc_body_count)return RF_FORMAT;
    memset(p,0,sizeof(*p));
    {int status=scene_npc_checkpoint_restore_spheres(e,&p->spheres,&p->count);if(status)return status;}
    memcpy(p->position,e->published,12);memcpy(p->basis,e->basis,36);return RF_OK;
}
/* An authored spawn can contain overlapping NPC collision spheres (L6S3's
 * clustered monitor actors do). Permit only a fresh-boot overlap that already
 * exists in the live authored bodies and has not deepened by over .01 unit. */
static int scene_npc_checkpoint_restore_original_overlap(void *context,uint32_t i,uint32_t j,
    uint32_t a,uint32_t b,double candidate_distance2,double radius)
{
    const scene_npc_checkpoint_restore_stage *stage=context;
    const campaign_npc_body *first,*second;const rf_physics_sphere *one,*two;
    double center[2][3],distance2=0;uint32_t axis,k;
#ifndef RF_IMAGE_XBOX_NATIVE
    {const char *enabled=getenv("RF_REPLAY_WORLD_RESTORE_PROBE");
     if(enabled&&!strcmp(enabled,"1")&&stage&&i<stage->count&&j<stage->count)
         printf("WORLD_RESTORE_NPC_PAIR_UID first%u second%u fresh%u depth%.6g retired%u/%u dead%u/%u health%.6g/%.6g move%u/%u event%u/%u follow%u/%u\n",
             stage->entries[i].saved.uid,stage->entries[j].saved.uid,
             stage->fresh_boot,radius-sqrt(candidate_distance2),
             stage->entries[i].saved.retired,stage->entries[j].saved.retired,
             stage->entries[i].saved.dead_pose,stage->entries[j].saved.dead_pose,
             (double)stage->entries[i].saved.health,(double)stage->entries[j].saved.health,
             stage->entries[i].saved.move.active,stage->entries[j].saved.move.active,
             stage->entries[i].saved.move.event,stage->entries[j].saved.move.event,
             stage->entries[i].saved.move.follow,stage->entries[j].saved.move.follow);}
#endif
    if(!stage||i>=stage->count||j>=stage->count||
       stage->entries[i].slot>=campaign_npc_body_count||stage->entries[j].slot>=campaign_npc_body_count)return 0;
    /* Ordinary movement can produce broad-sphere overlaps. This
     * includes same-order Goto_Player actors (8696/8697) and a pursuing
     * combatant contacting an idle actor (L1S1 8462/9389). Continuation
     * may preserve same-order stacking up to 0.9 unit and pursuit contacts
     * up to 0.75 unit, both on fresh load and in-place staging. */
    {const rf_npc_checkpoint_record *a=&stage->entries[i].saved,*b=&stage->entries[j].saved;
     if(a->move.active&&b->move.active&&a->move.follow==1&&b->move.follow==1&&
        a->move.event&&a->move.event==b->move.event&&radius-sqrt(candidate_distance2)<=.9)return 1;}
    /* Two actors can finish a live fight with overlapping death bodies. Keep
     * that shallow terminal contact across a load without admitting arbitrary
     * stacked actors or a live actor embedded in a corpse. */
    {const rf_npc_checkpoint_record *a=&stage->entries[i].saved,*b=&stage->entries[j].saved;
     if(!a->retired&&!b->retired&&a->dead_pose&&b->dead_pose&&a->health<=0&&b->health<=0&&
        radius-sqrt(candidate_distance2)<=.75)return 1;}
    /* An active pursuer can brush a fallen actor during ordinary play. The
     * L1S1 8456/9391 save records 0.122 unit of such contact; preserve only
     * shallow pursuit/corpse contact, never an arbitrary live/dead stack. */
    {const rf_npc_checkpoint_record *a=&stage->entries[i].saved,*b=&stage->entries[j].saved;
     if(!a->retired&&!b->retired&&
        (((a->move.active&&a->move.follow==2)&&!a->dead_pose&&a->health>0&&b->dead_pose&&b->health<=0)||
         ((b->move.active&&b->move.follow==2)&&!b->dead_pose&&b->health>0&&a->dead_pose&&a->health<=0))&&
        radius-sqrt(candidate_distance2)<=.25)return 1;}
    {const rf_npc_checkpoint_record *a=&stage->entries[i].saved,*b=&stage->entries[j].saved;
     if(!a->retired&&!b->retired&&!a->dead_pose&&!b->dead_pose&&a->health>0&&b->health>0&&
        ((a->move.active&&a->move.follow==2)||(b->move.active&&b->move.follow==2))&&
        radius-sqrt(candidate_distance2)<=.75)return 1;}
    if(!stage->fresh_boot||stage->entries[i].saved.airborne.present||stage->entries[j].saved.airborne.present)return 0;
    first=campaign_npc_bodies+stage->entries[i].slot;second=campaign_npc_bodies+stage->entries[j].slot;
    if(a>=first->body.spheres.count||b>=second->body.spheres.count)return 0;
    one=first->body.spheres.items+a;two=second->body.spheres.items+b;
    for(axis=0;axis<3;axis++){
        center[0][axis]=first->body.state.position[axis];center[1][axis]=second->body.state.position[axis];
        for(k=0;k<3;k++){
            center[0][axis]+=(double)one->center[k]*first->body.state.orientation[k*3+axis];
            center[1][axis]+=(double)two->center[k]*second->body.state.orientation[k*3+axis];
        }
        if(!isfinite(center[0][axis])||!isfinite(center[1][axis]))return 0;
        distance2+=(center[0][axis]-center[1][axis])*(center[0][axis]-center[1][axis]);
    }
    return distance2<radius*radius&&sqrt(candidate_distance2)+.01>=sqrt(distance2);
}
static inline void scene_npc_checkpoint_restore_discard_storage(scene_npc_checkpoint_restore_stage **stage,const rf_prepare_storage *storage)
{
    uint32_t i;
    if(stage&&*stage){
        for(i=0;i<(*stage)->count;i++)scene_medic_syringe_restore_discard(&(*stage)->entries[i].medic_child);
        (void)rf_prepare_storage_release(storage,NULL,NULL,*stage,RF_OK);*stage=NULL;
    }
}
static inline void scene_npc_checkpoint_restore_discard(scene_npc_checkpoint_restore_stage **stage)
{scene_npc_checkpoint_restore_discard_storage(stage,NULL);}
static int scene_npc_checkpoint_restore_eye(uint32_t slot,const scene_npc_checkpoint_restore_entry *e,float result[3])
{
    rf_eye_input input={0};const campaign_npc_eye_class *eye;const rf_entity_pose *pose;
    float tag[12],placed[12];int status;
    if(!campaign_npc_eyes||slot>=campaign_poses.count||!campaign_poses.items)return RF_RANGE;
    eye=campaign_npc_eyes+e->saved.class_id;pose=campaign_poses.items+slot;
    memcpy(input.position,e->published,12);memcpy(input.orientation,e->basis,36);
    memcpy(input.standing_offset,eye->offsets,12);memcpy(input.crouching_offset,eye->offsets+3,12);
    input.flags=campaign_seeds.classes[e->saved.class_id].physics.flags2;input.eye_tag=eye->tag;
    status=rf_eye_position(&input,result);if(status!=RF_NOT_FOUND)return status;
    if(eye->parent<0||(uint32_t)eye->parent>=pose->bone_count||!pose->matrices)return RF_RANGE;
    status=rf_model_compose_transform(eye->local,e->matrices?e->matrices[eye->parent]:pose->matrices[eye->parent],tag);
    if(!status)status=rf_model_place_tag(tag,e->basis,e->published,placed);
    if(!status)memcpy(result,placed+9,12);return status;
}
/* Motion IDs index the identity-bound catalog. Envelopes/markers remain authored
 * resources; only playback scalars and resource reference counts are mutable. */
static int scene_npc_checkpoint_restore_clip(uint32_t skeleton,int32_t id,uint32_t owners)
{
    const rf_entity_playback_model *model;uint32_t cache;
    if(id<0||!campaign_playback_resources.models||skeleton>=campaign_playback_resources.model_count||
       !campaign_motion_catalog.models||skeleton>=campaign_motion_catalog.model_count)return RF_RANGE;
    model=campaign_playback_resources.models+skeleton;
    if(!model->resources||!model->cache_ids||(uint32_t)id>=model->count||
       (uint32_t)id>=campaign_motion_catalog.models[skeleton].count||!campaign_motion_catalog.models[skeleton].items)return RF_RANGE;
    cache=model->cache_ids[id];
    if(!campaign_npc_motion_data||cache>=campaign_npc_motion_count||!campaign_npc_motion_data[cache]||
       campaign_motion_catalog.models[skeleton].items[id].file.resident!=campaign_npc_motion_data[cache]||
       model->resources[id].references<0||model->resources[id].references>INT32_MAX-(int32_t)(owners*16u))return RF_RANGE;
    return RF_OK;
}
static inline int scene_npc_checkpoint_restore_prepare_mode_storage_accounted(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,uint32_t fresh_boot,scene_npc_checkpoint_restore_stage **out,
    const scene_npc_seat_checkpoint_stage *seats,const rf_npc_checkpoint_rng *rng,uint32_t flight_profile,const rf_prepare_memory *memory,const rf_prepare_storage *storage)
{
    scene_npc_checkpoint_restore_stage *stage=NULL;rf_npc_checkpoint_record current;
    unsigned char identity[32]={0},wire[RF_NPC_CHECKPOINT_HEADER+RF_NPC_CHECKPOINT_ROW_MAX];
    uint32_t i,j,expected=0,written,spread_seen=0,spread_saved=0,pain_seen=0,pain_random=0;uint64_t bytes;unsigned char *bone_storage;int status;
    const char *detail="preflight";
    memset(rf_scene_npc_restore_reject,0,sizeof(rf_scene_npc_restore_reject));
    rf_scene_npc_restore_reject[1]=rf_scene_npc_restore_reject[2]=UINT32_MAX;
    rf_scene_npc_restore_reject[5]=count;
    if(flight_profile>2||(flight_profile&&!rng)||(rng&&(rng->present>1||(!rng->present&&rng->value)||(!flight_profile&&rng->present)))||fresh_boot>1||!out||*out||!place||!catalog||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||now<0||now>RF_TIMER_PERIOD)
        return scene_npc_checkpoint_restore_failure("arguments",UINT32_MAX,UINT32_MAX,RF_RANGE);
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||!campaign_seeds.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return scene_npc_checkpoint_restore_failure("owners",UINT32_MAX,UINT32_MAX,RF_RANGE);
    for(i=0;i<count;i++)if(rows[i].alarm.present)for(j=0;j<i;j++)
        if(rows[j].alarm.present&&rows[j].alarm.event==rows[i].alarm.event)
            return scene_npc_checkpoint_restore_failure("duplicate_alarm_runner",i,rows[i].uid,RF_FORMAT);
    {int boundary=scene_waypoint_boundary();if(boundary)return boundary;}
    if(!fresh_boot){
        uint32_t tactical_present;
        status=scene_tactical_live_present(&tactical_present);if(status)return status;
        if(tactical_present||scene_tactical_cursor)return RF_NOT_FOUND;
        for(i=0;i<count;i++)if(rows[i].tactical.present)return RF_NOT_FOUND;
        uint32_t blind_outgoing,blind_incoming;
        status=scene_blind_live_present(combat_frame==UINT32_MAX?0:combat_frame,&blind_outgoing);if(status)return status;
        status=scene_blind_incoming_present(rows,count,&blind_incoming);if(status)return status;
        if(blind_outgoing||blind_incoming)return RF_NOT_FOUND;
        uint32_t outgoing_one_way,incoming_one_way;
        status=scene_patrol_one_way_live_present(&outgoing_one_way);
        if(status)return scene_npc_checkpoint_restore_failure("standalone_one_way_owner",UINT32_MAX,UINT32_MAX,status);
        status=scene_patrol_one_way_incoming_present(rows,count,&incoming_one_way);
        if(status||outgoing_one_way||incoming_one_way)
            return scene_npc_checkpoint_restore_failure("standalone_one_way",UINT32_MAX,UINT32_MAX,status?status:RF_NOT_FOUND);
        uint32_t tags_present;
        status=scene_waypoint_live_present(&tags_present);if(status||tags_present)return status?status:RF_NOT_FOUND;
        for(i=0;i<count;i++)if(rows[i].route.patrol&1){
            for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
            if(j==campaign_npc_body_count)return RF_FORMAT;
            status=scene_waypoint_actor_events(j,&tags_present);if(status||tags_present)return status?status:RF_NOT_FOUND;
        }
        uint32_t noise_present;
        status=scene_noise_live_present(&noise_present);
        if(status||noise_present)return scene_npc_checkpoint_restore_failure("standalone_noise_owner",UINT32_MAX,UINT32_MAX,status?status:RF_NOT_FOUND);
        for(i=0;i<count;i++)if(rows[i].ai_mode==6)
            return scene_npc_checkpoint_restore_failure("standalone_noise",i,rows[i].uid,RF_NOT_FOUND);
        uint32_t dwell_present;
        status=scene_patrol_dwell_live_present(&dwell_present);
        if(status||dwell_present)return scene_npc_checkpoint_restore_failure("standalone_dwell_owner",UINT32_MAX,UINT32_MAX,status?status:RF_NOT_FOUND);
        for(i=0;i<count;i++)if(rows[i].route.flags&64)
            return scene_npc_checkpoint_restore_failure("standalone_dwell",i,rows[i].uid,RF_NOT_FOUND);
        uint32_t alarm_present,reload_present;
        status=scene_alarm_runner_live_present(&alarm_present);
        if(status||alarm_present)return scene_npc_checkpoint_restore_failure("standalone_alarm_owner",UINT32_MAX,UINT32_MAX,status?status:RF_NOT_FOUND);
        for(i=0;i<count;i++)if(rows[i].alarm.present)
            return scene_npc_checkpoint_restore_failure("standalone_alarm",i,rows[i].uid,RF_NOT_FOUND);
        status=scene_npc_reload_live_present(combat_frame==UINT32_MAX?0:combat_frame,&reload_present);
        if(status||reload_present)return scene_npc_checkpoint_restore_failure("standalone_reload_owner",UINT32_MAX,UINT32_MAX,status?status:RF_NOT_FOUND);
        for(i=0;i<campaign_npc_body_count;i++)if(scene_guard_retreat_active(campaign_npc_bodies+i)||campaign_npc_bodies[i].guard_retreat_fault)
            return scene_npc_checkpoint_restore_failure("standalone_guard_owner",i,(uint32_t)campaign_seeds.records.items[i].record.uid,RF_NOT_FOUND);
        for(i=0;i<count;i++)if(rows[i].guard_retreat.present)
            return scene_npc_checkpoint_restore_failure("standalone_guard",i,rows[i].uid,RF_NOT_FOUND);
        for(i=0;i<count;i++)if(rows[i].reload_present)
            return scene_npc_checkpoint_restore_failure("standalone_reload",i,rows[i].uid,RF_NOT_FOUND);
        for(i=0;i<count;i++)if(rows[i].airborne.present)
            return scene_npc_checkpoint_restore_failure("standalone_airborne",i,rows[i].uid,RF_NOT_FOUND);
        for(i=0;i<campaign_npc_body_count;i++)if(scene_npc_airborne_potential(i,campaign_npc_bodies+i))
            return scene_npc_checkpoint_restore_failure("standalone_airborne_owner",i,
                (uint32_t)campaign_seeds.records.items[i].record.uid,RF_NOT_FOUND);
    }
    /* Ordinary quickload reconstructs a fresh scene. The component-only
     * in-session API cannot overwrite slots whose poses belong to corpses. */
    if(campaign_live_corpse_count)return scene_npc_checkpoint_restore_failure("live_corpses",UINT32_MAX,UINT32_MAX,RF_NOT_FOUND);
    status=rf_npc_checkpoint_encode(identity,catalog,NULL,0,wire,sizeof(wire),&written);
    if(status)return scene_npc_checkpoint_restore_failure("catalog",UINT32_MAX,UINT32_MAX,status);
    for(i=0;i<campaign_npc_body_count;i++){
        /* Fresh boot may still display its initialization clip; it may not
         * silently discard active routes, combat or unsupported gameplay. */
        /* Pre-boss saves omit only the new hidden generated dependency. The
         * world phase join must still require a living original fighter. */
        if(fresh_boot&&scene_masako_seed_is(i)){
            for(j=0;j<count;j++)if(rows[j].uid==SCENE_MASAKO_GENERATED_UID)break;
            if(j==count)continue;
        }
        status=scene_npc_checkpoint_row_profile(i,now,&current,fresh_boot,0,1);
        if(status==RF_NOT_FOUND)continue;
        if(status)return scene_npc_checkpoint_restore_failure("resident_capture",i,
            (uint32_t)campaign_seeds.records.items[i].record.uid,status);
        expected++;rf_scene_npc_restore_reject[4]=expected;
    }
    if(expected!=count)return scene_npc_checkpoint_restore_failure("row_count",UINT32_MAX,UINT32_MAX,RF_FORMAT);
    bytes=sizeof(*stage)+(uint64_t)count*sizeof(*stage->entries);
    for(i=0;i<count;i++){
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
        if(j==campaign_npc_body_count)return scene_npc_checkpoint_restore_failure("pose_identity",i,rows[i].uid,RF_FORMAT);
        if(!rows[i].animation_present&&(!scene_medic_syringe_qualified(campaign_npc_bodies+j)||rows[i].retired))continue;
        if(!campaign_poses.items||j>=campaign_poses.count||campaign_poses.items[j].bone_count>50)
            return scene_npc_checkpoint_restore_failure("pose_storage",i,rows[i].uid,RF_FORMAT);
        bytes+=(uint64_t)campaign_poses.items[j].bone_count*48+((campaign_poses.items[j].bone_count*2+3u)&~3u);
    }
    if(bytes>budget||bytes>UINT32_MAX)return scene_npc_checkpoint_restore_failure("budget",UINT32_MAX,UINT32_MAX,RF_RANGE);
    {void *allocation=NULL;status=rf_prepare_storage_allocate(storage,memory,NULL,(size_t)bytes,&allocation);
     if(status)return scene_npc_checkpoint_restore_failure("allocation",UINT32_MAX,UINT32_MAX,status);stage=allocation;}stage->count=count;stage->bytes=(uint32_t)bytes;stage->now=now;stage->fresh_boot=fresh_boot;
    stage->flight_profile=flight_profile;stage->flight_joined=!flight_profile;stage->alarm_joined=1;stage->blind_joined=!fresh_boot;stage->tactical_joined=1;stage->pursuit_return_joined=1;
    stage->crouch_joined=1;
    stage->patrol_events_joined=!scene_waypoint_events.handle_count;
    stage->patrol_binding_generation=scene_waypoint_events.generation;
    for(i=0;i<count;i++)if(rows[i].alarm.present)stage->alarm_joined=0;
    if(rng){stage->flight_rng.present=rng->present;stage->flight_rng.value=rng->value;}
    scene_medic_flight_rng_capture(&stage->flight_rng_before);
    bone_storage=(unsigned char*)(stage->entries+count);
    for(i=0;i<count;i++){
        detail="row";
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body *o;
        const rf_entity_creation_vitals_class *vitals;rf_checkpoint_placement placement={0};
        rf_motion_playback_resource clip;rf_entity_playback_model *model;float angles[3]={0};int32_t id;uint32_t cls,seated=0;
        status=rf_npc_checkpoint_record_validate(rows+i,catalog);if(status)goto fail;
        detail="row_identity";
        if(i&&rows[i-1].uid>=rows[i].uid){status=RF_FORMAT;goto fail;}
        for(j=0;j<campaign_npc_body_count;j++)if(campaign_npc_bodies[j].persistence_registered&&
            (uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
        if(j==campaign_npc_body_count){status=RF_FORMAT;goto fail;}o=campaign_npc_bodies+j;cls=campaign_seeds.items[j].class_index;
        if(rows[i].dead_pose==2&&(!fresh_boot||!flight_profile||
           scene_robot_death_profile_at(j)<0||!scene_robot_death_identity(j,o->registration.handle)||
           !o->body.allocated_bytes||!campaign_model_owners||j>=campaign_model_owner_count||
           campaign_model_owners[j].owned)){status=RF_FORMAT;goto fail;}
        if(cls!=rows[i].class_id||cls>=campaign_seeds.class_count||!campaign_seeds.classes){status=RF_FORMAT;goto fail;}
        detail="medic_flight";
        if((rows[i].flight.entered&&(!rng||!rng->present))||(rows[i].flight.present&&!flight_profile)||
           (flight_profile&&o->flight.qualified==1&&scene_medic_flight_legacy_owner(j)&&!rows[i].retired&&!rows[i].flight.present)||
           (rows[i].retired&&rows[i].flight.present)){status=RF_FORMAT;goto fail;}
        if(rows[i].flight.present&&!scene_medic_flight_legacy_owner(j)&&flight_profile<2){status=RF_FORMAT;goto fail;}
        status=scene_medic_flight_checkpoint_prepare(&rows[i].flight,now,&e->flight);if(status)goto fail;
        detail="guard_retreat";
        if(rows[i].guard_retreat.present&&(!flight_profile||!rng||!rng->present)){status=RF_FORMAT;goto fail;}
        status=scene_guard_retreat_checkpoint_prepare(j,rows+i,now,combat_frame==UINT32_MAX?0:combat_frame,&e->guard_retreat);if(status)goto fail;
        if(rows[i].guard_retreat.present){
            if(spread_seen&&spread_saved!=rows[i].guard_retreat.spread_rng){status=RF_FORMAT;goto fail;}
            spread_seen=1;spread_saved=rows[i].guard_retreat.spread_rng;
        }
        if(rng&&rng->present&&rows[i].pain.present&&rng->value!=rows[i].pain.random){status=RF_FORMAT;goto fail;}
        detail="actor_route";
        status=scene_actor_route_admit(o,&rows[i].route,&e->move_path);if(status)goto fail;
        e->route_before=rf_scene_defeated_actors.routes[o->persistence_slot];
        e->route_owner_snapshot=o->patrol.enabled||rows[i].route.flags||e->route_before.flags;
        if(e->route_owner_snapshot){
            e->owner_before=*o;
            if(!campaign_poses.items||j>=campaign_poses.count){status=RF_FORMAT;goto fail;}
            e->pose_before=campaign_poses.items[j];
        }
        detail="medic_state";
        status=scene_medic_state_admit(o,&rows[i].medic);if(status)goto fail;
        e->medic_owner_snapshot=scene_medic_syringe_qualified(o);
        e->effective_ai_mode=rows[i].ai_mode;
        if((e->medic_owner_snapshot||o->flight.qualified==1)&&!rows[i].retired&&!rows[i].dead_pose&&rows[i].health>0&&
           rows[i].ai_mode==0&&(o->medic.default_action==2||o->flight.qualified==1)){
            rf_level_entity_ai authored;
            /* Older NPC capture retained raw calloc0 before medic placed-AI
             * initialization existed. All audited explicit living setters
             * use nonzero modes; dead-seat release0 is excluded above. */
            status=rf_level_entity_ai_read(campaign_seeds.records.items+j,&authored);if(status)goto fail;
            if(authored.mode==1)e->effective_ai_mode=2;
        }
        if(e->medic_owner_snapshot){
            /* Capturing current owner state is not admitting it into RFNC.
             * Pending/residual ownership must remain byte-identical through
             * candidate preparation, including target/deadline/full handles. */
            e->owner_before=*o;
            if(!campaign_poses.items||j>=campaign_poses.count){status=RF_FORMAT;goto fail;}
            e->pose_before=campaign_poses.items[j];
            if(stage->bytes>budget){status=RF_RANGE;goto fail;}
            status=scene_medic_syringe_restore_prepare_accounted(o,rows[i].medic.child_state,
                rows[i].medic.child_health,rows[i].retired,budget-stage->bytes,&e->medic_child,memory);
            if(status)goto fail;
            if(e->medic_child.allocated_bytes>budget-stage->bytes){status=RF_RANGE;goto fail;}
            stage->bytes+=e->medic_child.allocated_bytes;
        }
        detail="configured_item";
        status=scene_death_item_owner_admit(o,&rows[i].item_drop);if(status)goto fail;
        if(rows[i].item_drop.state==RF_CAMPAIGN_ITEM_DROP_AVAILABLE){
            int kind=scene_death_item_kind(rows[i].item_drop.stable_definition_id);
            if(!scene_actor_collision_owner||!scene_actor_collision_owner->pickup_resources||kind<0){status=RF_FORMAT;goto fail;}
            status=scene_death_item_resource_admit(scene_actor_collision_owner->pickup_resources+kind-1,
                rows[i].item_drop.stable_definition_id);if(status)goto fail;
        }
        detail="capek_shield_broken";
        if(rows[i].capek_shield_broken&&!scene_capek_shield_movement_owner(o)){status=RF_FORMAT;goto fail;}
        vitals=&campaign_seeds.classes[cls].vitals;
        detail="vitals";
        if(rows[i].health>(vitals->health<0?100:vitals->health)||rows[i].armor>vitals->armor){status=RF_FORMAT;goto fail;}
        e->item_attempted_before=o->death_item_attempted;e->item_fault_before=o->death_item_fault;
        e->item_drop_before=rf_scene_defeated_actors.item_drops[o->persistence_slot];
        e->slot=j;e->handle=o->registration.handle;e->registered=o->registration.view!=NULL;e->saved=rows[i];e->idle=-1;
        status=scene_npc_checkpoint_restore_spheres(e,&placement.spheres,&placement.count);if(status)goto fail;
        detail="base_speed";
        if(!campaign_npc_movement_configs){status=RF_RANGE;goto fail;}
        e->base_speed=scene_npc_effective_base_speed(o,cls,rows[i].capek_shield_broken);
        if(rows[i].pain.present){
            if(pain_seen&&pain_random!=rows[i].pain.random){status=RF_FORMAT;goto fail;}
            pain_seen=1;pain_random=rows[i].pain.random;
        }
        detail="controller_handle";
        status=scene_npc_checkpoint_controller_handle(rows[i].controller_uid,&e->controller_handle);if(status)goto fail;
        if(rows[i].move.active){
            uint32_t mode,k;
            detail="move_binding";
            if(rows[i].alarm.present){mode=0;status=RF_OK;}
            else if(rows[i].route.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT)
                status=scene_patrol_binding(j,&e->move_path,&mode);
            else status=scene_npc_checkpoint_move_binding(rows[i].move.event,e->handle,rows[i].move.follow,&e->move_path,&mode);
            if(status)goto fail;
            if(mode!=rows[i].move.path_mode||e->move_path.count!=rows[i].move.path_count){status=RF_FORMAT;goto fail;}
            if(rows[i].route.flags&&e->move_path.count&&memcmp(rows[i].move.target,
                campaign_navigation.nodes[rf_level_waypoint_node(&e->move_path,rows[i].route.cursor)].candidate.position,12)){
                status=RF_FORMAT;goto fail;
            }
            for(k=0;k<rows[i].move.retained_count;k++){
                uint32_t ordinal=rows[i].move.retained_nodes[k];
                if(ordinal==UINT32_MAX||ordinal==UINT32_MAX-1)continue;
                if(!campaign_navigation.references||!ordinal||ordinal>campaign_navigation.count||
                   !campaign_navigation.references[ordinal-1].candidate){status=RF_FORMAT;goto fail;}
                e->move_nodes[k]=campaign_navigation.references[ordinal-1].candidate;
            }
        }
        if(rows[i].look.active){
            detail="look_binding";
            status=scene_npc_checkpoint_look_binding(rows[i].look.event,e->handle,
                rows[i].look.target_uid,rows[i].look.position);
            if(status)goto fail;
        }
        if(rows[i].shot_count){
            detail="single_binding";
            for(uint32_t k=0;k<rows[i].shot_count;k++){
                const rf_npc_checkpoint_shot *shot=rows[i].shots+k;
                status=scene_npc_checkpoint_single_binding(shot->event,e->handle,shot->mode);if(status)goto fail;
                if(shot->mode==1?(!scene_tankbot_missile_resources||rows[i].secondary!=
                    rf_weapon_name_find(&campaign_weapon_supply.names,"Tankbot Missile")):rows[i].primary<0){status=RF_FORMAT;goto fail;}
            }
            if(spread_seen&&spread_saved!=rows[i].shot_rng){status=RF_FORMAT;goto fail;}
            spread_seen=1;spread_saved=rows[i].shot_rng;
        }
        if(rows[i].combat.active){
            if(rows[i].combat.active==1){
                detail="shoot_binding";
                status=scene_npc_checkpoint_shoot_binding(rows[i].combat.event,e->handle,rows[i].combat.point);
                if(status)goto fail;
            }else if(rows[i].combat.active==2){
                detail="attack_target";
                status=scene_npc_checkpoint_attack_target_handle(rows[i].combat.event,
                    rows,count,&e->attack_target_handle);
                if(status)goto fail;
            }else if(rows[i].combat.active==4){
                detail="reactive_target";
                status=scene_npc_checkpoint_living_npc_handle(rows[i].combat.event,
                    rows,count,&e->attack_target_handle);
                if(status)goto fail;
                if(e->attack_target_handle==e->handle){status=RF_FORMAT;goto fail;}
            }else{
                detail="generated_attack_target";
                status=scene_turret_generated_attack_handle(rows[i].combat.event,&e->attack_target_handle);
                if(status)goto fail;
            }
            if(spread_seen&&spread_saved!=rows[i].combat.spread_rng){status=RF_FORMAT;goto fail;}
            spread_seen=1;spread_saved=rows[i].combat.spread_rng;
        }
        detail="blind_pursuit";
        status=scene_blind_candidate(j,rows+i,rows,count,combat_frame==UINT32_MAX?0:combat_frame,
            &e->blind_target,&e->blind_last_seen);if(status)goto fail;
        if(e->blind_target&&rows[i].combat.active&&e->blind_target!=e->attack_target_handle){status=RF_FORMAT;goto fail;}
        status=scene_pursuit_delay_candidate(j,rows+i,e->blind_target,stage->now,&e->pursuit_delay_prepared);if(status)goto fail;
        detail="tactical";
        {scene_tactical_receipt prepared;
         status=scene_tactical_candidate(j,rows+i,rows,count,now,&prepared);if(status)goto fail;
         e->tactical_target=prepared.target;e->tactical_phase_due=prepared.phase_due;e->tactical_recovery_due=prepared.recovery_due;
         if(rows[i].tactical.present)stage->tactical_joined=0; /* Complete player/seat joins remain mandatory. */
         detail="pursuit_return";
         status=scene_pursuit_return_candidate(j,rows+i,rows,count,&prepared,&e->pursuit_return_prepared);if(status)goto fail;
         if(e->pursuit_return_prepared.phase)stage->pursuit_return_joined=0;
        }
        /* Return-only ownership also needs an old full-body/pose seal when
         * no patrol, medic or crouch owner happened to request one. */
        if(o->pursuit_return.phase||e->pursuit_return_prepared.phase||
           o->speech_motion_mask||e->saved.speech_motion_mask){
            if(o->pursuit_return.phase){
                status=scene_pursuit_return_tactical_admit(&o->pursuit_return,&o->tactical);if(status)goto fail;
                status=scene_tactical_owner(j);if(status)goto fail;
                e->pursuit_return_retire_pending=!!e->saved.retired;
            }
            if(!campaign_poses.items||j>=campaign_poses.count){status=RF_FORMAT;goto fail;}
            e->owner_before=*o;e->pose_before=campaign_poses.items[j];
        }
        detail="fresh_identity";
        if(fresh_boot){
            if(!campaign_poses.items||j>=campaign_poses.count){status=RF_FORMAT;goto fail;}
            e->owner_before=*o;e->pose_before=campaign_poses.items[j];
            if(o->persistence_slot>=rf_scene_defeated_actors.count||
               (e->registered&&(o->registration.view!=&o->view||rf_object_registry_lookup(&campaign_registry,e->handle)!=&o->registration))){status=RF_FORMAT;goto fail;}
        }else {status=scene_npc_checkpoint_row_profile(j,now,&e->before,0,0,1);if(status)goto fail;}
        if(!rows[i].retired&&!e->registered){status=RF_RANGE;goto fail;} /* Fresh actors, never resurrect freed bodies. */
        e->flags=(o->object_flags&~0x4004u)|rows[i].flags;
        detail="crouch_shape";
        status=scene_npc_checkpoint_restore_crouch(stage,e);if(status)goto fail;
        status=scene_npc_checkpoint_restore_spheres(e,&placement.spheres,&placement.count);if(status)goto fail;
        detail="drone_pose";status=scene_npc_drone_checkpoint_admit(j,o,rows+i);if(status)goto fail;
        detail="ordinary_airborne";status=scene_npc_airborne_checkpoint_admit(j,o,rows+i,fresh_boot);if(status)goto fail;
        memcpy(e->published,rows[i].position,12);angles[1]=rows[i].yaw;
        if(rows[i].drone.kind)angles[0]=rows[i].drone.pitch;
        status=rf_look_orientation(angles,e->basis);if(status)goto fail;
        detail="seat_basis";status=scene_npc_seat_checkpoint_basis(seats,rows+i,e->basis,&seated);if(status)goto fail;
        if(seated&&rows[i].airborne.present){status=RF_FORMAT;goto fail;}
        if(rows[i].drone.kind)memcpy(e->look_basis,e->basis,36); /* Canonical ordinary eye, including retired look-only state. */
        else if(rows[i].airborne.present){
            status=rf_ordinary_eye_orientation(angles,rows[i].eye_angles,e->look_basis);if(status)goto fail;
        }
        else {rf_eye_angle_state eye_angles={0};
         memcpy(eye_angles.angles_87c,rows[i].eye_angles,12);
         status=rf_eye_physics_orientation(e->basis,&eye_angles,e->look_basis);if(status)goto fail;}
        rf_motion_playback_initialize(&e->playback);
        if(rows[i].retired)continue;
        if(rows[i].dead_pose==2){
            /* This body has already died and will only be removed at the next
             * outer drain. No placement, idle clip, death choice or loot replay.
             * The retained blast center belongs to RFAP, not this inert pose. */
            e->unowned_motion_mask|=SCENE_ROBOT_DEATH_CHECKPOINT_UNJOINED;e->body=o->body.state;e->room=o->room;
            e->selection=o->selection;e->support.material=-1;
            memcpy(e->body.position,e->published,12);memcpy(e->body.next_position,e->published,12);
            memcpy(e->body.orientation,e->basis,36);memcpy(e->body.next_orientation,e->basis,36);
            memset(e->body.velocity,0,12);memcpy(e->room.query_position,e->published,12);
            status=rf_physics_spheres_bounds(placement.spheres,placement.count,
                e->body.position,&e->body.bounds);if(status)goto fail;
            status=rf_physics_tensor_world(e->body.local_tensor,e->basis,e->body.world_tensor);if(status)goto fail;
            memcpy(e->eye,e->published,12);continue;
        }
        if(!campaign_model_owners||j>=campaign_model_owner_count||!campaign_poses.items||j>=campaign_poses.count){status=RF_RANGE;goto fail;}
        e->movement=o->movement;e->movement_slot=o->movement_slot;
        detail="movement_slot";
        if(rows[i].movement_present){
            uint32_t slot=rows[i].movement_slot;
            if(rows[i].capek_shield_broken&&!scene_capek_broken_movement_slot(slot)){status=RF_FORMAT;goto fail;}
            if(!campaign_npc_movement_configs||slot>=16||(slot&&!(campaign_modes[slot].enabled&255u))||
               (rows[i].support_uid&&campaign_modes[slot].index!=1)){status=RF_FORMAT;goto fail;}
            status=scene_npc_resolve_speed(o,cls,rows[i].capek_shield_broken,
                (int32_t)rows[i].speed_mode,-1,&e->movement);if(status)goto fail;
            if(e->movement.speed<0){status=RF_FORMAT;goto fail;}e->movement_slot=slot;e->movement_resolved=1;
        }else if(!rows[i].dead_pose&&scene_capek_shield_movement_owner(o)){
            /* RFNC1..12 has no movement continuation and no break latch.
             * Use constructor normal/hover for this exact owner, not a later
             * resident broken speed/run state. Other legacy owners unchanged. */
            status=scene_npc_resolve_speed(o,cls,0,1,-1,&e->movement);if(status)goto fail;
            e->movement_slot=(campaign_modes[12].enabled&255u)?12u:0u;e->movement_resolved=1;
        }
        e->body=o->body.state;
        if(e->movement_resolved){
            e->body.coefficients[1]=e->movement.response;
            if(campaign_modes[e->movement_slot].index==3||campaign_modes[e->movement_slot].index==8)e->body.flags|=1u;
            else if(campaign_modes[e->movement_slot].index==1)e->body.flags&=~1u;
        }
        memcpy(e->body.position,rows[i].position,12);memcpy(e->body.next_position,rows[i].position,12);
        if(seated)e->body.flags&=~(1u|0x400000u);
        memcpy(e->body.orientation,e->basis,36);memcpy(e->body.next_orientation,e->basis,36);memset(e->body.velocity,0,12);
        detail="body_prepare";
        status=rf_physics_spheres_bounds(placement.spheres,placement.count,e->body.position,&e->body.bounds);if(status)goto fail;
        status=rf_physics_tensor_world(e->body.local_tensor,e->basis,e->body.world_tensor);if(status)goto fail;
        status=rf_physics_publish_position(&e->body,e->published,&e->flags);if(status)goto fail;
        detail="script_physics";
        status=scene_npc_physics_checkpoint_prepare(o,rows+i,&e->body,&e->flags);if(status)goto fail;
        scene_npc_drone_checkpoint_prepare(j,o,rows+i,&e->body,&e->flags);
        scene_npc_airborne_checkpoint_prepare(rows+i,&e->body,&e->flags);
        if(rows[i].guard_retreat.present){
            memcpy(e->body.vector_c8,rows[i].guard_retreat.angular,12);
            memcpy(e->body.vector_ec,rows[i].guard_retreat.torque,12);
        }
        detail="candidate_shape";
        status=scene_npc_checkpoint_restore_spheres(e,&placement.spheres,&placement.count);if(status)goto fail;
        placement.query_flags=(((rows[i].airborne.present||rows[i].drone.kind==1)?e->body.state_124:o->body.state.state_124)&~0x1000u)|4u;
        memcpy(placement.position,e->published,12);memcpy(placement.basis,e->basis,36);
        detail="placement";status=place(context,e->slot,e->handle,e->saved.uid,&placement,&e->room,&e->support);if(status)goto fail;
        detail="support_admission";
        if(rows[i].airborne.present&&(e->support.handle||e->support.material!=-1||e->body.flags!=0x80000079u)){status=RF_FORMAT;goto fail;}
        if(memcmp(e->room.query_position,e->published,12)||
           (!!e->support.handle!=!!rows[i].support_uid)||!e->room.room||
           e->support.material< -1||(!placement.count&&e->support.material!=-1)||
           (!seated&&!rows[i].airborne.present&&!scene_npc_physics_checkpoint_suspended(rows+i)&&placement.count&&e->support.material==-1&&
            (((rows[i].physics.present||rows[i].drone.kind==1)?(e->body.flags&(1u|0x400000u)):(o->body.state.flags&(1u|0x400000u)))||
             ((rows[i].physics.present||rows[i].drone.kind==1)?rows[i].support_uid:o->support.handle)))){status=RF_FORMAT;goto fail;}e->room.flags=e->flags;
        if(rows[i].support_uid){e->body.flags&=~1u;e->body.flags|=0x400000u;}
        detail="motion_selection";
        status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&e->selection);if(status)goto fail;
        if(rows[i].primary>=0){status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,rows[i].primary,&e->selection);if(status)goto fail;}
        if(e->selection.mapping.skeleton!=campaign_poses.items[j].skeleton){status=RF_FORMAT;goto fail;}
        detail="noise_investigation";
        status=scene_noise_candidate(j,rows+i,&e->selection);if(status)goto fail;
        if(rows[i].ai_mode==6&&e->movement.speed<=0){status=RF_FORMAT;goto fail;}
        detail="alarm_runner";
        status=scene_alarm_runner_candidate(j,rows+i,&e->selection);if(status)goto fail;
        detail="patrol_candidate";
        status=scene_patrol_candidate(j,rows+i,&e->selection);if(status)goto fail;
        e->dwell_deadline=(rows[i].route.flags&64)?scene_patrol_frame()+rows[i].route.dwell_remaining:0;
        if(rows[i].alarm.present&&e->movement.speed<=0){status=RF_FORMAT;goto fail;}
        detail="reload_owner";
        status=scene_npc_reload_saved(j,rows+i,&e->selection,1,&e->reload_deadline);if(status)goto fail;
        if((rows[i].blind_present||rows[i].ai_mode==6||rows[i].reload_present||rows[i].guard_retreat.present||rows[i].flight.entered||rows[i].alarm.present||(rows[i].route.flags&64))&&seated){status=RF_FORMAT;goto fail;}
        detail="pain_owner";
        status=scene_npc_pain_checkpoint_prepare(j,rows+i,&e->selection,now,e->pain_deadlines);if(status)goto fail;
        id=e->selection.mapping.states[0];
        detail="animation_restore";
        if(rows[i].speech_motion_mask&&!rows[i].animation_present){status=RF_FORMAT;goto fail;}
        if(rows[i].animation_present){
            uint32_t k,skeleton=e->selection.mapping.skeleton;
            if(!campaign_playback_resources.models||skeleton>=campaign_playback_resources.model_count){status=RF_RANGE;goto fail;}
            model=campaign_playback_resources.models+skeleton;
            if(rows[i].dead_pose){
                int32_t death_motion=e->selection.mapping.actions[rows[i].death_action];
                if(death_motion<0||rows[i].playback.completion.active.slots[0].motion!=death_motion||
                   !model->resources||(uint32_t)death_motion>=model->count||model->resources[death_motion].looping){status=RF_FORMAT;goto fail;}
            }
            if(rows[i].script_animation.active){
                status=scene_npc_checkpoint_restore_clip(skeleton,rows[i].script_animation.motion,count);if(status)goto fail;
                if(model->resources[rows[i].script_animation.motion].looping!=rows[i].script_animation.loop){status=RF_FORMAT;goto fail;}
            }
            for(k=0;k<3;k++){
                int32_t state=k==0?rows[i].controller.current:k==1?rows[i].controller.next:
                    rows[i].controller.override_enabled?rows[i].controller.override_state:-1;
                if(state>=0){
                    int32_t motion=e->selection.mapping.states[state];
                    if(motion>=0){status=scene_npc_checkpoint_restore_clip(skeleton,motion,count);if(status)goto fail;}
                }
            }
            if(rows[i].playback.completion.frozen&&!rows[i].script_animation.active&&!rows[i].dead_pose){
                detail="motion_frozen";status=RF_FORMAT;goto fail;
            }
            for(k=0;k<rows[i].playback.completion.active.count;k++){
                int32_t motion=rows[i].playback.completion.active.slots[k].motion;
                detail="clip";status=scene_npc_checkpoint_restore_clip(skeleton,motion,count);if(status)goto fail;
                if(!model->resources[motion].looping&&!rows[i].script_animation.active&&!rows[i].dead_pose&&
                   !scene_npc_pain_checkpoint_clip(&rows[i].pain,&e->selection,motion)&&
                   !scene_npc_speech_checkpoint_residual(j,&e->selection,&rows[i].playback,
                     rows+i,e->flags,rows[i].speech_motion_mask,motion)&&
                   !(!rows[i].reload_present&&(rows[i].combat_alert||rows[i].combat.active)&&
                     (motion==e->selection.mapping.actions[2]||motion==e->selection.mapping.actions[39]||
                      scene_crouch_action4(j,!!(rows[i].crouch.flags&1u),&e->selection,motion)))&&
                   !((rows[i].reload_present||(rows[i].combat_alert==0&&rows[i].combat.active==0))&&
                     !rows[i].retired&&rows[i].health>0&&!(e->flags&2u)&&
                     scene_npc_checkpoint_residual_weapon_clip(j,cls,rows[i].primary,&rows[i].inventory,
                         &e->selection,&rows[i].playback,motion,0,!!(rows[i].crouch.flags&1u)))&&
                   !(rows[i].pain_residual_profile&&rows[i].pain.present&&
                     !rows[i].retired&&!rows[i].dead_pose&&rows[i].health>0&&!(e->flags&2u)&&
                     scene_npc_pain_checkpoint_residual(j,cls,rows[i].primary,
                         &e->selection,&rows[i].playback,motion,0)))
                    e->unowned_motion_mask|=1u<<k;
            }
            if(!scene_npc_speech_checkpoint_mask(j,&e->selection,&rows[i].playback,
                rows+i,e->flags,rows[i].speech_motion_mask)){
                detail="speech_owner";status=RF_FORMAT;goto fail;
            }
            /* RFNC lacks the independent RFAP burn-pain action. Resolve only
             * those exact remaining slots at the existing composed join;
             * validate refuses publication while any obligation remains. */
            if(rows[i].dead_pose){
                detail="death_clip";
                status=scene_corpse_checkpoint_death_restore(rows+i,&e->selection);if(status)goto fail;
            }
            e->playback=rows[i].playback;
            if(campaign_poses.items[j].bone_count){
                rf_entity_pose candidate=campaign_poses.items[j];float displacement[3]={0};uint32_t k;
                if(!candidate.matrices||!candidate.generations){status=RF_RANGE;goto fail;}
                e->bone_count=candidate.bone_count;e->original_matrices=candidate.matrices;e->original_generations=candidate.generations;
                e->matrices=(float(*)[12])bone_storage;bone_storage+=e->bone_count*48;
                e->generations=(uint16_t*)bone_storage;bone_storage+=(e->bone_count*2+3u)&~3u;
                candidate.matrices=e->matrices;candidate.generations=e->generations;candidate.playback=e->playback;
                for(k=0;k<e->bone_count;k++)e->generations[k]=(uint16_t)(e->playback.generation-1u);
                status=rf_entity_pose_evaluate(&candidate,&campaign_skeletons,&campaign_motion_catalog,displacement);if(status)goto fail;
            }
        }else if(id>=0){
            uint32_t skeleton=e->selection.mapping.skeleton,cache;
            if(!campaign_playback_resources.models||skeleton>=campaign_playback_resources.model_count||
               !campaign_motion_catalog.models||skeleton>=campaign_motion_catalog.model_count){status=RF_RANGE;goto fail;}
            model=campaign_playback_resources.models+skeleton;
            if(!model->resources||!model->cache_ids||(uint32_t)id>=model->count||
               (uint32_t)id>=campaign_motion_catalog.models[skeleton].count||!campaign_motion_catalog.models[skeleton].items){status=RF_RANGE;goto fail;}
            cache=model->cache_ids[id];
            if(!campaign_npc_motion_data||cache>=campaign_npc_motion_count||!campaign_npc_motion_data[cache]||
               campaign_motion_catalog.models[skeleton].items[id].file.resident!=campaign_npc_motion_data[cache]||
               !model->resources[id].looping||model->resources[id].references>INT32_MAX-(int32_t)count){status=RF_RANGE;goto fail;}
            clip=model->resources[id];status=rf_motion_set_weight(&e->playback,&clip,1,0,1);if(status)goto fail;
            e->playback.completion.active.slots[0].motion=id;e->playback.completion.active.slots[0].tick=clip.comparison.start_tick;e->idle=id;
        }
        if(e->medic_owner_snapshot){
            /* A legacy row with no animation still reconstructs a real idle
             * pose privately. Its generated child must use this saved hand,
             * never the current timeline's hand until the next frame. */
            if(!e->bone_count){
                rf_entity_pose candidate=campaign_poses.items[j];float displacement[3]={0};uint32_t k;
                detail="medic_pose";
                if(!candidate.bone_count||!candidate.matrices||!candidate.generations){status=RF_RANGE;goto fail;}
                e->bone_count=candidate.bone_count;e->original_matrices=candidate.matrices;e->original_generations=candidate.generations;
                e->matrices=(float(*)[12])bone_storage;bone_storage+=e->bone_count*48;
                e->generations=(uint16_t*)bone_storage;bone_storage+=(e->bone_count*2+3u)&~3u;
                candidate.matrices=e->matrices;candidate.generations=e->generations;candidate.playback=e->playback;
                for(k=0;k<e->bone_count;k++)e->generations[k]=(uint16_t)(e->playback.generation-1u);
                status=rf_entity_pose_evaluate(&candidate,&campaign_skeletons,&campaign_motion_catalog,displacement);if(status)goto fail;
            }
            if(e->medic_child.child){
                rf_model_attachment attachment;float local[12],tag[12],transform[12];
                int32_t hand=e->medic_child.hand_tag;uint32_t skeleton=campaign_poses.items[j].skeleton;
                detail="medic_child_pose";
                if(hand<0||skeleton>=campaign_render_models.count){status=RF_FORMAT;goto fail;}
                if((uint32_t)hand<e->bone_count)memcpy(tag,e->matrices[hand],48);
                else {
                    status=rf_model_file_attachment(&campaign_render_models.items[skeleton].file,0,(uint32_t)hand-e->bone_count,&attachment);if(status)goto fail;
                    if(attachment.parent<0||(uint32_t)attachment.parent>=e->bone_count){status=RF_FORMAT;goto fail;}
                    status=rf_model_attachment_transform(attachment.rotation,attachment.position,local);if(status)goto fail;
                    status=rf_model_compose_transform(local,e->matrices[attachment.parent],tag);if(status)goto fail;
                }
                status=rf_model_place_tag(tag,e->basis,e->published,transform);if(status)goto fail;
                status=scene_medic_syringe_pose_publish(e->medic_child.child,transform+9,transform,e->room.room);if(status)goto fail;
            }
        }
        detail="eye";status=scene_npc_checkpoint_restore_eye(j,e,e->eye);if(status)goto fail;
    }
    detail="pair_fit";status=scene_npc_checkpoint_pair_fit_allowed(count,scene_npc_checkpoint_restore_placement,stage,
        scene_npc_checkpoint_restore_original_overlap);if(status)goto fail;
    *out=stage;return RF_OK;
fail:
    scene_npc_checkpoint_restore_failure(detail,i,i<count?rows[i].uid:UINT32_MAX,status);
#ifndef RF_IMAGE_XBOX_NATIVE
    if(getenv("RF_REPLAY_WORLD_RESTORE_PROBE"))printf("WORLD_RESTORE_NPC_ROW index%u uid%u phase%s status%d\n",
        i,i<count?rows[i].uid:0,detail,status);
#else
    (void)detail;
#endif
    scene_npc_checkpoint_restore_discard_storage(&stage,storage);return status;
}
static inline int scene_npc_checkpoint_restore_prepare_mode_accounted(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,uint32_t fresh_boot,scene_npc_checkpoint_restore_stage **out,
    const scene_npc_seat_checkpoint_stage *seats,const rf_npc_checkpoint_rng *rng,uint32_t flight_profile,const rf_prepare_memory *memory)
{return scene_npc_checkpoint_restore_prepare_mode_storage_accounted(rows,count,catalog,now,place,context,budget,fresh_boot,out,seats,rng,flight_profile,memory,NULL);}
static inline int scene_npc_checkpoint_restore_prepare_mode(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,uint32_t fresh_boot,scene_npc_checkpoint_restore_stage **out,
    const scene_npc_seat_checkpoint_stage *seats,const rf_npc_checkpoint_rng *rng,uint32_t flight_profile)
{return scene_npc_checkpoint_restore_prepare_mode_accounted(rows,count,catalog,now,place,context,budget,fresh_boot,out,seats,rng,flight_profile,NULL);}
static inline int scene_npc_checkpoint_restore_prepare(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,scene_npc_checkpoint_restore_stage **out)
{
    uint32_t i;int status;scene_medic_flight_rng live_rng;
    if((count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    /* NPC-only replacement has no shared receipt/player composition. Refuse
     * old live nonconstructor ownership before staging, not during capture or
     * ordinary QuickLoad. Untouched qualification remains legacy eligible. */
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    {uint32_t present;status=scene_tactical_live_present(&present);if(status)return status;
     if(present||scene_tactical_cursor)return scene_npc_checkpoint_restore_failure("standalone_tactical",UINT32_MAX,UINT32_MAX,RF_NOT_FOUND);}
    for(i=0;i<count;i++)if(rows[i].tactical_profile||rows[i].tactical.present)
        return scene_npc_checkpoint_restore_failure("standalone_tactical_row",i,rows[i].uid,RF_NOT_FOUND);
    scene_medic_flight_rng_capture(&live_rng);
    if(live_rng.present)return scene_npc_checkpoint_restore_failure("standalone_flight_rng",UINT32_MAX,UINT32_MAX,RF_NOT_FOUND);
    for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].flight.entered)
        return scene_npc_checkpoint_restore_failure("standalone_flight_owner",i,
            (uint32_t)campaign_seeds.records.items[i].record.uid,RF_NOT_FOUND);
    status=scene_npc_checkpoint_restore_prepare_mode(rows,count,catalog,now,place,context,budget,0,out,NULL,NULL,0);
    if(status)return status;
    /* Component-only restoration has no RFAP owner to discharge a slot. */
    for(i=0;i<(*out)->count;i++)if((*out)->entries[i].unowned_motion_mask){
        status=scene_npc_checkpoint_restore_failure("animation_owner",i,(*out)->entries[i].saved.uid,RF_FORMAT);
        scene_npc_checkpoint_restore_discard(out);return status;
    }
    return RF_OK;
}
/* Rebind the exact prepared generation, including the incoming target row's
 * living/standalone semantics. The paused combat frame is part of the receipt;
 * no age is recomputed or seeded at final publication. */
#include "scene_tactical_prepared.inc"
#include "scene_crouch_prepared.inc"
static int scene_blind_prepared(const scene_npc_checkpoint_restore_stage *stage,const scene_npc_checkpoint_restore_entry *e)
{
    const rf_npc_checkpoint_record *r=&e->saved;uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame,uid;float seconds;int status;
    if(!e->blind_target)return e->blind_last_seen||r->blind_present?RF_FORMAT:RF_OK;
    if(e->slot>=campaign_npc_body_count||(campaign_npc_bodies[e->slot].view.flags_810&1u)||
       !e->registered||e->blind_target==e->handle||r->blind_profile>1||r->blind_present>1||
       (!r->blind_profile&&r->blind_present)||(r->blind_profile&&!r->blind_present)||
       (!r->blind_present&&r->blind_age)||r->retired||r->dead_pose||r->health<=0||
       !r->combat_alert||(r->combat.active!=0&&r->combat.active!=2&&r->combat.active!=4))return RF_FORMAT;
    status=scene_blind_source(e->slot,r->primary,!!r->blind_present,&seconds);if(status)return status;
    if(r->blind_age>scene_blind_age_limit(seconds)||frame-e->blind_last_seen!=(r->blind_present?r->blind_age:0))return RF_FORMAT;
    uid=r->combat.active?r->combat.event:0;
    if(!uid)return r->combat.active==4||e->blind_target!=campaign_player_object.handle||
        campaign_player_object.view!=&campaign_player_view||
        rf_object_registry_lookup(&campaign_registry,e->blind_target)!=&campaign_player_object||
        rf_entity_lookup(&campaign_entities,(int32_t)e->blind_target)!=&campaign_player_view?RF_FORMAT:RF_OK;
    if(e->attack_target_handle!=e->blind_target)return RF_FORMAT;
    for(uint32_t j=0;j<stage->count;j++)if(stage->entries[j].saved.uid==uid){
        const scene_npc_checkpoint_restore_entry *v=stage->entries+j;
        if(!v->registered||v->handle!=e->blind_target||v->saved.retired||v->saved.dead_pose||
           v->saved.health<=0||v->slot>=campaign_npc_body_count||
           (campaign_npc_bodies[v->slot].view.flags_810&1u)||
           scene_npc_reload_identity(v->slot,campaign_npc_bodies+v->slot))return RF_FORMAT;
        return RF_OK;
    }
    return RF_FORMAT;
}
static int scene_pursuit_delay_prepared(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e)
{
    scene_pursuit_delay_receipt prepared;int status;
    if(!stage||!e)return RF_RANGE;
    status=scene_pursuit_delay_candidate(e->slot,&e->saved,e->blind_target,stage->now,&prepared);if(status)return status;
    if(memcmp(&prepared,&e->pursuit_delay_prepared,sizeof(prepared)))return RF_FORMAT;
    /* The caller's ordinary Blind validator also proves the living prepared
     * target and exact registry generation on both composed player sides. */
    if(prepared.flags)return scene_blind_prepared(stage,e);
    return RF_OK;
}
/* All fallible work finishes before the first actor assignment. The caller
 * keeps scene/resource ownership stable between prepare and commit. */
static int scene_npc_speech_prepared(
    const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_checkpoint_restore_entry *e)
{
    const campaign_npc_body *o;
    uint32_t old,incoming;
    if(!stage||!e||!campaign_npc_bodies||!campaign_poses.items||
       e->slot>=campaign_npc_body_count||e->slot>=campaign_poses.count)
        return RF_RANGE;
    o=campaign_npc_bodies+e->slot;
    old=e->owner_before.speech_motion_mask;
    incoming=e->saved.speech_motion_mask;
    if((old|incoming|o->speech_motion_mask)&~SCENE_NPC_SPEECH_ALL)
        return RF_FORMAT;
    if(o->speech_motion_mask!=old)return RF_FORMAT;
    if(!(old|incoming))return RF_OK;
    if(!e->registered||e->handle!=o->registration.handle||e->unowned_motion_mask)
        return RF_FORMAT;
    if(old&&!scene_npc_speech_checkpoint_mask(e->slot,&o->selection,
        &campaign_poses.items[e->slot].playback,NULL,o->object_flags,old))
        return RF_FORMAT;
    if(incoming){
        if(!e->saved.animation_present||!e->bone_count||e->bone_count>50||
           e->bone_count!=campaign_poses.items[e->slot].bone_count||
           !e->matrices||!e->generations||
           memcmp(&e->playback,&e->saved.playback,sizeof(e->playback))||
           !scene_npc_speech_checkpoint_mask(e->slot,&e->selection,&e->playback,
               &e->saved,e->flags,incoming))return RF_FORMAT;
    }
    return RF_OK;
}
static inline int scene_npc_checkpoint_restore_validate(const scene_npc_checkpoint_restore_stage *stage)
{
    uint32_t i;int status;rf_npc_checkpoint_record current;
    if(!stage||stage->blind_joined!=1||stage->tactical_joined!=1||stage->crouch_joined!=1||stage->pursuit_return_joined!=1)return RF_RANGE;
    status=scene_waypoint_boundary();if(status)return status;
    if(!stage->alarm_joined||!stage->patrol_events_joined||
       stage->patrol_binding_generation!=scene_waypoint_events.generation)return RF_FORMAT;
    {scene_medic_flight_rng current_rng;scene_medic_flight_rng_capture(&current_rng);
     if(!stage->flight_joined||memcmp(&current_rng,&stage->flight_rng_before,sizeof(current_rng)))return RF_FORMAT;}
    if(stage->flight_profile&&(stage->flight_player!=campaign_player_object.handle||
       campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,stage->flight_player)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)stage->flight_player)!=&campaign_player_view))return RF_FORMAT;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        status=scene_npc_speech_prepared(stage,e);if(status)return status;
        status=scene_blind_prepared(stage,e);if(status)return status;
        status=scene_pursuit_delay_prepared(stage,e);if(status)return status;
        status=scene_tactical_prepared(stage,e);if(status)return status;
        status=scene_pursuit_return_prepared(stage,e);if(status)return status;
        status=scene_crouch_prepared(stage,e);if(status)return status;
        status=scene_guard_retreat_checkpoint_validate(&e->guard_retreat);if(status)return status;
        status=scene_noise_candidate(e->slot,&e->saved,&e->selection);if(status)return status;
        status=scene_alarm_runner_candidate(e->slot,&e->saved,&e->selection);if(status)return status;
        status=scene_patrol_candidate(e->slot,&e->saved,&e->selection);if(status)return status;
        if(e->saved.alarm.present)for(uint32_t j=0;j<i;j++)if(stage->entries[j].saved.alarm.present&&
           stage->entries[j].saved.alarm.event==e->saved.alarm.event)return RF_FORMAT;
        if(e->unowned_motion_mask||e->slot>=campaign_npc_body_count||
           campaign_npc_bodies[e->slot].registration.handle!=e->handle||
           campaign_npc_bodies[e->slot].death_item_attempted!=e->item_attempted_before||
           campaign_npc_bodies[e->slot].death_item_fault!=e->item_fault_before)return RF_FORMAT;
        if(!campaign_npc_bodies[e->slot].persistence_registered||
           campaign_npc_bodies[e->slot].persistence_slot>=rf_scene_defeated_actors.count||
           memcmp(&rf_scene_defeated_actors.item_drops[campaign_npc_bodies[e->slot].persistence_slot],
               &e->item_drop_before,sizeof(e->item_drop_before)))return RF_FORMAT;
        if(e->bone_count&&(!campaign_poses.items||e->slot>=campaign_poses.count||
           campaign_poses.items[e->slot].bone_count!=e->bone_count||campaign_poses.items[e->slot].matrices!=e->original_matrices||
           campaign_poses.items[e->slot].generations!=e->original_generations))return RF_FORMAT;
        if(e->registered&&campaign_collision_caches&&e->slot<campaign_model_owner_count&&
           campaign_collision_caches[e->slot].bone_count>50)return RF_FORMAT;
        if(e->medic_owner_snapshot){
            status=scene_medic_syringe_restore_validate(campaign_npc_bodies+e->slot,&e->medic_child);if(status)return status;
        }
        if(memcmp(&rf_scene_defeated_actors.routes[campaign_npc_bodies[e->slot].persistence_slot],
            &e->route_before,sizeof(e->route_before)))return RF_FORMAT;
        if((e->medic_owner_snapshot||e->route_owner_snapshot||e->owner_before.pursuit_return.phase||
            e->pursuit_return_prepared.phase||e->owner_before.speech_motion_mask||
            e->saved.speech_motion_mask)&&!stage->fresh_boot){
            if(memcmp(campaign_npc_bodies+e->slot,&e->owner_before,sizeof(e->owner_before))||
               !campaign_poses.items||e->slot>=campaign_poses.count||
               memcmp(campaign_poses.items+e->slot,&e->pose_before,sizeof(e->pose_before)))return RF_FORMAT;
        }
        if(stage->fresh_boot){
            if(memcmp(campaign_npc_bodies+e->slot,&e->owner_before,sizeof(e->owner_before))||
               memcmp(campaign_poses.items+e->slot,&e->pose_before,sizeof(e->pose_before)))return RF_FORMAT;
        }else {
            status=scene_npc_checkpoint_row_profile(e->slot,stage->now,&current,0,0,1);if(status)return status;
            if(memcmp(&current,&e->before,sizeof(current)))return RF_FORMAT;
        }
    }
    return RF_OK;
}
/* Nonpersistent full-load receipt. Marked only after NPC assignment; both
 * later vehicle publication and storage close can still fail. */
typedef struct scene_npc_airborne_restore_receipt {
    uint64_t epoch;campaign_npc_body *owners;uint32_t count,assigned,active;
} scene_npc_airborne_restore_receipt;
static int scene_npc_airborne_restore_identity(const scene_npc_checkpoint_restore_stage *stage,
    const scene_npc_airborne_restore_receipt *receipt,uint32_t before)
{
    uint32_t i;int status;
    if(!receipt)return RF_RANGE;if(!receipt->active)return RF_OK;
    /* Check lifetime before reading an owner, pose or stage row. */
    if(receipt->epoch!=scene_robot_death_epoch||receipt->owners!=campaign_npc_bodies||
       receipt->count!=campaign_npc_body_count||!stage||!stage->fresh_boot)return RF_FORMAT;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;const campaign_npc_body *o;const rf_entity_pose *p;
        if(!e->saved.airborne.present)continue;
        if(e->slot>=receipt->count||!campaign_npc_bodies||!campaign_poses.items||e->slot>=campaign_poses.count||
           !campaign_seeds.records.items||e->slot>=campaign_seeds.records.count||!campaign_seeds.items||
           e->saved.uid!=(uint32_t)campaign_seeds.records.items[e->slot].record.uid||
           e->saved.class_id!=campaign_seeds.items[e->slot].class_index)return RF_FORMAT;
        o=campaign_npc_bodies+e->slot;p=campaign_poses.items+e->slot;
        status=scene_npc_airborne_identity(e->slot,o,NULL);if(status)return status;
        if(o->registration.handle!=e->handle||e->handle!=e->owner_before.registration.handle||
           o->body.allocated_bytes!=e->owner_before.body.allocated_bytes||
           o->body.spheres.items!=e->owner_before.body.spheres.items||
           o->body.spheres.count!=e->owner_before.body.spheres.count||
           o->body.spheres.allocated_bytes!=e->owner_before.body.spheres.allocated_bytes||
           p->skeleton!=e->pose_before.skeleton||p->bone_count!=e->pose_before.bone_count||
           p->matrices!=e->pose_before.matrices||p->generations!=e->pose_before.generations||p->overrides!=e->pose_before.overrides||
           o->support.handle||o->piece_support.tag)return RF_FORMAT;
        if(before&&(memcmp(o,&e->owner_before,sizeof(*o))||memcmp(p,&e->pose_before,sizeof(*p))))return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_npc_airborne_restore_begin(const scene_npc_checkpoint_restore_stage *stage,
    scene_npc_airborne_restore_receipt *receipt)
{
    uint32_t i;if(!receipt||!stage)return RF_RANGE;memset(receipt,0,sizeof(*receipt));
    for(i=0;i<stage->count;i++)if(stage->entries[i].saved.airborne.present){receipt->active=1;break;}
    if(!receipt->active)return RF_OK;
    receipt->epoch=scene_robot_death_epoch;receipt->owners=campaign_npc_bodies;receipt->count=campaign_npc_body_count;
    return scene_npc_airborne_restore_identity(stage,receipt,1);
}
static int scene_npc_airborne_restore_abort(const scene_npc_checkpoint_restore_stage *stage,
    scene_npc_airborne_restore_receipt *receipt)
{
    uint32_t i;int status;
    if(!receipt||!receipt->assigned)return RF_OK;
    /* All rows must pass before any rollback store. No full owner/pose copy,
     * resource setter or restoration of the discarded old level is claimed. */
    status=scene_npc_airborne_restore_identity(stage,receipt,0);
    if(status)return scene_npc_checkpoint_restore_failure("airborne_abort_identity",UINT32_MAX,UINT32_MAX,status);
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body *o;
        const campaign_npc_body *old=&e->owner_before;rf_physics_body_state *b;const rf_physics_body_state *saved=&old->body.state;
        if(!e->saved.airborne.present)continue;o=campaign_npc_bodies+e->slot;b=&o->body.state;
        b->flags=saved->flags;b->state_124=saved->state_124;
        memcpy(b->velocity,saved->velocity,12);memcpy(b->vector_c8,saved->vector_c8,12);
        memcpy(b->mass_vector_d4,saved->mass_vector_d4,12);memcpy(b->vector_e0,saved->vector_e0,12);memcpy(b->vector_ec,saved->vector_ec,12);
        memcpy(b->vector_138,saved->vector_138,12);b->scalar_144=saved->scalar_144;b->reference_15c=saved->reference_15c;
        b->word_164=saved->word_164;b->word_168=saved->word_168;o->collision_contact=old->collision_contact;
        o->support=old->support;memcpy(o->support_velocity,old->support_velocity,12);
        o->piece_support=old->piece_support;o->piece_reacquire=old->piece_reacquire;
        o->object_flags=(o->object_flags&~RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK)|(old->object_flags&RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK);
        o->view.flags_7c=(o->view.flags_7c&~RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK)|(old->view.flags_7c&RF_NPC_CHECKPOINT_AIRBORNE_OBJECT_MASK);
    }
    receipt->assigned=0;return RF_OK;
}

/* Full composition validates all owners first, then calls assignment-only
 * publications while simulation and resource ownership remain paused. */
/* restored actors, catatonic actors, scheduler/view mismatches, saved mode mismatches */
uint32_t rf_scene_ai_mode_restored[4];
uint32_t rf_scene_medic_ai_restored[4]; /* legacy migrations, last UID, raw mode, effective mode */
uint32_t rf_scene_single_fire_restored[4]; /* count, first event, remaining frames, mode */
static inline void scene_npc_checkpoint_restore_assign(const scene_npc_checkpoint_restore_stage *stage)
{
    uint32_t i,j;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body *o=campaign_npc_bodies+e->slot;
        uint32_t persistence=o->persistence_slot;rf_entity_pose *pose=NULL;
        scene_pursuit_token_invalidate_owner(o); /* accepted saved timeline before pose/owner stores */
        if(e->registered){
            pose=campaign_poses.items+e->slot;
            for(j=0;j<pose->playback.completion.active.count;j++){
                int32_t id=pose->playback.completion.active.slots[j].motion;
                rf_motion_playback_resource *r=campaign_playback_resources.models[pose->skeleton].resources+id;
                if(r->references>0)r->references--;
            }
            for(j=0;j<e->playback.completion.active.count;j++)
                campaign_playback_resources.models[pose->skeleton].resources[e->playback.completion.active.slots[j].motion].references++;
            pose->playback=e->playback;
            if(e->bone_count){memcpy(pose->matrices,e->matrices,e->bone_count*48);memcpy(pose->generations,e->generations,e->bone_count*2);}
            else if(pose->generations)for(j=0;j<pose->bone_count;j++)pose->generations[j]=(uint16_t)(pose->playback.generation-1u);
            /* Derived collision skinning may carry the same generation from a
             * different pose. Rebuild it on demand after this publication. */
            if(campaign_collision_caches&&e->slot<campaign_model_owner_count&&campaign_collision_caches[e->slot].generations)
                for(j=0;j<campaign_collision_caches[e->slot].bone_count;j++)
                    campaign_collision_caches[e->slot].generations[j]=(uint16_t)(pose->playback.generation-1u);
            if(e->saved.animation_present)pose->controller=e->saved.controller;
            else {memset(&pose->controller,0,sizeof(pose->controller));pose->controller.override_state=-1;}
        }
        o->inventory=e->saved.inventory;o->view.weapons[0]=e->saved.primary;o->view.weapons[1]=e->saved.secondary;
        /* Living RFNC16 restores only the two presentation/fire bits. Legacy
         * rows decode zero, clearing a later in-session holster. Corpse810
         * belongs to death_flags_810 below and is deliberately untouched. */
        if(!e->saved.retired&&!e->saved.dead_pose){
            o->view.flags_810=(o->view.flags_810&~0x800u)|((e->saved.holster&1u)?0x800u:0u);
            o->view.flags_7d0=(o->view.flags_7d0&~0x200u)|((e->saved.holster&2u)?0x200u:0u);
            o->damage.effects.flags_810=(o->damage.effects.flags_810&~0x800u)|(o->view.flags_810&0x800u);
            o->firing.flags_810=(o->firing.flags_810&~0x800u)|(o->view.flags_810&0x800u);
            o->firing.flags_7d0=(o->firing.flags_7d0&~0x200u)|(o->view.flags_7d0&0x200u);
        }
        memset(&o->script_animation,0,sizeof(o->script_animation));
        if(e->saved.animation_present){
            o->script_animation.active=e->saved.script_animation.active;o->script_animation.loop=e->saved.script_animation.loop;
            o->script_animation.freeze=e->saved.script_animation.freeze;o->script_animation.motion=e->saved.script_animation.motion;
        }
        if(e->saved.shield_disabled<2)
            o->damage.effects.flags_814=(o->damage.effects.flags_814&~0x20u)|(e->saved.shield_disabled?0x20u:0u);
        o->damage.effects.health=e->saved.health;o->damage.effects.armor=e->saved.armor;o->damage.effects.affiliation=e->saved.affiliation;
        o->capek_shield_broken=e->saved.capek_shield_broken; /* Includes legacy/unbroken zero on rollback. */
        o->view.base_speed=e->base_speed; /* Candidate latch, including terminal rows; no setter/replay. */
        memset(&o->script_move,0,sizeof(o->script_move));
        {rf_entity_navigation_candidate start=o->navigation.start;
         memset(&o->navigation,0,sizeof(o->navigation));
         if(e->owner_before.flight.qualified==1)o->navigation.start=start;}
        while(o->script_once.count)campaign_script_single_pop(o,0);
        for(uint32_t k=0;k<e->saved.shot_count;k++){
            const rf_npc_checkpoint_shot *shot=e->saved.shots+k;
            campaign_single_fire_request *request=o->script_once.requests+k;
            uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;
            request->event=shot->event;request->mode=shot->mode;request->expires=frame+shot->remaining;
            memcpy(request->point,shot->point,12);++o->script_once.count;
            ++(shot->mode==2?rf_scene_script_fire_no_anim:rf_scene_script_shoot_once)[5];
            ++rf_scene_single_fire_restored[0];
            if(!k){rf_scene_single_fire_restored[1]=shot->event;rf_scene_single_fire_restored[2]=shot->remaining;
                rf_scene_single_fire_restored[3]=shot->mode;}
        }
        if(e->saved.shot_count)campaign_enemy_spread_random.value=e->saved.shot_rng;
        o->script_look.active=0;o->script_shoot.active=0;
        if(e->saved.look.active){
            o->script_look.active=1;o->script_look.event=e->saved.look.event;
            o->script_look.target_uid=e->saved.look.target_uid;
            memcpy(o->script_look.position,e->saved.look.position,sizeof(e->saved.look.position));
        }
        if(e->saved.dead_pose){
            uint32_t flags=e->saved.death_flags_810;
            /* The old sphere allocation remains authoritative until final
             * successful close. Do not advertise the incoming physical
             * stance while those old sphere centers are still installed. */
            if(e->crouch_shape_present)flags=(flags&~0x400u)|(o->view.flags_810&0x400u);
            o->view.flags_810=flags;o->damage.effects.flags_810=flags;
            o->death.action_824=e->saved.death_action;o->death.deadline_4b8=-1;
        }
        if(e->saved.move.active){
            const rf_npc_checkpoint_move *m=&e->saved.move;
            o->script_move.active=1;o->script_move.event=m->event;o->script_move.follow=m->follow;
            o->script_move.path=e->move_path;o->script_move.path_index=m->path_index;
            o->script_move.path_mode=m->path_mode;o->script_move.path_reverse=m->path_reverse;
            o->script_move.route_index=m->route_index;o->script_move.retry=m->retry;
            o->script_move.fall_speed=m->fall_speed;memcpy(o->script_move.target,m->target,sizeof(m->target));
            if(e->owner_before.flight.qualified!=1)memcpy(o->navigation.start.position,m->route_start,sizeof(m->route_start));
            memcpy(o->navigation.goal.position,m->route_goal,sizeof(m->route_goal));
            o->navigation.retained.count=m->retained_count;
            for(j=0;j<m->retained_count;j++)o->navigation.retained.nodes[j]=
                m->retained_nodes[j]==UINT32_MAX?&o->navigation.start:
                m->retained_nodes[j]==UINT32_MAX-1?&o->navigation.goal:e->move_nodes[j];
        }
        o->ai_mode.action_280=e->effective_ai_mode;o->ai_mode.clock_288=stage->now;
        o->view.action_520=e->effective_ai_mode;
        if(e->effective_ai_mode==6){
            o->ai_mode.state_2b4=e->saved.move.retry?12:1;
            o->ai_mode.clock_2bc=stage->now;
            o->ai_mode.argument_28c=o->ai_mode.argument_290=UINT32_MAX;
        }else if(scene_noise_active(&e->owner_before)){
            /* An incoming absence retires the old wait/travel phase only at
             * final-success assignment. New typed owners publish below. */
            o->ai_mode.state_2b4=0;
        }
        if(e->effective_ai_mode!=e->saved.ai_mode){
            ++rf_scene_medic_ai_restored[0];rf_scene_medic_ai_restored[1]=e->saved.uid;
            rf_scene_medic_ai_restored[2]=(uint32_t)e->saved.ai_mode;rf_scene_medic_ai_restored[3]=(uint32_t)e->effective_ai_mode;
        }
        ++rf_scene_ai_mode_restored[0];rf_scene_ai_mode_restored[1]+=e->effective_ai_mode==1;
        rf_scene_ai_mode_restored[2]+=o->view.action_520!=o->ai_mode.action_280;
        rf_scene_ai_mode_restored[3]+=o->view.action_520!=e->effective_ai_mode;
        o->combat_alert=e->saved.combat_alert;
        o->combat_due=o->combat_alert?(combat_frame==UINT32_MAX?30u:combat_frame+30u):0u;
        o->combat_navigation_due=0;o->combat_burst_remaining=0;
        o->combat_scripted=0;o->combat_target=o->combat_alert?campaign_player_object.handle:0;
        o->combat_reload_pending=0;o->combat_reload_due=0;o->combat_reload_weapon=0;
        if(e->saved.combat.active){
            uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;
            if(e->saved.combat.active==1){
                o->script_shoot.active=1;o->script_shoot.event=e->saved.combat.event;
                memcpy(o->script_shoot.point,e->saved.combat.point,sizeof(e->saved.combat.point));
                o->combat_scripted=3;o->combat_target=0;
            }else{o->combat_scripted=e->saved.combat.active==4?2:1;o->combat_target=e->attack_target_handle;}
            o->combat_alert=1;
            o->combat_burst_remaining=e->saved.combat.burst;
            o->combat_due=frame+e->saved.combat.due_remaining;
            o->combat_reload_due=e->saved.combat.reload_remaining?frame+e->saved.combat.reload_remaining:0;
            o->combat_reload_weapon=e->saved.combat.reload_weapon;
            campaign_enemy_spread_random.value=e->saved.combat.spread_rng;
            scene_turret_generated_attack_record(o,&e->saved,frame);
        }
        if(e->saved.reload_present){
            o->combat_reload_pending=1;o->combat_reload_due=e->reload_deadline;
            o->combat_reload_weapon=(int32_t)e->saved.reload_weapon;
        }
        o->controller_handle=e->controller_handle;
        memset(&o->piece_support,0,sizeof(o->piece_support));
        o->piece_reacquire=!e->saved.retired && !e->support.handle && e->support.material!=-1;
        o->object_flags=e->flags;memcpy(o->published,e->published,12);memcpy(o->previous,e->published,12);
        memset(o->look.body_angles,0,12);o->look.body_angles[1]=e->saved.yaw;
        if(e->saved.drone.kind)o->look.body_angles[0]=e->saved.drone.pitch;
        memcpy(o->look.orientation,e->look_basis,36);
        memcpy(o->look.angles.angles_87c,e->saved.eye_angles,12);
        memcpy(o->look.command_708,e->saved.look_command,sizeof(e->saved.look_command));
        memcpy(o->look.angles.delta_888,e->saved.look_delta,sizeof(e->saved.look_delta));
        memcpy(o->look.angles.offset_894,e->saved.look_offset,sizeof(e->saved.look_offset));
        memcpy(o->look.angles.vector_870,e->saved.look_vector,sizeof(e->saved.look_vector));
        rf_scene_defeated_actors.items[persistence].retired=e->saved.retired;
        rf_scene_defeated_actors.vitals[persistence].valid=!e->saved.retired;
        rf_scene_defeated_actors.vitals[persistence].health=e->saved.health;rf_scene_defeated_actors.vitals[persistence].armor=e->saved.armor;
        rf_scene_defeated_actors.mission[persistence].affiliation=e->saved.affiliation;rf_scene_defeated_actors.mission[persistence].flags=e->saved.flags;
        rf_scene_defeated_actors.drops[persistence]=e->saved.drop;
        rf_scene_defeated_actors.item_drops[persistence]=e->saved.item_drop;
        rf_scene_defeated_actors.capek_shield_broken[persistence]=(uint8_t)e->saved.capek_shield_broken;
        if(e->saved.retired){
            if(!e->crouch_retire_pending&&!e->pursuit_return_retire_pending){
                if(e->registered)(void)rf_entity_view_unregister(&campaign_registry,&campaign_entities,&o->registration);
                rf_physics_body_close(&o->body);
            }
            o->object_flags|=2|0x4000u;o->view.flags_810|=1;o->damage.effects.flags_810=o->view.flags_810;
        }else{
            o->body.state=e->body;o->room=e->room;o->support=e->support;o->selection=e->selection;memcpy(o->eye_position,e->eye,12);
            if(e->movement_resolved){o->movement=e->movement;o->movement_slot=e->movement_slot;o->movement_orientation=campaign_identity[0];}

            memcpy(o->support_velocity,e->saved.support_velocity,12);
            scene_npc_airborne_checkpoint_assign(o,&e->saved);
            memcpy(campaign_model_owners[e->slot].position,e->published,12);memcpy(campaign_model_owners[e->slot].basis,e->basis,36);
            campaign_model_owners[e->slot].room=e->room.room?e->room.room-1:UINT32_MAX;
        }
        scene_npc_physics_checkpoint_assign(o,&e->saved);
        if(!e->saved.retired)scene_npc_pain_checkpoint_assign(o,&e->saved,e->pain_deadlines);
        o->view.flags_7c=o->object_flags;
    }
    scene_npc_reactive_restore_probe();
    scene_corpse_checkpoint_schedule();
}
/* Call only after the complete publication transaction succeeds. Full-world
 * restore delays this until its final storage close, not the NPC assign phase. */
static inline void scene_npc_checkpoint_restore_finish(scene_npc_checkpoint_restore_stage *stage)
{
    uint32_t i;
    /* Retire old transient ownership only at this completed transaction boundary.
     * This reset never overwrites the newly assigned saved AI/playback. */
    scene_medic_load_committed();
    scene_tactical_cursor=stage->tactical_cursor;
    scene_tactical_service_frame=(combat_frame==UINT32_MAX?0:combat_frame)-1u;
    for(i=0;i<stage->count;i++){
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        campaign_npc_body *o=campaign_npc_bodies+e->slot;
        if(e->medic_owner_snapshot)scene_medic_syringe_restore_publish(o,&e->medic_child);
        /* Geometry and policy change only after the complete transaction has
         * passed its final close. All shape/identity checks were performed
         * against the sealed old owner, never during these assignments. */
        if(e->crouch_retire_pending||e->pursuit_return_retire_pending){
            /* Registry identity and allocation were sealed before close.
             * These ordinary retirement stores/free operations have no
             * gameplay callback, resource demand or recoverable failure. */
            if(e->registered)(void)rf_entity_view_unregister(&campaign_registry,&campaign_entities,&o->registration);
            rf_physics_body_close(&o->body);scene_crouch_body_retired(o);
        }else if(e->crouch_shape_present){
            scene_crouch_shape_publish(&o->body.spheres,&e->crouch_shape.prepared);
            o->view.flags_810=(o->view.flags_810&~0x400u)|(e->crouch_shape.prepared.flags&0x400u);
            o->damage.effects.flags_810=(o->damage.effects.flags_810&~0x400u)|(o->view.flags_810&0x400u);
            o->crouch=e->crouch_prepared;o->unholster.stance_7bc=(int32_t)e->crouch_intent;
            o->stance_clock_7b4=e->crouch_stamp;
        }else if(e->saved.retired){
            memset(&o->crouch,0,sizeof(o->crouch));o->unholster.stance_7bc=0;o->stance_clock_7b4=0;
        }
        scene_blind_clear(o);
        if(e->blind_target){o->blind.present=1;o->blind.target=e->blind_target;o->blind.last_seen=e->blind_last_seen;}
        o->pursuit_delay=e->pursuit_delay_prepared;
        memset(&o->tactical,0,sizeof(o->tactical));
        if(e->saved.tactical.present){
            const rf_npc_checkpoint_tactical *t=&e->saved.tactical;
            uint32_t mode=e->saved.combat.active==2?1u:e->saved.combat.active==4?2u:0u;
            o->tactical.flags=(t->phase?(SCENE_TACTICAL_PRESENT|(mode<<SCENE_TACTICAL_MODE_SHIFT)):0u)|
                (t->recovery_present?SCENE_TACTICAL_RECOVERY:0u);
            o->tactical.phase=t->phase;o->tactical.target=e->tactical_target;
            o->tactical.phase_due=e->tactical_phase_due;o->tactical.recovery_due=e->tactical_recovery_due;
            o->tactical.random.value=t->random;
        }
        scene_actor_route_publish(o,&e->saved.route,&e->move_path,e->dwell_deadline);
        rf_scene_defeated_actors.routes[o->persistence_slot]=e->saved.route;
        scene_medic_state_assign(o,&e->saved.medic);
        rf_scene_defeated_actors.medic[o->persistence_slot]=e->saved.medic;
        /* Full-world storage has closed successfully and history keys/slots
         * are rebound. Standalone commit also reaches this complete boundary. */
        campaign_npc_ai_suppression_assign(o,e->saved.ai_suppressed);
        rf_scene_defeated_actors.ai_suppressed[o->persistence_slot]=(uint8_t)e->saved.ai_suppressed;
        /* Restore the ordinary start projection before optional typed flight
         * publication. A constructor flight has start_valid0 and deliberately
         * leaves a simultaneously admitted ordinary route's actual start alone. */
        if(e->owner_before.flight.qualified==1){
            memset(&o->navigation.start,0,sizeof(o->navigation.start));
            /* Source constructor geometry is real even while the portable
             * start coordinates remain unwritten/start_valid0. */
            o->navigation.start.radius=o->navigation_radius;
            o->navigation.start.height=o->death_item_extent_7c4;
            if(e->saved.move.active)memcpy(o->navigation.start.position,e->saved.move.route_start,12);
        }
        if(e->saved.flight.present)scene_medic_flight_restore_publish(o,&e->flight,stage->flight_player);
        else if(!e->saved.guard_retreat.present){
            memset(o->command_714,0,sizeof(o->command_714));
            if(e->saved.retired)memset(&o->flight,0,sizeof(o->flight));
            else scene_medic_flight_legacy_restore(o,stage->now);
        }
        scene_guard_retreat_checkpoint_assign(&e->guard_retreat);
        o->alarm=e->saved.alarm;o->alarm_fault=0;
        if(o->alarm.present){
            o->script_move.origin=SCENE_MOVE_ORIGIN_NONE;
            o->ai_mode.action_280=o->view.action_520=16;o->ai_mode.state_2b4=o->alarm.phase==1?1:10;
            o->ai_mode.flags_530=(o->ai_mode.flags_530&~0x200000u)|(o->alarm.started?0x200000u:0);
        }
        o->death_item_attempted=e->saved.retired||e->saved.dead_pose;o->death_item_fault=0;
        /* Final-success continuation publication follows every publisher that
         * may clear the outgoing return owner (Blind, route, flight, guard).
         * All relation/late-close proofs must precede this assignment-only
         * boundary. The temporary nonempty-return save fence remains until
         * those callers are complete; this is not admission authority. */
        o->pursuit_return=e->pursuit_return_prepared;
        o->speech_motion_mask=e->saved.speech_motion_mask;
    }
    scene_medic_flight_rng_publish(&stage->flight_rng);
}
/* Narrow aliases of the newly retained flight owner. Other early world/body
 * fields keep the established fresh-scene failure policy. No callbacks/draws. */
static inline void scene_npc_checkpoint_flight_abort(const scene_npc_checkpoint_restore_stage *stage)
{
    uint32_t i;if(!stage)return;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        if(e->owner_before.flight.qualified==1&&e->owner_before.flight.entered&&e->slot<campaign_npc_body_count){
            campaign_npc_body *o=campaign_npc_bodies+e->slot;
            memcpy(o->body.state.vector_c8,e->owner_before.body.state.vector_c8,12);
            memcpy(o->body.state.vector_ec,e->owner_before.body.state.vector_ec,12);
        }
    }
    if(stage->flight_rng_before.present)scene_medic_flight_rng_publish(&stage->flight_rng_before);
}
static inline int scene_npc_checkpoint_restore_commit(scene_npc_checkpoint_restore_stage *stage)
{
    if(!stage||stage->count>RF_NPC_CHECKPOINT_MAX_COUNT)return RF_RANGE;
    for(uint32_t i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        if(!campaign_npc_bodies||e->slot>=campaign_npc_body_count)return RF_FORMAT;
        if(e->saved.speech_motion_mask||e->owner_before.speech_motion_mask||
           campaign_npc_bodies[e->slot].speech_motion_mask)return RF_NOT_FOUND;
    }
    int status=scene_npc_checkpoint_restore_validate(stage);if(status)return status;
    scene_medic_flight_restore_scope(1);
    scene_npc_checkpoint_restore_assign(stage);
    scene_npc_checkpoint_restore_finish(stage);scene_medic_flight_restore_scope(0);scene_robot_death_reset();
    scene_ai_drone_smash_reset();scene_ai_drone_secondary_reset();scene_ai_tankbot_secondary_reset();scene_ai_drone_missile_reset();scene_ai_laser_reset();scene_ai_tribeam_reset();scene_ai_cane_reset();scene_ai_spit_reset();scene_ai_sonar_reset();return RF_OK;
}
#endif
