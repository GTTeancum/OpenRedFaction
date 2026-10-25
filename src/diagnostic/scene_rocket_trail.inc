#ifndef RF_SCENE_ROCKET_TRAIL_INC
#define RF_SCENE_ROCKET_TRAIL_INC
/* Projectile trail emitter, original 4c77a0 (weapon object create):
 *   type = weapon_info[+0x98] trail emitter index; if >=0 and the emitter
 *   table entry (7b2770[type]) exists: room = 40a490(object pos, 1);
 *   emitter = 497ca0(object handle, type, room); link it onto the object's
 *   emitter list (+0x268 head, emitter +0x150 next).
 * The emitter then follows its parent object through 4972f0 updates, and is
 * released with the object (497d80 detaches its live particles, which keep
 * flying). weapons.tbl Rocket Launcher names "missile"; emitters.tbl supplies
 * the template. Player rockets only for now: AI, APC and Fighter projectiles
 * do not create their trail emitters yet.
 * Port detail: player rockets are not registry objects, so each launch gets a
 * stable port handle in SCENE_ROCKET_TRAIL_HANDLE range for the emitter owner
 * and the particle owner lookup. Include before the particle bitmap resolver. */
#define SCENE_ROCKET_TRAIL_TAG 0x70000000u
#define SCENE_ROCKET_TRAIL_HANDLE 0x7e000000u /* | generation<<6 | slot (SCENE_ROCKETS<=64) */
static rf_particle_emitter_template scene_rocket_trail_template;
static rf_particle_animation scene_rocket_trail_image;
static uint32_t scene_rocket_trail_ready,scene_rocket_trail_generation;
static uint32_t scene_rocket_trail_slot[SCENE_ROCKETS],scene_rocket_trail_handle[SCENE_ROCKETS];
/* ready, emitters created, create misses (pool full), updates, particles
 * created, releases, last status, texture resident bytes */
uint32_t rf_scene_rocket_trail[8];
static void scene_rocket_trail_forget(void)
{
    uint32_t i;for(i=0;i<SCENE_ROCKETS;i++){scene_rocket_trail_slot[i]=UINT32_MAX;scene_rocket_trail_handle[i]=0;}
}
static void scene_rocket_trail_close(void)
{
    if(scene_rocket_trail_ready)rf_particle_animation_close(&scene_rocket_trail_image);
    memset(&scene_rocket_trail_image,0,sizeof(scene_rocket_trail_image));scene_rocket_trail_ready=0;
    scene_rocket_trail_forget();
}
/* Authored resources only. A missing table entry or bitmap leaves rockets
 * without a trail, as the original does for a missing emitter type. */
static int scene_rocket_trail_open(const char *tables_path,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp tables={0};rf_particle_definition p;rf_particle_emitter_template t={0};uint32_t i;int status;
    scene_rocket_trail_close();memset(rf_scene_rocket_trail,0,sizeof(rf_scene_rocket_trail));
    status=rf_vpp_open(&tables,tables_path);
    if(!status)status=rf_emitter_definition_load(&tables,"missile",128*1024,&p);
    rf_vpp_close(&tables);
    if(!status)status=rf_particle_animation_open(&scene_rocket_trail_image,&p,maps,map_count,128*1024);
    rf_scene_rocket_trail[6]=(uint32_t)status;
    if(status){printf("ROCKET_TRAIL_UNAVAILABLE %d\n",status);memset(&scene_rocket_trail_image,0,sizeof(scene_rocket_trail_image));return RF_OK;}
    for(i=0;i<3;i++){t.position[i]=p.position[i];t.direction[i]=p.direction[i];}
    t.direction_random=p.direction_random;t.min_velocity=p.min_velocity;t.max_velocity=p.max_velocity;
    t.spawn_radius=p.spawn_radius;t.min_spawn_delay=p.min_spawn_delay;t.max_spawn_delay=p.max_spawn_delay;
    t.flags=p.flags.emitter;t.min_life=p.min_life;t.max_life=p.max_life;t.min_radius=p.min_radius;t.max_radius=p.max_radius;
    t.growth=p.growth;t.acceleration=p.acceleration;t.gravity_scale=p.gravity_scale;t.cycle=p.cycle;
    t.bitmap=SCENE_ROCKET_TRAIL_TAG;t.frame_count=scene_rocket_trail_image.count;
    memcpy(&t.color,p.color,4);memcpy(&t.color_destination,p.color_destination,4);
    t.particle_flags=p.flags.particle;t.secondary=p.flags.secondary;t.age_to_finish_vbm=p.age_to_finish_vbm;
    scene_rocket_trail_template=t;scene_rocket_trail_ready=1;scene_rocket_trail_forget();
    rf_scene_rocket_trail[0]=1;rf_scene_rocket_trail[7]=scene_rocket_trail_image.resident_bytes;return RF_OK;
}
static const rf_particle_animation *scene_rocket_trail_animation(uint32_t index)
{return scene_rocket_trail_ready && !index?&scene_rocket_trail_image:NULL;}
/* Resolved parent view (original object fields 3c/48/144/2c/34). Class flags
 * from 4c90f0 bit 0x40 only choose the particle owner, which is already this
 * handle, so zero is behaviour-identical here. */
static int scene_rocket_trail_parent(scene_stream *s,uint32_t i,rf_particle_emitter_parent *parent,uint32_t *room)
{
    rf_collision_room_location location;int status;
    memset(parent,0,sizeof(*parent));
    memcpy(parent->position,s->rockets[i].position,12);memcpy(parent->basis,s->rocket_basis[i],36);
    memcpy(parent->velocity,s->rockets[i].velocity,12);parent->handle=scene_rocket_trail_handle[i];
    parent->remaining_life=(float)s->rockets[i].remaining;
    status=rf_geometry_collision_world_locate(s->collision,s->rockets[i].position,&location);if(status)return status;
    *room=location.room==UINT32_MAX?0:location.room+1;return RF_OK;
}
/* 4c77a0 tail: create the emitter with the projectile as owner/parent. */
static int scene_rocket_trail_start(scene_stream *s,uint32_t i,int32_t now)
{
    rf_particle_emitter_parent parent;uint32_t room,slot;int status;
    if(i>=SCENE_ROCKETS)return RF_RANGE;
    scene_rocket_trail_slot[i]=UINT32_MAX;scene_rocket_trail_handle[i]=0;
    if(!scene_rocket_trail_ready || !s->particles.state || !s->collision)return RF_OK;
    scene_rocket_trail_generation=(scene_rocket_trail_generation+1)&0x7ffffu;
    scene_rocket_trail_handle[i]=SCENE_ROCKET_TRAIL_HANDLE|(scene_rocket_trail_generation<<6)|i;
    status=scene_rocket_trail_parent(s,i,&parent,&room);if(status)return status;
    status=rf_emitter_pool_create(&s->particles.state->emitters,&scene_rocket_trail_template,
        (int32_t)scene_rocket_trail_handle[i],room,1,now,&parent,&s->particles.state->random,&slot);
    rf_scene_rocket_trail[6]=(uint32_t)status;
    if(status==RF_NOT_FOUND){++rf_scene_rocket_trail[2];return RF_OK;} /* Exhausted pool: no trail. */
    if(status)return status;
    scene_rocket_trail_slot[i]=slot;++rf_scene_rocket_trail[1];return RF_OK;
}
/* 4972f0 while the projectile lives; 497d80 once it is gone. */
static int scene_rocket_trail_tick(scene_stream *s,uint32_t i,int32_t now)
{
    rf_particle_emitter_parent parent;rf_particle_emitter_update_result result;uint32_t room,slot;int status;
    if(!scene_rocket_trail_ready || i>=SCENE_ROCKETS || (slot=scene_rocket_trail_slot[i])==UINT32_MAX || !s->particles.state)return RF_OK;
    if(!s->rockets[i].active) {
        status=rf_emitter_pool_release(&s->particles.state->emitters,slot);rf_scene_rocket_trail[6]=(uint32_t)status;
        scene_rocket_trail_slot[i]=UINT32_MAX;scene_rocket_trail_handle[i]=0;
        if(status)return status;++rf_scene_rocket_trail[5];return RF_OK;
    }
    status=scene_rocket_trail_parent(s,i,&parent,&room);if(status)return status;
    status=rf_particle_emitter_update(&s->particles.state->particles,&s->particles.state->slots[slot].runtime,
        slot+1,1,1.f/60,now,&parent,room,&s->particles.state->random,&result);
    rf_scene_rocket_trail[6]=(uint32_t)status;if(status)return status;
    ++rf_scene_rocket_trail[3];rf_scene_rocket_trail[4]+=result.created;return RF_OK;
}
/* Particle/emitter owner lookup for the scene's particle passes. Handle zero
 * and expired trail handles resolve to no object; other handles stay
 * unresolved (RF_NOT_FOUND), exactly as with no lookup. */
static int scene_rocket_trail_lookup(void *context,uint32_t handle,rf_level_particle_object *object)
{
    scene_stream *s=context;uint32_t i;int status;
    if(!handle)return RF_OK;
    if((handle&0xfe000000u)!=SCENE_ROCKET_TRAIL_HANDLE)return RF_NOT_FOUND;
    i=handle&63u;if(!s || !scene_rocket_trail_ready || i>=SCENE_ROCKETS || scene_rocket_trail_handle[i]!=handle || !s->rockets[i].active)return RF_OK;
    status=scene_rocket_trail_parent(s,i,&object->parent,&object->room);if(status)return status;
    object->found=1;return RF_OK;
}
#endif
