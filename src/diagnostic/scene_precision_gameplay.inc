/* Include immediately before combat_tick, after retained damage/death helpers.
 * Port first-pass precision shots. Sniper consumes authored object power;
 * world-metal exit/ricochet traversal remains separate. Rail is unchanged.
 * Call only for an accepted PRIMARY shot, after one ammo consumption. */
#include "rf/weapon_precision.h"
#include "scene_sniper_penetration.inc"
typedef struct scene_precision_contact {
    rf_hitscan_selection actor;scene_npc_shield_candidate shield;uint32_t shield_hit;
    float query_fraction; /* Local advanced-ray fraction; shield token stays intact. */
} scene_precision_contact;
/* Stable near-to-far ordering carries the original shield query token intact. */
static void scene_precision_insert(scene_precision_contact *hits,uint32_t *count,
    uint32_t capacity,const scene_precision_contact *hit)
{
    uint32_t pos,j;
    for(pos=0;pos<*count && hits[pos].actor.fraction<=hit->actor.fraction;pos++){}
    if(pos==capacity)return;
    if(*count<capacity)++*count;
    for(j=*count-1;j>pos;j--)hits[j]=hits[j-1];hits[pos]=*hit;
}
/* A flying precision round (RF.exe 4c77a0 weapon object): sniper and rail
 * rounds fly at $Velocity for $Lifetime. The accepted shot's damage, kind and
 * identities are captured at fire; sniper power and visited handles live on
 * the round (projectile+2f0); rail rounds remember the bodies already hit. */
typedef struct scene_precision_round {
    scene_sniper_penetration sniper;
    float origin[3],path[3],speed,flown_fraction,base;
    int32_t kind,weapon,nano_weapon;uint32_t rail,rail_vehicle_done,active;
} scene_precision_round;
#define SCENE_PRECISION_ROUND_CAPACITY 8u
static scene_precision_round scene_precision_rounds[SCENE_PRECISION_ROUND_CAPACITY];
/* launched, completed, instant fallbacks, active */
uint32_t rf_scene_precision_rounds[4];
/* One step of a precision round. Sniper: the whole path from the origin, with
 * contacts taken in order up to `limit` (fraction of the path reached) and
 * actor queries starting at `flown`; a farther contact waits (*done=0).
 * Rail: everything on this step's stretch of the path is hit, once per body;
 * it pierces walls and flies to the end of its range. */
static int scene_precision_step(scene_stream *stream,uint32_t frame,scene_precision_round *r,
    float flown,float limit,uint32_t *done)
{
    float delta[3],end[3],origin[3];const float *position=origin;uint32_t i,rail=r->rail;int status;
    float sniper_base=r->base;int32_t sniper_kind=r->kind;
    int32_t weapon=r->weapon; /* Retain the accepted shot's identity across damage callbacks. */
    *done=1;
    if(rail)for(i=0;i<3;++i){origin[i]=r->origin[i]+r->path[i]*flown;delta[i]=r->path[i]*(limit-flown);end[i]=origin[i]+delta[i];}
    else {
        for(i=0;i<3;++i){origin[i]=r->origin[i];delta[i]=r->path[i];end[i]=origin[i]+delta[i];}
        if(r->sniper.cursor<flown) {
            r->sniper.cursor=flown;
            for(i=0;i<3;++i){r->sniper.start[i]=origin[i]+delta[i]*flown;r->sniper.delta[i]=end[i]-r->sniper.start[i];}
        }
    }
    for(;;){
    scene_precision_contact hits[RF_WEAPON_PRECISION_HITS];uint32_t count=0,continue_sniper=0;
    scene_clutter_precision_contact prop={UINT32_MAX,UINT32_MAX,1};
    rf_weapon_flight_contact vehicle_contact;uint32_t vehicle_selected=0;
    float nearest=1,sniper_damage=sniper_base;
    const float *actor_position=rail?position:r->sniper.start,*actor_delta=rail?delta:r->sniper.delta;
    if(!rail){status=scene_sniper_penetration_damage(&r->sniper,sniper_base,&sniper_damage);if(status)return status;}
    /* One borrowed candidate at a time avoids an actor-count-sized allocation.
     * Retain globally nearest hits even if a far actor was enumerated first. */
    for(i=0;i<campaign_npc_body_count;++i){
        campaign_npc_body *owner=campaign_npc_bodies+i;
        rf_hitscan_candidate candidate;rf_weapon_precision_result selected;
        scene_precision_contact hit={0};uint32_t accepted=0,capacity=rail?RF_WEAPON_PRECISION_HITS:1;
        if(!owner->registration.view || (owner->object_flags&(2|0x4000)) ||
           owner->damage.effects.health<=0 ||
           (owner->view.flags_810&1))continue;
        if(rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration)continue;
        if(scene_sniper_penetration_visited(&r->sniper,owner->registration.handle))continue;
        hit.actor=(rf_hitscan_selection){0,i,owner->registration.handle,1};
        if(owner->body.allocated_bytes) {
            candidate=(rf_hitscan_candidate){owner->registration.handle,1,&owner->registration,&owner->body,NULL};
            status=rf_weapon_precision_select(&campaign_registry,&candidate,1,actor_position,actor_delta,1,
                rail?RF_WEAPON_PRECISION_RAIL:RF_WEAPON_PRECISION_SNIPER,NULL,NULL,&selected);
            if(status)return status;
            if(selected.count){hit.actor=selected.hits[0];hit.actor.index=i;}
        }
        if(scene_npc_shields.owners) {
            status=scene_npc_shield_query(i,actor_position,end,1,&hit.shield,&accepted);
            if(status && status!=RF_NOT_FOUND)return status;
            if(!status && accepted){
                uint32_t body_before=0;
                /* A coarse sphere can extend beyond the shield. Resolve the
                 * actual body surface before allowing that proxy to occlude it. */
                if(hit.actor.matched && hit.actor.fraction<hit.shield.hit.time) {
                    status=scene_npc_shield_body_before(&hit.shield,actor_position,end,&body_before);
                    if(status)return status;
                }
                if(!body_before){hit.shield_hit=1;hit.actor.matched=1;hit.actor.fraction=hit.shield.hit.time;}
            }
        }
        if(hit.actor.matched){
            hit.query_fraction=hit.actor.fraction;
            if(!rail)hit.actor.fraction=r->sniper.cursor+(1-r->sniper.cursor)*hit.actor.fraction;
            scene_precision_insert(hits,&count,capacity,&hit);
        }
    }
    if(count)nearest=hits[0].actor.fraction;
    /* Sniper rechecks ALL competing cover from the original shot origin on
     * every pass. Advancing through an actor must not leap over a wall, prop,
     * vehicle or detached piece inside that advance or behind earlier actors.
     * Rail retains its original single full-ray pass. */
    status=scene_driller_firearm_select(campaign_player_object.handle,position,delta,
        rail?1:nearest,&vehicle_contact,&vehicle_selected);if(status)return status;
    if(vehicle_selected&&!rail)nearest=vehicle_contact.hit.fraction;
    if(!rail){
        status=scene_clutter_precision_select(position,delta,nearest,&prop);if(status)return status;
        if(prop.slot!=UINT32_MAX)nearest=prop.fraction;
    }
    {
        rf_geomod_registry_hit hit;uint32_t matched,wall=0;float end[3];
        ++rf_scene_detached_hitscan[0];
        status=scene_detached_sources_sweep(stream,0x460,position,delta,0,rail?1:nearest,&hit,&matched);
        rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
        /* Flying sniper round: only a contact this step reaches is taken now. */
        if(!rail && limit<1) {
            float contact=(count || vehicle_selected || prop.slot!=UINT32_MAX)?nearest:1,from[3],to[3];
            rf_collision_solid_hit stop;uint32_t walled=0;
            if(matched && hit.piece.hit.fraction<contact)contact=hit.piece.hit.fraction;
            for(i=0;i<3;++i){from[i]=position[i]+delta[i]*flown;to[i]=position[i]+delta[i]*limit;}
            status=rf_geometry_collision_ray(stream->collision,&campaign_movers,from,to,0x26,&stop,&walled);if(status)return status;
            if(!walled && contact>limit){*done=0;return RF_OK;}
        }
        if(matched && (rail || (!count&&!vehicle_selected&&prop.slot==UINT32_MAX) || hit.piece.hit.fraction<nearest ||
           ((vehicle_selected||prop.slot!=UINT32_MAX)&&hit.piece.hit.fraction==nearest))){
            if(!rail){
                for(i=0;i<3;++i)end[i]=position[i]+delta[i]*hit.piece.hit.fraction;
                status=rf_geometry_collision_ray(stream->collision,&campaign_movers,position,end,0x27,NULL,&wall);
                if(status)return status;
            }
            if(!wall){
                status=scene_detached_sources_damage(stream,hit.batch,hit.piece.piece,rail?r->base:sniper_damage);
                rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
                ++rf_scene_detached_hitscan[1];rf_scene_detached_hitscan[3]=hit.batch;
                rf_scene_detached_hitscan[4]=hit.piece.piece;
                rf_scene_detached_hitscan[5]=npc_hash_bytes(2166136261u,hit.piece.hit.point,12);
                combat_surface_frame=frame;
                scene_weapon_impact_audio_contact(weapon,-1,hit.piece.hit.point);
            }else if(!rail)scene_weapon_impact_world(stream,weapon,position,delta,hit.piece.hit.fraction);
            if(!rail)return RF_OK;
        }
    }

    if(rail){uint32_t prop_hits;
        status=scene_clutter_precision_rail(position,delta,r->base,r->kind,weapon,&prop_hits);if(status)return status;
    }else{uint32_t handled;
        status=scene_clutter_precision_commit(stream,position,delta,&prop,sniper_damage,sniper_kind,weapon,&handled);if(status)return status;
        if(handled)return RF_OK;
    }
    if(vehicle_selected && !(rail && r->rail_vehicle_done)){uint32_t blocked=0,handled;float applied;if(rail)r->rail_vehicle_done=1;
        if(!rail){status=combat_shot_obstructed(stream,position,delta,vehicle_contact.hit.fraction,&blocked);if(status)return status;}
        if(!blocked){
            uint32_t audio_contact=scene_weapon_impact_audio_ready(weapon) &&
                scene_driller_projectile_contact_live(&vehicle_contact,campaign_player_object.handle);
            status=scene_driller_projectile_damage(&vehicle_contact,campaign_player_object.handle,
                rail?r->base:sniper_damage,rail?r->kind:sniper_kind,frame,&handled,&applied);if(status)return status;
            if(handled && audio_contact)scene_weapon_impact_audio_contact(weapon,-1,vehicle_contact.hit.point);
        }else if(!rail)scene_weapon_impact_world(stream,weapon,position,delta,vehicle_contact.hit.fraction);
        if(!rail)return RF_OK;
    }
    for(i=0;i<count;++i){
        campaign_npc_body *owner=campaign_npc_bodies+hits[i].actor.index;
        uint32_t handle=hits[i].actor.handle,entered=0,bits,blocked=0;
        float amount=0,seconds=(float)frame/60,point[3];
        rf_damage_request request={rail?r->base:sniper_base,campaign_player_object.handle,
            rail?r->kind:sniper_kind,0,UINT32_MAX,0};
        combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
            combat_random,combat_notify,combat_playing,combat_play,&feedback};
        if(rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
           owner->damage.effects.health<=0 || (owner->object_flags&(2|0x4000))){
            if(!rail)return RF_OK;continue;
        }
        if(!rail){status=combat_shot_obstructed(stream,position,delta,hits[i].actor.fraction,&blocked);
            if(status)return status;if(blocked){
                scene_weapon_impact_world(stream,weapon,position,delta,hits[i].actor.fraction);return RF_OK;}}
        if(hits[i].shield_hit){
            uint32_t accepted,broken;
            status=scene_npc_shield_commit(&hits[i].shield,actor_position,end,request.amount,request.kind,&accepted,&broken);
            if(status && status!=RF_NOT_FOUND)return status;
            /* A shield-only candidate never falls through to body damage if
             * ownership changed. Rail continues with later ordered owners. */
            if(!rail)return RF_OK;continue;
        }
        for(uint32_t j=0;j<3;j++)point[j]=actor_position[j]+actor_delta[j]*hits[i].query_fraction;
        {uint32_t consumed;status=scene_nano_weapon_contact(owner,rail?r->nano_weapon:weapon,request.amount,point,&consumed);
         if(status)return status;if(consumed){combat_hit_frame=frame;return RF_OK;}}
        if(!rail){
            /* Nano and physical shields precede4c9b20; they keep unscaled base
             * damage and consume the shot. Snapshot this real body's radius
             * before ordinary damage/death can retire or rebuild its owner. */
            request.amount=sniper_damage;
            status=scene_sniper_penetration_body(&r->sniper,handle,owner->body.state.bounds.radius,
                hits[i].actor.fraction,point,end,&continue_sniper);if(status)return status;
        }
        if(rail && r->sniper.count<RF_WEAPON_PRECISION_HITS)r->sniper.handles[r->sniper.count++]=handle;
        memcpy(&bits,&seconds,4);
        status=rf_scene_npc_damage(handle,&request,1,bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        scene_weapon_impact_audio_contact(weapon,(int32_t)owner->collision_material,point);
        if(amount>0){
            combat_hit_frame=frame;campaign_combat_event(frame,0,handle,amount,owner->damage.effects.health);
            if(!campaign_npc_ai_suppressed(owner) && !owner->combat_alert && owner->view.weapons[0]>=0){
                owner->combat_alert=1;owner->combat_due=frame+30;scene_blind_sync(owner,frame);++rf_scene_enemy_combat[1];}
            if(owner->damage.effects.health>0 && owner->combat_alert && campaign_player_form.active){
                campaign_player_form.compromised=1;rf_scene_player_form[2]=1;
            }
        }
        ++rf_scene_combat[1];rf_scene_combat[3]=handle;
        memcpy(rf_scene_combat+4,&owner->damage.effects.health,4);
        if(owner->damage.effects.health<=0){
            status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
            if(entered){++rf_scene_combat[2];status=combat_death_start(hits[i].actor.index);if(status)return status;}
        }
    }
    if(!rail && continue_sniper)continue;
    /* A sniper miss can still contact world geometry. Rail deliberately has
     * no synthetic world hit: its pierced world-contact sequence is not retained. */
    if(!rail && !count)scene_weapon_impact_world(stream,weapon,position,delta,nearest);
    if(rail && limit<1)*done=0; /* rail keeps flying to the end of its range */
    return RF_OK;
    }
}
/* Fire (after the single ammo debit): queue a precision round, or resolve it
 * at once (100-unit ray) when the weapon has no table speed/lifetime or the
 * pool is full. */
static int scene_precision_fire(scene_stream *stream,uint32_t frame,
    const float position[3],const float forward[3],uint32_t rail)
{
    scene_precision_round round={0},*r=NULL;uint32_t i,done;int status;int32_t weapon=campaign_selected_weapon();
    round.rail=rail;round.base=campaign_pistol.damage;round.kind=campaign_pistol.damage_kind;round.weapon=weapon;
    round.nano_weapon=campaign_slot_weapon(campaign_equipped_slot);memcpy(round.origin,position,12);
    if(weapon>=0 && weapon<64 && scene_tracer_weapons[weapon].velocity>0 && scene_tracer_weapons[weapon].lifetime>0)
        for(i=0;i<SCENE_PRECISION_ROUND_CAPACITY;i++)if(!scene_precision_rounds[i].active){r=scene_precision_rounds+i;break;}
    if(r){round.speed=scene_tracer_weapons[weapon].velocity;
        for(i=0;i<3;i++)round.path[i]=forward[i]*round.speed*scene_tracer_weapons[weapon].lifetime;}
    else {for(i=0;i<3;i++)round.path[i]=forward[i]*100.0f;++rf_scene_precision_rounds[2];}
    if(!rail){status=scene_sniper_penetration_begin(campaign_pistol.piercing_enabled,
        campaign_pistol.piercing_power,round.origin,round.path,&round.sniper);if(status)return status;}
    if(rail)scene_rail_trail_fire(stream,weapon,position,round.path);
    if(!r)return scene_precision_step(stream,frame,&round,0,1,&done);
    round.active=1;*r=round;++rf_scene_precision_rounds[0];return RF_OK;
}
/* One game update for every flying precision round. */
static int scene_precision_rounds_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,k,done,active=0;int status;
    for(i=0;i<SCENE_PRECISION_ROUND_CAPACITY;i++){
        scene_precision_round *r=scene_precision_rounds+i;float length=0,limit;
        if(!r->active)continue;
        for(k=0;k<3;k++)length+=r->path[k]*r->path[k];length=sqrtf(length);
        limit=length>0?r->flown_fraction+r->speed*scene_step_seconds/length:1;if(!(limit<1))limit=1;
        status=scene_precision_step(stream,frame,r,r->flown_fraction,limit,&done);if(status)return status;
        if(done || limit>=1){r->active=0;++rf_scene_precision_rounds[1];continue;}
        r->flown_fraction=limit;++active;
    }
    rf_scene_precision_rounds[3]=active;return RF_OK;
}
static void scene_precision_rounds_reset(void){memset(scene_precision_rounds,0,sizeof(scene_precision_rounds));}
