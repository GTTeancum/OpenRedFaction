#ifndef RF_SCENE_GUARD_RETREAT_CHECKPOINT_INCLUDED
#define RF_SCENE_GUARD_RETREAT_CHECKPOINT_INCLUDED
/* RFNC28 projection. These functions are read-only until assignment; they
 * never replay hits, reset, RNG, routing, audio, ammo or reload completion. */
typedef struct scene_guard_retreat_checkpoint_stage {
    uint32_t slot,handle,player,participates,fire_deadline,navigation_deadline;
    uint64_t epoch,lifetime;
    rf_npc_checkpoint_guard_retreat saved;
    scene_medic_flight_snapshot candidate;
    scene_medic_flight_state before;
    rf_entity_navigation_candidate start_before;
    int32_t resume_before;
} scene_guard_retreat_checkpoint_stage;
static int scene_guard_retreat_checkpoint_nodes(const campaign_npc_body *o,
    const scene_medic_flight_snapshot *p)
{
    const scene_medic_flight_nav *n=&p->state.nav;int status;
    if(n->count<2||n->count>4||n->previous<0||n->current!=n->previous+1||
       (uint32_t)n->current>=n->count||n->start_valid!=1||n->eye_valid!=1||
       memcmp(n->pair,n->prefix+n->previous,sizeof(n->pair)))return RF_FORMAT;
    status=scene_medic_flight_nav_admit(o,n,&p->start);if(status)return status;
    return RF_OK;
}
static int scene_guard_retreat_checkpoint_capture(uint32_t index,const campaign_npc_body *o,
    int32_t now,uint32_t frame,rf_npc_checkpoint_guard_retreat *out)
{
    rf_npc_checkpoint_guard_retreat g={0};scene_medic_flight_snapshot p={0};int status;
    if(!out||!o||index>=campaign_npc_body_count||o!=campaign_npc_bodies+index)return RF_RANGE;
    if(o->guard_retreat_fault)return RF_FORMAT;
    if(!scene_guard_retreat_active(o)){*out=g;return RF_OK;}
    status=scene_guard_retreat_live(index,frame);if(status)return status;
    p.state=o->flight;p.state.target_handle=0;
    status=scene_medic_flight_nav_capture(o,&p.state.nav,&p.start);if(status)return status;
    status=scene_guard_retreat_checkpoint_nodes(o,&p);if(status)return status;
    status=scene_guard_retreat_resources(index,&o->selection,0);if(status)return status;
    g.present=1;g.resume_action=o->guard_resume_action;
    {const int32_t deadlines[2]={o->flight.deadline_250,o->flight.reset_deadline_1370};
     for(uint32_t j=0;j<2;j++){
        if(deadlines[j]<0)return RF_FORMAT;
        status=rf_timer_remaining(deadlines[j],now,g.remaining+j);if(status)return status;
        if(g.remaining[j]<0)g.remaining[j]=0;
        if(g.remaining[j]>RF_TIMER_PERIOD/2)return RF_RANGE;
     }}
    status=rf_timer_remaining(p.state.nav.wait_134,now,&g.wait_remaining);if(status)return status;
    if(g.wait_remaining<0)g.wait_remaining=0;
    if(g.wait_remaining>RF_TIMER_PERIOD/2)return RF_RANGE;
    for(uint32_t j=0;j<2;j++){g.pair[j].kind=p.state.nav.pair[j].kind;g.pair[j].ordinal=p.state.nav.pair[j].ordinal;}
    for(uint32_t j=0;j<4;j++){g.prefix[j].kind=p.state.nav.prefix[j].kind;g.prefix[j].ordinal=p.state.nav.prefix[j].ordinal;}
    g.count=p.state.nav.count;g.previous=p.state.nav.previous;g.current=p.state.nav.current;
    memcpy(g.turn,p.state.nav.turn_138,12);memcpy(g.eye,p.state.nav.request_eye_14c,12);
    g.bias=p.state.nav.bias_158;memcpy(g.steering,p.state.nav.steering_174,12);
    g.approach=p.state.nav.approach_15c;g.activity=p.state.nav.activity_170;
    g.start_valid=p.state.nav.start_valid;g.eye_valid=p.state.nav.eye_valid;
    memcpy(g.start_position,p.start.position,12);memcpy(g.start_query,p.start.query_point,12);
    g.start_radius=p.start.radius;g.start_height=p.start.height;g.start_word=p.start.word_040;
    memcpy(g.command,o->command_714,12);memcpy(g.angular,o->body.state.vector_c8,12);memcpy(g.torque,o->body.state.vector_ec,12);
    g.combat_burst=o->combat_burst_remaining;g.fire_remaining=o->combat_due>frame?o->combat_due-frame:0;
    g.navigation_remaining=o->combat_navigation_due>frame?o->combat_navigation_due-frame:0;
    g.spread_rng=campaign_enemy_spread_random.value;
    *out=g;return RF_OK;
}
static int scene_guard_retreat_checkpoint_prepare(uint32_t index,const rf_npc_checkpoint_record *row,
    int32_t now,uint32_t frame,scene_guard_retreat_checkpoint_stage *out)
{
    scene_guard_retreat_checkpoint_stage value={0};scene_medic_flight_snapshot *p=&value.candidate;
    const rf_npc_checkpoint_guard_retreat *g;const campaign_npc_body *o;rf_entity_motion_selection selection;int status;
    if(!row||!out||index>=campaign_npc_body_count||!campaign_npc_bodies)return RF_RANGE;
    o=campaign_npc_bodies+index;g=&row->guard_retreat;
    if(o->guard_retreat_fault)return RF_FORMAT;
    value.participates=scene_guard_retreat_active(o)||g->present;
    if(!value.participates){*out=value;return RF_OK;}
    value.slot=index;value.handle=o->registration.handle;value.player=campaign_player_object.handle;
    value.epoch=scene_continuous_charge_runtime.epoch;value.lifetime=scene_medic_flight_lifetime;
    value.before=o->flight;value.start_before=o->navigation.start;value.resume_before=o->guard_resume_action;
    value.saved=*g;
    if(!g->present){*out=value;return RF_OK;}
    if(g->present!=1||row->flight.present||row->retired||row->dead_pose||row->health<=0||row->ai_mode!=7||
       row->combat_alert!=1||row->combat.active||row->shot_count||row->move.active||row->look.active||
       row->script_animation.active||row->controller_uid||row->route.flags||row->physics.scripted||row->airborne.present||
       !row->movement_present||row->movement_slot>=16||
       (campaign_modes[row->movement_slot].index!=1&&campaign_modes[row->movement_slot].index!=3)||
       (g->resume_action!=0&&g->resume_action!=2)||
       g->fire_remaining>3600||g->navigation_remaining>3600||g->combat_burst>256||
       frame>UINT32_MAX-g->fire_remaining||frame>UINT32_MAX-g->navigation_remaining)return RF_FORMAT;
    status=rf_entity_motion_selection_base(&campaign_motion_catalog,&campaign_base_motions,row->class_id,&selection);if(status)return status;
    status=rf_entity_motion_selection_weapon(&campaign_motion_catalog,&campaign_base_motions,row->class_id,row->primary,&selection);if(status)return status;
    status=scene_guard_retreat_binding(index,row->primary,row->secondary,&row->inventory,&selection,1);if(status)return status;
    status=scene_guard_retreat_player_onfoot(scene_actor_collision_owner);if(status)return status;
    p->state.qualified=2;p->state.entered=1;p->state.target_kind=SCENE_FLIGHT_TARGET_PLAYER;
    p->state.sight_deadline_294=p->state.no_route_deadline_4d4=p->state.stall_deadline_4d0=-1;
    if(g->remaining[0]<0||g->remaining[1]<0||g->remaining[0]>RF_TIMER_PERIOD/2||g->remaining[1]>RF_TIMER_PERIOD/2||
       g->wait_remaining<0||g->wait_remaining>RF_TIMER_PERIOD/2)return RF_RANGE;
    status=rf_timer_set(&p->state.deadline_250,now,g->remaining[0]);if(status)return status;
    status=rf_timer_set(&p->state.reset_deadline_1370,now,g->remaining[1]);if(status)return status;
    status=rf_timer_set(&p->state.nav.wait_134,now,g->wait_remaining);if(status)return status;
    for(uint32_t j=0;j<2;j++){p->state.nav.pair[j].kind=g->pair[j].kind;p->state.nav.pair[j].ordinal=g->pair[j].ordinal;}
    for(uint32_t j=0;j<4;j++){p->state.nav.prefix[j].kind=g->prefix[j].kind;p->state.nav.prefix[j].ordinal=g->prefix[j].ordinal;}
    p->state.nav.count=g->count;p->state.nav.previous=g->previous;p->state.nav.current=g->current;
    memcpy(p->state.nav.turn_138,g->turn,12);memcpy(p->state.nav.request_eye_14c,g->eye,12);
    p->state.nav.bias_158=g->bias;memcpy(p->state.nav.steering_174,g->steering,12);
    p->state.nav.approach_15c=g->approach;p->state.nav.activity_170=g->activity;
    p->state.nav.start_valid=g->start_valid;p->state.nav.eye_valid=g->eye_valid;
    memcpy(p->start.position,g->start_position,12);memcpy(p->start.query_point,g->start_query,12);
    p->start.radius=g->start_radius;p->start.height=g->start_height;p->start.word_040=g->start_word;
    memcpy(p->command_714,g->command,12);memcpy(p->angular_150,g->angular,12);memcpy(p->torque_174,g->torque,12);
    status=scene_guard_retreat_checkpoint_nodes(o,p);if(status)return status;
    /* Existing independent physics owner, when present, must say exactly the
     * same thing. Look708 has no duplicate tail and remains row.look_command. */
    if(row->physics.present&&(memcmp(g->angular,row->physics.angular,12)||memcmp(g->torque,row->physics.torque,12)))return RF_FORMAT;
    value.fire_deadline=frame+g->fire_remaining;value.navigation_deadline=frame+g->navigation_remaining;
    *out=value;return RF_OK;
}
static int scene_guard_retreat_checkpoint_validate(const scene_guard_retreat_checkpoint_stage *stage)
{
    const campaign_npc_body *o;
    if(!stage)return RF_RANGE;if(!stage->participates)return RF_OK;
    if(stage->epoch!=scene_continuous_charge_runtime.epoch||stage->lifetime!=scene_medic_flight_lifetime||
       !campaign_npc_bodies||stage->slot>=campaign_npc_body_count)return RF_FORMAT;
    o=campaign_npc_bodies+stage->slot;
    if(o->guard_retreat_fault||o->registration.handle!=stage->handle||
       o->guard_resume_action!=stage->resume_before||memcmp(&o->flight,&stage->before,sizeof(o->flight))||
       memcmp(&o->navigation.start,&stage->start_before,sizeof(stage->start_before))||
       !scene_flight_owner_live(o)||stage->player!=campaign_player_object.handle||
       campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,stage->player)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)stage->player)!=&campaign_player_view)return RF_FORMAT;
    return RF_OK;
}
static void scene_guard_retreat_checkpoint_assign(const scene_guard_retreat_checkpoint_stage *stage)
{
    campaign_npc_body *o;if(!stage->participates)return;o=campaign_npc_bodies+stage->slot;
    /* All candidate/resource/placement seals and storage close succeeded.
     * No fallible transition or live cancellation callback occurs here. */
    if(!stage->saved.present){
        if(o->flight.qualified==2)memset(&o->flight,0,sizeof(o->flight));
        o->guard_resume_action=0;return;
    }
    o->flight=stage->candidate.state;o->flight.target_handle=stage->player;
    o->guard_resume_action=stage->saved.resume_action;
    scene_medic_flight_nav_publish(o,&stage->candidate.state.nav,&stage->candidate.start);
    memcpy(o->command_714,stage->candidate.command_714,12);
    memcpy(o->body.state.vector_c8,stage->candidate.angular_150,12);memcpy(o->body.state.vector_ec,stage->candidate.torque_174,12);
    o->combat_alert=1;o->combat_scripted=0;o->combat_target=stage->player;
    o->combat_due=stage->fire_deadline;o->combat_navigation_due=stage->navigation_deadline;
    o->combat_burst_remaining=stage->saved.combat_burst;campaign_enemy_spread_random.value=stage->saved.spread_rng;
    o->ai_mode.flags_530&=~8u;o->view.flags_7d0&=~8u;o->firing.flags_7d0&=~8u;
    o->view.flags_810&=~0x02000000u;o->damage.effects.flags_810&=~0x02000000u;
    o->ai_override.action_1364=o->ai_override.motion_1368=o->ai_override.word_834=-1;
}
#endif
