#ifndef RF_SCENE_CLUTTER_DEATH_QUERY_INCLUDED
#define RF_SCENE_CLUTTER_DEATH_QUERY_INCLUDED
/* When_Dead16 borrows the same ordinary prop owner used by damage, Slay,
 * Remove_Object and RFPC. Include before campaign_death_query. No lifecycle
 * operation belongs in this read-only query: retained registration is needed
 * by deferred breaks and ordinary saves after logical retirement. */
static int campaign_clutter_uid_life(uint32_t uid,uint32_t *present,uint32_t *alive)
{
    uint32_t i;
    if(!present || !alive)return RF_RANGE;
    if(!campaign_clutter_records.items || !campaign_clutter_bodies)
        return RF_NOT_FOUND;
    for(i=0;i<campaign_clutter_records.count;i++) {
        const rf_clutter_base_owner *owner;
        const scene_clutter_damage_binding *binding;
        const scene_clutter_damage_profile *profile;
        uint32_t living;
        if(campaign_clutter_records.items[i].uid!=uid)continue;
        owner=campaign_clutter_bodies[i];
        /* An authored placement without an admitted owner is unknown, not
         * destroyed. A stale generation must not turn into a false death. */
        if(!owner || !scene_clutter_damage_identity(&campaign_registry,owner))
            return RF_NOT_FOUND;
        if(owner->uid!=uid || !campaign_clutter_damage_bindings ||
           !campaign_clutter_damage_profiles || owner->state.class_index<0 ||
           (uint32_t)owner->state.class_index>=campaign_clutter_classes.count ||
           !campaign_clutter_classes.items)return RF_NOT_FOUND;
        binding=campaign_clutter_damage_bindings+i;
        profile=campaign_clutter_damage_profiles+owner->state.class_index;
        if(!binding->initialized || binding->handle!=owner->state.handle ||
           binding->class_index!=owner->state.class_index ||
           owner->state.definition!=campaign_clutter_classes.items+owner->state.class_index)
            return RF_NOT_FOUND;
        if(!profile->ordinary)return RF_NOT_FOUND;
        if(!isfinite(owner->state.health))return RF_FORMAT;
        /* A retained registry identity is still present. Bit2 retires the
         * prop even when Remove_Object or GeoMod retained positive health,
         * but does not prove original40a0e0 absence for flags[0]'s optional
         * any-missing gate. Hidden0x4000 alone is neither absent nor dead. */
        living=owner->state.health>0 && !(owner->state.flags&2u);
        *present=1;*alive=living;
        return RF_OK;
    }
    return RF_NOT_FOUND;
}
#endif
