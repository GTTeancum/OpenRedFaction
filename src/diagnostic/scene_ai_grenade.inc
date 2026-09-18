#ifndef RF_SCENE_AI_GRENADE_INCLUDED
#define RF_SCENE_AI_GRENADE_INCLUDED
/* Include after scene_grenade_gameplay.inc. Real timed/gravity grenades;
 * release-at-accepted-attack is a first-pass NPC policy (no recovered throw
 * animation marker). Fixed pool owns flights beyond thrower death/removal. */
#define SCENE_AI_GRENADE_CAPACITY 8u
typedef struct scene_ai_grenade_projectile {
    scene_grenade_projectile projectile;uint32_t source;float damage;
} scene_ai_grenade_projectile;
static scene_ai_grenade_projectile scene_ai_grenades[SCENE_AI_GRENADE_CAPACITY];
/* launches, contacts, detonations, live, pool refusals */
uint32_t rf_scene_ai_grenades[5];
static uint32_t scene_ai_grenade_pending(void)
{uint32_t i;for(i=0;i<SCENE_AI_GRENADE_CAPACITY;i++)if(scene_ai_grenades[i].projectile.flight.lifecycle.active)return 1;return 0;}
/* Parent factors existing shared explosion implementation into this entry,
 * preserving its player-source wrapper for current callers. */
static int scene_explosion_blast_source(scene_stream *,uint32_t,const float *,float,float,uint32_t,int32_t);
static int scene_ai_grenade_aim(const float delta[3],float speed,float gravity,float direction[3])
{
    double d,v2,disc,tangent,cosine;uint32_t i;
    if(!delta || !direction || !isfinite(speed) || speed<=0 || !isfinite(gravity) || gravity<0)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(delta[i]))return RF_RANGE;
    d=sqrt((double)delta[0]*delta[0]+(double)delta[2]*delta[2]);
    if(d<.001 || gravity<.001){if(d<.001 && fabsf(delta[1])<.001)return RF_NOT_FOUND;memcpy(direction,delta,12);return RF_OK;}
    v2=(double)speed*speed;disc=v2*v2-gravity*(gravity*d*d+2*delta[1]*v2);
    if(disc<0)return RF_NOT_FOUND;
    tangent=(v2-sqrt(disc))/(gravity*d);cosine=1/sqrt(1+tangent*tangent);
    direction[0]=(float)(delta[0]/d*cosine);direction[1]=(float)(tangent*cosine);direction[2]=(float)(delta[2]/d*cosine);
    return RF_OK;
}
static int scene_ai_grenade_launch(campaign_npc_body *owner,const float target[3])
{
    scene_ai_grenade_projectile pending={0};float delta[3],direction[3];uint32_t i,j;int32_t ammo;int status;
    if(!owner || !target || campaign_grenade_id<0 || campaign_grenade_id>=64)return RF_RANGE;
    if(!owner->inventory.owned[campaign_grenade_id])return RF_NOT_FOUND;
    ammo=campaign_weapon_supply.definitions[campaign_grenade_id].ammo_type;
    if(ammo<0 || ammo>=32)return RF_RANGE;
    if(owner->inventory.reserve[ammo]<=0)return RF_NOT_FOUND;
    for(i=0;i<SCENE_AI_GRENADE_CAPACITY && scene_ai_grenades[i].projectile.flight.lifecycle.active;i++);
    if(i==SCENE_AI_GRENADE_CAPACITY){++rf_scene_ai_grenades[4];return RF_NOT_FOUND;}
    for(j=0;j<3;j++)delta[j]=target[j]-owner->eye_position[j];
    if(fabsf(scene_gravity.vector[0])>.001f || fabsf(scene_gravity.vector[2])>.001f || scene_gravity.vector[1]>0)return RF_NOT_FOUND;
    status=scene_ai_grenade_aim(delta,campaign_grenade.speed,-scene_gravity.vector[1],direction);if(status)return status;
    status=rf_grenade_flight_launch(&pending.projectile.flight,owner->eye_position,direction,campaign_grenade.speed,
        campaign_grenade.collision_radius,campaign_grenade.lifetime,0,0);if(status)return status;
    pending.source=owner->registration.handle;pending.damage=combat_enemy_primary_damage(campaign_primary+5);
    scene_ai_grenades[i]=pending;--owner->inventory.reserve[ammo];++rf_scene_ai_grenades[0];++rf_scene_ai_grenades[3];return RF_OK;
}
static void scene_ai_grenade_reset(void)
{memset(scene_ai_grenades,0,sizeof(scene_ai_grenades));memset(rf_scene_ai_grenades,0,sizeof(rf_scene_ai_grenades));}
static int scene_ai_grenade_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i;int status;rf_scene_ai_grenades[3]=0;
    for(i=0;i<SCENE_AI_GRENADE_CAPACITY;i++) {
        scene_ai_grenade_projectile *shot=scene_ai_grenades+i;rf_grenade_flight_event event;
        scene_ai_projectile_query query={stream,shot->source};
        if(!shot->projectile.flight.lifecycle.active)continue;
        status=rf_grenade_flight_step(&shot->projectile.flight,1.f/60,scene_gravity.vector,.45f,scene_ai_projectile_solid_sweep,&query,&event);if(status)return status;
        if(event.contacts){rf_scene_ai_grenades[1]+=event.contacts;shot->projectile.last_contact=event.contact;shot->projectile.has_contact=1;combat_sound("Grenade Bounce",event.position);}
        if(shot->projectile.flight.lifecycle.active)++rf_scene_ai_grenades[3];
        if(event.detonate) {
            ++rf_scene_ai_grenades[2];
            rf_weapon_flight_contact contact={0};rf_collision_room_location location;
            if(shot->projectile.has_contact)contact=shot->projectile.last_contact;else contact.hit.normal[1]=1;
            memcpy(contact.hit.point,event.position,12);contact.object=contact.face=UINT32_MAX;
            status=rf_geometry_collision_world_locate(stream->collision,event.position,&location);if(status)return status;contact.room=location.room;
            status=scene_impact_start(stream,&contact,frame);if(status)return status;
            scene_impact_sound(event.position,frame);
            status=scene_explosion_blast_source(stream,frame,event.position,shot->damage,campaign_grenade.damage_radius,shot->source,3);if(status)return status;
            status=scene_explosion_terrain(stream,frame,&contact,campaign_grenade.crater_radius);if(status)return status;
        }
    }
    return RF_OK;
}
#endif
