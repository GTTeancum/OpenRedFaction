#ifndef RF_SCENE_FLAME_VISUAL_INC
#define RF_SCENE_FLAME_VISUAL_INC
/* First-pass sampled flame jet using installed flamethrower fire metadata and
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
static int scene_flame_visual_tick(scene_stream *s,uint32_t frame,const float muzzle[3],
    const float forward[3],float visible_length,uint32_t active)
{
    uint32_t i,k,texture=UINT32_MAX,bitmap;float direction[3];double norm=0;int status;
    if(!frame)memset(rf_scene_flame_visual,0,sizeof(rf_scene_flame_visual));
    if(!s || !muzzle || !forward || active>1 || !isfinite(visible_length) || visible_length<0 || visible_length>100)return RF_RANGE;
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
