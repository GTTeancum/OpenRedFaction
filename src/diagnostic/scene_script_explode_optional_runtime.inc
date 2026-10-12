#ifndef RF_SCENE_SCRIPT_EXPLODE_OPTIONAL_RUNTIME_INC
#define RF_SCENE_SCRIPT_EXPLODE_OPTIONAL_RUNTIME_INC
static float scene_script_explode_range(rf_random_state *random,float low,float high)
{
    uint32_t draw;rf_random_next(random,&draw);
    return (float)((double)low+((double)high-low)*((double)draw/32768.0));
}
/* Original48e5a0 packet: lifetime, velocity and radius draws, with only radius
 * scaled here. Sparks/head velocities are supplied separately below. This
 * cosmetic stream never consumes the central-emitter creation RNG. */
static int scene_script_explode_optional_packet(scene_script_explode_effect_owner *effect,
    uint32_t slot,uint32_t clip,float size,rf_random_state *random,rf_particle_spawn *out)
{
    const rf_particle_definition *p=effect->definition.emitters+slot;
    uint32_t texture=effect->materials.slot_texture[slot],i;float speed;
    rf_particle_spawn v={0};
    if(texture>=effect->materials.count || !isfinite(size) || size<=0)return RF_RANGE;
    v.growth=p->growth;v.acceleration=p->acceleration;v.gravity_scale=p->gravity_scale;
    v.bitmap=0x50000000u+(clip<<8)+texture;v.frame_count=effect->materials.animations[texture].count;
    memcpy(&v.color,p->color,4);memcpy(&v.color_destination,p->color_destination,4);
    v.flags=p->flags.particle;v.secondary=p->flags.secondary;v.age_to_finish_vbm=p->age_to_finish_vbm;
    v.life=scene_script_explode_range(random,p->min_life,p->max_life);
    speed=scene_script_explode_range(random,p->min_velocity,p->max_velocity);
    for(i=0;i<3;i++)v.velocity[i]=p->direction[i]*speed;
    v.radius=scene_script_explode_range(random,p->min_radius,p->max_radius)*size;
    if(!isfinite(v.life)||v.life<=0||!isfinite(v.radius)||v.radius<=0||
       !isfinite(v.growth)||!isfinite(v.acceleration)||!isfinite(v.gravity_scale)||
       !isfinite(v.age_to_finish_vbm)||(v.flags&0xf0000000u)||(v.secondary&1u))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(v.velocity[i]))return RF_RANGE;
    *out=v;return RF_OK;
}
static int scene_script_explode_optional_emit(scene_stream *s,scene_script_explode_trails *t,
    const rf_particle_spawn *spawn,uint32_t kind,uint32_t *index)
{
    uint32_t i,id;int status;rf_particle_pool *pool=&s->particles.state->particles;
    /* Pool0 is shared with blood/other bursts. Leave64 records available to
     * them; optional channels never allocate extra particles or emitters. */
    if(pool->live[0]>=RF_PARTICLE_POOL0_CAPACITY-64u){++rf_scene_script_explode_optional_stats[4];return RF_NOT_FOUND;}
    for(i=0;i<3;i++)if(!isfinite(spawn->position[i])||!isfinite(spawn->velocity[i]))return RF_RANGE;
    status=rf_particle_pool_create(pool,0,spawn,UINT32_MAX,t->room,0,&t->random,&id);
    if(status==RF_NOT_FOUND)++rf_scene_script_explode_optional_stats[4];
    else if(!status){++rf_scene_script_explode_optional_stats[5+kind];if(index)*index=id;}
    return status;
}
static void scene_script_explode_optional_start(scene_stream *s,scene_script_explode_effect_owner *effect,
    uint32_t n,uint32_t clip,const float position[3],uint32_t room,uint32_t frame,float size)
{
    static const float axis[3]={1,0,0};
    scene_script_explode_optional *o=effect->optional;scene_script_explode_trails *t;
    const rf_explosion_recipe *recipe=&effect->definition.recipe;
    rf_particle_pool *pool=&s->particles.state->particles;
    rf_particle_spawn packet;uint32_t i,k;float speed,extent;int status;
    if(!o || pool->recycle_sink!=scene_script_explode_recycled || pool->recycle_context!=pool)return;
    t=o->instances+n;memset(t,0,sizeof(*t));for(i=0;i<8;i++)t->head[i]=UINT32_MAX;
    t->room=room;t->random.value=s->particles.state->random.value^(frame*1664525u)^(clip*1013904223u)^(n+1u);
    if((o->mask&128u) && !(recipe->flags&1u) && size>=1){
        t->count=size>=8?8u:(uint32_t)scene_script_explode_range(&t->random,size-1,size+1);
        if(t->count>8)t->count=8;
    }
    /*48e9aa..48ea88: one packet, authored count, a new cone/speed per spark.
     * Bound malformed replacement counts without changing central admission. */
    if((o->mask&64u) && recipe->sparks_count>0 && recipe->sparks_count<=128){
        const rf_particle_definition *p=effect->definition.emitters+6;
        status=scene_script_explode_optional_packet(effect,6,clip,size,&t->random,&packet);
        if(!status){memcpy(packet.position,position,12);
            for(i=0;i<(uint32_t)recipe->sparks_count;i++){
                status=rf_particle_cone_oriented(axis,p->direction_random,&t->random,packet.velocity);if(status)break;
                speed=scene_script_explode_range(&t->random,p->min_velocity,p->max_velocity)*size;
                for(k=0;k<3;k++)packet.velocity[k]*=speed;
                status=scene_script_explode_optional_emit(s,t,&packet,0,NULL);
                if(status!=RF_OK && status!=RF_NOT_FOUND)break;
            }
        }
        if(status!=RF_OK && status!=RF_NOT_FOUND)rf_scene_script_explode_optional_stats[10]=(uint32_t)status;
    }
    if(!t->count)return;
    /*48ea8c..48ec48: <=8 stable seed positions/velocities, then initial heads.
     * Existing scripted wrapper direction is +X; Y/Z span its normal plane. */
    speed=scene_script_explode_range(&t->random,effect->definition.emitters[7].min_velocity,
        effect->definition.emitters[7].max_velocity)*size;
    extent=recipe->head_random*size;
    if(!isfinite(speed)||!isfinite(extent)||extent<0){t->count=0;return;}
    for(i=0;i<t->count;i++){
        memcpy(t->position[i],position,12);
        t->position[i][1]+=scene_script_explode_range(&t->random,-extent,extent);
        t->position[i][2]+=scene_script_explode_range(&t->random,-extent,extent);
        status=rf_particle_cone_oriented(axis,.7f,&t->random,t->velocity[i]);
        if(status){t->count=0;rf_scene_script_explode_optional_stats[10]=(uint32_t)status;return;}
        for(k=0;k<3;k++)t->velocity[i][k]*=speed;
    }
    status=scene_script_explode_optional_packet(effect,7,clip,size,&t->random,&packet);
    if(status){t->count=0;rf_scene_script_explode_optional_stats[10]=(uint32_t)status;return;}
    for(i=0;i<t->count;i++){
        memcpy(packet.position,t->position[i],12);memcpy(packet.velocity,t->velocity[i],12);
        status=scene_script_explode_optional_emit(s,t,&packet,1,t->head+i);
        if(status!=RF_OK && status!=RF_NOT_FOUND)rf_scene_script_explode_optional_stats[10]=(uint32_t)status;
    }
}
static void scene_script_explode_optional_tick(scene_stream *s,scene_script_explode_effect_owner *effect,
    uint32_t n,uint32_t clip)
{
    scene_script_explode_optional *o=effect->optional;scene_script_explode_trails *t;
    const rf_explosion_clock *clock=&effect->instances[n].clock;
    rf_particle_pool *pool=&s->particles.state->particles;
    rf_particle_spawn packet;uint32_t i,slot;int status;
    if(!o)return;t=o->instances+n;if(!t->count)return;
    if(pool->recycle_sink!=scene_script_explode_recycled || pool->recycle_context!=pool){t->count=0;return;}
    /*48e2ca/48e386: strict head-time boundary, then tails at the last live
     * head's previous position (+6c). Do not advance gameplay or particles here. */
    slot=clock->elapsed<effect->definition.recipe.head_time?7u:8u;
    if(!(o->mask&(1u<<slot)))return;
    status=scene_script_explode_optional_packet(effect,slot,clip,clock->size,&t->random,&packet);
    if(status){t->count=0;rf_scene_script_explode_optional_stats[10]=(uint32_t)status;return;}
    for(i=0;i<t->count;i++){
        if(slot==7){memcpy(packet.position,t->position[i],12);memcpy(packet.velocity,t->velocity[i],12);}
        else{
            const rf_particle *head;
            if(t->head[i]==UINT32_MAX)continue;
            if(t->head[i]>=RF_PARTICLE_POOL0_CAPACITY){t->head[i]=UINT32_MAX;continue;}
            head=pool->particles+t->head[i];
            if(!(head->flags&1u)){t->head[i]=UINT32_MAX;continue;}
            memcpy(packet.position,head->previous_position,12);
        }
        status=scene_script_explode_optional_emit(s,t,&packet,slot==7?1u:2u,slot==7?t->head+i:NULL);
        if(status!=RF_OK && status!=RF_NOT_FOUND)rf_scene_script_explode_optional_stats[10]=(uint32_t)status;
    }
}
/* Final successful-load publication only. Clear this timeline's effects,
 * including detached central particles; keep decoded resources available for
 * later ordinary activation. Failed/absent loads do not call this function. */
static void scene_script_explode_effects_reset(scene_stream *s)
{
    uint32_t clip,n,i;rf_particle_pool *pool;
    if(!s || !s->particles.state)return;pool=&s->particles.state->particles;
    for(clip=0;clip<64;clip++)if(scene_script_explode_effects[clip]){
        scene_script_explode_effect_owner *e=scene_script_explode_effects[clip];
        for(n=0;n<8;n++)for(i=0;i<6;i++)if(e->instances[n].clock.live&(1u<<i))
            (void)rf_emitter_pool_release(&s->particles.state->emitters,e->instances[n].slots[i]);
        memset(e->instances,0,sizeof(e->instances));
        if(e->optional)memset(e->optional->instances,0,sizeof(e->optional->instances));
    }
    for(i=0;i<RF_PARTICLE_CAPACITY;i++)if((pool->particles[i].flags&1u) &&
       pool->particles[i].bitmap>=0x50000000u && pool->particles[i].bitmap<0x50004000u)
        (void)rf_particle_pool_recycle(pool,i);
    rf_scene_script_explode_visual[5]=0;++rf_scene_script_explode_optional_stats[13];
}
#endif
