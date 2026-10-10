#ifndef RF_SCENE_PLAYER_OUTSIDE_DAMAGE_INCLUDED
#define RF_SCENE_PLAYER_OUTSIDE_DAMAGE_INCLUDED
/* Original421170: local-player bit8 with positive CURRENT armor returns;
 * otherwise retained room+42 outside, class724 humanoid and !class728 mutant
 * request kind10 at10*frame_seconds. Serialized room byte30 owns static+42;
 * airlock pressure is separate+44, not an immunity or this exposure predicate.
 * Include after campaign_liquid_damage_tick and call once immediately after
 * it in the ordinary simulation step (original41e549 then41e54f).
 * Current body-room lookup is a port phase adaptation, not original retained
 * room timing. Attachment/mount/cutscene exclusions bound this on-foot slice;
 * they are not claimed original exemptions. No oxygen or hazard timer. */
/* ticks,qualified owners,armor skips,nonhumanoid/mutant skips,missing rooms,
 * outside rooms,requests,positive results,last request handle,room(index),
 * requested/applied/health after(float bits),frame,errors,status. */
uint32_t rf_scene_player_outside_damage[16];

static int scene_player_outside_damage_tick(scene_stream *stream,uint32_t frame,int32_t now)
{
    const rf_geometry *geometry;rf_collision_room_location room;
    uint32_t handle=campaign_player_object.handle,offset,clock_bits;int status;
    float amount=0,seconds=(float)((double)(frame+1)*scene_step_seconds);
    combat_feedback feedback={now,0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    rf_damage_request request={(float)(10.0*(double)scene_step_seconds),UINT32_MAX,10,0,UINT32_MAX,0};
    if(!frame){
        memset(rf_scene_player_outside_damage,0,sizeof(rf_scene_player_outside_damage));
        rf_scene_player_outside_damage[8]=rf_scene_player_outside_damage[9]=
            rf_scene_player_outside_damage[13]=UINT32_MAX;
    }
    ++rf_scene_player_outside_damage[0];rf_scene_player_outside_damage[15]=RF_OK;
    if(!campaign_spawn || !stream || !scene_actor_body.allocated_bytes ||
       campaign_player_object.object_kind!=0 || campaign_player_object.view!=&campaign_player_view ||
       (uint32_t)campaign_player_view.handle!=handle || campaign_player_view.type!=0 ||
       campaign_player_damage.state.effects.handle!=handle ||
       rf_object_registry_lookup(&campaign_registry,handle)!=&campaign_player_object ||
       rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&campaign_player_view ||
       !(campaign_player_view.flags_7c&8u) || (campaign_player_view.flags_7c&(2u|0x4000u)) ||
       (campaign_player_view.flags_810&1u) || !(campaign_player_damage.state.effects.health>0) ||
       campaign_player_view.linked_handle!=-1 || campaign_cutscene_runtime.active ||
       scene_driller_active(stream) || scene_turret_player_active())return RF_OK;
    ++rf_scene_player_outside_damage[1];
    /* Consume live armor AFTER this step's liquid damage, never class armor,
     * appearance/form choice, or the unrelated actor+810 suit bit40. */
    if(campaign_player_damage.state.effects.armor>0){
        ++rf_scene_player_outside_damage[2];return RF_OK;
    }
    geometry=stream->geometry;
    if(!stream->collision || !geometry || !geometry->data || !geometry->room_offsets)
        goto missing_room;
    status=scene_player_room_locate(stream,0,scene_actor_body.state.position,&room);
    if(status==RF_NOT_FOUND)goto missing_room;
    if(status)goto fail;
    if(room.room>=stream->collision->room_count || room.room>=geometry->rooms)goto missing_room;
    offset=geometry->room_offsets[room.room];
    /* Successfully opened geometry already validates variable record tails.
     * Require the complete fixed header, using subtraction without overflow. */
    if(offset>geometry->bytes || geometry->bytes-offset<40)goto missing_room;
    if(!geometry->data[offset+30])return RF_OK;
    ++rf_scene_player_outside_damage[5];
    if(!(campaign_player_damage.class_flags&0x20000u) ||
       (campaign_player_damage.state.effects.class_flags_728&8u)){
        ++rf_scene_player_outside_damage[3];return RF_OK;
    }
    memcpy(&clock_bits,&seconds,4);++rf_scene_player_outside_damage[6];
    rf_scene_player_outside_damage[8]=handle;rf_scene_player_outside_damage[9]=room.room;
    memcpy(rf_scene_player_outside_damage+10,&request.amount,4);
    rf_scene_player_outside_damage[13]=frame;
    status=rf_scene_player_damage_audio(handle,&request,1,clock_bits,now,
        &combat_pain_random,&effects,&amount);
    if(!status)status=feedback.status;
    memcpy(rf_scene_player_outside_damage+11,&amount,4);
    memcpy(rf_scene_player_outside_damage+12,&campaign_player_damage.state.effects.health,4);
    if(status)goto fail;
    rf_scene_player_outside_damage[7]+=amount>0;
    /* Shared kind10 pain/flash policy and next-step campaign_life_input own
     * consequences. Do not subtract health or start a second death path. */
    return RF_OK;
missing_room:
    ++rf_scene_player_outside_damage[4];return RF_OK;
fail:
    ++rf_scene_player_outside_damage[14];rf_scene_player_outside_damage[15]=(uint32_t)status;
    return status;
}
#endif
