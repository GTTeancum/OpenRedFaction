#ifndef RF_SCENE_DRILLER_DAMAGE_RUNTIME_INC
#define RF_SCENE_DRILLER_DAMAGE_RUNTIME_INC
/* Include after entry/exit, player publication and combat effect helpers.
 * Embed one owner in the existing DEV runtime; keep the entry registration
 * alive for the retained wreck. No second registry identity is allocated.
 * A destroyed occupied host retries the existing validated exit. A blocked
 * living driver stays seated, but MUST have propulsion/weapon input disabled
 * by the parent via entry.host.alive. No guessed ejection or fabricated kill.
 * Wreck visuals, persistent host burns and destruction explosions are deferred.
 */
#include "scene_driller_damage.inc"
typedef struct scene_driller_damage_runtime {
    scene_driller_damage damage;
    scene_driller_entry_exit *entry;
    scene_driller_player_adapter *player;
    uint32_t active,eject_pending,last_destroyer,damage_calls,exit_count;
    float last_amount;
} scene_driller_damage_runtime;
/* One DEV host. Clear before entry unregister/free; full generation validation
 * is performed on each source/receive operation, never trust just slot bits. */
static scene_driller_damage_runtime *scene_driller_damage_owner;
static uint32_t scene_driller_damage_runtime_valid(const scene_driller_damage_runtime *owner)
{
    return owner&&owner->active&&owner->entry&&owner->entry->registration.view==&owner->entry->view&&
        owner->entry->registration.handle==owner->damage.state.effects.handle&&
        rf_entity_lookup(owner->entry->entities,(int32_t)owner->damage.state.effects.handle)==&owner->entry->view;
}
/* Parent combat_source calls this first (forward declaration needs no types).
 * Host credit remains host-owned; affiliation follows its present driver. */
static int scene_driller_damage_source(uint32_t handle,uint32_t *affiliation)
{
    scene_driller_damage_runtime *owner=scene_driller_damage_owner;
    if(!affiliation||!scene_driller_damage_runtime_valid(owner)||handle!=owner->damage.state.effects.handle)return 0;
    *affiliation=owner->entry->session.active?campaign_player_damage.state.effects.affiliation:
        owner->damage.state.effects.affiliation;
    return 1;
}
static int scene_driller_damage_runtime_open(scene_driller_damage_runtime *owner,
    scene_driller_entry_exit *entry,scene_driller_player_adapter *player,const scene_driller_damage *prototype)
{
    scene_driller_damage_runtime value={0};
    if(!owner||!entry||!player||!prototype||owner->active||scene_driller_damage_owner||
       entry->registration.view!=&entry->view)return RF_RANGE;
    if(rf_entity_lookup(entry->entities,(int32_t)entry->registration.handle)!=&entry->view)return RF_NOT_FOUND;
    value.damage=*prototype;
    value.damage.state.effects.handle=entry->registration.handle;
    value.damage.object_flags|=entry->view.flags_7c;
    value.entry=entry;value.player=player;value.active=1;value.last_destroyer=UINT32_MAX;
    entry->view.flags_7c=value.damage.object_flags;
    *owner=value;scene_driller_damage_owner=owner;return RF_OK;
}
static int scene_driller_damage_runtime_destroy(void *context,uint32_t host,uint32_t responsible)
{
    scene_driller_damage_runtime *owner=context;
    if(!scene_driller_damage_runtime_valid(owner)||host!=owner->entry->host.handle)return RF_NOT_FOUND;
    owner->entry->host.alive=0;owner->last_destroyer=responsible;
    owner->eject_pending=owner->entry->session.active;
    /* Damage is committed. Exit/world publication is scheduled separately;
     * an obstructed exit must not roll health back or abort the game loop. */
    return RF_OK;
}
static int scene_driller_damage_runtime_receive(scene_driller_damage_runtime *owner,uint32_t target,
    const rf_damage_request *request,uint32_t frame,float *amount)
{
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    float seconds=(float)frame/60;uint32_t clock;int status;
    if(!request||!amount)return RF_RANGE;
    if(!scene_driller_damage_runtime_valid(owner)||target!=owner->damage.state.effects.handle)return RF_NOT_FOUND;
    memcpy(&clock,&seconds,4);*amount=0;
    status=scene_driller_damage_receive(&owner->damage,request,1,clock,owner->entry->session.active,
        &effects,scene_driller_damage_runtime_destroy,owner,amount);
    /* Core damage may commit before a feedback error; publish its flags and
     * destruction gate even when caller needs to report that later error. */
    owner->entry->view.flags_7c=owner->damage.object_flags;
    if(owner->damage.destroyed){owner->entry->host.alive=0;owner->eject_pending=owner->entry->session.active;}
    if(status)return status;
    if(feedback.status)return feedback.status;
    ++owner->damage_calls;owner->last_amount=*amount;
    return RF_OK;
}
/* Call before use/drive each tick. Already publishes player release through
 * the normal adapter; changed lets parent update its exit counters. While a
 * dead host retains a blocked occupant, skip entry_exit_use (avoid two exit
 * queries) and call this again next tick. Keep normal seat publication active. */
static int scene_driller_damage_runtime_tick(scene_driller_damage_runtime *owner,uint32_t *changed)
{
    scene_vehicle_possession_ops ops;scene_driller_entry_exit *entry;int status;uint32_t alive;
    if(!changed)return RF_RANGE;
    *changed=0;
    if(!scene_driller_damage_runtime_valid(owner))return RF_NOT_FOUND;
    entry=owner->entry;
    if(!owner->damage.destroyed)return RF_OK;
    if(owner->damage.destruction_pending){
        status=scene_driller_damage_runtime_destroy(owner,owner->damage.state.effects.handle,
            owner->damage.state.responsible_handle);if(status)return status;
        owner->damage.destruction_pending=0;
    }
    entry->host.alive=0;
    if(!entry->session.active){owner->eject_pending=0;return RF_OK;}
    owner->eject_pending=1;
    ops=(scene_vehicle_possession_ops){scene_driller_entry_allowed,scene_driller_entry_seat,scene_driller_exit_pose,entry};
    alive=campaign_player_damage.state.effects.health>0;
    status=scene_vehicle_unpossess(&entry->session,&entry->player,&entry->host,alive?1:2,&ops,changed);
    if(status)return status;
    if(!*changed)return RF_OK;
    entry->player_view->linked_handle=-1;entry->occupant=-1;owner->eject_pending=0;++owner->exit_count;
    return scene_driller_player_publish(owner->player,entry,1);
}
static void scene_driller_damage_runtime_close(scene_driller_damage_runtime *owner)
{
    if(!owner)return;
    if(scene_driller_damage_owner==owner)scene_driller_damage_owner=NULL;
    memset(owner,0,sizeof(*owner));
}
#endif
