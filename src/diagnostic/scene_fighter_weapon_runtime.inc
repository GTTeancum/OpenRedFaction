#ifndef SCENE_FIGHTER_WEAPON_RUNTIME_INC
#define SCENE_FIGHTER_WEAPON_RUNTIME_INC
/* Include after APC and submarine runtime providers. Fixed authored guns:
 * primary muzzle supplies aim; secondary tags supply launch positions only.
 * Their authored +Z points downward. Undeviating skips random spread. Homing acquires
 * live hostile actors without the submarine's wet-point restriction. */
uint32_t rf_scene_fighter_weapon[8];
static int scene_fighter_live_candidate(void *context,uint32_t index,scene_submarine_homing_candidate *out)
{
    const campaign_npc_body *npc;(void)context;
    if(index>=campaign_npc_body_count)return RF_RANGE;
    npc=campaign_npc_bodies+index;
    if(!npc->registration.view||!npc->body.allocated_bytes||npc->damage.effects.health<=0||
       npc->damage.effects.affiliation!=0||(npc->object_flags&(2u|0x4000u))||(npc->view.flags_810&1u))return RF_OK;
    if(rf_entity_lookup(&campaign_entities,(int32_t)npc->registration.handle)!=&npc->view)return RF_OK;
    out->handle=npc->registration.handle;out->alive=out->hostile=1;memcpy(out->position,npc->eye_position,12);return RF_OK;
}
static int scene_fighter_live_launch(void *context,uint32_t source,const scene_vehicle_primary_definition *definition,
    const float muzzle[12],const float direction[3],uint32_t *accepted)
{
    scene_stream *s=context;scene_submarine_weapon_round round={0};uint32_t i;int status;
    for(i=0;i<48&&s->fighter_weapon.bullets[i].flight.active;i++){}
    if(i==48){*accepted=0;return RF_OK;}
    status=rf_weapon_flight_launch(&round.flight,muzzle+9,direction,definition->speed,definition->lifetime,definition->radius);if(status)return status;
    round.source=source;round.driver=s->driller_runtime->entry.host.driver;memcpy(round.previous,muzzle+9,12);
    s->fighter_weapon.bullets[i]=round;*accepted=1;return RF_OK;
}
static int scene_fighter_weapon_tick(scene_stream *s,uint32_t frame)
{
    scene_fighter_weapon_state *state;scene_driller_runtime *runtime;float aim_pose[12],dt=frame?scene_step_seconds:0;
    uint32_t i,k,allowed,fired,free_slot;int status;
    if(!s)return RF_RANGE;
    if(rf_scene_vehicle_enabled!=5||!s->driller_runtime)return RF_OK;
    state=&s->fighter_weapon;runtime=s->driller_runtime;
    if(!frame)memset(rf_scene_fighter_weapon,0,sizeof(rf_scene_fighter_weapon));
    ++rf_scene_fighter_weapon[0];rf_scene_fighter_weapon[5]=0;
    for(i=0;i<48;i++){
        scene_submarine_weapon_round *round=state->bullets+i;scene_apc_primary_query query={s,round->source,round->driver};
        rf_weapon_flight_event event;float end[3];
        if(!round->flight.active)continue;
        memcpy(round->previous,round->flight.position,12);
        for(k=0;k<3;k++)end[k]=round->flight.position[k]+round->flight.velocity[k]*fminf(dt,(float)round->flight.remaining);
        status=rf_weapon_flight_step(&round->flight,dt,scene_apc_primary_sweep,&query,&event);if(status)return status;
        if(event.kind==1){
            scene_apc_primary_round direct={0};direct.source=round->source;direct.driver=round->driver;
            direct.damage=state->primary.damage;direct.kind=state->primary.damage_kind;
            status=scene_apc_primary_damage(s,frame,&direct,&event.contact,round->previous,end);if(status)return status;
            ++rf_scene_fighter_weapon[3];
        }
        if(round->flight.active)++rf_scene_fighter_weapon[5];
    }
    for(i=0;i<4;i++){
        scene_submarine_weapon_round *round=state->rockets+i;scene_apc_primary_query query={s,round->source,round->driver};
        rf_weapon_flight_liquid_event event;uint32_t target;float end[3],origin[3],elapsed;
        if(!round->flight.active)continue;
        elapsed=fminf(dt,(float)round->flight.remaining);
        status=scene_fighter_rocket_step(round,&state->rocket,dt,scene_fighter_live_candidate,s,campaign_npc_body_count,
            scene_submarine_homing_sight,s,scene_apc_primary_sweep_all,&query,&event,&target);if(status)return status;
        /* Steering preceded sweep: current velocity defines the same full ray. */
        for(k=0;k<3;k++)end[k]=round->previous[k]+round->flight.velocity[k]*elapsed;
        if(event.terminal.kind==1){
            rf_weapon_flight_contact *contact=&event.terminal.contact;scene_apc_primary_round direct={0};
            direct.source=round->source;direct.driver=round->driver;direct.damage=state->rocket.damage;direct.kind=3;
            status=scene_apc_primary_damage(s,frame,&direct,contact,round->previous,end);if(status)return status;
            for(k=0;k<3;k++)origin[k]=contact->hit.point[k]+contact->hit.normal[k]*.01f;
            status=scene_impact_start_sized(s,contact,frame,1.5f);if(status)return status;
            scene_impact_sound(origin,frame);
            status=scene_explosion_blast_source(s,frame,origin,state->rocket.damage,state->rocket.blast,round->source,3);if(status)return status;
            status=scene_explosion_terrain(s,frame,contact,state->rocket.crater);if(status)return status;
            ++rf_scene_fighter_weapon[4];
        }
        if(round->flight.active)++rf_scene_fighter_weapon[5];
    }
    allowed=runtime->entry.session.active&&runtime->entry.host.alive&&campaign_player_damage.state.effects.health>0;
    status=scene_fighter_weapon_fire(state,&s->driller->tags,runtime->physics.state.position,runtime->physics.state.orientation,NULL,
        runtime->entry.host.handle,runtime->entry.host.driver,0,player_input.fire,allowed,dt,scene_fighter_live_launch,s,&fired);if(status)return status;
    if(fired){++rf_scene_fighter_weapon[1];++rf_scene_fighter_weapon[5];combat_sound("Vauss1 Fire",runtime->physics.state.position);}
    for(free_slot=0;free_slot<4&&state->rockets[free_slot].flight.active;free_slot++){}
    /* Fixed forward guns: preserve the rocket mount origin, but use the
     * actual primary muzzle forward, transformed with the flying chassis. */
    status=rf_static_model_tag_place(&s->driller->tags,state->primary_muzzle,
        runtime->physics.state.orientation,runtime->physics.state.position,aim_pose);if(status)return status;
    status=scene_fighter_weapon_fire(state,&s->driller->tags,runtime->physics.state.position,runtime->physics.state.orientation,aim_pose+6,
        runtime->entry.host.handle,runtime->entry.host.driver,1,player_input.alt_fire,allowed,dt,NULL,NULL,&fired);if(status)return status;
    if(fired){++rf_scene_fighter_weapon[2];++rf_scene_fighter_weapon[5];combat_sound("Fighter Rocket Launch",state->rockets[free_slot].flight.position);}
    rf_scene_fighter_weapon[6]=(uint32_t)state->primary_reserve;rf_scene_fighter_weapon[7]=(uint32_t)state->rocket_reserve;return RF_OK;
}
/* Minigun tracer is explicit shared first-pass beam presentation. Actual
 * DrillMissile01.VFX rocket submission is integrated in scene_rockets_draw. */
static int scene_fighter_weapon_draw(scene_stream *s)
{
    uint32_t i,j,k;int status;
    if(!s||!s->mesh||!s->mesh->vertices||s->mesh->bytes>s->capacity||
       s->mesh->count>s->capacity/sizeof(rf_preview_vertex)||s->mesh->bytes!=s->mesh->count*sizeof(rf_preview_vertex))return RF_RANGE;
    if(rf_scene_vehicle_enabled!=5)return RF_OK;
    for(i=0;i<48;i++){
        const scene_submarine_weapon_round *round=s->fighter_weapon.bullets+i;rf_particle_screen_polygon polygon;float length=0;
        if(!round->flight.active)continue;
        for(k=0;k<3;k++){float d=round->flight.position[k]-round->previous[k];length+=d*d;}
        if(length<1e-10f)continue;
        status=rf_volume_beam_project(&s->particle_camera,round->flight.position,round->previous,.04f,&polygon);if(status)return status;
        if(polygon.count>12)return RF_RANGE;
        if(polygon.count<3)continue;
        for(j=0;j<polygon.count;j++)if(!isfinite(polygon.vertices[j].reciprocal_z)||polygon.vertices[j].reciprocal_z<=0||
            !isfinite(polygon.vertices[j].screen[0])||!isfinite(polygon.vertices[j].screen[1]))return RF_FORMAT;
        if((polygon.count-2)*3>(s->capacity-s->mesh->bytes)/sizeof(rf_preview_vertex))return RF_RANGE;
        for(j=1;j+1<polygon.count;j++)for(k=0;k<3;k++){
            uint32_t index=k==0?0:(k==1?j:j+1);const rf_particle_screen_vertex *in=polygon.vertices+index;
            rf_preview_vertex *out=s->mesh->vertices+s->mesh->count++;float q=in->reciprocal_z;
            memset(out,0,sizeof(*out));out->position[0]=in->screen[0];out->position[1]=in->screen[1];
            out->position[2]=(1000.f/999.9f)*(1-.1f*q)*16777215.f;
            out->color[0]=1;out->color[1]=.85f;out->color[2]=.45f;
            out->texture[0]=in->uv[0]*q;out->texture[1]=in->uv[1]*q;out->texture[2]=q;
            out->material=UINT32_MAX;out->lightmap=RF_PREVIEW_ADDITIVE_FADE_TAG|220u;out->lightmap_texture[2]=q;
        }
        s->mesh->bytes=s->mesh->count*sizeof(rf_preview_vertex);
    }
    return RF_OK;
}
#endif
