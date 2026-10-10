#ifndef RF_SCENE_SONAR_VISUAL_INCLUDED
#define RF_SCENE_SONAR_VISUAL_INCLUDED
/* Include after scene_ai_sonar.inc. Exact SonarAttack.vfx PART subset, not a
 * mesh-loader extension or a generic pool emitter. RF.exe 5420f0/542e90,
 * 540bf0/540e80 and 557460. Source evidence and first-pass limits are in
 * docs/SEA-CREATURE-SONAR-PART.md. No original data or gameplay RNG changes. */
#define SCENE_SONAR_PARTICLES 10u
#define SCENE_SONAR_PART_BYTES 1432u
typedef struct scene_sonar_particle {
    float age,lifetime,angle,position[3],velocity[3];
} scene_sonar_particle;
typedef struct scene_sonar_effect {
    /* Full source handle and complete copied flight payload, never an actor
     * pointer. Explicit accept/reset owns reuse, not a sampled active edge. */
    scene_ai_laser_projectile owner;
    scene_sonar_particle particles[SCENE_SONAR_PARTICLES];
    rf_random_state random;
    uint64_t ticket;uint32_t frame,active;
    float emission_credit,effect_frame;
} scene_sonar_effect;
static scene_sonar_effect scene_sonar_effects[SCENE_AI_SONAR_CAPACITY];
static uint64_t scene_sonar_ticket,scene_sonar_visual_epoch;
/* accepts,births,retirements,live particles,quads,vertices,bytes,last status,
 * dropped requested births,terminal/reuse drops,identity/clock drops,draws */
uint32_t rf_scene_sonar_visual[12];
struct scene_sonar_visual {
    rf_vfx_material_bank *materials;rf_vfx_material_textures textures;
    float lifetime,size,rate,size_in,size_out,alpha_in,alpha_out,opacity;
    float local_origin[3],local_velocity[3],duration;
    uint32_t resident_bytes,peak_bytes;
};
static uint32_t scene_sonar_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float scene_sonar_float(const unsigned char *p)
{uint32_t word=scene_sonar_word(p);float value;memcpy(&value,&word,4);return value;}
static int scene_sonar_part_read(const unsigned char *p,uint32_t bytes,scene_sonar_visual *out)
{
    const unsigned char *sample;float transform[10]={0,0,0,0,0,0,1,1,1,1};
    float local[3]={0,0,0},field[13],speed;uint32_t i;int status;
    if(!p || !out)return RF_RANGE;
    if(bytes!=SCENE_SONAR_PART_BYTES || memcmp(p,"VParticle02\0Scene Root\0",23) || p[23] ||
       scene_sonar_word(p+24)!=4 || scene_sonar_word(p+28) || scene_sonar_word(p+32) ||
       scene_sonar_word(p+36)!=26 || scene_sonar_word(p+40) || scene_sonar_word(p+44)!=SCENE_SONAR_PARTICLES ||
       scene_sonar_word(p+48) || scene_sonar_word(p+52)!=4480 || scene_sonar_float(p+56)!=0 ||
       scene_sonar_word(p+60))return RF_FORMAT;
    /* Byte23 is the original opaque high-bit flag, NOT an enable byte.
     * PART flag4 selects normalized bitmap time, NOT pool particle flags. */
    out->size_in=scene_sonar_float(p+64);out->size_out=scene_sonar_float(p+68);
    out->alpha_in=scene_sonar_float(p+72);out->alpha_out=scene_sonar_float(p+76);
    if(out->size_in!=0 || out->size_out!=.445f || out->alpha_in!=.355f || out->alpha_out!=.635f)return RF_FORMAT;
    sample=p+80;
    for(i=1;i<26;i++) {
        if(memcmp(sample,sample+i*52,52))return RF_FORMAT;
    }
    for(i=0;i<13;i++) {
        field[i]=scene_sonar_float(sample+i*4);
        if(!isfinite(field[i]))return RF_FORMAT;
    }
    /* Reject spatial/speed variation and other authored subsets rather than
     * silently ignoring them. The constant quaternion uses the existing
     * unnormalized original 5194c0/4fe900 transform path. */
    if(field[0]!=0 || field[1]!=0 || field[2]!=0 || field[7]!=0 || field[8]!=0 || field[11]!=0 ||
       field[9]!=.7953f || field[10]!=.254f || field[12]!=.0022321429569274187f)return RF_FORMAT;
    if(field[3]!=-.7071067094802856f || field[4]!=0 || field[5]!=0 || field[6]!=.7071068286895752f)return RF_FORMAT;
    memcpy(transform+3,field+3,16);speed=field[10]*12.f;local[1]=-speed;
    status=rf_vfx_transform_point(transform,local,out->local_velocity);
    if(status)return status;
    memcpy(out->local_origin,field,12);
    out->lifetime=(float)scene_sonar_word(p+52)*.00020833333837799728f;
    out->size=field[9];out->rate=field[12]*320.f*15.f;
    return RF_OK;
}
static void scene_sonar_visual_reset(void)
{
    memset(scene_sonar_effects,0,sizeof(scene_sonar_effects));++scene_sonar_visual_epoch;
    rf_scene_sonar_visual[3]=0;
    /* Ticket sequence deliberately survives timeline resets. */
}
static void scene_sonar_visual_close(scene_sonar_visual **out)
{
    scene_sonar_visual *v;
    if(!out || !(v=*out))return;
    scene_sonar_visual_reset();rf_vfx_material_textures_close(&v->textures);
    rf_vfx_material_bank_close(&v->materials);free(v);*out=NULL;rf_scene_sonar_visual[6]=0;
}
static int scene_sonar_visual_open(rf_vpp *effects,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,scene_sonar_visual **out)
{
    rf_vfx_directory directory={0};scene_sonar_visual *v=NULL;unsigned char *raw=NULL;
    uint32_t i,part=UINT32_MAX,material=UINT32_MAX,used,temporary,word;float brightness;int status;
    uint64_t fixed=sizeof(*v)+sizeof(scene_sonar_effects)+sizeof(rf_scene_sonar_visual)+
        sizeof(scene_sonar_ticket)+sizeof(scene_sonar_visual_epoch);
    if(!effects || (!maps && map_count) || !out || *out || fixed>budget)return RF_RANGE;
    used=(uint32_t)fixed;
    status=rf_vfx_directory_open(effects,"SonarAttack.vfx",budget-used,&directory);
    if(status)return status;
    if(directory.header.version!=0x40006u || directory.entry.size!=1642 || directory.count!=2 ||
       directory.header.values[1]!=25 || directory.header.values[2] || directory.header.values[4] ||
       directory.header.values[5]!=1 || directory.header.values[9]!=1){status=RF_FORMAT;goto done;}
    for(i=0;i<directory.count;i++){
        const rf_vfx_chunk *chunk=directory.chunks+i;
        if(chunk->type==0x54524150u && part==UINT32_MAX && chunk->bytes==SCENE_SONAR_PART_BYTES)part=i;
        else if(chunk->type==0x4c54414du && material==UINT32_MAX && chunk->bytes==66)material=i;
        else {status=RF_FORMAT;goto done;}
    }
    if(part==UINT32_MAX || material==UINT32_MAX){status=RF_FORMAT;goto done;}
    temporary=directory.allocated_bytes+SCENE_SONAR_PART_BYTES;
    if((uint64_t)used+temporary>budget){status=RF_RANGE;goto done;}
    v=calloc(1,sizeof(*v));raw=malloc(SCENE_SONAR_PART_BYTES);
    if(!v || !raw){status=RF_IO;goto done;}
    status=rf_vfx_chunk_read(&directory,part,0,raw,SCENE_SONAR_PART_BYTES);
    if(status)goto done;
    status=scene_sonar_part_read(raw,SCENE_SONAR_PART_BYTES,v);
    if(status)goto done;
    v->duration=(float)directory.header.values[1];
    status=rf_vfx_material_bank_open(&directory,budget-used-temporary,&v->materials);
    if(status)goto done;
    if((uint64_t)used+temporary+v->materials->allocated_bytes>budget){status=RF_RANGE;goto done;}
    used+=v->materials->allocated_bytes;
    if(v->materials->count!=1){status=RF_FORMAT;goto done;}
    {const rf_vfx_material_view *m=v->materials->views;float speed;
     memcpy(&speed,m->words+15,4);
     if(m->words[0] || m->words[1] || m->words[2]!=1 || m->words[30]!=15 || m->bitmap_requests!=1 ||
        strcmp((const char *)(m->words+5),"SonarAttack.tga") || m->words[14] || speed!=1 || m->words[16]!=2 ||
        m->words[33] || m->words[34] || m->words[35] || m->words[46]!=1 || m->words[48]!=1){status=RF_FORMAT;goto done;}
     status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,1,0,&word);
     if(status)goto done;
     memcpy(&brightness,&word,4);
     status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,m,2,0,&word);
     if(status)goto done;
     memcpy(&v->opacity,&word,4);
     /* Brightness1 is white even though PART unlit flag1 is absent. The
      * exact material's byte8=1 selects the existing additive glow mode. */
     if(brightness!=1 || v->opacity!=.35f){status=RF_FORMAT;goto done;}}
    status=rf_vfx_material_textures_open(&v->textures,v->materials->views,1,maps,map_count,
        budget-used-temporary+(uint32_t)sizeof(v->textures));
    if(status)goto done;
    if(v->textures.resident_bytes<sizeof(v->textures) ||
       (uint64_t)used+temporary+v->textures.resident_bytes-sizeof(v->textures)>budget){status=RF_RANGE;goto done;}
    used+=v->textures.resident_bytes-(uint32_t)sizeof(v->textures);
    if(v->textures.texture_count!=1 || !v->textures.bindings || v->textures.bindings[0][0]!=0 ||
       v->textures.textures[0].animation.count!=1 || !v->textures.textures[0].animation.images ||
       v->textures.textures[0].animation.images[0].width!=64 ||
       v->textures.textures[0].animation.images[0].height!=64){status=RF_FORMAT;goto done;}
    v->resident_bytes=used;v->peak_bytes=used+temporary;
    rf_scene_sonar_visual[6]=used;*out=v;v=NULL;status=RF_OK;
done:
    free(raw);rf_vfx_directory_close(&directory);
    /* Failure must not reset an unrelated live runtime owner. */
    if(v){rf_vfx_material_textures_close(&v->textures);rf_vfx_material_bank_close(&v->materials);free(v);}
    rf_scene_sonar_visual[7]=(uint32_t)status;
    return status;
}
static uint32_t scene_sonar_effect_live(const scene_sonar_effect *e)
{
    uint32_t i,n=0;
    for(i=0;i<SCENE_SONAR_PARTICLES;i++)n+=e->particles[i].age>=0;
    return n;
}
static void scene_sonar_visual_accept(uint32_t slot,const scene_ai_laser_projectile *accepted,uint32_t frame)
{
    scene_sonar_effect *e;uint32_t i;
    if(slot>=SCENE_AI_SONAR_CAPACITY || !accepted || !accepted->flight.active){rf_scene_sonar_visual[7]=RF_RANGE;return;}
    e=scene_sonar_effects+slot;
    if(e->active)rf_scene_sonar_visual[9]+=scene_sonar_effect_live(e);
    memset(e,0,sizeof(*e));e->owner=*accepted;e->frame=frame;e->active=1;
    if(!++scene_sonar_ticket)++scene_sonar_ticket;
    e->ticket=scene_sonar_ticket;
    e->random.value=1; /* Private presentation seed: original global seed parity deferred. */
    for(i=0;i<SCENE_SONAR_PARTICLES;i++)e->particles[i].age=-1;
    ++rf_scene_sonar_visual[0];
}
static int scene_sonar_owner_matches(const scene_ai_laser_projectile *a,const scene_ai_laser_projectile *b)
{
    return a->source==b->source && a->weapon==b->weapon && a->damage_kind==b->damage_kind &&
        a->damage==b->damage && a->liquid_weapon_flags==b->liquid_weapon_flags &&
        a->flight.radius==b->flight.radius && !memcmp(a->flight.velocity,b->flight.velocity,12) &&
        !memcmp(a->basis,b->basis,36);
}
static float scene_sonar_random_unit(rf_random_state *random)
{uint32_t draw;rf_random_next(random,&draw);return (float)draw*(1.f/32768.f);}
static int scene_sonar_effect_step(scene_sonar_effect *e,const scene_ai_laser_projectile *flight,
    const scene_sonar_visual *v,uint32_t frame)
{
    float velocity[3],origin[3],dt=1.f/60,credit,elapsed;uint32_t i,k,requested,left;
    if(frame==e->frame)return RF_OK;
    /* A visual missing a simulation step is retired, never caught up against
     * a later/root-reused trajectory. Normal pause repeats do not advance. */
    if(frame-e->frame!=1 || !scene_sonar_owner_matches(&e->owner,flight))return RF_RANGE;
    elapsed=(float)(e->owner.flight.remaining-flight->flight.remaining);
    if(!isfinite(elapsed) || fabsf(elapsed-dt)>.00001f)return RF_RANGE;
    for(k=0;k<3;k++){
        if(!isfinite(flight->flight.position[k]) || !isfinite(e->owner.flight.position[k]))return RF_RANGE;
        velocity[k]=(float)(((double)v->local_velocity[2]*flight->basis[6+k]+
            (double)v->local_velocity[1]*flight->basis[3+k])+(double)v->local_velocity[0]*flight->basis[k]);
        origin[k]=(float)(((double)v->local_origin[2]*flight->basis[6+k]+
            (double)v->local_origin[1]*flight->basis[3+k])+(double)v->local_origin[0]*flight->basis[k]);
        if(!isfinite(velocity[k]) || !isfinite(origin[k]))return RF_RANGE;
    }
    for(i=0;i<SCENE_SONAR_PARTICLES;i++){
        scene_sonar_particle *p=e->particles+i;
        if(p->age<0)continue;
        if(!isfinite(p->age) || !isfinite(p->lifetime) || p->lifetime<=0)return RF_RANGE;
        p->age+=dt;
        if(p->age>=p->lifetime){p->age=-1;++rf_scene_sonar_visual[2];continue;}
        for(k=0;k<3;k++) {
            p->position[k]+=p->velocity[k]*dt;
            if(!isfinite(p->position[k]))return RF_RANGE;
        }
    }
    /* Constant-track wall-time integration is intentional. Original outer
     * loop wraps25 effect frames, while PART aging wraps26; this adapter does
     * not invent a 1/15-second age jump at each outer loop. See parity limits. */
    e->effect_frame+=dt*15.f;
    if(e->effect_frame>=v->duration)e->effect_frame-=v->duration;
    credit=e->emission_credit+v->rate*dt;
    if(!isfinite(credit) || credit<0 || credit>SCENE_SONAR_PARTICLES+1)return RF_RANGE;
    requested=(uint32_t)credit;e->emission_credit=credit-(float)requested;left=requested;
    for(i=0;i<SCENE_SONAR_PARTICLES && left;i++){
        scene_sonar_particle *p=e->particles+i;float fraction,age;
        if(p->age>=0)continue;
        fraction=(float)left/(float)requested;
        /* Original draws zero-width X, zero-depth Z, birth age, and zero life
         * variance. Preserve that four-draw sequence in an isolated RNG. */
        (void)scene_sonar_random_unit(&e->random);(void)scene_sonar_random_unit(&e->random);
        age=scene_sonar_random_unit(&e->random)*fraction*dt;
        (void)scene_sonar_random_unit(&e->random);
        p->age=age;p->lifetime=v->lifetime;p->angle=0;
        for(k=0;k<3;k++){
            p->velocity[k]=velocity[k];
            p->position[k]=origin[k]+e->owner.flight.position[k]+
                (flight->flight.position[k]-e->owner.flight.position[k])*fraction+velocity[k]*age;
            if(!isfinite(p->position[k]))return RF_RANGE;
        }
        --left;++rf_scene_sonar_visual[1];
    }
    /* Credit is consumed before pool search, so exhaustion drops births. */
    rf_scene_sonar_visual[8]+=left;e->owner=*flight;e->frame=frame;
    return RF_OK;
}
static int scene_sonar_visual_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,live=0;int status;
    if(!stream)return RF_RANGE;
    for(i=0;i<SCENE_AI_SONAR_CAPACITY;i++){
        scene_sonar_effect *e=scene_sonar_effects+i;const scene_ai_laser_projectile *flight=scene_ai_sonars+i;
        if(!e->active)continue;
        if(!stream->sonar_visual || !flight->flight.active){
            /* Ordinary owned projectile teardown486670->489fc0->502b10
             * ->504620->54b570->542b60 destroys its PART slots. No tail is
             * detached here; shared/external model ownership is unsupported. */
            rf_scene_sonar_visual[9]+=scene_sonar_effect_live(e);memset(e,0,sizeof(*e));continue;
        }
        status=scene_sonar_effect_step(e,flight,stream->sonar_visual,frame);
        if(status){rf_scene_sonar_visual[7]=(uint32_t)status;++rf_scene_sonar_visual[10];memset(e,0,sizeof(*e));continue;}
        live+=scene_sonar_effect_live(e);
    }
    rf_scene_sonar_visual[3]=live;
    return RF_OK; /* Presentation failure cannot undo/reject a shot. */
}
static float scene_sonar_envelope(float age,float begin,float end)
{
    if(age<begin)return begin>0?age/begin:1;
    if(age>end)return (1.f-age)/(1.f-end);
    return 1;
}
static int scene_sonar_visual_draw(scene_stream *stream,rf_scene_particle_sink sink,void *context)
{
    scene_sonar_visual *v;rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    uint32_t i,j,k;uint64_t epoch=scene_sonar_visual_epoch;int status;
    if(!stream || !(v=stream->sonar_visual))return RF_OK;
    status=rf_particle_render_decode(RF_PARTICLE_GLOW_MODE,&re,&states);
    if(status)return status;
    ++rf_scene_sonar_visual[11];
    for(i=0;i<SCENE_AI_SONAR_CAPACITY;i++){
        const scene_sonar_effect *e=scene_sonar_effects+i;uint64_t ticket=e->ticket;
        if(!e->active || !scene_ai_sonars[i].flight.active || !scene_sonar_owner_matches(&e->owner,scene_ai_sonars+i))continue;
        for(j=0;j<SCENE_SONAR_PARTICLES;j++){
            const scene_sonar_particle *p=e->particles+j;const rf_image *image;
            rf_particle_screen_polygon polygon={0};rf_particle_draw_vertex vertices[12];rf_particle_vertex_environment env={0};
            float u,radius,alpha;uint32_t a;
            if(p->age<0 || p->age>=p->lifetime)continue;
            if(!isfinite(p->age) || !isfinite(p->lifetime) || p->lifetime<=0)return RF_RANGE;
            u=p->age/p->lifetime;
            radius=2.f*v->size*scene_sonar_envelope(u,v->size_in,v->size_out);
            alpha=v->opacity*scene_sonar_envelope(u,v->alpha_in,v->alpha_out);
            if(!isfinite(radius) || !isfinite(alpha) || radius<0 || alpha<0 || alpha>1)return RF_RANGE;
            a=(uint32_t)(alpha*255.f);
            if(!a || radius<=0)continue;
            status=rf_vfx_material_texture_sample(&v->textures,v->materials->views,0,0,u,1,&image);
            if(status)return status;
            status=rf_particle_world_billboard(&stream->particle_camera,p->position,0,radius,image->width,image->height,&polygon);
            if(status)return status;
            if(!polygon.count)continue;
            env.rgba=0x00ffffffu|(a<<24);env.vertex_color=states.vertex_color;env.vertex_alpha=states.vertex_alpha;
            env.depth_scale=env.reciprocal_scale=stream->particle_camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
            for(k=0;k<polygon.count;k++) {
                status=rf_particle_vertex_encode(&env,polygon.vertices+k,vertices+k);
                if(status)return status;
            }
            if(sink) {
                status=sink(context,vertices,polygon.count,image,RF_PARTICLE_GLOW_MODE);
                if(status)return status;
            }
            ++rf_scene_sonar_visual[4];rf_scene_sonar_visual[5]+=polygon.count;
            if(epoch!=scene_sonar_visual_epoch || ticket!=scene_sonar_effects[i].ticket)return RF_OK;
        }
    }
    return RF_OK;
}
#endif
