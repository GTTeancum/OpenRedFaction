#ifndef RF_SCENE_PURSUIT_DELAY_RUNTIME_INCLUDED
#define RF_SCENE_PURSUIT_DELAY_RUNTIME_INCLUDED
/* Live-only logical admission. No resource callback, route or query is added.
 * The class/style cohort is immutable; current weapon/temporary owners affect
 * use, not retention. Existing Blind target qualification remains authoritative. */
static int scene_pursuit_delay_live_sync(uint32_t slot)
{
    campaign_npc_body *o;uint32_t seed,mode,target;int status;
    if(!campaign_npc_bodies||slot>=campaign_npc_body_count)return RF_RANGE;
    o=campaign_npc_bodies+slot;
    status=scene_pursuit_delay_admit(&o->pursuit_delay);if(status)return status;
    if(!o->blind.present){memset(&o->pursuit_delay,0,sizeof(o->pursuit_delay));return RF_OK;}
    if(o->pursuit_delay.flags&SCENE_PURSUIT_DELAY_PRESENT){
        if(o->pursuit_delay.target!=o->blind.target||o->blind.target!=scene_blind_target_handle(o))return RF_FORMAT;
        o->pursuit_delay.flags=SCENE_PURSUIT_DELAY_PRESENT|(o->combat_scripted<<SCENE_PURSUIT_DELAY_MODE_SHIFT);
        return RF_OK; /* Immutable cohort was established when admitted. */
    }
    status=scene_tactical_source(slot);
    if(status==RF_NOT_FOUND){memset(&o->pursuit_delay,0,sizeof(o->pursuit_delay));return RF_OK;}
    if(status)return status;
    target=scene_blind_target_handle(o);mode=o->combat_scripted;
    if(!target||o->blind.target!=target||!scene_blind_target_live(target))return RF_FORMAT;
    seed=(uint32_t)campaign_seeds.records.items[slot].record.uid;
    seed=(seed^o->registration.handle)*16777619u;seed=(seed^target^0x53494748u)*16777619u;
    return scene_pursuit_delay_begin(&o->pursuit_delay,target,mode,seed);
}
static int scene_pursuit_delay_age(campaign_npc_body *o,int32_t now)
{
    int status;if(!o)return RF_RANGE;
    status=scene_pursuit_delay_admit(&o->pursuit_delay);if(status)return status;
    return (o->pursuit_delay.flags&SCENE_PURSUIT_DELAY_PRESENT)?
        scene_pursuit_sight_age(&o->pursuit_delay.sight,now):RF_OK;
}
/* Gate for using the retained sample in ordinary ranged pursuit. Deliberately
 * preserve the existing practical range/hysteresis and navigation retry owner.
 * No attempt is made to replace the original whole state1/2/11 machine here. */
static uint32_t scene_pursuit_delay_use(const campaign_npc_body *o,
    const campaign_enemy_weapon_selection *selected,uint32_t target)
{
    return o&&selected&&selected->primary&&!selected->melee&&
        (o->pursuit_delay.flags&SCENE_PURSUIT_DELAY_PRESENT)&&
        o->pursuit_delay.target==target&&o->blind.present&&o->blind.target==target&&
        (o->pursuit_delay.flags>>SCENE_PURSUIT_DELAY_MODE_SHIFT)==o->combat_scripted&&
        o->view.weapons[0]>=0&&o->movement_slot<16&&
        (campaign_modes[o->movement_slot].index==1||campaign_modes[o->movement_slot].index==3);
}
#endif
