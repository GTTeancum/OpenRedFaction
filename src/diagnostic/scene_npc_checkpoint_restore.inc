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
    rf_npc_checkpoint_record saved,before;
    campaign_npc_body owner_before;rf_entity_pose pose_before;
    rf_physics_body_state body;rf_entity_room_state room;rf_physics_support_contact support;
    rf_entity_motion_selection selection;rf_motion_playback_state playback;
    rf_level_waypoint_path move_path;rf_entity_navigation_candidate *move_nodes[4];
    float basis[9],look_basis[9],published[3],eye[3];int32_t idle;
    uint32_t bone_count;float (*matrices)[12];uint16_t *generations;
    float (*original_matrices)[12];uint16_t *original_generations;
} scene_npc_checkpoint_restore_entry;
typedef struct scene_npc_checkpoint_restore_stage {
    uint32_t count,bytes,fresh_boot;int32_t now;
    scene_npc_checkpoint_restore_entry entries[];
} scene_npc_checkpoint_restore_stage;
static int scene_npc_checkpoint_attack_target_handle(uint32_t uid,
    const rf_npc_checkpoint_record *rows,uint32_t count,uint32_t *handle)
{
    uint32_t i,j;
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
    return RF_FORMAT;
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
{if(stage){free(*stage);*stage=NULL;}}
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
    uint32_t budget,uint32_t fresh_boot,scene_npc_checkpoint_restore_stage **out)
{
    scene_npc_checkpoint_restore_stage *stage=NULL;rf_npc_checkpoint_record current;
    unsigned char identity[32]={0},wire[RF_NPC_CHECKPOINT_HEADER+RF_NPC_CHECKPOINT_ROW_MAX];
    uint32_t i,j,expected=0,written,spread_seen=0,spread_saved=0;uint64_t bytes;unsigned char *bone_storage;int status;
    const char *detail="preflight";
    if(fresh_boot>1||!out||*out||!place||!catalog||(count&&!rows)||count>RF_NPC_CHECKPOINT_MAX_COUNT||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.records.items||!campaign_seeds.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    /* Ordinary quickload reconstructs a fresh scene. The component-only
     * in-session API cannot overwrite slots whose poses belong to corpses. */
    if(campaign_live_corpse_count)return RF_NOT_FOUND;
    status=rf_npc_checkpoint_encode(identity,catalog,NULL,0,wire,sizeof(wire),&written);if(status)return status;
    for(i=0;i<campaign_npc_body_count;i++){
        /* Fresh boot may still display its initialization clip; it may not
         * silently discard active routes, combat or unsupported gameplay. */
        status=scene_npc_checkpoint_row_mode(i,now,&current,fresh_boot);
        if(status==RF_NOT_FOUND)continue;if(status)return status;expected++;
    }
    if(expected!=count)return RF_FORMAT;
    bytes=sizeof(*stage)+(uint64_t)count*sizeof(*stage->entries);
    for(i=0;i<count;i++)if(rows[i].animation_present){
        for(j=0;j<campaign_npc_body_count;j++)if((uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
        if(j==campaign_npc_body_count||!campaign_poses.items||j>=campaign_poses.count||campaign_poses.items[j].bone_count>50)return RF_FORMAT;
        bytes+=(uint64_t)campaign_poses.items[j].bone_count*48+((campaign_poses.items[j].bone_count*2+3u)&~3u);
    }
    if(bytes>budget||bytes>UINT32_MAX)return RF_RANGE;
    stage=calloc(1,(size_t)bytes);if(!stage)return RF_IO;stage->count=count;stage->bytes=(uint32_t)bytes;stage->now=now;stage->fresh_boot=fresh_boot;
    bone_storage=(unsigned char*)(stage->entries+count);
    for(i=0;i<count;i++){
        detail="row";
        scene_npc_checkpoint_restore_entry *e=stage->entries+i;campaign_npc_body *o;
        const rf_entity_creation_vitals_class *vitals;rf_checkpoint_placement placement={0};
        rf_motion_playback_resource clip;rf_entity_playback_model *model;float angles[3]={0};int32_t id;uint32_t cls;
        status=rf_npc_checkpoint_encode(identity,catalog,rows+i,1,wire,sizeof(wire),&written);if(status)goto fail;
        if(i&&rows[i-1].uid>=rows[i].uid){status=RF_FORMAT;goto fail;}
        for(j=0;j<campaign_npc_body_count;j++)if(campaign_npc_bodies[j].persistence_registered&&
            (uint32_t)campaign_seeds.records.items[j].record.uid==rows[i].uid)break;
        if(j==campaign_npc_body_count){status=RF_FORMAT;goto fail;}o=campaign_npc_bodies+j;cls=campaign_seeds.items[j].class_index;
        if(cls!=rows[i].class_id||cls>=campaign_seeds.class_count||!campaign_seeds.classes){status=RF_FORMAT;goto fail;}
        vitals=&campaign_seeds.classes[cls].vitals;
        if(rows[i].health>(vitals->health<0?100:vitals->health)||rows[i].armor>vitals->armor){status=RF_FORMAT;goto fail;}
        e->slot=j;e->handle=o->registration.handle;e->registered=o->registration.view!=NULL;e->saved=rows[i];e->idle=-1;
        status=scene_npc_checkpoint_controller_handle(rows[i].controller_uid,&e->controller_handle);if(status)goto fail;
        if(rows[i].move.active){
            uint32_t mode,k;
            detail="move_binding";
            status=scene_npc_checkpoint_move_binding(rows[i].move.event,e->handle,rows[i].move.follow,&e->move_path,&mode);
            if(status)goto fail;
            if(mode!=rows[i].move.path_mode||e->move_path.count!=rows[i].move.path_count){status=RF_FORMAT;goto fail;}
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
            }else{
                detail="attack_target";
                status=scene_npc_checkpoint_attack_target_handle(rows[i].combat.event,
                    rows,count,&e->attack_target_handle);
                if(status)goto fail;
            }
            if(spread_seen&&spread_saved!=rows[i].combat.spread_rng){status=RF_FORMAT;goto fail;}
            spread_seen=1;spread_saved=rows[i].combat.spread_rng;
        }
        if(fresh_boot){
            if(!campaign_poses.items||j>=campaign_poses.count){status=RF_FORMAT;goto fail;}
            e->owner_before=*o;e->pose_before=campaign_poses.items[j];
            if(o->persistence_slot>=rf_scene_defeated_actors.count||
               (e->registered&&(o->registration.view!=&o->view||rf_object_registry_lookup(&campaign_registry,e->handle)!=&o->registration))){status=RF_FORMAT;goto fail;}
        }else {status=scene_npc_checkpoint_row(j,now,&e->before);if(status)goto fail;}
        if(!rows[i].retired&&!e->registered){status=RF_RANGE;goto fail;} /* Fresh actors, never resurrect freed bodies. */
        e->flags=(o->object_flags&~0x4004u)|rows[i].flags;
        memcpy(e->published,rows[i].position,12);angles[1]=rows[i].yaw;
        status=rf_look_orientation(angles,e->basis);if(status)goto fail;
        {rf_eye_angle_state eye_angles={0};
         memcpy(eye_angles.angles_87c,rows[i].eye_angles,12);
         status=rf_eye_physics_orientation(e->basis,&eye_angles,e->look_basis);if(status)goto fail;}
        rf_motion_playback_initialize(&e->playback);
        if(rows[i].retired)continue;
        if(!campaign_model_owners||j>=campaign_model_owner_count||!campaign_poses.items||j>=campaign_poses.count){status=RF_RANGE;goto fail;}
        e->body=o->body.state;memcpy(e->body.position,rows[i].position,12);memcpy(e->body.next_position,rows[i].position,12);
        memcpy(e->body.orientation,e->basis,36);memcpy(e->body.next_orientation,e->basis,36);memset(e->body.velocity,0,12);
        status=rf_physics_spheres_bounds(o->body.spheres.items,o->body.spheres.count,e->body.position,&e->body.bounds);if(status)goto fail;
        status=rf_physics_tensor_world(e->body.local_tensor,e->basis,e->body.world_tensor);if(status)goto fail;
        status=rf_physics_publish_position(&e->body,e->published,&e->flags);if(status)goto fail;
        placement.spheres=o->body.spheres.items;placement.count=o->body.spheres.count;
        placement.query_flags=(o->body.state.state_124&~0x1000u)|4u;
        memcpy(placement.position,e->published,12);memcpy(placement.basis,e->basis,36);
        detail="placement";status=place(context,&placement,&e->room,&e->support);if(status)goto fail;
        if(memcmp(e->room.query_position,e->published,12)||
           (!!e->support.handle!=!!rows[i].support_uid)||!e->room.room||
           e->support.material< -1||(!placement.count&&e->support.material!=-1)||
           (placement.count&&e->support.material==-1&&((o->body.state.flags&(1u|0x400000u))||o->support.handle))){status=RF_FORMAT;goto fail;}e->room.flags=e->flags;
        if(rows[i].support_uid){e->body.flags&=~1u;e->body.flags|=0x400000u;}
        status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&e->selection);if(status)goto fail;
        if(rows[i].primary>=0){status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,rows[i].primary,&e->selection);if(status)goto fail;}
        if(e->selection.mapping.skeleton!=campaign_poses.items[j].skeleton){status=RF_FORMAT;goto fail;}
        id=e->selection.mapping.states[0];
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
            for(k=0;k<rows[i].playback.completion.active.count;k++){
                detail="clip";status=scene_npc_checkpoint_restore_clip(skeleton,rows[i].playback.completion.active.slots[k].motion,count);if(status)goto fail;
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
        status=scene_npc_checkpoint_restore_eye(j,e,e->eye);if(status)goto fail;
    }
    detail="pair_fit";status=scene_npc_checkpoint_pair_fit_allowed(count,scene_npc_checkpoint_restore_placement,stage,
        scene_npc_checkpoint_restore_original_overlap);if(status)goto fail;
    *out=stage;return RF_OK;
fail:
#ifndef RF_IMAGE_XBOX_NATIVE
    if(getenv("RF_REPLAY_WORLD_RESTORE_PROBE"))printf("WORLD_RESTORE_NPC_ROW index%u uid%u phase%s status%d\n",
        i,i<count?rows[i].uid:0,detail,status);
#else
    (void)detail;
#endif
    free(stage);return status;
}
static inline int scene_npc_checkpoint_restore_prepare(const rf_npc_checkpoint_record *rows,uint32_t count,
    const rf_npc_checkpoint_catalog *catalog,int32_t now,scene_npc_checkpoint_place place,void *context,
    uint32_t budget,scene_npc_checkpoint_restore_stage **out)
{return scene_npc_checkpoint_restore_prepare_mode(rows,count,catalog,now,place,context,budget,0,out);}
/* All fallible work finishes before the first actor assignment. The caller
 * keeps scene/resource ownership stable between prepare and commit. */
static inline int scene_npc_checkpoint_restore_validate(const scene_npc_checkpoint_restore_stage *stage)
{
    uint32_t i;int status;rf_npc_checkpoint_record current;
    if(!stage)return RF_RANGE;
    for(i=0;i<stage->count;i++){
        const scene_npc_checkpoint_restore_entry *e=stage->entries+i;
        if(e->slot>=campaign_npc_body_count||campaign_npc_bodies[e->slot].registration.handle!=e->handle)return RF_FORMAT;
        if(e->bone_count&&(!campaign_poses.items||e->slot>=campaign_poses.count||
           campaign_poses.items[e->slot].bone_count!=e->bone_count||campaign_poses.items[e->slot].matrices!=e->original_matrices||
           campaign_poses.items[e->slot].generations!=e->original_generations))return RF_FORMAT;
        if(e->registered&&campaign_collision_caches&&e->slot<campaign_model_owner_count&&
           campaign_collision_caches[e->slot].bone_count>50)return RF_FORMAT;
        if(stage->fresh_boot){
            if(memcmp(campaign_npc_bodies+e->slot,&e->owner_before,sizeof(e->owner_before))||
               memcmp(campaign_poses.items+e->slot,&e->pose_before,sizeof(e->pose_before)))return RF_FORMAT;
        }else {
            status=scene_npc_checkpoint_row(e->slot,stage->now,&current);if(status)return status;
            if(memcmp(&current,&e->before,sizeof(current)))return RF_FORMAT;
        }
    }
    return RF_OK;
}
/* Full composition validates all owners first, then calls assignment-only
 * publications while simulation and resource ownership remain paused. */
/* restored actors, catatonic actors, scheduler/view mismatches, saved mode mismatches */
uint32_t rf_scene_ai_mode_restored[4];
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
        memset(&o->script_animation,0,sizeof(o->script_animation));
        if(e->saved.animation_present){
            o->script_animation.active=e->saved.script_animation.active;o->script_animation.loop=e->saved.script_animation.loop;
            o->script_animation.freeze=e->saved.script_animation.freeze;o->script_animation.motion=e->saved.script_animation.motion;
        }
        if(e->saved.shield_disabled<2)
            o->damage.effects.flags_814=(o->damage.effects.flags_814&~0x20u)|(e->saved.shield_disabled?0x20u:0u);
        o->damage.effects.health=e->saved.health;o->damage.effects.armor=e->saved.armor;o->damage.effects.affiliation=e->saved.affiliation;
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
        o->ai_mode.action_280=e->saved.ai_mode;o->ai_mode.clock_288=stage->now;
        o->view.action_520=e->saved.ai_mode;
        ++rf_scene_ai_mode_restored[0];rf_scene_ai_mode_restored[1]+=e->saved.ai_mode==1;
        rf_scene_ai_mode_restored[2]+=o->view.action_520!=o->ai_mode.action_280;
        rf_scene_ai_mode_restored[3]+=o->view.action_520!=e->saved.ai_mode;
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
            }else{o->combat_scripted=1;o->combat_target=e->attack_target_handle;}
            o->combat_alert=1;
            o->combat_burst_remaining=e->saved.combat.burst;
            o->combat_due=frame+e->saved.combat.due_remaining;
            o->combat_reload_due=e->saved.combat.reload_remaining?frame+e->saved.combat.reload_remaining:0;
            o->combat_reload_weapon=e->saved.combat.reload_weapon;
            campaign_enemy_spread_random.value=e->saved.combat.spread_rng;
        }
        o->controller_handle=e->controller_handle;
        memset(&o->piece_support,0,sizeof(o->piece_support));
        o->piece_reacquire=!e->saved.retired && !e->support.handle && e->support.material!=-1;
        o->object_flags=e->flags;memcpy(o->published,e->published,12);memcpy(o->previous,e->published,12);
        memset(o->look.body_angles,0,12);o->look.body_angles[1]=e->saved.yaw;memcpy(o->look.orientation,e->look_basis,36);
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
        if(e->saved.retired){
            if(e->registered)(void)rf_entity_view_unregister(&campaign_registry,&campaign_entities,&o->registration);
            rf_physics_body_close(&o->body);o->object_flags|=2|0x4000u;o->view.flags_810|=1;o->damage.effects.flags_810=o->view.flags_810;
        }else{
            o->body.state=e->body;o->room=e->room;o->support=e->support;o->selection=e->selection;memcpy(o->eye_position,e->eye,12);
            memcpy(o->support_velocity,e->saved.support_velocity,12);
            memcpy(campaign_model_owners[e->slot].position,e->published,12);memcpy(campaign_model_owners[e->slot].basis,e->basis,36);
            campaign_model_owners[e->slot].room=e->room.room?e->room.room-1:UINT32_MAX;
        }
        o->view.flags_7c=o->object_flags;
    }
    scene_corpse_checkpoint_schedule();
}
static inline int scene_npc_checkpoint_restore_commit(scene_npc_checkpoint_restore_stage *stage)
{
    int status=scene_npc_checkpoint_restore_validate(stage);if(status)return status;
    scene_npc_checkpoint_restore_assign(stage);return RF_OK;
}
#endif
