#ifndef RF_SCENE_SWITCH_AMBIENT_INC
#define RF_SCENE_SWITCH_AMBIENT_INC
/* Original4bc340 ->45afe0 ->45b010/45b040 ->505b50: change the existing
 * ambient control slot's volume only. No sample/voice allocation or restart.
 *
 * Bounded first-playable ownership: one initially enabled Switch is the only
 * event/trigger writer of an immediate-start sound. This is the authored L7S1
 * energy-sound arrangement and lets RFEC/RFCH's existing Switch state restore
 * the volume without a new sound-save owner. Other arrangements remain inert;
 * in particular, do not invent latent gating for a delayed/unallocated slot.
 * Include after the ambient services and before the Switch backend. */
uint32_t rf_scene_switch_ambient[8]; /* lookups,requests,writes,no slot,restore writes,last UID,slot,volume bits */
static rf_ambient_instance *campaign_switch_ambient_binding(uint32_t uid,
    const rf_runtime_event **source)
{
    rf_ambient_instance *instance=NULL;const rf_runtime_event *owner=NULL;
    uint32_t i,j;
    if(source)*source=NULL;
    for(i=0;i<campaign_ambient_instances.count;i++)if(campaign_ambient_instances.items[i].uid==uid) {
        if(instance)return NULL;
        instance=campaign_ambient_instances.items+i;
    }
    if(!instance || instance->authored_word || instance->sample<0)return NULL;
    for(i=0;i<campaign_events.count;i++) {
        const rf_runtime_event *event=campaign_events.items+i;uint32_t linked=0;
        if(!event->authored)return NULL;
        for(j=0;j<event->authored->record.link_count;j++)if(event->authored->links[j]==uid) {
            /* Sound IDs deliberately remain authored UIDs. Never reinterpret
             * a resolved object/key owner (even a stale handle) as a sound. */
            if(!event->links || event->links[j].kind || event->links[j].value!=uid)return NULL;
            linked=1;
        }
        if(!linked)continue;
        if(owner || event->state.type!=32 || !event->switch_state ||
           event->authored->record.words[0])return NULL;
        owner=event;
    }
    if(!owner)return NULL;
    for(i=0;i<campaign_triggers.count;i++) {
        const rf_runtime_trigger *trigger=campaign_triggers.items+i;
        if(!trigger->authored)return NULL;
        for(j=0;j<trigger->authored->record.link_count;j++)
            if(trigger->authored->links[j]==uid)return NULL;
    }
    /* Section history does not retain the final state of a removed Switch.
     * No shipped energy owner has an incoming Remove_Object. Keep this
     * unsupported lifecycle outside the reconstructible-volume contract. */
    for(i=0;i<campaign_events.count;i++) {
        const rf_runtime_event *event=campaign_events.items+i;
        if(event->state.type!=2)continue;
        for(j=0;j<event->authored->record.link_count;j++)
            if(event->authored->links[j]==owner->authored->record.uid)return NULL;
    }
    {
        void *registered=rf_object_registry_lookup(&campaign_registry,owner->handle);
        if(owner->retired?registered!=NULL:registered!=owner)return NULL;
    }
    if(source)*source=owner;
    return instance;
}
static void campaign_switch_ambient_volume(rf_ambient_instance *instance,
    uint32_t enabled,uint32_t restoring)
{
    float volume=enabled?instance->volume:0;
    int32_t slot=instance->slot;
    /* Slots belong to this level instance; device voices retain their existing
     * independent generation-qualified IDs. An absent/reused slot is a no-op. */
    if(slot<0 || (uint32_t)slot>=RF_AMBIENT_SLOTS ||
       campaign_ambient_slots[slot].sample!=instance->sample) {
        ++rf_scene_switch_ambient[3];return;
    }
    rf_ambient_slot_volume(campaign_ambient_slots,1,slot,volume);
    ++rf_scene_switch_ambient[2];rf_scene_switch_ambient[4]+=restoring;
    rf_scene_switch_ambient[5]=instance->uid;rf_scene_switch_ambient[6]=(uint32_t)slot;
    memcpy(rf_scene_switch_ambient+7,&volume,4);
}
static int campaign_switch_ambient_lookup(uint32_t family,uint32_t uid,rf_switch_target *target)
{
    if(family!=RF_SWITCH_SOUND)return RF_NOT_FOUND;
    if(!target)return RF_RANGE;
    if(!campaign_switch_ambient_binding(uid,NULL))return RF_NOT_FOUND;
    memset(target,0,sizeof(*target));target->token=uid;
    ++rf_scene_switch_ambient[0];return RF_OK;
}
static int campaign_switch_ambient_dispatch(const rf_switch_request *request)
{
    rf_ambient_instance *instance;
    if(!request)return RF_RANGE;
    if(request->family!=RF_SWITCH_SOUND)return RF_NOT_FOUND;
    if(request->enabled>1)return RF_RANGE;
    instance=campaign_switch_ambient_binding(request->token,NULL);
    if(!instance)return RF_OK;
    ++rf_scene_switch_ambient[1];
    campaign_switch_ambient_volume(instance,request->enabled,0);return RF_OK;
}
/* Assignment-only repair after the ordinary world transaction has published
 * its validated RFEC states. Do not fire Switches, consume limits, reset audio
 * timers/cursors, or replay any ordinary downstream event/controller links. */
static void campaign_switch_ambient_restore(void)
{
    uint32_t i;
    for(i=0;i<campaign_ambient_instances.count;i++) {
        const rf_runtime_event *source=NULL;
        rf_ambient_instance *instance=campaign_switch_ambient_binding(
            campaign_ambient_instances.items[i].uid,&source);
        if(instance)campaign_switch_ambient_volume(instance,source->switch_state->disabled==0,1);
    }
}
#endif
