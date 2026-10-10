#ifndef RF_SCENE_DRONE_PURSUIT_INCLUDED
#define RF_SCENE_DRONE_PURSUIT_INCLUDED
/* Include beside scene_submode_pursuit.inc before campaign_pursuit_target.
 * Exact original Drone authors robot fly11, also accepted by42a060. Keep
 * authored/current descriptors: this is not general Make_Fly/free-flight.
 * Target refresh can begin a pursuit, so follow2 is checked by movement only.
 * Full-target steering is intentional:40b8af..40b910 flattens descriptor9/7
 * or class0x400000, none of which applies to this original Drone profile. */
static uint32_t scene_drone_pursuit_owner(const campaign_npc_body *owner)
{
    uint32_t index,cls;
    if(!owner || !campaign_npc_bodies || !campaign_seeds.items ||
       !campaign_seeds.records.items || !campaign_seeds.classes)return 0;
    index=(uint32_t)(owner-campaign_npc_bodies);
    if(index>=campaign_npc_body_count || index>=campaign_seeds.records.count ||
       owner->movement_slot>=16 || campaign_modes[owner->movement_slot].index!=11)return 0;
    cls=campaign_seeds.items[index].class_index;
    return cls<campaign_seeds.class_count &&
        campaign_seeds.classes[cls].physics.movement_index==11 &&
        !strcmp(campaign_seeds.records.items[index].record.class_name,"Drone");
}
#endif
