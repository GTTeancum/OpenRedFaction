#ifndef RF_SCENE_NOISE_INVESTIGATION_INCLUDED
#define RF_SCENE_NOISE_INVESTIGATION_INCLUDED
/*408477..408520 /4052f0: Waiting armed listeners investigate a fixed noise
 * point after500ms. No target grant, moving-player tracking or search timer.
 * The existing16-unit loud-shot and ordinary sight policies remain in force. */
static int scene_noise_authored(uint32_t slot)
{
    const rf_level_owned_entity *r;const rf_entity_seed_class *c;rf_level_entity_ai ai;
    char held[2][RF_LEVEL_NAME_CAPACITY];const uint8_t *raw;uint32_t cls,at=4,size;int status,style;
    static const int32_t overrides[4]={-1,1,3,2}; /*464acf ->59c028*/
    if(!campaign_seeds.items||!campaign_seeds.records.items||!campaign_seeds.classes||slot>=campaign_seeds.records.count)return RF_RANGE;
    r=campaign_seeds.records.items+slot;cls=campaign_seeds.items[slot].class_index;
    if(!scene_noise_classes.valid||scene_noise_classes.count!=campaign_seeds.class_count||cls>=scene_noise_classes.count)return RF_FORMAT;
    c=campaign_seeds.classes+cls;
    if((c->physics.flags&0x20011u)!=0x20011u||(c->physics.flags&(0x200u|0x2000u|0x400000u))||
       (c->physics.flags2&0x100u)||c->physics.use_kind==1||c->model_kind!=2||
       !isfinite(c->physics.mass)||c->physics.mass<=0||c->physics.movement_index>=16||
       (campaign_modes[c->physics.movement_index].index!=1&&campaign_modes[c->physics.movement_index].index!=3)||
       campaign_seeds.items[slot].spawn.seat_host_uid!=-1)return RF_NOT_FOUND;
    status=rf_level_entity_ai_read(r,&ai);if(status)return status;
    if(ai.mode!=1||ai.attack_style>=4)return RF_NOT_FOUND;
    style=overrides[ai.attack_style];if(style<0)style=scene_noise_classes.style[cls];
    if(style!=1&&style!=3)return RF_NOT_FOUND;
    status=rf_level_entity_held_clutter_read(r,held);if(status)return status;
    for(uint32_t j=0;j<2;j++)if(held[j][0]&&!scene_npc_loadout_equal(held[j],"none"))return RF_NOT_FOUND;
    raw=r->raw;size=r->record.bytes;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<48)return RF_FORMAT;at+=48;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<13)return RF_FORMAT;at+=13;
    for(uint32_t j=0;j<2;j++){status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;}
    if(at>size||size-at<29)return RF_FORMAT;at+=29;
    for(uint32_t j=0;j<7;j++){status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;}
    if(at>size||size-at<35)return RF_FORMAT;
    for(uint32_t j=14;j<18;j++)if(raw[at+j]!=255)return RF_NOT_FOUND; /* No Alarm. */
    return RF_OK;
}
static int scene_noise_binding(uint32_t slot,int32_t primary,const rf_weapon_inventory *inventory,
    const rf_entity_motion_selection *selection)
{
    campaign_enemy_weapon_selection weapon={0};rf_entity_motion_selection bound;
    const campaign_model_owner *model;const rf_entity_pose *pose;uint32_t cls;int status;
    const int32_t ids[8]={campaign_pistol_id,campaign_rifle_id,campaign_riot_id,campaign_shotgun_id,
        campaign_rocket_id,campaign_grenade_id,campaign_sniper_id,campaign_rail_id};
    status=scene_noise_authored(slot);if(status)return status;
    if(primary<0||primary>=64||!inventory||!selection||inventory->owned[primary]!=1)return RF_NOT_FOUND;
    status=campaign_enemy_weapon_select(primary,ids,campaign_primary,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,&weapon);
    if(status==RF_NOT_FOUND)status=campaign_enemy_extra_weapon_select(primary,&campaign_weapon_supply,&weapon);
    if(status)return status;
    if(!weapon.primary||(!weapon.melee&&!weapon.ammo))return RF_NOT_FOUND;
    if(slot>=campaign_npc_body_count||!campaign_npc_bodies)return RF_FORMAT;
    status=scene_npc_reload_identity(slot,campaign_npc_bodies+slot);if(status)return status;
    if(!campaign_npc_bodies[slot].body.allocated_bytes)return RF_NOT_FOUND;
    if(!campaign_model_owners||slot>=campaign_model_owner_count||!campaign_poses.items||slot>=campaign_poses.count)return RF_FORMAT;
    model=campaign_model_owners+slot;pose=campaign_poses.items+slot;
    if(!model->registration.loaded||model->owned||model->pose!=pose||
       model->registration.active!=&pose->playback.completion.active||!pose->matrices||!pose->generations||
       !pose->bone_count||!model->registration.next||!model->registration.previous||
       model->registration.next->previous!=&model->registration||model->registration.previous->next!=&model->registration)return RF_NOT_FOUND;
    cls=campaign_seeds.items[slot].class_index;
    status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,cls,&bound);
    if(!status)status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,cls,primary,&bound);
    if(status)return status;
    if(pose->skeleton!=selection->mapping.skeleton||memcmp(&bound.mapping,&selection->mapping,sizeof(bound.mapping))||
       memcmp(bound.action_sounds,selection->action_sounds,sizeof(bound.action_sounds))||
       memcmp(bound.action_declarations,selection->action_declarations,sizeof(bound.action_declarations)))return RF_FORMAT;
    return scene_patrol_resources(slot,selection,0);
}
static int scene_noise_owner(uint32_t slot,uint32_t active)
{
    const campaign_npc_body *o;uint32_t flags;int status;
    if(slot>=campaign_npc_body_count||!campaign_npc_bodies)return RF_RANGE;o=campaign_npc_bodies+slot;
    status=scene_npc_reload_identity(slot,o);if(status)return RF_NOT_FOUND;
    flags=o->ai_mode.flags_530|o->view.flags_7d0|o->firing.flags_7d0;
    if(!scene_npc_reload_standalone(o)||o->registration.handle==campaign_player_object.handle||
       !isfinite(o->damage.effects.health)||o->damage.effects.health<=0||o->damage.effects.affiliation!=0||
       (o->object_flags&(2u|0x4000u))||(o->view.flags_810&1u)||
       ((o->view.flags_814|o->damage.effects.flags_814)&0x10u)||
       (flags&(SCENE_NPC_GOTO_AI_SUPPRESSION|0x800u|0x800000u))||!scene_guard_retreat_canonical(o)||
       o->view.action_520!=o->ai_mode.action_280||
       (active?o->ai_mode.action_280!=6:(o->ai_mode.action_280!=0&&o->ai_mode.action_280!=2))||
       !o->body.allocated_bytes||!o->persistence_registered||
       o->flight.qualified||o->guard_retreat_fault||o->alarm.present||o->alarm_fault||o->patrol.enabled||o->patrol.dwell_present||
       o->medic.pending||o->controller_handle||o->script_look.active||o->script_shoot.active||o->script_once.count||
       o->script_animation.active||o->script_animation.freeze||o->scripted_physics||o->scripted_physics_seen||
       scene_npc_seat_attached(o->registration.handle)||o->combat_alert||o->combat_target||o->combat_scripted||
       o->combat_burst_remaining||o->combat_due||o->combat_navigation_due||o->combat_reload_pending||o->combat_reload_due||
       o->movement_slot>=16||(campaign_modes[o->movement_slot].index!=1&&campaign_modes[o->movement_slot].index!=3)||
       !isfinite(o->movement.speed)||o->movement.speed<=0||
       (!active&&(o->ai_mode.state_2b4||o->script_move.active||o->script_move.origin||
          o->script_move.event||o->script_move.path.count||o->script_move.path.indices||o->navigation.retained.count)))return RF_NOT_FOUND;
    for(uint32_t j=0;j<64;j++)if(o->firing.active[j])return RF_NOT_FOUND;
    if(!campaign_poses.items||slot>=campaign_poses.count||campaign_poses.items[slot].playback.completion.frozen||
       campaign_poses.items[slot].playback.completion.active.freeze_slot!=-1)return RF_NOT_FOUND;
    return scene_noise_binding(slot,o->view.weapons[0],&o->inventory,&o->selection);
}
uint32_t rf_scene_noise_live_trace[20]; /* calls,slot,owner status,move fields,route fields,state,target mismatch */
static int scene_noise_live(uint32_t slot)
{
    const campaign_npc_body *o;int status;
    if(slot>=campaign_npc_body_count||!campaign_npc_bodies)return RF_RANGE;o=campaign_npc_bodies+slot;
    if(!scene_noise_active(o))return RF_OK;
    status=scene_noise_owner(slot,1);
    ++rf_scene_noise_live_trace[0];rf_scene_noise_live_trace[1]=slot;rf_scene_noise_live_trace[2]=(uint32_t)status;
    rf_scene_noise_live_trace[3]=o->script_move.active;rf_scene_noise_live_trace[4]=o->script_move.follow;
    rf_scene_noise_live_trace[5]=o->script_move.event;rf_scene_noise_live_trace[6]=o->script_move.origin;
    rf_scene_noise_live_trace[7]=o->script_move.path.count;rf_scene_noise_live_trace[8]=o->script_move.path.indices!=NULL;
    rf_scene_noise_live_trace[9]=o->script_move.path_index;rf_scene_noise_live_trace[10]=o->script_move.path_mode;
    rf_scene_noise_live_trace[11]=o->script_move.path_reverse;rf_scene_noise_live_trace[12]=o->script_move.stop;
    rf_scene_noise_live_trace[13]=o->script_move.retry;rf_scene_noise_live_trace[14]=o->navigation.retained.count;
    rf_scene_noise_live_trace[15]=o->script_move.route_index;rf_scene_noise_live_trace[16]=o->ai_mode.state_2b4;
    rf_scene_noise_live_trace[17]=memcmp(o->script_move.target,o->navigation.goal.position,12)!=0;
    rf_scene_noise_live_trace[18]=rf_scene_profile_stage[0];rf_scene_noise_live_trace[19]=campaign_seeds.records.items[slot].record.uid;
    if(status)return status;
    if(o->script_move.active!=1||o->script_move.follow!=2||o->script_move.event||o->script_move.origin||
       o->script_move.path.count||o->script_move.path.indices||o->script_move.path_index||o->script_move.path_mode||
       o->script_move.path_reverse||o->script_move.stop||o->script_move.retry>30||o->navigation.retained.count>4||
       (o->script_move.retry&&(o->navigation.retained.count||o->script_move.route_index!=1))||
       (o->navigation.retained.count&&(!o->script_move.route_index||o->script_move.route_index>o->navigation.retained.count))||
       o->ai_mode.state_2b4!=(o->script_move.retry?12:1)||
       memcmp(o->script_move.target,o->navigation.goal.position,12))return RF_FORMAT;
    for(uint32_t j=0;j<3;j++)if(!isfinite(o->script_move.target[j]))return RF_FORMAT;
    return RF_OK;
}
static int scene_noise_live_present(uint32_t *present)
{
    int status;if(!present)return RF_RANGE;*present=0;
    for(uint32_t i=0;i<campaign_npc_body_count;i++)if(scene_noise_active(campaign_npc_bodies+i)){
        status=scene_noise_live(i);if(status)return status;*present=1;
    }
    return RF_OK;
}
static int scene_noise_candidate(uint32_t slot,const rf_npc_checkpoint_record *r,
    const rf_entity_motion_selection *selection)
{
    int status;if(!r||!selection)return RF_RANGE;if(r->ai_mode!=6)return RF_OK;
    /* Recheck owner semantics at the final seal too, not just wire decode. */
    if(r->retired||r->dead_pose||!isfinite(r->health)||r->health<=0||r->flags||r->affiliation||
       r->combat_alert||r->combat.active||r->look.active||r->shot_count||r->reload_present||
       r->route.flags||r->flight.present||r->guard_retreat.present||r->alarm.present||r->drone.kind||
       r->medic.reserve_seen||r->medic.child_state||r->controller_uid||r->physics.present||r->ai_suppressed||
       !r->animation_present||r->script_animation.active||r->script_animation.freeze||
       r->playback.completion.frozen||r->playback.completion.active.freeze_slot!=-1||
       r->move.active!=1||r->move.follow!=2||r->move.event||r->move.path_count||r->move.path_index||
       r->move.path_mode||r->move.path_reverse||r->move.retry>30||r->move.retained_count>4||!r->move.route_index||
       (r->move.retained_count&&r->move.route_index>r->move.retained_count)||
       (r->move.retry&&(r->move.retained_count||r->move.route_index!=1))||
       memcmp(r->move.target,r->move.route_goal,12))return RF_FORMAT;
    for(uint32_t j=0;j<3;j++)if(!isfinite(r->move.target[j])||!isfinite(r->move.route_start[j]))return RF_FORMAT;
    status=scene_noise_binding(slot,r->primary,&r->inventory,selection);if(status)return status;
    {const campaign_npc_body *o=campaign_npc_bodies+slot;
     /* These retained flags/overrides are not erased by an incoming living
      * row. Refuse resurrection or an inherited nonrepresentable owner. */
     if(o->registration.handle==campaign_player_object.handle||(o->object_flags&2u)||
        (o->view.flags_810&1u)||((o->view.flags_814|o->damage.effects.flags_814)&0x10u)||
        ((o->ai_mode.flags_530|o->view.flags_7d0|o->firing.flags_7d0)&(0x800u|0x800000u))||
        !scene_guard_retreat_canonical(o))return RF_NOT_FOUND;
     for(uint32_t j=0;j<64;j++)if(o->firing.active[j])return RF_NOT_FOUND;}
    if(r->class_id!=campaign_seeds.items[slot].class_index||r->uid!=(uint32_t)campaign_seeds.records.items[slot].record.uid||
       !r->movement_present||r->movement_slot>=16||
       (campaign_modes[r->movement_slot].index!=1&&campaign_modes[r->movement_slot].index!=3)||
       !scene_npc_reload_standalone(campaign_npc_bodies+slot))return RF_FORMAT;
    /* Two future motion states may crossfade with existing admitted slots.
     * Reserve the same actual slot contribution as live onset, without
     * demanding payloads or mutating references in this candidate check. */
    {uint32_t slots=r->playback.completion.active.count;
     if(slots>16)return RF_FORMAT;
     for(uint32_t k=0;k<2;k++){
        int32_t id=selection->mapping.states[k?2:0];uint32_t j;
        for(j=0;j<r->playback.completion.active.count;j++)if(r->playback.completion.active.slots[j].motion==id)break;
        if(j==r->playback.completion.active.count&&(!k||id!=selection->mapping.states[0]))++slots;
     }
     if(slots>16)return RF_NOT_FOUND;}
    /* Wire validation proves scalar exclusivity. Scene adds typed graph
     * identities and source-derived loadout/motion metadata, before demand. */
    for(uint32_t j=0;j<r->move.retained_count;j++){
        uint32_t n=r->move.retained_nodes[j];
        if(n==UINT32_MAX||n==UINT32_MAX-1)continue;
        if(!n||n>campaign_navigation.count||!campaign_navigation.references||
           !campaign_navigation.references[n-1].candidate)return RF_FORMAT;
    }
    return RF_OK;
}
static int scene_noise_hear_player_shot(scene_stream *stream,const float position[3],float radius,
    uint32_t frame,uint32_t source,uint64_t epoch,uint32_t slot,uint32_t *handled)
{
    scene_npc_reload_player_receipt player;campaign_npc_body *o;float distance=0,delta[3],eye_distance=0,forward=0;
    uint32_t blocked,handle;int status;
    if(!handled||!stream||!position||slot>=campaign_npc_body_count)return RF_RANGE;*handled=0;
    if(!frame||scene_medic_flight_restoring||campaign_cutscene_runtime.active||campaign_endgame.phase||
       epoch!=scene_continuous_charge_runtime.epoch||source!=campaign_player_object.handle||
       !isfinite(campaign_player_damage.state.effects.health)||campaign_player_damage.state.effects.health<=0)return RF_OK;
    status=scene_noise_owner(slot,0);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    o=campaign_npc_bodies+slot;
    for(uint32_t j=0;j<3;j++){
        float d=o->published[j]-position[j];distance+=d*d;
        delta[j]=position[j]-o->eye_position[j];eye_distance+=delta[j]*delta[j];forward+=o->look.orientation[6+j]*delta[j];
    }
    if(!isfinite(distance)||distance>radius*radius)return RF_OK;
    status=scene_npc_reload_player_admit(stream,&player);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(source!=player.player||stream!=scene_actor_collision_owner)return RF_OK;
    /* Original checks FOV then noise-point obstruction. Use the established
     * port120-degree cone; a clear visible point retains ordinary hearing. */
    if(eye_distance<.0001f)return RF_OK;
    if(forward>0&&forward*forward>=eye_distance*.25f){
        status=combat_obstructed(stream,o->eye_position,delta,&blocked);if(status)return status;
        if(!blocked)return RF_OK;
    }
    *handled=1; /* Resource refusal cannot grant through-wall player pursuit. */
    if(!campaign_navigation.count||!campaign_navigation_workspace.storage||
       !campaign_navigation_workspace.references||!campaign_navigation_workspace.scratch)return RF_OK;
    handle=o->registration.handle;
    status=scene_patrol_resources(slot,&o->selection,1);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    if(epoch!=scene_continuous_charge_runtime.epoch||source!=campaign_player_object.handle||
       scene_npc_reload_player_seal(stream,&player))return RF_OK;
    if(o!=campaign_npc_bodies+slot||o->registration.handle!=handle)return RF_OK;
    status=scene_noise_owner(slot,0);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    status=campaign_script_locomotion(slot,0);if(status)return status;
    memset(&o->script_move,0,sizeof(o->script_move));memset(&o->navigation,0,sizeof(o->navigation));
    o->script_move.active=1;o->script_move.follow=2;o->script_move.route_index=1;o->script_move.retry=30;
    memcpy(o->script_move.target,position,12);memcpy(o->navigation.goal.position,position,12);
    memcpy(o->navigation.start.position,o->body.state.position,12);
    o->ai_mode.action_280=o->view.action_520=6;o->ai_mode.state_2b4=12;
    o->ai_mode.clock_288=o->ai_mode.clock_2bc=scene_medic_now(frame);
    o->ai_mode.argument_28c=o->ai_mode.argument_290=UINT32_MAX;
    return RF_OK;
}
#endif
