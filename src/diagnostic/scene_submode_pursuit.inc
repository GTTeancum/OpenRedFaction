#ifndef RF_SCENE_SUBMODE_PURSUIT_INCLUDED
#define RF_SCENE_SUBMODE_PURSUIT_INCLUDED
/* Include immediately before campaign_pursuit_target. The callers pass a
 * retained campaign owner. This bounded consumer is only Sea_Creature with
 * authored/current sub mode7; no general swim, vehicle or Make_Fly adapter.
 * Do not require follow2 here: target refresh also starts a new pursuit. */
static uint32_t scene_submode_pursuit_owner(const campaign_npc_body *owner)
{
    uint32_t index,cls;
    if(!owner || !campaign_npc_bodies || !campaign_seeds.items ||
       !campaign_seeds.records.items || !campaign_seeds.classes)return 0;
    index=(uint32_t)(owner-campaign_npc_bodies);
    if(index>=campaign_npc_body_count || index>=campaign_seeds.records.count ||
       owner->movement_slot>=16 || campaign_modes[owner->movement_slot].index!=7)return 0;
    cls=campaign_seeds.items[index].class_index;
    return cls<campaign_seeds.class_count &&
        campaign_seeds.classes[cls].physics.movement_index==7 &&
        !strcmp(campaign_seeds.records.items[index].record.class_name,"Sea_Creature");
}
/* Shared by exact Sea_Creature sub7, Drone fly11 and Capek hover12 consumers.
 * Caller keeps the existing seat and scripted-physics suspension gates.
 * Refused exact owners stay still rather than falling through to the
 * horizontal movement path while hidden, frozen, terminal or stale. */
static uint32_t scene_xyz_pursuit_live(const campaign_npc_body *owner)
{
    return owner->registration.view==&owner->view &&
        rf_object_registry_lookup(&campaign_registry,owner->registration.handle)==&owner->registration &&
        rf_entity_lookup(&campaign_entities,(int32_t)owner->registration.handle)==&owner->view &&
        owner->view.type==0 && owner->damage.effects.health>0 &&
        !(owner->object_flags&(2u|0x4000u)) && !(owner->view.flags_810&1u) &&
        owner->view.linked_handle==-1;
}
#endif
