#ifndef RF_SCENE_AI_CONTACT_MATERIAL_INCLUDED
#define RF_SCENE_AI_CONTACT_MATERIAL_INCLUDED
/* Include immediately after scene_ai_laser.inc. Shared optional feedback
 * reads the ALREADY retained contact before damage can retire an owner.
 * This is only identity/material qualification: no sweep, damage callback,
 * Nano prepass, weapon admission, liquid policy, RNG or sound dispatch.
 * Each family retains its own readiness and dry/underwater policy.
 * A qualified immune contact remains eligible; stale generations do not.
 * Missing material metadata selects catalog Default-1, never guessed Flesh. */
static uint32_t scene_ai_contact_material(scene_stream *stream,
    const scene_ai_laser_projectile *shot,const scene_ai_laser_query *q,
    const rf_weapon_flight_contact *contact,int32_t *material)
{
    campaign_npc_body *npc=NULL;*material=-1;
    if(q->kind==SCENE_LASER_WORLD) {
        uint32_t solid=UINT32_MAX,texture,surface;
        if(contact->object!=UINT32_MAX) {
            if((contact->object&0xffff0000u)!=SCENE_MOVER_ROCKET_OWNER)return 0;
            solid=contact->object&0xffffu;if(solid>=campaign_movers.count)return 0;
        }
        if(!scene_detached_contact_material(NULL,solid,contact->face,&texture,&surface))
            *material=(int32_t)surface;
        return 1;
    }
    if(q->kind==SCENE_LASER_PIECE) {
        rf_geomod_piece_batch *batch=NULL;uint32_t tag=(contact->object>>16)&0x7fffu;
        rf_geomod_piece_registry *registry=scene_detached_source_registry(stream,scene_detached_tag_source(tag));
        if(rf_geomod_piece_registry_get(registry,scene_detached_tag_batch(tag),&batch))return 0;
        /* Registry owns the piece material, but this public retained-contact
         * view does not expose it. Default is explicit until that is retained. */
        return rf_geomod_piece_batch_alive(batch,contact->object&0xffffu);
    }
    if(q->kind==SCENE_LASER_CLUTTER) {
        uint32_t slot=contact->object&0xffffu;rf_clutter_base_owner *owner;
        if((contact->object&0xffff0000u)!=SCENE_CLUTTER_PROJECTILE_OWNER ||
           !campaign_clutter_bodies || slot>=campaign_clutter_records.count ||
           !(owner=campaign_clutter_bodies[slot]) || owner->state.handle!=contact->face ||
           rf_object_registry_lookup(&campaign_registry,contact->face)!=&owner->state ||
           (owner->state.flags&(2u|0x4000u)))return 0;
        *material=(int32_t)owner->material;return 1;
    }
    if(q->kind==SCENE_LASER_VEHICLE)
        return scene_driller_projectile_material(stream,contact,shot->source,material);
    if(q->target_slot==UINT32_MAX) {
        if(!scene_ai_actor_penetration_player_live(q->target))return 0;
    } else {
        npc=scene_ai_melee_npc_live(q->target_slot,q->target);if(!npc)return 0;
    }
    if(q->kind==SCENE_LASER_BODY) {
        *material=npc?(int32_t)npc->collision_material:(int32_t)campaign_player_material;return 1;
    }
    if(q->kind==SCENE_LASER_PLAYER_SHIELD) {
        const scene_player_shield_state *shield=&scene_player_shield_damage_state;
        if(npc || q->player_shield.player!=q->target || !shield->owned ||
           !shield->selected || shield->broken || !(shield->damage.health>0))return 0;
    } else if(q->kind==SCENE_LASER_NPC_SHIELD) {
        const scene_riot_shield_owner *shield;
        if(!npc || !scene_npc_shields.owners || q->target_slot>=scene_npc_shields.count ||
           scene_npc_shields.weapon<0 || scene_npc_shields.weapon>=64 ||
           npc->view.weapons[0]!=scene_npc_shields.weapon || !npc->inventory.owned[scene_npc_shields.weapon] ||
           q->npc_shield.kind!=SCENE_AI_SHIELD_RAY_SHIELD || q->npc_shield.shield.handle!=q->target)return 0;
        shield=scene_npc_shields.owners+q->target_slot;
        if(shield->initialized && shield->handle==q->target && !(shield->damage.health>0))return 0;
    } else return 0;
    /* The physical shield's authored clutter material, not the actor body.
     * Original riot_shield is Solid6; never hardcode that index here. */
    if(campaign_riot_shield_class>=0 && (uint32_t)campaign_riot_shield_class<campaign_clutter_classes.count &&
       campaign_clutter_classes.items)*material=(int32_t)campaign_clutter_classes.items[campaign_riot_shield_class].material;
    return 1;
}
#endif
