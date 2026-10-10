#ifndef RF_SCENE_MEDIC_SYRINGE_BLAST_INCLUDED
#define RF_SCENE_MEDIC_SYRINGE_BLAST_INCLUDED
/*4109d4..4109fd unconditionally links the generated type4 into the clutter
 * family.488ecc..488f19 scans that family with hidden/AABB gates, without a
 * held/carry/physics0x20 exclusion;489532 dispatches type4 health to410270.
 * Preserve the existing shared CF5 cover and exact ordinary falloff owner.
 * Do not add weapon/object collision flags: actual can_carry alone is0x80. */
static int scene_medic_syringe_blast_view(void *context,uint32_t actor,
    scene_clutter_blast_candidate *out,uint32_t *present)
{
    rf_clutter_base_owner *child=scene_medic_syringe_child_at(actor);(void)context;*present=0;
    if(!child||!out)return RF_OK;
    if(child->state.class_index<0||(uint32_t)child->state.class_index>=campaign_clutter_classes.count||
       !campaign_clutter_damage_profiles[child->state.class_index].ordinary)return RF_FORMAT;
    out->handle=child->state.handle;out->flags=child->state.flags;out->health=child->state.health;
    memcpy(out->position,child->body.state.position,12);out->extent=0;*present=1;return RF_OK;
}
static int scene_medic_syringe_blast_damage(void *context,const scene_clutter_blast_candidate *candidate,
    float amount,int32_t kind,uint32_t source)
{
    uint32_t i;(void)context;(void)source;
    for(i=0;i<campaign_npc_body_count;i++){
        rf_clutter_base_owner *child=scene_medic_syringe_child_at(i);
        if(child&&child->state.handle==candidate->handle)return scene_medic_syringe_damage(i,amount,kind);
    }
    return RF_NOT_FOUND;
}
#endif
