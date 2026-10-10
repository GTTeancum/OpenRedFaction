/* Include after campaign_enemy_tick and scene_turret_combat.inc. Compose the
 * same owned body/shield/vehicle/clutter/GeoMod/damage services as NPC hitscan;
 * no temporary NPC owner or separate damage formula is manufactured. */
#include "scene_turret_audio.inc"
/* This adapter precedes precision gameplay in scene.c. Both includes are
 * guarded, so the same bounded power policy has one definition in this TU. */
#include "rf/weapon_precision.h"
#include "scene_sniper_penetration.inc"
typedef struct scene_turret_scene_context {scene_stream *stream;uint32_t frame;float player_eye[3];} scene_turret_scene_context;
typedef struct scene_turret_penetration_shot {
    uint32_t source,damage_source,player_source,operator_source,enabled;
    int32_t weapon,kind;float base,power;
} scene_turret_penetration_shot;
uint32_t rf_scene_turret_shots[8]; /* shots,damaging hits,shields,vehicles,cover,misses,last victim,errors */
static uint32_t scene_turret_scene_npc_live(uint32_t slot)
{
    const campaign_npc_body *o=campaign_npc_bodies+slot;
    return o->registration.view==&o->view && o->body.allocated_bytes &&
        rf_object_registry_lookup(&campaign_registry,o->registration.handle)==&o->registration &&
        rf_entity_lookup(&campaign_entities,(int32_t)o->registration.handle)==&o->view &&
        !(o->object_flags&(2u|0x4000u)) && !(o->view.flags_810&1u) && o->damage.effects.health>0;
}
static uint32_t scene_turret_scene_player_live(void)
{
    return campaign_player_object.view==&campaign_player_view && scene_actor_body.allocated_bytes &&
        rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view &&
        !(campaign_player_view.flags_7c&(2u|0x4000u)) && !(campaign_player_view.flags_810&1u) &&
        campaign_player_damage.state.effects.health>0;
}
static int scene_turret_scene_target(void *context,uint32_t source,uint32_t index,scene_turret_combat_target *out)
{
    scene_turret_scene_context *c=context;scene_turret_owner *owner=scene_turret_lookup(source);
    scene_turret_combat_target next={0};const float *velocity;uint32_t affiliation;float speed=0;
    if(!c || !out || !owner || index>campaign_npc_body_count)return RF_RANGE;
    affiliation=owner->damage.effects.affiliation;
    if(!index) {
        if(!scene_turret_scene_player_live())return RF_NOT_FOUND;
        next.handle=campaign_player_object.handle;next.alive=1;
        /* Existing NPC policy: affiliation0 acquires the player. Player damage
         * affiliation currently defaults0 too, so equality is NOT a team ID. */
        next.hostile=affiliation==0 && (owner->target==next.handle ||
            !(campaign_player_form.active && !campaign_player_form.compromised));
        memcpy(next.point,c->player_eye,12);velocity=scene_actor_body.state.velocity;
    } else {
        const campaign_npc_body *npc=campaign_npc_bodies+index-1;uint32_t other=npc->damage.effects.affiliation;
        if(!scene_turret_scene_npc_live(index-1)||(scene_turret_operator_excludes(source,npc->registration.handle)||scene_turret_generated_related(source,npc->registration.handle)))return RF_NOT_FOUND;
        next.handle=npc->registration.handle;next.alive=1;
        /* Opposed authored unfriendly/friendly affiliations; neutral actors
         * are not automatic targets. Unknown/outcast policy stays deferred. */
        next.hostile=(affiliation==0 && other==2) || (affiliation==2 && other==0);
        memcpy(next.point,npc->eye_position,12);velocity=npc->body.state.velocity;
    }
    for(uint32_t i=0;i<3;i++)speed+=velocity[i]*velocity[i];next.moving=speed>.0001f;
    *out=next;return RF_OK;
}
static int scene_turret_scene_blocked(void *context,uint32_t source,const float start[3],const float delta[3],uint32_t *blocked)
{
    scene_turret_scene_context *c=context;uint32_t slot;float fraction;int status;(void)source;
    if(!c || !c->stream || !blocked)return RF_RANGE;
    status=combat_shot_obstructed(c->stream,start,delta,1,blocked);if(status || *blocked)return status;
    status=campaign_clutter_firearm_select(start,delta,1,&slot,&fraction);if(status)return status;
    *blocked=slot!=UINT32_MAX;return RF_OK;
}
/* Authored-enabled Vauss only. The accepted shot keeps one spread/audio/cadence
 * event; only live actor selection and contact damage repeat. HEAP, ordinary
 * NPC weapons and the disabled-piercing fallback below retain their adapters. */
static int scene_turret_scene_penetration(scene_turret_scene_context *c,
    const scene_turret_penetration_shot *shot,const float eye[3],const float ray[3])
{
    scene_sniper_penetration penetration;float start[3],delta[3],end[3];
    uint32_t i,clock,frame=c->frame;float seconds=(float)frame/60;int status;
    /* Callbacks cannot retarget the original segment or accepted identity,
     * base damage, authored power and operator's full generation-bearing ID. */
    memcpy(start,eye,12);memcpy(delta,ray,12);memcpy(&clock,&seconds,4);
    for(i=0;i<3;i++)end[i]=start[i]+delta[i];
    status=scene_sniper_penetration_begin(shot->enabled,shot->power,start,delta,&penetration);if(status)return status;
    for(;;) {
        scene_ai_shield_ray_selection npc_shield={0};scene_player_shield_candidate player_shield={0};
        rf_weapon_flight_contact vehicle_contact={0};uint32_t blocked,vehicle=0,kind=0,slot=UINT32_MAX;
        uint32_t handle=UINT32_MAX,body_hit=0,more=0;
        float fraction=1,query_fraction,body_fraction=1,damage,amount=0;
        const float *actor_start=penetration.start,*actor_delta=penetration.delta;
        combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60%RF_TIMER_PERIOD),0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
            combat_random,combat_notify,combat_playing,combat_play,&feedback};
        status=scene_sniper_penetration_damage(&penetration,shot->base,&damage);if(status)return status;
        /* Preserve player-first enumeration, independent physical shields and
         * strict-nearer NPC ties. Only actor queries use the advanced segment. */
        if(scene_turret_scene_player_live() && campaign_player_object.handle!=shot->source &&
           campaign_player_object.handle!=shot->player_source &&
           !scene_sniper_penetration_visited(&penetration,campaign_player_object.handle)) {
            float hit_fraction=1;uint32_t shield_hit;
            body_hit=combat_body(actor_start,actor_delta,&scene_actor_body,1,&hit_fraction);body_fraction=hit_fraction;
            if(body_hit){fraction=hit_fraction;kind=1;handle=campaign_player_object.handle;}
            /* Autonomous context retains the physical player eye; the mounted
             * firing player's own body and shield are excluded above. */
            status=scene_player_shield_query(c->stream,c->player_eye,actor_look.eye_orientation,
                actor_start,end,1,&player_shield,&shield_hit);if(status)return status;
            if(shield_hit){fraction=player_shield.hit.time;kind=3;handle=campaign_player_object.handle;}
        }
        for(i=0;i<campaign_npc_body_count;i++) {
            scene_ai_shield_ray_selection shield={0};float hit_fraction=1;uint32_t hit;
            const campaign_npc_body *npc=campaign_npc_bodies+i;
            if(!scene_turret_scene_npc_live(i) || npc->registration.handle==shot->source ||
               npc->registration.handle==shot->player_source ||
               scene_sniper_penetration_visited(&penetration,npc->registration.handle))continue;
            if(npc->registration.handle==shot->operator_source){++rf_scene_turret_operator[5];continue;}
            if(scene_turret_generated_related(shot->source,npc->registration.handle))continue;
            hit=combat_body(actor_start,actor_delta,&npc->body,1,&hit_fraction);
            status=scene_ai_shield_ray_select(i,actor_start,end,hit,hit_fraction,&shield);if(status)return status;
            if(shield.kind!=SCENE_AI_SHIELD_RAY_MISS && shield.fraction<fraction) {
                slot=i;handle=npc->registration.handle;fraction=shield.fraction;npc_shield=shield;
                body_hit=hit;body_fraction=hit_fraction;kind=shield.kind==SCENE_AI_SHIELD_RAY_SHIELD?4:2;
            }
        }
        query_fraction=fraction;
        if(kind)fraction=penetration.cursor+(1-penetration.cursor)*query_fraction;
        /* Keep the original complete-ray cover order on EVERY contact. A body
         * radius advance cannot leap over a prop, vehicle, fragment or wall.
         * These terminal objects receive fresh pre-contact power scaling. */
        status=scene_driller_firearm_select(shot->source,start,delta,fraction,&vehicle_contact,&vehicle);if(status)return status;
        if(vehicle)fraction=vehicle_contact.hit.fraction;
        {uint32_t consumed;
         status=campaign_clutter_enemy_fire(c->stream,start,delta,fraction,damage,shot->kind,0,&consumed);if(status)return status;
         if(consumed){++rf_scene_turret_shots[4];return RF_OK;}}
        status=combat_enemy_fragment_shot(c->stream,start,delta,fraction,damage,&blocked);if(status)return status;
        if(blocked){++rf_scene_turret_shots[4];return RF_OK;}
        if(vehicle) {
            uint32_t handled;
            status=scene_driller_projectile_damage(&vehicle_contact,shot->damage_source,damage,shot->kind,frame,&handled,&amount);if(status)return status;
            rf_scene_turret_shots[3]+=handled;rf_scene_turret_shots[1]+=amount>0;
            rf_scene_turret_shots[6]=vehicle_contact.face;return RF_OK;
        }
        if(!kind){if(!penetration.count)++rf_scene_turret_shots[5];return RF_OK;}
        /* Revalidate the exact selected full handle after cover callbacks. */
        if((kind==2 || kind==4)?
           (slot>=campaign_npc_body_count || campaign_npc_bodies[slot].registration.handle!=handle || !scene_turret_scene_npc_live(slot)):
           (campaign_player_object.handle!=handle || !scene_turret_scene_player_live()))return RF_OK;
        {
            rf_damage_request request={shot->base,shot->damage_source,shot->kind,0,UINT32_MAX,0};
            uint32_t accepted=0,broken;float point[3],radius;
            if(kind==3)status=scene_player_shield_commit(&player_shield,&request,&accepted,&broken);
            else if(kind==4)status=scene_ai_shield_ray_commit(&npc_shield,actor_start,end,&request,&accepted,&broken);
            else status=RF_OK;if(status)return status;
            if(accepted){++rf_scene_turret_shots[2];return RF_OK;}
            /* A rejected shield is not a body contact. Admit only its retained
             * actual sphere point within the already-checked actor/cover limit;
             * do not jump to an unchecked farther body or fabricate a hit. */
            if(!body_hit || body_fraction>query_fraction){if(!penetration.count)++rf_scene_turret_shots[5];return RF_OK;}
            if(kind==3 || kind==4)kind-=2;
            if(kind==2) {
                campaign_npc_body *victim=campaign_npc_bodies+slot;uint32_t consumed;
                if(slot>=campaign_npc_body_count || victim->registration.handle!=handle || !scene_turret_scene_npc_live(slot))return RF_OK;
                ++rf_scene_enemy_damage_kinds[shot->kind];
                status=scene_nano_weapon_contact(victim,shot->weapon,shot->base,&consumed);if(status)return status;
                if(consumed)return RF_OK;
                radius=victim->body.state.bounds.radius;
            } else {
                if(campaign_player_object.handle!=handle || !scene_turret_scene_player_live())return RF_OK;
                ++rf_scene_enemy_damage_kinds[shot->kind];radius=scene_actor_body.state.bounds.radius;
            }
            for(i=0;i<3;i++)point[i]=actor_start[i]+actor_delta[i]*body_fraction;
            fraction=penetration.cursor+(1-penetration.cursor)*body_fraction;
            request.amount=damage;
            /* Spend the real enclosing body radius and prepare the next ray
             * before damage/death can rebuild its owner. Zero applied damage
             * still spends power; the exhausting body still receives damage. */
            status=scene_sniper_penetration_body(&penetration,handle,radius,fraction,point,end,&more);if(status)return status;
            if(kind==2) {
                campaign_npc_body *victim=campaign_npc_bodies+slot;uint32_t entered;
                status=rf_scene_npc_damage(handle,&request,1,clock,&effects,&amount);if(!status)status=feedback.status;if(status)return status;
                rf_scene_turret_shots[6]=handle;
                if(victim->damage.effects.health<=0) {
                    status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
                    if(entered){victim->script_move.active=0;status=combat_death_start(slot);if(status)return status;}
                }
            } else {
                status=rf_scene_player_damage(handle,&request,1,clock,&effects,&amount);if(!status)status=feedback.status;if(status)return status;
                rf_scene_turret_shots[6]=handle;
                if(amount>0)campaign_combat_event(frame,1,shot->source,amount,campaign_player_damage.state.effects.health);
            }
        }
        rf_scene_turret_shots[1]+=amount>0;++rf_scene_enemy_spread[2];
        memcpy(rf_scene_enemy_combat+5,&campaign_player_damage.state.effects.health,4);
        rf_scene_enemy_combat[6]=campaign_player_damage.state.effects.health<=0;
        if(!more)return RF_OK;
    }
}
static int scene_turret_scene_shot(void *context,uint32_t source,uint32_t intended,int32_t weapon,
    const float eye[3],const float muzzle[3],const float ray[3],const rf_weapon_primary_definition *definition)
{
    scene_turret_scene_context *c=context;scene_turret_owner *shooter=scene_turret_lookup(source);
    scene_turret_penetration_shot piercing={0};
    scene_ai_shield_ray_selection npc_shield={0};scene_player_shield_candidate player_shield={0};
    rf_weapon_flight_contact vehicle_contact={0};uint32_t i,blocked,vehicle=0,kind=0,slot=UINT32_MAX;
    uint32_t body_hit=0,clock,player_source,damage_source;
    float spread[3],end[3],fraction=1,body_fraction=1,damage,spread_degrees,amount=0;
    int status;float seconds;combat_feedback feedback;
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    (void)intended; /* The actual nearest hit, not the intended target, owns damage. */
    if(!c || !c->stream || !shooter || !definition || !eye || !muzzle || !ray ||
       !scene_turret_primary_for(weapon) || definition->damage_kind<0 || definition->damage_kind>8)return RF_RANGE;
    /* Collision identity stays the turret so its own mesh is excluded.
     * A mounted player's actual handle owns damage and uses authored player
     * values; an autonomous/NPC-operated turret retains the prior AI policy. */
    player_source=scene_turret_player_source(source);
    damage_source=player_source==UINT32_MAX?source:player_source;
    if(weapon==scene_turret_heap_id){
        uint32_t excluded=player_source,accepted=0;
        scene_turret_audio_stop(source);
        if(excluded==UINT32_MAX){uint32_t bound,eligible,affiliation;
            status=scene_turret_operator_controls(source,&bound,&eligible,&affiliation,&excluded);if(status)return status;
            if(!bound)excluded=UINT32_MAX;
        }
        status=scene_turret_heap_launch(c->stream,source,damage_source,excluded,eye,ray,definition,&scene_turret_heap_explosive,&accepted);
        if(status)return status;if(!accepted)return RF_NOT_FOUND;
        ++rf_scene_turret_shots[0];return RF_OK;
    }
    damage=player_source==UINT32_MAX?combat_enemy_primary_damage(definition):definition->damage;
    spread_degrees=player_source==UINT32_MAX?definition->ai_spread_degrees:definition->spread_degrees;
    if(!isfinite(damage) || damage<0)return RF_FORMAT;
    if(weapon==scene_turret_vauss_id && definition->piercing_enabled) {
        const scene_turret_operator_binding *binding=scene_turret_operator_find(source);
        piercing=(scene_turret_penetration_shot){source,damage_source,player_source,
            binding?binding->actor:UINT32_MAX,definition->piercing_enabled,
            weapon,definition->damage_kind,damage,definition->piercing_power};
    }
    seconds=(float)c->frame/60;memcpy(&clock,&seconds,4);
    feedback=(combat_feedback){(int32_t)((uint64_t)c->frame*1000/60%RF_TIMER_PERIOD),0};
    status=rf_weapon_spread_ray(ray,spread_degrees,&campaign_enemy_spread_random,spread);if(status)return status;
    for(i=0;i<3;i++)end[i]=eye[i]+spread[i];
    ++rf_scene_turret_shots[0];++rf_scene_enemy_spread[0];
    if(spread_degrees>0)++rf_scene_enemy_spread[1];rf_scene_enemy_spread[5]=campaign_enemy_spread_random.value;
    {const char *auto_head="Auto Turret Head";
     const char *name;status=scene_turret_class_name(&campaign_seeds,shooter->class_index,&name);if(status)goto fail;
     scene_turret_audio_shot(shooter,weapon,
         rf_emitter_name_lookup(&auto_head,1,name)==0?"Vauss2 Fire":"Vauss5 Fire",muzzle);}
    if(piercing.enabled) {
        status=scene_turret_scene_penetration(c,&piercing,eye,spread);if(status)goto fail;
        return RF_OK;
    }
    /* Each candidate combines its body and held shield before comparing with
     * other actors. This preserves outstretched shields outside coarse spheres. */
    if(scene_turret_scene_player_live() && campaign_player_object.handle!=source &&
       campaign_player_object.handle!=player_source) {
        float hit_fraction=1;uint32_t shield_hit;
        body_hit=combat_body(eye,spread,&scene_actor_body,1,&hit_fraction);body_fraction=hit_fraction;
        if(body_hit){fraction=hit_fraction;kind=1;}
        status=scene_player_shield_query(c->stream,c->player_eye,actor_look.eye_orientation,eye,end,1,&player_shield,&shield_hit);if(status)goto fail;
        if(shield_hit){fraction=player_shield.hit.time;kind=3;}
    }
    for(i=0;i<campaign_npc_body_count;i++) {
        scene_ai_shield_ray_selection shield={0};float hit_fraction=1;uint32_t hit;
        const campaign_npc_body *npc=campaign_npc_bodies+i;
        if(!scene_turret_scene_npc_live(i) || npc->registration.handle==source || (scene_turret_operator_excludes(source,npc->registration.handle)||scene_turret_generated_related(source,npc->registration.handle)))continue;
        hit=combat_body(eye,spread,&npc->body,1,&hit_fraction);
        status=scene_ai_shield_ray_select(i,eye,end,hit,hit_fraction,&shield);if(status)goto fail;
        if(shield.kind!=SCENE_AI_SHIELD_RAY_MISS && shield.fraction<fraction) {
            slot=i;fraction=shield.fraction;npc_shield=shield;body_hit=hit;body_fraction=hit_fraction;
            kind=shield.kind==SCENE_AI_SHIELD_RAY_SHIELD?4:2;
        }
    }
    status=scene_driller_firearm_select(source,eye,spread,fraction,&vehicle_contact,&vehicle);if(status)goto fail;
    if(vehicle)fraction=vehicle_contact.hit.fraction;
    {uint32_t consumed;
     status=campaign_clutter_enemy_fire(c->stream,eye,spread,fraction,damage,definition->damage_kind,0,&consumed);if(status)goto fail;
     if(consumed){++rf_scene_turret_shots[4];return RF_OK;}}
    status=combat_enemy_fragment_shot(c->stream,eye,spread,fraction,damage,&blocked);if(status)goto fail;
    if(blocked){++rf_scene_turret_shots[4];return RF_OK;}
    if(vehicle) {
        uint32_t handled;
        status=scene_driller_projectile_damage(&vehicle_contact,damage_source,damage,definition->damage_kind,c->frame,&handled,&amount);if(status)goto fail;
        rf_scene_turret_shots[3]+=handled;rf_scene_turret_shots[1]+=amount>0;
        rf_scene_turret_shots[6]=vehicle_contact.face;return RF_OK;
    }
    {rf_damage_request request={damage,damage_source,definition->damage_kind,0,UINT32_MAX,0};uint32_t accepted=0,broken;
     if(kind==3)status=scene_player_shield_commit(&player_shield,&request,&accepted,&broken);
     else if(kind==4)status=scene_ai_shield_ray_commit(&npc_shield,eye,end,&request,&accepted,&broken);
     else status=RF_OK;if(status)goto fail;
     if(accepted){++rf_scene_turret_shots[2];return RF_OK;}
     if(kind==3 || kind==4) {
         /* Match ordinary AI fallback if a held shield changes between query
          * and commit: only damage the original body when its own cover clears. */
         if(!body_hit){++rf_scene_turret_shots[5];return RF_OK;}
         status=combat_shot_obstructed(c->stream,eye,spread,body_fraction,&blocked);if(status)goto fail;
         if(blocked){++rf_scene_turret_shots[4];return RF_OK;}kind-=2;
     }
     if(!kind){++rf_scene_turret_shots[5];return RF_OK;}
     ++rf_scene_enemy_damage_kinds[definition->damage_kind];
     if(kind==2) {
         campaign_npc_body *victim=campaign_npc_bodies+slot;uint32_t consumed,entered;
         status=scene_nano_weapon_contact(victim,weapon,damage,&consumed);if(status)goto fail;if(consumed)return RF_OK;
         status=rf_scene_npc_damage(victim->registration.handle,&request,1,clock,&effects,&amount);if(!status)status=feedback.status;if(status)goto fail;
         rf_scene_turret_shots[6]=victim->registration.handle;
         if(victim->damage.effects.health<=0) {
             status=rf_scene_npc_death_entry(victim->registration.handle,&entered);if(status)goto fail;
             if(entered){victim->script_move.active=0;status=combat_death_start(slot);if(status)goto fail;}
         }
     } else {
         status=rf_scene_player_damage(campaign_player_object.handle,&request,1,clock,&effects,&amount);if(!status)status=feedback.status;if(status)goto fail;
         rf_scene_turret_shots[6]=campaign_player_object.handle;
         if(amount>0)campaign_combat_event(c->frame,1,source,amount,campaign_player_damage.state.effects.health);
     }
    }
    rf_scene_turret_shots[1]+=amount>0;++rf_scene_enemy_spread[2];
    /* Publish ordinary health/death HUD inputs as the NPC path does. */
    memcpy(rf_scene_enemy_combat+5,&campaign_player_damage.state.effects.health,4);
    rf_scene_enemy_combat[6]=campaign_player_damage.state.effects.health<=0;
    return RF_OK;
fail:
    ++rf_scene_turret_shots[7];return status;
}
static int scene_turret_scene_tick(scene_stream *stream,uint32_t frame)
{
    scene_turret_scene_context context={stream,frame,{0}};scene_turret_combat_backend backend;
    if(!scene_turret_count){scene_turret_audio_close();return RF_OK;}
    if(!stream || !stream->collision || campaign_npc_body_count>=1024){scene_turret_audio_close();return RF_RANGE;}
    if(rf_scene_swim_test_enabled){scene_turret_audio_close();return RF_OK;} /* Same quiet movement-fixture rule as NPC AI. */
    /* Frame zero primes the scene; its ordinary load commits after callbacks.
     * Do not acquire/fire from authored state before restored owners publish. */
    if(!frame){scene_turret_audio_close();memset(rf_scene_turret_shots,0,sizeof(rf_scene_turret_shots));return RF_OK;}
    /* actor_follow_view publishes this gameplay listener before combat, ahead
     * of diagnostic view overrides. Do not advance actor_listener_pose twice. */
    memcpy(context.player_eye,campaign_listener_position,12);
    /* Mounted camera/audio are at the gun; enemy aim and shield queries
     * still target the player's physical body, which remains collision-live. */
    (void)scene_turret_player_body_eye(context.player_eye);
    backend=(scene_turret_combat_backend){campaign_npc_body_count+1,scene_turret_scene_target,
        scene_turret_scene_blocked,scene_turret_scene_shot,&context};
    {int status=scene_turret_generated_activation_tick(frame,&backend);if(status){scene_turret_audio_close();return status;}}
    return scene_turret_combat_tick(frame,&backend);
}
