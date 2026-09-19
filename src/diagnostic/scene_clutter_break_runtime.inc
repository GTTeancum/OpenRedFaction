#ifndef RF_SCENE_CLUTTER_BREAK_EFFECTS_RUNTIME_INC
#define RF_SCENE_CLUTTER_BREAK_EFFECTS_RUNTIME_INC
/* Authored central emitters; trail head/tail ownership remains deferred. */
static __declspec(noinline) int scene_clutter_break_impact_start(scene_stream *s,const rf_weapon_flight_contact *hit,uint32_t frame,float radius)
{
    uint32_t n,i;int status;int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!isfinite(radius) || radius<=0)return RF_RANGE;
    if(!s->particles.state || !scene_clutter_break_effects || !scene_clutter_break_effects->materials.count)return RF_OK;
    for(n=0;n<8 && scene_clutter_break_effects->instances[n].clock.active;n++){}
    if(n==8 || s->particles.state->emitters.live>RF_PARTICLE_EMITTER_CAPACITY-6)return RF_OK;
    if(scene_clutter_break_effects->definition.recipe.central_count>6 || scene_clutter_break_effects->definition.recipe.central_random!=0)return RF_FORMAT;
    memset(scene_clutter_break_effects->instances+n,0,sizeof(scene_clutter_break_effects->instances[n]));
    scene_clutter_break_effects->instances[n].clock.size=radius;scene_clutter_break_effects->instances[n].clock.active=1;
    for(i=0;i<scene_clutter_break_effects->definition.recipe.central_count;i++) {
        rf_particle_definition p;rf_particle_emitter_template t={0};float extent;uint32_t texture,j;
        status=rf_explosion_central_prepare(&scene_clutter_break_effects->definition,i,radius,&p,&extent);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        texture=scene_clutter_break_effects->materials.slot_texture[i];if(texture>=scene_clutter_break_effects->materials.count)return RF_RANGE;
        for(j=0;j<3;j++){t.position[j]=hit->hit.point[j]+hit->hit.normal[j]*.01f;t.direction[j]=hit->hit.normal[j];}
        t.source_id=i;t.direction_random=p.direction_random;t.min_velocity=p.min_velocity;t.max_velocity=p.max_velocity;
        t.spawn_radius=p.spawn_radius;t.min_spawn_delay=p.min_spawn_delay;t.max_spawn_delay=p.max_spawn_delay;
        t.flags=p.flags.emitter;t.min_life=p.min_life;t.max_life=p.max_life;t.min_radius=p.min_radius;t.max_radius=p.max_radius;
        t.growth=p.growth;t.acceleration=p.acceleration;t.gravity_scale=p.gravity_scale;t.cycle=p.cycle;
        t.bitmap=0x40000000u+texture;t.frame_count=scene_clutter_break_effects->materials.animations[texture].count;
        memcpy(&t.color,p.color,4);memcpy(&t.color_destination,p.color_destination,4);
        t.particle_flags=p.flags.particle;t.secondary=p.flags.secondary;t.age_to_finish_vbm=p.age_to_finish_vbm;
        status=rf_emitter_pool_create(&s->particles.state->emitters,&t,-1,hit->room+1,1,now,NULL,
            &s->particles.state->random,scene_clutter_break_effects->instances[n].slots+i);if(status)return status;
        scene_clutter_break_effects->instances[n].clock.live|=1u<<i;

    }
    if(rf_scene_combat_trace)printf("CLUTTER_BREAK_IMPACT_START %u %u %u %u\n",frame,n,scene_clutter_break_effects->instances[n].clock.live,s->particles.state->particles.live[1]);
    return RF_OK;
}
static int scene_clutter_break_impacts_tick(scene_stream *s,uint32_t frame)
{
    uint32_t n,i;int status;int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!scene_clutter_break_effects)return RF_OK;
    for(n=0;n<8;n++)if(scene_clutter_break_effects->instances[n].clock.active) {
        rf_explosion_clock_actions actions;
        status=rf_explosion_clock_tick(&scene_clutter_break_effects->definition.recipe,1.f/60,&scene_clutter_break_effects->instances[n].clock,&actions);if(status)return status;
        for(i=0;i<scene_clutter_break_effects->definition.recipe.central_count;i++)if(actions.process&(1u<<i)) {
            uint32_t slot=scene_clutter_break_effects->instances[n].slots[i];rf_particle_emitter_update_result result;
            status=rf_particle_emitter_update(&s->particles.state->particles,&s->particles.state->slots[slot].runtime,
                slot+1,1,1.f/60,now,NULL,0,&s->particles.state->random,&result);if(status)return status;
        }
        for(i=0;i<scene_clutter_break_effects->definition.recipe.central_count;i++)if(actions.release&(1u<<i)) {
            status=rf_emitter_pool_release(&s->particles.state->emitters,scene_clutter_break_effects->instances[n].slots[i]);if(status)return status;
        }
        if(actions.release && rf_scene_combat_trace)printf("CLUTTER_BREAK_IMPACT_RELEASE %u %u %u\n",frame,n,actions.release);
    }
    return RF_OK;
}

static int scene_clutter_break_tick(scene_stream *s,uint32_t frame)
{
    uint32_t i,j;int status,expired;int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    status=scene_clutter_break_impacts_tick(s,frame);if(status)return status;
    for(i=0;campaign_clutter_bodies&&campaign_clutter_damage_bindings&&i<campaign_clutter_records.count;i++){
        scene_clutter_damage_binding *binding=campaign_clutter_damage_bindings+i;
        rf_clutter_base_owner *owner=campaign_clutter_bodies[i];scene_clutter_damage_profile *profile;
        rf_weapon_flight_contact contact={0};rf_collision_room_location room;
        if(!binding->break_pending)continue;binding->break_pending=0;
        if(!owner||owner->state.handle!=binding->handle||!scene_clutter_damage_identity(&campaign_registry,owner)||
           owner->state.health>0||owner->state.class_index<0||(uint32_t)owner->state.class_index>=campaign_clutter_classes.count)continue;
        profile=campaign_clutter_damage_profiles+owner->state.class_index;
        if(!profile->break_yellboom||!scene_clutter_break_effects)continue;
        status=rf_timer_expired(owner->state.definition->timer,now,&expired);if(status)return status;if(!expired)continue;
        for(j=0;j<3;j++)contact.hit.point[j]=owner->state.position[j]+profile->break_offset[0]*owner->matrix[j]+
            profile->break_offset[1]*owner->matrix[3+j]+profile->break_offset[2]*owner->matrix[6+j];
        contact.hit.normal[1]=1;contact.object=UINT32_MAX;
        status=rf_geometry_collision_world_locate(s->collision,contact.hit.point,&room);if(status)return status;contact.room=room.room;
        status=scene_clutter_break_impact_start(s,&contact,frame,profile->break_radius);if(status)return status;
        combat_sound("light break",contact.hit.point);
        status=rf_timer_set(&owner->state.definition->timer,now,50);if(status)return status;
        printf("CLUTTER_BREAK uid%u frame%u radius%.9g\n",campaign_clutter_records.items[i].uid,frame,profile->break_radius);
    }
    return RF_OK;
}
#endif
