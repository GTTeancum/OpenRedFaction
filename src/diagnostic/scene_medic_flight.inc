#ifndef RF_SCENE_MEDIC_FLIGHT_INCLUDED
#define RF_SCENE_MEDIC_FLIGHT_INCLUDED
/* Known L3S1 medic783 only. Original407fb0/4085f0/404f70; reviewed private
 * navigation is separate. No global AI dispatch or optional persona service. */
static uint32_t scene_medic_flight_rng_seen,scene_medic_flight_restoring;
static uint64_t scene_medic_flight_lifetime; /* Runtime-only replacement fence. */
/* qualified, requests, entries, extensions, defaults, ticks, pain pauses,
 * no-route, sight refresh, unsupported, last source, status */
uint32_t rf_scene_medic_flight[12];
static int scene_medic_flight_raw_string(const uint8_t *p,uint32_t size,uint32_t *at)
{
    uint32_t n;
    if(*at>size||size-*at<2)return RF_FORMAT;
    n=p[*at]|(uint32_t)p[*at+1]<<8;*at+=2;
    if(n>size-*at)return RF_FORMAT;
    *at+=n;return RF_OK;
}
static int scene_medic_flight_binding(uint32_t index)
{
    const rf_level_owned_entity *record;rf_level_entity_spawn spawn;rf_level_entity_ai ai;
    const uint8_t *raw;uint32_t at=4,size,i;int status;const char *level=campaign_current_level,*name;
    if(index>=campaign_seeds.records.count||!campaign_seeds.records.items)return RF_RANGE;
    record=campaign_seeds.records.items+index;name=record->record.class_name;
    if(record->record.uid!=783||rf_emitter_name_lookup(&name,1,"medic1")!=0||
       rf_emitter_name_lookup(&level,1,"l3s1.rfl")!=0)return RF_NOT_FOUND;
    status=rf_level_entity_spawn_read(record,&spawn);if(status)return status;
    status=rf_level_entity_ai_read(record,&ai);if(status)return status;
    if(spawn.seat_host_uid!=-1||ai.mode!=1)return RF_NOT_FOUND;
    raw=record->raw;size=record->record.bytes;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<48)return RF_FORMAT;at+=48;
    status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;
    if(at>size||size-at<13)return RF_FORMAT;at+=13;
    for(i=0;i<2;i++){status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;}
    if(at>size||size-at<29)return RF_FORMAT;at+=29;
    for(i=0;i<7;i++){status=scene_medic_flight_raw_string(raw,size,&at);if(status)return status;}
    if(at>size||size-at<35)return RF_FORMAT;
    /*464401/46440c third UID ->76c. No Alarm means4087a0 returnsfalse
     * regardless of the earlier fear scalar, with no fabricated live event. */
    for(i=14;i<18;i++)if(raw[at+i]!=255)return RF_NOT_FOUND;
    if(raw[at+18+11])return RF_NOT_FOUND;
    return RF_OK;
}
static void scene_medic_flight_construct(campaign_npc_body *o,int32_t now,float bias)
{
    scene_medic_flight_state f={0};
    f.qualified=1;f.deadline_250=now;f.sight_deadline_294=now;
    f.no_route_deadline_4d4=now;f.stall_deadline_4d0=now;f.reset_deadline_1370=-1;
    f.nav.previous=f.nav.current=-1;f.nav.wait_134=now;f.nav.approach_15c=1;f.nav.bias_158=bias;
    /* Only steering174 is an approved zero-initialization safety policy.
     * Unwritten request/start geometry has explicit validity bits. */
    o->flight=f;
}
static int scene_medic_flight_initialize(campaign_npc_body *o,uint32_t index,int32_t now,uint32_t ordinal)
{
    static const float biases[3]={-.5f,0,.5f};int status;
    memset(&o->flight,0,sizeof(o->flight));
    status=scene_medic_flight_binding(index);if(status==RF_NOT_FOUND)return RF_OK;
    if(status)return status;
    if(o->medic.default_action!=2||o->view.weapons[0]!=-1||o->view.weapons[1]!=-1)return RF_NOT_FOUND;
    scene_medic_flight_construct(o,now,biases[ordinal%3]);
    o->navigation.start.radius=o->navigation_radius;
    o->navigation.start.height=o->death_item_extent_7c4;o->navigation.start.word_040=0;
    ++rf_scene_medic_flight[0];return RF_OK;
}
static uint32_t scene_medic_flight_live(const campaign_npc_body *o)
{return scene_medic_live(o)&&o->flight.qualified==1;}
static int scene_medic_flight_overlap(const campaign_npc_body *o)
{
    return o->patrol.enabled||o->script_move.active||o->script_move.path.count||
        o->script_move.path.indices||o->navigation.retained.count||o->script_look.active||
        o->script_shoot.active||o->script_once.count||o->combat_alert||o->combat_scripted||
        o->combat_target||o->controller_handle||o->view.linked_handle!=-1||
        o->ai_mode.state_2b4!=0||o->view.weapons[0]!=-1||o->view.weapons[1]!=-1;
}
static int scene_medic_flight_threat(campaign_npc_body *o,uint32_t frame,scene_medic_flight_threat_view *out)
{
    scene_medic_flight_threat_view v={0};int status;
    if(o->flight.target_kind!=SCENE_FLIGHT_TARGET_PLAYER)return RF_NOT_FOUND;
    if(o->flight.target_handle!=campaign_player_object.handle||
       campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,o->flight.target_handle)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)o->flight.target_handle)!=&campaign_player_view)return RF_NOT_FOUND;
    v.handle=o->flight.target_handle;v.view=&campaign_player_view;
    memcpy(v.position,rf_scene_actor_pose.public_position,12);
    status=scene_medic_player_eye(frame,v.eye);if(status)return status;
    v.model_radius=campaign_player_geometry.model_radius;
    if(!isfinite(v.model_radius)||v.model_radius<0)return RF_FORMAT;
    for(uint32_t j=0;j<3;j++)if(!isfinite(v.position[j])||!isfinite(v.eye[j]))return RF_FORMAT;
    *out=v;return RF_OK;
}
static uint32_t scene_medic_flight_random(void)
{
    uint32_t value=0;
    /* The actual core API has two distinct nonnull pointers here and its
     * only refusal is a null argument. Exactly one draw is serviced. */
    (void)rf_random_next(&combat_pain_random,&value);
    scene_medic_flight_rng_seen=1;return value;
}
static int scene_medic_flight_no_route(campaign_npc_body *o,int32_t now)
{
    int status=rf_timer_set(&o->flight.no_route_deadline_4d4,now,
        3000+(int32_t)(scene_medic_flight_random()%3001u));
    if(status)return status;
    /*4085e1: actor150, projected by scene's ordinary angular owner. */
    memset(o->body.state.vector_c8,0,12);++rf_scene_medic_flight[7];return RF_OK;
}
static int scene_medic_flight_default(campaign_npc_body *o,int32_t now)
{
    int status;
    if(scene_medic_flight_restoring||o->ai_mode.action_280!=7||o->view.action_520!=7)return RF_OK;
    status=rf_entity_ai_set_action(&o->ai_mode,2,UINT32_MAX,UINT32_MAX,(float)now,0,0);
    if(status)return status;
    status=rf_entity_ai_set_state(&o->ai_mode,0,(float)now);if(status)return status;
    o->view.action_520=2;++rf_scene_medic_flight[4];return RF_OK;
}
static int scene_medic_flight_event_default(campaign_npc_body *o,int32_t now)
{
    int status;
    if(scene_medic_flight_restoring)return RF_OK;
    /* Source OFF407ee0 is unconditional, including catatonic or healing
     * current states; retained flight metadata and residual43 remain owned. */
    status=rf_entity_ai_set_action(&o->ai_mode,2,UINT32_MAX,UINT32_MAX,(float)now,0,0);
    if(status)return status;
    status=rf_entity_ai_set_state(&o->ai_mode,0,(float)now);if(status)return status;
    if(o->medic.pending){o->medic.pending=0;++rf_scene_medic_action[6];}
    o->view.action_520=2;++rf_scene_medic_flight[4];return RF_OK;
}
static void scene_medic_flight_target_clear(campaign_npc_body *o,int32_t now)
{
    if(scene_medic_flight_restoring||!o->flight.qualified||!o->flight.entered)return;
    if(o->flight.target_kind==SCENE_FLIGHT_TARGET_NONE)return;
    o->flight.target_kind=SCENE_FLIGHT_TARGET_NONE;o->flight.target_handle=0;
    o->flight.sight_deadline_294=now;
}
static void scene_medic_flight_use_target(campaign_npc_body *o,uint32_t target,int32_t now)
{
    if(scene_medic_flight_restoring||!o->flight.qualified||!o->flight.entered)return;
    if(o->flight.target_kind==SCENE_FLIGHT_TARGET_PLAYER&&o->flight.target_handle==target)return;
    /*40a73c installs14, then40a746 binds the actual accepted player through
     *409050. Caller has committed that real target without an intervening callback. */
    o->flight.target_kind=SCENE_FLIGHT_TARGET_PLAYER;o->flight.target_handle=target;
    o->flight.sight_deadline_294=now;
}
static void scene_medic_flight_player_replaced(void)
{
    if(scene_medic_flight_restoring)return;
    scene_medic_flight_player_motion(NULL);++scene_medic_flight_lifetime;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->flight.qualified&&o->flight.target_kind==SCENE_FLIGHT_TARGET_PLAYER&&
           o->flight.target_handle==campaign_player_object.handle){
            o->flight.target_kind=SCENE_FLIGHT_TARGET_UNRESOLVED;o->flight.target_handle=0;
        }
    }
}
static int scene_medic_flight_request(scene_stream *s,campaign_npc_body *o,
    const scene_medic_flight_threat_view *threat,int32_t now,float seconds)
{
    int32_t remaining,duration;uint32_t found=0;int status;
    ++rf_scene_medic_flight[1];
    o->ai_mode.flags_530&=~8u;o->view.flags_7d0&=~8u;o->firing.flags_7d0&=~8u;
    if((o->damage.effects.flags_814&0x10u)||campaign_npc_ai_suppressed(o)||o->ai_mode.action_280==17)return RF_OK;
    status=rf_scene_npc_reset_ai_animation(o->registration.handle,1);if(status)return status;
    status=rf_timer_set(&o->flight.reset_deadline_1370,now,
        30000+(int32_t)(scene_medic_flight_random()%30001u));if(status)return status;
    /*41ae70 with primary-1 is inert; retain actual unarmed reset admission. */
    status=campaign_pain_reset_retained(NULL,o->registration.handle,o->view.weapons[0]);if(status)return status;
    o->view.flags_810&=~0x02000000u;o->damage.effects.flags_810=o->view.flags_810;
    o->ai_override.word_834=-1;duration=(int32_t)((double)seconds*1000.0);
    if(o->ai_mode.action_280==7){
        status=rf_timer_remaining(o->flight.deadline_250,now,&remaining);if(status)return status;
        if(duration<=remaining)return RF_OK;
        if(o->flight.target_kind==SCENE_FLIGHT_TARGET_PLAYER&&o->flight.target_handle==threat->handle){
            status=rf_timer_set(&o->flight.deadline_250,now,duration);
            if(!status)++rf_scene_medic_flight[3];return status;
        }
    }
    status=rf_timer_set(&o->flight.deadline_250,now,duration);if(status)return status;
    status=rf_entity_ai_set_action(&o->ai_mode,7,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    o->view.action_520=7;o->flight.entered=1;
    if(o->flight.target_kind!=SCENE_FLIGHT_TARGET_PLAYER||o->flight.target_handle!=threat->handle){
        o->flight.target_kind=SCENE_FLIGHT_TARGET_PLAYER;o->flight.target_handle=threat->handle;
        o->flight.sight_deadline_294=now;
    }
    if(o->medic.pending){o->medic.pending=0;++rf_scene_medic_action[6];}
    ++rf_scene_medic_flight[2];
    status=scene_medic_flight_nav_request(s,o,threat,now,&found);if(status)return status;
    return found?RF_OK:scene_medic_flight_no_route(o,now);
}
static int scene_medic_flight_damage(campaign_npc_body *o,uint32_t source,int32_t now,uint32_t *handled)
{
    scene_medic_flight_threat_view threat={0};uint32_t supported=0,index,handle;uint64_t epoch,lifetime;int status;
    *handled=0;if(scene_medic_flight_restoring||!scene_medic_flight_live(o))return RF_OK;
    if(source!=campaign_player_object.handle||campaign_player_object.view!=&campaign_player_view||
       rf_object_registry_lookup(&campaign_registry,source)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)source)!=&campaign_player_view)return RF_OK;
    *handled=1;
    if(o->ai_mode.action_280==1||o->ai_mode.action_280==9||o->ai_mode.action_280==8||o->ai_mode.action_280==13||
       o->damage.effects.health<=0||(o->object_flags&(2u|0x4000u))||(o->view.flags_810&1u))return RF_OK;
    index=(uint32_t)(o-campaign_npc_bodies);
    if(scene_medic_flight_overlap(o)||o->movement_slot>=16||
       (campaign_modes[o->movement_slot].index!=1&&campaign_modes[o->movement_slot].index!=3)||
       !(campaign_seeds.classes[campaign_seeds.items[index].class_index].physics.flags&0x20000u)||
       (campaign_seeds.classes[campaign_seeds.items[index].class_index].physics.flags&(0x200u|0x400000u))){
        ++rf_scene_medic_flight[9];return RF_OK;
    }
    /* Capture actual threat without installing it merely to call a resolver. */
    threat.handle=source;threat.view=&campaign_player_view;
    memcpy(threat.position,rf_scene_actor_pose.public_position,12);
    status=scene_medic_player_eye(rf_scene_profile_stage[0],threat.eye);if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    threat.model_radius=campaign_player_geometry.model_radius;
    status=scene_medic_flight_visibility_admit(scene_actor_collision_owner,o,&threat,&supported);if(status)return status;
    if(!supported){++rf_scene_medic_flight[9];return RF_OK;}
    handle=o->registration.handle;epoch=scene_continuous_charge_runtime.epoch;lifetime=scene_medic_flight_lifetime;
    status=rf_scene_npc_reset_ai_animation(handle,1);if(status)return status;
    /* Existing actor Message owns1400. Audio may callback; revalidate before
     * acquiring any flight/RNG owner, never retaining a released target. */
    for(uint32_t i=0;i<RF_AUDIO_ORDINARY_SLOTS;i++){
        scene_message_voice *v=campaign_message_voices+i;
        if(v->route==SCENE_MESSAGE_ACTOR&&v->key==(uint32_t)campaign_seeds.records.items[index].record.uid&&
           campaign_message_active(v))campaign_message_release(v);
        if(epoch!=scene_continuous_charge_runtime.epoch||lifetime!=scene_medic_flight_lifetime||!scene_medic_flight_live(o)||o->registration.handle!=handle)return RF_OK;
    }
    rf_scene_medic_flight[10]=source;
    for(uint32_t i=0;i<2;i++){
        float seconds=(float)(60.0+(double)scene_medic_flight_random()*(60.0/32768.0));
        status=scene_medic_flight_request(scene_actor_collision_owner,o,&threat,now,seconds);
        if(status)return status;
    }
    return RF_OK;
}
static int scene_medic_flight_index(const campaign_npc_body *o,uint32_t *index)
{
    if(!o||!index)return RF_RANGE;
    for(uint32_t i=0;i<campaign_npc_body_count;i++){
        const campaign_npc_body *live=campaign_npc_bodies+i;
        if(o==live||(o->persistence_slot==live->persistence_slot&&
           o->registration.handle==live->registration.handle)){
            *index=i;return scene_medic_flight_binding(i);
        }
    }
    return RF_NOT_FOUND;
}
static int scene_medic_flight_restore_admit(const campaign_npc_body *o,int32_t action,
    const scene_medic_flight_snapshot *p,uint32_t player)
{
    const scene_medic_flight_state *f;uint32_t index;int status;
    if(!o||!p)return RF_RANGE;f=&p->state;
    if(!f->qualified){scene_medic_flight_snapshot zero={0};return memcmp(&zero,p,sizeof(zero))?RF_FORMAT:RF_OK;}
    status=scene_medic_flight_index(o,&index);if(status)return status;
    if(o->medic.default_action!=2||o->ai_mode.state_2b4!=0||f->qualified!=1||f->entered>1||f->target_handle||f->target_kind>SCENE_FLIGHT_TARGET_UNRESOLVED||
       f->visible_published_29c>1||f->visible_eye_29d>1||
       (f->target_kind==SCENE_FLIGHT_TARGET_PLAYER&&(!player||player==UINT32_MAX))||
       (action==7&&!f->entered)||
       (f->entered&&action!=1&&action!=2&&action!=7&&action!=14))return RF_FORMAT;
    {const int32_t deadlines[5]={f->deadline_250,f->sight_deadline_294,f->no_route_deadline_4d4,
        f->reset_deadline_1370,f->stall_deadline_4d0};
     for(uint32_t i=0;i<5;i++)if(deadlines[i]<-1||deadlines[i]>RF_TIMER_PERIOD)return RF_FORMAT;}
    if(f->entered&&scene_medic_flight_overlap(o))return RF_NOT_FOUND;
    if(!f->entered&&(f->target_kind||f->nav.count||f->nav.start_valid||f->nav.eye_valid||
       f->nav.pair[0].kind||f->nav.pair[1].kind||f->visible_published_29c||f->visible_eye_29d||
       f->reset_deadline_1370!=-1))return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(p->command_714[i])||fabsf(p->command_714[i])>1.f||
       (!f->entered&&p->command_714[i]!=0)||
       !isfinite(p->angular_150[i])||!isfinite(p->torque_174[i])||
       (!f->entered&&(p->angular_150[i]!=0||p->torque_174[i]!=0)))return RF_FORMAT;
    return scene_medic_flight_nav_admit(o,&f->nav,&p->start);
}
static int scene_medic_flight_capture(const campaign_npc_body *o,int32_t now,scene_medic_flight_snapshot *out)
{
    scene_medic_flight_snapshot p={0};int status;
    if(!o||!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!o->flight.qualified){*out=p;return RF_OK;}
    p.state=o->flight;p.state.target_handle=0;
    if(p.state.target_kind==SCENE_FLIGHT_TARGET_PLAYER&&
       (o->flight.target_handle!=campaign_player_object.handle||
        campaign_player_object.view!=&campaign_player_view||
        rf_object_registry_lookup(&campaign_registry,o->flight.target_handle)!=&campaign_player_object||
        rf_entity_lookup(&campaign_entities,(int32_t)o->flight.target_handle)!=&campaign_player_view))
        p.state.target_kind=SCENE_FLIGHT_TARGET_UNRESOLVED;
    status=scene_medic_flight_nav_capture(o,&p.state.nav,&p.start);if(status)return status;
    memcpy(p.command_714,o->command_714,12);
    if(p.state.entered){
        memcpy(p.angular_150,o->body.state.vector_c8,12);
        memcpy(p.torque_174,o->body.state.vector_ec,12);
    }
    status=scene_medic_flight_restore_admit(o,o->ai_mode.action_280,&p,campaign_player_object.handle);
    if(status)return status;*out=p;return RF_OK;
}
static void scene_medic_flight_restore_publish(campaign_npc_body *o,const scene_medic_flight_snapshot *p,uint32_t player)
{
    ++scene_medic_flight_lifetime;
    o->flight=p->state;
    if(o->flight.target_kind==SCENE_FLIGHT_TARGET_PLAYER)o->flight.target_handle=player;
    if(o->flight.qualified)scene_medic_flight_nav_publish(o,&p->state.nav,&p->start);
    memcpy(o->command_714,p->command_714,12);
    if(p->state.entered){
        memcpy(o->body.state.vector_c8,p->angular_150,12);
        memcpy(o->body.state.vector_ec,p->torque_174,12);
    }
    scene_medic_flight_player_motion(NULL);
}
static void scene_medic_flight_legacy_restore(campaign_npc_body *o,int32_t now)
{
    if(o->flight.qualified)scene_medic_flight_construct(o,now,o->flight.nav.bias_158);
}
static void scene_medic_flight_rng_capture(scene_medic_flight_rng *out)
{out->present=scene_medic_flight_rng_seen;out->value=out->present?combat_pain_random.value:0;}
static void scene_medic_flight_rng_publish(const scene_medic_flight_rng *p)
{
    scene_medic_flight_rng_seen=p->present;
    if(p->present)combat_pain_random.value=p->value;
}
static void scene_medic_flight_restore_scope(uint32_t active)
{scene_medic_flight_restoring=!!active;}
static int scene_medic_flight_history_admit(const campaign_npc_body *o)
{
    scene_medic_flight_snapshot p;int status;
    if(!o||!o->flight.qualified||!o->flight.entered||o->ai_mode.action_280!=7)return RF_NOT_FOUND;
    if(o->view.action_520!=7)return RF_FORMAT;
    status=scene_medic_flight_capture(o,scene_medic_now(rf_scene_profile_stage[0]),&p);
    return status;
}
static void scene_medic_flight_retire(campaign_npc_body *o)
{
    if(scene_medic_flight_restoring)return;
    memset(&o->flight,0,sizeof(o->flight));memset(o->command_714,0,12);
}
static void scene_medic_flight_world_construct(void)
{
    scene_medic_flight_rng_seen=0;scene_medic_flight_restoring=0;++scene_medic_flight_lifetime;
    memset(rf_scene_medic_flight,0,sizeof(rf_scene_medic_flight));
    scene_medic_flight_player_motion(NULL);
}
static uint32_t scene_medic_flight_command_overlap(const campaign_npc_body *o)
{return !scene_medic_flight_restoring&&o->flight.qualified&&o->flight.entered;}
#endif
