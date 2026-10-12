#ifndef RF_SCENE_AI_FLAME_INCLUDED
#define RF_SCENE_AI_FLAME_INCLUDED
/* NPC primary flame, after the shared player flame/particle adapters.
 * Owns a synchronous accepted pulse only. Existing RFNC inventory, ordinary
 * combat/reload deadlines, RFAP burns and pooled cosmetic particles retain
 * their current owners. No per-NPC allocation, active-flight or timer sidecar.
 * Practical eye-origin, cone width and batched fuel match the existing port
 * flame policy; they are not original emitter/collider reconstruction. */
uint32_t rf_scene_ai_flame[8]; /* pulses,fuel,contacts,player hits,NPC hits,deaths,last source,status */
typedef struct scene_ai_flame_contact_context {scene_stream *stream;uint32_t source;} scene_ai_flame_contact_context;
static int scene_ai_flame_cover(void *context,const float *start,const float *end,uint32_t *blocked)
{
    scene_ai_flame_contact_context *q=context;rf_weapon_flight_contact contact={0};
    float delta[3];uint32_t liquid=0,found=0;int status;
    status=scene_driller_flame_cover(q->stream,q->source,start,end,blocked);if(status||*blocked)return status;
    for(uint32_t j=0;j<3;j++)delta[j]=end[j]-start[j];
    /* Opaque props also stop the volume. This is a read-only query; a flame
     * pulse does not silently substitute bullet damage on intervening props. */
    status=scene_clutter_projectile_compose(q->source,start,delta,0,&contact,&liquid,&found);
    if(!status)*blocked=found;return status;
}
static int scene_ai_flame_body_contact(scene_ai_flame_contact_context *q,const rf_physics_body *body,
    const float origin[3],const float forward[3],uint32_t *hit)
{
    uint32_t i,j,k,inside;int status;float center[3];
    if(!q || !body || !hit || (body->spheres.count&&!body->spheres.items))return RF_RANGE;
    *hit=0;
    for(i=0;i<body->spheres.count;i++){
        const rf_physics_sphere *sphere=body->spheres.items+i;
        for(j=0;j<3;j++){
            center[j]=body->state.position[j];
            for(k=0;k<3;k++)center[j]+=sphere->center[k]*body->state.orientation[k*3+j];
        }
        status=rf_flame_contact(origin,forward,SCENE_FLAME_RANGE,SCENE_FLAME_RADIUS,
            center,sphere->radius,scene_ai_flame_cover,q,&inside);if(status)return status;
        if(inside){*hit=1;return RF_OK;} /* Sphere union: one dose per body. */
    }
    return RF_OK;
}
static uint32_t scene_ai_flame_source_live(uint32_t slot,uint32_t handle,int32_t weapon,uint64_t epoch)
{
    campaign_npc_body *o;
    if(epoch!=scene_robot_death_epoch)return 0;
    o=scene_ai_melee_npc_live(slot,handle);
    return o && o->view.weapons[0]==weapon && o->inventory.owned[weapon];
}
static int scene_ai_flame_fire(scene_stream *stream,uint32_t slot,uint32_t frame,
    const float target[3],uint32_t once,uint32_t mode)
{
    campaign_enemy_weapon_selection selected={0};campaign_npc_body *owner;
    scene_ai_flame_contact_context query;rf_damage_request request;
    combat_feedback feedback={(int32_t)((uint64_t)frame*1000/60%RF_TIMER_PERIOD),0};
    rf_damage_effect_backend effects={combat_predicate,combat_uid,combat_source,combat_burn,
        combat_random,combat_notify,combat_playing,combat_play,&feedback};
    float origin[3],direction[3],seconds=(float)frame/60,amount,difficulty;
    double length=0;uint32_t source,player,count,clock,fuel,burst,contact,entered;
    uint64_t epoch=scene_robot_death_epoch;int32_t weapon;int status;
    if(!stream || !target || slot>=campaign_npc_body_count || once>1 || mode>2)return RF_RANGE;
    owner=campaign_npc_bodies+slot;
    status=campaign_enemy_flame_select(owner,&campaign_weapon_supply,&selected);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    weapon=owner->view.weapons[0];source=owner->registration.handle;
    if(owner->inventory.loaded[weapon]<=0)return RF_OK;
    memcpy(origin,owner->eye_position,12);
    for(uint32_t j=0;j<3;j++){
        if(!isfinite(origin[j]) || !isfinite(target[j]))return RF_RANGE;
        direction[j]=target[j]-origin[j];length+=(double)direction[j]*direction[j];
    }
    if(!isfinite(length)||length<1e-12)return RF_OK;
    length=sqrt(length);for(uint32_t j=0;j<3;j++)direction[j]=(float)(direction[j]/length);
    /* The installed primary lacks underwater fire. Eye immersion is the
     * bounded NPC policy, matching the actual launch origin. */
    if(stream->collision&&stream->liquid_rooms){rf_collision_room_location location;
        status=rf_geometry_collision_world_locate(stream->collision,origin,&location);if(status)return status;
        if(location.room<stream->swim_room_count){const rf_liquid_room *liquid=stream->liquid_rooms+location.room;
            if(liquid->type>=0&&isfinite(liquid->depth)&&origin[1]<=liquid->minimum_y+liquid->depth)return RF_OK;
        }
    }
    burst=owner->combat_burst_remaining;
    if(!once){status=campaign_enemy_cadence(selected.primary,frame,&owner->combat_burst_remaining,&owner->combat_due);if(status)return status;}
    fuel=owner->inventory.loaded[weapon]<5?(uint32_t)owner->inventory.loaded[weapon]:5u;
    owner->inventory.loaded[weapon]-=(int32_t)fuel;
    request=(rf_damage_request){combat_enemy_primary_damage(selected.primary),source,4,0,UINT32_MAX,0};
    player=campaign_player_object.handle;count=campaign_npc_body_count;
    query=(scene_ai_flame_contact_context){stream,source};memcpy(&clock,&seconds,4);
    /* Acceptance precedes all audio/damage callbacks. A finite forced request
     * is retired before any callback can replace its queue. */
    if(once)campaign_script_single_pop(owner,1);
    ++rf_scene_ai_flame[0];rf_scene_ai_flame[1]+=fuel;rf_scene_ai_flame[6]=source;
    ++rf_scene_enemy_combat[2];++rf_scene_enemy_damage_kinds[4];
    if(owner->combat_scripted==3&&!once)++rf_scene_script_shoot_at[3];
    status=campaign_enemy_fire_presentation(slot,!(once&&mode==2),selected.primary,burst,once);
    rf_scene_enemy_fire[5]=(uint32_t)status;
    if(status==RF_NOT_FOUND)++rf_scene_enemy_fire[2];else if(status)return status;
    if(!scene_ai_flame_source_live(slot,source,weapon,epoch))return RF_OK;
    /* Reuse the already bounded shared particle pool. Source-aware sweep
     * prevents the shooter truncating its own jet at its body sphere. */
    {scene_ai_projectile_query visual={stream,source};rf_weapon_flight_contact hit;
     float ray[3],visible=SCENE_FLAME_RANGE;uint32_t liquid=0,matched=0;
     for(uint32_t j=0;j<3;j++)ray[j]=direction[j]*SCENE_FLAME_RANGE;
     status=scene_ai_projectile_sweep(&visual,origin,ray,.01f,4u,&hit,&liquid,&matched);if(status)return status;
     if(matched)visible*=hit.hit.fraction;
     status=scene_flame_visual_tick(stream,frame,origin,direction,visible,1);if(status)return status;}
    if(!scene_ai_flame_source_live(slot,source,weapon,epoch))return RF_OK;
    {uint32_t host_contact;float host_amount;
     status=scene_driller_flame_pulse(stream,frame,origin,direction,&request,&host_contact,&host_amount);if(status)return status;}
    if(!scene_ai_flame_source_live(slot,source,weapon,epoch))return RF_OK;
    if(scene_ai_actor_penetration_player_live(player)&&!scene_ai_actor_ray_related(source,player)){
        status=scene_ai_flame_body_contact(&query,&scene_actor_body,origin,direction,&contact);if(status)return status;
        if(contact&&scene_ai_actor_penetration_player_live(player)){
            ++rf_scene_ai_flame[2];
            status=combat_enemy_player_difficulty(stream,player,&request,&difficulty);if(status)return status;
            amount=0;status=rf_scene_player_damage(player,&request,difficulty,clock,&effects,&amount);
            if(!status)status=feedback.status;if(status)return status;
            if(!scene_ai_flame_source_live(slot,source,weapon,epoch))return RF_OK;
            if(amount>0){++rf_scene_ai_flame[3];++rf_scene_enemy_combat[3];
                if(scene_ai_melee_player_identity(player))
                    campaign_combat_event(frame,1,source,amount,campaign_player_damage.state.effects.health);}
        }
    }
    for(uint32_t i=0;i<count;i++){
        campaign_npc_body *victim;uint32_t handle;
        if(!scene_ai_flame_source_live(slot,source,weapon,epoch))return RF_OK;
        if(i>=campaign_npc_body_count)return RF_OK;
        handle=campaign_npc_bodies[i].registration.handle;
        victim=scene_ai_melee_npc_live(i,handle);
        if(!victim||scene_ai_actor_ray_related(source,handle))continue;
        status=scene_ai_flame_body_contact(&query,&victim->body,origin,direction,&contact);if(status)return status;
        if(!contact)continue;
        victim=scene_ai_melee_npc_live(i,handle);if(!victim)continue;
        ++rf_scene_ai_flame[2];amount=0;
        /* Shared flame policy: generic fire immunity/burn admission owns the
         * result. No invented projectile/Nano debit or shield-bash contact. */
        status=rf_scene_npc_damage(handle,&request,1,clock,&effects,&amount);
        if(!status)status=feedback.status;if(status)return status;
        if(!scene_ai_flame_source_live(slot,source,weapon,epoch))return RF_OK;
        if(amount>0){++rf_scene_ai_flame[4];++rf_scene_enemy_combat[3];}
        victim=scene_ai_melee_npc_identity(i,handle);
        if(victim&&victim->damage.effects.health<=0){
            status=rf_scene_npc_death_entry(handle,&entered);if(status)return status;
            if(!scene_ai_flame_source_live(slot,source,weapon,epoch))return RF_OK;
            if(entered&&scene_ai_melee_npc_identity(i,handle)){
                ++rf_scene_ai_flame[5];status=combat_death_start(i);if(status)return status;
            }
        }
    }
    return RF_OK;
}
#endif
