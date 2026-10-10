#ifndef RF_SCENE_AUTHORED_HOLSTER_INCLUDED
#define RF_SCENE_AUTHORED_HOLSTER_INCLUDED
/* Original Holster_Weapon64 ->4b9980. Include after registered NPC owners
 * and before campaign_enemy_tick. This changes no inventory, selection,
 * animation, AI order or cooldown. The retained world draw consumes both
 * bits through rf_weapon_world_visibility. Player event65 is separate. */
uint32_t rf_scene_authored_holster[4]; /* calls, applied, last UID, fire attempts held */
static int campaign_holster_npc_weapon(void *context,uint32_t handle)
{
    uint32_t i;(void)context;++rf_scene_authored_holster[0];
    for(i=0;i<campaign_npc_body_count;i++) {
        campaign_npc_body *owner=campaign_npc_bodies+i;
        if(owner->registration.handle!=handle || owner->registration.view!=&owner->view ||
           rf_object_registry_lookup(&campaign_registry,handle)!=&owner->registration ||
           rf_entity_lookup(&campaign_entities,(int32_t)handle)!=&owner->view)continue;
        /* Only the ordinary living NPC owner is represented by this adapter. */
        if(owner->damage.effects.health<=0 || (owner->view.flags_810&1u) || (owner->object_flags&2u))return RF_NOT_FOUND;
        scene_ai_drone_smash_cancel_source(handle);
        owner->view.flags_810|=0x800u;owner->view.flags_7d0|=0x200u;
        owner->damage.effects.flags_810|=0x800u;
        owner->firing.flags_810|=0x800u;owner->firing.flags_7d0|=0x200u;
        ++rf_scene_authored_holster[1];rf_scene_authored_holster[2]=(uint32_t)campaign_seeds.records.items[i].record.uid;
        return RF_OK;
    }
    return RF_NOT_FOUND;
}
/* Original425830 rejects428e60 before firing.428e60 exempts an entity linked
 * to a class4 turret;7d0/200 is independently a world-render exclusion only.
 * Call at actual shot admission, after ordinary awareness/movement/reload. */
static uint32_t campaign_npc_holster_blocks_fire(const campaign_npc_body *owner)
{
    const rf_entity_view *linked;
    if(!(owner->view.flags_810&0x800u))return 0;
    linked=rf_entity_lookup(&campaign_entities,owner->view.linked_handle);
    if(linked && linked->class_type==4)return 0;
    ++rf_scene_authored_holster[3];return 1;
}
#endif
