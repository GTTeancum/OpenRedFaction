#ifndef RF_SCENE_NPC_LIQUID_DAMAGE_INCLUDED
#define RF_SCENE_NPC_LIQUID_DAMAGE_INCLUDED
/* Original general entity update41e4b0 calls421240 at41e549. The shared
 * rf_liquid_damage_prepare owns its exact lava/acid request, not a player
 * policy. Include after combat_death_start and the combat effect callbacks.
 * Call once after ordinary NPC movement/playback and room refresh. Current
 * body-origin membership is a bounded scene adaptation: original421240
 * consumes already-owned wet flags/room before the later refresh sequence.
 * This pass does not install NPC swimming, eye-wet state or drowning. */
/* passes,owners,cached rooms,room queries,wet bodies,linked-kind1 rejects,
 * lava requests,acid requests,positive results,death entries,stale owners,
 * errors,last request UID,handle,room(index),liquid type,material,
 * requested/applied/health before/health after(float bits),frame,
 * liquid surface(float bits),status. Last-request fields persist on dry steps. */
uint32_t rf_scene_npc_liquid_damage[24];

static void scene_npc_liquid_damage_reset(void)
{
    memset(rf_scene_npc_liquid_damage,0,sizeof(rf_scene_npc_liquid_damage));
    rf_scene_npc_liquid_damage[12]=rf_scene_npc_liquid_damage[13]=
        rf_scene_npc_liquid_damage[14]=rf_scene_npc_liquid_damage[21]=UINT32_MAX;
}

static uint32_t scene_npc_liquid_damage_owner(const campaign_npc_body *owner)
{
    uint32_t handle=owner->registration.handle;
    return owner->registration.view==&owner->view &&
        owner->view.handle==(int32_t)handle && owner->damage.effects.handle==handle &&
        rf_object_registry_lookup(&campaign_registry,handle)==&owner->registration &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&owner->view;
}

/* Reuse only the actor's own exact body-position cache. A retained room may
 * be older when publication/refresh was suppressed or its last query found
 * no room. In that case locate now, without replacing room/model ownership.
 * Never borrow the player, camera, support-contact or attachment-host room. */
static int scene_npc_liquid_damage_room(const scene_stream *stream,
    const campaign_npc_body *owner,uint32_t *room)
{
    rf_collision_room_location found;int status;
    if(owner->room.room && owner->room.room<=stream->collision->room_count &&
       owner->room.room<=stream->swim_room_count &&
       !memcmp(owner->room.query_position,owner->body.state.position,12)) {
        *room=owner->room.room-1;++rf_scene_npc_liquid_damage[2];return RF_OK;
    }
    ++rf_scene_npc_liquid_damage[3];
    status=rf_geometry_collision_world_locate(stream->collision,owner->body.state.position,&found);
    if(!status)*room=found.room;return status;
}

static int scene_npc_liquid_damage_tick(scene_stream *stream,uint32_t frame,int32_t now)
{
    uint32_t slot,clock_bits;int status=RF_OK;
    float seconds=(float)((double)(frame+1)*scene_step_seconds);
    if(!frame)scene_npc_liquid_damage_reset();
    ++rf_scene_npc_liquid_damage[0];rf_scene_npc_liquid_damage[23]=RF_OK;
    if(!stream){status=RF_RANGE;goto fail;}
    if(!stream->collision || !stream->liquid_rooms || !stream->swim_room_count)return RF_OK;
    if(campaign_npc_body_count && !campaign_npc_bodies){status=RF_RANGE;goto fail;}
    memcpy(&clock_bits,&seconds,4);
    for(slot=0;slot<campaign_npc_body_count;slot++) {
        campaign_npc_body *owner=campaign_npc_bodies+slot;const rf_entity_view *linked;
        const rf_liquid_room *liquid;uint32_t room=UINT32_MAX,entered=0;
        rf_liquid_damage_input input={0};rf_liquid_damage_result result;
        double surface;float surface_bits,amount=0,before;combat_feedback feedback={now,0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
            combat_random,combat_notify,combat_playing,combat_play,&feedback};
        /* Removed, player, original40a110 exclusion and dead owners are not
         * ordinary living NPCs. AI Waiting/Do_Nothing, frozen animation and
         * disabled physics do not suppress this per-step environmental hit. */
        if(!owner->registration.view || !owner->body.allocated_bytes ||
           (owner->object_flags&(2u|8u|0x4000u)) || (owner->view.flags_810&1u) ||
           owner->damage.effects.health<=0)continue;
        if(!scene_npc_liquid_damage_owner(owner)){
            ++rf_scene_npc_liquid_damage[10];continue;
        }
        ++rf_scene_npc_liquid_damage[1];
        input.target=owner->registration.handle;
        /*4290d0 resolves the full parent handle as an entity, then requires
         * class kind1. A different live kind or stale generation is not1. */
        linked=rf_entity_lookup(&campaign_entities,owner->view.linked_handle);
        input.rejection_4290d0=linked && linked->class_type==1;
        if(input.rejection_4290d0){++rf_scene_npc_liquid_damage[5];continue;}
        status=scene_npc_liquid_damage_room(stream,owner,&room);if(status)goto fail;
        if(room>=stream->swim_room_count)continue;
        liquid=stream->liquid_rooms+room;
        if(!isfinite(liquid->depth))continue; /* Dry rows are authored NaNs. */
        /*4ce08a..4ce097 adds authored minY/depth on the x87 stack and
         * compares directly with body Y, without a binary32 sum store.
         * Retain the wide sum for membership, round only diagnostic output.
         * Only421240's any-wet predicate is needed, not a guessed eye bit. */
        surface=(double)liquid->minimum_y+liquid->depth;
        if(!isfinite(surface)){status=RF_FORMAT;goto fail;}
        if(!((double)owner->body.state.position[1]<=surface))continue;
        ++rf_scene_npc_liquid_damage[4];
        input.actor_flags_810=(owner->view.flags_810&~0x3000u)|0x1000u;
        input.actor_kind_1fc=owner->collision_material;
        input.room_present=1;input.liquid_type=liquid->type;
        input.frame_seconds=scene_step_seconds;
        input.lava_per_second=campaign_liquid_damage_rates[0];
        input.acid_per_second=campaign_liquid_damage_rates[1];
        status=rf_liquid_damage_prepare(&input,&result);if(status)goto fail;
        if(!result.emit)continue;
        if(slot>=campaign_seeds.records.count || !campaign_seeds.records.items){status=RF_RANGE;goto fail;}
        ++rf_scene_npc_liquid_damage[result.request.kind==4?6:7];
        rf_scene_npc_liquid_damage[12]=(uint32_t)campaign_seeds.records.items[slot].record.uid;
        rf_scene_npc_liquid_damage[13]=result.target;rf_scene_npc_liquid_damage[14]=room;
        rf_scene_npc_liquid_damage[15]=(uint32_t)input.liquid_type;
        rf_scene_npc_liquid_damage[16]=input.actor_kind_1fc;
        memcpy(rf_scene_npc_liquid_damage+17,&result.request.amount,4);
        before=owner->damage.effects.health;
        memcpy(rf_scene_npc_liquid_damage+19,&before,4);
        rf_scene_npc_liquid_damage[21]=frame;
        surface_bits=(float)surface;memcpy(rf_scene_npc_liquid_damage+22,&surface_bits,4);
        status=rf_scene_npc_damage(result.target,&result.request,1,clock_bits,&effects,&amount);
        if(!status)status=feedback.status;
        memcpy(rf_scene_npc_liquid_damage+18,&amount,4);
        memcpy(rf_scene_npc_liquid_damage+20,&owner->damage.effects.health,4);
        if(status)goto fail;
        rf_scene_npc_liquid_damage[8]+=amount>0;
        if(owner->damage.effects.health<=0) {
            status=rf_scene_npc_death_entry(result.target,&entered);if(status)goto fail;
            if(entered){
                ++rf_scene_npc_liquid_damage[9];owner->script_move.active=0;
                status=combat_death_start(slot);if(status)goto fail;
            }
        }
    }
    return RF_OK;
fail:
    ++rf_scene_npc_liquid_damage[11];rf_scene_npc_liquid_damage[23]=(uint32_t)status;
    return status;
}
#endif
