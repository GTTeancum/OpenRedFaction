#ifndef RF_SCENE_SCRIPT_EXPLODE_OPTIONAL_RESOURCES_INC
#define RF_SCENE_SCRIPT_EXPLODE_OPTIONAL_RESOURCES_INC
#ifdef RF_IMAGE_XBOX_NATIVE
extern uint32_t rf_xbox_available_memory_pages(void);
#endif
static int scene_script_explode_optional_memory(uint32_t bytes)
{
#ifdef RF_IMAGE_XBOX_NATIVE
    uint32_t pages=rf_xbox_available_memory_pages();
    rf_scene_script_explode_optional_stats[11]=pages;
    /* Same conservative16MiB reserve as the other optional scene packs.
     * Allocation can still fail because contiguous texture pages are needed. */
    if(pages<4096u+(bytes+4095u)/4096u)return 0;
#else
    (void)bytes;
#endif
    return 1;
}
static int scene_script_explode_image_equal(const char *a,const char *b)
{
    uint32_t i;for(i=0;i<64;i++){
        unsigned char x=(unsigned char)a[i],y=(unsigned char)b[i];
        if(x>='A'&&x<='Z')x+='a'-'A';if(y>='A'&&y<='Z')y+='a'-'A';
        if(x!=y)return 0;if(!x)return 1;
    }
    return 0;
}
static void scene_script_explode_recycled(void *context,uint32_t index)
{
    uint32_t clip,n,k;(void)context;
    /* Heads are pool0 particles. Invalidate before the free-list append;
     * another effect may allocate this exact index during the same tick. */
    if(index>=RF_PARTICLE_POOL0_CAPACITY)return;
    for(clip=0;clip<64;clip++)if(scene_script_explode_effects[clip]){
        scene_script_explode_optional *o=scene_script_explode_effects[clip]->optional;
        if(!o)continue;
        for(n=0;n<8;n++)for(k=0;k<o->instances[n].count;k++)
            if(o->instances[n].head[k]==index){o->instances[n].head[k]=UINT32_MAX;
                ++rf_scene_script_explode_optional_stats[8];}
    }
}
/* Append one optional, installed static image. Never replace/copy a central
 * image. Decoder pixel ownership transfers once, and failure stays local.
 * Page rounding is charged on Xbox in addition to the image descriptor. */
static int scene_script_explode_optional_image(scene_script_explode_effect_owner *owner,
    uint32_t slot,rf_vpp *maps,uint32_t map_count,uint32_t *charged,uint32_t *padding)
{
    rf_explosion_materials *m=&owner->materials;rf_image pixels={0};rf_vpp_entry entry;
    rf_particle_animation image={0};uint32_t i,archive,remaining,bytes,pad=0,limit=0;int status;
    unsigned char header[18];uint32_t width,height;
    *charged=*padding=0;
    for(i=0;i<slot;i++)if(m->slot_texture[i]<m->count &&
       scene_script_explode_image_equal(owner->definition.emitters[i].bitmap,
                                       owner->definition.emitters[slot].bitmap)){
        m->slot_texture[slot]=m->slot_texture[i];return RF_OK;
    }
    if(m->count>=9 || scene_script_explode_effect_bytes>=512u*1024u)return RF_RANGE;
    remaining=512u*1024u-scene_script_explode_effect_bytes;
    if(remaining<=4095u+sizeof(rf_image) ||
       !scene_script_explode_optional_memory(remaining))return RF_RANGE;
    /* Reserving a page-minus-one also bounds the transient decoded pixels,
     * even before their actual rounded charge is known. Leave32KiB for the
     * remaining authored channels when a256px head would consume the cap.
     * Stream its128px box-filtered fallback without a full-size allocation. */
    for(archive=0;archive<map_count;archive++){
        status=rf_vpp_find(maps+archive,owner->definition.emitters[slot].bitmap,&entry);
        if(status==RF_NOT_FOUND)continue;if(status)return status;break;}
    if(archive==map_count)return RF_NOT_FOUND;
    status=rf_vpp_read(maps+archive,&entry,0,header,sizeof(header));if(status)return status;
    width=header[12]|(uint32_t)header[13]<<8;height=header[14]|(uint32_t)header[15]<<8;
    if((width>128 || height>128) &&
       (uint64_t)width*height*4+sizeof(rf_image)+4095u+32u*1024u>remaining)limit=128;
    if(scene_script_explode_effect_bytes+remaining>rf_scene_script_explode_optional_stats[12])
        rf_scene_script_explode_optional_stats[12]=scene_script_explode_effect_bytes+remaining;
    status=limit?rf_image_tga_limited(&pixels,maps+archive,&entry,limit,remaining-4095u-(uint32_t)sizeof(rf_image)):
        rf_image_tga(&pixels,maps+archive,&entry,remaining-4095u-(uint32_t)sizeof(rf_image));
    if(status)return status;
#ifdef RF_IMAGE_XBOX_NATIVE
    pad=(4096u-(pixels.bytes&4095u))&4095u;
#endif
    bytes=(uint32_t)sizeof(rf_image)+pixels.bytes+pad;
    if(bytes>remaining){status=RF_RANGE;goto done;}
    image.images=calloc(1,sizeof(*image.images));if(!image.images){status=RF_IO;goto done;}
    image.count=1;image.archive_index=archive;
    image.resident_bytes=(uint32_t)sizeof(image)+sizeof(rf_image)+pixels.bytes;
    image.images[0]=pixels;memset(&pixels,0,sizeof(pixels));
    m->slot_texture[slot]=m->count;m->animations[m->count++]=image;
    m->resident_bytes+=bytes-pad;*charged=bytes;*padding=pad;status=RF_OK;
    rf_scene_script_explode_optional_stats[18]+=bytes-pad-(uint32_t)sizeof(rf_image);
    rf_scene_script_explode_optional_stats[19]+=(uint32_t)sizeof(rf_image);
    if(limit)++rf_scene_script_explode_optional_stats[15];
done:
    rf_image_close(&pixels);return status;
}
static void scene_script_explode_optional_open(scene_stream *s,rf_vpp *maps,uint32_t map_count)
{
    uint32_t clip,slot;rf_particle_pool *pool;
    memset(rf_scene_script_explode_optional_stats,0,sizeof(rf_scene_script_explode_optional_stats));
    rf_scene_script_explode_optional_stats[2]=(uint32_t)(scene_script_explode_effect_count*sizeof(void *)+
        sizeof(rf_particle_recycle_sink)+sizeof(void *)+sizeof(rf_scene_script_explode_optional_stats));
    rf_scene_script_explode_optional_stats[3]=rf_scene_script_explode_optional_stats[12]=scene_script_explode_effect_bytes;
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_script_explode_optional_stats[16]=rf_scene_script_explode_optional_stats[17]=rf_xbox_available_memory_pages();
#endif
    if(!s || !s->particles.state)return;pool=&s->particles.state->particles;
    /* Do not displace another observer. Only this scene owns this pool today. */
    if(pool->recycle_sink && (pool->recycle_sink!=scene_script_explode_recycled || pool->recycle_context!=pool)){
        ++rf_scene_script_explode_optional_stats[9];rf_scene_script_explode_optional_stats[10]=RF_RANGE;return;}
    for(clip=0;clip<64;clip++)if(scene_script_explode_effects[clip]){
        scene_script_explode_effect_owner *owner=scene_script_explode_effects[clip];
        scene_script_explode_optional *o;uint32_t mask=owner->definition.resolved&448u;
        if(owner->optional)continue;
        if(owner->definition.recipe.flags&1u)mask&=64u;
        if(!(mask&128u))mask&=~256u;
        if(owner->definition.recipe.sparks_count<=0)mask&=~64u;
        if(!mask)continue;
        if(scene_script_explode_effect_bytes+sizeof(*o)>512u*1024u ||
           !scene_script_explode_optional_memory((uint32_t)sizeof(*o))){
            ++rf_scene_script_explode_optional_stats[9];rf_scene_script_explode_optional_stats[10]=RF_RANGE;continue;}
        o=calloc(1,sizeof(*o));if(!o){++rf_scene_script_explode_optional_stats[9];
            rf_scene_script_explode_optional_stats[10]=RF_IO;continue;}
        scene_script_explode_effect_bytes+=(uint32_t)sizeof(*o);
        for(slot=6;slot<9;slot++)if(mask&(1u<<slot)){
            uint32_t bytes,padding;int status;
            if(slot==8 && !(o->mask&128u))continue;
            status=scene_script_explode_optional_image(owner,slot,maps,map_count,&bytes,&padding);
            if(status){++rf_scene_script_explode_optional_stats[9];rf_scene_script_explode_optional_stats[10]=(uint32_t)status;continue;}
            o->mask|=1u<<slot;scene_script_explode_effect_bytes+=bytes;
            rf_scene_script_explode_optional_stats[1]+=bytes;
            rf_scene_script_explode_optional_stats[14]+=padding;
        }
        if(o->mask){owner->optional=o;++rf_scene_script_explode_optional_stats[0];
            rf_scene_script_explode_optional_stats[1]+=(uint32_t)sizeof(*o);
            rf_scene_script_explode_optional_stats[2]+=(uint32_t)sizeof(*o);
        }else{free(o);scene_script_explode_effect_bytes-=(uint32_t)sizeof(*o);}
        rf_scene_script_explode_optional_stats[3]=scene_script_explode_effect_bytes;
        if(scene_script_explode_effect_bytes>rf_scene_script_explode_optional_stats[12])
            rf_scene_script_explode_optional_stats[12]=scene_script_explode_effect_bytes;
    }
    if(rf_scene_script_explode_optional_stats[0])
        rf_particle_pool_set_recycle_sink(pool,scene_script_explode_recycled,pool);
#ifdef RF_IMAGE_XBOX_NATIVE
    rf_scene_script_explode_optional_stats[17]=rf_xbox_available_memory_pages();
#endif
}
static void scene_script_explode_optional_unbind(scene_stream *s)
{
    if(s && s->particles.state){rf_particle_pool *pool=&s->particles.state->particles;
        if(pool->recycle_sink==scene_script_explode_recycled && pool->recycle_context==pool)
            rf_particle_pool_set_recycle_sink(pool,NULL,NULL);}
}
#endif
