#ifndef RF_SCENE_NANO_HIT_VISUAL_INCLUDED
#define RF_SCENE_NANO_HIT_VISUAL_INCLUDED
/* Original4c16e0/4c1d50 world-owned cues, not actor attachments. */
#define SCENE_NANO_HIT_VISUAL_CAP (640u*1024u)
#define SCENE_NANO_HIT_TOTAL_CAP (768u*1024u)
#define SCENE_NANO_HIT_ALLOWANCE (64u*1024u) /*16KiB stack;48KiB heap/page slack*/
#define SCENE_NANO_HIT_CALLBACK 0x488b07u
#define SCENE_NANO_HIT_PARTICLE_CALLBACK 0x488b08u
typedef struct scene_nano_hit_particle {float age,life,angle,position[3],velocity[3];} scene_nano_hit_particle;
typedef struct scene_nano_hit_slot {
    rf_vfx_instance *instances[7];float position[3],basis[9],seconds,credit,opacity,cull_radius;
    uint32_t clock_padding; /* Explicit32-bit ABI padding, charged in sizeof. */
    uint64_t ticket,queued_ticket;uint32_t active,kind,room,live,frame,fresh;
} scene_nano_hit_slot;
typedef struct scene_nano_hit_owner {
    scene_stream *stream;rf_vfx_mesh *meshes[11];rf_vfx_material_bank *materials[3];
    rf_vfx_material_view texture_views[8];rf_vfx_material_textures textures;uint32_t slot_padding;
    scene_nano_hit_slot slots[10];scene_nano_hit_particle particles[2][100];
    float tracks[30][13],beam_direction[3],radius[3];uint64_t ticket;
    uint32_t resident_bytes,peak_bytes;
} scene_nano_hit_owner;
static scene_nano_hit_owner *scene_nano_hit;
static struct {scene_stream *stream;rf_vpp *meshes,*maps;uint32_t map_count,attempted;} scene_nano_hit_pending;
static uint64_t scene_nano_hit_epoch;
/* status,resident,peak,demand,requests,starts,overflow,bad geometry,retired,
 * live,queue drops,polygons,births,particle overflow,free pages,simulation errors */
uint32_t rf_scene_nano_hit[16];
static uint32_t scene_nano_hit_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float scene_nano_hit_float(const unsigned char *p)
{uint32_t w=scene_nano_hit_word(p);float f;memcpy(&f,&w,4);return f;}
static void scene_nano_hit_reset(void)
{
    uint32_t i,j;++scene_nano_hit_epoch;
    if(scene_nano_hit) {
        for(i=0;i<10;i++){scene_nano_hit->slots[i].active=0;scene_nano_hit->slots[i].queued_ticket=0;}
        for(i=0;i<2;i++)for(j=0;j<100;j++)scene_nano_hit->particles[i][j].age=-1;
    }
    rf_scene_nano_hit[9]=0;
}
static void scene_nano_hit_free(scene_nano_hit_owner *v)
{
    uint32_t i,j;if(!v)return;
    for(i=0;i<10;i++)for(j=0;j<7;j++)rf_vfx_instance_close(v->slots[i].instances+j);
    for(i=0;i<11;i++)rf_vfx_mesh_close(v->meshes+i);
    rf_vfx_material_textures_close(&v->textures);
    for(i=0;i<3;i++)rf_vfx_material_bank_close(v->materials+i);free(v);
}
static void scene_nano_hit_close(void)
{
    scene_nano_hit_reset();scene_nano_hit_free(scene_nano_hit);scene_nano_hit=NULL;
    memset(&scene_nano_hit_pending,0,sizeof(scene_nano_hit_pending));rf_scene_nano_hit[1]=0;
}
static int scene_nano_hit_part_read(const unsigned char *p,uint32_t bytes,scene_nano_hit_owner *v)
{
    static const float sample[12]={0,0,0,-.7071067094802856f,0,0,.7071068286895752f,
        .4854429364204407f,.9600732922554016f,.20000000298023224f,0,.10159999877214432f};
    uint32_t i,j;
    if(bytes!=1640 || memcmp(p,"VParticle02\0Scene Root\0",23) || p[23] ||
       scene_nano_hit_word(p+24)!=0x10 || scene_nano_hit_word(p+28) || scene_nano_hit_word(p+32) ||
       scene_nano_hit_word(p+36)!=30 || scene_nano_hit_word(p+40)!=3 || scene_nano_hit_word(p+44)!=100 ||
       scene_nano_hit_word(p+48) || scene_nano_hit_word(p+52)!=2400 || scene_nano_hit_float(p+56)!=0 ||
       scene_nano_hit_word(p+60)!=1 || scene_nano_hit_float(p+64)!=0 || scene_nano_hit_float(p+68)!=.5f ||
       scene_nano_hit_float(p+72)!=0 || scene_nano_hit_float(p+76)!=.5f)return RF_FORMAT;
    for(i=0;i<30;i++)for(j=0;j<13;j++) {
        float f=scene_nano_hit_float(p+80+i*52+j*4);
        if(!isfinite(f) || f!=(j<12?sample[j]:(i==3 || i==4)?.041874997317790985f:0.f))return RF_FORMAT;
        v->tracks[i][j]=j==10?f*12.f:j==11?fabsf(f*12.f):f;
    }
    return RF_OK;
}
static int scene_nano_hit_image_admit(rf_vpp *maps,uint32_t count,const char *name,uint32_t which)
{
    static const uint32_t bytes[6]={39566,65580,45645,49196,53205,9116};
    static const uint32_t type[6]={10,2,10,2,10,10},bits[6]={24,32,24,24,32,32};
    uint32_t i,size=which==5?64u:128u;unsigned char h[18];rf_vpp_entry entry;int status;
    for(i=0;i<count;i++) {
        status=rf_vpp_find(maps+i,name,&entry);if(status==RF_NOT_FOUND)continue;if(status)return status;
        if(entry.size!=bytes[which])return RF_FORMAT;
        status=rf_vpp_read(maps+i,&entry,0,h,sizeof(h));if(status)return status;
        if(h[0] || h[1] || (uint32_t)h[2]!=type[which] || (uint32_t)h[16]!=bits[which] || (h[17]&0xc0) ||
           ((uint32_t)h[12]|(uint32_t)h[13]<<8)!=size || ((uint32_t)h[14]|(uint32_t)h[15]<<8)!=size)return RF_FORMAT;
        return RF_OK;
    }
    return RF_NOT_FOUND;
}
static int scene_nano_hit_open(scene_stream *stream,rf_vpp *effects,rf_vpp *maps,uint32_t map_count)
{
    static const char *const files[3]={"NanoShieldHitSmall.vfx","NanoShieldHitBig.vfx","NanoShieldDestroy.vfx"};
    static const char *const names[6]={"NanoFlare01.tga","NanoShieldHit01.tga","NanoMuzzleFlash.tga","NanoVFXShock.tga","NanoHitFlare02.tga","Electricity01.tga"};
    static const uint32_t bytes[3]={5834,22794,17745};
    static const uint32_t sizes[3][12]={{757,4749,82,86},{21709,757,86,82},{2101,2101,2205,2101,2205,2457,1640,2101,182,179,181,68}};
    static const uint32_t flags[11]={0x15,0x110,0x110,0x15,0x15,0x15,0x14,0x15,0x14,0x814,0x15};
    static const uint32_t global_material[11]={0,1,0,1,0,1,1,2,1,1,2};
    static const uint32_t texture_ids[8]={0,1,1,0,2,3,4,5};
    rf_vfx_directory directory={0};scene_nano_hit_owner *v=NULL;unsigned char *raw=NULL;
    uint32_t kind,i,j,mesh_at=0,view_at=0,used,temporary=0,peak=0;int status=RF_RANGE;
    /* Fixed charge includes the embedded texture owner and all track/particle
     * state, read-only lookup/string storage, globals and conservative slack. */
    uint32_t fixed=(uint32_t)(sizeof(*v)+sizeof(scene_nano_hit)+sizeof(scene_nano_hit_pending)+
        sizeof(scene_nano_hit_epoch)+sizeof(rf_scene_nano_hit)+2048u+SCENE_NANO_HIT_ALLOWANCE);
    if(scene_nano_hit || !stream || !effects || !maps || !map_count || fixed>=SCENE_NANO_HIT_VISUAL_CAP)goto done;
    used=fixed;
    for(i=0;i<6;i++){status=scene_nano_hit_image_admit(maps,map_count,names[i],i);if(status)goto done;}
    v=calloc(1,sizeof(*v));if(!v){status=RF_IO;goto done;}v->stream=stream;
    for(kind=0;kind<3;kind++) {
        uint32_t count=kind==2?12u:4u,nmesh=kind==2?7u:2u,nmat=kind==2?4u:2u,mesh_begin=mesh_at;
        uint32_t largest=kind==0?4749u:kind==1?21709u:2457u;
        status=rf_vfx_directory_open(effects,files[kind],SCENE_NANO_HIT_VISUAL_CAP-used,&directory);if(status)goto done;
        if(directory.header.version!=0x40006 || directory.entry.size!=bytes[kind] || directory.count!=count ||
           directory.header.values[1]!=(kind==2?29u:5u) || directory.header.values[2]!=nmesh || directory.header.values[9]!=nmat ||
           directory.header.values[4] || directory.header.values[5]!=(kind==2?1u:0u) || directory.header.values[6]){status=RF_FORMAT;goto done;}
        for(i=0;i<count;i++) {
            uint32_t tag=kind==2 && i==6?0x54524150u:i<(kind==2?8u:2u)?0x4f584653u:0x4c54414du;
            if(directory.chunks[i].type!=tag || directory.chunks[i].bytes!=sizes[kind][i]){status=RF_FORMAT;goto done;}
        }
        temporary=directory.allocated_bytes+largest;
        if((uint64_t)used+temporary>=SCENE_NANO_HIT_VISUAL_CAP){status=RF_RANGE;goto done;}
        raw=malloc(largest);if(!raw){status=RF_IO;goto done;}
        status=rf_vfx_material_bank_open(&directory,SCENE_NANO_HIT_VISUAL_CAP-used-temporary,v->materials+kind);if(status)goto done;
        used+=v->materials[kind]->allocated_bytes;if(v->materials[kind]->count!=nmat){status=RF_FORMAT;goto done;}
        for(i=0;i<nmat;i++) {
            const rf_vfx_material_view *m=v->materials[kind]->views+i;float speed,value;uint32_t word;
            memcpy(&speed,m->words+15,4);
            if(m->words[0] || m->words[1] || m->words[2]!=1 || m->words[30]!=15 || m->bitmap_requests!=1 ||
               strcmp((const char *)(m->words+5),names[texture_ids[view_at+i]]) || m->words[14] || speed!=1 || m->words[16]!=2 ||
               m->words[33] || m->words[34] || m->words[35] || m->words[46]!=1 ||
               m->words[48]!=(kind==2?(i==3?1u:29u):5u)){status=RF_FORMAT;goto done;}
            status=rf_vfx_material_sample(v->materials[kind]->data,v->materials[kind]->bytes,m,1,0,&word);if(status)goto done;
            memcpy(&value,&word,4);if(value!=1){status=RF_FORMAT;goto done;}
            for(j=0;j<m->words[48];j++) {
                status=rf_vfx_material_sample(v->materials[kind]->data,v->materials[kind]->bytes,m,2,j,&word);if(status)goto done;
                memcpy(&value,&word,4);if(!isfinite(value) || value<0 || value>1){status=RF_FORMAT;goto done;}
            }
            v->texture_views[view_at+i]=*m;
        }
        for(i=0;i<(kind==2?8u:2u);i++) {
            status=rf_vfx_chunk_read(&directory,i,0,raw,sizes[kind][i]);if(status)goto done;
            if(kind==2 && i==6){status=scene_nano_hit_part_read(raw,sizes[kind][i],v);if(status)goto done;continue;}
            status=rf_vfx_mesh_open(raw,sizes[kind][i],0x40006,nmat,NULL,SCENE_NANO_HIT_VISUAL_CAP-used-temporary,v->meshes+mesh_at);if(status)goto done;
            {const rf_vfx_mesh *m=v->meshes[mesh_at];uint32_t sphere=mesh_at==1 || mesh_at==2;
             used+=m->allocated_bytes;
             if(strcmp(m->prefix.parent,"Scene Root") || m->prefix.vertices!=(sphere?(mesh_at==1?17u:49u):4u) ||
                m->prefix.faces!=(sphere?(mesh_at==1?16u:80u):(mesh_at==6 || mesh_at==8 || mesh_at==9)?4u:2u) ||
                m->prefix.timing.samples!=(kind==2?30u:6u) || m->prefix.timing.start!=0 ||
                m->materials!=1 || scene_nano_hit_word(m->data+m->material_offset)!=global_material[mesh_at] ||
                m->edges.flags!=flags[mesh_at] || !isfinite(m->edges.radius)){status=RF_FORMAT;goto done;}
             v->radius[kind]=fmaxf(v->radius[kind],m->edges.radius);
            }
            ++mesh_at;
        }
        if(mesh_at-mesh_begin!=nmesh){status=RF_FORMAT;goto done;}
        if(used+temporary>peak)peak=used+temporary;
        free(raw);raw=NULL;rf_vfx_directory_close(&directory);temporary=0;view_at+=nmat;
    }
    /* Beam direction is an authored mesh field, never a camera-facing guess. */
    {const float *d=v->meshes[9]->frames[0].direction;double length=0;
     if(!(v->meshes[9]->frames[0].present&4)){status=RF_FORMAT;goto done;}
     for(i=0;i<3;i++)length+=(double)d[i]*d[i];
     if(!isfinite(length) || length<=0){status=RF_FORMAT;goto done;}length=sqrt(length);
     for(i=0;i<3;i++)v->beam_direction[i]=(float)(d[i]/length);
     for(i=1;i<v->meshes[9]->prefix.timing.samples;i++)
        if((v->meshes[9]->frames[i].present&4) && memcmp(d,v->meshes[9]->frames[i].direction,12)){status=RF_FORMAT;goto done;}}
    for(i=0;i<10;i++)for(j=0;j<(i<8?4u:7u);j++) {
        status=rf_vfx_instance_open(v->meshes[i<8?j:4+j],SCENE_NANO_HIT_VISUAL_CAP-used,v->slots[i].instances+j);if(status)goto done;
        used+=v->slots[i].instances[j]->allocated_bytes;
    }
    status=rf_vfx_material_textures_open(&v->textures,v->texture_views,8,maps,map_count,
        SCENE_NANO_HIT_VISUAL_CAP-used+(uint32_t)sizeof(v->textures));if(status)goto done;
    if(v->textures.resident_bytes<sizeof(v->textures) || !v->textures.bindings || !v->textures.textures ||
       v->textures.count!=8 || v->textures.texture_count!=6){status=RF_FORMAT;goto done;}
    used+=v->textures.resident_bytes-(uint32_t)sizeof(v->textures);
    for(i=0;i<8;i++)if(v->textures.bindings[i][0]!=texture_ids[i] ||
       v->textures.bindings[i][1]!=UINT32_MAX || v->textures.bindings[i][2]!=UINT32_MAX){status=RF_FORMAT;goto done;}
    for(i=0;i<6;i++) {
        const rf_particle_animation *a=&v->textures.textures[i].animation;uint32_t size=i==5?64u:128u;
        uint32_t format=i==1 || i==4 || i==5?7u:6u;
        if(strcmp(v->textures.textures[i].name,names[i]) || a->count!=1 || !a->images || !a->images[0].rgba ||
           a->images[0].width!=size || a->images[0].height!=size || a->images[0].source_format!=format ||
           a->images[0].bytes!=size*size*4){status=RF_FORMAT;goto done;}
    }
    if(used>peak)peak=used;if(peak>SCENE_NANO_HIT_VISUAL_CAP){status=RF_RANGE;goto done;}
    v->resident_bytes=used;v->peak_bytes=peak;scene_nano_hit=v;v=NULL;scene_nano_hit_reset();
    rf_scene_nano_hit[1]=used;rf_scene_nano_hit[2]=peak;status=RF_OK;
done:
    free(raw);rf_vfx_directory_close(&directory);scene_nano_hit_free(v);rf_scene_nano_hit[0]=(uint32_t)status;return status;
}

static void scene_nano_hit_transform(const float local[3],const float origin[3],const float basis[9],float out[3])
{
    uint32_t i;for(i=0;i<3;i++)out[i]=(float)((double)origin[i]+
        (((double)local[2]*basis[6+i]+(double)local[1]*basis[3+i])+(double)local[0]*basis[i]));
}
static int scene_nano_hit_sample(scene_nano_hit_owner *v,scene_nano_hit_slot *s)
{
    uint32_t i,j,begin=s->kind==2?0:s->kind*2,n=s->kind==2?7:2;float time=s->seconds*15.f;int status;
    s->cull_radius=0;
    for(i=0;i<n;i++) {
        rf_vfx_instance *instance=s->instances[begin+i];const rf_vfx_mesh *m=instance->mesh;
        status=rf_vfx_instance_update(instance,time);if(status)return status;if(!instance->active)continue;
        if(m->edges.flags&(1u|0x800u)) {
            double length=0;for(j=0;j<3;j++)length+=(double)instance->center[j]*instance->center[j];
            s->cull_radius=fmaxf(s->cull_radius,(float)(sqrt(length)+
                sqrt((double)instance->extra[0]*instance->extra[0]+(double)instance->extra[1]*instance->extra[1])*.5));
        }else for(j=0;j<m->prefix.vertices;j++) {
            const float *point=instance->vertices+j*3;
            s->cull_radius=fmaxf(s->cull_radius,(float)sqrt((double)point[0]*point[0]+(double)point[1]*point[1]+(double)point[2]*point[2]));
        }
    }
    if(!isfinite(s->cull_radius))return RF_RANGE;
    if(s->kind==2)return rf_vfx_material_evaluate(v->materials[2]->data,v->materials[2]->bytes,v->materials[2]->views+3,2,time,&s->opacity);
    return RF_OK;
}
/* Complete captured request; no actor, pose, contact, or audio handle retained. */
static void scene_nano_hit_emit(uint32_t kind,uint32_t room,const float position[3],const float *direction)
{
    scene_nano_hit_owner *v;scene_nano_hit_slot *s;float basis[3][3]={{1,0,0},{0,1,0},{0,0,1}};
    uint32_t i,j,first=kind==2?8u:0u,end=kind==2?10u:8u;uint64_t epoch=scene_nano_hit_epoch;
    if(kind>2 || !room)return;
    ++rf_scene_nano_hit[4];scene_nano_hit_audio_request(position); /* Before visual admission. */
    v=scene_nano_hit;if(epoch!=scene_nano_hit_epoch || !v || v->ticket==UINT64_MAX)return;
    if(kind!=2 && (!direction || rf_entity_navigation_basis(direction,basis))){++rf_scene_nano_hit[7];return;}
    for(i=first;i<end;i++)if(!v->slots[i].active)break;
    if(i==end){++rf_scene_nano_hit[6];return;}s=v->slots+i;
    s->seconds=s->credit=0;s->opacity=1;s->live=0;s->kind=kind;s->room=room;s->fresh=1;
    s->ticket=++v->ticket;s->queued_ticket=0;memcpy(s->position,position,12);memcpy(s->basis,basis,36);
    if(kind==2)for(j=0;j<100;j++)v->particles[i-8][j].age=-1;
    if(scene_nano_hit_sample(v,s)){++rf_scene_nano_hit[15];return;}
    s->active=1;++rf_scene_nano_hit[5];
}
/* Admission/payment has already completed. depleted is an explicit success
 * receipt from this contact, never inferred from armor, Event76 or restore. */
static void scene_nano_hit_response(campaign_npc_body *owner,uint32_t big,const float *point,uint32_t depleted,uint32_t is_direction)
{
    scene_stream *stream=scene_nano_hit_pending.stream;rf_collision_room_location location;
    float position[3],direction[3];uint32_t i;uint64_t epoch=scene_nano_hit_epoch;
    if(!owner || !stream || !stream->collision)return;
    for(i=0;i<3;i++) {
        position[i]=owner->published[i];if(!isfinite(position[i]))return;
        direction[i]=point?(is_direction?point[i]:(float)((double)point[i]-position[i])):0;
    }
    /* The current accepted actor origin selects a room once; no new collision
     * test can change admission. Missing geometry suppresses only imagery. */
    if(rf_geometry_collision_world_locate(stream->collision,position,&location) || location.room==UINT32_MAX)return;
    if(depleted)scene_nano_hit_emit(2,location.room+1,position,NULL);
    if(epoch!=scene_nano_hit_epoch)return; /* Scene changed inside external audio service. */
    scene_nano_hit_emit(big?1u:0u,location.room+1,position,point?direction:NULL);
}
static float scene_nano_hit_range(float low,float high)
{return (float)(((double)high-low)*((double)rf_random_next(&impact_sound_random)/32768.0)+low);}
static int scene_nano_hit_birth(scene_nano_hit_owner *v,scene_nano_hit_slot *s,float fraction,float dt,scene_nano_hit_particle *p)
{
    float d[3],local[3],point[3],speed,age;uint32_t k;int status;
    status=rf_particle_cone_sample(-1,&impact_sound_random,d);if(status)return status;
    speed=scene_nano_hit_range(-v->tracks[0][11],v->tracks[0][11])+v->tracks[0][10];
    for(k=0;k<3;k++)local[k]=d[k]*speed;
    {const float zero[3]={0};scene_nano_hit_transform(local,zero,s->basis,p->velocity);}
    for(k=0;k<3;k++)local[k]=(float)((double)d[k]*v->tracks[0][7]+v->tracks[0][k]);
    scene_nano_hit_transform(local,s->position,s->basis,point);
    age=scene_nano_hit_range(0,(float)(fraction*dt));p->age=age;
    for(k=0;k<3;k++){p->position[k]=point[k]+p->velocity[k]*age;if(!isfinite(p->position[k]))return RF_RANGE;}
    /* A zero-width lifetime draw still advances the shared presentation RNG. */
    p->life=(scene_nano_hit_range(-0.f,0.f)+2400.f)*.00020833333837799728f;
    p->angle=scene_nano_hit_range(0,6.2831853071795864769f);++s->live;++rf_scene_nano_hit[12];return RF_OK;
}
static int scene_nano_hit_part_step(scene_nano_hit_owner *v,scene_nano_hit_slot *s,uint32_t slot,float previous,float dt)
{
    float now=s->seconds*15.f,carry=s->credit;uint32_t i,k,wanted,left;int status;
    scene_nano_hit_particle *particles=v->particles[slot-8];
    for(i=0;i<100;i++) {
        scene_nano_hit_particle *p=particles+i;if(p->age<0)continue;
        p->age+=dt;if(p->age>=p->life){p->age=-1;--s->live;continue;}
        /* Installed flags10: no gravity, forces, inherited velocity or bounce. */
        for(k=0;k<3;k++)p->position[k]+=p->velocity[k]*dt;
    }
    /*540bf0: current-frame admission, left-held rates, carry stores per span.
     * Unchanged frames are not scheduled by this caller. No offscreen rebuild. */
    if(!(now>0 && now<30 && previous<now))return RF_OK;
    {float at=previous;
     while(at<now) {
        uint32_t index=(uint32_t)floorf(at);float next=fminf(now,floorf(at)+1.f);
        if(index>=30 || next<=at)return RF_RANGE;
        carry=(float)((double)carry+(double)v->tracks[index][12]*(next-at)*320.0);at=next;
     }}
    if(!isfinite(carry) || carry<0 || carry>100.f)return RF_RANGE;
    wanted=(uint32_t)floorf(carry);s->credit=carry-wanted;left=wanted;
    for(i=0;i<100 && left;i++)if(particles[i].age==-1.f) {
        status=scene_nano_hit_birth(v,s,(float)left/(float)wanted,dt,particles+i);if(status)return status;--left;
    }
    rf_scene_nano_hit[13]+=left;return RF_OK;
}
static void scene_nano_hit_tick(scene_stream *stream,float dt,uint32_t frame)
{
    scene_nano_hit_owner *v=scene_nano_hit;uint32_t i;int status;
    if(!v || v->stream!=stream || !isfinite(dt) || dt<0)return;
    rf_scene_nano_hit[9]=0;
    for(i=0;i<10;i++) {
        scene_nano_hit_slot *s=v->slots+i;float previous;
        if(!s->active)continue;
        if(!s->fresh && s->frame==frame){++rf_scene_nano_hit[9];continue;}
        s->queued_ticket=0;
        /*4c1d50 retires completion from the previous advance, before this one. */
        if(s->seconds*15.f>=(s->kind==2?29.f:5.f) && !s->live) {
            s->active=0;++rf_scene_nano_hit[8];continue;
        }
        previous=s->seconds*15.f;s->frame=frame;
        if(s->fresh){s->fresh=0;++rf_scene_nano_hit[9];continue;}
        s->seconds=(float)((double)s->seconds+dt);status=RF_OK;
        if(!isfinite(s->seconds))status=RF_RANGE;
        if(!status && s->kind==2)status=scene_nano_hit_part_step(v,s,i,previous,dt);
        if(!status)status=scene_nano_hit_sample(v,s);
        if(status){s->active=0;++rf_scene_nano_hit[15];rf_scene_nano_hit[0]=(uint32_t)status;continue;}
        ++rf_scene_nano_hit[9];
    }
}
static void scene_nano_hit_prepare(scene_stream *stream,rf_vpp *meshes,rf_vpp *maps,uint32_t map_count)
{
    scene_nano_hit_pending.stream=stream;scene_nano_hit_pending.meshes=meshes;
    scene_nano_hit_pending.maps=maps;scene_nano_hit_pending.map_count=map_count;scene_nano_hit_pending.attempted=0;
    memset(rf_scene_nano_hit,0,sizeof(rf_scene_nano_hit));memset(rf_scene_nano_hit_audio,0,sizeof(rf_scene_nano_hit_audio));
}
static void scene_nano_hit_admit(scene_stream *stream)
{
    uint32_t i,demand=0;
    if(scene_nano_hit_pending.stream!=stream || scene_nano_hit_pending.attempted)return;
    scene_nano_hit_pending.attempted=1;
    if(!campaign_seeds.items || !campaign_seeds.classes)return;
    for(i=0;i<campaign_seeds.records.count;i++) {
        uint32_t cls=campaign_seeds.items[i].class_index;
        if(cls<campaign_seeds.class_count && (campaign_seeds.classes[cls].physics.flags&0x02000000u))++demand;
    }
    rf_scene_nano_hit[3]=demand;if(!demand)return;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_nano_hit[14]=rf_xbox_available_memory_pages();
    if(rf_scene_nano_hit[14]<4096u+SCENE_NANO_HIT_TOTAL_CAP/4096u){rf_scene_nano_hit[0]=RF_RANGE;return;}
#endif
    scene_nano_hit_audio_load();
    (void)scene_nano_hit_open(stream,scene_nano_hit_pending.meshes,scene_nano_hit_pending.maps,scene_nano_hit_pending.map_count);
}
static void scene_nano_hit_queue_room(scene_stream *stream,uint32_t room,rf_render_queue_record *records,uint32_t capacity,uint32_t *count)
{
    scene_nano_hit_owner *v=scene_nano_hit;uint32_t i,j,accepted;int status;
    if(!v || v->stream!=stream)return;
    for(i=0;i<10;i++) {
        scene_nano_hit_slot *s=v->slots+i;rf_render_queue_record entry={0};
        if(!s->active || s->room!=room)continue;
        s->queued_ticket=s->ticket;entry.sorted=1;entry.lighting_flag=1;entry.object=i;entry.callback=SCENE_NANO_HIT_CALLBACK;
        if(s->seconds*15.f<(s->kind==2?29.f:5.f) && s->cull_radius>0) {
            memcpy(entry.position,s->position,12);entry.radius=s->cull_radius;
            status=rf_render_queue_append(&stream->particle_camera.frustum,entry.position,&entry,records,capacity,count,&accepted);
            if(status || !accepted)++rf_scene_nano_hit[10];
        }
        if(s->kind==2)for(j=0;j<100;j++) {
            const scene_nano_hit_particle *p=v->particles[i-8]+j;
            if(p->age<0 || p->age>=p->life)continue;
            entry.object=(i-8)*100+j;entry.callback=SCENE_NANO_HIT_PARTICLE_CALLBACK;
            memcpy(entry.position,p->position,12);entry.radius=v->tracks[0][9]*2.f;
            status=rf_render_queue_append(&stream->particle_camera.frustum,entry.position,&entry,records,capacity,count,&accepted);
            if(status || !accepted)++rf_scene_nano_hit[10];
        }
    }
}
static int scene_nano_hit_submit(scene_stream *stream,const rf_particle_screen_polygon *polygon,const rf_image *image,
    uint32_t alpha,rf_scene_particle_sink sink,void *context)
{
    rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    rf_particle_vertex_environment env={0};rf_particle_draw_vertex vertices[12];uint32_t i;int status;
    if(!polygon->count || !alpha)return RF_OK;if(polygon->count>12)return RF_RANGE;
    status=rf_particle_render_decode(RF_PARTICLE_GLOW_MODE,&re,&states);if(status)return status;
    env.rgba=0x00ffffffu|(alpha<<24);env.vertex_color=states.vertex_color;env.vertex_alpha=states.vertex_alpha;
    env.depth_scale=env.reciprocal_scale=stream->particle_camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
    for(i=0;i<polygon->count;i++){status=rf_particle_vertex_encode(&env,polygon->vertices+i,vertices+i);if(status)return status;}
    if(sink){status=sink(context,vertices,polygon->count,image,RF_PARTICLE_GLOW_MODE);if(status)return status;}
    ++rf_scene_nano_hit[11];return RF_OK;
}
static int scene_nano_hit_draw(scene_stream *stream,uint32_t id,uint32_t particle,rf_scene_particle_sink sink,void *context)
{
    scene_nano_hit_owner *v=scene_nano_hit;scene_nano_hit_slot *s;uint32_t slot=particle?8+id/100:id;
    uint32_t i,j,k,kind,begin,n;float time;int status;
    if(!v || v->stream!=stream || slot>=10 || (particle && id>=200))return RF_OK;s=v->slots+slot;
    if(!s->active || !s->queued_ticket || s->queued_ticket!=s->ticket)return RF_OK;
    kind=s->kind;time=s->seconds*15.f;
    if(particle) {
        const scene_nano_hit_particle *p=v->particles[slot-8]+id%100;const rf_image *image;
        rf_particle_screen_polygon polygon={0};float t,envelope,radius,alpha;
        if(p->age<0 || p->age>=p->life)return RF_OK;
        t=p->age/p->life;envelope=t>.5f?(1.f-t)/.5f:1.f;
        radius=(v->tracks[0][9]*2.f)*envelope;alpha=s->opacity*envelope;
        status=rf_vfx_material_texture_sample(&v->textures,v->materials[2]->views+3,7,0,p->age*15.f,0,&image);if(status)return status;
        status=rf_particle_world_billboard(&stream->particle_camera,p->position,p->angle,radius,image->width,image->height,&polygon);if(status)return status;
        /*557aa6 truncates the wide product; selected brightness1 floors RGB255. */
        return scene_nano_hit_submit(stream,&polygon,image,(uint32_t)((double)alpha*255.0)&255u,sink,context);
    }
    if(time>=(kind==2?29.f:5.f))return RF_OK;
    begin=kind==2?0:kind*2;n=kind==2?7:2;
    for(i=0;i<n;i++) {
        rf_vfx_instance *instance=s->instances[begin+i];const rf_vfx_mesh *m=instance->mesh;
        const rf_vfx_material_bank *bank=v->materials[kind];const rf_vfx_material_view *material;
        const rf_image *image;uint32_t ordinal=scene_nano_hit_word(m->data+m->material_offset),alpha;float opacity;
        if(!instance->active)continue;material=bank->views+ordinal;
        status=rf_vfx_material_evaluate(bank->data,bank->bytes,material,2,time,&opacity);if(status)return status;
        alpha=(uint32_t)(fmaxf(0,fminf(1,opacity))*255.f+.5f);if(!alpha)continue;
        status=rf_vfx_material_texture_sample(&v->textures,material,(kind==2?4:kind*2)+ordinal,0,time,0,&image);if(status)return status;
        for(j=0;j<m->prefix.faces;j++) {
            rf_particle_screen_polygon polygon={0};
            if(m->edges.flags&0x800u) {
                float first[3],second[3],a[3],b[3];if(j)break;
                if(instance->extra[0]<=0 || instance->extra[1]<=0)continue;
                for(k=0;k<3;k++){float half=v->beam_direction[k]*instance->extra[1]*.5f;a[k]=instance->center[k]-half;b[k]=instance->center[k]+half;}
                scene_nano_hit_transform(a,s->position,s->basis,first);scene_nano_hit_transform(b,s->position,s->basis,second);
                status=rf_volume_beam_project(&stream->particle_camera,second,first,instance->extra[0],&polygon);
            }else if(m->edges.flags&1u) {
                float center[3];if(j)break;if(instance->extra[0]<=0 || instance->extra[1]<=0)continue;
                scene_nano_hit_transform(instance->center,s->position,s->basis,center);
                status=scene_cane_flare_project(&stream->particle_camera,center,instance->extra,&polygon);
            }else {
                rf_vfx_face face;rf_vfx_face_output facing;float points[9];rf_particle_billboard_vertex quad[4];
                uint32_t at=m->prefix.face_offset+j*96;
                status=rf_vfx_face_read(m->data+at,m->bytes-at,m->version,&face);if(status)return status;
                if(face.material)return RF_FORMAT;
                for(k=0;k<3;k++)scene_nano_hit_transform(instance->vertices+face.indices[k]*3,s->position,s->basis,points+k*3);
                status=rf_vfx_world_face(&stream->particle_camera,points,(int32_t)ordinal,&facing);
                if(status==RF_RANGE || status==RF_FORMAT)continue;if(status)return status;if(!facing.visible)continue;
                for(k=0;k<3;k++){memcpy(quad[k].position,points+k*3,12);quad[k].uv[0]=instance->uv[j*6+k];quad[k].uv[1]=instance->uv[j*6+3+k];}
                quad[3]=quad[2];status=rf_particle_world_quad(&stream->particle_camera,quad,&polygon);
            }
            if(status)return status;status=scene_nano_hit_submit(stream,&polygon,image,alpha,sink,context);if(status)return status;
        }
    }
    return RF_OK;
}
#endif
