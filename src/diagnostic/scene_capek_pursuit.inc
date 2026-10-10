#ifndef RF_SCENE_CAPEK_PURSUIT_INCLUDED
#define RF_SCENE_CAPEK_PURSUIT_INCLUDED
/* Include beside the other exact XYZ consumers before campaign_pursuit_target.
 * Original Capek authors hover12, accepted by42a060; hover's Y translation
 * is parent-space. Keep authored/current mode12: Slow selecting another mode
 * must not inherit this admission. No general hover or Make_Fly support.
 * Target refresh can begin pursuit; movement separately requires follow2.
 * Full-target steering is intentional:40b8af..40b910 flattens only mode9/7
 * or class0x400000, absent from the original Capek profile. */
static uint32_t scene_capek_pursuit_owner(const campaign_npc_body *owner)
{
    uint32_t index,cls;
    if(!owner || owner->capek_shield_broken || !campaign_npc_bodies || !campaign_seeds.items ||
       !campaign_seeds.records.items || !campaign_seeds.classes)return 0;
    index=(uint32_t)(owner-campaign_npc_bodies);
    if(index>=campaign_npc_body_count || index>=campaign_seeds.records.count ||
       owner->movement_slot>=16 || campaign_modes[owner->movement_slot].index!=12)return 0;
    cls=campaign_seeds.items[index].class_index;
    return cls<campaign_seeds.class_count &&
        campaign_seeds.classes[cls].physics.movement_index==12 &&
        !strcmp(campaign_seeds.records.items[index].record.class_name,"Capek");
}
#endif
