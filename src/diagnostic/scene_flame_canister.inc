#ifndef RF_SCENE_FLAME_CANISTER_INC
#define RF_SCENE_FLAME_CANISTER_INC
/* Installed weapons.tbl Flamethrower: speed10, radius.051, lifetime10,
 * alternate release1.8s, cooldown4s, damage100, blast7, visual radius8.
 * 42c362..42c386 replaces the thrown tank with a full reserve tank. First-pass
 * policy for insufficient reserve: partial refill, with no negative inventory.
 * Only successful spawn commits ammo (bounded-pool refusal costs no gas).
 * First-pass policies:
 * explode on contact (shared grenade next-tick contact gate), inherit velocity.
 * Generic rocket-hit visuals are temporary; flame_can_explode is the authored
 * effect and must replace that presentation when its resource owner is loaded.
 * Shared visual radius2 is a temporary port choice: authored8 belongs to the
 * flame-specific recipe and overwhelms the view when applied to rocket smoke.
 * No crater is authored: this adapter does not invent a GeoMod radius.
 * Include after grenade/explosion helpers. Tick every frame, even deselected.
 * Parent must inhibit primary/reload while busy and pass blocked for death,
 * swimming or an already-running reload. Draw uses a separately loaded
 * powerup_flamecan.V3D model handle, NOT the third-person weapon model. */
#define SCENE_FLAME_CANISTER_CAPACITY 8u
static scene_grenade_projectile scene_flame_canisters[SCENE_FLAME_CANISTER_CAPACITY];
static uint32_t scene_flame_canister_held,scene_flame_canister_pending,scene_flame_canister_cooldown;
/* starts, releases, explosions, live, pool misses */
uint32_t rf_scene_flame_canister[5];
static void scene_flame_canister_reset(void)
{
    memset(scene_flame_canisters,0,sizeof(scene_flame_canisters));
    memset(rf_scene_flame_canister,0,sizeof(rf_scene_flame_canister));
    scene_flame_canister_held=scene_flame_canister_pending=scene_flame_canister_cooldown=0;
}
static uint32_t scene_flame_canister_busy(void)
{return !!(scene_flame_canister_pending || scene_flame_canister_cooldown);}
/* Checkpoint admission must not discard a pending throw or flying canister. */
static uint32_t scene_flame_canister_save_pending(void)
{
    uint32_t i;if(scene_flame_canister_pending)return 1;
    for(i=0;i<SCENE_FLAME_CANISTER_CAPACITY;i++)if(scene_flame_canisters[i].flight.lifecycle.active)return 1;
    return 0;
}
static int scene_flame_canister_tick(scene_stream *s,uint32_t frame,const float position[3],
    const float forward[3],int32_t weapon,const rf_weapon_primary_definition *definition,
    uint32_t selected,uint32_t blocked,uint32_t alternate)
{
    uint32_t i,k,edge;int status;int32_t ammo;const rf_weapon_acquire_definition *supply;
    if(!s || !position || !forward || !definition || weapon<0 || weapon>=64 || selected>1 || blocked>1 || alternate>1 ||
       !isfinite(definition->alt_damage) || definition->alt_damage<=0 || !isfinite(definition->alt_fire_seconds) ||
       definition->alt_fire_seconds<=0 || definition->alt_fire_seconds>60)return RF_RANGE;
    supply=campaign_weapon_supply.definitions+weapon;ammo=supply->ammo_type;
    if(ammo<0 || ammo>=32 || supply->magazine<=0 || campaign_player_inventory.reserve[ammo]<0)return RF_RANGE;
    edge=alternate&&!scene_flame_canister_held;scene_flame_canister_held=alternate;
    if(scene_flame_canister_cooldown)--scene_flame_canister_cooldown;
    rf_scene_flame_canister[3]=0;
    for(i=0;i<SCENE_FLAME_CANISTER_CAPACITY;i++)if(scene_flame_canisters[i].flight.lifecycle.active){
        scene_grenade_projectile *p=scene_flame_canisters+i;rf_grenade_flight_event event;
        status=rf_grenade_flight_step(&p->flight,1.f/60,scene_gravity.vector,.3f,scene_grenade_sweep,s,&event);if(status)return status;
        if(event.contacts){p->last_contact=event.contact;p->has_contact=1;}
        if(event.detonate){rf_weapon_flight_contact contact={0};rf_collision_room_location location;
            if(p->has_contact)contact=p->last_contact;else contact.hit.normal[1]=1;
            memcpy(contact.hit.point,event.position,12);contact.hit.fraction=0;contact.object=contact.face=UINT32_MAX;
            status=rf_geometry_collision_world_locate(s->collision,event.position,&location);if(status)return status;contact.room=location.room;
            status=scene_impact_start_sized(s,&contact,frame,2.f);if(status)return status;
            scene_impact_sound(event.position,frame);
            status=scene_explosion_blast(s,frame,event.position,definition->alt_damage,7.f);if(status)return status;
            ++rf_scene_flame_canister[2];
        }
        if(p->flight.lifecycle.active)++rf_scene_flame_canister[3];
    }
    if(!selected || blocked || !campaign_player_inventory.owned[weapon]){scene_flame_canister_pending=0;return RF_OK;}
    if(scene_flame_canister_pending){
        if(--scene_flame_canister_pending)return RF_OK;
        if(campaign_player_inventory.loaded[weapon]<=0)return RF_OK;
        for(i=0;i<SCENE_FLAME_CANISTER_CAPACITY && scene_flame_canisters[i].flight.lifecycle.active;i++){}
        if(i==SCENE_FLAME_CANISTER_CAPACITY){++rf_scene_flame_canister[4];return RF_OK;}
        {scene_grenade_projectile pending={0};
         status=rf_grenade_flight_launch(&pending.flight,position,forward,10.f,.051f,10.f,0,0x10u);if(status)return status;
         for(k=0;k<3;k++)pending.flight.velocity[k]+=scene_actor_body.state.velocity[k];
         scene_flame_canisters[i]=pending;}
        {int32_t refill=campaign_player_inventory.reserve[ammo];
         if(refill>supply->magazine)refill=supply->magazine;
         campaign_player_inventory.loaded[weapon]=refill;campaign_player_inventory.reserve[ammo]-=refill;}
        scene_flame_gas_remainder=0;campaign_ammo_publish();
        ++rf_scene_flame_canister[1];++rf_scene_flame_canister[3];
        if(rf_scene_combat_trace)printf("FLAME_CANISTER_RELEASE %u %u\n",frame,i);
    }else if(edge && !scene_flame_canister_cooldown && campaign_player_inventory.loaded[weapon]>0){
        scene_flame_canister_pending=108;scene_flame_canister_cooldown=(uint32_t)ceilf(definition->alt_fire_seconds*60);
        ++rf_scene_flame_canister[0];++rf_scene_combat[0];campaign_last_alt=1;
        combat_sound("Flame Alt Fire",position);
    }
    return RF_OK;
}
#endif
