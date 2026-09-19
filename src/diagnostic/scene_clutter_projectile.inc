/* Shared finite-radius projectile contacts with actual retained prop meshes.
 * The contact face carries owner generation; the object tag carries its slot. */
#define SCENE_CLUTTER_PROJECTILE_OWNER 0x04000000u
static int scene_clutter_projectile_compose(uint32_t source,const float start[3],const float delta[3],float radius,
    rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    uint32_t i,j;int status;
    if(!start||!delta||!out||!liquid||!matched||!isfinite(radius)||radius<0)return RF_RANGE;
    for(i=0;campaign_clutter_bodies&&i<campaign_clutter_records.count;i++){
        rf_clutter_base_owner *owner=campaign_clutter_bodies[i];rf_collision_model_part_query query={0};
        rf_collision_model_response_hit hit={0};rf_collision_ray_hit local;rf_physics_bounds bounds;float broad;
        uint32_t accepted;float limit=*matched?out->hit.fraction:1;
        if(!owner||owner->state.handle==source||(owner->state.flags&(2u|0x4000u))||
           !owner->state.definition||!(owner->state.definition->flags&6u))continue;
        if(rf_object_registry_lookup(&campaign_registry,owner->state.handle)!=&owner->state)continue;
        bounds=owner->body.state.bounds;
        for(j=0;j<3;j++){bounds.minimum[j]-=radius;bounds.maximum[j]+=radius;}
        if(!combat_box(start,delta,&bounds,limit,&broad))continue;
        memcpy(query.input.start,start,12);memcpy(query.input.displacement,delta,12);
        memcpy(query.input.origin,owner->state.position,12);memcpy(query.input.matrix,owner->matrix,36);query.input.radius=radius;
        status=rf_scene_clutter_collision_query(owner->state.handle,&query,&hit,1,&accepted);if(status)return status;
        if(!accepted||!isfinite(hit.time)||hit.time<0||hit.time>=limit)continue;
        local.fraction=hit.time;memcpy(local.point,hit.point,12);memcpy(local.normal,hit.normal,12);
        status=rf_collision_contact_world(&local,owner->state.position,(const float (*)[3])owner->matrix,&out->hit);if(status)return status;
        out->object=SCENE_CLUTTER_PROJECTILE_OWNER|i;out->face=owner->state.handle;out->room=UINT32_MAX;
        *matched=1;*liquid=0;
    }
    return RF_OK;
}
static int scene_clutter_projectile_damage(const rf_weapon_flight_contact *contact,float damage,int32_t kind,uint32_t *handled)
{
    uint32_t slot;rf_clutter_base_owner *owner;
    if(!contact||!handled)return RF_RANGE;*handled=0;
    if((contact->object&0xffff0000u)!=SCENE_CLUTTER_PROJECTILE_OWNER)return RF_OK;
    *handled=1;slot=contact->object&0xffffu;
    if(!campaign_clutter_bodies||slot>=campaign_clutter_records.count||!(owner=campaign_clutter_bodies[slot])||
       owner->state.handle!=contact->face||rf_object_registry_lookup(&campaign_registry,contact->face)!=&owner->state)return RF_OK;
    return campaign_clutter_firearm_damage(slot,damage,kind);
}
