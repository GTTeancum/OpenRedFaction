#ifndef SCENE_TURRET_OPERATOR_INC
#define SCENE_TURRET_OPERATOR_INC
/* NPC stationary-turret control only. scene_npc_seat_bind owns the real seat,
 * links, actor pose and detach transaction; these rows only associate control.
 * Include after scene_turret_combat.inc and before scene_npc_seat_bind.inc. */
static int scene_npc_seat_pair(uint32_t,uint32_t,int32_t,uint32_t *);
typedef struct scene_turret_operator_binding {
    uint32_t active,host,actor,host_uid,actor_uid,owns_host_flag;int32_t tag;
} scene_turret_operator_binding;
static scene_turret_operator_binding scene_turret_operator_rows[SCENE_TURRET_OWNER_LIMIT];
uint32_t rf_scene_turret_operator[8]; /* bound,unbound,eligible ticks,held ticks,handheld skips,exclusions,last actor UID,errors */
static campaign_npc_body *scene_turret_operator_npc(uint32_t handle)
{
    uint32_t i;
    if(handle==campaign_player_object.handle||handle==UINT32_MAX)return NULL;
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *o=campaign_npc_bodies+i;
        if(o->registration.view==&o->view&&o->registration.handle==handle&&
           rf_object_registry_lookup(&campaign_registry,handle)==&o->registration&&
           rf_entity_lookup(&campaign_entities,(int32_t)handle)==&o->view)return o;
    }
    return NULL;
}
static scene_turret_operator_binding *scene_turret_operator_find(uint32_t host)
{
    uint32_t i;for(i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)
        if(scene_turret_operator_rows[i].active&&scene_turret_operator_rows[i].host==host)return scene_turret_operator_rows+i;
    return NULL;
}
/* Reset after physical seat teardown, or before a staged whole-world restore.
 * Never call to detach a single occupied live seat: use unbind first. */
static void scene_turret_operator_reset(void)
{
    memset(scene_turret_operator_rows,0,sizeof(scene_turret_operator_rows));
    memset(rf_scene_turret_operator,0,sizeof(rf_scene_turret_operator));
}
static int scene_turret_operator_bind(uint32_t host,uint32_t actor,int32_t tag,int32_t now,uint32_t restoring)
{
    scene_turret_owner *turret=scene_turret_lookup(host);campaign_npc_body *npc=scene_turret_operator_npc(actor);
    scene_turret_operator_binding value={0},*row;rf_entity_ai_transition_state next;
    uint32_t i,slot=UINT32_MAX,attached=0;int status;
    if(now<0||now>RF_TIMER_PERIOD||tag<0||restoring>1)return RF_RANGE;
    if(!turret||!npc||host==actor)return RF_NOT_FOUND;
    /* Generated Auto Turret heads are a separate typed child owner, not NPC
     * operators. All three installed NPC-operated hosts use kind4. */
    if(turret->view.class_type!=4||turret->dead||turret->damage.effects.health<=0||
       (turret->view.flags_7c&(2u|0x4000u))||!npc->body.allocated_bytes||
       npc->damage.effects.health<=0||(npc->view.flags_810&1u)||
       (npc->object_flags&(2u|0x4000u))||npc->view.linked_handle!=(int32_t)host)return RF_NOT_FOUND;
    status=scene_npc_seat_pair(host,actor,tag,&attached);if(status)return status;if(!attached)return RF_NOT_FOUND;
    row=scene_turret_operator_find(host);
    if(row)return row->actor==actor&&row->tag==tag?RF_OK:RF_RANGE;
    for(i=0;i<SCENE_TURRET_OWNER_LIMIT;i++){
        if(scene_turret_operator_rows[i].active&&scene_turret_operator_rows[i].actor==actor)return RF_RANGE;
        if(!scene_turret_operator_rows[i].active&&slot==UINT32_MAX)slot=i;
    }
    if(slot==UINT32_MAX)return RF_RANGE;
    /* No installed Attack/Shoot_At/Once order addresses these operators.
     * Reject an unsupported handoff rather than dropping pending shots. */
    if(npc->combat_scripted||npc->combat_reload_due||npc->script_shoot.active||npc->script_once.count||
       npc->script_animation.active||npc->script_move.active)return RF_NOT_FOUND;
    next=npc->ai_mode;
    if(!restoring){status=rf_entity_ai_set_action(&next,13,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;}
    value.active=1;value.host=host;value.actor=actor;value.tag=tag;value.host_uid=turret->uid;
    value.actor_uid=(uint32_t)campaign_seeds.records.items[npc-campaign_npc_bodies].record.uid;
    value.owns_host_flag=!(turret->view.flags_810&0x10000u);
    if(!restoring){
        scene_turret_combat_state *combat=scene_turret_combat_states+(turret-scene_turrets);
        if(!scene_turret_combat_ready||combat->handle!=host)return RF_NOT_FOUND;
        campaign_pursuit_stop(npc);npc->combat_alert=npc->combat_due=npc->combat_burst_remaining=npc->combat_navigation_due=0;
        npc->ai_mode=next;npc->view.action_520=next.action_280;
        /* Original4096b9..4096dc; do not replace a protected host AI action. */
        if(!(turret->view.flags_814&0x4000u))turret->view.action_520=2;
        turret->view.flags_810|=0x10000u;turret->damage.effects.flags_810=turret->view.flags_810;
        scene_turret_combat_release(turret,combat);combat->action=turret->view.action_520;
    }
    scene_turret_operator_rows[slot]=value;
    ++rf_scene_turret_operator[0];rf_scene_turret_operator[6]=value.actor_uid;return RF_OK;
}
/* Called before the physical seat bank removes its pair. Death/removal may
 * already make one endpoint absent: never dereference a stale actor handle. */
static int scene_turret_operator_unbind(uint32_t host,uint32_t actor,int32_t now)
{
    scene_turret_operator_binding *row=scene_turret_operator_find(host);scene_turret_owner *turret;
    campaign_npc_body *npc;rf_entity_ai_transition_state next;int status;
    if(now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(!row)return RF_OK;if(row->actor!=actor)return RF_NOT_FOUND;
    npc=scene_turret_operator_npc(actor);turret=scene_turret_lookup(host);
    if(npc&&npc->damage.effects.health>0&&!(npc->view.flags_810&1u)&&npc->ai_mode.action_280==13){
        next=npc->ai_mode;
        /* Practical release policy: ordinary waiting instead of a detached
         * actor stuck in13; exact original4096f0 exit behavior stays separate. */
        status=rf_entity_ai_set_action(&next,2,UINT32_MAX,UINT32_MAX,(float)now,0,0);if(status)return status;
        npc->ai_mode=next;npc->view.action_520=next.action_280;
    }
    if(turret){
        scene_turret_combat_state *combat=scene_turret_combat_states+(turret-scene_turrets);
        if(scene_turret_combat_ready&&combat->handle==host)scene_turret_combat_release(turret,combat);
        if(row->owns_host_flag)turret->view.flags_810&=~0x10000u;
        turret->damage.effects.flags_810=turret->view.flags_810;
    }
    memset(row,0,sizeof(*row));++rf_scene_turret_operator[1];return RF_OK;
}
/* Bound control remains inhibited when an endpoint dies or becomes stale;
 * the seat owner must detach it before ordinary unoccupied policy resumes.
 * Host affiliation is authoritative: L19S1 changes host9877 to hostile while
 * its seated miner9882 remains neutral. Never overwrite that script decision. */
static int scene_turret_operator_controls(uint32_t host,uint32_t *bound,uint32_t *eligible,
    uint32_t *affiliation,uint32_t *actor)
{
    scene_turret_operator_binding *row;scene_turret_owner *turret=scene_turret_lookup(host);
    campaign_npc_body *npc;uint32_t attached=0;int32_t action;int status;
    if(!bound||!eligible||!affiliation||!actor)return RF_RANGE;if(!turret)return RF_NOT_FOUND;
    *bound=0;*eligible=1;*actor=UINT32_MAX;*affiliation=turret->damage.effects.affiliation;
    row=scene_turret_operator_find(host);if(!row){
        if(scene_npc_seat_requires_operator(host)){*bound=1;*eligible=0;++rf_scene_turret_operator[3];}
        return RF_OK;
    }
    *bound=1;*eligible=0;*actor=row->actor;npc=scene_turret_operator_npc(row->actor);
    if(!npc||!npc->body.allocated_bytes||npc->view.linked_handle!=(int32_t)host||
       npc->damage.effects.health<=0||(npc->view.flags_810&1u)||(npc->object_flags&(2u|0x4000u))||
       turret->dead||turret->damage.effects.health<=0)goto held;
    status=scene_npc_seat_pair(host,row->actor,row->tag,&attached);
    if(status){++rf_scene_turret_operator[7];goto held;}if(!attached)goto held;
    action=npc->ai_mode.action_280;
    if((action!=13&&action!=-1&&action!=0&&action!=2&&action!=3&&action!=11)||
       npc->combat_scripted||npc->script_shoot.active||npc->script_once.count||npc->script_animation.active)goto held;
    *eligible=1;++rf_scene_turret_operator[2];return RF_OK;
held:
    ++rf_scene_turret_operator[3];return RF_OK;
}
static uint32_t scene_turret_operator_motion_only(uint32_t host)
{
    scene_turret_operator_binding *row=scene_turret_operator_find(host);
    campaign_npc_body *npc=row?scene_turret_operator_npc(row->actor):NULL;
    return npc&&npc->ai_mode.action_280==11;
}
/* Put before ALL pending single-shot/rocket/handheld processing. A seated NPC
 * retains its inventory but only the turret owner consumes targeting/cadence. */
static uint32_t scene_turret_operator_suppresses(uint32_t actor)
{
    uint32_t i;for(i=0;i<SCENE_TURRET_OWNER_LIMIT;i++)
        if(scene_turret_operator_rows[i].active&&scene_turret_operator_rows[i].actor==actor){
            ++rf_scene_turret_operator[4];return 1;}
    return 0;
}
/* Exclude only the firing turret's own operator from acquisition and both
 * NPC body/shield ray candidates; other actors may still damage that NPC. */
static uint32_t scene_turret_operator_excludes(uint32_t host,uint32_t candidate)
{
    scene_turret_operator_binding *row=scene_turret_operator_find(host);
    if(row&&row->actor==candidate){++rf_scene_turret_operator[5];return 1;}return 0;
}
#endif
