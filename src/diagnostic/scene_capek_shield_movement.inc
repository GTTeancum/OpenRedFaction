#ifndef RF_SCENE_CAPEK_SHIELD_MOVEMENT_INCLUDED
#define RF_SCENE_CAPEK_SHIELD_MOVEMENT_INCLUDED
/* Immutable placed-class qualification, independent of live descriptor or
 * selected weapon. Slow/run/fall and retired owners retain the same identity.
 * This is not an armor-zero inference: authored zero and Event76 OFF can also
 * exhaust ordinary armor without the original427df0 shield-break call. */
static uint32_t scene_capek_shield_movement_owner(const campaign_npc_body *owner)
{
    uint32_t index,cls;
    if(!owner || !campaign_npc_bodies || !campaign_seeds.items ||
       !campaign_seeds.records.items || !campaign_seeds.classes)return 0;
    index=(uint32_t)(owner-campaign_npc_bodies);
    if(index>=campaign_npc_body_count || index>=campaign_seeds.records.count)return 0;
    cls=campaign_seeds.items[index].class_index;
    return cls<campaign_seeds.class_count &&
        campaign_seeds.classes[cls].physics.movement_index==12 &&
        (campaign_seeds.classes[cls].physics.flags&0x02000000u) &&
        !strcmp(campaign_seeds.records.items[index].record.class_name,"Capek");
}
/* New broken-state continuations can be falling, normalized/Slow run, or
 * the existing disabled-descriptor fallback0. No new-mode save admission. */
static uint32_t scene_capek_broken_movement_slot(uint32_t slot)
{
    return slot<16 && (!slot || campaign_modes[slot].index==1 || campaign_modes[slot].index==3);
}
static uint32_t scene_npc_normal_movement_index(const campaign_npc_body *owner,uint32_t cls)
{
    return owner->capek_shield_broken && scene_capek_shield_movement_owner(owner)?1u:
        campaign_seeds.classes[cls].physics.movement_index;
}
/* 422aed..422af0 retains the authored base;422b23 installs8.0. The shared
 * table remains that retained authored value here. Callers supply the explicit
 * live OR candidate break latch and a validated matching class, never armor,
 * Event76 state or the current descriptor. No per-owner config allocation. */
static float scene_npc_effective_base_speed(const campaign_npc_body *owner,uint32_t cls,uint32_t broken)
{
    return !broken && scene_capek_shield_movement_owner(owner)?8.0f:
        campaign_npc_movement_configs[cls].base_speed;
}
static int scene_npc_resolve_speed(const campaign_npc_body *owner,uint32_t cls,uint32_t broken,
    int32_t requested,int32_t forced_action,rf_movement_settings *movement)
{
    rf_movement_config config;
    if(!owner || !campaign_npc_movement_configs || cls>=campaign_seeds.class_count || broken>1)return RF_RANGE;
    config=campaign_npc_movement_configs[cls];
    config.base_speed=scene_npc_effective_base_speed(owner,cls,broken);
    return rf_movement_set_mode(movement,&config,requested,forced_action,owner->body.state.mass,0);
}
/* 4c5af7..4c5afa ->427df0(owner,1):4281a0 installs fall3/body bit1 and
 * identity movement orientation. Class-name mutation to run becomes this
 * owner's retained normal default. The authored class/speed stays immutable.
 * Call before publishing the exhausted armor so fall admission failure leaves
 * both durability and the new latch unchanged. No attack/cadence reset. */
static int scene_capek_shield_break(campaign_npc_body *owner)
{
    uint32_t cls;int status;
    if(!scene_capek_shield_movement_owner(owner))return RF_OK;
    if(owner->registration.view!=&owner->view ||
       rf_object_registry_lookup(&campaign_registry,owner->registration.handle)!=&owner->registration ||
       rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)!=&owner->view ||
       owner->view.type!=0 || !(owner->damage.effects.health>0) ||
       (owner->view.flags_810&1u))return RF_NOT_FOUND;
    if(!campaign_npc_movement_configs)return RF_RANGE;
    cls=campaign_seeds.items[(uint32_t)(owner-campaign_npc_bodies)].class_index;
    status=rf_scene_npc_fall(owner->registration.handle);if(status)return status;
    owner->capek_shield_broken=1;
    owner->view.base_speed=scene_npc_effective_base_speed(owner,cls,1);
    /* 427e66 restores class+50 only. Keep live speed+8c0/mode+8c4 until the
     * ordinary Slow/Normal setter (including landing) resolves authored speed. */
    return RF_OK;
}
#endif
