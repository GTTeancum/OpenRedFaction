#include "rf/corpse_effect.h"
typedef struct corpse_surface_fixture {uint32_t input[11],calls,names,fail;int32_t lookup[2];uint32_t *flags,mutation;} corpse_surface_fixture;
static uint32_t cs_metadata(void *p,uint32_t model){corpse_surface_fixture *f=p;(void)model;++f->calls;return 1;}
static int32_t cs_lookup(void *p,uint32_t metadata,const char *name,uint32_t fallback)
{corpse_surface_fixture *f=p;(void)metadata;
 if(!fallback){f->names=(f->names<<2)|(!strcmp(name,"eye")?1u:2u);if(f->flags)*f->flags=f->mutation;}
 return f->lookup[fallback];}
static int cs_place(void *p,const rf_corpse_surface_source *s,int32_t index,float point[3])
{corpse_surface_fixture *f=p;(void)s;(void)index;memset(point,0,12);return f->fail==1?RF_IO:RF_OK;}
static int cs_surface(void *p,uint32_t descriptor,const float point[3],rf_corpse_surface_hit *hit,uint32_t *matched)
{
    corpse_surface_fixture *f=p;(void)descriptor;(void)point;
    memcpy(hit->point,f->input+5,24);hit->face=1;*matched=f->input[4];return f->fail==2?RF_IO:RF_OK;
}
static int cs_color(void *p,uint32_t face,const float point[3],uint32_t *color)
{corpse_surface_fixture *f=p;(void)face;(void)point;*color=0xff123456;return f->fail==3?RF_IO:RF_OK;}
static uint32_t cs_index(rf_corpse_surface_effect *p,rf_corpse_surface_effect nodes[3])
{return p?(uint32_t)(p-nodes)+1:0;}
static int corpse_surface_probe(void)
{
    corpse_surface_fixture f;memset(&f,0,sizeof(f));f.lookup[0]=4;
    while(fread(f.input,sizeof(f.input),1,stdin)==1) {
        rf_corpse_surface_effect nodes[3];rf_corpse_surface_pool pool;
        rf_corpse_surface_source source;uint32_t out[28],i;float growth,size;
        rf_corpse_surface_backend backend={cs_metadata,cs_lookup,cs_place,cs_surface,cs_color,&f};
        if(f.input[0]<1 || f.input[0]>2 || f.input[1]>1)return 2;
        memset(nodes,0,sizeof(nodes));memset(&source,0,sizeof(source));
        source.descriptor=0x30003000;source.model=1;
        nodes[0].next=nodes[0].previous=nodes+(f.input[0]==2?2:0);
        nodes[1].next=nodes[1].previous=nodes+1;
        nodes[2].next=nodes[2].previous=nodes;
        pool.free=nodes;pool.active=f.input[1]?nodes+1:NULL;pool.capacity=3;
        memcpy(&growth,f.input+2,4);memcpy(&size,f.input+3,4);
        out[0]=(uint32_t)rf_corpse_surface_create(&pool,&source,1,"eye",growth,size,&backend);
        memcpy(out+1,nodes,76);out[20]=cs_index(pool.free,nodes);out[21]=cs_index(pool.active,nodes);
        for(i=0;i<3;++i){out[22+2*i]=cs_index(nodes[i].next,nodes);out[23+2*i]=cs_index(nodes[i].previous,nodes);}
        if(fwrite(out,sizeof(out),1,stdout)!=1)return 3;
    }
    return ferror(stdin)?1:0;
}

static int corpse_surface_guards(void)
{
    corpse_surface_fixture f;rf_corpse_surface_effect nodes[3];
    rf_corpse_surface_pool pool;rf_corpse_surface_source source;
    rf_corpse_surface_backend backend={cs_metadata,cs_lookup,cs_place,cs_surface,cs_color,&f};
    uint32_t selected,i,mode,flags,mutation;int status;float age[3],hit[6]={1,2,3,0,1,0};
    memset(&source,0,sizeof(source));source.descriptor=1;
    for(selected=0;selected<3;++selected)for(mode=0;mode<5;++mode) {
        memset(&f,0,sizeof(f));memset(nodes,0,sizeof(nodes));
        f.lookup[0]=-1;f.lookup[1]=mode?4:-1;f.fail=mode>1?mode-1:0;
        f.input[4]=mode>1;memcpy(f.input+5,hit,24);
        age[0]=age[1]=age[2]=1;age[selected]=3;
        for(i=0;i<3;++i){nodes[i].elapsed=age[i];nodes[i].next=nodes+(i+1)%3;nodes[i].previous=nodes+(i+2)%3;}
        pool.free=NULL;pool.active=nodes;pool.capacity=3;
        status=rf_corpse_surface_create(&pool,&source,1,"eye",5,.25f,&backend);
        if(status!=(mode>1?RF_IO:RF_OK) || pool.free!=nodes+selected ||
           pool.free->next!=pool.free || pool.free->previous!=pool.free ||
           pool.active!=nodes+(selected==0?1:0))return 1;
        if(nodes[selected].elapsed!=age[selected])return 2;
        if(mode==4) {if(nodes[selected].position[0]!=1 || nodes[selected].position[2]!=3 || nodes[selected].color)return 3;}
        else if(nodes[selected].position[0] || nodes[selected].position[1] || nodes[selected].position[2])return 4;
    }
    /* Dispatch uses the original's second live flag read, including mutation
     * during the first effect. A miss leaves the same slot available. */
    for(i=0;i<4;++i)for(mutation=0;mutation<4;++mutation) {
        uint32_t expected=0;memset(&f,0,sizeof(f));memset(nodes,0,sizeof(nodes));
        pool.free=nodes;pool.active=NULL;pool.capacity=1;nodes[0].next=nodes[0].previous=nodes;
        flags=i<<27;f.flags=&flags;f.mutation=mutation<<27;f.lookup[0]=4;
        if(i&1)expected=1;
        if((i&1?mutation:i)&2)expected=(expected<<2)|2;
        status=rf_corpse_source_effects(&pool,&source,&flags,1,&backend);
        if(status || f.names!=expected)return 5;
    }
    memset(&f,0,sizeof(f));pool.free=pool.active=NULL;pool.capacity=1;
    if(rf_corpse_surface_create(NULL,NULL,0,NULL,0,0,NULL)!=RF_OK)return 6;
    if(rf_corpse_surface_create(&pool,&source,1,"eye",5,.25f,&backend)!=RF_RANGE)return 7;
    if(rf_corpse_surface_create(&pool,&source,1,"eye",0,.25f,&backend)!=RF_RANGE)return 8;
    return 0;
}

static int corpse_surface_pool_probe(void)
{
    struct {uint32_t count;float dt;unsigned char payload[8][76];} input;
    while(fread(&input,sizeof(input),1,stdin)==1) {
        rf_corpse_surface_effect nodes[8];rf_corpse_surface_pool pool;uint32_t i,out[18];
        if(input.count>8)return 2;
        memset(nodes,0,sizeof(nodes));for(i=0;i<8;++i)memcpy(nodes+i,input.payload[i],76);
        if(rf_corpse_surface_reset(&pool,nodes))return 3;
        out[0]=(uint32_t)(pool.free-nodes)+1;out[1]=pool.active?99:0;
        for(i=0;i<8;++i){out[2+i*2]=(uint32_t)(nodes[i].next-nodes)+1;out[3+i*2]=(uint32_t)(nodes[i].previous-nodes)+1;}
        fwrite(out,sizeof(out),1,stdout);
        for(i=0;i<8;++i)fwrite(nodes+i,76,1,stdout);
        pool.active=input.count?nodes:NULL;
        for(i=0;i<input.count;++i){nodes[i].next=nodes+(i+1)%input.count;nodes[i].previous=nodes+(i+input.count-1)%input.count;}
        if(rf_corpse_surface_tick(&pool,input.dt))return 4;
        for(i=0;i<8;++i)fwrite(nodes+i,76,1,stdout);
    }
    return ferror(stdin)||ferror(stdout)?5:0;
}

static int corpse_surface_quad_probe(void)
{
    rf_corpse_surface_effect effect;rf_corpse_surface_quad quad;
    while(fread(&effect,sizeof(effect),1,stdin)==1) {
        if(rf_corpse_surface_build_quad(&effect,&quad))return 2;
        fwrite(&effect,sizeof(effect),1,stdout);fwrite(&quad,sizeof(quad),1,stdout);
    }
    return ferror(stdin)||ferror(stdout)?3:0;
}
