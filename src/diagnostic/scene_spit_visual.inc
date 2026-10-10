#ifndef RF_SCENE_SPIT_VISUAL_INCLUDED
#define RF_SCENE_SPIT_VISUAL_INCLUDED
/* Include after scene_ai_spit.inc. Exact original SpitAttack.vfx PARTs;
 * shared directory/material/texture and particle queue/billboard ownership.
 * RF.exe5420f0,540e80,5414d0,557460. See SMALL-ROCK-SNAKE-SPIT-PART.md.
 * This optional presentation cannot reject/debit a shot or deal damage. */
#define SCENE_SPIT_PARTS 2u
#define SCENE_SPIT_PARTICLES 27u
#define SCENE_SPIT_PART_BYTES 1068u
#define SCENE_SPIT_DRAW_CALLBACK 0x488b02u
#define SCENE_SPIT_PARTICLE_IDS (SCENE_AI_SPIT_CAPACITY*SCENE_SPIT_PARTS*SCENE_SPIT_PARTICLES)
typedef struct scene_spit_particle {
    float age,lifetime,angle,position[3],velocity[3];uint32_t room;
} scene_spit_particle;
typedef struct scene_spit_part {
    float lifetime_ticks,lifetime_variation,size,rate,size_in,size_out,alpha_in,alpha_out;
    float origin[3],rotation[10],width,depth,speed,spread,opacity;
    uint32_t flags,kind,material;
} scene_spit_part;
typedef struct scene_spit_effect {
    scene_ai_laser_projectile owner; /* Copied flight, never a mutable NPC pose. */
    scene_spit_particle particles[SCENE_SPIT_PARTS][SCENE_SPIT_PARTICLES];
    rf_random_state random;float emission_credit[SCENE_SPIT_PARTS];
    uint64_t ticket,queued_ticket;uint32_t frame,active;
} scene_spit_effect;
static scene_spit_effect scene_spit_effects[SCENE_AI_SPIT_CAPACITY];
static uint64_t scene_spit_visual_ticket;
/* accepts,births,age retirements,live particles,quads,vertices,resident bytes,
 * last status,pool drops,terminal/reuse drops,identity/clock drops,queue drops */
uint32_t rf_scene_spit_visual[12];
struct scene_spit_visual {
    scene_spit_part parts[SCENE_SPIT_PARTS];rf_vfx_material_bank *materials;
    rf_vfx_material_textures textures;uint32_t resident_bytes,peak_bytes;
};
static uint32_t scene_spit_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float scene_spit_float(const unsigned char *p)
{uint32_t word=scene_spit_word(p);float value;memcpy(&value,&word,4);return value;}
static int scene_spit_part_read(const unsigned char *p,uint32_t bytes,uint32_t index,scene_spit_part *out)
{
    static const char *names[2]={"VParticle01\0Scene Root\0","VParticle04\0Scene Root\0"};
    static const float samples[2][13]={
        {-.0041970545426011086f,-.011440942995250225f,.26415345072746277f,
         -.7071067690849304f,0,0,.7071068286895752f,.29474806785583496f,.32816800475120544f,
         .22608299553394318f,.1544319987297058f,.03175000101327896f,.011250000447034836f},
        {-.0041970545426011086f,.31130728125572205f,.26415345072746277f,
         -.7071067690849304f,0,0,.7071068286895752f,.42072340846061707f,.32816800475120544f,
         .20000000298023224f,.20319999754428864f,.03175000101327896f,.004218750167638063f}};
    scene_spit_part next={0};float field[13];uint32_t i;
    if(!p || !out || index>=SCENE_SPIT_PARTS)return RF_RANGE;
    if(bytes!=SCENE_SPIT_PART_BYTES || memcmp(p,names[index],23) || p[23] ||
       scene_spit_word(p+24)!=(index?0x12u:0x10u) || scene_spit_word(p+28) || scene_spit_word(p+32) ||
       scene_spit_word(p+36)!=19 || scene_spit_word(p+40)!=index || scene_spit_word(p+44)!=SCENE_SPIT_PARTICLES ||
       scene_spit_word(p+48) || scene_spit_word(p+52)!=(index?6400u:2400u) ||
       scene_spit_float(p+56)!=.34f || scene_spit_word(p+60)!=index)return RF_FORMAT;
    /* Byte23 is opaque, not an enable gate. Kind1 uses a sphere surface;
     * its stored quaternion/depth are validated but unused by540ef1..541005. */
    for(i=1;i<19;i++)if(memcmp(p+80,p+80+i*52,52))return RF_FORMAT;
    for(i=0;i<13;i++){
        field[i]=scene_spit_float(p+80+i*4);
        if(!isfinite(field[i]) || field[i]!=samples[index][i])return RF_FORMAT;
    }
    next.size_in=scene_spit_float(p+64);next.size_out=scene_spit_float(p+68);
    next.alpha_in=scene_spit_float(p+72);next.alpha_out=scene_spit_float(p+76);
    if(next.size_in!=0 || next.size_out!=(index?.7f:.56f) || next.alpha_in!=.24f ||
       next.alpha_out!=(index?.8f:.56f))return RF_FORMAT;
    next.flags=scene_spit_word(p+24);next.kind=scene_spit_word(p+60);next.material=scene_spit_word(p+40);
    next.lifetime_ticks=(float)scene_spit_word(p+52);next.lifetime_variation=scene_spit_float(p+56);
    memcpy(next.origin,field,12);memcpy(next.rotation+3,field+3,16);
    next.rotation[7]=next.rotation[8]=next.rotation[9]=1;
    next.width=field[7];next.depth=field[8];next.size=field[9];
    next.speed=field[10]*12.f;next.spread=fabsf(field[11]*12.f);next.rate=field[12]*320.f*15.f;
    *out=next;return RF_OK;
}
static void scene_spit_visual_reset(void)
{
    memset(scene_spit_effects,0,sizeof(scene_spit_effects));rf_scene_spit_visual[3]=0;
    /* Ticket sequence survives timeline resets and never wraps into reuse. */
}
static void scene_spit_visual_close(scene_spit_visual **out)
{
    scene_spit_visual *v;if(!out || !(v=*out))return;
    scene_spit_visual_reset();rf_vfx_material_textures_close(&v->textures);
    rf_vfx_material_bank_close(&v->materials);free(v);*out=NULL;rf_scene_spit_visual[6]=0;
}
static int scene_spit_visual_open(rf_vpp *effects,rf_vpp *maps,uint32_t map_count,
    uint32_t budget,scene_spit_visual **out)
{
    static const char *textures[2]={"AcidBlob.tga","AcidBlob02.tga"};
    rf_vfx_directory directory={0};scene_spit_visual *v=NULL;unsigned char *raw=NULL;
    uint32_t i,parts[2],n=0,m=0,used,temporary,word;float brightness;int status;
    uint64_t fixed=sizeof(*v)+sizeof(scene_spit_effects)+sizeof(rf_scene_spit_visual)+sizeof(scene_spit_visual_ticket);
    if(!effects || (!maps && map_count) || !out || *out || fixed>budget)return RF_RANGE;
    used=(uint32_t)fixed;
    status=rf_vfx_directory_open(effects,"SpitAttack.vfx",budget-used,&directory);if(status)goto done;
    if(directory.header.version!=0x40006u || directory.entry.size!=2424 || directory.count!=4 ||
       directory.header.values[1]!=18 || directory.header.values[2] || directory.header.values[4] ||
       directory.header.values[5]!=2 || directory.header.values[9]!=2){status=RF_FORMAT;goto done;}
    for(i=0;i<directory.count;i++){
        const rf_vfx_chunk *chunk=directory.chunks+i;
        if(chunk->type==0x54524150u && n<2 && chunk->bytes==SCENE_SPIT_PART_BYTES)parts[n++]=i;
        else if(chunk->type==0x4c54414du && m<2 && chunk->bytes==(m?65u:63u))++m;
        else {status=RF_FORMAT;goto done;}
    }
    if(n!=2 || m!=2){status=RF_FORMAT;goto done;}
    temporary=directory.allocated_bytes+SCENE_SPIT_PART_BYTES;
    if((uint64_t)used+temporary>budget){status=RF_RANGE;goto done;}
    v=calloc(1,sizeof(*v));raw=malloc(SCENE_SPIT_PART_BYTES);
    if(!v || !raw){status=RF_IO;goto done;}
    for(i=0;i<2;i++){
        status=rf_vfx_chunk_read(&directory,parts[i],0,raw,SCENE_SPIT_PART_BYTES);if(status)goto done;
        status=scene_spit_part_read(raw,SCENE_SPIT_PART_BYTES,i,v->parts+i);if(status)goto done;
    }
    status=rf_vfx_material_bank_open(&directory,budget-used-temporary,&v->materials);if(status)goto done;
    if((uint64_t)used+temporary+v->materials->allocated_bytes>budget){status=RF_RANGE;goto done;}
    used+=v->materials->allocated_bytes;
    if(v->materials->count!=2){status=RF_FORMAT;goto done;}
    for(i=0;i<2;i++){
        const rf_vfx_material_view *material=v->materials->views+i;float speed;
        memcpy(&speed,material->words+15,4);
        if(material->words[0] || material->words[1] || material->words[2] || material->words[30]!=15 ||
           material->bitmap_requests!=1 || strcmp((const char *)(material->words+5),textures[i]) ||
           material->words[14] || speed!=1 || material->words[16]!=2 || material->words[33] ||
           material->words[34] || material->words[35] || material->words[46]!=1 || material->words[48]!=1){status=RF_FORMAT;goto done;}
        status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,material,1,0,&word);if(status)goto done;
        memcpy(&brightness,&word,4);
        status=rf_vfx_material_sample(v->materials->data,v->materials->bytes,material,2,0,&word);if(status)goto done;
        memcpy(&v->parts[i].opacity,&word,4);
        if(brightness!=1 || v->parts[i].opacity!=.75f){status=RF_FORMAT;goto done;}
    }
    status=rf_vfx_material_textures_open(&v->textures,v->materials->views,2,maps,map_count,
        budget-used-temporary+(uint32_t)sizeof(v->textures));if(status)goto done;
    if(v->textures.resident_bytes<sizeof(v->textures) ||
       (uint64_t)used+temporary+v->textures.resident_bytes-sizeof(v->textures)>budget){status=RF_RANGE;goto done;}
    used+=v->textures.resident_bytes-(uint32_t)sizeof(v->textures);
    if(v->textures.texture_count!=2 || !v->textures.bindings){status=RF_FORMAT;goto done;}
    for(i=0;i<2;i++)if(v->textures.bindings[i][0]!=i || v->textures.textures[i].animation.count!=1 ||
       !v->textures.textures[i].animation.images || v->textures.textures[i].animation.images[0].width!=64 ||
       v->textures.textures[i].animation.images[0].height!=64){status=RF_FORMAT;goto done;}
    v->resident_bytes=used;v->peak_bytes=used+temporary;
    rf_scene_spit_visual[6]=used;*out=v;v=NULL;status=RF_OK;
done:
    free(raw);rf_vfx_directory_close(&directory);
    /* Failed optional admission never resets another live visual owner. */
    if(v){rf_vfx_material_textures_close(&v->textures);rf_vfx_material_bank_close(&v->materials);free(v);}
    rf_scene_spit_visual[7]=(uint32_t)status;return status;
}
static uint32_t scene_spit_effect_live(const scene_spit_effect *e)
{
    uint32_t part,i,count=0;
    for(part=0;part<SCENE_SPIT_PARTS;part++)for(i=0;i<SCENE_SPIT_PARTICLES;i++)count+=e->particles[part][i].age>=0;
    return count;
}
static void scene_spit_visual_accept(uint32_t slot,const scene_ai_laser_projectile *accepted,uint32_t frame)
{
    scene_spit_effect *e;uint32_t part,i;
    if(slot>=SCENE_AI_SPIT_CAPACITY || !accepted || !accepted->flight.active){rf_scene_spit_visual[7]=RF_RANGE;return;}
    e=scene_spit_effects+slot;
    if(e->active)rf_scene_spit_visual[9]+=scene_spit_effect_live(e);
    memset(e,0,sizeof(*e));
    if(scene_spit_visual_ticket==UINT64_MAX){rf_scene_spit_visual[7]=RF_RANGE;return;}
    e->owner=*accepted;e->frame=frame;e->active=1;e->ticket=++scene_spit_visual_ticket;
    e->random.value=1; /* Isolated deterministic presentation policy; no gameplay draws. */
    for(part=0;part<SCENE_SPIT_PARTS;part++)for(i=0;i<SCENE_SPIT_PARTICLES;i++)e->particles[part][i].age=-1;
    ++rf_scene_spit_visual[0];
}
static int scene_spit_owner_matches(const scene_ai_laser_projectile *a,const scene_ai_laser_projectile *b)
{
    return a->source==b->source && a->weapon==b->weapon && a->damage_kind==b->damage_kind && a->damage==b->damage &&
        a->blast_radius==b->blast_radius && a->blast_kind==b->blast_kind &&
        a->liquid_weapon_flags==b->liquid_weapon_flags && a->flight.radius==b->flight.radius &&
        !memcmp(a->flight.velocity,b->flight.velocity,12) && !memcmp(a->basis,b->basis,36);
}
static float scene_spit_random_range(rf_random_state *random,float lo,float hi)
{uint32_t draw;rf_random_next(random,&draw);return (float)(((double)hi-lo)*((double)draw/32768.0)+lo);}
static int scene_spit_to_world(const float local[3],const float basis[9],float out[3])
{
    uint32_t k;
    for(k=0;k<3;k++){
        out[k]=(float)(((double)local[2]*basis[6+k]+(double)local[1]*basis[3+k])+(double)local[0]*basis[k]);
        if(!isfinite(out[k]))return RF_RANGE;
    }
    return RF_OK;
}
static int scene_spit_birth(scene_spit_effect *e,const scene_spit_part *part,
    const scene_ai_laser_projectile *flight,float fraction,float dt,scene_spit_particle *out)
{
    scene_spit_particle p={0};float local[3],point[3],rotated[3],speed,age,variation;uint32_t k;int status;
    if(!part->kind){
        /*541058: three independent spread draws in X,Z,Y order. */
        local[0]=scene_spit_random_range(&e->random,-part->spread,part->spread);
        local[2]=scene_spit_random_range(&e->random,-part->spread,part->spread);
        local[1]=scene_spit_random_range(&e->random,-part->spread,part->spread)-part->speed;
        status=rf_vfx_transform_point(part->rotation,local,rotated);if(status)return status;
        status=scene_spit_to_world(rotated,flight->basis,p.velocity);if(status)return status;
        point[0]=scene_spit_random_range(&e->random,-part->width*.5f,part->width*.5f);point[1]=0;
        point[2]=scene_spit_random_range(&e->random,-part->depth*.5f,part->depth*.5f);
        status=rf_vfx_transform_point(part->rotation,point,local);if(status)return status;
    } else {
        /*540ef1->4fad10: two-draw uniform sphere, width is radius. The
         * sphere path has no quaternion rotation and does not use depth. */
        status=rf_particle_cone_sample(-1,&e->random,local);if(status)return status;
        speed=scene_spit_random_range(&e->random,-part->spread,part->spread)+part->speed;
        for(k=0;k<3;k++){point[k]=local[k]*speed;local[k]*=part->width;}
        status=scene_spit_to_world(point,flight->basis,p.velocity);if(status)return status;
    }
    for(k=0;k<3;k++)local[k]+=part->origin[k];
    status=scene_spit_to_world(local,flight->basis,point);if(status)return status;
    /* Flags8 absent: retain visual-local velocity, never add projectile30. */
    age=scene_spit_random_range(&e->random,0,fraction*dt);p.age=age;
    for(k=0;k<3;k++){
        p.position[k]=point[k]+e->owner.flight.position[k]+(flight->flight.position[k]-e->owner.flight.position[k])*fraction+p.velocity[k]*age;
        if(!isfinite(p.position[k]))return RF_RANGE;
    }
    variation=part->lifetime_ticks*part->lifetime_variation;
    p.lifetime=(scene_spit_random_range(&e->random,-variation,variation)+part->lifetime_ticks)*.00020833333837799728f;
    p.angle=scene_spit_random_range(&e->random,0,6.2831853071795864769f);
    if(!isfinite(p.lifetime) || p.lifetime<=0)return RF_RANGE;
    *out=p;return RF_OK;
}
static int scene_spit_effect_step(scene_spit_effect *e,const scene_ai_laser_projectile *flight,
    const scene_spit_visual *v,const rf_geometry_collision_world *world,uint32_t frame)
{
    float dt=1.f/60,elapsed;uint32_t part,i,k;int status;
    if(frame==e->frame)return RF_OK;
    if(frame-e->frame!=1 || !scene_spit_owner_matches(&e->owner,flight))return RF_RANGE;
    elapsed=(float)(e->owner.flight.remaining-flight->flight.remaining);
    if(!isfinite(elapsed) || fabsf(elapsed-dt)>.00001f)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(flight->flight.position[k]) || !isfinite(e->owner.flight.position[k]))return RF_RANGE;
    for(part=0;part<SCENE_SPIT_PARTS;part++){
        const scene_spit_part *definition=v->parts+part;float credit;uint32_t requested,left;
        for(i=0;i<SCENE_SPIT_PARTICLES;i++){
            scene_spit_particle *p=e->particles[part]+i;float next_y;
            if(p->age<0)continue;
            if(!isfinite(p->age) || !isfinite(p->lifetime) || p->lifetime<=0)return RF_RANGE;
            p->age+=dt;if(p->age>=p->lifetime){p->age=-1;++rf_scene_spit_visual[2];continue;}
            /*541525/589858: world-Y -9.8; original trapezoidal movement.
             * No external force refs, collision response or gameplay acid. */
            next_y=p->velocity[1]-((definition->flags&2u)?9.8f*dt:0);
            p->position[0]+=p->velocity[0]*dt;p->position[2]+=p->velocity[2]*dt;
            p->position[1]+=(p->velocity[1]+next_y)*.5f*dt;p->velocity[1]=next_y;
        }
        /* Constant tracks integrate continuously at60Hz. Original outer18/
         * inner19-frame wrap scheduler remains an explicit first-pass limit. */
        credit=e->emission_credit[part]+definition->rate*dt;
        if(!isfinite(credit) || credit<0 || credit>SCENE_SPIT_PARTICLES+1)return RF_RANGE;
        requested=(uint32_t)credit;e->emission_credit[part]=credit-(float)requested;left=requested;
        for(i=0;i<SCENE_SPIT_PARTICLES && left;i++){
            scene_spit_particle *p=e->particles[part]+i;if(p->age>=0)continue;
            status=scene_spit_birth(e,definition,flight,(float)left/(float)requested,dt,p);if(status)return status;
            --left;++rf_scene_spit_visual[1];
        }
        rf_scene_spit_visual[8]+=left; /* Original drops excess birth credit. */
        for(i=0;i<SCENE_SPIT_PARTICLES;i++){
            scene_spit_particle *p=e->particles[part]+i;rf_collision_room_location location;
            if(p->age<0)continue;
            for(k=0;k<3;k++)if(!isfinite(p->position[k]) || !isfinite(p->velocity[k]))return RF_RANGE;
            status=rf_geometry_collision_world_locate(world,p->position,&location);if(status)return status;
            p->room=location.room==UINT32_MAX?0:location.room+1;
        }
    }
    e->owner=*flight;e->frame=frame;return RF_OK;
}
static void scene_spit_visual_tick(scene_stream *stream,uint32_t frame)
{
    uint32_t i,live=0;int status;
    if(!stream){rf_scene_spit_visual[7]=RF_RANGE;return;}
    for(i=0;i<SCENE_AI_SPIT_CAPACITY;i++){
        scene_spit_effect *e=scene_spit_effects+i;const scene_ai_laser_projectile *flight=scene_ai_spits+i;
        if(!e->active)continue;
        if(!stream->spit_visual || !flight->flight.active){
            rf_scene_spit_visual[9]+=scene_spit_effect_live(e);memset(e,0,sizeof(*e));continue;
        }
        status=scene_spit_effect_step(e,flight,stream->spit_visual,stream->collision,frame);
        if(status){rf_scene_spit_visual[7]=(uint32_t)status;++rf_scene_spit_visual[10];memset(e,0,sizeof(*e));continue;}
        live+=scene_spit_effect_live(e);
    }
    rf_scene_spit_visual[3]=live;
}
static float scene_spit_envelope(float age,float begin,float end)
{
    if(age<begin)return begin>0?age/begin:1;
    if(age>end)return (1.f-age)/(1.f-end);
    return 1;
}
static int scene_spit_particle_style(const scene_spit_particle *p,const scene_spit_part *part,float *radius,uint32_t *alpha)
{
    float u,a;
    if(!isfinite(p->age) || !isfinite(p->lifetime) || p->lifetime<=0)return RF_RANGE;
    u=p->age/p->lifetime;*radius=2.f*part->size*scene_spit_envelope(u,part->size_in,part->size_out);
    a=part->opacity*scene_spit_envelope(u,part->alpha_in,part->alpha_out);
    if(!isfinite(*radius) || !isfinite(a) || *radius<0 || a<0 || a>1)return RF_RANGE;
    *alpha=(uint32_t)(a*255.f);return RF_OK;
}
static void scene_spit_visual_queue_room(scene_stream *stream,uint32_t room,
    rf_render_queue_record *records,uint32_t capacity,uint32_t *count)
{
    scene_spit_visual *v;uint32_t slot,part,i;
    if(!stream || !(v=stream->spit_visual))return;
    if(!count || capacity>2048 || *count>capacity || (capacity && !records)){rf_scene_spit_visual[7]=RF_RANGE;return;}
    for(slot=0;slot<SCENE_AI_SPIT_CAPACITY;slot++){
        scene_spit_effect *e=scene_spit_effects+slot;
        if(!e->active || !scene_ai_spits[slot].flight.active || !scene_spit_owner_matches(&e->owner,scene_ai_spits+slot))continue;
        for(part=0;part<SCENE_SPIT_PARTS;part++)for(i=0;i<SCENE_SPIT_PARTICLES;i++){
            const scene_spit_particle *p=e->particles[part]+i;rf_render_queue_record entry={0};uint32_t a,accepted;int status;
            if(p->age<0 || p->age>=p->lifetime || p->room!=room)continue;
            status=scene_spit_particle_style(p,v->parts+part,&entry.radius,&a);
            if(status){rf_scene_spit_visual[7]=(uint32_t)status;continue;}
            if(!a || entry.radius<=0)continue;
            entry.object=(slot*SCENE_SPIT_PARTS+part)*SCENE_SPIT_PARTICLES+i;
            memcpy(entry.position,p->position,12);entry.sorted=1;entry.lighting_flag=1;entry.callback=SCENE_SPIT_DRAW_CALLBACK;
            status=rf_render_queue_append(&stream->particle_camera.frustum,entry.position,&entry,records,capacity,count,&accepted);
            if(status){rf_scene_spit_visual[7]=(uint32_t)status;continue;}
            if(accepted)e->queued_ticket=e->ticket;else ++rf_scene_spit_visual[11];
        }
    }
}
static int scene_spit_visual_draw_particle(scene_stream *stream,uint32_t id,rf_scene_particle_sink sink,void *context)
{
    scene_spit_visual *v;scene_spit_effect *e;const scene_spit_particle *p;const scene_spit_part *part;const rf_image *image;
    rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    rf_particle_screen_polygon polygon={0};rf_particle_draw_vertex vertices[12];rf_particle_vertex_environment env={0};
    uint32_t slot,part_index,i,a;float radius;int status;
    if(!stream || !(v=stream->spit_visual))return RF_OK;
    if(id>=SCENE_SPIT_PARTICLE_IDS)return RF_RANGE;
    i=id%SCENE_SPIT_PARTICLES;id/=SCENE_SPIT_PARTICLES;part_index=id%SCENE_SPIT_PARTS;slot=id/SCENE_SPIT_PARTS;
    e=scene_spit_effects+slot;p=e->particles[part_index]+i;part=v->parts+part_index;
    if(!e->active || !e->queued_ticket || e->queued_ticket!=e->ticket || !scene_ai_spits[slot].flight.active ||
       !scene_spit_owner_matches(&e->owner,scene_ai_spits+slot) || p->age<0 || p->age>=p->lifetime)return RF_OK;
    status=scene_spit_particle_style(p,part,&radius,&a);if(status)return status;if(!a || radius<=0)return RF_OK;
    /* Flags4 absent: texture time is particle age*15, not normalized age.
     * Both original32-bit TGAs are static. Brightness1 floors RGB to white;
     * MATL glow0/PART unlit0 selects the normal alpha blend, not Sonar glow. */
    status=rf_vfx_material_texture_sample(&v->textures,v->materials->views+part->material,part->material,0,p->age*15.f,0,&image);if(status)return status;
    status=rf_particle_render_decode(RF_PARTICLE_NORMAL_MODE,&re,&states);if(status)return status;
    status=rf_particle_world_billboard(&stream->particle_camera,p->position,p->angle,radius,image->width,image->height,&polygon);if(status)return status;
    if(!polygon.count)return RF_OK;
    env.rgba=0x00ffffffu|(a<<24);env.vertex_color=states.vertex_color;env.vertex_alpha=states.vertex_alpha;
    env.depth_scale=env.reciprocal_scale=stream->particle_camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
    for(i=0;i<polygon.count;i++){status=rf_particle_vertex_encode(&env,polygon.vertices+i,vertices+i);if(status)return status;}
    if(sink){status=sink(context,vertices,polygon.count,image,RF_PARTICLE_NORMAL_MODE);if(status)return status;}
    ++rf_scene_spit_visual[4];rf_scene_spit_visual[5]+=polygon.count;return RF_OK;
}
static void scene_spit_visual_draw_one(scene_stream *stream,uint32_t id,rf_scene_particle_sink sink,void *context)
{
    int status=scene_spit_visual_draw_particle(stream,id,sink,context);
    if(status)rf_scene_spit_visual[7]=(uint32_t)status;
}
#endif
