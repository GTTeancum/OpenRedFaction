#ifndef RF_SCENE_CONTINUOUS_CHARGE_CHECKPOINT_INCLUDED
#define RF_SCENE_CONTINUOUS_CHARGE_CHECKPOINT_INCLUDED
/* RFAP5 kind11: a singleton player role, not an authored object UID. The
 * independent profile names real FP resources; it is not an ammo owner.
 * All times here are relative until candidate preparation rebases f90. */
static scene_continuous_charge_state scene_continuous_charge_checkpoint_empty(void)
{
    scene_continuous_charge_state value={-1,-1,0,0,-1,-1,-1,0};return value;
}
static uint32_t scene_continuous_charge_checkpoint_zero(float value)
{
    uint32_t bits;memcpy(&bits,&value,4);return bits==0;
}
static int scene_continuous_charge_checkpoint_shape(const scene_continuous_charge_state *value)
{
    if(!value||value->current< -1||value->current>2||value->pending< -1||value->pending>2||
       !isfinite(value->duration)||!isfinite(value->elapsed)||value->elapsed<0||
       value->elapsed>.2f+scene_step_seconds||value->last_profile< -1||value->last_profile>18||
       value->due_ms< -1||value->due_ms>25||value->episode_weapon< -1||value->episode_weapon>=64||
       (value->episode_mode!=0&&value->episode_mode!=0x20u))return RF_FORMAT;
    /* Signed zero is not producer state. Other admitted binary32 bits survive
     * exactly, including the source's completed-transition overshoot. */
    if((value->elapsed==0&&!scene_continuous_charge_checkpoint_zero(value->elapsed))||
       (value->duration==0&&!scene_continuous_charge_checkpoint_zero(value->duration)))return RF_FORMAT;
    if(value->duration==0){
        if(value->pending!=-1||(value->current==-1&&value->elapsed!=0))return RF_FORMAT;
    }else if(value->duration!=.2f||value->pending<0||value->elapsed>=value->duration)return RF_FORMAT;
    if(value->episode_weapon==-1&&value->episode_mode)return RF_FORMAT;
    return RF_OK;
}
static int scene_continuous_charge_checkpoint_row(const unsigned char *row,scene_continuous_charge_state *value)
{
    scene_continuous_charge_state empty=scene_continuous_charge_checkpoint_empty();
    if(!row||!value||checkpoint_u32(row)!=11||checkpoint_u32(row+4)||checkpoint_u32(row+8)||checkpoint_u32(row+12))return RF_FORMAT;
    value->current=(int32_t)checkpoint_u32(row+16);value->pending=(int32_t)checkpoint_u32(row+20);
    value->duration=checkpoint_float(row+24);value->elapsed=checkpoint_float(row+28);
    value->due_ms=(int32_t)checkpoint_u32(row+32);value->episode_weapon=(int32_t)checkpoint_u32(row+36);
    value->episode_mode=checkpoint_u32(row+40);value->last_profile=(int32_t)checkpoint_u32(row+44);
    for(uint32_t i=48;i<112;i++)if(row[i])return RF_FORMAT;
    if(!memcmp(value,&empty,sizeof(empty)))return RF_FORMAT; /* Conditional canonical absence. */
    return scene_continuous_charge_checkpoint_shape(value);
}
/* Read-only cheap preflight runs before QuickLoad accepts a scene replacement.
 * It has no dependency on the current held input, episode or world resources. */
static int scene_continuous_charge_checkpoint_scan(const void *data,uint32_t bytes,
    scene_continuous_charge_state *value,uint32_t *ignition)
{
    const unsigned char *p=data;uint32_t version,count,seen=0,flame_seen=0,active=0;
    int32_t flame=rf_weapon_name_find(&campaign_weapon_supply.names,"Flamethrower");
    if(!value||!ignition)return RF_RANGE;
    *value=scene_continuous_charge_checkpoint_empty();*ignition=0;
    if(!bytes)return data?RF_FORMAT:RF_OK;
    if(!p||bytes<16||bytes>RF_CHECKPOINT_FILE_MAX||memcmp(p,"RFAP",4)||checkpoint_u32(p+12))return RF_FORMAT;
    version=checkpoint_u32(p+4);count=checkpoint_u32(p+8);
    if(version<1||version>5||!count||count>(RF_CHECKPOINT_FILE_MAX-16u)/112u||bytes!=16+count*112u)return RF_FORMAT;
    for(uint32_t i=0;i<count;i++){
        const unsigned char *row=p+16+112*i;uint32_t kind=checkpoint_u32(row);
        if(!kind||kind>(version==1?5u:version==2?7u:version==3?9u:version==4?10u:11u))return RF_FORMAT;
        if(kind==11){int status;if(seen++)return RF_FORMAT;
            status=scene_continuous_charge_checkpoint_row(row,value);if(status)return status;
        }else if(kind==9){
            uint32_t delay=checkpoint_u32(row+20),debt=checkpoint_u32(row+36);
            if(flame_seen++||checkpoint_u32(row+4)||checkpoint_u32(row+8)||checkpoint_u32(row+12))return RF_FORMAT;
            *ignition=checkpoint_u32(row+16);active=checkpoint_u32(row+48);
            if(*ignition>2||delay>6||active>1||(!*ignition&&delay)||(*ignition==2&&delay)||(active&&*ignition!=2))return RF_FORMAT;
            /* Legacy debt and ignition provenance have no unique pair/f90
             * conversion. Reload, canister and damage cadence remain encoded. */
            if(version<5&&(debt||*ignition||delay||active))return RF_NOT_FOUND;
            if(version==5&&debt)return RF_FORMAT;
        }
    }
    if(value->episode_weapon!=-1&&value->episode_weapon!=flame)return RF_NOT_FOUND;
    if(*ignition){
        if(version<5||!seen||flame<0||value->episode_weapon!=flame||value->episode_mode)return RF_FORMAT;
    }else if(value->episode_weapon!=-1)return RF_FORMAT;
    return RF_OK;
}
static int scene_continuous_charge_checkpoint_preflight(const void *data,uint32_t bytes)
{
    scene_continuous_charge_state value;uint32_t ignition;
    return scene_continuous_charge_checkpoint_scan(data,bytes,&value,&ignition);
}
static int scene_continuous_charge_checkpoint_relative(scene_continuous_charge_state *value)
{
    int status;if(!value)return RF_RANGE;*value=scene_continuous_charge;
    if(value->due_ms>=0){
        status=rf_timer_remaining(value->due_ms,scene_continuous_charge_now(),&value->due_ms);if(status)return status;
        if(value->due_ms<0)value->due_ms=0;
    }
    return scene_continuous_charge_checkpoint_shape(value);
}
static int scene_continuous_charge_checkpoint_save_guard(const scene_stream *stream)
{
    scene_continuous_charge_state value;int status;
    if(scene_continuous_charge_busy())return RF_NOT_FOUND;
    status=scene_continuous_charge_live_admit();if(status)return status;
    status=scene_continuous_charge_checkpoint_relative(&value);if(status)return status;
    status=scene_continuous_charge_profile_admit(stream,value.last_profile);if(status)return status;
    if(value.episode_weapon!=-1&&value.episode_weapon!=campaign_flame_id)return RF_NOT_FOUND;
    if((scene_flame_input.ignition!=0)!=(value.episode_weapon!=-1)||
       (value.episode_weapon!=-1&&(value.episode_mode||campaign_equipped_slot!=10||campaign_explicit_unarmed)))return RF_FORMAT;
    return RF_OK;
}
static int scene_continuous_charge_checkpoint_standalone(void)
{
    /* RFCP has no shared-owner lane. Candidate validation does not use this
     * live guard; its in-session publication and all exports do. */
    return scene_continuous_charge_busy()||!scene_continuous_charge_is_constructor()?RF_NOT_FOUND:RF_OK;
}
static int scene_continuous_charge_checkpoint_capture(const scene_stream *stream,unsigned char *row,uint32_t *count)
{
    scene_continuous_charge_state value;int status;
    if(!count)return RF_RANGE;
    status=scene_continuous_charge_checkpoint_save_guard(stream);if(status)return status;
    *count=!scene_continuous_charge_is_constructor();if(!*count)return RF_OK;
    status=scene_continuous_charge_checkpoint_relative(&value);if(status)return status;
    if(!row)return RF_OK;
    checkpoint_put(row,11);checkpoint_put(row+16,(uint32_t)value.current);checkpoint_put(row+20,(uint32_t)value.pending);
    checkpoint_put_float(row+24,value.duration);checkpoint_put_float(row+28,value.elapsed);
    checkpoint_put(row+32,(uint32_t)value.due_ms);checkpoint_put(row+36,(uint32_t)value.episode_weapon);
    checkpoint_put(row+40,value.episode_mode);checkpoint_put(row+44,(uint32_t)value.last_profile);return RF_OK;
}
static int scene_continuous_charge_checkpoint_prepare(const scene_stream *stream,const void *data,uint32_t bytes,
    const rf_campaign_player_state *player,scene_continuous_charge_checkpoint_stage *out)
{
    scene_continuous_charge_checkpoint_stage next={0};uint32_t ignition;int status;
    if(!stream||!player||!out||out->ready)return RF_RANGE;
    status=scene_continuous_charge_checkpoint_scan(data,bytes,&next.next,&ignition);if(status)return status;
    status=scene_continuous_charge_profile_admit(stream,next.next.last_profile);if(status)return status;
    if(ignition&&(campaign_flame_id<0||campaign_flame_id>=64||
       player->weapon!=(uint32_t)campaign_flame_id||!player->inventory.owned[campaign_flame_id]))return RF_FORMAT;
    /* No current held/ammo test: ignition1 already owns the source receipt. */
    if(next.next.due_ms>=0){status=rf_timer_set(&next.next.due_ms,scene_continuous_charge_now(),next.next.due_ms);if(status)return status;}
    next.before=scene_continuous_charge;next.epoch=scene_continuous_charge_runtime.epoch;
    next.source=campaign_player_object.handle;next.ready=1;*out=next;return RF_OK;
}
static int scene_continuous_charge_checkpoint_validate(const scene_continuous_charge_checkpoint_stage *stage)
{
    if(!stage||!stage->ready||stage->source!=campaign_player_object.handle||
       stage->epoch!=scene_continuous_charge_runtime.epoch||memcmp(&stage->before,&scene_continuous_charge,sizeof(stage->before))||
       rf_object_registry_lookup(&campaign_registry,stage->source)!=&campaign_player_object||
       rf_entity_lookup(&campaign_entities,(int32_t)stage->source)!=&campaign_player_view)return RF_FORMAT;
    return RF_OK;
}
static void scene_continuous_charge_checkpoint_finish(scene_continuous_charge_checkpoint_stage *stage,uint32_t success)
{
    if(!stage||!stage->ready)return;
    if(success)scene_continuous_charge_restore_publish(&stage->next);
    if(stage->locked)scene_continuous_charge_restore_scope(0);
    stage->locked=stage->ready=0;
}
#endif
