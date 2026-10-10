#ifndef RF_SCENE_HANDHELD_PENETRATION_INC
#define RF_SCENE_HANDHELD_PENETRATION_INC
/* Include after scene_precision_gameplay.inc. Bounded ordinary handheld
 * continuation shares4c9b20 power/radius policy, NOT precision shot selection.
 * The caller admits only authored-enabled AR/HMG/scoped rifle/MP Special,
 * snapshots accepted identity/mode/base after one ammo debit, and supplies the
 * existing post-spread ray. No trigger, ammo, random draw or persistent state.
 * This deliberately duplicates only the ordinary contact/damage adapter;
 * Riot Stick, shotgun and nonpiercing consumers remain untouched. */
typedef struct scene_handheld_shot {
    int32_t weapon,kind;
    uint32_t slot,alternate,enabled,source;
    float base,power;
} scene_handheld_shot;

static int scene_handheld_penetration_fire(scene_stream *stream,uint32_t frame,
    uint32_t pellet,const float position[3],const float delta[3],const scene_handheld_shot *shot)
{
    scene_sniper_penetration penetration;float end[3],original_position[3],original_delta[3];uint32_t i;int status;
    if(!shot || !shot->enabled || !position || !delta)return RF_RANGE;
    /* Damage/death callbacks cannot retarget the retained original segment. */
    memcpy(original_position,position,12);memcpy(original_delta,delta,12);
    position=original_position;delta=original_delta;
    status=scene_sniper_penetration_begin(shot->enabled,shot->power,position,delta,&penetration);
    if(status)return status;
    for(i=0;i<3;i++)end[i]=position[i]+delta[i];
    for(;;) {
        scene_npc_shield_candidate shield_candidate={0};
        scene_clutter_precision_contact prop={UINT32_MAX,UINT32_MAX,1};
        rf_weapon_flight_contact vehicle_contact;
        uint32_t target=UINT32_MAX,handle=UINT32_MAX,shield_selected=0,vehicle_selected=0;
        uint32_t body_selected=0,blocked=0,more=0;
        float nearest=1,query_fraction,body_fraction=1,damage=shot->base;
        const float *actor_position=penetration.start,*actor_delta=penetration.delta;
        status=scene_sniper_penetration_damage(&penetration,shot->base,&damage);if(status)return status;
        /* Keep the ordinary body-before-shield enumeration and strict-nearer
         * ties. Only actor queries advance; generation-bearing visited handles
         * prevent a surviving owner from receiving this same shot twice. */
        for(i=0;i<campaign_npc_body_count;i++) {
            campaign_npc_body *owner=campaign_npc_bodies+i;
            float fraction=1,actor_limit=nearest;uint32_t body_hit=0;
            if(!owner->registration.view || (owner->object_flags&(2|0x4000)) ||
               !owner->body.allocated_bytes || owner->damage.effects.health<=0 || (owner->view.flags_810&1))continue;
            if(rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration ||
               scene_sniper_penetration_visited(&penetration,owner->registration.handle))continue;
            if(combat_body(actor_position,actor_delta,&owner->body,nearest,&fraction) && fraction<nearest) {
                body_hit=1;nearest=fraction;target=i;handle=owner->registration.handle;shield_selected=0;
                body_selected=1;body_fraction=fraction;
            }
            if(scene_npc_shields.owners) {
                uint32_t hit;scene_npc_shield_candidate candidate;
                status=scene_npc_shield_query(i,actor_position,end,actor_limit,&candidate,&hit);if(status)return status;
                if(hit && target==i && nearest<candidate.hit.time) {
                    uint32_t body_before;
                    status=scene_npc_shield_body_before(&candidate,actor_position,end,&body_before);if(status)return status;
                    if(body_before)hit=0;
                }
                if(hit && (target==i || candidate.hit.time<nearest)) {
                    nearest=candidate.hit.time;target=i;handle=owner->registration.handle;
                    shield_candidate=candidate;shield_selected=1;body_selected=body_hit;body_fraction=fraction;
                }
            }
        }
        query_fraction=nearest;
        if(target!=UINT32_MAX)nearest=penetration.cursor+(1-penetration.cursor)*query_fraction;
        /* Every competing prop, vehicle, fragment and world/rubble check uses
         * the ORIGINAL complete ray, up to the next absolute actor contact.
         * Radius advance must not leap over cover, even behind prior actors. */
        status=scene_driller_firearm_select(shot->source,position,delta,nearest,
            &vehicle_contact,&vehicle_selected);if(status)return status;
        if(vehicle_selected)nearest=vehicle_contact.hit.fraction;
        status=scene_clutter_precision_select(position,delta,nearest,&prop);if(status)return status;
        if(prop.slot!=UINT32_MAX)nearest=prop.fraction;
        if(rf_scene_combat_trace) {
            uint32_t uid=target==UINT32_MAX?UINT32_MAX:campaign_seeds.records.items[target].record.uid;
            printf("SHOT_RAY %u %u %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",frame,shot->slot,pellet,uid,nearest,
                position[0],position[1],position[2],delta[0],delta[1],delta[2]);
            if(target!=UINT32_MAX) {
                const rf_physics_body *body=&campaign_npc_bodies[target].body;
                for(uint32_t n=0;n<body->spheres.count;n++) {
                    const rf_physics_sphere *sphere=body->spheres.items+n;float center[3];
                    for(uint32_t k=0;k<3;k++)center[k]=(float)((double)body->state.position[k]+(double)sphere->center[0]*body->state.orientation[k]+
                        (double)sphere->center[1]*body->state.orientation[3+k]+(double)sphere->center[2]*body->state.orientation[6+k]);
                    printf("SHOT_SHAPE %u %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",frame,uid,n,
                        center[0],center[1],center[2],sphere->radius,body->state.bounds.minimum[0],body->state.bounds.minimum[1],body->state.bounds.minimum[2],
                        body->state.bounds.maximum[0],body->state.bounds.maximum[1],body->state.bounds.maximum[2]);
                }
            }
        }
        {
            rf_geomod_registry_hit hit;uint32_t matched;
            ++rf_scene_detached_hitscan[0];
            status=scene_detached_sources_sweep(stream,0x460,position,delta,0,nearest,&hit,&matched);
            rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
            if(matched && (target==UINT32_MAX || hit.piece.hit.fraction<nearest)) {
                float contact_end[3];uint32_t wall;
                for(i=0;i<3;i++)contact_end[i]=position[i]+delta[i]*hit.piece.hit.fraction;
                status=rf_geometry_collision_ray(stream->collision,&campaign_movers,position,contact_end,0x27,NULL,&wall);
                if(status)return status;
                if(!wall) {
                    status=scene_detached_sources_damage(stream,hit.batch,hit.piece.piece,damage);
                    rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
                    ++rf_scene_detached_hitscan[1];rf_scene_detached_hitscan[3]=hit.batch;rf_scene_detached_hitscan[4]=hit.piece.piece;
                    rf_scene_detached_hitscan[5]=npc_hash_bytes(2166136261u,hit.piece.hit.point,12);combat_surface_frame=frame;
                    scene_weapon_impact_audio_contact(shot->weapon,-1,hit.piece.hit.point);
                }else scene_weapon_impact_audio_world(stream,shot->weapon,position,delta,hit.piece.hit.fraction);
                return RF_OK;
            }
        }
        {uint32_t handled;
            status=scene_clutter_precision_commit(stream,position,delta,&prop,damage,shot->kind,shot->weapon,&handled);
            if(status)return status;if(handled)return RF_OK;
        }
        if(vehicle_selected) {
            uint32_t handled;float applied;
            status=combat_shot_obstructed(stream,position,delta,nearest,&blocked);if(status)return status;
            if(!blocked) {
                uint32_t audio_contact=scene_weapon_impact_audio_ready(shot->weapon) &&
                    scene_driller_projectile_contact_live(&vehicle_contact,shot->source);
                status=scene_driller_projectile_damage(&vehicle_contact,shot->source,damage,
                    shot->kind,frame,&handled,&applied);if(status)return status;
                if(handled && audio_contact)scene_weapon_impact_audio_contact(shot->weapon,-1,vehicle_contact.hit.point);
            }else scene_weapon_impact_audio_world(stream,shot->weapon,position,delta,nearest);
            return RF_OK;
        }
        if(target==UINT32_MAX) {
            scene_weapon_impact_audio_world(stream,shot->weapon,position,delta,nearest);return RF_OK;
        }
        status=combat_shot_obstructed(stream,position,delta,nearest,&blocked);if(status)return status;
        if(blocked){scene_weapon_impact_audio_world(stream,shot->weapon,position,delta,nearest);return RF_OK;}
        {
            campaign_npc_body *owner=campaign_npc_bodies+target;uint32_t entered,clock_bits,material;
            float amount,point[3],seconds=(float)frame/60;
            rf_damage_request request={shot->base,shot->source,shot->kind,0,UINT32_MAX,0};
            combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
            rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
                combat_random,combat_notify,combat_playing,combat_play,&feedback};
            if(rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
               !owner->registration.view || !owner->body.allocated_bytes || owner->damage.effects.health<=0 ||
               (owner->object_flags&(2|0x4000)) || (owner->view.flags_810&1))return RF_OK;
            if(scene_npc_shields.owners) {
                uint32_t accepted,broken;
                status=shield_selected?scene_npc_shield_commit(&shield_candidate,actor_position,end,
                    request.amount,request.kind,&accepted,&broken):
                    scene_npc_shield_receive(target,actor_position,end,query_fraction,
                    request.amount,request.kind,&accepted,&broken);if(status)return status;
                if(accepted)return RF_OK;
            }
            /* Preserve ordinary receive/commit fallback only for a real body
             * already qualified by the checked segment. A rejected shield-only
             * token, or a body beyond that surface, cannot fabricate a contact. */
            if(!body_selected || body_fraction>query_fraction)return RF_OK;
            query_fraction=body_fraction;
            nearest=penetration.cursor+(1-penetration.cursor)*query_fraction;
            {uint32_t consumed;status=scene_nano_weapon_contact(owner,shot->weapon,request.amount,&consumed);
                if(status)return status;if(consumed){combat_hit_frame=frame;return RF_OK;}}
            for(i=0;i<3;i++)point[i]=actor_position[i]+actor_delta[i]*query_fraction;
            material=owner->collision_material;
            /* Shields/nano keep fresh unattenuated base. Snapshot actual body
             * radius and prepare continuation BEFORE damage/death callbacks.
             * Even zero applied damage spends this contact's original power. */
            request.amount=damage;
            status=scene_sniper_penetration_body(&penetration,handle,owner->body.state.bounds.radius,
                nearest,point,end,&more);if(status)return status;
            memcpy(&clock_bits,&seconds,4);
            status=rf_scene_npc_damage(handle,&request,1,clock_bits,&effects,&amount);
            if(!status)status=feedback.status;if(status)return status;
            scene_weapon_impact_audio_contact(shot->weapon,(int32_t)material,point);
            if(amount>0) {
                combat_hit_frame=frame;campaign_combat_event(frame,0,handle,amount,owner->damage.effects.health);
                if(shot->alternate && shot->slot==1)++rf_scene_rifle_alt[1];
                if(!campaign_npc_ai_suppressed(owner) && !owner->combat_alert && owner->view.weapons[0]>=0) {
                    owner->combat_alert=1;owner->combat_burst_remaining=0;owner->combat_due=frame+30;++rf_scene_enemy_combat[1];
                }
                if(owner->damage.effects.health>0 && owner->combat_alert && campaign_player_form.active) {
                    campaign_player_form.compromised=1;rf_scene_player_form[2]=1;
                }
            }
            ++rf_scene_combat[1];rf_scene_combat[3]=handle;memcpy(rf_scene_combat+4,&owner->damage.effects.health,4);
            if(owner->damage.effects.health<=0) {
                status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
                if(entered) {
                    ++rf_scene_combat[2];if(shot->alternate && shot->slot==1)++rf_scene_rifle_alt[2];
                    status=combat_death_start(target);if(status)return status;
                }
            }
        }
        if(!more)return RF_OK;
    }
}
#endif
