#ifndef RF_SCENE_FUSION_EFFECTS_RUNTIME_INC
#define RF_SCENE_FUSION_EFFECTS_RUNTIME_INC
/* Authored central emitters; trail head/tail ownership remains deferred. */
static __declspec(noinline) int scene_fusion_impact_start(scene_stream *s,const rf_weapon_flight_contact *hit,uint32_t frame,float radius)
{
    uint32_t n,i;int status;int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!isfinite(radius) || radius<=0)return RF_RANGE;
    if(!s->impact || !s->particles.state || !scene_fusion_effects || !scene_fusion_effects->materials.count)return RF_OK;
    for(n=0;n<4 && scene_fusion_effects->instances[n].clock.active;n++){}
    if(n==4 || s->particles.state->emitters.live>RF_PARTICLE_EMITTER_CAPACITY-3)return RF_OK;
    if(scene_fusion_effects->definition.recipe.central_count>3 || scene_fusion_effects->definition.recipe.central_random!=0)return RF_FORMAT;
    memset(scene_fusion_effects->instances+n,0,sizeof(scene_fusion_effects->instances[n]));
    scene_fusion_effects->instances[n].clock.size=radius;scene_fusion_effects->instances[n].clock.active=1;
    for(i=0;i<scene_fusion_effects->definition.recipe.central_count;i++) {
        rf_particle_definition p;rf_particle_emitter_template t={0};float extent;uint32_t texture,j;
        status=rf_explosion_central_prepare(&scene_fusion_effects->definition,i,radius,&p,&extent);
        if(status==RF_NOT_FOUND)continue;if(status)return status;
        texture=scene_fusion_effects->materials.slot_texture[i];if(texture>=scene_fusion_effects->materials.count)return RF_RANGE;
        for(j=0;j<3;j++){t.position[j]=hit->hit.point[j]+hit->hit.normal[j]*.01f;t.direction[j]=hit->hit.normal[j];}
        t.source_id=i;t.direction_random=p.direction_random;t.min_velocity=p.min_velocity;t.max_velocity=p.max_velocity;
        t.spawn_radius=p.spawn_radius;t.min_spawn_delay=p.min_spawn_delay;t.max_spawn_delay=p.max_spawn_delay;
        t.flags=p.flags.emitter;t.min_life=p.min_life;t.max_life=p.max_life;t.min_radius=p.min_radius;t.max_radius=p.max_radius;
        t.growth=p.growth;t.acceleration=p.acceleration;t.gravity_scale=p.gravity_scale;t.cycle=p.cycle;
        t.bitmap=s->particles.materials.texture_count+s->impact->materials.count+2+(scene_flame_effects?scene_flame_effects->materials.count:0)+texture;t.frame_count=scene_fusion_effects->materials.animations[texture].count;
        memcpy(&t.color,p.color,4);memcpy(&t.color_destination,p.color_destination,4);
        t.particle_flags=p.flags.particle;t.secondary=p.flags.secondary;t.age_to_finish_vbm=p.age_to_finish_vbm;
        status=rf_emitter_pool_create(&s->particles.state->emitters,&t,-1,hit->room+1,1,now,NULL,
            &s->particles.state->random,scene_fusion_effects->instances[n].slots+i);if(status)return status;
        scene_fusion_effects->instances[n].clock.live|=1u<<i;

    }
    if(rf_scene_combat_trace)printf("FUSION_IMPACT_START %u %u %u %u\n",frame,n,scene_fusion_effects->instances[n].clock.live,s->particles.state->particles.live[1]);
    return RF_OK;
}
static int scene_fusion_impacts_tick(scene_stream *s,uint32_t frame)
{
    uint32_t n,i;int status;int32_t now=(int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);
    if(!scene_fusion_effects)return RF_OK;
    for(n=0;n<4;n++)if(scene_fusion_effects->instances[n].clock.active) {
        rf_explosion_clock_actions actions;
        status=rf_explosion_clock_tick(&scene_fusion_effects->definition.recipe,1.f/60,&scene_fusion_effects->instances[n].clock,&actions);if(status)return status;
        for(i=0;i<scene_fusion_effects->definition.recipe.central_count;i++)if(actions.process&(1u<<i)) {
            uint32_t slot=scene_fusion_effects->instances[n].slots[i];rf_particle_emitter_update_result result;
            status=rf_particle_emitter_update(&s->particles.state->particles,&s->particles.state->slots[slot].runtime,
                slot+1,1,1.f/60,now,NULL,0,&s->particles.state->random,&result);if(status)return status;
        }
        for(i=0;i<scene_fusion_effects->definition.recipe.central_count;i++)if(actions.release&(1u<<i)) {
            status=rf_emitter_pool_release(&s->particles.state->emitters,scene_fusion_effects->instances[n].slots[i]);if(status)return status;
        }
        if(actions.release && rf_scene_combat_trace)printf("FUSION_IMPACT_RELEASE %u %u %u\n",frame,n,actions.release);
    }
    return RF_OK;
}

#endif
