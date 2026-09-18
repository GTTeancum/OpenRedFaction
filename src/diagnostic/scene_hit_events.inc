/* Shared hit signal is observed without consumption, then cleared after the
 * complete event pass. Damage occurring afterward belongs to the next pass. */
static int campaign_query_hit_flags(void *context,uint32_t handle,uint32_t *flags)
{
    void *object;uint32_t i;(void)context;if(!flags)return RF_RANGE;
    object=rf_object_registry_lookup(&campaign_registry,handle);if(!object)return RF_NOT_FOUND;
    if(campaign_player_object.view && handle==campaign_player_object.handle && object==&campaign_player_object){
        *flags=campaign_player_view.flags_7c;return RF_OK;}
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if(owner->registration.view && owner->registration.handle==handle && object==&owner->registration){*flags=owner->object_flags;return RF_OK;}}
    if(campaign_clutter_bodies)for(i=0;i<campaign_clutter_records.count;i++){
        rf_clutter_base_owner *owner=campaign_clutter_bodies[i];
        if(owner && owner->state.handle==handle && object==&owner->state){*flags=owner->state.flags;return RF_OK;}}
    return RF_NOT_FOUND;
}
static void campaign_hit_flags_clear(void)
{
    uint32_t i;
    campaign_player_view.flags_7c&=~0x200000u;campaign_player_damage.object_flags&=~0x200000u;
    for(i=0;i<campaign_npc_body_count;i++){
        campaign_npc_body *owner=campaign_npc_bodies+i;
        owner->object_flags&=~0x200000u;owner->view.flags_7c&=~0x200000u;owner->room.flags&=~0x200000u;}
    if(campaign_clutter_bodies)for(i=0;i<campaign_clutter_records.count;i++)
        if(campaign_clutter_bodies[i])campaign_clutter_bodies[i]->state.flags&=~0x200000u;
}
