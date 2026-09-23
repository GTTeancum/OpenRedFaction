#ifndef RF_SCENE_SCRIPT_EXPLODE_RUNTIME_INC
#define RF_SCENE_SCRIPT_EXPLODE_RUNTIME_INC
static int scene_script_explode_effects_start(scene_stream *s,const float position[3],
    uint32_t frame,float radius,uint32_t clip_index)
{
    scene_script_explode_effect_owner *effect=clip_index<64?scene_script_explode_effects[clip_index]:NULL;
    rf_collision_room_location room={0};uint32_t n,i;int status;
    if(!effect || !s->particles.state || radius<=0){++rf_scene_script_explode_visual[4];return RF_OK;}
    if(!isfinite(radius))return RF_RANGE;
    for(n=0;n<8 && effect->instances[n].clock.active;n++){}
    if(n==8 || s->particles.state->emitters.live>
       RF_PARTICLE_EMITTER_CAPACITY-effect->definition.recipe.central_count){
        ++rf_scene_script_explode_visual[4];return RF_OK;
    }
    status=rf_geometry_collision_world_locate(s->collision,position,&room);if(status)return status;
    memset(effect->instances+n,0,sizeof(effect->instances[n]));
    effect->instances[n].clock.size=radius;effect->instances[n].clock.active=1;
    for(i=0;i<effect->definition.recipe.central_count;i++) {
        rf_particle_definition p;rf_particle_emitter_template t={0};float extent;uint32_t texture;
        status=rf_explosion_central_prepare(&effect->definition,i,radius,&p,&extent);
        if(status==RF_NOT_FOUND)continue;if(status)goto failed;
        texture=effect->materials.slot_texture[i];if(texture>=effect->materials.count){status=RF_RANGE;goto failed;}
        memcpy(t.position,position,12);
        /* The world-wrapper visual has no impact normal. Unit X is the
         * explicit direction supplied by the sibling scripted GeoMod call. */
        t.direction[0]=1;
        t.source_id=i;t.direction_random=p.direction_random;t.min_velocity=p.min_velocity;t.max_velocity=p.max_velocity;
        t.spawn_radius=p.spawn_radius;t.min_spawn_delay=p.min_spawn_delay;t.max_spawn_delay=p.max_spawn_delay;
        t.flags=p.flags.emitter;t.min_life=p.min_life;t.max_life=p.max_life;t.min_radius=p.min_radius;t.max_radius=p.max_radius;
        t.growth=p.growth;t.acceleration=p.acceleration;t.gravity_scale=p.gravity_scale;t.cycle=p.cycle;
        t.bitmap=0x50000000u+(clip_index<<8)+texture;t.frame_count=effect->materials.animations[texture].count;
        memcpy(&t.color,p.color,4);memcpy(&t.color_destination,p.color_destination,4);
        t.particle_flags=p.flags.particle;t.secondary=p.flags.secondary;t.age_to_finish_vbm=p.age_to_finish_vbm;
        status=rf_emitter_pool_create(&s->particles.state->emitters,&t,-1,
            room.room==UINT32_MAX?0:room.room+1,1,(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD),NULL,
            &s->particles.state->random,effect->instances[n].slots+i);if(status)goto failed;
        effect->instances[n].clock.live|=1u<<i;++rf_scene_script_explode_visual[1];
    }
    ++rf_scene_script_explode_visual[0];rf_scene_script_explode_visual[6]=clip_index;
    return RF_OK;
failed:
    rf_scene_script_explode_visual[7]=(uint32_t)status;
    for(i=0;i<6;i++)if(effect->instances[n].clock.live&(1u<<i))
        rf_emitter_pool_release(&s->particles.state->emitters,effect->instances[n].slots[i]);
    memset(effect->instances+n,0,sizeof(effect->instances[n]));return status;
}
static int scene_script_explode_effects_tick(scene_stream *s,uint32_t frame)
{
    uint32_t clip,n,i,active=0;int status;int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    for(clip=0;clip<64;clip++)if(scene_script_explode_effects[clip]) {
        scene_script_explode_effect_owner *effect=scene_script_explode_effects[clip];
        for(n=0;n<8;n++)if(effect->instances[n].clock.active) {
            rf_explosion_clock_actions actions;
            status=rf_explosion_clock_tick(&effect->definition.recipe,1.f/60,&effect->instances[n].clock,&actions);
            if(status)return status;
            for(i=0;i<effect->definition.recipe.central_count;i++)if(actions.process&(1u<<i)) {
                uint32_t slot=effect->instances[n].slots[i];rf_particle_emitter_update_result result;
                status=rf_particle_emitter_update(&s->particles.state->particles,&s->particles.state->slots[slot].runtime,
                    slot+1,1,1.f/60,now,NULL,0,&s->particles.state->random,&result);if(status)return status;
                ++rf_scene_script_explode_visual[2];
            }
            for(i=0;i<effect->definition.recipe.central_count;i++)if(actions.release&(1u<<i)) {
                status=rf_emitter_pool_release(&s->particles.state->emitters,effect->instances[n].slots[i]);if(status)return status;
                ++rf_scene_script_explode_visual[3];
            }
            if(effect->instances[n].clock.active)++active;
        }
    }
    rf_scene_script_explode_visual[5]=active;return RF_OK;
}
#endif
