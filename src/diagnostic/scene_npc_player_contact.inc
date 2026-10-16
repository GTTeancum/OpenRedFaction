#ifndef RF_SCENE_NPC_PLAYER_CONTACT_INC
#define RF_SCENE_NPC_PLAYER_CONTACT_INC
/* Live grounded NPC translation only. Complements the existing player-first
 * adapter without starting the global pair scheduler or changing clearance.
 * RF.exe48bf35..48bf92 is the distinct NPC-first/player-second admission;
 *49ab00 supplies the max-sphere-radius horizontal shape. Use the same explicit
 * geometric-fraction port adaptation as scene_player_npc_contact.inc.
 * Requires that include and the existing rf_scene_npc_body_sweep definition.
 */
static uint32_t scene_npc_player_pair(const campaign_npc_body *owner,uint32_t slot,
    const rf_physics_body_state *state)
{
    rf_collision_actor_pair_view a={0},b={0};
    rf_collision_pair_actor_state first={0},second={0};uint32_t flags=0;
    /* Grounded humanoid contact stays lateral-only; Drone uses full3D. */
    if(campaign_modes[rf_scene_actor_landing[1]].index!=1 && !scene_contact_drone(slot))return 0;
    a.object_flags=owner->object_flags;a.body_flags=state->flags;a.extent_180=state->bounds.radius;
    a.use_kind=campaign_seeds.classes[campaign_seeds.items[slot].class_index].physics.use_kind;
    b.object_flags=campaign_player_view.flags_7c;b.body_flags=scene_actor_body.state.flags;
    b.extent_180=scene_actor_body.state.bounds.radius;
    /* Do not reuse the player-first predicate:48bf64/48bf71 deliberately
     * compare twice the PLAYER extent in both NPC-first size comparisons. */
    if(rf_collision_actor_pair_reject(&a,&b,0,0,&flags)||
       (flags!=0x20u && !(scene_contact_drone(slot)&&flags==2u)))return 0;
    first.handle=owner->registration.handle;first.parent_handle=(uint32_t)owner->view.linked_handle;
    first.object_flags=owner->object_flags;
    second.handle=campaign_player_object.handle;second.parent_handle=(uint32_t)campaign_player_view.linked_handle;
    second.object_flags=campaign_player_view.flags_7c;
    return rf_collision_pair_response_allowed(&first,&second);
}

/* All readers below are synchronous and callback-free. The player has already
 * moved this frame: freeze its COMMITTED position, never its stale next_position
 * or a second velocity*dt projection. Existing world hits win ties, including
 * zero-time contact. Miss/error preserves the supplied result and matched bit.
 * No reciprocal contact, damage, impulse, support handle or velocity write.
 */
static int scene_npc_player_compose(const campaign_npc_body *owner,
    const rf_physics_body_state *state,rf_geometry_body_hit *hit,uint32_t *matched)
{
    const rf_physics_body_state *player=&scene_actor_body.state;
    rf_geometry_body_hit result={0};float delta[3],ray[6],point[3],source_radius,target_radius;
    float limit,length,t,combined_radius;double magnitude,inverse,norm;uint32_t slot,k;int status;
    static const float zero[3]={0};
    if(!owner||!state||!hit||!matched||*matched>1)return RF_RANGE;
    if(!campaign_npc_bodies||owner<campaign_npc_bodies||owner>=campaign_npc_bodies+campaign_npc_body_count)return RF_NOT_FOUND;
    slot=(uint32_t)(owner-campaign_npc_bodies);
    if(scene_player_npc_owner(slot)!=owner||!scene_player_npc_player(player,campaign_player_object.handle))return RF_OK;
    if(memcmp(state->position,owner->body.state.position,12))return RF_NOT_FOUND;
    if(scene_contact_drone(slot)) {
        rf_scene_drone_player_contact[4]=2;rf_scene_drone_player_contact[5]=slot;
        rf_scene_drone_player_contact[6]=owner->body.spheres.count;
        rf_scene_drone_player_contact[7]=campaign_player_material;
        memcpy(rf_scene_drone_player_contact+8,&state->mass,4);
        memcpy(rf_scene_drone_player_contact+9,&state->bounds.radius,4);
    }
    if(!isfinite(state->bounds.radius)||state->bounds.radius<=0||
       !isfinite(player->bounds.radius)||player->bounds.radius<=0||campaign_player_material>=10)return RF_FORMAT;
    limit=*matched?hit->contact.fraction:1.f;
    if(!isfinite(limit)||limit<0||limit>1)return RF_FORMAT;
    if(!limit||!scene_npc_player_pair(owner,slot,state))return RF_OK;
    if(scene_contact_drone(slot)) {
        uint32_t found=0;++rf_scene_drone_player_contact[0];
        status=scene_drone_sphere_sweep(state,&owner->body.spheres,player,
            &scene_actor_body.spheres,limit,&result,&found);if(status)return status;
        if(found) {
            ++rf_scene_drone_player_contact[1];result.contact.material=campaign_player_material;
            result.contact.object_id=campaign_player_object.handle;
            for(k=0;k<3;k++) {
                result.contact.velocity[k]=player->velocity[k]+campaign_support_velocity[k];
                if(!isfinite(result.contact.velocity[k]))return RF_FORMAT;
            }
            *hit=result;*matched=1;
        }
        return RF_OK;
    }
    for(k=0;k<3;k++){
        float low,high,other_low,other_high;
        if(!isfinite(state->position[k])||!isfinite(state->next_position[k])||!isfinite(player->position[k]))return RF_FORMAT;
        delta[k]=state->next_position[k]-state->position[k];if(!isfinite(delta[k]))return RF_FORMAT;
        low=(float)((double)fminf(state->position[k],state->next_position[k])-state->bounds.radius);
        high=(float)((double)fmaxf(state->position[k],state->next_position[k])+state->bounds.radius);
        other_low=(float)((double)player->position[k]-player->bounds.radius);
        other_high=(float)((double)player->position[k]+player->bounds.radius);
        if(!isfinite(low)||!isfinite(high)||!isfinite(other_low)||!isfinite(other_high))return RF_FORMAT;
        /* Original strict full-3D overlap precedes the lateral-only ray. */
        if(!(low<other_high&&other_low<high))return RF_OK;
        ray[k]=(float)((double)player->position[k]-state->position[k]);ray[k+3]=-delta[k];
        if(!isfinite(ray[k]))return RF_FORMAT;
    }
    if(delta[0]==0&&delta[2]==0)return RF_OK;
    status=scene_player_npc_radius(&owner->body.spheres,&source_radius);if(status)return status;
    status=scene_player_npc_radius(&scene_actor_body.spheres,&target_radius);if(status)return status;
    combined_radius=(float)((double)source_radius+target_radius);if(!isfinite(combined_radius))return RF_FORMAT;
    ray[1]=ray[4]=0;
    magnitude=sqrt((double)ray[3]*ray[3]+(double)ray[5]*ray[5]);
    length=(float)magnitude;if(!isfinite(length))return RF_FORMAT;if(!length)return RF_OK;
    inverse=1.0/magnitude;for(k=0;k<3;k++)ray[k+3]=(float)((double)ray[k+3]*inverse);
    /*508e40 already returns geometric [0,1]. Leave49ad33's additional
     * divide-by-length in core; it is unsuitable for this world's hit times. */
    if(!rf_collision_ray_sphere(ray,length,zero,combined_radius,point,&t))return RF_OK;
    if(!isfinite(t))return RF_FORMAT;
    if(t<0||!(t<limit))return RF_OK;
    if(t==0&&((double)ray[3]*point[0]+(double)ray[5]*point[2])>=0)return RF_OK;
    norm=sqrt((double)point[0]*point[0]+(double)point[2]*point[2]);
    if(norm>0){inverse=1.0/norm;for(k=0;k<3;k++)result.contact.normal[k]=(float)(-(double)point[k]*inverse);}
    else result.contact.normal[0]=-1;
    result.contact.fraction=t;result.contact.material=campaign_player_material;
    result.contact.object_id=campaign_player_object.handle;result.contact.texture=UINT32_MAX;
    result.contact.face_token=UINT32_MAX;result.solid=result.room=result.face=result.sphere=UINT32_MAX;result.hits=1;
    for(k=0;k<3;k++){
        float travel=(float)((double)delta[k]*t),offset=(float)((double)result.contact.normal[k]*source_radius);
        result.contact.point[k]=(float)((double)state->position[k]+travel);
        result.contact.point[k]=(float)((double)result.contact.point[k]-offset);
        result.contact.velocity[k]=(float)((double)player->velocity[k]+campaign_support_velocity[k]);
        if(!isfinite(result.contact.point[k])||!isfinite(result.contact.velocity[k]))return RF_FORMAT;
    }
    *hit=result;*matched=1;return RF_OK;
}

/* Private live-motion wrapper. Generic rf_scene_npc_body_sweep remains the
 * established world/clearance API, including stance, teleport and restore.
 * Call again for each wall-slide/uphill replacement proposal. The existing
 * kinematic consumer owns stop/slide; original49dd0b..49dd16 skips the dynamic
 * velocity impulse for an NPC source against a player target.
 */
static int scene_npc_live_body_sweep(const rf_geometry_collision_world *world,
    campaign_npc_body *owner,const rf_physics_body_state *state,uint32_t flags,
    rf_collision_body_sphere *scratch,uint32_t capacity,rf_geometry_body_hit *hit,uint32_t *matched)
{
    rf_geometry_body_hit result={0};uint32_t found=0;int status;
    if(!owner||!hit||!matched)return RF_RANGE;
    status=rf_scene_npc_body_sweep(world,owner->registration.handle,state,flags,scratch,capacity,&result,&found);
    if(status)return status;
    status=scene_npc_player_compose(owner,state,&result,&found);if(status)return status;
    if(found)*hit=result;*matched=found;return RF_OK;
}
#endif
