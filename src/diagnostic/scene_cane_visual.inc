#ifndef RF_SCENE_CANE_VISUAL_INCLUDED
#define RF_SCENE_CANE_VISUAL_INCLUDED
/* Include after scene_ai_cane.inc and its flight implementation. Exact bounded
 * NanoAttackMissile.vfx: two morph billboards, one PART and its radial WARP.
 * RF.exe 53f060/554bf0/555510,5420f0/5414d0,56a9c0/56af20.
 * Optional presentation never rejects/debits a shot or changes gameplay. */
#define SCENE_CANE_PARTICLES 50u
#define SCENE_CANE_DRAW_ITEMS (SCENE_CANE_PARTICLES+2u)
#define SCENE_CANE_DRAW_CALLBACK 0x488b03u
#define SCENE_CANE_VISUAL_BUDGET (256u*1024u)
/* Reserve fixed loader/render stack as well as all owned/static storage.
 * TGA decoding streams through its4096-byte reader; no encoded-file copy. */
#define SCENE_CANE_VISUAL_SCRATCH 8192u
#ifdef RF_IMAGE_XBOX_NATIVE
extern uint32_t rf_xbox_available_memory_pages(void);
#endif
typedef struct scene_cane_particle {
    float age,lifetime,angle,position[3],velocity[3];uint32_t room;
} scene_cane_particle;
typedef struct scene_cane_effect {
    scene_ai_laser_projectile owner;
    scene_cane_particle particles[SCENE_CANE_PARTICLES];
    rf_random_state random;float credit,effect_frame;
    uint64_t ticket,queued_ticket;uint32_t frame,active;
} scene_cane_effect;
static scene_cane_effect scene_cane_effects[SCENE_AI_CANE_CAPACITY];
static uint64_t scene_cane_visual_ticket;
/* accepts,births,age retirements,live particles,quads,vertices,resident bytes,
 * last status,pool drops,terminal/reuse drops,identity/clock drops,queue drops,
 * peak accounted bytes,admission free pages,flare draws,force steps */
uint32_t rf_scene_cane_visual[16];
struct scene_cane_visual {
    rf_vfx_mesh *flares[2];rf_vfx_material_bank *materials;
    rf_vfx_material_textures textures;
    float origin[3],width,size,spread,rate,lifetime,size_out,alpha_out;
    float wind_center[3],wind_strength;
    uint32_t resident_bytes,peak_bytes;
};
static uint32_t scene_cane_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float scene_cane_float(const unsigned char *p)
{uint32_t word=scene_cane_word(p);float value;memcpy(&value,&word,4);return value;}
static int scene_cane_part_read(const unsigned char *p,uint32_t bytes,scene_cane_visual *v)
{
    static const float sample[13]={0,0,0,0,0,0,1,.22832441329956055f,.15675613284111023f,.1f,0,.0508f,.015625f};
    float f[13];uint32_t i;
    if(!p || !v)return RF_RANGE;
    if(bytes!=608 || memcmp(p,"VParticle01\0Scene Root\0",23) || p[23] ||
       scene_cane_word(p+24)!=0x10 || scene_cane_word(p+28)!=1 || memcmp(p+32,"VWind01",8) ||
       scene_cane_word(p+40) || scene_cane_word(p+44)!=10 || scene_cane_word(p+48)!=2 ||
       scene_cane_word(p+52)!=SCENE_CANE_PARTICLES || scene_cane_word(p+56) ||
       scene_cane_word(p+60)!=3200 || scene_cane_float(p+64)!=0 || scene_cane_word(p+68)!=1 ||
       scene_cane_float(p+72)!=0 || scene_cane_float(p+76)!=.7f ||
       scene_cane_float(p+80)!=0 || scene_cane_float(p+84)!=.7f)return RF_FORMAT;
    for(i=1;i<10;i++)if(memcmp(p+88,p+88+i*52,52))return RF_FORMAT;
    for(i=0;i<13;i++){f[i]=scene_cane_float(p+88+i*4);if(!isfinite(f[i]) || f[i]!=sample[i])return RF_FORMAT;}
    memcpy(v->origin,f,12);v->width=f[7];v->size=f[9];v->spread=fabsf(f[11]*12.f);
    v->rate=f[12]*320.f*15.f;v->lifetime=3200.f*.00020833333837799728f;
    v->size_out=scene_cane_float(p+76);v->alpha_out=scene_cane_float(p+84);return RF_OK;
}
static int scene_cane_warp_read(const unsigned char *p,uint32_t bytes,scene_cane_visual *v)
{
    static const float sample[12]={4.3762518942003226e-8f,4.3762518942003226e-8f,-1.0011696815490723f,
        .5f,-.5f,.5f,-.4999999701976776f,-877824.f,0,0,0,1};
    float f[12];uint32_t i;
    if(!p || !v)return RF_RANGE;
    if(bytes!=507 || memcmp(p,"VWind01\0Scene Root\0",19) ||
       scene_cane_word(p+19)!=1 || scene_cane_word(p+23)!=10)return RF_FORMAT;
    for(i=1;i<10;i++)if(memcmp(p+27,p+27+i*48,48))return RF_FORMAT;
    for(i=0;i<12;i++){f[i]=scene_cane_float(p+27+i*4);if(!isfinite(f[i]) || f[i]!=sample[i])return RF_FORMAT;}
    /* Kind1 is radial: position is used, quaternion direction is not. Exact
     * record has no exponential decay or noise, so no force is omitted. */
    memcpy(v->wind_center,f,12);v->wind_strength=(f[7]*.0001f)*.0625f;return RF_OK;
}
static void scene_cane_visual_reset(void)
{memset(scene_cane_effects,0,sizeof(scene_cane_effects));rf_scene_cane_visual[3]=0;}
static void scene_cane_visual_free(scene_cane_visual *v)
{
    if(!v)return;
    rf_vfx_mesh_close(v->flares);rf_vfx_mesh_close(v->flares+1);
    rf_vfx_material_textures_close(&v->textures);rf_vfx_material_bank_close(&v->materials);free(v);
}
static void scene_cane_visual_close(scene_cane_visual **out)
{
    if(!out || !*out)return;scene_cane_visual_reset();scene_cane_visual_free(*out);*out=NULL;
    rf_scene_cane_visual[6]=0;
}
static int scene_cane_visual_open(rf_vpp *effects,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,scene_cane_visual **out)
{
    static const uint32_t types[7]={0x4f584653u,0x50524157u,0x4f584653u,0x54524150u,0x4c54414du,0x4c54414du,0x4c54414du};
    static const uint32_t sizes[7]={987,507,987,608,100,102,68};
    static const char *names[3]={"NanoCaneFlare.tga","NanoMuzzleFlash.tga","Electricity01.tga"};
    static const char *flares[2]={"EnergyFlare02","EnergyFlare01"};
    rf_vfx_directory directory={0};scene_cane_visual *v=NULL;unsigned char *raw=NULL;
    uint32_t i,j,used,temporary,word;int status=RF_RANGE;
    uint64_t fixed=sizeof(*v)+sizeof(scene_cane_effects)+sizeof(scene_cane_visual_ticket)+sizeof(rf_scene_cane_visual);
    if(!effects || (!maps && map_count) || !out || *out || budget>SCENE_CANE_VISUAL_BUDGET ||
       fixed+SCENE_CANE_VISUAL_SCRATCH>budget)goto done;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_cane_visual[13]=rf_xbox_available_memory_pages();
    /* Preserve the existing optional16MiB gameplay/render reserve. Static
     * storage is charged here too, conservatively, despite already resident. */
    if(rf_scene_cane_visual[13]<4096u+(budget+4095u)/4096u)goto done;
#endif
    used=(uint32_t)fixed;
    status=rf_vfx_directory_open(effects,"NanoAttackMissile.vfx",budget-used-SCENE_CANE_VISUAL_SCRATCH,&directory);if(status)goto done;
    if(directory.header.version!=0x40006 || directory.entry.size!=3543 || directory.count!=7 ||
       directory.header.values[1]!=9 || directory.header.values[2]!=2 || directory.header.values[4] ||
       directory.header.values[5]!=1 || directory.header.values[6]!=1 || directory.header.values[9]!=3){status=RF_FORMAT;goto done;}
    for(i=0;i<7;i++)if(directory.chunks[i].type!=types[i] || directory.chunks[i].bytes!=sizes[i]){status=RF_FORMAT;goto done;}
    temporary=directory.allocated_bytes+987u+SCENE_CANE_VISUAL_SCRATCH;
    if((uint64_t)used+temporary>budget){status=RF_RANGE;goto done;}
    v=calloc(1,sizeof(*v));raw=malloc(987);if(!v || !raw){status=RF_IO;goto done;}
    status=rf_vfx_chunk_read(&directory,3,0,raw,608);if(status)goto done;
    status=scene_cane_part_read(raw,608,v);if(status)goto done;
    status=rf_vfx_chunk_read(&directory,1,0,raw,507);if(status)goto done;
    status=scene_cane_warp_read(raw,507,v);if(status)goto done;
    status=rf_vfx_material_bank_open(&directory,budget-used-temporary,&v->materials);if(status)goto done;
    if((uint64_t)used+temporary+v->materials->allocated_bytes>budget){status=RF_RANGE;goto done;}
    used+=v->materials->allocated_bytes;
    if(v->materials->count!=3){status=RF_FORMAT;goto done;}
    for(i=0;i<3;i++){
        const rf_vfx_material_view *m=v->materials->views+i;float speed,value;
        memcpy(&speed,m->words+15,4);
        if(m->words[0] || m->words[1] || m->words[2]!=1 || m->words[30]!=15 || m->bitmap_requests!=1 ||
           strcmp((const char *)(m->words+5),names[i]) || m->words[14] || speed!=1 || m->words[16]!=2 ||
           m->words[33] || m->words[34] || m->words[35] || m->words[46]!=1 || m->words[48]!=(i==2?1u:9u)){status=RF_FORMAT;goto done;}
        status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,1,0,&word);if(status)goto done;
        memcpy(&value,&word,4);if(value!=1){status=RF_FORMAT;goto done;}
        for(j=0;j<m->words[48];j++){
            status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,2,j,&word);if(status)goto done;
            memcpy(&value,&word,4);if(!isfinite(value) || value<0 || value>1 || (i==2 && value!=1)){status=RF_FORMAT;goto done;}
        }
    }
    for(i=0;i<2;i++){
        rf_vfx_mesh *m;
        status=rf_vfx_chunk_read(&directory,i?2:0,0,raw,987);if(status)goto done;
        status=rf_vfx_mesh_open(raw,987,0x40006,3,NULL,budget-used-temporary,v->flares+i);if(status)goto done;
        m=v->flares[i];used+=m->allocated_bytes;
        if((uint64_t)used+temporary>budget){status=RF_RANGE;goto done;}
        if(strcmp(m->prefix.name,flares[i]) || strcmp(m->prefix.parent,"Scene Root") ||
           m->prefix.vertices!=4 || m->prefix.faces!=2 || m->prefix.timing.samples!=10 ||
           m->prefix.timing.flags!=60 || m->prefix.timing.start!=0 || m->prefix.timing.end!=.6333333253860474f ||
           m->materials!=1 || scene_cane_word(m->data+m->material_offset)!=i ||
           m->edges.flags!=0x15 || m->edges.mesh_flags!=60){status=RF_FORMAT;goto done;}
        /* Original554bf0 ignores triangle vertices/UVs for this flag1 path;
         * shared full mesh decoding still validates every original record. */
        for(j=0;j<10;j++)if(!(m->frames[j].present&2u) ||
            m->frames[j].extra[0]<=0 || m->frames[j].extra[1]<=0){status=RF_FORMAT;goto done;}
    }
    status=rf_vfx_material_textures_open(&v->textures,v->materials->views,3,maps,map_count,
        budget-used-temporary+(uint32_t)sizeof(v->textures));if(status)goto done;
    if(v->textures.resident_bytes<sizeof(v->textures) ||
       (uint64_t)used+temporary+v->textures.resident_bytes-sizeof(v->textures)>budget){status=RF_RANGE;goto done;}
    used+=v->textures.resident_bytes-(uint32_t)sizeof(v->textures);
    if(v->textures.texture_count!=3 || !v->textures.bindings){status=RF_FORMAT;goto done;}
    for(i=0;i<3;i++){
        const rf_particle_animation *a=&v->textures.textures[i].animation;uint32_t size=i==2?64:128;
        if(v->textures.bindings[i][0]!=i || a->count!=1 || !a->images || a->images[0].width!=size ||
           a->images[0].height!=size || a->images[0].source_format!=(i==2?7u:6u)){status=RF_FORMAT;goto done;}
    }
    v->resident_bytes=used;v->peak_bytes=used+temporary;
    rf_scene_cane_visual[6]=used;rf_scene_cane_visual[12]=v->peak_bytes;*out=v;v=NULL;status=RF_OK;
done:
    free(raw);rf_vfx_directory_close(&directory);scene_cane_visual_free(v);
    rf_scene_cane_visual[7]=(uint32_t)status;return status;
}
static uint32_t scene_cane_effect_live(const scene_cane_effect *e)
{uint32_t i,n=0;for(i=0;i<SCENE_CANE_PARTICLES;i++)n+=e->particles[i].age>=0;return n;}
static void scene_cane_visual_accept(uint32_t slot,const scene_ai_laser_projectile *shot,uint32_t frame)
{
    scene_cane_effect *e;uint32_t i;
    if(slot>=SCENE_AI_CANE_CAPACITY || !shot || !shot->flight.active){rf_scene_cane_visual[7]=RF_RANGE;return;}
    e=scene_cane_effects+slot;if(e->active)rf_scene_cane_visual[9]+=scene_cane_effect_live(e);
    memset(e,0,sizeof(*e));
    if(scene_cane_visual_ticket==UINT64_MAX){rf_scene_cane_visual[7]=RF_RANGE;return;}
    e->owner=*shot;e->frame=frame;e->active=1;e->ticket=++scene_cane_visual_ticket;e->random.value=1;
    for(i=0;i<SCENE_CANE_PARTICLES;i++)e->particles[i].age=-1;
    ++rf_scene_cane_visual[0];
}
static int scene_cane_owner_matches(const scene_ai_laser_projectile *a,const scene_ai_laser_projectile *b)
{
    /* Guidance changes velocity and basis. Neither NPC liveness nor mutable
     * target/pose is an existing flight's presentation identity. */
    return a->source==b->source && a->weapon==b->weapon && a->damage_kind==b->damage_kind &&
        a->damage==b->damage && a->blast_radius==b->blast_radius && a->blast_kind==b->blast_kind &&
        a->liquid_weapon_flags==b->liquid_weapon_flags && a->flight.radius==b->flight.radius;
}
static int scene_cane_to_world(const float local[3],const float basis[9],float out[3])
{
    uint32_t k;
    for(k=0;k<3;k++){
        out[k]=(float)(((double)local[2]*basis[6+k]+(double)local[1]*basis[3+k])+(double)local[0]*basis[k]);
        if(!isfinite(out[k]))return RF_RANGE;
    }
    return RF_OK;
}
static float scene_cane_random_range(rf_random_state *random,float lo,float hi)
{uint32_t draw;rf_random_next(random,&draw);return (float)(((double)hi-lo)*((double)draw/32768.0)+lo);}
static int scene_cane_birth(scene_cane_effect *e,const scene_ai_laser_projectile *flight,
    const scene_cane_visual *v,float fraction,float dt,scene_cane_particle *out)
{
    scene_cane_particle p={0};float direction[3],local[3],point[3],speed;uint32_t k;int status;
    status=rf_particle_cone_sample(-1,&e->random,direction);if(status)return status;
    speed=scene_cane_random_range(&e->random,-v->spread,v->spread);
    for(k=0;k<3;k++){local[k]=direction[k]*speed;point[k]=direction[k]*v->width+v->origin[k];}
    status=scene_cane_to_world(local,flight->basis,p.velocity);if(status)return status;
    status=scene_cane_to_world(point,flight->basis,local);if(status)return status;
    p.age=scene_cane_random_range(&e->random,0,fraction*dt);
    for(k=0;k<3;k++){
        p.position[k]=local[k]+e->owner.flight.position[k]+(flight->flight.position[k]-e->owner.flight.position[k])*fraction+p.velocity[k]*p.age;
        if(!isfinite(p.position[k]))return RF_RANGE;
    }
    /* Original still draws zero life variation; flag0x10 randomizes angle.
     * Flag8 absent: never add the projectile's speed20 to birth velocity. */
    (void)scene_cane_random_range(&e->random,0,0);p.lifetime=v->lifetime;
    p.angle=scene_cane_random_range(&e->random,0,6.2831853071795864769f);*out=p;return RF_OK;
}
static int scene_cane_effect_step(scene_cane_effect *e,const scene_ai_laser_projectile *flight,
    const scene_cane_visual *v,const rf_geometry_collision_world *world,uint32_t frame)
{
    const float dt=1.f/60;float elapsed,wind[3],credit;uint32_t i,k,requested,left;int status;
    if(frame==e->frame)return RF_OK;
    if(frame-e->frame!=1 || !scene_cane_owner_matches(&e->owner,flight))return RF_RANGE;
    elapsed=(float)(e->owner.flight.remaining-flight->flight.remaining);
    if(!isfinite(elapsed) || fabsf(elapsed-dt)>.00001f)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(flight->flight.position[k]) || !isfinite(e->owner.flight.position[k]))return RF_RANGE;
    status=scene_cane_to_world(v->wind_center,flight->basis,wind);if(status)return status;
    for(k=0;k<3;k++)wind[k]+=flight->flight.position[k];
    for(i=0;i<SCENE_CANE_PARTICLES;i++){
        scene_cane_particle *p=e->particles+i;float delta[3];double length=0;
        if(p->age<0)continue;
        if(!isfinite(p->age) || !isfinite(p->lifetime) || p->lifetime<=0)return RF_RANGE;
        p->age+=dt;if(p->age>=p->lifetime){p->age=-1;++rf_scene_cane_visual[2];continue;}
        for(k=0;k<3;k++){delta[k]=p->position[k]-wind[k];length+=(double)delta[k]*delta[k];}
        if(!isfinite(length))return RF_RANGE;length=sqrt(length);
        /*541579..5417e0: radial normalized position-minus-force-center times
         * authored strength, conversion1e-4 then1/16. No gravity/decay/noise.
         * Exact zero-distance is a safe zero-force port policy. */
        for(k=0;k<3;k++){
            float acceleration=length>0?(float)((double)delta[k]/length)*v->wind_strength:0;
            float next=p->velocity[k]+acceleration*dt;
            p->position[k]+=(p->velocity[k]+next)*.5f*dt;p->velocity[k]=next;
        }
        ++rf_scene_cane_visual[15];
    }
    e->effect_frame+=dt*15.f;if(e->effect_frame>=9)e->effect_frame-=9;
    credit=e->credit+v->rate*dt;
    if(!isfinite(credit) || credit<0 || credit>SCENE_CANE_PARTICLES+1)return RF_RANGE;
    requested=(uint32_t)credit;e->credit=credit-(float)requested;left=requested;
    for(i=0;i<SCENE_CANE_PARTICLES && left;i++){
        scene_cane_particle *p=e->particles+i;if(p->age>=0)continue;
        status=scene_cane_birth(e,flight,v,(float)left/(float)requested,dt,p);if(status)return status;
        --left;++rf_scene_cane_visual[1];
    }
    rf_scene_cane_visual[8]+=left;
    for(i=0;i<SCENE_CANE_PARTICLES;i++){
        scene_cane_particle *p=e->particles+i;rf_collision_room_location location;
        if(p->age<0)continue;
        for(k=0;k<3;k++)if(!isfinite(p->position[k]) || !isfinite(p->velocity[k]))return RF_RANGE;
        status=rf_geometry_collision_world_locate(world,p->position,&location);if(status)return status;
        p->room=location.room==UINT32_MAX?0:location.room+1;
    }
    e->owner=*flight;e->frame=frame;return RF_OK;
}
static void scene_cane_visual_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,live=0;int status;
    if(!stream){rf_scene_cane_visual[7]=RF_RANGE;return;}
    for(i=0;i<SCENE_AI_CANE_CAPACITY;i++){
        scene_cane_effect *e=scene_cane_effects+i;const scene_ai_laser_projectile *flight=scene_ai_canes+i;
        if(!e->active)continue;
        if(!stream->cane_visual || !flight->flight.active){
            rf_scene_cane_visual[9]+=scene_cane_effect_live(e);memset(e,0,sizeof(*e));continue;
        }
        status=scene_cane_effect_step(e,flight,stream->cane_visual,stream->collision,frame);
        if(status){rf_scene_cane_visual[7]=(uint32_t)status;++rf_scene_cane_visual[10];memset(e,0,sizeof(*e));continue;}
        live+=scene_cane_effect_live(e);
    }
    rf_scene_cane_visual[3]=live;
}
static float scene_cane_envelope(float u,float fade)
{return u>fade?(1.f-u)/(1.f-fade):1.f;}
static int scene_cane_particle_style(const scene_cane_visual *v,const scene_cane_particle *p,float *radius,uint32_t *alpha)
{
    float u,a;
    if(!isfinite(p->age) || !isfinite(p->lifetime) || p->lifetime<=0)return RF_RANGE;
    u=p->age/p->lifetime;*radius=2.f*v->size*scene_cane_envelope(u,v->size_out);
    a=scene_cane_envelope(u,v->alpha_out);
    if(!isfinite(*radius) || !isfinite(a) || *radius<0 || a<0 || a>1)return RF_RANGE;
    *alpha=(uint32_t)(a*255.f);return RF_OK;
}
static int scene_cane_flare_sample(const scene_cane_visual *v,const scene_cane_effect *e,uint32_t index,
    float center[3],float size[2],uint32_t *alpha)
{
    rf_vfx_morph_sample sample;float a;uint32_t k;int status;
    if(index>=2)return RF_RANGE;
    status=rf_vfx_mesh_sample(v->flares[index],e->effect_frame,0,&sample);if(status)return status;
    status=scene_cane_to_world(sample.center,e->owner.basis,center);if(status)return status;
    for(k=0;k<3;k++){center[k]+=e->owner.flight.position[k];if(!isfinite(center[k]))return RF_RANGE;}
    memcpy(size,sample.extra,8);
    status=rf_vfx_material_evaluate(v->materials->data,v->materials->bytes,v->materials->views+index,2,e->effect_frame,&a);if(status)return status;
    if(!isfinite(a) || a<0 || a>1 || size[0]<=0 || size[1]<=0)return RF_RANGE;
    /*554de2 alpha uses504e90 (add.5 then truncate), unlike PART truncation. */
    *alpha=(uint32_t)(a*255.f+.5f);return RF_OK;
}
static void scene_cane_visual_queue_room(scene_stream *stream,uint32_t room,
    rf_render_queue_record *records,uint32_t capacity,uint32_t *count)
{
    scene_cane_visual *v;uint32_t slot,i;
    if(!stream || !(v=stream->cane_visual))return;
    if(!count || capacity>2048 || *count>capacity || (capacity && !records)){rf_scene_cane_visual[7]=RF_RANGE;return;}
    for(slot=0;slot<SCENE_AI_CANE_CAPACITY;slot++){
        scene_cane_effect *e=scene_cane_effects+slot;
        if(!e->active || !scene_ai_canes[slot].flight.active || !scene_cane_owner_matches(&e->owner,scene_ai_canes+slot))continue;
        for(i=0;i<SCENE_CANE_DRAW_ITEMS;i++){
            rf_render_queue_record entry={0};uint32_t a,accepted;int status;
            if(i<SCENE_CANE_PARTICLES){
                const scene_cane_particle *p=e->particles+i;
                if(p->age<0 || p->age>=p->lifetime || p->room!=room)continue;
                status=scene_cane_particle_style(v,p,&entry.radius,&a);memcpy(entry.position,p->position,12);
            }else{
                float size[2];rf_collision_room_location location;
                status=scene_cane_flare_sample(v,e,i-SCENE_CANE_PARTICLES,entry.position,size,&a);
                if(!status){entry.radius=.5f*sqrtf(size[0]*size[0]+size[1]*size[1]);
                    status=rf_geometry_collision_world_locate(stream->collision,entry.position,&location);
                    if(!status && (location.room==UINT32_MAX?0:location.room+1)!=room)continue;}
            }
            if(status){rf_scene_cane_visual[7]=(uint32_t)status;continue;}if(!a || entry.radius<=0)continue;
            entry.object=slot*SCENE_CANE_DRAW_ITEMS+i;entry.sorted=1;entry.lighting_flag=1;entry.callback=SCENE_CANE_DRAW_CALLBACK;
            status=rf_render_queue_append(&stream->particle_camera.frustum,entry.position,&entry,records,capacity,count,&accepted);
            if(status){rf_scene_cane_visual[7]=(uint32_t)status;continue;}
            if(accepted)e->queued_ticket=e->ticket;else ++rf_scene_cane_visual[11];
        }
    }
}
/* Exact angle0 rectangle555510 expressed through the shared billboard packet.
 * Width/height are FULL sampled extras; depth bias is half width, as555706.
 * Adjust only corner scale, never the camera transform/projection itself. */
static int scene_cane_flare_project(const rf_visibility_camera *camera,const float position[3],
    const float size[2],rf_particle_screen_polygon *out)
{
    rf_particle_projected_point center={0};rf_particle_billboard_packet packet;
    const rf_visibility_projection *view;float delta[3],scale[3];uint32_t i,j;int status;
    if(!camera || !position || !size || !out || !isfinite(size[0]) || !isfinite(size[1]) || size[0]<=0 || size[1]<=0)return RF_RANGE;
    view=&camera->projection;
    for(i=0;i<3;i++){
        if(!isfinite(position[i]) || !isfinite(view->origin[i]) || !isfinite(camera->view.scale[i]))return RF_RANGE;
        delta[i]=position[i]-view->origin[i];for(j=0;j<3;j++)if(!isfinite(view->matrix[i*3+j]))return RF_RANGE;
    }
    for(i=0;i<3;i++)center.camera[i]=(float)(((double)view->matrix[i*3]*delta[0]+
        (double)view->matrix[i*3+1]*delta[1])+(double)view->matrix[i*3+2]*delta[2]);
    if(!(view->perspective&255u))center.camera[2]=view->flat_depth;
    status=rf_particle_project(&view->projection,&center);if(status)return status;
    if(!(center.flags&1u)){*out=(rf_particle_screen_polygon){0};return RF_OK;}
    memcpy(scale,camera->view.scale,12);scale[1]*=size[1]/size[0];
    status=rf_particle_billboard_prepare(center.camera,0,size[0]*.5f,1,1,scale,&view->clip,&packet);if(status)return status;
    return rf_particle_billboard_project(&view->projection,&view->clip,&packet,out);
}
static int scene_cane_visual_draw_particle(scene_stream *stream,uint32_t id,rf_scene_particle_sink sink,void *context)
{
    scene_cane_visual *v;scene_cane_effect *e;const rf_image *image;
    rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    rf_particle_screen_polygon polygon={0};rf_particle_draw_vertex vertices[12];rf_particle_vertex_environment env={0};
    uint32_t slot,item,i,a,material;float radius,time;int status;
    if(!stream || !(v=stream->cane_visual))return RF_OK;
    if(id>=SCENE_AI_CANE_CAPACITY*SCENE_CANE_DRAW_ITEMS)return RF_RANGE;
    slot=id/SCENE_CANE_DRAW_ITEMS;item=id%SCENE_CANE_DRAW_ITEMS;e=scene_cane_effects+slot;
    if(!e->active || !e->queued_ticket || e->queued_ticket!=e->ticket || !scene_ai_canes[slot].flight.active ||
       !scene_cane_owner_matches(&e->owner,scene_ai_canes+slot))return RF_OK;
    if(item<SCENE_CANE_PARTICLES){
        const scene_cane_particle *p=e->particles+item;
        if(p->age<0 || p->age>=p->lifetime)return RF_OK;
        status=scene_cane_particle_style(v,p,&radius,&a);if(status)return status;
        if(!a || radius<=0)return RF_OK;material=2;time=p->age*15.f;
        status=rf_vfx_material_texture_sample(&v->textures,v->materials->views+material,material,0,time,0,&image);if(status)return status;
        status=rf_particle_world_billboard(&stream->particle_camera,p->position,p->angle,radius,image->width,image->height,&polygon);if(status)return status;
    }else{
        float center[3],size[2];material=item-SCENE_CANE_PARTICLES;time=e->effect_frame;
        status=scene_cane_flare_sample(v,e,material,center,size,&a);if(status)return status;if(!a)return RF_OK;
        status=rf_vfx_material_texture_sample(&v->textures,v->materials->views+material,material,0,time,0,&image);if(status)return status;
        status=scene_cane_flare_project(&stream->particle_camera,center,size,&polygon);if(status)return status;
    }
    if(!polygon.count)return RF_OK;
    /* All three original MATLs have glow1 and brightness1: additive white.
     * No normal-alpha substitution, missing-texture fallback or invented tint. */
    status=rf_particle_render_decode(RF_PARTICLE_GLOW_MODE,&re,&states);if(status)return status;
    env.rgba=0x00ffffffu|(a<<24);env.vertex_color=states.vertex_color;env.vertex_alpha=states.vertex_alpha;
    env.depth_scale=env.reciprocal_scale=stream->particle_camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
    for(i=0;i<polygon.count;i++){status=rf_particle_vertex_encode(&env,polygon.vertices+i,vertices+i);if(status)return status;}
    if(sink){status=sink(context,vertices,polygon.count,image,RF_PARTICLE_GLOW_MODE);if(status)return status;}
    ++rf_scene_cane_visual[4];rf_scene_cane_visual[5]+=polygon.count;
    if(item>=SCENE_CANE_PARTICLES)++rf_scene_cane_visual[14];return RF_OK;
}
static void scene_cane_visual_draw_one(scene_stream *stream,uint32_t id,rf_scene_particle_sink sink,void *context)
{int status=scene_cane_visual_draw_particle(stream,id,sink,context);if(status)rf_scene_cane_visual[7]=(uint32_t)status;}
#endif
