#ifndef SCENE_CLUTTER_PRECISION_INC
#define SCENE_CLUTTER_PRECISION_INC
/* Include before scene_precision_gameplay.inc. Sniper selection is read-only
 * until existing detached-fragment priority has been resolved. Rail's authored
 * pierces_all continues across props, actors, shields and the vehicle; no new
 * material-specific sniper penetration policy is invented. */
typedef struct scene_clutter_precision_contact {uint32_t slot,handle;float fraction;} scene_clutter_precision_contact;
static int scene_clutter_precision_select(const float start[3],const float delta[3],float limit,
    scene_clutter_precision_contact *out)
{
    scene_clutter_precision_contact value={UINT32_MAX,UINT32_MAX,limit};int status;
    if(!out||!start||!delta||!isfinite(limit)||limit<0||limit>1)return RF_RANGE;
    status=campaign_clutter_firearm_select(start,delta,limit,&value.slot,&value.fraction);if(status)return status;
    if(value.slot!=UINT32_MAX)value.handle=campaign_clutter_bodies[value.slot]->state.handle;
    *out=value;return RF_OK;
}
/* handled=1 consumes sniper's selected surface, even if blocked/stale, so a
 * stale prop never redirects the shot into an actor behind its old surface. */
static int scene_clutter_precision_commit(scene_stream *stream,const float start[3],const float delta[3],
    const scene_clutter_precision_contact *contact,float damage,int32_t kind,uint32_t *handled)
{
    rf_clutter_base_owner *owner;uint32_t blocked;int status;
    if(!stream||!contact||!handled)return RF_RANGE;
    if(contact->slot==UINT32_MAX){*handled=0;return RF_OK;}
    if(!campaign_clutter_bodies||contact->slot>=campaign_clutter_records.count||
       !(owner=campaign_clutter_bodies[contact->slot])||owner->state.handle!=contact->handle||
       rf_object_registry_lookup(&campaign_registry,contact->handle)!=&owner->state||
       (owner->state.flags&(2u|0x4000u))){*handled=1;return RF_OK;}
    status=combat_shot_obstructed(stream,start,delta,contact->fraction,&blocked);if(status)return status;
    if(!blocked){status=campaign_clutter_firearm_damage(contact->slot,damage,kind);if(status)return status;}
    *handled=1;return RF_OK;
}
static int scene_clutter_precision_rail(const float start[3],const float delta[3],float damage,int32_t kind,uint32_t *hits)
{
    uint32_t i,count=0;int status;
    if(!start||!delta||!hits||!isfinite(damage)||damage<0||kind<0||kind>8)return RF_RANGE;
    /* One borrowed owner/query at a time; no per-prop array or fixed hit cap.
     * Mirrors the existing nearest selector but visits every intersected owner
     * once, necessary for pierces_all without re-hitting a surviving prop. */
    for(i=0;campaign_clutter_bodies&&i<campaign_clutter_records.count;i++){
        rf_clutter_base_owner *owner=campaign_clutter_bodies[i];rf_collision_model_part_query query={0};
        rf_collision_model_response_hit hit={0};float broad;uint32_t accepted=0;
        if(!owner||(owner->state.flags&(2u|0x4000u))||!owner->state.definition||!(owner->state.definition->flags&2u))continue;
        if(rf_object_registry_lookup(&campaign_registry,owner->state.handle)!=&owner->state)continue;
        if(!combat_box(start,delta,&owner->body.state.bounds,1,&broad))continue;
        memcpy(query.input.start,start,12);memcpy(query.input.displacement,delta,12);
        memcpy(query.input.origin,owner->state.position,12);memcpy(query.input.matrix,owner->matrix,36);
        status=rf_scene_clutter_collision_query(owner->state.handle,&query,&hit,1,&accepted);if(status)return status;
        if(!accepted||!isfinite(hit.time)||hit.time<0||hit.time>=1)continue;
        status=campaign_clutter_firearm_damage(i,damage,kind);if(status)return status;++count;
    }
    *hits=count;return RF_OK;
}
#endif
