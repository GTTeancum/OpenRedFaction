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
    if(scene_driller_damage_runtime_valid(owner)&&!scene_driller_projectile_source_excluded(owner,source)) {
        physics=owner->entry->physics;
        if(!physics||!physics->sphere_count||physics->sphere_count>8)return RF_RANGE;
        /* Borrow initialized physics spheres; collision-body rows differ in layout. */
        body.spheres.items=physics->spheres;body.spheres.count=physics->sphere_count;
        memcpy(body.state.position,physics->state.position,12);
        memcpy(body.state.orientation,physics->state.orientation,36);
        if(scene_rocket_body_hit(&body,start,delta,radius,limit,&hit)&&hit.fraction<limit) {
            out->hit=hit;out->object=SCENE_DRILLER_PROJECTILE_OWNER;
            out->room=UINT32_MAX;out->face=owner->entry->registration.handle;*matched=1;
            limit=hit.fraction;
        }
    }
    if(campaign_passive_vehicle_count) {
        scene_stream *s=scene_actor_collision_owner;
        if(!s)return RF_NOT_FOUND;
        for(i=0;i<campaign_passive_vehicle_count;i++) {
            const scene_passive_vehicle *passive=campaign_passive_vehicles+i;
            rf_physics_sphere spheres[8]={{0}};uint32_t j,kind=passive->resource_kind;
            if(source==passive->handle)continue;
            if(kind>=6||!s->passive_vehicle_sphere_count[kind]||
               s->passive_vehicle_sphere_count[kind]>8)return RF_FORMAT;
            if(rf_object_registry_lookup(&campaign_registry,passive->handle)!=passive)return RF_NOT_FOUND;
            for(j=0;j<s->passive_vehicle_sphere_count[kind];j++) {
                memcpy(spheres[j].center,s->passive_vehicle_spheres[kind][j].center,12);
                spheres[j].radius=s->passive_vehicle_spheres[kind][j].radius;
            }
            body.spheres.items=spheres;body.spheres.count=s->passive_vehicle_sphere_count[kind];
            memcpy(body.state.position,passive->pose.position,12);
            memcpy(body.state.orientation,passive->pose.input_matrix,36);
            ++rf_scene_passive_damage[0];
            if(!scene_rocket_body_hit(&body,start,delta,radius,limit,&hit)||hit.fraction>=limit)continue;
            out->hit=hit;out->object=SCENE_DRILLER_PROJECTILE_OWNER;
            out->room=UINT32_MAX;out->face=passive->handle;*matched=1;limit=hit.fraction;
            ++rf_scene_passive_damage[1];rf_scene_passive_damage[4]=passive->uid;
        }
    }
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
static int scene_passive_vehicle_destroy(void *context,uint32_t host,uint32_t responsible)
{
    scene_passive_vehicle *owner=context;(void)responsible;
    if(!owner||owner->handle!=host||
       rf_object_registry_lookup(&campaign_registry,host)!=owner)return RF_NOT_FOUND;
    return RF_OK; /* Retain the wreck pose and registry identity for When_Dead. */
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
    if(scene_driller_damage_runtime_valid(owner)&&contact->face==owner->entry->registration.handle) {
        if(scene_driller_projectile_source_excluded(owner,source))return RF_OK;
        return scene_driller_damage_runtime_receive(owner,contact->face,&request,frame,amount);
    }
    for(uint32_t i=0;i<campaign_passive_vehicle_count;i++) {
        scene_passive_vehicle *passive=campaign_passive_vehicles+i;
        combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60),0};
        rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
            combat_random,combat_notify,combat_playing,combat_play,&feedback};
        float seconds=(float)frame/60;uint32_t clock,was_destroyed;int status;
        if(passive->handle!=contact->face)continue;
        if(rf_object_registry_lookup(&campaign_registry,passive->handle)!=passive ||
           source==passive->handle)return RF_OK;
        was_destroyed=passive->damage.destroyed;
        memcpy(&clock,&seconds,4);
        status=scene_driller_damage_receive(&passive->damage,&request,1,clock,0,&effects,
            scene_passive_vehicle_destroy,passive,amount);
        if(!status)status=feedback.status;
        rf_scene_passive_damage[6]=(uint32_t)status;if(status)return status;
        ++rf_scene_passive_damage[2];rf_scene_passive_damage[3]+=!was_destroyed&&passive->damage.destroyed;
        rf_scene_passive_damage[4]=passive->uid;
        memcpy(rf_scene_passive_damage+5,&passive->damage.state.effects.health,4);
        return RF_OK;
    }
    return RF_OK;
}
#endif
