#ifndef RF_SCENE_PURSUIT_TOKEN_OWNER_INCLUDED
#define RF_SCENE_PURSUIT_TOKEN_OWNER_INCLUDED
/* Current-owner bridge, after bodies/count/epoch definitions. Only genuine
 * accepted command or destructive retirement boundaries call this. Routine
 * pain, deadline maintenance and in-range route stopping must not call it.
 * Registered result and current array are live/disjoint under the tick scope.
 */
static uint32_t scene_pursuit_token_current_owner(const scene_pursuit_ordinary_token *t,
    const campaign_npc_body *owner)
{
    const campaign_npc_body *current;
    if(!owner||!campaign_npc_bodies||t->epoch!=scene_robot_death_epoch||
       t->owners!=campaign_npc_bodies||t->count!=campaign_npc_body_count||
       t->slot>=campaign_npc_body_count)return 0;
    current=campaign_npc_bodies+t->slot;
    if(owner!=current)return 0;
    return current->registration.handle==t->handle;
}
static void scene_pursuit_token_invalidate_owner(const campaign_npc_body *owner)
{
    scene_pursuit_work_dispatch_result *r=scene_pursuit_token_scope;
    if(!r)return;
    if(r->ordinary_valid&&scene_pursuit_token_current_owner(&r->ordinary,owner))
        scene_pursuit_token_invalidate(scene_robot_death_epoch,campaign_npc_bodies,
            r->ordinary.slot,owner->registration.handle);
    if(r->cancelled_valid&&scene_pursuit_token_current_owner(&r->cancelled_owner,owner))
        scene_pursuit_token_invalidate(scene_robot_death_epoch,campaign_npc_bodies,
            r->cancelled_owner.slot,owner->registration.handle);
}
#endif
