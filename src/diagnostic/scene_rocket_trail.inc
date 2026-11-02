#ifndef RF_SCENE_ROCKET_TRAIL_INC
#define RF_SCENE_ROCKET_TRAIL_INC
/* Projectile trail emitters, original 4c77a0 (weapon object create):
 *   type = weapon_info[+0x98] ($Trail emitter index); if >=0 and the emitter
 *   table entry (7b2770[type]) exists: room = 40a490(object pos, 1);
 *   emitter = 497ca0(object handle, type, room); link it onto the object's
 *   emitter list (+0x268 head, emitter +0x150 next).
 * The emitter follows its parent object through 4972f0 updates and is released
 * with the object (497d80 detaches its live particles, which keep flying).
 * weapons.tbl names the emitter per weapon (Rocket Launcher, Drone Missile,
 * Tankbot Missile, Fighter Rocket: "missile"; APC Rocket: "ballistic";
 * Torpedo: "torpedo"); emitters.tbl supplies each template.
 * Port detail: projectiles are not registry objects here, so each trail gets a
 * stable port handle in the SCENE_TRAIL_HANDLE range, used as the emitter owner
 * and resolved by scene_rocket_trail_lookup. Projectile owners call start at
 * launch, update every tick after moving, and stop when they discard a
 * projectile without stepping it. Include before the particle bitmap resolver. */
#define SCENE_ROCKET_TRAIL_TAG 0x70000000u
#define SCENE_TRAIL_HANDLE 0x7e000000u /* | generation<<6 | entry */
#define SCENE_TRAIL_ENTRIES 64u
#define SCENE_TRAIL_TYPES 6u /* four trail templates plus the two flame-stream emitters */
enum { SCENE_TRAIL_PLAYER_ROCKET=1, SCENE_TRAIL_AI_ROCKET, SCENE_TRAIL_APC, SCENE_TRAIL_FIGHTER,
    SCENE_TRAIL_SUBMARINE, SCENE_TRAIL_VEHICLE_AI, SCENE_TRAIL_VEHICLE_ATTACK, SCENE_TRAIL_FLAME };
typedef struct scene_trail_entry {
    uint32_t key,slot,handle;rf_particle_emitter_parent parent;uint32_t room;
} scene_trail_entry;
static rf_particle_emitter_template scene_trail_templates[SCENE_TRAIL_TYPES];
static rf_particle_animation scene_trail_images[SCENE_TRAIL_TYPES];
static char scene_trail_names[SCENE_TRAIL_TYPES][64];
static uint32_t scene_trail_type_count,scene_trail_weapon[64],scene_trail_generation;
static scene_trail_entry scene_trail_entries[SCENE_TRAIL_ENTRIES];
static uint32_t scene_rocket_trail_ready;
/* Flame-stream templates ("youfired", "youfiredtoo"), see scene_flame_visual.inc. */
static uint32_t scene_flame_stream_type[2]={UINT32_MAX,UINT32_MAX};
/* types loaded, emitters created, create misses (pool/entries full), updates,
 * particles created, releases, last status, texture resident bytes */
uint32_t rf_scene_rocket_trail[8];
static void scene_rocket_trail_forget(void)
{memset(scene_trail_entries,0,sizeof(scene_trail_entries));}
static void scene_rocket_trail_close(void)
{
    uint32_t i;
    for(i=0;i<scene_trail_type_count;i++)rf_particle_animation_close(scene_trail_images+i);
    memset(scene_trail_images,0,sizeof(scene_trail_images));memset(scene_trail_names,0,sizeof(scene_trail_names));
    scene_trail_type_count=0;scene_rocket_trail_ready=0;
    for(i=0;i<64;i++)scene_trail_weapon[i]=UINT32_MAX;
    scene_flame_stream_type[0]=scene_flame_stream_type[1]=UINT32_MAX;
    scene_rocket_trail_forget();
}
/* $Trail emitter of the named weapons.tbl block, if authored. */
static int scene_trail_weapon_emitter(const char *text,uint32_t bytes,const char *weapon,char out[64])
{
    uint32_t at=0;int selected=0;
    out[0]=0;
    while(at<bytes){
        char line[512],*p,*q;uint32_t n=0;
        while(at<bytes && text[at]!='\r' && text[at]!='\n'){if(n+1<sizeof(line))line[n++]=text[at];at++;}
        while(at<bytes && (text[at]=='\r' || text[at]=='\n'))at++;line[n]=0;
        for(p=line;*p==' '||*p=='\t';p++);
        if(!strncmp(p,"$Name:",6)){
            if(selected)return RF_OK;
            p=strchr(p,'"');q=p?strchr(p+1,'"'):NULL;if(!p || !q)continue;*q=0;
            selected=scene_weapon_impact_visual_equal(p+1,weapon);continue;
        }
        if(selected && !strncmp(p,"$Trail emitter:",15)){
            p=strchr(p,'"');q=p?strchr(p+1,'"'):NULL;if(!p || !q || q-p-1>=64)return RF_FORMAT;
            memcpy(out,p+1,(size_t)(q-p-1));out[q-p-1]=0;return RF_OK;
        }
    }
    return RF_OK;
}
static int scene_trail_template(rf_vpp *tables,const char *name,rf_vpp *maps,uint32_t map_count,uint32_t *type)
{
    rf_particle_definition p;rf_particle_emitter_template t={0};uint32_t i,k;int status;
    for(i=0;i<scene_trail_type_count;i++)if(scene_weapon_impact_visual_equal(scene_trail_names[i],name)){*type=i;return RF_OK;}
    if(scene_trail_type_count==SCENE_TRAIL_TYPES)return RF_RANGE;
    status=rf_emitter_definition_load(tables,name,128*1024,&p);if(status)return status;
    i=scene_trail_type_count;
    status=rf_particle_animation_open(scene_trail_images+i,&p,maps,map_count,320*1024);if(status)return status;
    for(k=0;k<3;k++){t.position[k]=p.position[k];t.direction[k]=p.direction[k];}
    t.direction_random=p.direction_random;t.min_velocity=p.min_velocity;t.max_velocity=p.max_velocity;
    t.spawn_radius=p.spawn_radius;t.min_spawn_delay=p.min_spawn_delay;t.max_spawn_delay=p.max_spawn_delay;
    t.flags=p.flags.emitter;t.min_life=p.min_life;t.max_life=p.max_life;t.min_radius=p.min_radius;t.max_radius=p.max_radius;
    t.growth=p.growth;t.acceleration=p.acceleration;t.gravity_scale=p.gravity_scale;t.cycle=p.cycle;
    t.bitmap=SCENE_ROCKET_TRAIL_TAG+i;t.frame_count=scene_trail_images[i].count;
    memcpy(&t.color,p.color,4);memcpy(&t.color_destination,p.color_destination,4);
    t.particle_flags=p.flags.particle;t.secondary=p.flags.secondary;t.age_to_finish_vbm=p.age_to_finish_vbm;
    scene_trail_templates[i]=t;snprintf(scene_trail_names[i],sizeof(scene_trail_names[i]),"%s",name);
    rf_scene_rocket_trail[7]+=scene_trail_images[i].resident_bytes;
    scene_trail_type_count++;*type=i;return RF_OK;
}
/* Authored resources only. A weapon whose emitter or bitmap cannot load gets
 * no trail, as the original does for a missing emitter type. */
static int scene_rocket_trail_open(const char *tables_path,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp tables={0};rf_vpp_entry entry;char *text=NULL,name[64];uint32_t i;int status;
    scene_rocket_trail_close();memset(rf_scene_rocket_trail,0,sizeof(rf_scene_rocket_trail));
    status=rf_vpp_open(&tables,tables_path);
    if(!status)status=rf_vpp_find(&tables,"weapons.tbl",&entry);
    if(!status && !(text=malloc(entry.size)))status=RF_IO;
    if(!status)status=rf_vpp_read(&tables,&entry,0,text,entry.size);
    for(i=0;!status && i<campaign_weapon_supply.names.count && i<64;i++) {
        uint32_t type;int loaded;
        status=scene_trail_weapon_emitter(text,entry.size,campaign_weapon_supply.names.names[i],name);
        if(status || !name[0])continue;
        loaded=scene_trail_template(&tables,name,maps,map_count,&type);
        if(loaded){printf("TRAIL_UNAVAILABLE %s %s %d\n",campaign_weapon_supply.names.names[i],name,loaded);continue;}
        scene_trail_weapon[i]=type;
    }
    /* 48edc0 single-player flame stream: two continuous emitters per shooter. */
    if(!status && (scene_flame_resources || scene_ai_flame_resources || rf_scene_dev_room_enabled)) {
        static const char *const stream[2]={"youfired","youfiredtoo"};
        for(i=0;i<2;i++){uint32_t type;int loaded=scene_trail_template(&tables,stream[i],maps,map_count,&type);
            if(loaded)printf("FLAME_STREAM_UNAVAILABLE %s %d\n",stream[i],loaded);
            else {
                /* Port: the authored "damages, fire_damage" particle damage is
                 * not simulated (the particle step refuses secondary bit 1);
                 * flame damage stays with scene_flame_tick's volume test. */
                scene_trail_templates[type].secondary&=~(1u|8u);scene_flame_stream_type[i]=type;
            }}
    }
    free(text);rf_vpp_close(&tables);rf_scene_rocket_trail[6]=(uint32_t)status;
    if(status){printf("TRAIL_TABLE_UNAVAILABLE %d\n",status);scene_rocket_trail_close();return RF_OK;}
    rf_scene_rocket_trail[0]=scene_trail_type_count;scene_rocket_trail_ready=scene_trail_type_count!=0;return RF_OK;
}
static const rf_particle_animation *scene_rocket_trail_animation(uint32_t index)
{return scene_rocket_trail_ready && index<scene_trail_type_count?scene_trail_images+index:NULL;}
static scene_trail_entry *scene_trail_find(uint32_t key)
{
    uint32_t i;for(i=0;i<SCENE_TRAIL_ENTRIES;i++)if(scene_trail_entries[i].key==key)return scene_trail_entries+i;
    return NULL;
}
/* Resolved parent view (original object fields 3c/48/144/2c/34). Class flags
 * from 4c90f0 bit 0x40 only choose the particle owner, which is already this
 * handle, so zero is behaviour-identical here. */
static int scene_trail_pose(scene_stream *s,scene_trail_entry *e,const rf_weapon_flight *flight)
{
    rf_collision_room_location location;int status;
    memcpy(e->parent.position,flight->position,12);memcpy(e->parent.velocity,flight->velocity,12);
    e->parent.remaining_life=(float)flight->remaining;e->parent.handle=e->handle;
    status=rf_geometry_collision_world_locate(s->collision,flight->position,&location);if(status)return status;
    e->room=location.room==UINT32_MAX?0:location.room+1;return RF_OK;
}
static int32_t scene_trail_now(uint32_t frame){return (int32_t)(((uint64_t)frame*1000/60)%RF_TIMER_PERIOD);}
/* 4c77a0 tail: create the weapon's trail emitter with the projectile as owner.
 * basis: the projectile object's orientation at launch (rows right/up/forward). */
static int scene_projectile_trail_start(scene_stream *s,uint32_t kind,uint32_t index,int32_t weapon,
    const rf_weapon_flight *flight,const float basis[9],uint32_t frame)
{
    scene_trail_entry *e;uint32_t key=kind<<16|index,type,i,slot;int status;
    if(!scene_rocket_trail_ready || !s || !s->particles.state || !s->collision || !flight || !basis)return RF_OK;
    if((e=scene_trail_find(key))!=NULL)memset(e,0,sizeof(*e)); /* A reused slot never inherits a trail. */
    if(weapon<0 || weapon>=64 || (type=scene_trail_weapon[weapon])==UINT32_MAX)return RF_OK;
    for(i=0;i<SCENE_TRAIL_ENTRIES && scene_trail_entries[i].key;i++){}
    if(i==SCENE_TRAIL_ENTRIES){++rf_scene_rocket_trail[2];return RF_OK;}
    e=scene_trail_entries+i;memset(e,0,sizeof(*e));
    scene_trail_generation=(scene_trail_generation+1)&0x7ffffu;
    e->handle=SCENE_TRAIL_HANDLE|(scene_trail_generation<<6)|i;memcpy(e->parent.basis,basis,36);
    status=scene_trail_pose(s,e,flight);if(status){memset(e,0,sizeof(*e));return status;}
    status=rf_emitter_pool_create(&s->particles.state->emitters,scene_trail_templates+type,(int32_t)e->handle,e->room,1,
        scene_trail_now(frame),&e->parent,&s->particles.state->random,&slot);
    rf_scene_rocket_trail[6]=(uint32_t)status;
    if(status==RF_NOT_FOUND){memset(e,0,sizeof(*e));++rf_scene_rocket_trail[2];return RF_OK;} /* Exhausted pool: no trail. */
    if(status){memset(e,0,sizeof(*e));return status;}
    e->key=key;e->slot=slot;++rf_scene_rocket_trail[1];return RF_OK;
}
/* Launch orientation for owners that keep only a direction: forward along the
 * flight, right/up completed against world up (as the AI rocket owner does). */
static void scene_trail_basis(const float direction[3],float basis[9])
{
    float up[3]={0,1,0};double length=0;uint32_t k;
    for(k=0;k<3;k++)length+=(double)direction[k]*direction[k];length=sqrt(length);
    for(k=0;k<3;k++)basis[6+k]=length>0?(float)(direction[k]/length):(k==2);
    if(fabsf(basis[7])>.99f){up[0]=1;up[1]=0;}
    basis[0]=up[1]*basis[8]-up[2]*basis[7];basis[1]=up[2]*basis[6]-up[0]*basis[8];basis[2]=up[0]*basis[7]-up[1]*basis[6];
    length=sqrt((double)basis[0]*basis[0]+(double)basis[1]*basis[1]+(double)basis[2]*basis[2]);
    for(k=0;k<3;k++)basis[k]=(float)(basis[k]/length);
    basis[3]=basis[7]*basis[2]-basis[8]*basis[1];basis[4]=basis[8]*basis[0]-basis[6]*basis[2];basis[5]=basis[6]*basis[1]-basis[7]*basis[0];
}
static int32_t scene_trail_weapon_named(const char *name)
{return rf_weapon_name_find(&campaign_weapon_supply.names,name);}
/* 497d80: release with the projectile; live puffs keep flying. */
static int scene_projectile_trail_stop(scene_stream *s,uint32_t kind,uint32_t index)
{
    scene_trail_entry *e=scene_trail_find(kind<<16|index);int status;
    if(!e)return RF_OK;
    status=s && s->particles.state?rf_emitter_pool_release(&s->particles.state->emitters,e->slot):RF_OK;
    memset(e,0,sizeof(*e));rf_scene_rocket_trail[6]=(uint32_t)status;if(status)return status;
    ++rf_scene_rocket_trail[5];return RF_OK;
}
/* Owners that clear a whole projectile family at once release its trails. */
static int scene_projectile_trail_stop_kind(scene_stream *s,uint32_t kind)
{
    uint32_t i;int status;
    for(i=0;i<SCENE_TRAIL_ENTRIES;i++)if(scene_trail_entries[i].key>>16==kind){
        status=scene_projectile_trail_stop(s,kind,scene_trail_entries[i].key&0xffffu);if(status)return status;}
    return RF_OK;
}
/* 4972f0 while the projectile lives; release once it is gone. */
static int scene_projectile_trail_update(scene_stream *s,uint32_t kind,uint32_t index,const rf_weapon_flight *flight,uint32_t frame)
{
    scene_trail_entry *e;rf_particle_emitter_update_result result;int status;
    if(!scene_rocket_trail_ready || !s || !s->particles.state || !(e=scene_trail_find(kind<<16|index)))return RF_OK;
    if(!flight || !flight->active)return scene_projectile_trail_stop(s,kind,index);
    status=scene_trail_pose(s,e,flight);if(status)return status;
    status=rf_particle_emitter_update(&s->particles.state->particles,&s->particles.state->slots[e->slot].runtime,
        e->slot+1,1,1.f/60,scene_trail_now(frame),&e->parent,e->room,&s->particles.state->random,&result);
    rf_scene_rocket_trail[6]=(uint32_t)status;if(status)return status;
    ++rf_scene_rocket_trail[3];rf_scene_rocket_trail[4]+=result.created;return RF_OK;
}
/* Particle/emitter owner lookup for the scene's particle passes. Handle zero
 * and expired trail handles resolve to no object; other handles stay
 * unresolved (RF_NOT_FOUND), exactly as with no lookup. */
static int scene_rocket_trail_lookup(void *context,uint32_t handle,rf_level_particle_object *object)
{
    const scene_trail_entry *e;(void)context;
    if(!handle)return RF_OK;
    if((handle&0xfe000000u)!=SCENE_TRAIL_HANDLE)return RF_NOT_FOUND;
    e=scene_trail_entries+(handle&63u);
    if(!scene_rocket_trail_ready || !e->key || e->handle!=handle)return RF_OK;
    object->parent=e->parent;object->room=e->room;object->found=1;return RF_OK;
}
#endif
