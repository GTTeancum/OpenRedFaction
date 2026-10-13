#ifndef RF_SCENE_PLAY_VCLIP_RESOURCES_INCLUDED
#define RF_SCENE_PLAY_VCLIP_RESOURCES_INCLUDED
/* Authored Play_Vclip70 presentation. Original data stays in the VPPs.
 * All seven SFXO, ten PART and fifteen MATL records of transport_explode
 * are required: no partial effect is published. See AUTHORED-PLAY-VCLIP.md. */
#define SCENE_VCLIP_BUDGET (4u*1024u*1024u)
#define SCENE_VCLIP_RUNTIME_BUDGET (128u*1024u)
#define SCENE_VCLIP_SCRATCH 8192u
/* Charge allocator headers/page fragmentation beyond known native pixels;
 * no other consumers' standing caches are enlarged by this owner. */
#define SCENE_VCLIP_ALLOCATOR_RESERVE 16384u
#define SCENE_VCLIP_MESHES 7u
#define SCENE_VCLIP_TRACKS 10u
#define SCENE_VCLIP_PARTICLES 710u
/* The new scheduler callback/context are persistent outside the owner. */
#define SCENE_VCLIP_CALLBACK_BYTES (sizeof(((rf_runtime_triggers *)0)->play_vclip)+sizeof(((rf_runtime_triggers *)0)->vclip_context))
#ifdef RF_IMAGE_XBOX_NATIVE
extern uint32_t rf_xbox_available_memory_pages(void);
#endif
typedef struct scene_vclip_particle {float age,life,angle,position[3],velocity[3];} scene_vclip_particle;
typedef struct scene_vclip_track {
    char parent[65];float *samples;
    uint32_t flags,start,count,material,capacity,first,shape,parent_mesh;
    int32_t emit_after;float lifetime,variance,size_in,size_out,alpha_in,alpha_out,streak;
    float credit; /* Only mutable field in this resource record. */
} scene_vclip_track;
typedef struct scene_vclip_owner {
    char name[64],foley[64];
    rf_vfx_mesh *meshes[SCENE_VCLIP_MESHES];rf_vfx_instance *instances[SCENE_VCLIP_MESHES];
    rf_vfx_material_bank *materials;rf_vfx_material_textures textures;rf_image white;
    scene_vclip_track tracks[SCENE_VCLIP_TRACKS];scene_vclip_particle particles[SCENE_VCLIP_PARTICLES];
    rf_random_state random;float position[3],time,duration;int32_t last_ms;
    uint32_t active,mesh_count,track_count,particle_count,resident_bytes,peak_bytes,runtime_bytes;
} scene_vclip_owner;
static scene_vclip_owner *scene_play_vclip_owner;
/* demand,admitted,status,resident,peak,runtime,pixels,padding,descriptors,
 * track bytes,geometry bytes,free pages,starts,busy/refused,births,pool drops,
 * live particles,mesh submissions,particle submissions,last UID,retirements,
 * successful-load resets,resource failures,reserved. */
uint32_t rf_scene_play_vclip[24];
static uint32_t scene_vclip_word(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static float scene_vclip_float(const unsigned char *p)
{uint32_t w=scene_vclip_word(p);float f;memcpy(&f,&w,4);return f;}
static int scene_vclip_name_equal(const char *a,const char *b)
{
    while(*a&&*b){unsigned x=(unsigned char)*a++,y=(unsigned char)*b++;
        if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 0;}
    return *a==*b;
}
static void scene_play_vclip_reset(void)
{
    scene_vclip_owner *v=scene_play_vclip_owner;uint32_t i;
    if(!v)return;v->active=0;v->time=0;v->last_ms=-1;
    for(i=0;i<SCENE_VCLIP_PARTICLES;i++)v->particles[i].age=-1;
    for(i=0;i<v->track_count;i++)v->tracks[i].credit=0;
    rf_scene_play_vclip[16]=0;
}
static void scene_vclip_free(scene_vclip_owner *v)
{
    uint32_t i;if(!v)return;
    for(i=0;i<SCENE_VCLIP_MESHES;i++){rf_vfx_instance_close(v->instances+i);rf_vfx_mesh_close(v->meshes+i);}
    for(i=0;i<SCENE_VCLIP_TRACKS;i++)free(v->tracks[i].samples);
    rf_vfx_material_textures_close(&v->textures);rf_vfx_material_bank_close(&v->materials);
    rf_image_close(&v->white);free(v);
}
static void scene_play_vclip_close(void)
{scene_vclip_free(scene_play_vclip_owner);scene_play_vclip_owner=NULL;rf_scene_play_vclip[3]=0;}
static int scene_vclip_part_read(const unsigned char *p,uint32_t bytes,scene_vclip_track *t,uint32_t budget)
{
    const unsigned char *end;uint32_t at,n,i;uint64_t size;
    if(!p||!t)return RF_RANGE;
    end=memchr(p,0,bytes);if(!end||end-p>64)return RF_FORMAT;at=(uint32_t)(end-p)+1;
    end=memchr(p+at,0,bytes-at);if(!end||(n=(uint32_t)(end-(p+at)))>64)return RF_FORMAT;
    memcpy(t->parent,p+at,n);at+=n+1;
    if(bytes-at<57||p[at++])return RF_FORMAT;
    t->flags=scene_vclip_word(p+at);t->start=scene_vclip_word(p+at+8);t->count=scene_vclip_word(p+at+12);
    t->material=scene_vclip_word(p+at+16);t->capacity=scene_vclip_word(p+at+20);
    {uint32_t w=scene_vclip_word(p+at+24);memcpy(&t->emit_after,&w,4);}
    t->lifetime=(float)scene_vclip_word(p+at+28)*(1.f/4800.f);t->variance=scene_vclip_float(p+at+32);
    t->shape=scene_vclip_word(p+at+36);t->size_in=scene_vclip_float(p+at+40);t->size_out=scene_vclip_float(p+at+44);
    t->alpha_in=scene_vclip_float(p+at+48);t->alpha_out=scene_vclip_float(p+at+52);
    if((t->flags!=4&&t->flags!=20&&t->flags!=260)||scene_vclip_word(p+at+4)||t->start||t->count!=91||
       !t->capacity||t->capacity>100||t->material>=15||t->shape>1||t->emit_after < -2||t->emit_after>90||
       !isfinite(t->lifetime)||t->lifetime<=0||t->lifetime>4||t->variance!=0)return RF_FORMAT;
    at+=56;if(t->flags&256){if(bytes-at<4)return RF_FORMAT;t->streak=scene_vclip_float(p+at);at+=4;
        if(!isfinite(t->streak)||t->streak<0||t->streak>4)return RF_FORMAT;}
    if(!isfinite(t->size_in)||!isfinite(t->size_out)||!isfinite(t->alpha_in)||!isfinite(t->alpha_out)||
       t->size_in<0||t->size_out>1||t->size_in>t->size_out||t->alpha_in<0||t->alpha_out>1||t->alpha_in>t->alpha_out)return RF_FORMAT;
    size=(uint64_t)t->count*52;if(size!=bytes-at||size>budget)return RF_RANGE;
    for(i=0;i<t->count;i++){
        uint32_t k;float q[13];for(k=0;k<13;k++){q[k]=scene_vclip_float(p+at+i*52+k*4);if(!isfinite(q[k]))return RF_FORMAT;}
        if(q[7]<0||q[8]<0||q[9]<0||q[9]>100||q[11]<0||q[12]<0||q[12]>1)return RF_FORMAT;
    }
    t->samples=malloc((size_t)size);if(!t->samples)return RF_IO;memcpy(t->samples,p+at,(size_t)size);return RF_OK;
}
/* Headers preflight the *complete* decoded image cost before first allocation.
 * Generic bitmap decoding streams from VPP through its4KiB reader. Dedup is
 * identical to the material owner, and every native image is page charged. */
static int scene_vclip_texture_cost(const rf_vfx_material_bank *bank,rf_vpp *maps,uint32_t map_count,
    uint32_t *pixels,uint32_t *padding,uint32_t *descriptors)
{
    char names[45][33];uint32_t count=0,i,j,k;uint64_t p=0,extra=0,desc=0;
    static const uint32_t offsets[3]={5,18,36};
    for(i=0;i<bank->count;i++)for(j=0;j<3;j++)if(bank->views[i].bitmap_requests&(1u<<j)){
        const char *name=(const char *)(bank->views[i].words+offsets[j]);
        if(!memchr(name,0,33)||!name[0])return RF_FORMAT;
        for(k=0;k<count;k++)if(scene_vclip_name_equal(names[k],name))break;
        if(k<count)continue;if(count==45)return RF_RANGE;strcpy(names[count++],name);
    }
    for(i=0;i<count;i++){
        rf_vpp_entry e;unsigned char h[32];uint32_t w,height,frames=1;uint64_t bytes;int status=RF_NOT_FOUND;
        for(j=0;j<map_count;j++){status=rf_vpp_find(maps+j,names[i],&e);if(status!=RF_NOT_FOUND)break;}
        if(status)return status;if(e.size<32)return RF_FORMAT;
        status=rf_vpp_read(maps+j,&e,0,h,32);if(status)return status;
        if(!memcmp(h,".vbm",4)){w=scene_vclip_word(h+8);height=scene_vclip_word(h+12);frames=scene_vclip_word(h+24);}
        else {w=h[12]|(uint32_t)h[13]<<8;height=h[14]|(uint32_t)h[15]<<8;}
        if(!w||!height||w>4096||height>4096||(w&(w-1))||(height&(height-1))||!frames||frames>128)return RF_RANGE;
        bytes=(uint64_t)w*height*4;p+=bytes*frames;extra+=((4096-(bytes&4095))&4095)*frames;desc+=(uint64_t)frames*sizeof(rf_image);
    }
    if(p+extra+desc>SCENE_VCLIP_BUDGET)return RF_RANGE;
    *pixels=(uint32_t)p;*padding=(uint32_t)extra;*descriptors=(uint32_t)desc;return RF_OK;
}
static int scene_vclip_open(rf_vpp *tables,rf_vpp *effects,rf_vpp *maps,uint32_t map_count,const char *name)
{
    rf_vclip_definition clip;rf_vfx_directory d={0};scene_vclip_owner *v=NULL;unsigned char *raw=NULL;
    uint32_t budget=SCENE_VCLIP_BUDGET,used=sizeof(*v)+sizeof(rf_scene_play_vclip)+sizeof(scene_play_vclip_owner)+SCENE_VCLIP_CALLBACK_BYTES+SCENE_VCLIP_ALLOCATOR_RESERVE,
        temporary,maximum=0,i,j,pixels,padding,descriptors,peak,
        runtime=sizeof(*v)+sizeof(rf_scene_play_vclip)+sizeof(scene_play_vclip_owner)+SCENE_VCLIP_CALLBACK_BYTES+SCENE_VCLIP_SCRATCH+SCENE_VCLIP_ALLOCATOR_RESERVE;int status;
    if(!name||!scene_vclip_name_equal(name,"transport_explode")||scene_play_vclip_owner)return RF_NOT_FOUND;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_play_vclip[11]=rf_xbox_available_memory_pages();
    if(rf_scene_play_vclip[11]<=4096)return RF_RANGE;
    if((uint64_t)(rf_scene_play_vclip[11]-4096)*4096<budget)budget=(rf_scene_play_vclip[11]-4096)*4096;
#endif
    if((uint64_t)used+65536+SCENE_VCLIP_SCRATCH>budget)return RF_RANGE;
    status=rf_vclip_definition_load(tables,name,65536,&clip);if(status)return status;
    if(strcmp(clip.vfx,"shipexp01_anim.VFX")||clip.vbm[0]||clip.explosion[0]||clip.has_particle||clip.damage!=0)return RF_FORMAT;
    peak=used+65536+SCENE_VCLIP_SCRATCH;
    status=rf_vfx_directory_open(effects,clip.vfx,budget-used-SCENE_VCLIP_SCRATCH,&d);if(status)return status;
    if(d.header.version!=0x40006||d.entry.size!=385723||d.count!=32||d.header.values[1]!=90||
       d.header.values[2]!=7||d.header.values[5]!=10||d.header.values[9]!=15){status=RF_FORMAT;goto done;}
    for(i=0;i<d.count;i++){
        if(d.chunks[i].type!=0x4f584653u&&d.chunks[i].type!=0x54524150u&&d.chunks[i].type!=0x4c54414du){status=RF_FORMAT;goto done;}
        if(d.chunks[i].bytes>maximum)maximum=d.chunks[i].bytes;
    }
    temporary=d.allocated_bytes+maximum+SCENE_VCLIP_SCRATCH;
    if((uint64_t)used+temporary>budget){status=RF_RANGE;goto done;}
    v=calloc(1,sizeof(*v));raw=malloc(maximum);if(!v||!raw){status=RF_IO;goto done;}
    strcpy(v->name,name);if(clip.has_foley)memcpy(v->foley,clip.foley,64);v->duration=90;
    status=rf_vfx_material_bank_open(&d,budget-used-temporary,&v->materials);if(status)goto done;
    used+=v->materials->allocated_bytes;if(v->materials->count!=15){status=RF_FORMAT;goto done;}
    for(i=0;i<15;i++)if(v->materials->views[i].words[0]!=0&&v->materials->views[i].words[0]!=2){status=RF_FORMAT;goto done;}
    for(i=0;i<d.count;i++)if(d.chunks[i].type!=0x4c54414du){
        status=rf_vfx_chunk_read(&d,i,0,raw,d.chunks[i].bytes);if(status)goto done;
        if(d.chunks[i].type==0x4f584653u){
            rf_vfx_mesh *m;uint32_t slot=v->mesh_count;if(slot==SCENE_VCLIP_MESHES){status=RF_RANGE;goto done;}
            status=rf_vfx_mesh_open(raw,d.chunks[i].bytes,d.header.version,15,NULL,budget-used-temporary,v->meshes+slot);if(status)goto done;
            m=v->meshes[slot];++v->mesh_count;used+=m->allocated_bytes;rf_scene_play_vclip[10]+=m->allocated_bytes;
            if(strcmp(m->prefix.parent,"Scene Root")||!m->prefix.vertices||!m->materials||
               (m->edges.flags!=4&&m->edges.flags!=17)){status=RF_FORMAT;goto done;}
            status=rf_vfx_instance_open(m,budget-used-temporary,v->instances+slot);if(status)goto done;
            used+=v->instances[slot]->allocated_bytes;runtime+=v->instances[slot]->allocated_bytes;
            if(runtime>SCENE_VCLIP_RUNTIME_BUDGET){status=RF_RANGE;goto done;}
        }else{
            scene_vclip_track *t;if(v->track_count==SCENE_VCLIP_TRACKS){status=RF_RANGE;goto done;}
            t=v->tracks+v->track_count++;status=scene_vclip_part_read(raw,d.chunks[i].bytes,t,budget-used-temporary);if(status)goto done;
            used+=t->count*52;rf_scene_play_vclip[9]+=t->count*52;t->first=v->particle_count;
            v->particle_count+=t->capacity;if(v->particle_count>SCENE_VCLIP_PARTICLES){status=RF_RANGE;goto done;}
        }
        if(used+temporary>peak)peak=used+temporary;
    }
    if(v->mesh_count!=7||v->track_count!=10||v->particle_count!=710){status=RF_FORMAT;goto done;}
    for(i=0;i<v->track_count;i++){
        scene_vclip_track *t=v->tracks+i;t->parent_mesh=UINT32_MAX;
        if(!strcmp(t->parent,"Scene Root"))continue;
        for(j=0;j<v->mesh_count;j++)if(!strcmp(t->parent,v->meshes[j]->prefix.name))break;
        /* These export tracks bake debris motion in effect space; direct
         * mesh parenting must not apply the same motion a second time. */
        if(j==v->mesh_count||v->meshes[j]->edges.flags!=4||
           (strcmp(t->parent,"OBJ_Ship_Debris05")&&strcmp(t->parent,"OBJ_Ship_Debris01"))){status=RF_FORMAT;goto done;}
        {const rf_vfx_mesh *parent=v->meshes[j];uint32_t frame,k;
            if(parent->prefix.timing.samples!=t->count||parent->prefix.timing.start!=0||parent->prefix.timing.end!=6){status=RF_FORMAT;goto done;}
            for(frame=0;frame<t->count;frame++){
                double distance=0;for(k=0;k<3;k++){double delta=(double)t->samples[frame*13+k]-parent->frames[frame].vectors[k];distance+=delta*delta;}
                /* Exported emitter remains inside the named debris bound on
                 * every source sample. A changed/local-space export refuses. */
                if(distance>(double)parent->edges.radius*parent->edges.radius){status=RF_FORMAT;goto done;}
            }
        }
        t->parent_mesh=j;
    }
    free(raw);raw=NULL;rf_vfx_directory_close(&d);temporary=SCENE_VCLIP_SCRATCH;
    status=scene_vclip_texture_cost(v->materials,maps,map_count,&pixels,&padding,&descriptors);if(status)goto done;
    /* One native white texel supports the authored flat-color material. */
    padding+=4092;if((uint64_t)used+temporary+pixels+descriptors+padding+4>budget){status=RF_RANGE;goto done;}
    status=rf_vfx_material_textures_open(&v->textures,v->materials->views,15,maps,map_count,
        budget-used-temporary-padding-4+(uint32_t)sizeof(v->textures));if(status)goto done;
    used+=v->textures.resident_bytes-(uint32_t)sizeof(v->textures)+padding;
    if((uint64_t)used+temporary+4>budget){status=RF_RANGE;goto done;}
    for(i=0;i<15;i++)for(j=0;j<3;j++)if((v->materials->views[i].bitmap_requests&(1u<<j))&&v->textures.bindings[i][j]==UINT32_MAX){status=RF_NOT_FOUND;goto done;}
    v->white.width=v->white.height=1;v->white.bytes=4;v->white.source_format=7;
    status=rf_image_allocate_pixels(&v->white);if(status)goto done;memset(v->white.rgba,255,4);used+=4;
    if(used+temporary>peak)peak=used+temporary;
    v->resident_bytes=used;v->peak_bytes=peak;v->runtime_bytes=runtime;
    rf_scene_play_vclip[3]=used;rf_scene_play_vclip[4]=peak;rf_scene_play_vclip[5]=runtime;
    rf_scene_play_vclip[6]=pixels+4;rf_scene_play_vclip[7]=padding;rf_scene_play_vclip[8]=descriptors;
    scene_play_vclip_owner=v;v=NULL;scene_play_vclip_reset();++rf_scene_play_vclip[1];status=RF_OK;
done:
    free(raw);rf_vfx_directory_close(&d);scene_vclip_free(v);return status;
}
#endif
