#ifndef RF_SCENE_TURRET_HEAP_INCLUDED
#define RF_SCENE_TURRET_HEAP_INCLUDED
/* Include after scene_apc_primary_runtime.inc and shared explosion helpers.
 * HEAP has no gravity/model/homing declaration. Flights retain launch-time
 * identities after dismount/death; ownership is not consulted again at impact.
 * Shared impact appearance is provisional. Ordinary saves must reject pending
 * flights until a projectile companion serializes this bounded owner. */
enum {SCENE_TURRET_HEAP_CAPACITY=8};
typedef struct scene_turret_heap_round {
    rf_weapon_flight flight;
    uint32_t host,source,operator_handle;
    int32_t weapon;
    float damage,blast,crater,impact;
} scene_turret_heap_round;
static scene_turret_heap_round scene_turret_heap_rounds[SCENE_TURRET_HEAP_CAPACITY];
static uint32_t scene_turret_heap_frame=UINT32_MAX;
/* launches,contacts,expired,live,errors,blasts,last damage source,pool-full,
 * direct damaging contacts,last frame,last status,reserved */
uint32_t rf_scene_turret_heap[12];
/* Immutable first physical contact: valid,frame,object,face,host,source,
 * excluded operator,point xyz (float bits),normal y (float bits),kind. */
uint32_t rf_scene_turret_heap_contact[12];
/* Immutable first positive direct damage: valid,frame,object,face,target,
 * source,applied(float bits),requested(float bits). Blast telemetry remains
 * in the existing shared explosion service; this is not a total damage sum. */
uint32_t rf_scene_turret_heap_damage[8];
static void scene_turret_heap_reset(void)
{
    scene_turret_heap_frame=UINT32_MAX;
    memset(scene_turret_heap_rounds,0,sizeof(scene_turret_heap_rounds));
    memset(rf_scene_turret_heap,0,sizeof(rf_scene_turret_heap));
    memset(rf_scene_turret_heap_contact,0,sizeof(rf_scene_turret_heap_contact));
    memset(rf_scene_turret_heap_damage,0,sizeof(rf_scene_turret_heap_damage));
}
static uint32_t scene_turret_heap_pending(void)
{
    for(uint32_t i=0;i<SCENE_TURRET_HEAP_CAPACITY;i++)
        if(scene_turret_heap_rounds[i].flight.active)return 1;
    return 0;
}
static int scene_turret_heap_error(int status)
{++rf_scene_turret_heap[4];rf_scene_turret_heap[10]=(uint32_t)status;return status;}
static int scene_turret_heap_launch(scene_stream *stream,uint32_t sourcehost,
    uint32_t damage_source,uint32_t excluded_operator,const float *eye,const float *ray,
    const rf_weapon_primary_definition *primary,const rf_weapon_explosive_definition *explosive,
    uint32_t *accepted)
{
    scene_turret_heap_round shot={0};rf_random_state random=campaign_enemy_spread_random;
    float spread[3],degrees;uint32_t i;int status;
    if(!accepted)return scene_turret_heap_error(RF_RANGE);*accepted=0;
    if(!stream||!stream->collision||!eye||!ray||!primary||!explosive||
       !scene_turret_lookup(sourcehost)||damage_source==UINT32_MAX||
       primary->damage_kind!=3||primary->projectiles!=1)return scene_turret_heap_error(RF_RANGE);
    shot.weapon=rf_weapon_name_find(&campaign_weapon_supply.names,"HEAP");
    if(shot.weapon<0)return scene_turret_heap_error(RF_NOT_FOUND);
    shot.damage=damage_source==campaign_player_object.handle?primary->damage:combat_enemy_primary_damage(primary);
    degrees=damage_source==campaign_player_object.handle?primary->spread_degrees:primary->ai_spread_degrees;
    shot.blast=explosive->damage_radius;shot.crater=explosive->crater_radius;
    shot.impact=explosive->impact_count?explosive->impact_radius[0]:2.f;
    if(!isfinite(shot.damage)||shot.damage<0||!isfinite(shot.blast)||shot.blast<0||
       !isfinite(shot.crater)||shot.crater<0||!isfinite(shot.impact)||shot.impact<0)
        return scene_turret_heap_error(RF_FORMAT);
    for(i=0;i<SCENE_TURRET_HEAP_CAPACITY&&scene_turret_heap_rounds[i].flight.active;i++){}
    if(i==SCENE_TURRET_HEAP_CAPACITY){++rf_scene_turret_heap[7];return RF_OK;}
    status=rf_weapon_spread_ray(ray,degrees,&random,spread);if(status)return scene_turret_heap_error(status);
    status=rf_weapon_flight_launch(&shot.flight,eye,spread,explosive->speed,
        explosive->lifetime,explosive->collision_radius);if(status)return scene_turret_heap_error(status);
    shot.host=sourcehost;shot.source=damage_source;shot.operator_handle=excluded_operator;
    scene_turret_heap_rounds[i]=shot;campaign_enemy_spread_random=random;
    ++rf_scene_turret_heap[0];++rf_scene_turret_heap[3];rf_scene_turret_heap[6]=damage_source;
    combat_sound("Heap Launch",eye);*accepted=1;return RF_OK;
}
static void scene_turret_heap_direct_probe(const scene_turret_heap_round *shot,
    const rf_weapon_flight_contact *contact,uint32_t frame,uint32_t target,float applied)
{
    uint32_t *p=rf_scene_turret_heap_damage;
    if(applied<=0)return;++rf_scene_turret_heap[8];
    if(p[0])return;p[0]=1;p[1]=frame;p[2]=contact->object;p[3]=contact->face;
    p[4]=target;p[5]=shot->source;memcpy(p+6,&applied,4);memcpy(p+7,&shot->damage,4);
}
static int scene_turret_heap_direct(scene_stream *stream,uint32_t frame,
    const scene_turret_heap_round *shot,const rf_weapon_flight_contact *contact,uint32_t *consumed)
{
    uint32_t tag=contact->object,handled=0,index,target,entered,bits;
    float applied=0,seconds=(float)frame/60;int status;
    rf_damage_request request={shot->damage,shot->source,3,0,UINT32_MAX,0};
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60%RF_TIMER_PERIOD),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    *consumed=0;
    status=scene_driller_projectile_damage(contact,shot->source,shot->damage,3,frame,&handled,&applied);
    if(status)return status;
    if(handled){scene_turret_heap_direct_probe(shot,contact,frame,contact->face,applied);return RF_OK;}
    status=scene_clutter_projectile_damage(contact,shot->damage,3,&handled);if(status||handled)return status;
    if(tag!=UINT32_MAX&&(tag&SCENE_DETACHED_ROCKET_OWNER))
        return scene_detached_sources_damage(stream,(tag>>16)&0x7fff,tag&0xffff,shot->damage);
    if(tag!=SCENE_AI_PROJECTILE_PLAYER&&(tag&0xffff0000u)!=SCENE_ACTOR_ROCKET_OWNER)return RF_OK;
    memcpy(&bits,&seconds,4);
    if(tag==SCENE_AI_PROJECTILE_PLAYER){
        target=campaign_player_object.handle;
        if(target==shot->host||target==shot->operator_handle)return RF_OK;
        status=rf_scene_player_damage(target,&request,1,bits,&effects,&applied);
    }else{
        index=tag&0xffffu;if(index>=campaign_npc_body_count)return RF_RANGE;
        target=campaign_npc_bodies[index].registration.handle;
        if(target==shot->host||target==shot->operator_handle)return RF_OK;
        status=scene_nano_weapon_contact(campaign_npc_bodies+index,shot->weapon,shot->damage,consumed);
        if(status||*consumed)return status;
        status=rf_scene_npc_damage(target,&request,1,bits,&effects,&applied);
        if(!status&&campaign_npc_bodies[index].damage.effects.health<=0){
            status=rf_scene_npc_death_entry(target,&entered);
            if(!status&&entered)status=combat_death_start(index);
        }
    }
    if(status)return status;if(feedback.status)return feedback.status;
    scene_turret_heap_direct_probe(shot,contact,frame,target,applied);return RF_OK;
}
static int scene_turret_heap_tick(scene_stream *stream,uint32_t frame)
{
    float dt=frame?scene_step_seconds:0;uint32_t i,k;int status;
    if(!stream||!stream->collision||!isfinite(dt)||dt<0||dt>.05f)return scene_turret_heap_error(RF_RANGE);
    /* View preparation may repeat; vehicle occupancy must not freeze shells. */
    if(scene_turret_heap_frame==frame)return RF_OK;
    scene_turret_heap_frame=frame;
    rf_scene_turret_heap[3]=0;rf_scene_turret_heap[9]=frame;
    for(i=0;i<SCENE_TURRET_HEAP_CAPACITY;i++){
        scene_turret_heap_round *shot=scene_turret_heap_rounds+i;
        rf_weapon_flight_event event;scene_apc_primary_query query={stream,shot->host,shot->operator_handle};
        if(!shot->flight.active)continue;
        status=rf_weapon_flight_step(&shot->flight,dt,scene_apc_primary_sweep,&query,&event);
        if(status)return scene_turret_heap_error(status);
        if(event.kind==2)++rf_scene_turret_heap[2];
        if(event.kind==1){
            const rf_weapon_flight_contact *contact=&event.contact;
            uint32_t consumed,*p=rf_scene_turret_heap_contact;float origin[3];
            ++rf_scene_turret_heap[1];
            if(!p[0]){p[0]=1;p[1]=frame;p[2]=contact->object;p[3]=contact->face;
                p[4]=shot->host;p[5]=shot->source;p[6]=shot->operator_handle;
                memcpy(p+7,contact->hit.point,12);memcpy(p+10,contact->hit.normal+1,4);p[11]=3;}
            status=scene_turret_heap_direct(stream,frame,shot,contact,&consumed);
            if(status)return scene_turret_heap_error(status);if(consumed)continue;
            status=scene_impact_start_sized(stream,contact,frame,shot->impact);if(status)return scene_turret_heap_error(status);
            scene_impact_sound(contact->hit.point,frame);
            for(k=0;k<3;k++)origin[k]=contact->hit.point[k]+contact->hit.normal[k]*.01f;
            status=scene_explosion_blast_source(stream,frame,origin,shot->damage,shot->blast,shot->source,3);
            if(status)return scene_turret_heap_error(status);++rf_scene_turret_heap[5];
            status=scene_explosion_terrain(stream,frame,contact,shot->crater);if(status)return scene_turret_heap_error(status);
        }
        if(shot->flight.active)++rf_scene_turret_heap[3];
    }
    return RF_OK;
}
#endif
