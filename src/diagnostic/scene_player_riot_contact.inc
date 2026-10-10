#ifndef RF_SCENE_PLAYER_RIOT_CONTACT_INCLUDED
#define RF_SCENE_PLAYER_RIOT_CONTACT_INCLUDED
/* Shared finite contact walk, retaining the primary ray's exact population,
 * ordering and effects. A caller-owned immutable record supplies accepted
 * source/weapon/mode/amount/kind; an owner-specific guard checks its ticket and
 * live identity around callbacks. No current input or selected profile chooses
 * mode/damage here. The CURRENT eye/direction comes from timer service. */
typedef struct scene_player_riot_contact_record {
    uint32_t source,mode;int32_t weapon,kind;float damage;
    uint32_t (*current)(const void *);const void *context;
} scene_player_riot_contact_record;
static uint32_t scene_player_riot_contact_current(
    const scene_player_riot_contact_record *contact)
{return contact && contact->current && contact->current(contact->context);}

/* Retain the complete generation-bearing handle, not an array pointer/slot,
 * across shield, damage, sound or death callbacks. Dead-but-retained actors
 * remain eligible for the damage/death epilogue; removed/hidden ones do not.
 */
static uint32_t scene_player_riot_contact_npc_identity(const campaign_npc_body *npc,
    uint32_t handle,uint32_t living)
{
    return npc && handle && handle!=UINT32_MAX && npc->registration.handle==handle &&
        npc->registration.view==&npc->view && (uint32_t)npc->view.handle==handle &&
        npc->damage.effects.handle==handle && npc->body.allocated_bytes &&
        !(npc->object_flags&(2u|0x4000u)) &&
        rf_object_registry_lookup(&campaign_registry,handle)==&npc->registration &&
        rf_entity_lookup(&campaign_entities,(int32_t)handle)==&npc->view &&
        (!living || (npc->damage.effects.health>0 && !(npc->view.flags_810&1u)));
}
static campaign_npc_body *scene_player_riot_contact_npc(uint32_t handle,
    uint32_t living,uint32_t *slot)
{
    uint32_t i;
    if(!handle || handle==UINT32_MAX || !campaign_npc_bodies)return NULL;
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *npc=campaign_npc_bodies+i;
        if(npc->registration.handle!=handle)continue;
        if(!scene_player_riot_contact_npc_identity(npc,handle,living))return NULL;
        if(slot)*slot=i;
        return npc;
    }
    return NULL;
}

static int scene_player_riot_melee_contact(scene_stream *stream,
    const scene_player_riot_contact_record *contact,uint32_t frame,
    const float position[3],const float direction[3])
{
    scene_npc_shield_candidate shield_candidate={0};
    rf_weapon_flight_contact vehicle_contact;
    const float damage=contact?contact->damage:0;
    const int32_t kind=contact?contact->kind:-1;
    float eye[3],delta[3],end[3],nearest=1,amount;
    uint32_t i,target=UINT32_MAX,handle=UINT32_MAX,blocked;
    uint32_t shield_selected=0,vehicle_selected=0,clutter_selected=UINT32_MAX;
    uint32_t clutter_handle=UINT32_MAX;int status;
    if(!stream || !contact || !contact->current || !position || !direction)
        return RF_RANGE;
    if(contact->mode && contact->mode!=SCENE_RIOT_ALTERNATE_MODE)return RF_FORMAT;
    if(!scene_player_riot_contact_current(contact))return RF_OK;
    if(!isfinite(damage) || damage<0 || kind< -1 || kind>10)return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(direction[i]))return RF_RANGE;
        eye[i]=position[i];delta[i]=direction[i]*2.6f;end[i]=eye[i]+delta[i];
    }
    position=eye;

    /* Original body-before-shield selection, including strictly-nearer ties. */
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *npc=campaign_npc_bodies+i;
        uint32_t candidate_handle=npc->registration.handle;
        float fraction,actor_limit=nearest;
        if(!scene_player_riot_contact_npc_identity(npc,candidate_handle,1))continue;
        if(combat_body(position,delta,&npc->body,nearest,&fraction) && fraction<nearest) {
            nearest=fraction;target=i;handle=candidate_handle;shield_selected=0;
        }
        if(scene_npc_shields.owners) {
            uint32_t hit;scene_npc_shield_candidate candidate;
            status=scene_npc_shield_query(i,position,end,actor_limit,&candidate,&hit);
            if(status)return status;
            if(!scene_player_riot_contact_current(contact))return RF_OK;
            if(hit && target==i && nearest<candidate.hit.time) {
                uint32_t body_before;
                status=scene_npc_shield_body_before(&candidate,position,end,&body_before);
                if(status)return status;
                if(!scene_player_riot_contact_current(contact))return RF_OK;
                if(body_before)hit=0;
            }
            if(hit && (target==i || candidate.hit.time<nearest)) {
                nearest=candidate.hit.time;target=i;handle=candidate.handle;
                shield_candidate=candidate;shield_selected=1;
            }
        }
    }
    status=scene_driller_firearm_select(contact->source,position,delta,nearest,
        &vehicle_contact,&vehicle_selected);if(status)return status;
    if(!scene_player_riot_contact_current(contact))return RF_OK;
    if(vehicle_selected)nearest=vehicle_contact.hit.fraction;
    status=campaign_clutter_firearm_select(position,delta,nearest,&clutter_selected,&nearest);
    if(status)return status;
    if(!scene_player_riot_contact_current(contact))return RF_OK;
    if(clutter_selected!=UINT32_MAX) {
        if(!campaign_clutter_bodies || clutter_selected>=campaign_clutter_records.count ||
           !campaign_clutter_bodies[clutter_selected])return RF_OK;
        clutter_handle=campaign_clutter_bodies[clutter_selected]->state.handle;
    }
    if(rf_scene_combat_trace) {
        campaign_npc_body *npc=target==UINT32_MAX?NULL:
            scene_player_riot_contact_npc(handle,1,&target);
        uint32_t uid=npc && target<campaign_seeds.records.count?
            campaign_seeds.records.items[target].record.uid:UINT32_MAX;
        printf("SHOT_RAY %u %u %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
            frame,2u,0u,uid,nearest,position[0],position[1],position[2],delta[0],delta[1],delta[2]);
        if(npc) {
            const rf_physics_body *body=&npc->body;
            for(uint32_t n=0;n<body->spheres.count;n++) {
                const rf_physics_sphere *sphere=body->spheres.items+n;float center[3];
                for(uint32_t k=0;k<3;k++)center[k]=(float)((double)body->state.position[k]+
                    (double)sphere->center[0]*body->state.orientation[k]+
                    (double)sphere->center[1]*body->state.orientation[3+k]+
                    (double)sphere->center[2]*body->state.orientation[6+k]);
                printf("SHOT_SHAPE %u %u %u %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g %.9g\n",
                    frame,uid,n,center[0],center[1],center[2],sphere->radius,
                    body->state.bounds.minimum[0],body->state.bounds.minimum[1],body->state.bounds.minimum[2],
                    body->state.bounds.maximum[0],body->state.bounds.maximum[1],body->state.bounds.maximum[2]);
            }
        }
    }

    /* Keep world/mover coverage before fragment damage and consume the ray
     * even when the fragment is protected or a nearer wall blocks it. */
    {
        rf_geomod_registry_hit hit;uint32_t matched;
        ++rf_scene_detached_hitscan[0];
        status=scene_detached_sources_sweep(stream,0x460,position,delta,0,nearest,&hit,&matched);
        rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
        if(!scene_player_riot_contact_current(contact))return RF_OK;
        if(matched && (target==UINT32_MAX || hit.piece.hit.fraction<nearest)) {
            float contact_end[3];uint32_t wall;
            for(i=0;i<3;i++)contact_end[i]=position[i]+delta[i]*hit.piece.hit.fraction;
            status=rf_geometry_collision_ray(stream->collision,&campaign_movers,
                position,contact_end,0x27,NULL,&wall);if(status)return status;
            if(!scene_player_riot_contact_current(contact))return RF_OK;
            if(!wall) {
                status=scene_detached_sources_damage(stream,hit.batch,hit.piece.piece,damage);
                if(!scene_player_riot_contact_current(contact))return status;
                rf_scene_detached_hitscan[6]=(uint32_t)status;if(status)return status;
                ++rf_scene_detached_hitscan[1];rf_scene_detached_hitscan[3]=hit.batch;
                rf_scene_detached_hitscan[4]=hit.piece.piece;
                rf_scene_detached_hitscan[5]=npc_hash_bytes(2166136261u,hit.piece.hit.point,12);
                combat_surface_frame=frame;
                combat_sound("Riot Impact Default",hit.piece.hit.point);
                if(!scene_player_riot_contact_current(contact))return RF_OK;
                ++rf_scene_riot[4];
            }
            return RF_OK;
        }
    }
    if(clutter_selected!=UINT32_MAX) {
        rf_clutter_base_owner *prop;
        status=combat_shot_obstructed(stream,position,delta,nearest,&blocked);if(status)return status;
        if(!scene_player_riot_contact_current(contact))return RF_OK;
        if(blocked)return RF_OK;
        /* A stale selected prop still consumes this contact. */
        if(!campaign_clutter_bodies || clutter_selected>=campaign_clutter_records.count ||
           !(prop=campaign_clutter_bodies[clutter_selected]) || prop->state.handle!=clutter_handle ||
           (prop->state.flags&(2u|0x4000u)) ||
           rf_object_registry_lookup(&campaign_registry,clutter_handle)!=&prop->state)return RF_OK;
        /* Preserve the existing Riot retained-prop branch without added sound. */
        return campaign_clutter_firearm_damage(clutter_selected,damage,kind);
    }
    if(vehicle_selected) {
        uint32_t handled;float applied;
        status=combat_shot_obstructed(stream,position,delta,nearest,&blocked);if(status)return status;
        if(!scene_player_riot_contact_current(contact))return RF_OK;
        if(blocked)return RF_OK;
        /* contact.face carries the full vehicle/turret handle. The shared
         * damage adapter consumes stale tokens; no borrowed owner epilogue. */
        return scene_driller_projectile_damage(&vehicle_contact,contact->source,
            damage,kind,frame,&handled,&applied);
    }
    {
        rf_collision_solid_hit surface;float contact_end[3];uint32_t matched=0;
        for(i=0;i<3;i++)contact_end[i]=position[i]+delta[i]*nearest;
        status=rf_geometry_collision_ray(stream->collision,&campaign_movers,
            position,contact_end,0x26,&surface,&matched);if(status)return status;
        if(!scene_player_riot_contact_current(contact))return RF_OK;
        if(matched) {
            combat_sound("Riot Impact Default",surface.hit.point);
            if(!scene_player_riot_contact_current(contact))return RF_OK;
            ++rf_scene_riot[4];combat_surface_frame=frame;return RF_OK;
        }
    }
    if(target==UINT32_MAX)return RF_OK;
    status=combat_shot_obstructed(stream,position,delta,nearest,&blocked);if(status)return status;
    if(!scene_player_riot_contact_current(contact) || blocked)return RF_OK;
    {
        campaign_npc_body *npc=scene_player_riot_contact_npc(handle,1,&target);
        uint32_t entered,clock_bits;float seconds=(float)frame/60;
        int32_t death_hint=-1;
        rf_damage_request request={damage,contact->source,kind,0,UINT32_MAX,0};
        combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
            combat_random,combat_notify,combat_playing,combat_play,&feedback};
        if(!npc)return RF_OK;
        if(scene_npc_shields.owners) {
            uint32_t accepted,broken;
            status=shield_selected?scene_npc_shield_commit(&shield_candidate,position,end,
                request.amount,request.kind,&accepted,&broken):
                scene_npc_shield_receive(target,position,end,nearest,
                request.amount,request.kind,&accepted,&broken);if(status)return status;
            if(!scene_player_riot_contact_current(contact) || accepted)return RF_OK;
            npc=scene_player_riot_contact_npc(handle,1,&target);if(!npc)return RF_OK;
        }
        /* 4c5a90..4c5b5d: either qualified Riot mode uses primary amount
         * and consumes Nano before generic damage, including the contact
         * that exhausts its armor. */
        {uint32_t consumed;
            status=scene_nano_weapon_contact(npc,contact->weapon,request.amount,&consumed);
            if(status)return status;if(consumed)return RF_OK;
        }
        /* 4c6052..4c60a6 gates Riot + captured alternate bit 0x20.
         * 4fb990 transforms contact minus published origin by model basis;
         * positive local Z selects backward 7, otherwise forward 6.
         * Keep only a stack-local hint: living RFNC does not retain it.
         * The existing ray point approximates the original melee contact. */
        if(contact->weapon>=0 && contact->weapon==campaign_riot_id &&
           contact->mode==SCENE_RIOT_ALTERNATE_MODE && campaign_model_owners &&
           target<campaign_model_owner_count && campaign_model_owners[target].registration.loaded) {
            const float *basis=campaign_model_owners[target].basis;
            float relative[3],local_z;
            for(i=0;i<3;i++) {
                float point=position[i]+delta[i]*nearest;
                relative[i]=(float)((double)point-npc->published[i]);
            }
            local_z=(float)(((double)relative[2]*basis[8]+(double)relative[1]*basis[7])+
                (double)relative[0]*basis[6]);
            if(isfinite(local_z))death_hint=local_z>0?7:6;
        }
        memcpy(&clock_bits,&seconds,4);
        status=rf_scene_npc_damage(handle,&request,1,clock_bits,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(!scene_player_riot_contact_current(contact))return RF_OK;
        npc=scene_player_riot_contact_npc(handle,0,&target);if(!npc)return RF_OK;
        if(amount>0) {
            float impact[3];combat_hit_frame=frame;
            campaign_combat_event(frame,0,handle,amount,npc->damage.effects.health);
            if(contact->mode==SCENE_RIOT_ALTERNATE_MODE)++rf_scene_riot[3];
            for(i=0;i<3;i++)impact[i]=position[i]+delta[i]*nearest;
            combat_sound("Riot Impact Flesh",impact);
            if(!scene_player_riot_contact_current(contact))return RF_OK;
            npc=scene_player_riot_contact_npc(handle,0,&target);if(!npc)return RF_OK;
            ++rf_scene_riot[4];
            if(!campaign_npc_ai_suppressed(npc) && !npc->combat_alert && npc->view.weapons[0]>=0) {
                npc->combat_alert=1;npc->combat_burst_remaining=0;
                npc->combat_due=frame+30;++rf_scene_enemy_combat[1];
            }
            if(npc->damage.effects.health>0 && npc->combat_alert && campaign_player_form.active) {
                campaign_player_form.compromised=1;rf_scene_player_form[2]=1;
            }
        }
        ++rf_scene_combat[1];rf_scene_combat[3]=handle;
        memcpy(rf_scene_combat+4,&npc->damage.effects.health,4);
        if(npc->damage.effects.health<=0) {
            /* Lethal-only publication after the final ticket/full-handle
             * checks above. Never overwrite an already-entered death;
             * existing death selection still owns clearance and fallback. */
            if(amount>0 && death_hint!=-1 && !(npc->view.flags_810&1u))
                npc->death.action_824=death_hint;
            status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
            if(!scene_player_riot_contact_current(contact))return RF_OK;
            npc=scene_player_riot_contact_npc(handle,0,&target);if(!npc)return RF_OK;
            if(entered) {
                ++rf_scene_combat[2];
                /* Resolve the retained handle again before the slot API. No
                 * cached owner pointer or epilogue survives this callback. */
                return combat_death_start(target);
            }
        }
    }
    return RF_OK;
}

/* Primary adapter keeps its existing ticket/live contract, profile and
 * diagnostic-only due mask. No primary owner, timing or release changes. */
typedef struct scene_player_riot_primary_contact_guard {
    const scene_riot_primary_owner *owner;
    const scene_riot_primary_backend *backend;
    void *context;const scene_riot_primary_strike *strike;
} scene_player_riot_primary_contact_guard;
static uint32_t scene_player_riot_primary_contact_current(const void *context)
{
    const scene_player_riot_primary_contact_guard *guard=context;
    return guard && scene_riot_primary_contact_current(guard->owner,guard->strike) &&
        guard->backend && guard->backend->live &&
        guard->backend->live(guard->context,guard->strike->source,guard->strike->weapon);
}
static int scene_player_riot_primary_contact(scene_stream *stream,
    scene_riot_primary_owner *owner,const scene_riot_primary_backend *backend,
    void *context,const scene_riot_primary_strike *strike,uint32_t frame,
    const float position[3],const float direction[3],uint32_t due_mask)
{
    scene_player_riot_primary_contact_guard guard={owner,backend,context,strike};
    scene_player_riot_contact_record contact;
    (void)due_mask;
    if(!owner || !backend || !backend->live || !strike)return RF_RANGE;
    contact=(scene_player_riot_contact_record){strike->source,0,strike->weapon,
        campaign_primary[2].damage_kind,campaign_primary[2].damage,
        scene_player_riot_primary_contact_current,&guard};
    return scene_player_riot_melee_contact(stream,&contact,frame,position,direction);
}
#endif
