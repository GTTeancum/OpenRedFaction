#ifndef RF_SCENE_PURSUIT_DELAY_WIRE_INCLUDED
#define RF_SCENE_PURSUIT_DELAY_WIRE_INCLUDED
/* Pure conversion only. Identity/cohort and composed target joins are caller
 * obligations. Never seed, sample, age or mutate a live receipt from capture. */
static int scene_pursuit_delay_capture_wire(const scene_pursuit_delay_receipt *r,
    uint32_t target_uid,int32_t now,rf_npc_checkpoint_pursuit_delay *out)
{
    rf_npc_checkpoint_pursuit_delay next={0};int32_t remaining;int status;
    if(!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    status=scene_pursuit_delay_admit(r);if(status)return status;
    if(r->flags&SCENE_PURSUIT_DELAY_PRESENT){
        next.present=1;next.target_uid=target_uid;next.sight_flags=r->sight.flags;
        next.random=r->sight.random.value;
        if((next.sight_flags&SCENE_PURSUIT_ARMED)&&!(next.sight_flags&SCENE_PURSUIT_EXPIRED)){
            status=rf_timer_remaining(r->sight.due,now,&remaining);if(status)return status;
            if(remaining>5000)return RF_FORMAT;
            if(remaining>0)next.remaining=(uint32_t)remaining;
            else next.sight_flags|=SCENE_PURSUIT_EXPIRED;
        }
    }else if(target_uid)return RF_FORMAT;
    *out=next;return RF_OK;
}
static int scene_pursuit_delay_prepare_wire(const rf_npc_checkpoint_pursuit_delay *wire,
    uint32_t profile,uint32_t target,uint32_t mode,int32_t now,scene_pursuit_delay_receipt *out)
{
    scene_pursuit_delay_receipt next={0};int status;
    if(!wire||!out||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    if(profile>1||wire->present>1||(wire->sight_flags&~7u)||wire->remaining>5000)return RF_FORMAT;
    if(!wire->present){
        if(target||mode||wire->target_uid||wire->sight_flags||wire->remaining||wire->random)return RF_FORMAT;
        *out=next;return RF_OK;
    }
    if(!profile||!target||target==UINT32_MAX||mode>2||
       ((wire->sight_flags&SCENE_PURSUIT_EXPIRED)&&!(wire->sight_flags&SCENE_PURSUIT_ARMED)))return RF_FORMAT;
    next.flags=SCENE_PURSUIT_DELAY_PRESENT|(mode<<SCENE_PURSUIT_DELAY_MODE_SHIFT);next.target=target;
    next.sight.flags=wire->sight_flags;next.sight.random.value=wire->random;
    if((wire->sight_flags&SCENE_PURSUIT_ARMED)&&!(wire->sight_flags&SCENE_PURSUIT_EXPIRED)){
        if(!wire->remaining)return RF_FORMAT;
        status=rf_timer_set(&next.sight.due,now,(int32_t)wire->remaining);if(status)return status;
    }else if(wire->remaining)return RF_FORMAT;
    status=scene_pursuit_delay_admit(&next);if(status)return status;
    *out=next;return RF_OK;
}
#endif
