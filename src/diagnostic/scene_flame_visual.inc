#ifndef RF_SCENE_FLAME_VISUAL_INC
#define RF_SCENE_FLAME_VISUAL_INC
/* Flame stream, original single-player path: primary fire of a flame weapon
 * (4c90f0: weapons.tbl Flags2 "flame", info+0x268 bit 6) reaches 41a870, which
 * creates the shooter's stream once (entity+0x13d4 = 48edc0) and then 41ac60
 * aims it every frame at the muzzle (player: view-model muzzle tag; NPC:
 * 41b5a0 held-weapon muzzle) through 48f2a0, switched on/off by 48f130
 * (emitter+0x140 via 4973b0/4973d0). 48edc0 creates two continuous emitters
 * owned by the shooter: emitters.tbl "youfired" (Fire01.tga) at the muzzle and
 * "youfiredtoo" (Flame01_02.tga) 0.2 units further along the aim; multiplayer
 * uses the _multi variants instead. Their particles fly 8-10 m/s, grow
 * 3.5-4.5 units/s, collide and stretch along their velocity. Port: each
 * shooter's two emitters are trail entries (scene_rocket_trail.inc, kind
 * SCENE_TRAIL_FLAME) released when the shooter stops firing, which is how an
 * off emitter looks (live particles keep flying). The fallback below is the
 * earlier stand-in, used only when those emitters cannot load.
 *
 * Fallback: first-pass sampled flame jet using installed flamethrower fire metadata and
 * existing Fire01.tga impact texture. Authored particle color/lifetime/radius/
 * growth/gravity retained; eight samples every3ticks is explicit port policy,
 * not a reconstruction of original flame head/trail dispatch. Parent supplies
 * collision-clamped visible_length; this helper owns no damage or fuel logic.
 * Include after scene_stream/campaign_rocket_impact declarations. Forward declare
 * open before table loading if needed. Existing particle simulation and drawing
 * owns every spawned sprite, including scope projection; no extra draw hook. */
static rf_particle_definition scene_flame_particle;
static uint32_t scene_flame_visual_ready;
/* active ticks, particles spawned, pool misses, peak live, bitmap, status */
uint32_t rf_scene_flame_visual[6];
static int scene_flame_visual_open(rf_vpp *tables)
{
    rf_particle_definition definition;int status;
    scene_flame_visual_ready=0;
    status=rf_emitter_definition_load(tables,"flamethrower fire",128*1024,&definition);if(status)return status;
    scene_flame_particle=definition;scene_flame_visual_ready=1;return RF_OK;
}
static int scene_flame_visual_same_bitmap(const char *a,const char *b)
{
    uint32_t i;for(i=0;i<64;i++){
        unsigned char x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return 1;
    }return 0;
}
/* Last frame each flame trail entry was aimed; entries idle for more than a
 * couple of frames belong to a shooter that stopped and are released. */
static uint32_t scene_flame_stream_frame[SCENE_TRAIL_ENTRIES];
/* streams aimed, emitters created, create misses, particles created, releases */
uint32_t rf_scene_flame_stream[5];
static int scene_flame_stream_stop(scene_stream *s,uint32_t shooter)
{
    uint32_t k;int status;
    for(k=0;k<2;k++)if(scene_trail_find(SCENE_TRAIL_FLAME<<16|((shooter*2+k)&0xffffu))){
        status=scene_projectile_trail_stop(s,SCENE_TRAIL_FLAME,(shooter*2+k)&0xffffu);if(status)return status;++rf_scene_flame_stream[4];}
    return RF_OK;
}
static int scene_flame_stream_sweep(scene_stream *s,uint32_t frame)
{
    uint32_t i;int status;
    for(i=0;i<SCENE_TRAIL_ENTRIES;i++)if(scene_trail_entries[i].key>>16==SCENE_TRAIL_FLAME && frame-scene_flame_stream_frame[i]>4u){
        status=scene_projectile_trail_stop(s,SCENE_TRAIL_FLAME,scene_trail_entries[i].key&0xffffu);if(status)return status;++rf_scene_flame_stream[4];}
    return RF_OK;
}
/* 48edc0 on first use, then 48f2a0 + 4972f0 each update while firing. */
static int scene_flame_stream_tick(scene_stream *s,uint32_t frame,uint32_t shooter,const float muzzle[3],const float direction[3])
{
    uint32_t k,j;int status;float basis[9];
    scene_trail_basis(direction,basis);++rf_scene_flame_stream[0];
    for(k=0;k<2;k++) {
        uint32_t index=(shooter*2+k)&0xffffu,key=SCENE_TRAIL_FLAME<<16|index,slot,i;scene_trail_entry *e=scene_trail_find(key);
        rf_collision_room_location location;rf_particle_emitter_update_result result;
        if(!e) {
            for(i=0;i<SCENE_TRAIL_ENTRIES && scene_trail_entries[i].key;i++){}
            if(i==SCENE_TRAIL_ENTRIES){++rf_scene_flame_stream[2];return RF_OK;}
            e=scene_trail_entries+i;memset(e,0,sizeof(*e));
            scene_trail_generation=(scene_trail_generation+1)&0x7ffffu;
            e->handle=SCENE_TRAIL_HANDLE|(scene_trail_generation<<6)|i;
        }
        for(j=0;j<3;j++){e->parent.position[j]=muzzle[j]+direction[j]*(k?.2f:0);e->parent.velocity[j]=0;}
        memcpy(e->parent.basis,basis,36);e->parent.remaining_life=1;e->parent.handle=e->handle;
        status=rf_geometry_collision_world_locate(s->collision,e->parent.position,&location);if(status)return status;
        e->room=location.room==UINT32_MAX?0:location.room+1;
        scene_flame_stream_frame[e-scene_trail_entries]=frame;
        if(!e->key) {
            status=rf_emitter_pool_create(&s->particles.state->emitters,scene_trail_templates+scene_flame_stream_type[k],(int32_t)e->handle,
                e->room,1,scene_trail_now(frame),&e->parent,&s->particles.state->random,&slot);
            if(status==RF_NOT_FOUND){memset(e,0,sizeof(*e));++rf_scene_flame_stream[2];return RF_OK;}
            if(status){memset(e,0,sizeof(*e));return status;}
            e->key=key;e->slot=slot;++rf_scene_flame_stream[1];
        }
        /* "dont_move_with_parent" emitters ignore the parent pose; 48f2a0
         * writes the muzzle and aim into the stream's own storage instead. */
        {rf_particle_emitter *emitter=&s->particles.state->slots[e->slot].runtime.emitter;
         memcpy(emitter->position,e->parent.position,12);memcpy(emitter->direction,direction,12);emitter->room=e->room;}
        status=rf_particle_emitter_update(&s->particles.state->particles,&s->particles.state->slots[e->slot].runtime,
            e->slot+1,1,scene_step_seconds,scene_trail_now(frame),&e->parent,e->room,&s->particles.state->random,&result);
        if(status)return status;
        rf_scene_flame_stream[3]+=result.created;
    }
    return RF_OK;
}
static int scene_flame_visual_tick(scene_stream *s,uint32_t frame,uint32_t shooter,const float muzzle[3],
    const float forward[3],float visible_length,uint32_t active)
{
    uint32_t i,k,texture=UINT32_MAX,bitmap;float direction[3];double norm=0;int status;
    if(!frame)memset(rf_scene_flame_visual,0,sizeof(rf_scene_flame_visual));
    if(!s || !muzzle || !forward || active>1 || !isfinite(visible_length) || visible_length<0 || visible_length>100)return RF_RANGE;
    if(scene_rocket_trail_ready && scene_flame_stream_type[0]!=UINT32_MAX && scene_flame_stream_type[1]!=UINT32_MAX &&
       s->particles.state && s->collision) {
        status=scene_flame_stream_sweep(s,frame);if(status)return status;
        if(!active)return scene_flame_stream_stop(s,shooter);
        for(i=0;i<3;i++){if(!isfinite(muzzle[i]) || !isfinite(forward[i]))return RF_RANGE;norm+=(double)forward[i]*forward[i];}
        if(norm<1e-20)return RF_RANGE;norm=sqrt(norm);for(i=0;i<3;i++)direction[i]=(float)(forward[i]/norm);
        return scene_flame_stream_tick(s,frame,shooter,muzzle,direction);
    }
    if(!active || visible_length<=.1f)return RF_OK;
    if(!scene_flame_visual_ready || !s->particles.state || !s->impact)return RF_NOT_FOUND;
    for(i=0;i<3;i++){if(!isfinite(muzzle[i]) || !isfinite(forward[i]))return RF_RANGE;norm+=(double)forward[i]*forward[i];}
    if(norm<1e-20)return RF_RANGE;norm=sqrt(norm);for(i=0;i<3;i++)direction[i]=(float)(forward[i]/norm);
    for(i=0;i<9;i++)if((campaign_rocket_impact.resolved&(1u<<i)) &&
        scene_flame_visual_same_bitmap(scene_flame_particle.bitmap,campaign_rocket_impact.emitters[i].bitmap)){
        texture=s->impact->materials.slot_texture[i];break;}
    if(texture>=s->impact->materials.count || !s->impact->materials.animations[texture].count)return RF_NOT_FOUND;
    bitmap=s->particles.materials.texture_count+texture;rf_scene_flame_visual[4]=bitmap;
    ++rf_scene_flame_visual[0];if(rf_scene_flame_visual[0]%3!=1)return RF_OK;
    for(i=0;i<8;i++){
        rf_particle_spawn spawn={0};rf_collision_room_location location;uint32_t random,index;
        float t,speed,mix;
        status=rf_random_next(&s->particles.state->random,&random);if(status)return status;
        mix=(float)(random&0xffffu)*(1.f/65535);t=((float)i+mix)*(1.f/8);
        speed=scene_flame_particle.min_velocity+(scene_flame_particle.max_velocity-scene_flame_particle.min_velocity)*mix;
        for(k=0;k<3;k++){spawn.position[k]=muzzle[k]+direction[k]*(.1f+(visible_length-.1f)*t);spawn.velocity[k]=direction[k]*speed;}
        status=rf_geometry_collision_world_locate(s->collision,spawn.position,&location);if(status)return status;
        spawn.radius=scene_flame_particle.min_radius+(scene_flame_particle.max_radius-scene_flame_particle.min_radius)*mix;
        spawn.life=scene_flame_particle.min_life+(scene_flame_particle.max_life-scene_flame_particle.min_life)*mix;
        spawn.growth=scene_flame_particle.growth;spawn.acceleration=scene_flame_particle.acceleration;spawn.gravity_scale=scene_flame_particle.gravity_scale;
        spawn.bitmap=bitmap;spawn.frame_count=s->impact->materials.animations[texture].count;
        memcpy(&spawn.color,scene_flame_particle.color,4);memcpy(&spawn.color_destination,scene_flame_particle.color_destination,4);
        spawn.flags=scene_flame_particle.flags.particle;spawn.secondary=scene_flame_particle.flags.secondary;
        spawn.age_to_finish_vbm=scene_flame_particle.age_to_finish_vbm;
        /* Standalone sprites belong to pool0/list2. Pool1/list3 is not a
         * simulated/rendered standalone list: pool1 is reserved for emitters. */
        status=rf_particle_pool_create(&s->particles.state->particles,0,&spawn,UINT32_MAX,
            location.room==UINT32_MAX?0:location.room+1,0,&s->particles.state->random,&index);
        if(status==RF_NOT_FOUND){++rf_scene_flame_visual[2];break;}if(status){rf_scene_flame_visual[5]=(uint32_t)status;return status;}
        ++rf_scene_flame_visual[1];
    }
    if(s->particles.state->particles.live[0]>rf_scene_flame_visual[3])rf_scene_flame_visual[3]=s->particles.state->particles.live[0];
    return RF_OK;
}
#endif
