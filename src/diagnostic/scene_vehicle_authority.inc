#ifndef SCENE_VEHICLE_AUTHORITY_INCLUDED
#define SCENE_VEHICLE_AUTHORITY_INCLUDED
/* One rigid authority: parked banks may lend motion to a real independent
 * runtime, retaining only inventory/control metadata. RFSW5 marks the loan and
 * joins its physical snapshot to RFSV. No owner/handle/class substitution. */
static scene_secondary_vehicle_runtime *scene_selected_goto_navigation;
/*404dc0's nonopposed Goto_Player handback uses strict distance squared4,
 * without the hostile FOV/LOS branch. It is an order completion, not fire. */
static uint32_t scene_vehicle_pursuit_finished(uint32_t handle,const float position[3])
{
    rf_damage_effect_state *source=scene_vehicle_allegiance_owner(handle);float target[3],health;uint32_t hull;double distance=0;
    if(!source||scene_vehicle_ai_opposed(source->affiliation,campaign_player_damage.state.effects.affiliation)||
       !scene_vehicle_attack_target(campaign_player_object.handle,target,&hull))return 0;
    if(hull!=UINT32_MAX)(void)scene_campaign_vehicle_target(scene_vehicle_ai_stream,hull,target,&health);
    for(uint32_t i=0;i<3;i++){double d=(double)target[i]-position[i];distance+=d*d;}
    return distance<4.0;
}
static uint32_t scene_vehicle_switch_route_finished(const scene_campaign_vehicle_route *r)
{
    if(r->active||r->ai_action)return 0;
    if((r->pursuit==1||r->pursuit==2)&&!r->path.count)return 1;
    return !r->pursuit&&r->path.count&&r->index==r->path.count&&!r->mode&&!r->reverse;
}
static void scene_vehicle_selected_navigation_reset(void)
{free(scene_selected_goto_navigation);scene_selected_goto_navigation=NULL;}
static scene_vehicle_parked_state *scene_vehicle_switch_bank(uint32_t uid,uint32_t handle)
{
    scene_vehicle_parked_state *found=NULL;
    for(uint32_t i=0;scene_vehicle_parked&&i<scene_vehicle_parked_count;i++){
        scene_vehicle_parked_state *v=scene_vehicle_parked+i;
        if(!v->valid||v->uid!=uid)continue;
        if(found||v->handle!=handle||i>=campaign_passive_vehicle_count||
           campaign_passive_vehicles[i].uid!=uid||campaign_passive_vehicles[i].handle!=handle)return NULL;
        found=v;
    }
    return found;
}
static int scene_vehicle_switch_motion_borrow(scene_stream *s,uint32_t uid,uint32_t handle,uint32_t profile,
    rf_vehicle_rigid_state *state,uint32_t *body_flags)
{
    scene_vehicle_parked_state *bank=scene_vehicle_switch_bank(uid,handle);uint32_t quiet;int status;
    if(!state||!body_flags)return RF_RANGE;
    if(!bank)return RF_OK;
    if(scene_vehicle_switch_stream!=s||bank->profile!=profile||bank->motion_delegated||
       (profile!=2&&profile!=4&&profile!=5)||scene_secondary_vehicle_find(handle))return RF_FORMAT;
    status=scene_vehicle_switch_quiet(&bank->physics,&quiet);if(status)return status;if(!quiet)return RF_NOT_FOUND;
    /* Passive event pose remains live authority while parked. Preserve the
     * bank's mass/inertia, momentum and force state, not its old draw pose. */
    rf_vehicle_rigid_state value=bank->physics;
    memcpy(value.position,state->position,12);memcpy(value.orientation,state->orientation,36);
    *state=value;*body_flags=bank->body_flags;return RF_OK;
}
static void scene_vehicle_switch_motion_commit(uint32_t uid,uint32_t handle)
{scene_vehicle_parked_state *bank=scene_vehicle_switch_bank(uid,handle);if(bank)bank->motion_delegated=1;}
static uint32_t scene_vehicle_switch_supply_seed(uint32_t uid,uint32_t profile,int32_t *primary,int32_t *secondary,
    scene_vehicle_primary_state *fire,float *missile_cooldown)
{
    for(uint32_t i=0;scene_vehicle_parked&&i<scene_vehicle_parked_count;i++){
        const scene_vehicle_parked_state *v=scene_vehicle_parked+i;
        if(!v->valid||v->uid!=uid||v->profile!=profile)continue;
        if(primary)*primary=profile==4?0:v->primary_reserve;
        if(secondary)*secondary=profile==4?v->submarine_reserve:v->secondary_reserve;
        if(fire)*fire=v->primary;
        if(missile_cooldown)*missile_cooldown=profile==4?v->submarine_cooldown:fmaxf(0,v->secondary_cooldown);
        return 1;
    }
    return 0;
}
/* Export the existing weapon authority when a moving host is saved/promoted.
 * Trigger ownership remains RFAI/RFAK; the parked mirror stores finite supply
 * and accepted cooldowns but cannot replay an independently-held trigger. */
static void scene_vehicle_switch_supply_export(scene_vehicle_parked_state *v)
{
    scene_vehicle_ai_state *ai=scene_vehicle_ai_find(v->handle);
    scene_vehicle_attack_state *attack=scene_vehicle_attack_find(v->handle);
    if(ai){
        if(v->profile==2){v->primary_reserve=ai->primary_reserve;v->primary=ai->primary_fire;}
        else if(v->profile==4){v->submarine_reserve=ai->missile_reserve;v->submarine_cooldown=ai->missile_cooldown;}
        else if(v->profile==5){v->primary_reserve=ai->primary_reserve;v->secondary_reserve=ai->missile_reserve;
            v->primary=ai->primary_fire;v->fighter_secondary.cooldown=v->secondary_cooldown=ai->missile_cooldown;}
    }
    if(attack&&v->profile==5){v->primary_reserve=attack->reserve;if(attack->active||!ai)v->primary=attack->fire;}
    v->primary.held=0;v->primary.warmup=0;v->fighter_secondary.held=0;v->fighter_secondary.warmup=0;
}
static uint32_t scene_vehicle_switch_independent_admits(const scene_passive_vehicle *p)
{
    const scene_secondary_vehicle_runtime *r=scene_secondary_vehicle_find(p->handle);uint32_t quiet=0;
    if(!r)return 1;
    return r->uid==p->uid&&r->slot==(uint32_t)(p-campaign_passive_vehicles)&&r->profile!=6&&!r->retired&&
        !r->route.active&&scene_secondary_vehicle_empty(p)&&!scene_secondary_vehicle_has_rider(p->handle)&&
        !scene_vehicle_switch_quiet(&r->physics.state,&quiet)&&quiet;
}
/* Infallible after the registry exchange admission. The new player owner
 * receives the exact rigid state first; then this redundant runtime retires.
 * All emitted projectiles retain their separate generation-bearing source. */
static void scene_vehicle_switch_independent_commit(scene_secondary_vehicle_runtime *r)
{
    if(!r)return;
    uint32_t n=campaign_navigation_workspace.count;
    if(campaign_navigation_workspace.references){
        if(campaign_navigation_workspace.references[n].candidate==&r->navigation.goal)campaign_navigation_workspace.references[n].candidate=NULL;
        if(campaign_navigation_workspace.references[n+1].candidate==&r->navigation.start)campaign_navigation_workspace.references[n+1].candidate=NULL;
    }
    campaign_secondary_vehicles[r->slot]=NULL;if(campaign_secondary_vehicle_count)--campaign_secondary_vehicle_count;
    scene_vehicle_ai_cancel(r->handle);scene_vehicle_attack_cancel(r->handle);free(r);
}
/* Selected APC Goto borrows the recovered independent graph command service.
 * This object is navigation scratch only: it is never registered or stepped
 * as a body. Its physics snapshot is overwritten from the sole live chassis.
 * Saves retain RFVR's event/goal and replan the transient four-node prefix. */
static int scene_vehicle_selected_goto_command(const scene_driller_runtime *owner,rf_vehicle_rigid_command *command)
{
    scene_stream *s=owner->contact.stream;scene_secondary_vehicle_runtime *r;int status;
    if(!s||rf_scene_vehicle_enabled!=2||campaign_vehicle_route.pursuit!=1)return RF_NOT_FOUND;
    if(!scene_selected_goto_navigation){scene_selected_goto_navigation=calloc(1,sizeof(*r));if(!scene_selected_goto_navigation)return RF_IO;}
    r=scene_selected_goto_navigation;
    if(r->handle!=owner->entry.host.handle||r->route.event!=campaign_vehicle_route.event||memcmp(r->route.target,campaign_vehicle_route.target,12)){
        memset(r,0,sizeof(*r));r->uid=(uint32_t)campaign_authored_vehicle_uid;r->handle=owner->entry.host.handle;r->profile=2;
        r->route.event=campaign_vehicle_route.event;r->route.index=1;r->route.kind=SCENE_SECONDARY_ORDER_POINT;
        memcpy(r->route.target,campaign_vehicle_route.target,12);
    }
    r->physics=owner->physics;r->route.active=campaign_vehicle_route.active;
    status=scene_secondary_vehicle_command(s,r,command);if(status)return status;
    campaign_vehicle_route.active=r->route.active;rf_scene_vehicle_route_state[0]=r->route.active;
    return RF_OK;
}
static int scene_vehicle_selected_flight_command(const scene_driller_runtime *r,rf_vehicle_submersible_command *command)
{
    const float *player=NULL;uint32_t complete=0;int status;
    if(campaign_vehicle_route.pursuit==2&&scene_vehicle_pursuit_finished(r->entry.host.handle,r->physics.state.position)){
        campaign_vehicle_route.active=0;rf_scene_vehicle_route_state[0]=0;return RF_OK;
    }
    if(campaign_vehicle_route.pursuit==2&&campaign_player_object.view==&campaign_player_view&&
       scene_actor_body.allocated_bytes&&campaign_player_damage.state.effects.health>0&&!(campaign_player_view.flags_7c&2u))player=scene_actor_body.state.position;
    status=scene_secondary_vehicle_point_steer(rf_scene_vehicle_enabled,&r->physics.state,
        campaign_vehicle_route.pursuit==2?SCENE_SECONDARY_ORDER_PLAYER:SCENE_SECONDARY_ORDER_POINT,
        campaign_vehicle_route.target,player,command,&complete);if(status)return status;
    if(complete){campaign_vehicle_route.active=0;rf_scene_vehicle_route_state[0]=0;++rf_scene_vehicle_route_state[4];}
    return RF_OK;
}
#endif
