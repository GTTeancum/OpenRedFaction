/* First-pass player grenade adapter. Fixed storage and fixed60Hz; authored
 * speed/radius/fuse/blast settings, recovered release and contact-mode gates.
 * Flight bounce policy is shared port code, not a retail rigid-body claim.
 * Projectile rendering, liquid response and removed resting support are separate. */
#include "rf/grenade_flight.h"
#include "rf/grenade_throw.h"
#define SCENE_GRENADE_CAPACITY 8u
typedef struct scene_grenade_projectile {
    rf_grenade_flight flight;
    rf_weapon_flight_contact last_contact;
    uint32_t has_contact;
} scene_grenade_projectile;
static scene_grenade_projectile scene_grenades[SCENE_GRENADE_CAPACITY];
static rf_grenade_throw scene_grenade_throw;
/* starts, releases, bounces, detonations, live, pool exhaustion, limited, status */
uint32_t rf_scene_grenades[8];
static int scene_grenade_sweep(void *context,const float start[3],const float delta[3],
    float radius,rf_weapon_flight_contact *contact,uint32_t *matched)
{
    uint32_t liquid=0;
    /* No liquid entry query bit: grenades retain gravity and cross water for now. */
    return scene_rocket_sweep(context,start,delta,radius,4u|(radius<.05f?0x100u:0),contact,&liquid,matched);
}
static int scene_grenade_detonate(scene_stream *s,uint32_t frame,
    const scene_grenade_projectile *grenade,const rf_grenade_flight_event *event)
{
    rf_weapon_flight_contact contact={0};rf_collision_room_location location;int status;
    if(grenade->has_contact)contact=grenade->last_contact;
    else contact.hit.normal[1]=1;
    /* A timed explosion is centered on the current grenade, not an old bounce.
     * Center/room admission permits nearby terrain cuts even without a direct hit. */
    memcpy(contact.hit.point,event->position,12);contact.hit.fraction=0;
    contact.object=UINT32_MAX;contact.face=UINT32_MAX;
    status=rf_geometry_collision_world_locate(s->collision,event->position,&location);if(status)return status;
    contact.room=location.room;
    status=scene_impact_start(s,&contact,frame);if(status)return status;
    status=scene_explosion_blast(s,frame,event->position,campaign_primary[5].damage,campaign_grenade.damage_radius);if(status)return status;
    status=scene_explosion_terrain(s,frame,&contact,campaign_grenade.crater_radius);if(status)return status;
    ++rf_scene_grenades[3];
    if(rf_scene_combat_trace)printf("GRENADE_DETONATE %u %.9g %.9g %.9g\n",frame,event->position[0],event->position[1],event->position[2]);
    return RF_OK;
}
/* Include after explosion helpers and before campaign_combat_tick. Call once per
 * simulation frame after weapon cycling, even off-weapon, and return from legacy
 * magazine/hitscan handling when slot5 is selected. selected must also gate life,
 * ownership and explicit unarmed state. trigger_bits uses RF_GRENADE_THROW_*. */
static int scene_grenades_tick(scene_stream *s,uint32_t frame,const float position[3],
    const float forward[3],uint32_t selected,uint32_t trigger_bits)
{
    rf_grenade_throw_event action;int status;uint32_t i,k;int32_t ammo,*reserve;
    if(!frame){memset(scene_grenades,0,sizeof(scene_grenades));memset(&scene_grenade_throw,0,sizeof(scene_grenade_throw));memset(rf_scene_grenades,0,sizeof(rf_scene_grenades));}
    rf_scene_grenades[4]=0;
    for(i=0;i<SCENE_GRENADE_CAPACITY;i++)if(scene_grenades[i].flight.lifecycle.active){
        rf_grenade_flight_event event;
        status=rf_grenade_flight_step(&scene_grenades[i].flight,1.f/60,scene_gravity.vector,
            .45f,scene_grenade_sweep,s,&event);
        if(status){rf_scene_grenades[7]=(uint32_t)status;return status;}
        if(event.contacts){scene_grenades[i].last_contact=event.contact;scene_grenades[i].has_contact=1;
            rf_scene_grenades[2]+=event.contacts;combat_sound("Grenade Bounce",event.position);}
        rf_scene_grenades[6]+=event.limited;
        if(event.detonate){status=scene_grenade_detonate(s,frame,scene_grenades+i,&event);if(status){rf_scene_grenades[7]=(uint32_t)status;return status;}}
        if(scene_grenades[i].flight.lifecycle.active)++rf_scene_grenades[4];
    }
    if(campaign_grenade_id<0)return RF_OK;
    ammo=campaign_weapon_supply.definitions[campaign_grenade_id].ammo_type;
    if(ammo<0 || ammo>=32)return RF_RANGE;
    reserve=campaign_player_inventory.reserve+ammo;
    status=rf_grenade_throw_step(&scene_grenade_throw,trigger_bits,selected,*reserve,&action);if(status)return status;
    if(action.kind==RF_GRENADE_THROW_START){
        ++rf_scene_combat[0];campaign_last_alt=action.alternate;++rf_scene_grenades[0];
        combat_sound(action.alternate?"grenade twist throw":"grenade pinpull toss",position);
        if(rf_scene_combat_trace)printf("GRENADE_START %u %u %d\n",frame,action.alternate,*reserve);
    }else if(action.kind==RF_GRENADE_THROW_RELEASE){
        scene_grenade_projectile pending={0};uint32_t spawned=0;
        for(i=0;i<SCENE_GRENADE_CAPACITY && scene_grenades[i].flight.lifecycle.active;i++){}
        if(i<SCENE_GRENADE_CAPACITY && *reserve>0){
            /* Launch at the eye; first full sweep prevents spawning through walls. */
            status=rf_grenade_flight_launch(&pending.flight,position,forward,campaign_grenade.speed,
                campaign_grenade.collision_radius,campaign_grenade.lifetime,0,action.alternate?0x10u:0);
            if(status)return status;
            for(k=0;k<3;k++)pending.flight.velocity[k]+=scene_actor_body.state.velocity[k];
            spawned=1;
        }else if(i==SCENE_GRENADE_CAPACITY)++rf_scene_grenades[5];
        status=rf_grenade_throw_resolve(&scene_grenade_throw,spawned,reserve);if(status)return status;
        if(spawned){scene_grenades[i]=pending;++rf_scene_grenades[1];++rf_scene_grenades[4];
            if(rf_scene_combat_trace)printf("GRENADE_RELEASE %u %u %u %d %.9g %.9g %.9g\n",frame,i,action.alternate,*reserve,position[0],position[1],position[2]);}
        campaign_ammo_publish();
    }
    return RF_OK;
}
