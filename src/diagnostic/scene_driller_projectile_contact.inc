#ifndef RF_SCENE_DRILLER_PROJECTILE_CONTACT_INC
#define RF_SCENE_DRILLER_PROJECTILE_CONTACT_INC
/* Include after scene_rocket_body_hit and scene_driller_damage_runtime.inc.
 * Query the already committed host pose, using the same transformed authored
 * sphere union as ordinary rocket/NPC contacts. No world query is replaced.
 * Existing player tag is0x10000000; reserve a DISTINCT vehicle namespace.
 * The vehicle contact's face field carries its full generation-bearing host
 * handle, not a geometry face. room stays UINT32_MAX. Never use it for terrain
 * or face-material lookup; generic impact visuals use the contact position.
 */
#define SCENE_DRILLER_PROJECTILE_OWNER 0x08000000u
static uint32_t scene_driller_projectile_source_excluded(const scene_driller_damage_runtime *owner,uint32_t source)
{
    const scene_driller_entry_exit *entry=owner->entry;
    return source==entry->host.handle||
        (entry->session.active&&source==entry->host.driver)||
        (source==campaign_player_object.handle&&campaign_player_view.linked_handle==(int32_t)entry->host.handle);
}
static int scene_driller_projectile_query(uint32_t source,const float start[3],const float delta[3],
    float radius,float limit,rf_weapon_flight_contact *out,uint32_t *matched)
{
    scene_driller_damage_runtime *owner=scene_driller_damage_owner;
    rf_physics_body body={0};rf_collision_ray_hit hit;scene_driller_physics *physics;uint32_t i;
    if(!start||!delta||!out||!matched||!isfinite(radius)||radius<0||!isfinite(limit)||limit<0||limit>1)return RF_RANGE;
    *matched=0;
    for(i=0;i<3;i++)if(!isfinite(start[i])||!isfinite(delta[i]))return RF_RANGE;
    if(!scene_driller_damage_runtime_valid(owner)||scene_driller_projectile_source_excluded(owner,source))return RF_OK;
    physics=owner->entry->physics;
    if(!physics||!physics->sphere_count||physics->sphere_count>8)return RF_RANGE;
    /* Borrow actual initialized rf_physics_sphere rows, without a layout cast
     * from rf_collision_body_sphere and without heap or ownership transfer. */
    body.spheres.items=physics->spheres;body.spheres.count=physics->sphere_count;
    memcpy(body.state.position,physics->state.position,12);
    memcpy(body.state.orientation,physics->state.orientation,36);
    if(!scene_rocket_body_hit(&body,start,delta,radius,limit,&hit)||hit.fraction>=limit)return RF_OK;
    out->hit=hit;out->object=SCENE_DRILLER_PROJECTILE_OWNER;
    out->room=UINT32_MAX;out->face=owner->entry->registration.handle;*matched=1;
    return RF_OK;
}
/* Append after ALL existing world/NPC/mover/detached candidates. A nearer
 * host replaces the prior hit; equal fractions preserve original tie order.
 * Pass the shot's retained source, not whichever weapon is currently held. */
static int scene_driller_projectile_compose(uint32_t source,const float start[3],const float delta[3],
    float radius,rf_weapon_flight_contact *out,uint32_t *liquid,uint32_t *matched)
{
    rf_weapon_flight_contact candidate;uint32_t found;int status;
    if(!out||!liquid||!matched)return RF_RANGE;
    status=scene_driller_projectile_query(source,start,delta,radius,*matched?out->hit.fraction:1,&candidate,&found);
    if(status)return status;
    if(found){*out=candidate;*matched=1;*liquid=0;}
    return RF_OK;
}
/* Firearms: supply the nearest ALREADY established world/actor/shield/cover
 * fraction. Select with radius0 (no fictitious bullet thickness). If selected,
 * this host consumes the ray/pellet before ordinary victim damage dispatch. */
static int scene_driller_firearm_select(uint32_t source,const float start[3],const float delta[3],
    float nearest,rf_weapon_flight_contact *out,uint32_t *selected)
{
    return scene_driller_projectile_query(source,start,delta,0,nearest,out,selected);
}
/* Shared firearm and terminal projectile direct hit. Caller retains separate
 * radial blast behavior. Unknown tags remain unhandled; stale host generations
 * consume their hit without damaging a newly registered replacement vehicle. */
static int scene_driller_projectile_damage(const rf_weapon_flight_contact *contact,
    uint32_t source,float damage,int32_t kind,uint32_t frame,uint32_t *handled,float *amount)
{
    scene_driller_damage_runtime *owner=scene_driller_damage_owner;
    rf_damage_request request={damage,source,kind,0,UINT32_MAX,0};
    if(!contact||!handled||!amount)return RF_RANGE;
    *handled=0;*amount=0;
    if(contact->object!=SCENE_DRILLER_PROJECTILE_OWNER)return RF_OK;
    *handled=1;
    if(!isfinite(damage)||damage<0||kind< -1||kind>10)return RF_RANGE;
    if(!scene_driller_damage_runtime_valid(owner)||contact->face!=owner->entry->registration.handle||
       scene_driller_projectile_source_excluded(owner,source))return RF_OK;
    return scene_driller_damage_runtime_receive(owner,contact->face,&request,frame,amount);
}
#endif
