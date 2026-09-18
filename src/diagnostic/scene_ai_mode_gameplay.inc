#ifndef RF_SCENE_AI_MODE_GAMEPLAY_INCLUDED
#define RF_SCENE_AI_MODE_GAMEPLAY_INCLUDED
/* Runtime labels corroborated by reference/dashfaction/game_patch/rf/ai.h;
 * event34 translates authored0..5 to1,2,4,5,11,-1 in event.c. Port scheduler
 * policy below implements catatonic, waiting/default and motion awareness.
 * Waypoints4/collecting5 need route/item services: return NOT_FOUND rather
 * than falsely acknowledge them as normal combat. Turret13 cleanup and
 * linked vehicle occupants remain deferred. Requires owner.ai_mode field. */
static uint32_t campaign_enemy_mode_admits(const campaign_npc_body *owner,
    uint32_t target_moving,uint32_t gunshot)
{
    int32_t action=owner->ai_mode.action_280;
    if(action==1)return 0; /* Catatonic remains inert even when shot/noise alerts it. */
    if(action==11 && !owner->combat_alert && !owner->combat_scripted)
        return !gunshot && target_moving; /* First pass motion-only acquisition. */
    return 1;
}
static int campaign_ai_mode_apply(campaign_npc_body *owner,int32_t action,int32_t now)
{
    rf_entity_ai_transition_state next;int status;
    if(!owner || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    if(action!=1 && action!=2 && action!=11 && action!=-1)return RF_NOT_FOUND;
    if(owner->ai_mode.action_280==13 || owner->view.linked_handle!=-1)return RF_NOT_FOUND;
    next=owner->ai_mode;
    status=rf_entity_ai_set_action(&next,action,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
    campaign_pursuit_stop(owner);
    owner->combat_scripted=owner->combat_target=owner->combat_alert=0;
    owner->combat_burst_remaining=owner->combat_due=owner->combat_navigation_due=0;
    if(action==1){owner->script_move.active=0;owner->script_move.stop=1;owner->navigation.retained.count=0;}
    owner->ai_mode=next;return RF_OK;
}
static int campaign_set_ai_mode_live(void *context,uint32_t handle,int32_t action,int32_t now)
{
    uint32_t i;(void)context;
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if(owner->registration.handle!=handle || !owner->registration.view ||
           rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration)continue;
        if(owner->damage.effects.health<=0 || (owner->view.flags_810&1))return RF_NOT_FOUND;
        return campaign_ai_mode_apply(owner,action,now);
    }
    return RF_NOT_FOUND;
}
#endif
