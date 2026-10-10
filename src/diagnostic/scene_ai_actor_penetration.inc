#ifndef RF_SCENE_AI_ACTOR_PENETRATION_INC
#define RF_SCENE_AI_ACTOR_PENETRATION_INC
/* Include after scene_ai_actor_hit_probe.inc. Shared synchronous NPC bullet
 * actor continuation, extracted from the admitted Tankbot consumer. Exact
 * weapon wrappers qualify the accepted descriptor; the caller checks source
 * identity once before presentation and retains one shot/debit/cadence/spread.
 * Only the shared4c9b20 body policy lives here: no world-metal traversal,
 * ricochet, precision target policy or persistent projectile scheduler. */
#include "rf/weapon_precision.h"
#include "scene_sniper_penetration.inc"

typedef struct scene_ai_actor_penetration_shot {
    uint32_t source,intended,enabled;
    int32_t weapon,kind;
    float base,power;
} scene_ai_actor_penetration_shot;
typedef struct scene_ai_actor_penetration_result {
    float total_applied,player_applied,vehicle_applied;
    uint32_t vehicle_handle;
} scene_ai_actor_penetration_result;

static uint32_t scene_ai_actor_penetration_npc_identity(uint32_t slot,uint32_t handle)
{
    const campaign_npc_body *owner;
    if(slot>=campaign_npc_body_count)return 0;
    owner=campaign_npc_bodies+slot;
    return owner->registration.handle==handle && owner->registration.view==&owner->view &&
        rf_object_registry_lookup(&campaign_registry,handle)==&owner->registration &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&owner->view;
}
static uint32_t scene_ai_actor_penetration_npc_live(uint32_t slot,uint32_t handle)
{
    const campaign_npc_body *owner;
    if(!scene_ai_actor_penetration_npc_identity(slot,handle))return 0;
    owner=campaign_npc_bodies+slot;
    return owner->body.allocated_bytes && !(owner->object_flags&(2u|0x4000u)) &&
        !(owner->view.flags_810&1u) && owner->damage.effects.health>0;
}
static uint32_t scene_ai_actor_penetration_player_live(uint32_t handle)
{
    return campaign_player_object.handle==handle &&
        campaign_player_object.view==&campaign_player_view && scene_actor_body.allocated_bytes &&
        rf_object_registry_lookup(&campaign_registry,handle)==&campaign_player_object &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&campaign_player_view &&
        !(campaign_player_view.flags_7c&(2u|0x4000u)) && !(campaign_player_view.flags_810&1u) &&
        campaign_player_damage.state.effects.health>0;
}

/* Mirror scene_ai_actor_ray_select's player-first and strict-nearer ordering,
 * including its inclusive endpoint when no actor was selected. Query body and
 * independent physical shield together, then compare actors. Only the accepted
 * full source and visited fullhandles differ from the ordinary selector: a
 * damage callback changing/killing the shooter cannot retarget this shot. */
static int scene_ai_actor_penetration_actor_select(scene_stream *stream,
    uint32_t source,const scene_sniper_penetration *penetration,const float end[3],
    const float player_eye[3],scene_ai_actor_ray_selection *out,uint32_t *handle)
{
    scene_ai_actor_ray_selection selected={0};uint32_t i,selected_handle=UINT32_MAX;int status;
    const float *start=penetration->start,*delta=penetration->delta;
    selected.victim_slot=UINT32_MAX;selected.fraction=selected.body_fraction=1;
    if(scene_ai_actor_penetration_player_live(campaign_player_object.handle) &&
       !scene_ai_actor_ray_related(source,campaign_player_object.handle) &&
       !scene_sniper_penetration_visited(penetration,campaign_player_object.handle)) {
        selected.body_hit=combat_body(start,delta,&scene_actor_body,1,&selected.body_fraction);
        if(!selected.body_hit)selected.body_fraction=1;
        selected.actor_hit=selected.body_hit;selected.fraction=selected.body_fraction;
        status=scene_player_shield_query(stream,player_eye,actor_look.eye_orientation,
            start,end,1,&selected.player_shield,&selected.shield_hit);if(status)return status;
        if(selected.shield_hit){selected.actor_hit=1;selected.fraction=selected.player_shield.hit.time;}
        if(selected.actor_hit)selected_handle=campaign_player_object.handle;
    }
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *candidate=campaign_npc_bodies+i;
        scene_ai_shield_ray_selection shield={0};uint32_t hit,actual=candidate->registration.handle;
        float body_fraction=1;
        if(!scene_ai_actor_penetration_npc_live(i,actual) ||
           scene_ai_actor_ray_related(source,actual) ||
           scene_sniper_penetration_visited(penetration,actual))continue;
        hit=combat_body(start,delta,&candidate->body,1,&body_fraction);
        if(!hit)body_fraction=1;
        status=scene_ai_shield_ray_select(i,start,end,hit,body_fraction,&shield);if(status)return status;
        if(shield.kind==SCENE_AI_SHIELD_RAY_MISS ||
           (selected.actor_hit && shield.fraction>=selected.fraction))continue;
        selected.victim=candidate;selected.victim_slot=i;selected.actor_hit=1;
        selected.body_hit=hit;selected.body_fraction=body_fraction;selected.fraction=shield.fraction;
        selected.npc_shield=shield;selected.shield_hit=0;selected_handle=actual;
        memset(&selected.player_shield,0,sizeof(selected.player_shield));
    }
    *out=selected;*handle=selected_handle;return RF_OK;
}

static int scene_ai_actor_penetration_fire(scene_stream *stream,uint32_t frame,
    const campaign_npc_body *accepted_owner,uint32_t accepted_slot,
    const scene_ai_actor_penetration_shot *shot,const float eye[3],const float ray[3],
    const float player_eye[3],uint32_t clock_bits,const rf_damage_effect_backend *effects,
    combat_feedback *feedback,scene_ai_actor_penetration_result *out)
{
    scene_ai_actor_penetration_shot accepted;
    scene_sniper_penetration penetration;float start[3],delta[3],end[3],physical_eye[3];uint32_t i;int status;
    if(!stream || !shot || !eye || !ray || !player_eye || !effects || !feedback || !out ||
       shot->enabled!=1 || shot->weapon<0 || shot->weapon>=64 || shot->kind!=1 ||
       !isfinite(shot->base) || shot->base<0)return RF_RANGE;
    /* Identity and damage inputs belong to the accepted shot, not to live
     * inventory/selection after presentation or any contact/death callback. */
    accepted=*shot;shot=&accepted;
    *out=(scene_ai_actor_penetration_result){0,0,0,UINT32_MAX};
    memcpy(start,eye,12);memcpy(delta,ray,12);memcpy(physical_eye,player_eye,12);
    for(i=0;i<3;i++) {
        end[i]=start[i]+delta[i];
        if(!isfinite(end[i]) || !isfinite(physical_eye[i]))return RF_RANGE;
    }
    status=scene_sniper_penetration_begin(shot->enabled,shot->power,start,delta,&penetration);if(status)return status;
    for(;;) {
        scene_ai_actor_ray_selection actor;rf_weapon_flight_contact vehicle_contact;
        uint32_t handle,vehicle_hit=0,blocked=0,more=0;
        float fraction,query_fraction,damage,applied=0;
        const float *actor_start=penetration.start,*actor_delta=penetration.delta;
        status=scene_sniper_penetration_damage(&penetration,shot->base,&damage);if(status)return status;
        status=scene_ai_actor_penetration_actor_select(stream,shot->source,&penetration,end,
            physical_eye,&actor,&handle);if(status)return status;
        query_fraction=actor.fraction;
        fraction=actor.actor_hit?penetration.cursor+(1-penetration.cursor)*query_fraction:1;
        /* Actor queries advance. All competing cover keeps the ORIGINAL ray
         * on EVERY pass, so radius advance cannot skip a wall/prop/host/chunk.
         * Terminal objects receive fresh pre-contact power-scaled damage. */
        status=scene_driller_firearm_select(shot->source,start,delta,fraction,
            &vehicle_contact,&vehicle_hit);if(status)return status;
        if(vehicle_hit)fraction=vehicle_contact.hit.fraction;
        {uint32_t consumed;float contact_fraction;
            status=campaign_clutter_enemy_fire_contact(stream,start,delta,fraction,
                damage,shot->kind,0,&consumed,&contact_fraction);if(status)return status;
            if(contact_fraction>=0)scene_npc_bullet_impact_ray(shot->weapon,-1,start,delta,contact_fraction);
            if(consumed)return RF_OK;
        }
        {scene_npc_bullet_contact contact;
            status=combat_enemy_fragment_shot_contact(stream,start,delta,fraction,damage,&blocked,&contact);if(status)return status;
            scene_npc_bullet_impact_stop(stream,shot->weapon,start,delta,&contact);
        }
        if(blocked){++rf_scene_enemy_spread[4];return RF_OK;}
        if(vehicle_hit) {
            uint32_t handled,impact_live;
            status=combat_shot_obstructed(stream,start,delta,fraction,&blocked);if(status)return status;
            if(blocked){++rf_scene_enemy_spread[4];return RF_OK;}
            impact_live=scene_npc_bullet_impact_ready(shot->weapon) &&
                scene_driller_projectile_contact_live(&vehicle_contact,shot->source);
            status=scene_driller_projectile_damage(&vehicle_contact,shot->source,damage,
                shot->kind,frame,&handled,&applied);if(status)return status;
            if(handled && impact_live)scene_npc_bullet_impact_contact(shot->weapon,-1,vehicle_contact.hit.point);
            if(handled && applied>0) {
                out->total_applied+=applied;out->vehicle_applied=applied;
                out->vehicle_handle=vehicle_contact.face;
            }
            return RF_OK;
        }
        if(!actor.actor_hit){if(!penetration.count)++rf_scene_enemy_spread[3];return RF_OK;}
        /* Cover callbacks may retire/rebind candidates. Revalidate the exact
         * winning generation before shields, then again before body damage. */
        if(actor.victim?!scene_ai_actor_penetration_npc_live(actor.victim_slot,handle):
           !scene_ai_actor_penetration_player_live(handle))return RF_OK;
        {
            rf_damage_request request={shot->base,shot->source,shot->kind,0,UINT32_MAX,0};
            uint32_t shield_contact=actor.shield_hit ||
                (actor.victim && actor.npc_shield.kind==SCENE_AI_SHIELD_RAY_SHIELD);
            uint32_t shield_accepted=0,broken;float point[3],radius;int32_t material;
            if(actor.shield_hit)status=scene_player_shield_commit(&actor.player_shield,&request,&shield_accepted,&broken);
            else if(shield_contact)status=scene_ai_shield_ray_commit(&actor.npc_shield,
                actor_start,end,&request,&shield_accepted,&broken);
            else status=RF_OK;if(status)return status;
            /* Physical shields (including the breaking hit) and nano keep the
             * fresh AI-scaled base, stop immediately and never spend power. */
            if(shield_accepted)return RF_OK;
            if(!actor.body_hit || actor.body_fraction>query_fraction) {
                if(!penetration.count)++rf_scene_enemy_spread[3];return RF_OK;
            }
            fraction=penetration.cursor+(1-penetration.cursor)*actor.body_fraction;
            if(shield_contact) {
                /* Rejected protection may use only its retained real body
                 * within the already-arbitrated limit, never a farther body. */
                status=combat_shot_obstructed(stream,start,delta,fraction,&blocked);if(status)return status;
                if(blocked){++rf_scene_enemy_spread[4];return RF_OK;}
            }
            if(actor.victim) {
                campaign_npc_body *victim;uint32_t consumed;
                if(!scene_ai_actor_penetration_npc_live(actor.victim_slot,handle))return RF_OK;
                victim=campaign_npc_bodies+actor.victim_slot;
                ++rf_scene_enemy_spread[2];++rf_scene_enemy_damage_kinds[shot->kind];
                status=scene_nano_weapon_contact(victim,shot->weapon,shot->base,&consumed);if(status)return status;
                if(consumed)return RF_OK;
                if(!scene_ai_actor_penetration_npc_live(actor.victim_slot,handle))return RF_OK;
                radius=victim->body.state.bounds.radius;material=(int32_t)victim->collision_material;
            } else {
                if(!scene_ai_actor_penetration_player_live(handle))return RF_OK;
                ++rf_scene_enemy_spread[2];++rf_scene_enemy_damage_kinds[shot->kind];
                radius=scene_actor_body.state.bounds.radius;material=(int32_t)campaign_player_material;
            }
            for(i=0;i<3;i++)point[i]=actor_start[i]+actor_delta[i]*actor.body_fraction;
            request.amount=damage;
            /* Original4c9b75..4c9bde spends .2f times the actual enclosing body
             * radius and advances that radius along incoming direction. Save
             * point/radius/material and prepare continuation BEFORE callbacks.
             * Zero applied damage still spends; exhausting contact still hits. */
            status=scene_sniper_penetration_body(&penetration,handle,radius,fraction,point,end,&more);if(status)return status;
            if(actor.victim) {
                campaign_npc_body *victim=campaign_npc_bodies+actor.victim_slot;uint32_t entered;
                status=rf_scene_npc_damage(handle,&request,1,clock_bits,effects,&applied);
                if(!status)status=feedback->status;if(status)return status;
                if(scene_ai_actor_penetration_npc_identity(actor.victim_slot,handle) &&
                   victim->damage.effects.health<=0) {
                    status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
                    if(entered && scene_ai_actor_penetration_npc_identity(actor.victim_slot,handle)) {
                        victim->script_move.active=0;status=combat_death_start(actor.victim_slot);if(status)return status;
                    }
                }
            } else {
                status=rf_scene_player_damage(handle,&request,1,clock_bits,effects,&applied);
                if(!status)status=feedback->status;if(status)return status;
                out->player_applied+=applied;
            }
            out->total_applied+=applied;
            scene_npc_bullet_impact_contact(shot->weapon,material,point);
            /* Accepted owner/slot are observation only. Never manufacture a
             * temporary source owner or let a rebound slot corrupt the probe. */
            if(accepted_slot<campaign_npc_body_count && accepted_owner==campaign_npc_bodies+accepted_slot &&
               accepted_owner->registration.handle==shot->source)
                scene_ai_actor_hit_probe(accepted_owner,shot->intended,handle,frame,applied);
        }
        if(!more)return RF_OK;
    }
}
#endif
