#ifndef RF_SCENE_DRILLER_BLAST_INC
#define RF_SCENE_DRILLER_BLAST_INC
/* Include after scene_driller_runtime.inc + damage_runtime definitions and
 * scene_blast_amount. Borrow real committed rigid pose/authored CSPH bounds;
 * no temporary registry entry, body allocation or render-bound substitution.
 * Shared489010 policy remains center falloff plus CF5 cover. This does not
 * invent nearest-surface falloff for a large vehicle or exempt its own shots. */
static int scene_driller_blast(scene_stream *s,uint32_t frame,const float origin[3],
    float damage,float radius,uint32_t source,int32_t kind)
{
    scene_driller_damage_runtime *owner=scene_driller_damage_owner;
    scene_driller_physics *physics;rf_physics_body body={0};
    rf_damage_request request={0,source,kind,0,UINT32_MAX,0};float amount=0,applied=0;int status;
    if(!s || !origin)return RF_RANGE;
    if(!s->driller_runtime || !scene_driller_damage_runtime_valid(owner) ||
       owner->entry!=&s->driller_runtime->entry)return RF_OK;
    if(!owner->entry->host.alive || owner->damage.destroyed || owner->damage.state.effects.health<=0 ||
       (owner->entry->view.flags_7c&(2|0x4000)) || (owner->entry->view.flags_810&1))return RF_OK;
    if(!isfinite(damage) || !isfinite(radius))return RF_FORMAT;
    if(damage<=0 || radius<=.1f)return RF_OK;
    physics=owner->entry->physics;
    if(!physics || !physics->sphere_count || physics->sphere_count>8)return RF_OK;
    memcpy(body.state.position,physics->state.position,12);
    memcpy(body.state.orientation,physics->state.orientation,36);
    body.spheres.items=physics->spheres;body.spheres.count=physics->sphere_count;
    status=rf_physics_spheres_bounds(physics->spheres,physics->sphere_count,physics->state.position,&body.state.bounds);if(status)return status;
    ++rf_scene_rocket_blast[1];
    status=scene_blast_amount(s,origin,&body,damage,radius,&amount);if(status)return status;
    if(amount<=0)return RF_OK;
    request.amount=amount;
    status=scene_driller_damage_runtime_receive(owner,owner->damage.state.effects.handle,&request,frame,&applied);
    if(status==RF_NOT_FOUND)return RF_OK; /* Generation retired before dispatch. */
    if(status)return status;
    if(applied>0) {
        ++rf_scene_rocket_blast[2];memcpy(rf_scene_rocket_blast+5,&applied,4);
        /* Registered vehicle damage has separate owner health/death handling;
         * do not count an NPC kill or damage the seated player's health here. */
        if(rf_scene_combat_trace)printf("DRILLER_BLAST %u %u %u %d %.9g %.9g\n",frame,
            owner->damage.state.effects.handle,source,kind,applied,owner->damage.state.effects.health);
    }
    return RF_OK;
}
#endif
