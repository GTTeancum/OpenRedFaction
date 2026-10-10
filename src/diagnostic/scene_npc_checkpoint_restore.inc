#ifndef RF_SCENE_NPC_CHECKPOINT_RESTORE_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_RESTORE_INCLUDED
#include "rf/npc_checkpoint.h"
#include "scene_npc_checkpoint_pair_fit.inc"
/* Read-only candidate-world placement/support and room resolver. Must fill a
 * fresh room state with query_position equal to placement.position. No gameplay
 * callbacks, allocation of live resources or actor publication are permitted. */
typedef int (*scene_npc_checkpoint_place)(void *,const rf_checkpoint_placement *,rf_entity_room_state *,rf_physics_support_contact *);
typedef struct scene_npc_checkpoint_restore_entry {
    uint32_t slot,handle,registered,flags,controller_handle,attack_target_handle;
    uint32_t unowned_motion_mask; /* Staged only; RFAP may own exact burn pain. */
    uint32_t item_attempted_before;int32_t item_fault_before;rf_campaign_item_drop item_drop_before;
    rf_npc_checkpoint_record saved,before;
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
    scene_npc_checkpoint_restore_entry entries[];
} scene_npc_checkpoint_restore_stage;
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
static int scene_npc_checkpoint_restore_placement(void *context,uint32_t index,
    rf_checkpoint_placement *p,uint32_t *retired)
{
    const scene_npc_checkpoint_restore_stage *stage=context;
    const scene_npc_checkpoint_restore_entry *e;
    if(!stage||index>=stage->count)return RF_RANGE;e=stage->entries+index;
    /* Hidden actors retain their saved pose but do not participate in live
     * body contacts; pair clearance only applies to active solid actors. */
    *retired=e->saved.retired||!!(e->saved.flags&0x4000u);if(*retired)return RF_OK;
    if(e->slot>=campaign_npc_body_count)return RF_FORMAT;
    memset(p,0,sizeof(*p));p->spheres=campaign_npc_bodies[e->slot].body.spheres.items;
    p->count=campaign_npc_bodies[e->slot].body.spheres.count;
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
    if(!stage->fresh_boot)return 0;
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
static inline void scene_npc_checkpoint_restore_discard(scene_npc_checkpoint_restore_stage **stage)
{
    uint32_t i;
    if(stage&&*stage){
        for(i=0;i<(*stage)->count;i++)scene_medic_syringe_restore_discard(&(*stage)->entries[i].medic_child);
        free(*stage);*stage=NULL;
    }
}
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
static inline int scene_npc_checkpoint_restore_prepare_mode(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,uint32_t fresh_boot,scene_npc_checkpoint_restore_stage **out,
    const scene_npc_seat_checkpoint_stage *seats)
{
    scene_npc_checkpoint_restore_stage *stage=NULL;rf_npc_checkpoint_record current;
    unsigned char identity[32]={0},wire[RF_NPC_CHECKPOINT_HEADER+RF_NPC_CHECKPOINT_ROW_MAX];
    uint32_t i,j,expected=0,written,spread_seen=0,spread_saved=0,pain_seen=0,pain_random=0;uint64_t bytes;unsigned char *bone_storage;int status;
    const char *detail="preflight";
    memset(rf_scene_npc_restore_reject,0,sizeof(rf_scene_npc_restore_reject));
    rf_scene_npc_restore_reject[1]=rf_scene_npc_restore_reject[2]=UINT32_MAX;
    rf_scene_npc_restore_reject[5]=count;
    if(fresh_boot>1||!out||*out||!place||!catalog||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||now<0||now>RF_TIMER_PERIOD)
        return scene_npc_checkpoint_restore_failure("arguments",UINT32_MAX,UINT32_MAX,RF_RANGE);
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||!campaign_seeds.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return scene_npc_checkpoint_restore_failure("owners",UINT32_MAX,UINT32_MAX,RF_RANGE);
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
    stage=calloc(1,(size_t)bytes);if(!stage)return scene_npc_checkpoint_restore_failure("allocation",UINT32_MAX,UINT32_MAX,RF_IO);stage->count=count;stage->bytes=(uint32_t)bytes;stage->now=now;stage->fresh_boot=fresh_boot;
    bone_storage=(unsigned char*)(stage->entries+count);
    for(i=0;i<count;i++){
        detail="row";
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body *o;
        const rf_entity_creation_vitals_class *vitals;rf_checkpoint_placement placement={0};
        rf_motion_playback_resource clip;rf_entity_playback_model *model;float angles[3]={0};int32_t id;uint32_t cls,seated=0;
        status=rf_npc_checkpoint_encode(identity,catalog,rows+i,1,wire,sizeof(wire),&written);if(status)goto fail;
        detail="row_identity";
        if(i&&rows[i-1].uid>=rows[i].uid){status=RF_FORMAT;goto fail;}
        for(j=0;j<campaign_npc_body_count;j++)if(campaign_npc_bodies[j].persistence_registered&&
            (uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
        if(j==campaign_npc_body_count){status=RF_FORMAT;goto fail;}o=campaign_npc_bodies+j;cls=campaign_seeds.items[j].class_index;
        if(cls!=rows[i].class_id||cls>=campaign_seeds.class_count||!campaign_seeds.classes){status=RF_FORMAT;goto fail;}
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
        if(e->medic_owner_snapshot&&!rows[i].retired&&!rows[i].dead_pose&&rows[i].health>0&&
           rows[i].ai_mode==0&&o->medic.default_action==2){
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
            status=scene_medic_syringe_restore_prepare(o,rows[i].medic.child_state,
                rows[i].medic.child_health,rows[i].retired,budget-stage->bytes,&e->medic_child);
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
            if(rows[i].route.origin==SCENE_MOVE_ORIGIN_ACTOR_DEFAULT)
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
        detail="fresh_identity";
        if(fresh_boot){
            if(!campaign_poses.items||j>=campaign_poses.count){status=RF_FORMAT;goto fail;}
            e->owner_before=*o;e->pose_before=campaign_poses.items[j];
            if(o->persistence_slot>=rf_scene_defeated_actors.count||
               (e->registered&&(o->registration.view!=&o->view||rf_object_registry_lookup(&campaign_registry,e->handle)!=&o->registration))){status=RF_FORMAT;goto fail;}
        }else {status=scene_npc_checkpoint_row_profile(j,now,&e->before,0,0,1);if(status)goto fail;}
        if(!rows[i].retired&&!e->registered){status=RF_RANGE;goto fail;} /* Fresh actors, never resurrect freed bodies. */
        e->flags=(o->object_flags&~0x4004u)|rows[i].flags;
        detail="drone_pose";status=scene_npc_drone_checkpoint_admit(j,o,rows+i);if(status)goto fail;
        memcpy(e->published,rows[i].position,12);angles[1]=rows[i].yaw;
        if(rows[i].drone.kind)angles[0]=rows[i].drone.pitch;
        status=rf_look_orientation(angles,e->basis);if(status)goto fail;
        detail="seat_basis";status=scene_npc_seat_checkpoint_basis(seats,rows+i,e->basis,&seated);if(status)goto fail;
        if(rows[i].drone.kind)memcpy(e->look_basis,e->basis,36); /* Canonical ordinary eye, including retired look-only state. */
        else {rf_eye_angle_state eye_angles={0};
         memcpy(eye_angles.angles_87c,rows[i].eye_angles,12);
         status=rf_eye_physics_orientation(e->basis,&eye_angles,e->look_basis);if(status)goto fail;}
        rf_motion_playback_initialize(&e->playback);
        if(rows[i].retired)continue;
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
        status=rf_physics_spheres_bounds(o->body.spheres.items,o->body.spheres.count,e->body.position,&e->body.bounds);if(status)goto fail;
        status=rf_physics_tensor_world(e->body.local_tensor,e->basis,e->body.world_tensor);if(status)goto fail;
        status=rf_physics_publish_position(&e->body,e->published,&e->flags);if(status)goto fail;
        detail="script_physics";
        status=scene_npc_physics_checkpoint_prepare(o,rows+i,&e->body,&e->flags);if(status)goto fail;
        scene_npc_drone_checkpoint_prepare(j,o,rows+i,&e->body,&e->flags);
        placement.spheres=o->body.spheres.items;placement.count=o->body.spheres.count;
        placement.query_flags=((rows[i].drone.kind==1?e->body.state_124:o->body.state.state_124)&~0x1000u)|4u;
        memcpy(placement.position,e->published,12);memcpy(placement.basis,e->basis,36);
        detail="placement";status=place(context,&placement,&e->room,&e->support);if(status)goto fail;
        detail="support_admission";
        if(memcmp(e->room.query_position,e->published,12)||
           (!!e->support.handle!=!!rows[i].support_uid)||!e->room.room||
           e->support.material< -1||(!placement.count&&e->support.material!=-1)||
           (!seated&&!scene_npc_physics_checkpoint_suspended(rows+i)&&placement.count&&e->support.material==-1&&
            (((rows[i].physics.present||rows[i].drone.kind==1)?(e->body.flags&(1u|0x400000u)):(o->body.state.flags&(1u|0x400000u)))||
             ((rows[i].physics.present||rows[i].drone.kind==1)?rows[i].support_uid:o->support.handle)))){status=RF_FORMAT;goto fail;}e->room.flags=e->flags;
        if(rows[i].support_uid){e->body.flags&=~1u;e->body.flags|=0x400000u;}
        detail="motion_selection";
        status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&e->selection);if(status)goto fail;
        if(rows[i].primary>=0){status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,rows[i].primary,&e->selection);if(status)goto fail;}
        if(e->selection.mapping.skeleton!=campaign_poses.items[j].skeleton){status=RF_FORMAT;goto fail;}
        detail="pain_owner";
        status=scene_npc_pain_checkpoint_prepare(rows+i,&e->selection,now,e->pain_deadlines);if(status)goto fail;
        id=e->selection.mapping.states[0];
        detail="animation_restore";
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
                   !((rows[i].combat_alert||rows[i].combat.active)&&
                     (motion==e->selection.mapping.actions[2]||motion==e->selection.mapping.actions[39]))&&
                   !(rows[i].combat_alert==0&&rows[i].combat.active==0&&
                     !rows[i].retired&&rows[i].health>0&&!(e->flags&2u)&&
                     scene_npc_checkpoint_residual_weapon_clip(j,cls,rows[i].primary,&rows[i].inventory,
                         &e->selection,&rows[i].playback,motion,0)))
                    e->unowned_motion_mask|=1u<<k;
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
                status=scene_clutter_pose_publish(e->medic_child.child,transform+9,transform,e->room.room);if(status)goto fail;
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
    scene_npc_checkpoint_restore_discard(&stage);return status;
}
static inline int scene_npc_checkpoint_restore_prepare(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,scene_npc_checkpoint_restore_stage **out)
{
    uint32_t i;int status=scene_npc_checkpoint_restore_prepare_mode(rows,count,catalog,now,place,context,budget,0,out,NULL);
    if(status)return status;
    /* Component-only restoration has no RFAP owner to discharge a slot. */
    for(i=0;i<(*out)->count;i++)if((*out)->entries[i].unowned_motion_mask){
        status=scene_npc_checkpoint_restore_failure("animation_owner",i,(*out)->entries[i].saved.uid,RF_FORMAT);
        scene_npc_checkpoint_restore_discard(out);return status;
    }
    return RF_OK;
}
/* All fallible work finishes before the first actor assignment. The caller
 * keeps scene/resource ownership stable between prepare and commit. */
static inline int scene_npc_checkpoint_restore_validate(const scene_npc_checkpoint_restore_stage *stage)
{
    uint32_t i;int status;rf_npc_checkpoint_record current;
    if(!stage)return RF_RANGE;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
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
        if((e->medic_owner_snapshot||e->route_owner_snapshot)&&!stage->fresh_boot){
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
        memset(&o->script_move,0,sizeof(o->script_move));memset(&o->navigation,0,sizeof(o->navigation));
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
            o->view.flags_810=e->saved.death_flags_810;o->damage.effects.flags_810=e->saved.death_flags_810;
            o->death.action_824=e->saved.death_action;o->death.deadline_4b8=-1;
        }
        if(e->saved.move.active){
            const rf_npc_checkpoint_move *m=&e->saved.move;
            o->script_move.active=1;o->script_move.event=m->event;o->script_move.follow=m->follow;
            o->script_move.path=e->move_path;o->script_move.path_index=m->path_index;
            o->script_move.path_mode=m->path_mode;o->script_move.path_reverse=m->path_reverse;
            o->script_move.route_index=m->route_index;o->script_move.retry=m->retry;
            o->script_move.fall_speed=m->fall_speed;memcpy(o->script_move.target,m->target,sizeof(m->target));
            memcpy(o->navigation.start.position,m->route_start,sizeof(m->route_start));
            memcpy(o->navigation.goal.position,m->route_goal,sizeof(m->route_goal));
            o->navigation.retained.count=m->retained_count;
            for(j=0;j<m->retained_count;j++)o->navigation.retained.nodes[j]=
                m->retained_nodes[j]==UINT32_MAX?&o->navigation.start:
                m->retained_nodes[j]==UINT32_MAX-1?&o->navigation.goal:e->move_nodes[j];
        }
        o->ai_mode.action_280=e->effective_ai_mode;o->ai_mode.clock_288=stage->now;
        o->view.action_520=e->effective_ai_mode;
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
        o->combat_reload_due=0;o->combat_reload_weapon=0;
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
            if(e->registered)(void)rf_entity_view_unregister(&campaign_registry,&campaign_entities,&o->registration);
            rf_physics_body_close(&o->body);o->object_flags|=2|0x4000u;o->view.flags_810|=1;o->damage.effects.flags_810=o->view.flags_810;
        }else{
            o->body.state=e->body;o->room=e->room;o->support=e->support;o->selection=e->selection;memcpy(o->eye_position,e->eye,12);
            if(e->movement_resolved){o->movement=e->movement;o->movement_slot=e->movement_slot;o->movement_orientation=campaign_identity[0];}

            memcpy(o->support_velocity,e->saved.support_velocity,12);
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
    for(i=0;i<stage->count;i++){
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        campaign_npc_body *o=campaign_npc_bodies+e->slot;
        if(e->medic_owner_snapshot)scene_medic_syringe_restore_publish(o,&e->medic_child);
        scene_actor_route_publish(o,&e->saved.route,&e->move_path);
        rf_scene_defeated_actors.routes[o->persistence_slot]=e->saved.route;
        scene_medic_state_assign(o,&e->saved.medic);
        rf_scene_defeated_actors.medic[o->persistence_slot]=e->saved.medic;
        /* Full-world storage has closed successfully and history keys/slots
         * are rebound. Standalone commit also reaches this complete boundary. */
        campaign_npc_ai_suppression_assign(o,e->saved.ai_suppressed);
        rf_scene_defeated_actors.ai_suppressed[o->persistence_slot]=(uint8_t)e->saved.ai_suppressed;
        o->death_item_attempted=e->saved.retired||e->saved.dead_pose;o->death_item_fault=0;
    }
}
static inline int scene_npc_checkpoint_restore_commit(scene_npc_checkpoint_restore_stage *stage)
{
    int status=scene_npc_checkpoint_restore_validate(stage);if(status)return status;
    scene_npc_checkpoint_restore_assign(stage);
    scene_npc_checkpoint_restore_finish(stage);scene_robot_death_reset();
    scene_ai_drone_smash_reset();scene_ai_drone_secondary_reset();scene_ai_tankbot_secondary_reset();scene_ai_drone_missile_reset();scene_ai_laser_reset();scene_ai_tribeam_reset();scene_ai_cane_reset();scene_ai_spit_reset();scene_ai_sonar_reset();return RF_OK;
}
#endif
