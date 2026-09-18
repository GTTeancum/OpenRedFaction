/* First-pass NPC primary cadence, shared by PC and Xbox.
 * Authored fire/burst timings are already decoded by rf_weapon_primary_load.
 * This is a port gameplay policy, not a reconstruction of the original AI
 * scheduler. A successful shot advances the burst; aim/LOS/range failures do
 * not. The interval between burst starts is at least fire_seconds, and a
 * burst's shots remain separated by burst_seconds. Reload/ammo is separate.
 * Include after rf_weapon_primary_definition is visible. No allocation. */
static int campaign_enemy_cadence(const rf_weapon_primary_definition *definition,
    uint32_t frame,uint32_t *remaining,uint32_t *due)
{
    uint32_t count=1,fire_ticks=60,burst_ticks=0,next,delay,span;
    if(!remaining || !due)return RF_RANGE;
    if(definition) {
        if(!isfinite(definition->fire_seconds) || definition->fire_seconds<=0 ||
           definition->fire_seconds>60 || !definition->burst_count ||
           definition->burst_count>32)return RF_RANGE;
        count=definition->burst_count;
        fire_ticks=(uint32_t)ceilf(definition->fire_seconds*60.0f);
        if(count>1) {
            if(!isfinite(definition->burst_seconds) || definition->burst_seconds<=0 ||
               definition->burst_seconds>60)return RF_RANGE;
            burst_ticks=(uint32_t)ceilf(definition->burst_seconds*60.0f);
        }
    }
    /* A weapon change can leave an old burst. Start this weapon afresh when
     * the retained count cannot describe it; target changes clear it upstream. */
    next=*remaining;
    if(!next || next>=count)next=count;
    --next;
    span=(count-1)*burst_ticks;
    delay=next?burst_ticks:(fire_ticks>span?fire_ticks-span:1);
    if(frame>UINT32_MAX-delay)return RF_RANGE;
    *remaining=next;*due=frame+delay;
    return RF_OK;
}

/* Magazine/reload gate for supported NPC firearms. Call every combat tick,
 * before the shot deadline/aim gate; consume one loaded round only once a
 * shot is emitted. Existing startup grants already fill magazine and reserve.
 * Zero reload_due means idle. Weapon changes cancel the old reload without
 * consuming reserve. Events:0 none,1 reload started,2 completed,3 exhausted.
 * Finite ammo is deliberate; no automatic replenishment or melee selection.
 * The original inventory primitives are reused; timing is a port policy. */
static int campaign_enemy_ammo_ready(rf_weapon_inventory *inventory,
    const rf_weapon_acquire_definition *ammo,const rf_weapon_primary_definition *definition,
    int32_t weapon,uint32_t frame,uint32_t *reload_due,int32_t *reload_weapon,
    uint32_t *ready,uint32_t *event)
{
    uint32_t deadline,ticks,transferred=0,kind=0,can_fire=0;int32_t reloading;int status;
    if(!inventory || !ammo || !definition || !reload_due || !reload_weapon || !ready || !event ||
       weapon<0 || weapon>=64 || !inventory->owned[weapon] ||
       ammo->ammo_type<0 || ammo->ammo_type>=32 || ammo->magazine<=0 ||
       inventory->loaded[weapon]<0 || inventory->loaded[weapon]>ammo->magazine ||
       inventory->reserve[ammo->ammo_type]<0 || !isfinite(definition->reload_seconds) ||
       definition->reload_seconds<=0 || definition->reload_seconds>60)return RF_RANGE;
    deadline=*reload_due;reloading=*reload_weapon;
    if(deadline && reloading!=weapon)deadline=0;
    if(deadline && frame>=deadline) {
        status=rf_weapon_reload_transfer(inventory,ammo,weapon,&transferred);if(status)return status;
        deadline=0;kind=2;
    }
    if(!deadline) {
        if(inventory->loaded[weapon]>0)can_fire=1;
        else if(inventory->reserve[ammo->ammo_type]>0) {
            ticks=(uint32_t)ceilf(definition->reload_seconds*60.0f);
            if(frame>UINT32_MAX-ticks)return RF_RANGE;
            deadline=frame+ticks;reloading=weapon;kind=1;
        } else if(!kind)kind=3;
    }
    *reload_due=deadline;*reload_weapon=reloading;*ready=can_fire;*event=kind;
    return RF_OK;
}
