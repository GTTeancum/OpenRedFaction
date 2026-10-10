#ifndef RF_SCENE_AI_LASER_INCLUDED
#define RF_SCENE_AI_LASER_INCLUDED
/* Include after scene_ai_rocket.inc. Dedicated direct-damage flights: no
 * rocket explosion, radial damage, GeoMod, ricochet or actor penetration.
 * Current-pose physical shields use the existing directional CENTERLINE
 * policy; world, props, vehicles and body spheres retain authored radius .2.
 * Parent prepares the selected player shield pose before calling tick. */
#define SCENE_AI_LASER_CAPACITY 64u
static scene_ai_laser_projectile scene_ai_lasers[SCENE_AI_LASER_CAPACITY];
static uint64_t scene_ai_laser_epoch;
static uint32_t scene_ai_laser_updating;
/* launches,contacts,expiry,live,pool refusals,actor contacts,shield contacts,
 * Nano contacts,last source,last target,last frame,last status */
uint32_t rf_scene_ai_laser[12];
static void scene_ai_laser_reset(void)
{
    memset(scene_ai_lasers,0,sizeof(scene_ai_lasers));++scene_ai_laser_epoch;
    rf_scene_ai_laser[3]=0; /* Never reopen a reentrant update after reset. */
}
static uint32_t scene_ai_laser_pending(void)
{
    uint32_t i;if(scene_ai_laser_updating)return 1;
    for(i=0;i<SCENE_AI_LASER_CAPACITY;i++)if(scene_ai_lasers[i].flight.active)return 1;
    return 0;
}
/* Callback-free preparation, without pool mutation, ammunition mutation or
 * RNG publication. The exact caller owns source/weapon/resource admission.
 * Laser/Sonar call immediately; TriBeam only at its prepaid launch deadline. */
static int scene_ai_laser_prepare(campaign_npc_body *owner,const float target[3],
    const scene_ai_laser_profile *profile,rf_random_state *random,
    scene_ai_laser_projectile *out)
{
    scene_ai_laser_projectile next={0};
    float ray[3],direction[3],up[3]={0,1,0};double length;uint32_t k;int status;
    if(!owner || !target || !profile || !random || !out)return RF_RANGE;
    for(k=0;k<3;k++)ray[k]=target[k]-owner->eye_position[k];
    status=rf_weapon_spread_ray(ray,profile->primary.ai_spread_degrees,random,direction);
    if(status)return status;
    status=rf_weapon_flight_launch(&next.flight,owner->eye_position,direction,
        profile->speed,profile->lifetime,profile->radius);if(status)return status;
    for(k=0;k<3;k++)direction[k]=next.flight.velocity[k]/profile->speed;
    if(fabsf(direction[1])>.99f){up[0]=1;up[1]=0;}
    next.basis[0]=up[1]*direction[2]-up[2]*direction[1];
    next.basis[1]=up[2]*direction[0]-up[0]*direction[2];
    next.basis[2]=up[0]*direction[1]-up[1]*direction[0];
    length=sqrt((double)next.basis[0]*next.basis[0]+(double)next.basis[1]*next.basis[1]+(double)next.basis[2]*next.basis[2]);
    if(!isfinite(length) || length<=0)return RF_RANGE;
    for(k=0;k<3;k++){next.basis[k]=(float)(next.basis[k]/length);next.basis[6+k]=direction[k];}
    next.basis[3]=direction[1]*next.basis[2]-direction[2]*next.basis[1];
    next.basis[4]=direction[2]*next.basis[0]-direction[0]*next.basis[2];
    next.basis[5]=direction[0]*next.basis[1]-direction[1]*next.basis[0];
    next.source=owner->registration.handle;next.weapon=profile->weapon;
    next.damage=combat_enemy_primary_damage(&profile->primary);
    next.damage_kind=profile->primary.damage_kind;
    next.liquid_weapon_flags=profile->liquid_weapon_flags;
    next.liquid.query_flags=0x1004u;*out=next;return RF_OK;
}
static int scene_ai_laser_launch(campaign_npc_body *owner,const float target[3])
{
    scene_ai_laser_projectile next={0};rf_random_state random=campaign_enemy_spread_random;
    uint32_t i,source_slot;int32_t weapon;const rf_weapon_acquire_definition *ammo;int status;
    if(!owner || !target)return RF_RANGE;
    weapon=owner->view.weapons[0];if(!campaign_enemy_laser_weapon(weapon))return RF_NOT_FOUND;
    for(source_slot=0;source_slot<campaign_npc_body_count;source_slot++)if(campaign_npc_bodies+source_slot==owner)break;
    if(source_slot==campaign_npc_body_count ||
       !scene_ai_melee_npc_live(source_slot,owner->registration.handle))return RF_NOT_FOUND;
    ammo=campaign_weapon_supply.definitions+weapon;
    if(!owner->inventory.owned[weapon] || ammo->ammo_type<0 || ammo->ammo_type>=32 ||
       ammo->magazine || ammo->capacity!=200 || owner->inventory.loaded[weapon] ||
       owner->inventory.reserve[ammo->ammo_type]<=0)return RF_NOT_FOUND;
    for(i=0;i<SCENE_AI_LASER_CAPACITY && scene_ai_lasers[i].flight.active;i++){}
    if(i==SCENE_AI_LASER_CAPACITY){++rf_scene_ai_laser[4];return RF_NOT_FOUND;}
    status=scene_ai_laser_prepare(owner,target,&scene_ai_laser_definition,&random,&next);if(status)return status;
    /* Laser retains its exact immediate debit/publication contract. */
    status=rf_weapon_consume_shot(&owner->inventory,campaign_weapon_supply.definitions,
        campaign_weapon_supply.names.count,weapon);if(status)return status;
    campaign_enemy_spread_random=random;scene_ai_lasers[i]=next;
    ++rf_scene_ai_laser[0];++rf_scene_ai_laser[3];rf_scene_ai_laser[8]=next.source;
    rf_scene_ai_laser[10]=combat_frame;return RF_OK;
}

enum {SCENE_LASER_WORLD=0,SCENE_LASER_PIECE=1,SCENE_LASER_VEHICLE=2,
      SCENE_LASER_CLUTTER=3,SCENE_LASER_BODY=4,SCENE_LASER_NPC_SHIELD=5,SCENE_LASER_PLAYER_SHIELD=6};
typedef struct scene_ai_laser_query {
    scene_stream *stream;uint32_t source,kind,target,target_slot,no_world_collide;
    const float *player_eye;float start[3],end[3];
    scene_ai_shield_ray_selection npc_shield;
    scene_player_shield_candidate player_shield;
} scene_ai_laser_query;
static void scene_ai_laser_shield_hit(const rf_collision_model_response_hit *in,rf_collision_ray_hit *out)
{out->fraction=in->time;memcpy(out->point,in->point,12);memcpy(out->normal,in->normal,12);}
static int scene_ai_laser_sweep(void *context,const float *start,const float *delta,float radius,uint32_t flags,
    rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    scene_ai_laser_query *q=context;scene_stream *s=q->stream;
    rf_geometry_world_sweep_hit world;rf_geomod_registry_hit piece;
    uint32_t i,k,found;int status;
    q->kind=SCENE_LASER_WORLD;q->target=q->target_slot=UINT32_MAX;
    memset(&q->npc_shield,0,sizeof(q->npc_shield));memset(&q->player_shield,0,sizeof(q->player_shield));
    for(k=0;k<3;k++){q->start[k]=start[k];q->end[k]=start[k]+delta[k];}
    /* Same environment composers/order as shared NPC projectiles, separated
     * here so a coarse body cannot truncate an independently visible shield. */
    /* Cane flags26810 clears physics10:49bb70 skips static world/liquid
     * and type9 movers. Independent physics20 object pairs still include
     * eligible kind3 detached solids below. All prior callers remain zero. */
    *matched=0;*liquid=0;
    if(!q->no_world_collide){
        status=rf_geometry_collision_world_sweep_liquid(s->collision,flags,start,delta,radius,1,&world,matched,liquid);if(status)return status;
        if(*matched){out->hit=world.hit;out->room=world.room;out->face=world.face;out->object=UINT32_MAX;}
        for(i=0;i<campaign_movers.count;i++){
            const rf_collision_solid_view *solid=campaign_movers.views+i;rf_collision_sweep_room_hit local;rf_collision_ray_hit hit;
            status=rf_collision_solid_preferred(solid,NULL,flags&~0x1004u,start,delta,radius,*matched?out->hit.fraction:1,&local,&found);if(status)return status;
            if(!found || (*matched && local.tree.hit.fraction>=out->hit.fraction))continue;
            status=rf_collision_contact_world(&local.tree.hit,solid->output_origin,solid->output_matrix,&hit);if(status)return status;
            out->hit=hit;out->room=UINT32_MAX;out->face=local.tree.face_index;out->object=SCENE_MOVER_ROCKET_OWNER|i;*matched=1;*liquid=0;
        }
    }
    status=scene_detached_sources_sweep(s,flags,start,delta,radius,*matched?out->hit.fraction:1,&piece,&found);if(status)return status;
    if(found && (!*matched || piece.piece.hit.fraction<out->hit.fraction)){
        out->hit=piece.piece.hit;out->room=UINT32_MAX;out->face=piece.piece.face;
        out->object=SCENE_DETACHED_ROCKET_OWNER|(piece.batch<<16)|piece.piece.piece;
        q->kind=SCENE_LASER_PIECE;*matched=1;*liquid=0;
    }
    /* Compare complete per-actor body/shield choices against retained cover.
     * NPC-first/player-last ties match the existing NPC projectile composer. */
    for(i=0;i<=campaign_npc_body_count;i++){
        rf_collision_ray_hit hit={0};float fraction=1;uint32_t body_hit=0,actor_hit=0,kind=SCENE_LASER_BODY,handle,room;
        scene_ai_shield_ray_selection shield={0};scene_player_shield_candidate player={0};uint32_t shield_hit=0;
        if(i==campaign_npc_body_count){
            handle=campaign_player_object.handle;
            if(!scene_ai_actor_penetration_player_live(handle) || scene_ai_actor_ray_related(q->source,handle))continue;
            body_hit=scene_rocket_body_hit(&scene_actor_body,start,delta,radius,1,&hit);
            status=scene_player_shield_query(s,q->player_eye,actor_look.eye_orientation,start,q->end,1,&player,&shield_hit);
            if(status)return status;actor_hit=body_hit;fraction=body_hit?hit.fraction:1;
            if(shield_hit){scene_ai_laser_shield_hit(&player.hit,&hit);fraction=hit.fraction;actor_hit=1;kind=SCENE_LASER_PLAYER_SHIELD;}
            room=rf_scene_player_swim[0];
        }else{
            campaign_npc_body *npc=campaign_npc_bodies+i;handle=npc->registration.handle;
            if(!scene_ai_melee_npc_live(i,handle) || scene_ai_actor_ray_related(q->source,handle))continue;
            body_hit=scene_rocket_body_hit(&npc->body,start,delta,radius,1,&hit);fraction=body_hit?hit.fraction:1;
            if(scene_npc_shields.owners && scene_npc_shields.weapon>=0 && scene_npc_shields.weapon<64 &&
               npc->view.weapons[0]==scene_npc_shields.weapon && npc->inventory.owned[scene_npc_shields.weapon]){
                rf_entity_pose *pose=NULL;status=campaign_model_pose(i,&pose);if(status)return status;if(!pose)return RF_NOT_FOUND;
                status=campaign_model_pose_ready(i,pose);if(status)return status;
            }
            status=scene_ai_shield_ray_select(i,start,q->end,body_hit,fraction,&shield);if(status)return status;
            actor_hit=shield.kind!=SCENE_AI_SHIELD_RAY_MISS;fraction=shield.fraction;
            if(shield.kind==SCENE_AI_SHIELD_RAY_SHIELD){scene_ai_laser_shield_hit(&shield.shield.hit,&hit);kind=SCENE_LASER_NPC_SHIELD;}
            room=npc->room.room?npc->room.room-1:UINT32_MAX;
        }
        if(!actor_hit || (*matched && fraction>=out->hit.fraction))continue;
        out->hit=hit;out->room=room;out->face=handle;
        out->object=i==campaign_npc_body_count?SCENE_AI_PROJECTILE_PLAYER:SCENE_ACTOR_ROCKET_OWNER|i;
        q->kind=kind;q->target=handle;q->target_slot=i==campaign_npc_body_count?UINT32_MAX:i;
        q->npc_shield=shield;q->player_shield=player;*matched=1;*liquid=0;
    }
    {rf_weapon_flight_contact vehicle;uint32_t hit;
     status=scene_driller_projectile_query(q->source,start,delta,radius,*matched?out->hit.fraction:1,&vehicle,&hit);if(status)return status;
     if(hit){*out=vehicle;q->kind=SCENE_LASER_VEHICLE;q->target=q->target_slot=UINT32_MAX;*matched=1;*liquid=0;}}
    {float limit=*matched?out->hit.fraction:1;
     status=scene_clutter_projectile_compose(q->source,start,delta,radius,out,liquid,matched);if(status)return status;
     if(*matched && out->hit.fraction<limit && (out->object&0xffff0000u)==SCENE_CLUTTER_PROJECTILE_OWNER){
         q->kind=SCENE_LASER_CLUTTER;q->target=q->target_slot=UINT32_MAX;
     }}
    return RF_OK;
}
/* A terminal consumer may need the shield/Nano disposition. It must call
 * this exactly once: a Nano prepass would debit twice or blast on its break.
 * Selected physical shields consume even when stale or broken. */
static int scene_ai_laser_contact_result(scene_stream *stream,uint32_t frame,const scene_ai_laser_projectile *shot,
    const scene_ai_laser_query *q,const rf_weapon_flight_contact *contact,
    uint32_t stats[12],const uint64_t *pool_epoch,uint32_t *consumed)
{
    rf_damage_request request={shot->damage,shot->source,shot->damage_kind,0,UINT32_MAX,0};
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60%RF_TIMER_PERIOD),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,combat_random,combat_notify,combat_playing,combat_play,&feedback};
    uint32_t accepted=0,broken=0,entered=0,clock;float seconds=(float)frame/60,amount=0;int status;
    uint64_t epoch=*pool_epoch;(void)stream;memcpy(&clock,&seconds,4);
    if(consumed)*consumed=q->kind==SCENE_LASER_NPC_SHIELD || q->kind==SCENE_LASER_PLAYER_SHIELD;
    /*4c5a35..4c5a51 returns before direct/radial for an unresolved stored
     * object. Consuming the finished sweep remains shared port policy.
     * Liveness remains the direct consumer's policy below; death alone is
     * not a new identity and does not exclude an independent radial pulse. */
    if(consumed && (q->kind==SCENE_LASER_BODY || q->kind==SCENE_LASER_NPC_SHIELD || q->kind==SCENE_LASER_PLAYER_SHIELD)){
        if(q->target_slot==UINT32_MAX?!scene_ai_melee_player_identity(q->target):
           !scene_ai_melee_npc_identity(q->target_slot,q->target)){
            if(consumed)*consumed=1;return RF_OK;
        }
    }
    if(q->kind==SCENE_LASER_PIECE)
        return scene_detached_sources_damage(stream,(contact->object>>16)&0x7fff,contact->object&0xffff,shot->damage);
    if(q->kind==SCENE_LASER_CLUTTER)
        return scene_clutter_projectile_damage(contact,shot->damage,shot->damage_kind,&accepted);
    if(q->kind==SCENE_LASER_VEHICLE)
        return scene_driller_projectile_damage(contact,shot->source,shot->damage,shot->damage_kind,frame,&accepted,&amount);
    if(q->kind==SCENE_LASER_WORLD)return RF_OK;
    stats[9]=q->target;
    if(q->target_slot==UINT32_MAX){
        if(!scene_ai_actor_penetration_player_live(q->target))return RF_OK;
        if(q->kind==SCENE_LASER_PLAYER_SHIELD){
            status=scene_player_shield_commit(&q->player_shield,&request,&accepted,&broken);
            if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
            stats[6]+=accepted;return RF_OK; /* Selected stale/broken shield consumes. */
        }
        ++stats[5];status=rf_scene_player_damage(q->target,&request,1,clock,&effects,&amount);
        return status?status:feedback.status;
    }
    {campaign_npc_body *npc=scene_ai_melee_npc_live(q->target_slot,q->target);
     if(!npc)return RF_OK;
     if(q->kind==SCENE_LASER_NPC_SHIELD){
        status=scene_ai_shield_ray_commit(&q->npc_shield,q->start,q->end,&request,&accepted,&broken);
        if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
        stats[6]+=accepted;return RF_OK;
     }
     status=scene_nano_weapon_contact(npc,shot->weapon,shot->damage,&accepted);if(status)return status;
     if(accepted){if(consumed)*consumed=1;++stats[7];return RF_OK;}
     if(!scene_ai_melee_npc_live(q->target_slot,q->target))return RF_OK;
     ++stats[5];status=rf_scene_npc_damage(q->target,&request,1,clock,&effects,&amount);
     if(!status)status=feedback.status;if(status)return status;if(epoch!=*pool_epoch)return RF_OK;
     npc=scene_ai_melee_npc_live(q->target_slot,q->target);
     /* Death removes 'live' eligibility, so rebind identity for death entry. */
     if(!npc)npc=scene_ai_melee_npc_identity(q->target_slot,q->target);
     if(npc && npc->damage.effects.health<=0){
        status=rf_scene_npc_death_entry(q->target,&entered);if(status)return status;
        if(epoch!=*pool_epoch)return RF_OK;
        if(entered && scene_ai_melee_npc_identity(q->target_slot,q->target))return combat_death_start(q->target_slot);
     }}
    return RF_OK;
}
static int scene_ai_laser_contact(scene_stream *stream,uint32_t frame,const scene_ai_laser_projectile *shot,
    const scene_ai_laser_query *q,const rf_weapon_flight_contact *contact,
    uint32_t stats[12],const uint64_t *pool_epoch)
{return scene_ai_laser_contact_result(stream,frame,shot,q,contact,stats,pool_epoch,NULL);}
typedef int (*scene_ai_energy_contact)(scene_stream *,uint32_t,const scene_ai_laser_projectile *,
    const scene_ai_laser_query *,const rf_weapon_flight_contact *,uint32_t [12],const uint64_t *);
/* Only terminal dispatch is variable; sweep ordering, terminal publication,
 * pool guards and liquid/expiry semantics stay in this one implementation. */
static int scene_ai_energy_tick_contact(scene_stream *stream,uint32_t frame,const float player_eye[3],
    scene_ai_laser_projectile *shots,uint32_t capacity,uint32_t stats[12],
    uint64_t *pool_epoch,uint32_t *updating,scene_ai_energy_contact contact)
{
    uint32_t i,live=0;uint64_t epoch=*pool_epoch;int status=RF_OK;
    if(!stream || !player_eye || !contact)return RF_RANGE;if(*updating)return RF_OK;
    *updating=1;
    for(i=0;i<capacity;i++){
        scene_ai_laser_projectile shot=shots[i];rf_weapon_flight_liquid_event move;
        rf_weapon_flight_liquid_policy policy={shot.liquid_weapon_flags,0,-1,-1};scene_ai_laser_query q={0};
        if(!shot.flight.active)continue;q.stream=stream;q.source=shot.source;q.player_eye=player_eye;
        status=rf_weapon_flight_step_liquid(&shot.flight,1.f/60,&shot.liquid,&policy,scene_ai_laser_sweep,&q,&move);
        if(status)break;if(epoch!=*pool_epoch)break;
        shots[i]=shot; /* Terminal publication precedes every callback. */
        if(move.has_liquid){combat_sound("Medium Water Splash",move.liquid_contact.hit.point);
            if(epoch!=*pool_epoch)break;
            scene_ripple_start(stream,move.liquid_contact.hit.point,frame,0);}
        if(epoch!=*pool_epoch)break;
        if(move.terminal.kind==2)++stats[2];
        if(move.terminal.kind==1){++stats[1];
            status=contact(stream,frame,&shot,&q,&move.terminal.contact,stats,pool_epoch);
            if(status || epoch!=*pool_epoch)break;
        }
    }
    for(i=0;i<capacity;i++)live+=shots[i].flight.active!=0;
    stats[3]=live;stats[11]=(uint32_t)status;*updating=0;return status;
}
/* Existing Laser/TriBeam/Sonar calls retain their direct-only terminal. */
static int scene_ai_energy_tick(scene_stream *stream,uint32_t frame,const float player_eye[3],
    scene_ai_laser_projectile *shots,uint32_t capacity,uint32_t stats[12],
    uint64_t *pool_epoch,uint32_t *updating)
{return scene_ai_energy_tick_contact(stream,frame,player_eye,shots,capacity,stats,pool_epoch,updating,scene_ai_laser_contact);}
static int scene_ai_laser_tick(scene_stream *stream,uint32_t frame,const float player_eye[3])
{
    return scene_ai_energy_tick(stream,frame,player_eye,scene_ai_lasers,SCENE_AI_LASER_CAPACITY,
        rf_scene_ai_laser,&scene_ai_laser_epoch,&scene_ai_laser_updating);
}
#endif
