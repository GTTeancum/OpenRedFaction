/* Include after campaign_enemy_tick and scene_turret_combat.inc. Compose the
 * same owned body/shield/vehicle/clutter/GeoMod/damage services as NPC hitscan;
 * no temporary NPC owner or separate damage formula is manufactured. */
typedef struct scene_turret_scene_context {scene_stream *stream;uint32_t frame;float player_eye[3];} scene_turret_scene_context;
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
        if(!scene_turret_scene_npc_live(index-1)||scene_turret_operator_excludes(source,npc->registration.handle))return RF_NOT_FOUND;
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
static int scene_turret_scene_shot(void *context,uint32_t source,uint32_t intended,int32_t weapon,
    const float eye[3],const float muzzle[3],const float ray[3],const rf_weapon_primary_definition *definition)
{
    scene_turret_scene_context *c=context;scene_turret_owner *shooter=scene_turret_lookup(source);
    scene_ai_shield_ray_selection npc_shield={0};scene_player_shield_candidate player_shield={0};
    rf_weapon_flight_contact vehicle_contact={0};uint32_t i,blocked,vehicle=0,kind=0,slot=UINT32_MAX;
    uint32_t body_hit=0,clock;float spread[3],end[3],fraction=1,body_fraction=1,damage,amount=0;
    int status;float seconds;combat_feedback feedback;
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    (void)intended; /* The actual nearest hit, not the intended target, owns damage. */
    if(!c || !c->stream || !shooter || !definition || !eye || !muzzle || !ray ||
       weapon!=scene_turret_vauss_id || definition->damage_kind<0 || definition->damage_kind>8)return RF_RANGE;
    damage=combat_enemy_primary_damage(definition);if(!isfinite(damage) || damage<0)return RF_FORMAT;
    seconds=(float)c->frame/60;memcpy(&clock,&seconds,4);
    feedback=(combat_feedback){(int32_t)((uint64_t)c->frame*1000/60%RF_TIMER_PERIOD),0};
    status=rf_weapon_spread_ray(ray,definition->ai_spread_degrees,&campaign_enemy_spread_random,spread);if(status)return status;
    for(i=0;i<3;i++)end[i]=eye[i]+spread[i];
    ++rf_scene_turret_shots[0];++rf_scene_enemy_spread[0];
    if(definition->ai_spread_degrees>0)++rf_scene_enemy_spread[1];rf_scene_enemy_spread[5]=campaign_enemy_spread_random.value;
    {const char *auto_head="Auto Turret Head";
     const char *name=campaign_seeds.records.items[shooter->seed].record.class_name;
     combat_sound(rf_emitter_name_lookup(&auto_head,1,name)==0?"Vauss2 Fire":"Vauss5 Fire",muzzle);}
    /* Each candidate combines its body and held shield before comparing with
     * other actors. This preserves outstretched shields outside coarse spheres. */
    if(scene_turret_scene_player_live() && campaign_player_object.handle!=source) {
        float hit_fraction=1;uint32_t shield_hit;
        body_hit=combat_body(eye,spread,&scene_actor_body,1,&hit_fraction);body_fraction=hit_fraction;
        if(body_hit){fraction=hit_fraction;kind=1;}
        status=scene_player_shield_query(c->stream,c->player_eye,actor_look.eye_orientation,eye,end,1,&player_shield,&shield_hit);if(status)goto fail;
        if(shield_hit){fraction=player_shield.hit.time;kind=3;}
    }
    for(i=0;i<campaign_npc_body_count;i++) {
        scene_ai_shield_ray_selection shield={0};float hit_fraction=1;uint32_t hit;
        const campaign_npc_body *npc=campaign_npc_bodies+i;
        if(!scene_turret_scene_npc_live(i) || npc->registration.handle==source || scene_turret_operator_excludes(source,npc->registration.handle))continue;
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
        status=scene_driller_projectile_damage(&vehicle_contact,source,damage,definition->damage_kind,c->frame,&handled,&amount);if(status)goto fail;
        rf_scene_turret_shots[3]+=handled;rf_scene_turret_shots[1]+=amount>0;
        rf_scene_turret_shots[6]=vehicle_contact.face;return RF_OK;
    }
    {rf_damage_request request={damage,source,definition->damage_kind,0,UINT32_MAX,0};uint32_t accepted=0,broken;
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
    if(!scene_turret_count)return RF_OK;
    if(!stream || !stream->collision || campaign_npc_body_count>=1024)return RF_RANGE;
    if(rf_scene_swim_test_enabled)return RF_OK; /* Same quiet movement-fixture rule as NPC AI. */
    /* Frame zero primes the scene; its ordinary load commits after callbacks.
     * Do not acquire/fire from authored state before restored owners publish. */
    if(!frame){memset(rf_scene_turret_shots,0,sizeof(rf_scene_turret_shots));return RF_OK;}
    /* actor_follow_view publishes this gameplay listener before combat, ahead
     * of diagnostic view overrides. Do not advance actor_listener_pose twice. */
    memcpy(context.player_eye,campaign_listener_position,12);
    backend=(scene_turret_combat_backend){campaign_npc_body_count+1,scene_turret_scene_target,
        scene_turret_scene_blocked,scene_turret_scene_shot,&context};
    return scene_turret_combat_tick(frame,&backend);
}
