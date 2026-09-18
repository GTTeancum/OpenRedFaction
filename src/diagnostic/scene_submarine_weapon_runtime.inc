#ifndef SCENE_SUBMARINE_WEAPON_RUNTIME_INC
#define SCENE_SUBMARINE_WEAPON_RUNTIME_INC
#include "scene_submarine_projectile_collision.inc"
#include "scene_submarine_homing.inc"
/* Definition follows later with the common static projectile renderer. */
static int scene_apc_secondary_visual_draw(const scene_driller_resources *,scene_stream *,
    const float[3],const float[3],uint32_t,uint32_t,uint32_t *,uint32_t *);
/* Include after APC runtime and secondary visual helpers. Torpedo guidance
 * uses the shared explicit first-pass homing policies and live actor ownership.
 * Shared impact appearance is provisional, not charge_explode asset parity. */
uint32_t rf_scene_submarine_weapon[8]; /* ticks,launches,contacts,expired,live,liquid-stops,detonations,reserve */
static inline uint32_t scene_submarine_weapon_pending(const scene_stream *s)
{
    uint32_t i;for(i=0;i<SCENE_SUBMARINE_WEAPON_CAPACITY;i++)if(s->submarine_weapon.rounds[i].flight.active)return 1;
    return 0;
}
static int scene_submarine_weapon_muzzle_wet(void *context,const float point[3],uint32_t *wet)
{
    scene_stream *s=context;rf_collision_room_location room;const rf_liquid_room *liquid;float surface;int status;
    if(!s||!s->collision||!s->liquid_rooms||!wet)return RF_RANGE;
    status=rf_geometry_collision_world_locate(s->collision,point,&room);if(status)return status;
    *wet=0;if(room.room>=s->swim_room_count)return RF_OK;
    liquid=s->liquid_rooms+room.room;
    if(liquid->type!=1||!isfinite(liquid->minimum_y)||!isfinite(liquid->depth))return RF_OK;
    surface=liquid->minimum_y+liquid->depth;
    if(isfinite(surface)&&point[1]<=surface)*wet=1;
    return RF_OK;
}
/* First-pass player vehicle policy: affiliation0 is hostile, matching ordinary
 * player-directed enemy awareness. Hidden/dead actors and dry aim points are
 * excluded before LOS; generation-bearing handles remain the target identity. */
static int scene_submarine_homing_read(void *context,uint32_t index,scene_submarine_homing_candidate *out)
{
    scene_stream *s=context;const campaign_npc_body *npc;uint32_t wet;int status;
    if(index>=campaign_npc_body_count)return RF_RANGE;
    npc=campaign_npc_bodies+index;
    if(!npc->registration.view || !npc->body.allocated_bytes || npc->damage.effects.health<=0 ||
       npc->damage.effects.affiliation!=0 || (npc->object_flags&(2u|0x4000u)) || (npc->view.flags_810&1u))return RF_OK;
    if(rf_entity_lookup(&campaign_entities,(int32_t)npc->registration.handle)!=&npc->view)return RF_OK;
    status=scene_submarine_weapon_muzzle_wet(s,npc->eye_position,&wet);if(status)return status;
    out->handle=npc->registration.handle;out->alive=out->hostile=1;out->submerged=wet;
    memcpy(out->position,npc->eye_position,12);return RF_OK;
}
static int scene_submarine_homing_sight(void *context,const float start[3],const float end[3],
    uint32_t target,uint32_t source,uint32_t driver,uint32_t *clear)
{
    scene_stream *s=context;scene_apc_primary_query query={s,source,driver};
    rf_weapon_flight_contact contact={0};float delta[3];uint32_t i,found=0,liquid=0;int status;
    for(i=0;i<3;i++)delta[i]=end[i]-start[i];
    status=scene_apc_primary_sweep_all(&query,start,delta,0,0x1004u,&contact,&liquid,&found);if(status)return status;
    *clear=!found;
    if(found && !liquid && (contact.object&0xffff0000u)==SCENE_ACTOR_ROCKET_OWNER){
        i=contact.object&0xffffu;
        if(i<campaign_npc_body_count && campaign_npc_bodies[i].registration.handle==target)*clear=1;
    }
    return RF_OK;
}
static int scene_submarine_weapon_tick(scene_stream *s,uint32_t frame,uint32_t held,uint32_t allowed,const float *aim)
{
    scene_submarine_weapon_state *state;scene_driller_runtime *runtime;uint32_t i,k,fired=0,free_slot;
    float dt=frame?scene_step_seconds:0;int status;
    if(!s||held>1||allowed>1)return RF_RANGE;
    if(rf_scene_vehicle_enabled!=4||!s->driller_runtime)return RF_OK;
    state=&s->submarine_weapon;runtime=s->driller_runtime;
    if(!isfinite(dt)||dt<0||dt>.05f)return RF_RANGE;
    if(!frame)memset(rf_scene_submarine_weapon,0,sizeof(rf_scene_submarine_weapon));
    ++rf_scene_submarine_weapon[0];rf_scene_submarine_weapon[4]=0;
    for(i=0;i<SCENE_SUBMARINE_WEAPON_CAPACITY;i++){
        scene_submarine_weapon_round *round=state->rounds+i;rf_weapon_flight_liquid_event event;
        scene_apc_primary_query composed;scene_submarine_projectile_query query;float end[3],origin[3];
        if(!round->flight.active)continue;
        {uint32_t target;
         status=scene_submarine_homing_scan(round,&state->definition,dt,scene_submarine_homing_read,s,
            campaign_npc_body_count,scene_submarine_homing_sight,s,&target);if(status)return status;}
        composed.stream=s;composed.source=round->source;composed.driver=round->driver;
        query.world=s->collision;query.composed=scene_apc_primary_sweep_all;query.context=&composed;
        for(k=0;k<3;k++)end[k]=round->flight.position[k]+round->flight.velocity[k]*fminf(dt,(float)round->flight.remaining);
        status=scene_submarine_projectile_step(round,dt,&query,&round->liquid,0,-1,-1,&event);if(status)return status;
        if(event.has_liquid)++rf_scene_submarine_weapon[5];
        if(event.terminal.kind==2)++rf_scene_submarine_weapon[3];
        if(event.terminal.kind==1){
            rf_weapon_flight_contact *contact=&event.terminal.contact;scene_apc_primary_round direct={0};
            ++rf_scene_submarine_weapon[2];direct.source=round->source;direct.driver=round->driver;
            direct.damage=state->definition.damage;direct.kind=3;
            status=scene_apc_primary_damage(s,frame,&direct,contact,round->previous,end);if(status)return status;
            for(k=0;k<3;k++)origin[k]=contact->hit.point[k]+contact->hit.normal[k]*.01f;
            status=scene_impact_start_sized(s,contact,frame,2);if(status)return status;
            scene_impact_sound(origin,frame);
            status=scene_explosion_blast_source(s,frame,origin,state->definition.damage,state->definition.blast,round->source,3);if(status)return status;
            status=scene_explosion_terrain(s,frame,contact,state->definition.crater);if(status)return status;
            ++rf_scene_submarine_weapon[6];
        }
        if(round->flight.active)++rf_scene_submarine_weapon[4];
    }
    for(free_slot=0;free_slot<SCENE_SUBMARINE_WEAPON_CAPACITY&&state->rounds[free_slot].flight.active;free_slot++){}
    allowed=allowed&&runtime->entry.session.active&&runtime->entry.host.alive&&campaign_player_damage.state.effects.health>0;
    status=scene_submarine_weapon_fire(state,&s->driller->tags,runtime->physics.state.position,runtime->physics.state.orientation,
        aim,runtime->entry.host.handle,runtime->entry.host.driver,held,allowed,dt,scene_submarine_weapon_muzzle_wet,s,&fired);
    if(status)return status;
    if(fired){
        ++rf_scene_submarine_weapon[1];++rf_scene_submarine_weapon[4];
        combat_sound("TorpedoLaunch",state->rounds[free_slot].flight.position);
    }
    rf_scene_submarine_weapon[7]=(uint32_t)state->reserve;return RF_OK;
}
static int scene_submarine_weapon_draw(scene_stream *s)
{
    uint32_t i,submissions,vertices;int status;
    if(!s||!s->submarine_torpedo)return RF_OK;
    for(i=0;i<SCENE_SUBMARINE_WEAPON_CAPACITY;i++){
        const rf_weapon_flight *flight=&s->submarine_weapon.rounds[i].flight;
        if(!flight->active)continue;
        status=scene_apc_secondary_visual_draw(s->submarine_torpedo,s,flight->position,flight->velocity,
            s->submarine_torpedo_base,s->submarine_torpedo_textures,&submissions,&vertices);if(status)return status;
    }
    return RF_OK;
}
#endif
