#ifndef RF_SCENE_PURSUIT_RETURN_INPUT_INCLUDED
#define RF_SCENE_PURSUIT_RETURN_INPUT_INCLUDED
/* Uncalled read-only phase1/2 input capture, not a complete execution seal.
 * Caller authenticates lifetime of every registry/actor/player/record backing
 * before capture, and holds those lifetimes through use and recheck. Equality
 * does not establish command-history/ABA identity, sphere/graph/collision
 * contents, or resource ownership. The enclosing executor supplies those.
 * No Blind age/sync, timer, RNG, query, cancellation or gameplay writes. */
typedef struct scene_pursuit_return_input {
    uint64_t epoch;
    campaign_npc_body *owners;
    uint32_t count,frame,slot,handle,target_slot,target_player;
    int32_t primary,secondary;
    scene_pursuit_return_route_frame query;
    scene_tactical_receipt tactical;
    rf_entity_ai_transition_state ai;
    float published[3],navigation_height,target_forward[3],target_cosine;
} scene_pursuit_return_input;
static int scene_pursuit_return_input_capture(uint32_t slot,uint32_t frame,
    scene_pursuit_return_input *result)
{
    scene_pursuit_return_input out;const campaign_npc_body *o,*target_owner;
    uint32_t target,melee;int status;
    if(!result)return RF_RANGE;
    memset(&out,0,sizeof(out));out.target_slot=UINT32_MAX;
    status=scene_pursuit_return_live_admit(slot,&out.query.primary_limit);if(status)return status;
    status=scene_pursuit_return_owner(slot,&target,&melee);if(status)return status;
    o=campaign_npc_bodies+slot;
    status=scene_pursuit_return_tactical_admit(&o->pursuit_return,&o->tactical);if(status)return status;
    if((o->pursuit_return.phase!=SCENE_PURSUIT_RETURN_PENDING_ROUTE&&
        o->pursuit_return.phase!=SCENE_PURSUIT_RETURN_PENDING_FALLBACK)||
       o->pursuit_return.target!=target||o->pursuit_return.combat_mode!=o->combat_scripted||
       melee||o->ai_mode.action_280!=3||o->view.action_520!=3)return RF_FORMAT;
    out.epoch=scene_robot_death_epoch;out.owners=campaign_npc_bodies;
    out.count=campaign_npc_body_count;out.frame=frame;out.slot=slot;out.handle=o->registration.handle;
    out.primary=o->view.weapons[0];out.secondary=o->view.weapons[1];
    out.query.before=o->pursuit_return;out.query.melee_only=melee;
    out.query.visibility_override=!!(o->ai_mode.flags_530&0x1000u);
    out.query.authored_radius=o->navigation_radius;out.navigation_height=o->death_item_extent_7c4;
    out.tactical=o->tactical;out.ai=o->ai_mode;
    memcpy(out.query.source_body,o->body.state.position,12);
    memcpy(out.query.source_eye,o->eye_position,12);memcpy(out.published,o->published,12);
    if(target==campaign_player_object.handle){
        out.target_player=1;
        status=scene_medic_player_eye(frame,out.query.target_eye);if(status)return status;
        memcpy(out.query.target_body,scene_actor_body.state.position,12);
        memcpy(out.target_forward,actor_look.eye_orientation+6,12);
        out.target_cosine=campaign_player_geometry.sight_cosine;
        /* Preserve existing Blind/tactical player policy: hidden flags here
         * are NPC object flags, not an invented player visibility predicate. */
    }else {
        if(!scene_blind_target_slot(target,&out.target_slot))return RF_NOT_FOUND;
        target_owner=campaign_npc_bodies+out.target_slot;
        memcpy(out.query.target_body,target_owner->body.state.position,12);
        memcpy(out.query.target_eye,target_owner->eye_position,12);
        out.query.hidden=!!(target_owner->object_flags&(2u|0x4000u));
    }
    if(!isfinite(out.query.authored_radius)||out.query.authored_radius<=0||
       !isfinite(out.navigation_height)||out.navigation_height<0||
       !isfinite(out.target_cosine)||out.target_cosine < -1.0f||out.target_cosine>1.0f)return RF_FORMAT;
    for(uint32_t i=0;i<3;i++)if(!isfinite(out.query.source_body[i])||
        !isfinite(out.query.source_eye[i])||!isfinite(out.published[i])||
        !isfinite(out.query.target_body[i])||!isfinite(out.query.target_eye[i])||
        !isfinite(out.target_forward[i]))return RF_FORMAT;
    *result=out;return RF_OK;
}
static int scene_pursuit_return_input_matches(const scene_pursuit_return_input *expected)
{
    scene_pursuit_return_input current;int status;
    if(!expected)return RF_RANGE;
    /* Check owning containers before any actor dereference. Their equality
     * still relies on the enclosing lifetime/command-generation contract. */
    if(expected->epoch!=scene_robot_death_epoch||expected->owners!=campaign_npc_bodies||
       expected->count!=campaign_npc_body_count)return RF_NOT_FOUND;
    status=scene_pursuit_return_input_capture(expected->slot,expected->frame,&current);
    if(status)return status;
    /* Compare named members only: structure assignment need not preserve
     * padding. Float representations remain exact, including signed zero. */
    if(expected->epoch!=current.epoch||
       expected->owners!=current.owners||
       expected->count!=current.count||
       expected->frame!=current.frame||
       expected->slot!=current.slot||
       expected->handle!=current.handle||
       expected->target_slot!=current.target_slot||
       expected->target_player!=current.target_player||
       expected->primary!=current.primary||
       expected->secondary!=current.secondary||
       expected->query.before.phase!=current.query.before.phase||
       expected->query.before.target!=current.query.before.target||
       expected->query.before.combat_mode!=current.query.before.combat_mode||
       expected->query.melee_only!=current.query.melee_only||
       expected->query.hidden!=current.query.hidden||
       expected->query.visibility_override!=current.query.visibility_override||
       expected->tactical.flags!=current.tactical.flags||
       expected->tactical.phase!=current.tactical.phase||
       expected->tactical.target!=current.tactical.target||
       expected->tactical.phase_due!=current.tactical.phase_due||
       expected->tactical.recovery_due!=current.tactical.recovery_due||
       expected->tactical.random.value!=current.tactical.random.value||
       expected->ai.action_280!=current.ai.action_280||
       expected->ai.clock_288!=current.ai.clock_288||
       expected->ai.argument_28c!=current.ai.argument_28c||
       expected->ai.argument_290!=current.ai.argument_290||
       expected->ai.state_2b4!=current.ai.state_2b4||
       expected->ai.clock_2bc!=current.ai.clock_2bc||
       expected->ai.flags_530!=current.ai.flags_530||
       memcmp(&expected->query.authored_radius,&current.query.authored_radius,sizeof(current.query.authored_radius))||
       memcmp(&expected->query.primary_limit,&current.query.primary_limit,sizeof(current.query.primary_limit))||
       memcmp(&expected->navigation_height,&current.navigation_height,sizeof(current.navigation_height))||
       memcmp(&expected->target_cosine,&current.target_cosine,sizeof(current.target_cosine))||
       memcmp(expected->query.source_body,current.query.source_body,sizeof(current.query.source_body))||
       memcmp(expected->query.source_eye,current.query.source_eye,sizeof(current.query.source_eye))||
       memcmp(expected->query.target_body,current.query.target_body,sizeof(current.query.target_body))||
       memcmp(expected->query.target_eye,current.query.target_eye,sizeof(current.query.target_eye))||
       memcmp(expected->published,current.published,sizeof(current.published))||
       memcmp(expected->target_forward,current.target_forward,sizeof(current.target_forward)))return RF_NOT_FOUND;
    return RF_OK;
}
#endif
