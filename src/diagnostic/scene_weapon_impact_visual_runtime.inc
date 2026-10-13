#ifndef RF_SCENE_WEAPON_IMPACT_VISUAL_RUNTIME_INC
#define RF_SCENE_WEAPON_IMPACT_VISUAL_RUNTIME_INC
/* Include after ordinary impact audio, before its NPC adapter. All inputs are
 * receipts from accepted fired rays; no visibility or Use query calls this.
 * Original4c8a10 enumerates clips in order with paired authored radii. */
static void scene_weapon_impact_visual_error(int status)
{if(status){++rf_scene_weapon_impact_visual[7];rf_scene_weapon_impact_visual[8]=(uint32_t)status;}}
static float scene_weapon_impact_visual_range(rf_random_state *random,float low,float high)
{uint32_t draw;rf_random_next(random,&draw);return (float)((double)low+((double)high-low)*((double)draw/32768.0));}
static void scene_weapon_impact_visual_packet(const rf_particle_definition *d,
    uint32_t texture,rf_particle_spawn *p)
{
    memset(p,0,sizeof(*p));p->bitmap=SCENE_WEAPON_IMPACT_VISUAL_TAG+texture;
    p->frame_count=scene_weapon_impact_visual->materials.animations[texture].count;
    p->growth=d->growth;p->acceleration=d->acceleration;p->gravity_scale=d->gravity_scale;
    memcpy(&p->color,d->color,4);memcpy(&p->color_destination,d->color_destination,4);
    p->flags=d->flags.particle;p->secondary=d->flags.secondary;p->age_to_finish_vbm=d->age_to_finish_vbm;
}
static uint32_t scene_weapon_impact_visual_owned_slot(const rf_level_particle_state *state,
    const scene_weapon_impact_visual_instance *instance,uint32_t index)
{
    return state&&instance->slot<RF_PARTICLE_EMITTER_CAPACITY&&
        state->slots[instance->slot].active&&
        state->slots[instance->slot].source_id==SCENE_WEAPON_IMPACT_VISUAL_TAG+index;
}
static void scene_weapon_impact_visual_reset(scene_stream *stream,uint32_t loaded)
{
    scene_weapon_impact_visual_owner *v=scene_weapon_impact_visual;uint32_t i;int status;
    if(!v)return;
    if(stream&&stream->particles.state){rf_level_particle_state *state=stream->particles.state;
        for(i=0;i<SCENE_WEAPON_IMPACT_VISUAL_INSTANCES;i++)if(v->instances[i].clock.active&&
            scene_weapon_impact_visual_owned_slot(state,v->instances+i,i)){
            status=rf_emitter_pool_release(&state->emitters,v->instances[i].slot);scene_weapon_impact_visual_error(status);
        }
        /* Successful load only: these unsaved cosmetic records belong to the
         * old timeline. Do not recycle level, blood or other effect particles. */
        for(i=0;i<RF_PARTICLE_CAPACITY;i++)if((state->records[i].flags&1u)&&
            state->records[i].bitmap>=SCENE_WEAPON_IMPACT_VISUAL_TAG&&
            state->records[i].bitmap<SCENE_WEAPON_IMPACT_VISUAL_TAG+9u){
            status=rf_particle_pool_recycle(&state->particles,i);scene_weapon_impact_visual_error(status);
        }
    }
    memset(v->instances,0,sizeof(v->instances));rf_scene_weapon_impact_visual[12]=0;
    if(loaded){v->random.value=1;++rf_scene_weapon_impact_visual[13];}
}
static void scene_weapon_impact_visual_tick(scene_stream *stream)
{
    scene_weapon_impact_visual_owner *v=scene_weapon_impact_visual;uint32_t i,active=0;
    if(!v||!stream||!stream->particles.state)return;
    for(i=0;i<SCENE_WEAPON_IMPACT_VISUAL_INSTANCES;i++)if(v->instances[i].clock.active){
        scene_weapon_impact_visual_instance *instance=v->instances+i;rf_explosion_clock_actions actions;int status;
        if(!scene_weapon_impact_visual_owned_slot(stream->particles.state,instance,i)){
            memset(instance,0,sizeof(*instance));scene_weapon_impact_visual_error(RF_FORMAT);continue;
        }
        status=rf_explosion_clock_tick(&v->definitions[instance->kind].recipe,scene_step_seconds,&instance->clock,&actions);
        if(status || actions.release){int released=rf_emitter_pool_release(&stream->particles.state->emitters,instance->slot);
            scene_weapon_impact_visual_error(status);scene_weapon_impact_visual_error(released);
            memset(instance,0,sizeof(*instance));
        }else ++active;
    }
    rf_scene_weapon_impact_visual[12]=active;
}
static int scene_weapon_impact_visual_central(scene_stream *stream,uint32_t kind,float radius,
    const float position[3],const float normal[3],uint32_t room)
{
    scene_weapon_impact_visual_owner *v=scene_weapon_impact_visual;
    rf_particle_emitter_template t={0};rf_particle_definition d;
    scene_weapon_impact_visual_instance *instance;uint32_t i,texture,slot;float extent;int status;
    for(i=0;i<SCENE_WEAPON_IMPACT_VISUAL_INSTANCES&&v->instances[i].clock.active;i++){}
    if(i==SCENE_WEAPON_IMPACT_VISUAL_INSTANCES||stream->particles.state->emitters.live==RF_PARTICLE_EMITTER_CAPACITY){
        ++rf_scene_weapon_impact_visual[5];return RF_OK;
    }
    status=rf_explosion_central_prepare(v->definitions+kind,0,radius,&d,&extent);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    texture=v->materials.slot_texture[kind*2];if(texture>=v->materials.count)return RF_RANGE;
    memcpy(t.position,position,12);memcpy(t.direction,normal,12);t.source_id=SCENE_WEAPON_IMPACT_VISUAL_TAG+i;
    t.direction_random=d.direction_random;t.min_velocity=d.min_velocity;t.max_velocity=d.max_velocity;
    t.spawn_radius=d.spawn_radius;t.min_spawn_delay=d.min_spawn_delay;t.max_spawn_delay=d.max_spawn_delay;
    t.flags=d.flags.emitter;t.min_life=d.min_life;t.max_life=d.max_life;t.min_radius=d.min_radius;t.max_radius=d.max_radius;
    t.growth=d.growth;t.acceleration=d.acceleration;t.gravity_scale=d.gravity_scale;t.cycle=d.cycle;
    t.bitmap=SCENE_WEAPON_IMPACT_VISUAL_TAG+texture;t.frame_count=v->materials.animations[texture].count;
    memcpy(&t.color,d.color,4);memcpy(&t.color_destination,d.color_destination,4);
    t.particle_flags=d.flags.particle;t.secondary=d.flags.secondary;t.age_to_finish_vbm=d.age_to_finish_vbm;
    status=rf_emitter_pool_create(&stream->particles.state->emitters,&t,-1,room+1,1,
        (int32_t)(((uint64_t)combat_frame*1000/60)%RF_TIMER_PERIOD),NULL,&v->random,&slot);
    if(status==RF_NOT_FOUND){++rf_scene_weapon_impact_visual[5];return RF_OK;}if(status)return status;
    instance=v->instances+i;memset(instance,0,sizeof(*instance));
    instance->kind=kind;instance->slot=slot;instance->clock.active=1;instance->clock.live=1;instance->clock.size=radius;
    ++rf_scene_weapon_impact_visual[2];++rf_scene_weapon_impact_visual[12];return RF_OK;
}
/*48e5a0 samples one shared spark radius/lifetime;48e9b7..48ea88 emits the
 * recipe's exact count into global pool0. Velocity and radius scale by size,
 * lifetime does not. Cosmetic RNG is owned here, never combat/spread RNG. */
static int scene_weapon_impact_visual_sparks(scene_stream *stream,uint32_t kind,float size,
    const float position[3],const float normal[3],uint32_t room)
{
    scene_weapon_impact_visual_owner *v=scene_weapon_impact_visual;rf_particle_spawn p;
    const rf_explosion_definition *d=v->definitions+kind;const rf_particle_definition *spark=d->emitters+6;
    uint32_t texture=v->materials.slot_texture[kind*2+1],n,j,index;float speed;int status;
    if(texture>=v->materials.count)return RF_RANGE;
    scene_weapon_impact_visual_packet(spark,texture,&p);memcpy(p.position,position,12);
    p.radius=scene_weapon_impact_visual_range(&v->random,spark->min_radius,spark->max_radius)*size;
    /*48e5a0's preliminary velocity draw is overwritten per spark but consumed. */
    (void)scene_weapon_impact_visual_range(&v->random,spark->min_velocity,spark->max_velocity);
    p.life=scene_weapon_impact_visual_range(&v->random,spark->min_life,spark->max_life);
    for(n=0;n<(uint32_t)d->recipe.sparks_count;n++){
        status=rf_particle_cone_oriented(normal,spark->direction_random,&v->random,p.velocity);if(status)return status;
        speed=scene_weapon_impact_visual_range(&v->random,spark->min_velocity,spark->max_velocity)*size;
        for(j=0;j<3;j++)p.velocity[j]*=speed;
        status=rf_particle_pool_create(&stream->particles.state->particles,0,&p,UINT32_MAX,room+1,0,&v->random,&index);
        if(status==RF_NOT_FOUND){++rf_scene_weapon_impact_visual[5];continue;}if(status)return status;
        ++rf_scene_weapon_impact_visual[3];
    }return RF_OK;
}
/*4c1b8c/4c1ccd compares integer births against count*size (rounds up).
 * gatsmoke lacks radius_in_multiples: its sprite radius/life/speed are NOT
 * scaled by the weapon radius. Source offset/spawn radius are exactly zero. */
static int scene_weapon_impact_visual_smoke(scene_stream *stream,float size,
    const float position[3],const float normal[3],uint32_t room)
{
    scene_weapon_impact_visual_owner *v=scene_weapon_impact_visual;
    const rf_particle_definition *d=&v->smoke.particle;rf_particle_spawn p;
    uint32_t texture=v->materials.slot_texture[6],n,j,index;float speed,count;int status;
    if(texture>=v->materials.count)return RF_RANGE;
    count=(float)v->smoke.particle_count*size;
    for(n=0;(float)n<count;n++){
        scene_weapon_impact_visual_packet(d,texture,&p);memcpy(p.position,position,12);
        status=rf_particle_cone_oriented(normal,d->direction_random,&v->random,p.velocity);if(status)return status;
        speed=scene_weapon_impact_visual_range(&v->random,d->min_velocity,d->max_velocity);
        for(j=0;j<3;j++)p.velocity[j]*=speed;
        p.radius=scene_weapon_impact_visual_range(&v->random,d->min_radius,d->max_radius);
        p.life=scene_weapon_impact_visual_range(&v->random,d->min_life,d->max_life);
        p.flags|=0x2000u;p.secondary=0; /* Authored no_z_check; original direct burst clears secondary. */
        status=rf_particle_pool_create(&stream->particles.state->particles,0,&p,UINT32_MAX,room+1,0,&v->random,&index);
        if(status==RF_NOT_FOUND){++rf_scene_weapon_impact_visual[5];continue;}if(status)return status;
        ++rf_scene_weapon_impact_visual[4];
    }return RF_OK;
}
static void scene_weapon_impact_visual_contact(scene_stream *stream,int32_t weapon,
    const rf_collision_solid_hit *hit)
{
    scene_weapon_impact_visual_owner *v=scene_weapon_impact_visual;
    const scene_weapon_impact_visual_profile *profile;rf_collision_room_location location;
    float normal[3],probe[3];double squared=0;uint32_t i,room;int status;
    if(!scene_weapon_impact_visual_ready(weapon)||!stream||!stream->particles.state||!hit)return;
    ++rf_scene_weapon_impact_visual[1];profile=v->profiles+weapon;
    for(i=0;i<3;i++){
        if(!isfinite(hit->hit.point[i])||!isfinite(hit->hit.normal[i])){scene_weapon_impact_visual_error(RF_FORMAT);return;}
        squared+=(double)hit->hit.normal[i]*hit->hit.normal[i];
    }
    if(!isfinite(squared)||squared<1e-12){scene_weapon_impact_visual_error(RF_FORMAT);return;}
    for(i=0;i<3;i++){normal[i]=(float)(hit->hit.normal[i]/sqrt(squared));probe[i]=hit->hit.point[i]+normal[i]*.001f;}
    /* Room lookup alone uses a small outward probe so a surface boundary does
     * not classify a particle as inside solid. Births retain exact hit.point. */
    status=rf_geometry_collision_world_locate(stream->collision,probe,&location);
    if(status){scene_weapon_impact_visual_error(status);return;}room=location.room;
    if(room==UINT32_MAX||room>=stream->collision->room_count){++rf_scene_weapon_impact_visual[6];return;}
    if(stream->collision->contains_liquid&&stream->collision->contains_liquid[room]){
        const rf_liquid_room *liquid;
        if(!stream->liquid_rooms||room>=stream->swim_room_count){++rf_scene_weapon_impact_visual[6];return;}
        liquid=stream->liquid_rooms+room;
        if(!isfinite(liquid->minimum_y)||!isfinite(liquid->depth)||
           hit->hit.point[1]<=(float)((double)liquid->minimum_y+liquid->depth)){
            ++rf_scene_weapon_impact_visual[6];return;
        }
    }
    for(i=0;i<profile->count;i++){
        uint32_t kind=profile->kinds[i];float radius=profile->radii[i];
        if(kind<3){status=scene_weapon_impact_visual_central(stream,kind,radius,hit->hit.point,normal,room);
            if(!status)status=scene_weapon_impact_visual_sparks(stream,kind,radius,hit->hit.point,normal,room);
        }else status=scene_weapon_impact_visual_smoke(stream,radius,hit->hit.point,normal,room);
        if(status){scene_weapon_impact_visual_error(status);return;}
    }
}
/* Shared actual ordered-world feedback receipt, moved from the existing
 * audio-only helper. Keep its query0x26, target limit and audio path unchanged;
 * optional visual admission is independent of PCM admission. One ray result
 * supplies both consumers; no synthesized point/normal or duplicate ray. */
static void scene_weapon_impact_world(scene_stream *stream,int32_t weapon,
    const float start[3],const float delta[3],float limit)
{
    rf_collision_solid_hit hit;rf_geometry_materials mapping={0};
    rf_geometry_body_surfaces surfaces;float end[3];uint32_t matched=0,i,texture,material=0;
    uint32_t audio=scene_weapon_impact_audio_ready(weapon),visual=scene_weapon_impact_visual_ready(weapon);
    int status,material_status=RF_NOT_FOUND;
    if(!audio&&!visual)return;
    if(!stream||!stream->collision||!start||!delta||!isfinite(limit)||limit<0||limit>1){status=RF_RANGE;goto failed;}
    for(i=0;i<3;i++){
        if(!isfinite(start[i])||!isfinite(delta[i])){status=RF_FORMAT;goto failed;}
        end[i]=start[i]+delta[i]*limit;
    }
    if(audio)++rf_scene_weapon_impact_audio[4];if(visual)++rf_scene_weapon_impact_visual[14];
    status=rf_geometry_collision_ray(stream->collision,&campaign_movers,start,end,0x26,&hit,&matched);
    if(status)goto failed;if(!matched)return;
    if(audio){
        ++rf_scene_weapon_impact_audio[5];
        if(actor_follow_world&&campaign_surface_palette&&campaign_surface_sources){
            mapping.offsets=actor_follow_world->offsets;mapping.slots=actor_follow_world->slots;
            mapping.count=actor_follow_world->geometry_count;mapping.textures.count=actor_follow_world->material_count;
            surfaces.geometries=campaign_surface_sources;surfaces.count=mapping.count;
            surfaces.mapping=&mapping;surfaces.palette=campaign_surface_palette;
            material_status=campaign_body_surface(&surfaces,hit.solid_index,hit.face_index,&texture,&material);
        }
        scene_weapon_impact_audio_contact(weapon,material_status?-1:(int32_t)material,hit.hit.point);
        if(material_status){++rf_scene_weapon_impact_audio[3];rf_scene_weapon_impact_audio[8]=(uint32_t)material_status;}
    }
    if(visual){++rf_scene_weapon_impact_visual[15];scene_weapon_impact_visual_contact(stream,weapon,&hit);}
    return;
failed:
    if(audio){++rf_scene_weapon_impact_audio[3];rf_scene_weapon_impact_audio[8]=(uint32_t)status;}
    if(visual)scene_weapon_impact_visual_error(status);
}
#endif
