#ifndef RF_SCENE_SCRIPTED_CLUTTER_SLAY_INC
#define RF_SCENE_SCRIPTED_CLUTTER_SLAY_INC
/* Include after rf_scene_script_slays and before campaign_slay_object.
 * Slay_Object1 already has ordered generic dispatch and delayed/save support;
 * this adds its missing registered clutter consumer only. */
static int campaign_slay_clutter(uint32_t handle,int32_t now)
{
    void *registered=rf_object_registry_lookup(&campaign_registry,handle);
    uint32_t i;
    if(!registered)return RF_NOT_FOUND;
    for(i=0;campaign_clutter_bodies && i<campaign_clutter_records.count;i++) {
        rf_clutter_base_owner *owner=campaign_clutter_bodies[i];
        scene_clutter_damage_live_result result;int status;
        if(!owner || owner->state.handle!=handle || registered!=&owner->state)continue;
        if(!campaign_clutter_damage_profiles || !campaign_clutter_damage_bindings ||
           owner->state.class_index<0 || (uint32_t)owner->state.class_index>=campaign_clutter_classes.count)
            return RF_RANGE;
        /* Specialized held shields retain their existing damage owner. */
        if(!campaign_clutter_damage_profiles[owner->state.class_index].ordinary)return RF_NOT_FOUND;
        if(owner->state.flags&2u)return RF_OK;
        ++rf_scene_script_slays[0];rf_scene_script_slays[2]=campaign_clutter_records.items[i].uid;
        rf_scene_script_slays[4]=(uint32_t)now;
        status=scene_clutter_damage_slay_live(&campaign_registry,owner,
            campaign_clutter_damage_profiles+owner->state.class_index,
            campaign_clutter_damage_bindings+i,&result);
        if(!status) {
            ++rf_scene_clutter_damage[1];rf_scene_clutter_damage[2]+=result.applied;
            rf_scene_clutter_damage[3]+=result.retired;rf_scene_script_slays[1]+=result.retired;
            rf_scene_clutter_damage[4]=campaign_clutter_records.items[i].uid;
            memcpy(rf_scene_clutter_damage+5,&result.health_after,4);
            printf("CLUTTER_SLAY uid%u handle%u time%d health%.9g retired%u\n",
                rf_scene_clutter_damage[4],handle,now,result.health_after,result.retired);
        }
        memcpy(rf_scene_script_slays+3,&owner->state.health,4);
        rf_scene_script_slays[5]=(uint32_t)status;return status;
    }
    return RF_NOT_FOUND;
}
#endif
