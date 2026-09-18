#ifndef RF_SCENE_APC_SECONDARY_RUNTIME_INC
#define RF_SCENE_APC_SECONDARY_RUNTIME_INC
/* Include after APC primary runtime (dual-exclusion sweep/direct damage) and
 * shared explosion helpers. Actual Morter01 rendering is a separate owner.
 * Contact explodes immediately; lifetime expiry retires the round like the
 * existing rocket adapter. No authored expire-explosion flag is established.
 * Shared impact appearance is provisional, not charge_explode asset parity. */
uint32_t rf_scene_apc_secondary[8]; /* ticks,launches,contacts,expired,live,pool-full,detonations,reserve */
static uint32_t scene_apc_secondary_pending(const scene_stream *stream)
{
    uint32_t i;for(i=0;i<SCENE_APC_SECONDARY_CAPACITY;i++)if(stream->apc_secondary.rounds[i].flight.active)return 1;
    return 0;
}
static int scene_apc_secondary_tick(scene_stream *stream,uint32_t frame,uint32_t held,uint32_t allowed,const float *aim)
{
    scene_apc_secondary_state *state;scene_driller_runtime *runtime;float dt=frame?scene_step_seconds:0;uint32_t i,k;int status;
    if(!stream||held>1||allowed>1)return RF_RANGE;
    state=&stream->apc_secondary;runtime=stream->driller_runtime;
    if(rf_scene_vehicle_enabled!=2||!runtime)return RF_OK;
    if(state->reserve<0||(uint32_t)state->reserve>state->definition.capacity||!isfinite(state->cooldown)||!isfinite(dt)||dt<0||dt>.05f)return RF_RANGE;
    ++rf_scene_apc_secondary[0];rf_scene_apc_secondary[4]=0;
    state->cooldown=fmaxf(0,state->cooldown-dt);
    for(i=0;i<SCENE_APC_SECONDARY_CAPACITY;i++){
        scene_apc_secondary_round *round=state->rounds+i;rf_weapon_flight_event event;scene_apc_primary_query query;
        float end[3],origin[3];
        if(!round->flight.active)continue;
        query.stream=stream;query.source=round->source;query.driver=round->driver;
        /* End must match the semi-implicit trajectory used by shield receive. */
        {float elapsed=fminf(dt,(float)round->flight.remaining);
         for(k=0;k<3;k++)end[k]=round->flight.position[k]+(round->flight.velocity[k]+scene_gravity.vector[k]*elapsed)*elapsed;}
        status=scene_apc_secondary_step(round,dt,scene_gravity.vector,scene_apc_primary_sweep,&query,&event);if(status)return status;
        if(event.kind==2)++rf_scene_apc_secondary[3];
        if(event.kind==1){
            rf_weapon_flight_contact contact=event.contact;
            memcpy(origin,round->flight.position,12);
            if(event.kind==1){
                scene_apc_primary_round direct={0};++rf_scene_apc_secondary[2];
                direct.source=round->source;direct.driver=round->driver;direct.damage=state->definition.damage;direct.kind=3;
                status=scene_apc_primary_damage(stream,frame,&direct,&contact,round->previous,end);if(status)return status;
                for(k=0;k<3;k++)origin[k]=contact.hit.point[k]+contact.hit.normal[k]*.01f;
            }
            status=scene_impact_start_sized(stream,&contact,frame,2.5f);if(status)return status;
            scene_impact_sound(origin,frame);
            status=scene_explosion_blast_source(stream,frame,origin,state->definition.damage,state->definition.blast,round->source,3);if(status)return status;
            /* Air expiry is not a fabricated terrain contact. */
            if(event.kind==1){status=scene_explosion_terrain(stream,frame,&contact,state->definition.crater);if(status)return status;}
            ++rf_scene_apc_secondary[6];
        }
        if(round->flight.active)++rf_scene_apc_secondary[4];
    }
    if(held&&allowed&&runtime->entry.session.active&&runtime->entry.host.alive&&
       campaign_player_damage.state.effects.health>0&&state->cooldown<=1.e-6f&&state->reserve>0){
        for(i=0;i<SCENE_APC_SECONDARY_CAPACITY&&state->rounds[i].flight.active;i++){}
        if(i==SCENE_APC_SECONDARY_CAPACITY)++rf_scene_apc_secondary[5];
        else{
            scene_apc_secondary_round round={0};float pose[12],aimed[3];const float *direction=aim;
            status=rf_static_model_tag_place(&stream->driller->tags,state->muzzle,runtime->physics.state.orientation,runtime->physics.state.position,pose);if(status)return status;
            if(stream->apc_aim_active){
                status=scene_apc_aim_direction(stream,pose+9,aimed);
                if(status==RF_NOT_FOUND){rf_scene_apc_secondary[7]=(uint32_t)state->reserve;return RF_OK;}
                if(status)return status;
                direction=aimed;
            }
            if(!direction)direction=pose+6;
            status=rf_weapon_flight_launch(&round.flight,pose+9,direction,state->definition.speed,state->definition.lifetime,state->definition.radius);if(status)return status;
            round.source=runtime->entry.host.handle;round.driver=runtime->entry.host.driver;memcpy(round.previous,pose+9,12);
            state->rounds[i]=round;--state->reserve;state->cooldown=state->definition.fire_seconds;
            ++rf_scene_apc_secondary[1];++rf_scene_apc_secondary[4];
            combat_sound("Mortar Launch",pose+9);
        }
    }
    rf_scene_apc_secondary[7]=(uint32_t)state->reserve;return RF_OK;
}
#endif
