#ifndef RF_SCENE_NPC_CHECKPOINT_CAPTURE_INCLUDED
#define RF_SCENE_NPC_CHECKPOINT_CAPTURE_INCLUDED
#include "rf/npc_checkpoint.h"
/* Include after campaign NPC/seed/pose/persistence declarations. Caller owns
 * stable scene state throughout both passes. Global projectile, shield, burn,
 * mover, event and pickup admission remains the composed caller's duty. */
/* Persistence canonicalizes ASCII names, while the loaded archive member may
 * retain authored capitalization. Compare bounded names without mutating either. */
static int scene_npc_checkpoint_level_equal(const char *a,const char *b)
{
    uint32_t i;
    for(i=0;i<64;i++){
        unsigned char x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(x>='A'&&x<='Z')x=(unsigned char)(x+32);
        if(y>='A'&&y<='Z')y=(unsigned char)(y+32);
        if(x!=y)return 0;
        if(!x)return i!=0;
    }
    return 0;
}
uint32_t rf_scene_npc_checkpoint_reject_state[6]; /* UID, reason hash, part, value bits, status, frame */
static int scene_npc_checkpoint_reject(uint32_t index,const char *reason,uint32_t part,double value,int status)
{
    uint32_t hash=2166136261u,bits;float reduced=(float)value;
    const unsigned char *text=(const unsigned char *)reason;
    while(*text){hash=(hash^*text++)*16777619u;}
    memcpy(&bits,&reduced,4);
    rf_scene_npc_checkpoint_reject_state[0]=(uint32_t)campaign_seeds.records.items[index].record.uid;
    rf_scene_npc_checkpoint_reject_state[1]=hash;
    rf_scene_npc_checkpoint_reject_state[2]=part;
    rf_scene_npc_checkpoint_reject_state[3]=bits;
    rf_scene_npc_checkpoint_reject_state[4]=(uint32_t)status;
    rf_scene_npc_checkpoint_reject_state[5]=combat_frame;
#ifndef RF_IMAGE_XBOX_NATIVE
    const char *enabled=getenv("RF_REPLAY_WORLD_CHECKPOINT_PROBE");
    if(enabled&&!strcmp(enabled,"1"))printf("WORLD_CHECKPOINT_NPC_ADMISSION uid%u reason%s part%u value%.9g status%d\n",
        (uint32_t)campaign_seeds.records.items[index].record.uid,reason,part,value,status);
#else
    (void)index;(void)reason;(void)part;(void)value;
#endif
    return status;
}
static int scene_npc_checkpoint_controller_uid(uint32_t handle,uint32_t *uid)
{
    uint32_t i;
    if(!uid)return RF_RANGE;*uid=0;if(!handle)return RF_OK;
    for(i=0;i<campaign_group_registration.count;i++){
        const rf_group_registered_controller *c=campaign_group_registration.controllers+i;
        if(c->handle==handle){
            if(c->object_kind!=8||rf_object_registry_lookup(&campaign_registry,handle)!=c||
               !c->runtime||!c->runtime->source||!c->runtime->source->keys||
               !c->runtime->source->record.key_count)return RF_FORMAT;
            *uid=c->runtime->source->keys[0].uid;
            return *uid&&*uid!=UINT32_MAX?RF_OK:RF_FORMAT;
        }
    }
    return RF_FORMAT; /* Never omit a persistent NPC with a stale backlink. */
}
static int scene_npc_checkpoint_controller_handle(uint32_t uid,uint32_t *handle)
{
    uint32_t i;uint32_t found=0;
    if(!handle)return RF_RANGE;*handle=0;if(!uid)return RF_OK;
    for(i=0;i<campaign_group_registration.count;i++){
        const rf_group_registered_controller *c=campaign_group_registration.controllers+i;
        if(c->runtime&&c->runtime->source&&c->runtime->source->keys&&
           c->runtime->source->record.key_count&&c->runtime->source->keys[0].uid==uid){
            if(found||!c->handle||c->object_kind!=8||rf_object_registry_lookup(&campaign_registry,c->handle)!=c)return RF_FORMAT;
            *handle=c->handle;found=1;
        }
    }
    return found?RF_OK:RF_NOT_FOUND;
}
/* Resolve borrowed waypoint storage from a unique authored movement event.
 * Pursuit (follow 2) is ordinary combat state and has no authored event. */
static int scene_npc_checkpoint_move_binding(uint32_t uid,uint32_t handle,uint32_t follow,
    rf_level_waypoint_path *path,uint32_t *mode)
{
    uint32_t i,j,found=0,linked;int status;
    if(!path||!mode||follow>2)return RF_RANGE;
    memset(path,0,sizeof(*path));*mode=0;
    if(follow==2)return uid?RF_FORMAT:RF_OK;
    if(!uid)return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *runtime=campaign_events.items+i;const rf_level_event *record;
        if(!runtime->authored||runtime->authored->record.uid!=uid)continue;
        if(++found!=1)return RF_FORMAT;record=&runtime->authored->record;
        if((follow==1&&strcmp(record->type,"Goto_Player"))||
           (follow==0&&strcmp(record->type,"Goto")&&strcmp(record->type,"Follow_Waypoints")))return RF_FORMAT;
        if(record->link_count&&!runtime->links)return RF_FORMAT;
        linked=0;for(j=0;j<record->link_count;j++)if((runtime->links[j].kind==1||runtime->links[j].kind==2)&&
            runtime->links[j].value==handle)linked++;
        if(linked!=1)return RF_FORMAT;
        if(!strcmp(record->type,"Follow_Waypoints")){
            if(!campaign_waypoints)return RF_NOT_FOUND;
            if(!strcmp(record->texts[1],"Loop"))*mode=1;
            else if(!strcmp(record->texts[1],"Ping Pong"))*mode=2;
            else if(strcmp(record->texts[1],"One way"))return RF_FORMAT;
            status=rf_level_waypoint_find(campaign_waypoints,campaign_waypoint_bytes,
                campaign_navigation.count,record->texts[0],path);if(status)return status;
            if(!path->count)return RF_FORMAT;
        }
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static int scene_npc_checkpoint_route_ordinal(const campaign_npc_body *o,
    const rf_entity_navigation_candidate *node,uint32_t *ordinal)
{
    uint32_t i;if(!o||!node||!ordinal)return RF_RANGE;
    if(node==&o->navigation.start){*ordinal=UINT32_MAX;return RF_OK;}
    if(node==&o->navigation.goal){*ordinal=UINT32_MAX-1;return RF_OK;}
    if(!campaign_navigation.references)return RF_RANGE;
    for(i=0;i<campaign_navigation.count;i++)if(campaign_navigation.references[i].candidate==node){*ordinal=i+1;return RF_OK;}
    return RF_NOT_FOUND;
}
static int scene_npc_checkpoint_look_binding(uint32_t uid,uint32_t handle,uint32_t target_uid,const float position[3])
{
    uint32_t i,j,found=0,linked;
    if(!uid||!position)return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *runtime=campaign_events.items+i;const rf_level_event *record;
        if(!runtime->authored||runtime->authored->record.uid!=uid)continue;
        if(++found!=1)return RF_FORMAT;record=&runtime->authored->record;
        if(strcmp(record->type,"Look_At")||record->words[0]!=target_uid||
           memcmp(record->position,position,12)||(record->link_count&&!runtime->links))return RF_FORMAT;
        linked=0;for(j=0;j<record->link_count;j++)if((runtime->links[j].kind==1||runtime->links[j].kind==2)&&
            runtime->links[j].value==handle)linked++;
        if(linked!=1)return RF_FORMAT;
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static int scene_npc_checkpoint_shoot_binding(uint32_t uid,uint32_t handle,const float point[3])
{
    uint32_t i,j,found=0,linked;
    if(!uid||!point)return RF_FORMAT;
    for(i=0;i<campaign_events.count;i++){
        const rf_runtime_event *runtime=campaign_events.items+i;const rf_level_event *record;
        if(!runtime->authored||runtime->authored->record.uid!=uid)continue;
        if(++found!=1)return RF_FORMAT;record=&runtime->authored->record;
        if(strcmp(record->type,"Shoot_At")||memcmp(record->position,point,12)||
           (record->link_count&&!runtime->links))return RF_FORMAT;
        linked=0;for(j=0;j<record->link_count;j++)if((runtime->links[j].kind==1||runtime->links[j].kind==2)&&
            runtime->links[j].value==handle)linked++;
        if(linked!=1)return RF_FORMAT;
    }
    return found==1?RF_OK:RF_NOT_FOUND;
}
static int scene_npc_checkpoint_attack_target_uid(uint32_t handle,uint32_t *uid)
{
    uint32_t i;
    if(!uid||!handle)return RF_RANGE;
    if(handle==campaign_player_object.handle){*uid=0;return RF_OK;}
    if(campaign_authored_vehicle_uid && handle==campaign_authored_vehicle_handle){
        *uid=(uint32_t)campaign_authored_vehicle_uid;return RF_OK;}
    for(i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *target=campaign_npc_bodies+i;
        uint32_t candidate=(uint32_t)campaign_seeds.records.items[i].record.uid;
        if(target->registration.view&&target->registration.handle==handle&&
           target->damage.effects.health>0&&target->persistence_registered&&
           rf_object_registry_lookup(&campaign_registry,handle)==&target->registration&&
           candidate&&candidate!=UINT32_MAX){*uid=candidate;return RF_OK;}
    }
    return RF_NOT_FOUND;
}
static int scene_npc_checkpoint_row_mode(uint32_t index,int32_t now,rf_npc_checkpoint_record *row,uint32_t initializing)
{
    const campaign_npc_body *o=campaign_npc_bodies+index;
    const rf_campaign_object_record *key;const rf_entity_pose *pose;
    uint32_t i,slot=o->persistence_slot,controller_uid=0,terminal_dead=0,attack_target_uid=0;int pending,status;
    rf_level_waypoint_path authored_path={0};uint32_t authored_mode=0;
    int32_t deadlines[5]={o->pain.ai_timer,o->pain.animation_lock,o->pain.cooldown,
        o->unholster.deadline_518,o->death.deadline_4b8};
    if(!o->persistence_registered){return o->registration.view?RF_FORMAT:RF_NOT_FOUND;}
    if(slot>=rf_scene_defeated_actors.count||slot>=RF_CAMPAIGN_ACTOR_SLOTS)return RF_FORMAT;
    key=rf_scene_defeated_actors.items+slot;
    if(key->level>=rf_scene_defeated_actors.level_count||key->level>=RF_CAMPAIGN_PICKUP_LEVELS||
       !scene_npc_checkpoint_level_equal(rf_scene_defeated_actors.levels[key->level],campaign_current_level)||
       key->uid!=(uint32_t)campaign_seeds.records.items[index].record.uid||
       campaign_seeds.items[index].class_index>=campaign_seeds.class_count)return RF_FORMAT;
    if(!o->registration.view&&!key->retired)return RF_FORMAT;
    if(o->registration.view&&(o->registration.view!=&o->view||
       rf_object_registry_lookup(&campaign_registry,o->registration.handle)!=&o->registration||
       rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)!=&o->view))return RF_FORMAT;
    if(o->registration.view&&o->damage.effects.health<=0&&!key->retired){
        if(index>=campaign_poses.count||!campaign_poses.items)return RF_FORMAT;
        pose=campaign_poses.items+index;
        if(o->death.deadline_4b8!=-1||!(o->view.flags_810&1u)||o->death.action_824<0||o->death.action_824>=45||
           o->script_animation.active||pose->playback.completion.frozen!=1||
           pose->playback.completion.active.count!=1||
           pose->playback.completion.active.slots[0].motion!=o->selection.mapping.actions[o->death.action_824])
            return scene_npc_checkpoint_reject(index,"death_unsettled",0,o->death.action_824,RF_RANGE);
        terminal_dead=1;
    }
    /* Live routes require a separate continuation record. Terminal corpses
     * cannot travel, so their leftover route and steering state is discardable. */
#define NPC_CAPTURE_ZERO(field) do{if(o->field)return scene_npc_checkpoint_reject(index,#field,0,(double)o->field,RF_RANGE);}while(0)
    if(!terminal_dead){
        NPC_CAPTURE_ZERO(script_once.count);
        if(o->script_move.active){
            status=scene_npc_checkpoint_move_binding(o->script_move.event,o->registration.handle,
                o->script_move.follow,&authored_path,&authored_mode);
            if(status)return scene_npc_checkpoint_reject(index,"move_binding",0,o->script_move.event,status);
            if(o->script_move.path.count!=authored_path.count||o->script_move.path_mode!=authored_mode||
               (authored_path.count&&(!o->script_move.path.indices||
                memcmp(o->script_move.path.indices,authored_path.indices,authored_path.count*4))))
                return scene_npc_checkpoint_reject(index,"move_path",0,o->script_move.path.count,RF_FORMAT);
            NPC_CAPTURE_ZERO(script_move.stop);
        }else{NPC_CAPTURE_ZERO(script_move.path.count);NPC_CAPTURE_ZERO(navigation.retained.count);}
    }
    if(!terminal_dead&&!o->script_animation.active&&o->script_animation.freeze&&
       !(o->script_move.active&&index<campaign_poses.count&&campaign_poses.items&&
         !campaign_poses.items[index].playback.completion.frozen))
        return scene_npc_checkpoint_reject(index,"inactive_script_freeze",0,o->script_animation.freeze,RF_RANGE);
    if(o->view.linked_handle!=-1)return scene_npc_checkpoint_reject(index,"linked_handle",0,o->view.linked_handle,RF_RANGE);
    NPC_CAPTURE_ZERO(view.occupant_count);
    if(o->attachment_75c!=-1)return scene_npc_checkpoint_reject(index,"attachment_75c",0,o->attachment_75c,RF_RANGE);
    status=scene_npc_checkpoint_controller_uid(o->controller_handle,&controller_uid);
    if(status)return scene_npc_checkpoint_reject(index,"controller_handle",0,o->controller_handle,status);
    if(o->combat_alert>1||(!terminal_dead&&!o->combat_alert&&(o->combat_due||o->combat_navigation_due))||(key->retired&&o->combat_alert))
        return scene_npc_checkpoint_reject(index,"combat_alert",0,o->combat_alert,RF_RANGE);
    if(!terminal_dead&&o->combat_scripted==3&&o->script_shoot.active){
        if(o->script_shoot.active!=1||o->combat_alert!=1||o->combat_target||o->script_move.active||
           o->combat_burst_remaining>256||o->combat_reload_weapon< -1||o->combat_reload_weapon>=64)
            return scene_npc_checkpoint_reject(index,"shoot_state",0,o->combat_scripted,RF_FORMAT);
        status=scene_npc_checkpoint_shoot_binding(o->script_shoot.event,o->registration.handle,o->script_shoot.point);
        if(status)return scene_npc_checkpoint_reject(index,"shoot_binding",0,o->script_shoot.event,status);
    }else if(!terminal_dead&&o->combat_scripted==1){
        /* A scripted Attack retains its target by authored UID (or zero for
         * the local player), never by a process-local registry handle. */
        status=scene_npc_checkpoint_attack_target_uid(o->combat_target,&attack_target_uid);
        if(status||o->combat_alert!=1||o->script_shoot.active||
           (o->script_move.active&&o->script_move.follow!=2)||
           o->combat_burst_remaining>256||o->combat_reload_weapon< -1||o->combat_reload_weapon>=64)
            return scene_npc_checkpoint_reject(index,"attack_state",0,o->combat_target,RF_FORMAT);
    }else{NPC_CAPTURE_ZERO(combat_scripted);NPC_CAPTURE_ZERO(script_shoot.active);}
    if(!terminal_dead){
        if(o->script_look.active){
            status=scene_npc_checkpoint_look_binding(o->script_look.event,o->registration.handle,
                o->script_look.target_uid,o->script_look.position);
            if(status)return scene_npc_checkpoint_reject(index,"look_binding",0,o->script_look.event,status);
        }
    }
    if(o->combat_target&&!(terminal_dead||(o->combat_alert&&
        (o->combat_target==campaign_player_object.handle||o->combat_scripted==1))))
        return scene_npc_checkpoint_reject(index,"combat_target",0,o->combat_target,RF_RANGE);
    if(o->combat_scripted!=3&&o->combat_scripted!=1){NPC_CAPTURE_ZERO(combat_burst_remaining);NPC_CAPTURE_ZERO(combat_reload_due);}
    NPC_CAPTURE_ZERO(damage.effects.burn);
#undef NPC_CAPTURE_ZERO
    for(i=0;i<64;i++)if(o->firing.active[i])return scene_npc_checkpoint_reject(index,"firing.active",i,o->firing.active[i],RF_RANGE);
    if(o->registration.view){
        for(i=0;i<5;i++){status=rf_timer_pending(deadlines[i],now,&pending);
            if(status||pending)return scene_npc_checkpoint_reject(index,"deadline",i,deadlines[i],status?status:RF_RANGE);}
        if(index>=campaign_poses.count||!campaign_poses.items)return RF_FORMAT;pose=campaign_poses.items+index;
        if(pose->playback.completion.active.count>16)return scene_npc_checkpoint_reject(index,"motion_count",0,pose->playback.completion.active.count,RF_RANGE);
        if(!initializing&&!pose->playback.completion.active.count&&!o->script_animation.active&&
           (pose->controller.override_enabled||pose->controller.duration!=0||pose->controller.current>0||pose->controller.next>0))
            return scene_npc_checkpoint_reject(index,"controller_without_playback",0,pose->controller.current,RF_RANGE);
        if(pose->playback.completion.frozen&&!o->script_animation.active&&!terminal_dead)return scene_npc_checkpoint_reject(index,"motion_frozen",0,pose->playback.completion.frozen,RF_RANGE);

        /* A resident looping clip can be restored from its serialized playback
         * even when it is not one of this NPC's controller state mappings.
         * Non-script one-shot actions still need their gameplay owner saved. */
        for(i=0;i<pose->playback.completion.active.count;i++){
            int32_t motion=pose->playback.completion.active.slots[i].motion;
            if(key->retired||motion<0||
               !campaign_playback_resources.models||pose->skeleton>=campaign_playback_resources.model_count||
               (uint32_t)motion>=campaign_playback_resources.models[pose->skeleton].count||
               !campaign_playback_resources.models[pose->skeleton].resources||
               (!initializing&&!o->script_animation.active&&!terminal_dead&&
                !campaign_playback_resources.models[pose->skeleton].resources[motion].looping&&
                !((o->combat_alert==1||o->combat_scripted==3||o->combat_scripted==1)&&
                  (motion==o->selection.mapping.actions[2]||
                   motion==o->selection.mapping.actions[39])))||
               (terminal_dead&&campaign_playback_resources.models[pose->skeleton].resources[motion].looping))
                return scene_npc_checkpoint_reject(index,"motion_not_idle_loop",(uint32_t)o->selection.mapping.states[0],motion,RF_RANGE);
        }
        /* rf_entity_body_open deliberately supports an empty authored sphere
         * set (Cutter/fish): it still owns a body and installs an identity tensor.
         * Capture its real pose, never invent a collision sphere or body. */
        if(!o->body.allocated_bytes)return scene_npc_checkpoint_reject(index,"body_missing",0,0,RF_RANGE);
        if(o->body.spheres.count>8||(o->body.spheres.count&&!o->body.spheres.items)||
           (!o->body.spheres.count&&o->body.spheres.items))
            return scene_npc_checkpoint_reject(index,"body_sphere_storage",0,o->body.spheres.count,RF_FORMAT);
        for(i=0;i<3;i++){
            if(!isfinite(o->body.state.velocity[i])||fabsf(o->body.state.velocity[i])>.001f)
                return scene_npc_checkpoint_reject(index,"velocity",i,o->body.state.velocity[i],RF_RANGE);
#define NPC_CAPTURE_AXIS(field) do{if(o->field[i]!=0)return scene_npc_checkpoint_reject(index,#field,i,o->field[i],RF_RANGE);}while(0)
            NPC_CAPTURE_AXIS(command_714);NPC_CAPTURE_AXIS(support_velocity);
#undef NPC_CAPTURE_AXIS
        }
        if(o->script_animation.active){
            int32_t motion=o->script_animation.motion;const rf_entity_playback_model *model;
            if(o->script_animation.active!=1||o->script_animation.loop>1||o->script_animation.freeze>1||motion<0||
               !campaign_playback_resources.models||pose->skeleton>=campaign_playback_resources.model_count)return RF_FORMAT;
            model=campaign_playback_resources.models+pose->skeleton;
            if(!model->resources||(uint32_t)motion>=model->count||model->resources[motion].looping!=o->script_animation.loop)return RF_FORMAT;
        }
    }
    for(i=0;i<3;i+=2)if(o->look.body_angles[i]!=0)return scene_npc_checkpoint_reject(index,"body_pitch_roll",i,o->look.body_angles[i],RF_RANGE);
    memset(row,0,sizeof(*row));row->uid=key->uid;row->class_id=campaign_seeds.items[index].class_index;
    row->retired=key->retired;row->flags=o->object_flags&0x4004u;row->affiliation=o->damage.effects.affiliation;
    row->health=o->damage.effects.health;row->armor=o->damage.effects.armor;
    memcpy(row->position,o->registration.view?o->body.state.position:o->published,12);
    row->yaw=o->look.body_angles[1];memcpy(row->eye_angles,o->look.angles.angles_87c,12);
    row->primary=o->view.weapons[0];row->secondary=o->view.weapons[1];
    row->drop=rf_scene_defeated_actors.drops[slot];row->inventory=o->inventory;row->ai_mode=o->ai_mode.action_280;
    row->controller_uid=controller_uid;
    row->combat_alert=terminal_dead?0:o->combat_alert;
    row->dead_pose=terminal_dead;
    if(terminal_dead){row->death_flags_810=o->view.flags_810;row->death_action=o->death.action_824;}
    else {
        if(o->combat_scripted==3||o->combat_scripted==1){
            uint32_t frame=combat_frame==UINT32_MAX?0:combat_frame;
            row->combat.active=o->combat_scripted==3?1:2;
            row->combat.event=row->combat.active==1?o->script_shoot.event:attack_target_uid;
            row->combat.burst=o->combat_burst_remaining;
            row->combat.due_remaining=o->combat_due>frame?o->combat_due-frame:0;
            row->combat.reload_remaining=o->combat_reload_due>frame?o->combat_reload_due-frame:0;
            row->combat.reload_weapon=o->combat_reload_weapon;
            if(row->combat.active==1)memcpy(row->combat.point,o->script_shoot.point,sizeof(row->combat.point));
            row->combat.spread_rng=campaign_enemy_spread_random.value;
        }
        memcpy(row->look_command,o->look.command_708,sizeof(row->look_command));
        memcpy(row->look_delta,o->look.angles.delta_888,sizeof(row->look_delta));
        memcpy(row->look_offset,o->look.angles.offset_894,sizeof(row->look_offset));
        memcpy(row->look_vector,o->look.angles.vector_870,sizeof(row->look_vector));
        if(o->script_look.active){
            row->look.active=1;row->look.event=o->script_look.event;
            row->look.target_uid=o->script_look.target_uid;
            memcpy(row->look.position,o->script_look.position,sizeof(row->look.position));
        }
        if(o->script_move.active){
            rf_npc_checkpoint_move *m=&row->move;
            m->active=1;m->event=o->script_move.event;m->follow=o->script_move.follow;
            m->path_index=o->script_move.path_index;m->path_mode=o->script_move.path_mode;
            m->path_reverse=o->script_move.path_reverse;m->path_count=o->script_move.path.count;
            m->route_index=o->script_move.route_index;m->retry=o->script_move.retry;
            m->retained_count=o->navigation.retained.count;
            memcpy(m->target,o->script_move.target,sizeof(m->target));m->fall_speed=o->script_move.fall_speed;
            memcpy(m->route_start,o->navigation.start.position,sizeof(m->route_start));
            memcpy(m->route_goal,o->navigation.goal.position,sizeof(m->route_goal));
            if(m->retained_count>4)return scene_npc_checkpoint_reject(index,"route_count",0,m->retained_count,RF_FORMAT);
            for(i=0;i<m->retained_count;i++){
                status=scene_npc_checkpoint_route_ordinal(o,o->navigation.retained.nodes[i],m->retained_nodes+i);
                if(status)return scene_npc_checkpoint_reject(index,"route_node",i,0,status);
            }
        }
    }
    if(o->registration.view&&(o->script_animation.active||campaign_poses.items[index].playback.completion.active.count)){
        if(key->retired)return RF_RANGE;
        row->animation_present=1;row->script_animation.active=o->script_animation.active;
        row->script_animation.loop=o->script_animation.active?o->script_animation.loop:0;row->script_animation.freeze=o->script_animation.active?o->script_animation.freeze:0;
        row->script_animation.motion=o->script_animation.active?o->script_animation.motion:0;
        row->playback=campaign_poses.items[index].playback;row->controller=campaign_poses.items[index].controller;
        memset(row->playback.completion.active.slots+row->playback.completion.active.count,0,
            (16-row->playback.completion.active.count)*sizeof(row->playback.completion.active.slots[0]));
    }
    return RF_OK;
}
static int scene_npc_checkpoint_row(uint32_t index,int32_t now,rf_npc_checkpoint_record *row)
{return scene_npc_checkpoint_row_mode(index,now,row,0);}
static int scene_npc_checkpoint_row_compare(const void *a,const void *b)
{uint32_t x=((const rf_npc_checkpoint_record*)a)->uid,y=((const rf_npc_checkpoint_record*)b)->uid;return x<y?-1:x>y;}
static int scene_npc_checkpoint_capture(const rf_npc_checkpoint_catalog *catalog,int32_t now,
    rf_npc_checkpoint_record *rows,uint32_t capacity,uint32_t *count)
{
    rf_npc_checkpoint_record row;unsigned char encoded[RF_NPC_CHECKPOINT_HEADER+RF_NPC_CHECKPOINT_ROW_MAX],identity[32]={0};
    uint32_t i,j,n=0,written;int status;
    if(!catalog||!count||now<0||now>RF_TIMER_PERIOD||campaign_npc_body_count>RF_CAMPAIGN_ACTOR_SLOTS||
       (campaign_npc_body_count&&(!campaign_npc_bodies||!campaign_seeds.items||!campaign_seeds.records.items||
        campaign_seeds.records.count<campaign_npc_body_count)))return RF_RANGE;
    status=rf_npc_checkpoint_encode(identity,catalog,NULL,0,encoded,sizeof(encoded),&written);if(status)return status;
    for(i=0;i<campaign_npc_body_count;i++){
        status=scene_npc_checkpoint_row(i,now,&row);if(status==RF_NOT_FOUND)continue;if(status)return status;
        status=rf_npc_checkpoint_encode(identity,catalog,&row,1,encoded,sizeof(encoded),&written);
        if(status){
#ifndef RF_IMAGE_XBOX_NATIVE
            if(getenv("RF_REPLAY_WORLD_CHECKPOINT_PROBE"))printf("WORLD_CHECKPOINT_NPC_CODEC uid%u health%.9g dead%u flags%u action%d frozen%u count%u controller%d/%d script%u phase%.9g\n",
                row.uid,(double)row.health,row.dead_pose,row.death_flags_810,row.death_action,
                row.playback.completion.frozen,row.playback.completion.active.count,row.controller.current,row.controller.next,
                row.script_animation.active,(double)row.playback.phase);
            if(getenv("RF_REPLAY_WORLD_CHECKPOINT_PROBE"))printf("WORLD_CHECKPOINT_NPC_CODEC_DETAIL uid%u flags%u mode%d primary%d secondary%d anim%u slot%d tick%d weight%.9g slots%d/%d/%d primary_flag%u generation%u mask%u duration%.9g elapsed%.9g override%d/%u\n",
                row.uid,row.flags,row.ai_mode,row.primary,row.secondary,row.animation_present,
                row.playback.completion.active.slots[0].motion,row.playback.completion.active.slots[0].tick,
                (double)row.playback.completion.active.slots[0].weight,row.playback.completion.active.freeze_slot,
                row.playback.completion.active.primary_slot,row.playback.completion.active.dominant_slot,
                row.playback.completion.primary_flag,row.playback.generation,row.playback.event_mask,
                (double)row.controller.duration,(double)row.controller.elapsed,row.controller.override_state,row.controller.override_enabled);
#endif
            return scene_npc_checkpoint_reject(i,"codec",0,row.uid,status);
        }
        for(j=0;j<i;j++)if(campaign_npc_bodies[j].persistence_registered&&
            (uint32_t)campaign_seeds.records.items[j].record.uid==row.uid)return RF_FORMAT;
        n++;
    }
    if(n>capacity||(n&&!rows))return RF_RANGE;
    n=0;for(i=0;i<campaign_npc_body_count;i++)if(campaign_npc_bodies[i].persistence_registered){
        /* No callbacks/mutations occurred since preflight; this pass cannot fail. */
        (void)scene_npc_checkpoint_row(i,now,rows+n);n++;
    }
    if(n)qsort(rows,n,sizeof(*rows),scene_npc_checkpoint_row_compare);*count=n;return RF_OK;
}
#endif
