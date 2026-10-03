#ifndef RF_SCENE_APC_EXIT_TRANSITION_INCLUDED
#define RF_SCENE_APC_EXIT_TRANSITION_INCLUDED
/* Include after damage runtime, before the main vehicle tick.
 * An APC occupied at tick start keeps ownership during its final rigid step.
 * Resolve Use/death/wreck release against the accepted END pose, immediately
 * before ordinary player publication. Unoccupied boarding stays before physics.
 * This changes scheduling only; all existing range/lock/world/floor/hull
 * clearance and exit-look rules remain in the original entry adapter.
 */
typedef struct scene_apc_exit_sample {
    uint32_t deferred,frame;
    float position[3],basis[9];
} scene_apc_exit_sample;
/* deferred ticks,attempts,exits,blocked,last exit frame,reason,host,player,
 * driver,occupant,parent,session,final hull clear,probe status,
 * pre xyz3,post xyz3,post velocity3,exit player xyz3,
 * pre basis9,post basis9,step distance bits,host alive,player alive,reserved */
uint32_t rf_scene_apc_exit_transition[48];
/* frame,session,driver,parent,current player xyz3,health bits,velocity xyz3 */
uint32_t rf_scene_apc_exit_continuation[11];
static void scene_apc_exit_begin(scene_apc_exit_sample *sample,
    const scene_driller_entry_exit *entry,uint32_t frame)
{
    memset(sample,0,sizeof(*sample));sample->frame=frame;
    if(!entry||!entry->resource||strcmp(entry->resource->model,"APC.v3m")||!entry->session.active)return;
    sample->deferred=1;memcpy(sample->position,entry->physics->state.position,12);
    memcpy(sample->basis,entry->physics->state.orientation,36);
}
static int scene_apc_exit_finish(const scene_apc_exit_sample *sample,
    scene_driller_damage_runtime *owner,uint32_t held,uint32_t alive,uint32_t *changed)
{
    scene_driller_entry_exit *entry;uint32_t attempt,reason;int status;
    if(!sample||!sample->deferred||!owner||!changed||held>1||alive>1)return RF_RANGE;
    if(!scene_driller_damage_runtime_valid(owner))return RF_NOT_FOUND;
    entry=owner->entry;
    if(!entry->session.active||strcmp(entry->resource->model,"APC.v3m"))return RF_NOT_FOUND;
    attempt=(held&&!entry->held)||!alive||owner->damage.destroyed||!entry->host.alive;
    reason=!alive?2u:(owner->damage.destroyed||!entry->host.alive)?1u:0u;
    ++rf_scene_apc_exit_transition[0];if(attempt)++rf_scene_apc_exit_transition[1];
    status=scene_driller_damage_runtime_tick(owner,changed);if(status)return status;
    if(entry->host.alive){status=scene_driller_entry_exit_use(entry,held,alive,changed);if(status)return status;}
    if(attempt&&!*changed)++rf_scene_apc_exit_transition[3];
    if(*changed&&!entry->session.active){
        scene_vehicle_collision body;rf_collision_body_sphere spheres[64];
        uint32_t *p=rf_scene_apc_exit_transition;float distance=0;
        ++p[2];p[4]=sample->frame;p[5]=reason;p[6]=entry->host.handle;p[7]=entry->player.handle;
        p[8]=entry->host.driver;p[9]=(uint32_t)entry->occupant;p[10]=(uint32_t)entry->player_view->linked_handle;
        p[11]=entry->session.active;p[12]=0;p[13]=0;
        /* Read-only evidence. A living exit has already passed these complete
         * body/hull checks in exit_pose; no new failure follows ownership commit. */
        if(alive){int probe=scene_driller_player_shape(entry,&body,spheres);
            p[13]=(uint32_t)probe;if(!probe)p[12]=(uint32_t)scene_driller_excludes_host(entry,&body,&entry->player.pose);}
        memcpy(p+14,sample->position,12);memcpy(p+17,entry->physics->state.position,12);
        memcpy(p+20,entry->physics->state.velocity,12);memcpy(p+23,entry->player.pose.position,12);
        memcpy(p+26,sample->basis,36);memcpy(p+35,entry->physics->state.orientation,36);
        for(uint32_t i=0;i<3;i++){float d=sample->position[i]-entry->physics->state.position[i];distance+=d*d;}
        distance=sqrtf(distance);memcpy(p+44,&distance,4);p[45]=entry->host.alive;p[46]=alive;
    }
    return RF_OK;
}
static void scene_apc_exit_observe(const scene_driller_entry_exit *entry,uint32_t frame)
{
    uint32_t *p=rf_scene_apc_exit_continuation;
    if(!entry||!entry->resource||strcmp(entry->resource->model,"APC.v3m"))return;
    p[0]=frame;p[1]=entry->session.active;p[2]=entry->host.driver;p[3]=(uint32_t)entry->player_view->linked_handle;
    memcpy(p+4,entry->player_body->state.position,12);memcpy(p+7,&campaign_player_damage.state.effects.health,4);
    memcpy(p+8,entry->player_body->state.velocity,12);
}
#endif
