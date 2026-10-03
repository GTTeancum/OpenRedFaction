#ifndef RF_SCENE_AI_ACTOR_RAY_INC
#define RF_SCENE_AI_ACTOR_RAY_INC
/* Include before scene_ai_shotgun.inc, after both held-shield query adapters.
 * Every ordinary hitscan ray/pellet selects its actual actor independently of
 * the AI's intended target. Affiliation is not collision eligibility. Resolve
 * world/vehicle/clutter/rubble cover to fraction before committing a shield
 * or applying damage. This does not mutate AI targeting or advance animation.
 */
typedef struct scene_ai_actor_ray_selection {
    campaign_npc_body *victim; /* NULL denotes player only when actor_hit != 0. */
    uint32_t victim_slot,actor_hit,body_hit;
    float body_fraction,fraction;
    scene_ai_shield_ray_selection npc_shield;
    scene_player_shield_candidate player_shield;
    uint32_t shield_hit; /* Player shield; NPC shield uses npc_shield.kind. */
} scene_ai_actor_ray_selection;

static uint32_t scene_ai_actor_ray_related(uint32_t source,uint32_t candidate)
{
    return source==candidate || scene_npc_seat_related(source,candidate) ||
        scene_turret_generated_related(source,candidate) ||
        scene_turret_operator_excludes(source,candidate) ||
        scene_turret_operator_excludes(candidate,source);
}

static int scene_ai_actor_ray_select(scene_stream *stream,const campaign_npc_body *shooter,
    const float start[3],const float delta[3],const float player_eye[3],
    scene_ai_actor_ray_selection *out)
{
    scene_ai_actor_ray_selection selected={0};float end[3];uint32_t source,i;int status;
    if(!stream || !shooter || !start || !delta || !player_eye || !out)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(start[i]) || !isfinite(delta[i]) || !isfinite(player_eye[i]))return RF_RANGE;
        end[i]=start[i]+delta[i];if(!isfinite(end[i]))return RF_RANGE;
    }
    source=shooter->registration.handle;
    if(shooter->registration.view!=&shooter->view ||
       rf_object_registry_lookup(&campaign_registry,source)!=&shooter->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)source)!=&shooter->view)return RF_NOT_FOUND;
    selected.victim_slot=UINT32_MAX;selected.fraction=selected.body_fraction=1;
    /* Match the existing physical player body/shield path, including the
     * shield's independent query outside coarse body spheres. player_eye must
     * be the physical eye, not a mounted diagnostic/turret camera position. */
    if(campaign_player_object.view==&campaign_player_view && scene_actor_body.allocated_bytes &&
       rf_object_registry_lookup(&campaign_registry,campaign_player_object.handle)==&campaign_player_object &&
       rf_entity_lookup(&campaign_entities,(int32_t)campaign_player_object.handle)==&campaign_player_view &&
       !(campaign_player_view.flags_7c&(2u|0x4000u)) && !(campaign_player_view.flags_810&1u) &&
       campaign_player_damage.state.effects.health>0 &&
       !scene_ai_actor_ray_related(source,campaign_player_object.handle)) {
        selected.body_hit=combat_body(start,delta,&scene_actor_body,1,&selected.body_fraction);
        if(!selected.body_hit)selected.body_fraction=1;
        selected.actor_hit=selected.body_hit;selected.fraction=selected.body_fraction;
        status=scene_player_shield_query(stream,player_eye,actor_look.eye_orientation,
            start,end,1,&selected.player_shield,&selected.shield_hit);if(status)return status;
        if(selected.shield_hit){selected.actor_hit=1;selected.fraction=selected.player_shield.hit.time;}
    }
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *candidate=campaign_npc_bodies+i;
        scene_ai_shield_ray_selection shield={0};uint32_t hit;float body_fraction=1;
        if(candidate==shooter || candidate->registration.view!=&candidate->view || !candidate->body.allocated_bytes ||
           rf_object_registry_lookup(&campaign_registry,candidate->registration.handle)!=&candidate->registration ||
           rf_entity_lookup(&campaign_entities,(int32_t)candidate->registration.handle)!=&candidate->view ||
           (candidate->object_flags&(2u|0x4000u)) || (candidate->view.flags_810&1u) ||
           !(candidate->damage.effects.health>0) ||
           scene_ai_actor_ray_related(source,candidate->registration.handle))continue;
        hit=combat_body(start,delta,&candidate->body,1,&body_fraction);
        if(!hit)body_fraction=1;
        status=scene_ai_shield_ray_select(i,start,end,hit,body_fraction,&shield);if(status)return status;
        if(shield.kind==SCENE_AI_SHIELD_RAY_MISS ||
           (selected.actor_hit && shield.fraction>=selected.fraction))continue;
        /* Select body+shield together. A nearest-body-only prepass would miss
         * outstretched shields and could assign damage to the wrong actor. */
        selected.victim=candidate;selected.victim_slot=i;selected.actor_hit=1;
        selected.body_hit=hit;selected.body_fraction=body_fraction;selected.fraction=shield.fraction;
        selected.npc_shield=shield;selected.shield_hit=0;
        memset(&selected.player_shield,0,sizeof(selected.player_shield));
    }
    *out=selected;return RF_OK;
}
/* On rejected shield commit, body_hit/body_fraction are the selected actor's
 * original fallback. Recheck cover to body_fraction before body damage; a
 * shield-only contact never manufactures a body hit. Keep intended AI target
 * variables unchanged for pursuit, Attack completion and target-death tests.
 * Storage is one stack result/candidate; no persistent allocation or saves.
 */
#endif
