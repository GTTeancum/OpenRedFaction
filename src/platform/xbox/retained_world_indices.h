/* Private retained-world submission. Original NV2A element methods reference
 * the already-uploaded vertices; no geometry, state or visibility is changed.
 * Included after retained_face_visible; no emulator implementation is copied. */
typedef struct retained_index_group {
    uint32_t first,count,material,lightmap;
} retained_index_group;
static struct {
    uint16_t *items;retained_index_group *groups;uint8_t *rooms;
    float position[3];
    uint32_t group_capacity,count,group_count,faces,ranges,words,valid;
} retained_indices;
/* active, charged bytes, visible indices, groups, packets, rebuilds, reuses,
 * fallback reason:0 none,1 index domain,2 budget,3 allocation,4 command bound,
 * 5 explicit array/grouping/batching fallback. */
uint32_t rf_xbox_world_indexed[8],rf_xbox_world_indexed_disabled;
/* Binding pairs written/reused: indexed-world units0/1, retained-model unit0.
 * Frame reset belongs to retained_models_begin, before either draw pass. */
uint32_t rf_xbox_texture_reuse[4];
static void retained_world_indices_close(void)
{
    free(retained_indices.items);free(retained_indices.groups);free(retained_indices.rooms);
    memset(&retained_indices,0,sizeof(retained_indices));
    memset(rf_xbox_world_indexed,0,sizeof(rf_xbox_world_indexed));
}
static void retained_world_indices_open(void)
{
    uint32_t i,groups=0,material=UINT32_MAX,lightmap=UINT32_MAX;uint64_t bytes;
    const rf_geometry *g=retained_world.source;
    if(!g || !retained_world.count || retained_world.count>65535u){rf_xbox_world_indexed[7]=1;return;}
    for(i=0;i<retained_world.face_count;i++) {
        const retained_face *face=retained_world.faces+i;
        if(!face->count)continue;
        if(!groups || face->material!=material || face->lightmap!=lightmap) {
            ++groups;material=face->material;lightmap=face->lightmap;
        }
    }
    bytes=(uint64_t)retained_world.count*sizeof(uint16_t)+(uint64_t)groups*sizeof(retained_index_group)+g->rooms+
        sizeof(retained_indices)+sizeof(rf_xbox_world_indexed);
    if(!groups || bytes+retained_world.bytes+(uint64_t)retained_world.face_count*sizeof(retained_face)>4u*1024u*1024u) {
        rf_xbox_world_indexed[7]=2;return;
    }
    retained_indices.items=malloc(retained_world.count*sizeof(uint16_t));
    retained_indices.groups=malloc(groups*sizeof(retained_index_group));
    retained_indices.rooms=g->rooms?malloc(g->rooms):NULL;
    if(!retained_indices.items || !retained_indices.groups || (g->rooms&&!retained_indices.rooms)) {
        retained_world_indices_close();rf_xbox_world_indexed[7]=3;return;
    }
    retained_indices.group_capacity=groups;rf_xbox_world_indexed[1]=(uint32_t)bytes;
}
static uint8_t retained_index_room_visible(uint32_t room)
{
    const rf_visibility *v=retained_world.visibility;
    return !v || room>=v->count || v->rooms[room].visible!=0;
}
static int retained_world_indices_prepare(void)
{
    const rf_geometry *g=retained_world.source;uint32_t i,j,unchanged;
    rf_xbox_world_indexed[0]=rf_xbox_world_indexed[2]=rf_xbox_world_indexed[3]=rf_xbox_world_indexed[4]=0;
    if(!retained_indices.items)return RF_NOT_FOUND;
    if(rf_xbox_world_indexed_disabled || rf_xbox_world_grouping_disabled || rf_xbox_command_batching_disabled) {
        rf_xbox_world_indexed[7]=5;return RF_NOT_FOUND;
    }
    unchanged=retained_indices.valid && !memcmp(retained_indices.position,retained_world.position,12);
    for(i=0;unchanged && i<g->rooms;i++)if(retained_indices.rooms[i]!=retained_index_room_visible(i))unchanged=0;
    if(!unchanged) {
        uint32_t prior=UINT32_MAX,end=0;
        retained_indices.valid=retained_indices.count=retained_indices.group_count=retained_indices.faces=retained_indices.ranges=0;
        for(i=0;i<retained_world.face_count;i++) {
            const retained_face *face=retained_world.faces+i;retained_index_group *group;
            if(!retained_face_visible(face))continue;
            if(face->start>retained_world.count || face->count>retained_world.count-face->start ||
               face->count%3 || face->count>retained_world.count-retained_indices.count)return RF_FORMAT;
            group=retained_indices.group_count?retained_indices.groups+retained_indices.group_count-1:NULL;
            if(!group || group->material!=face->material || group->lightmap!=face->lightmap ||
               prior==UINT32_MAX || prior+1!=i || end!=face->start)++retained_indices.ranges;
            prior=i;end=face->start+face->count;
            if(!group || group->material!=face->material || group->lightmap!=face->lightmap) {
                if(retained_indices.group_count==retained_indices.group_capacity)return RF_FORMAT;
                group=retained_indices.groups+retained_indices.group_count++;
                *group=(retained_index_group){retained_indices.count,0,face->material,face->lightmap};
            }
            /* Keep the same sorted face/fan order, crossing only invisible
             * gaps. Each index names one of the unchanged uploaded vertices. */
            for(j=0;j<face->count;j++)retained_indices.items[retained_indices.count++]=(uint16_t)(face->start+j);
            group->count+=face->count;++retained_indices.faces;
        }
        /* Bound this world's full index command stream to128KiB, leaving
         * room for the unchanged model/CPU passes. Larger views use arrays.
         * The128 dword pb_begin/end contract is checked by renderer_reserve. */
        retained_indices.words=1024; /* Common shader/state setup, conservatively bounded. */
        for(i=0;i<retained_indices.group_count;i++) {
            const retained_index_group *group=retained_indices.groups+i;uint32_t pairs=group->count/2;
            retained_indices.words+=19+pairs+(pairs+119)/120+(group->count&1u)*2;
        }
        memcpy(retained_indices.position,retained_world.position,12);
        for(i=0;i<g->rooms;i++)retained_indices.rooms[i]=retained_index_room_visible(i);
        retained_indices.valid=1;++rf_xbox_world_indexed[5];
    } else ++rf_xbox_world_indexed[6];
    if(retained_indices.words>32768u){rf_xbox_world_indexed[7]=4;return RF_NOT_FOUND;}
    rf_xbox_world_indexed[0]=1;rf_xbox_world_indexed[2]=retained_indices.count;
    rf_xbox_world_indexed[3]=retained_indices.group_count;rf_xbox_world_indexed[7]=0;
    return RF_OK;
}
static int retained_world_indices_draw(const rf_materials *materials,const rf_lightmaps *lightmaps,
    const gpu_texture *textures)
{
    renderer_command_batch commands={NULL,NULL,rf_xbox_command_blocks};uint32_t i;
    uint32_t bound_address[2]={0,0},bound_format[2]={0,0},bound=0;
    for(i=0;i<retained_indices.group_count;i++) {
        const retained_index_group *group=retained_indices.groups+i;uint32_t at=group->first,left=group->count,*p,textured,address[2];
        const gpu_texture *texture,*lighting;
        if(group->lightmap!=UINT32_MAX && group->lightmap>=lightmaps->count){renderer_flush(&commands);return RF_FORMAT;}
        textured=group->material<materials->count && textures[group->material].pixels;
        texture=textured?textures+group->material:textures+materials->count;
        lighting=group->lightmap<lightmaps->count?textures+materials->count+1+group->lightmap:textures+materials->count;
        address[0]=(uint32_t)texture->pixels&0x03ffffff;
        address[1]=(uint32_t)lighting->pixels&0x03ffffff;
        p=renderer_reserve(&commands,17);
        /* State is private to this uninterrupted pass. Compare the actual
         * physical address/format written to NV2A, not material identity. */
        if(!bound || bound_address[0]!=address[0] || bound_format[0]!=texture->format) {
            p=pb_push1(p,NV097_SET_TEXTURE_OFFSET,address[0]);
            p=pb_push1(p,NV097_SET_TEXTURE_FORMAT,texture->format);++rf_xbox_texture_reuse[0];
        } else ++rf_xbox_texture_reuse[1];
        if(!bound || bound_address[1]!=address[1] || bound_format[1]!=lighting->format) {
            p=pb_push1(p,NV097_SET_TEXTURE_OFFSET+0x40,address[1]);
            p=pb_push1(p,NV097_SET_TEXTURE_FORMAT+0x40,lighting->format);++rf_xbox_texture_reuse[0];
        } else ++rf_xbox_texture_reuse[1];
        bound_address[0]=address[0];bound_address[1]=address[1];
        bound_format[0]=texture->format;bound_format[1]=lighting->format;bound=1;
        p=pb_push1(p,NV097_SET_TRANSFORM_CONSTANT_LOAD,99);
        p=pb_push4f(p,NV097_SET_TRANSFORM_CONSTANT,textured?0:1,textured?1:0,group->lightmap==UINT32_MAX?.5f:1,.1f);
        p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_TRIANGLES);commands.p=p;
        ++rf_xbox_command_blocks[1];
        while(left>=2) {
            uint32_t pairs=left/2,j;if(pairs>120)pairs=120;
            p=renderer_reserve(&commands,1+pairs);
            pb_push(p++,0x40000000u|NV097_ARRAY_ELEMENT16,pairs);
            for(j=0;j<pairs;j++,at+=2)*p++=(uint32_t)retained_indices.items[at]|((uint32_t)retained_indices.items[at+1]<<16);
            commands.p=p;left-=pairs*2;++rf_xbox_world_indexed[4];++rf_xbox_command_blocks[1];
        }
        if(left) {
            p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_ARRAY_ELEMENT32,retained_indices.items[at]);
            ++rf_xbox_world_indexed[4];++rf_xbox_command_blocks[1];
        }
        p=renderer_reserve(&commands,2);commands.p=pb_push1(p,NV097_SET_BEGIN_END,NV097_SET_BEGIN_END_OP_END);
        ++rf_xbox_command_blocks[1];
    }
    renderer_flush(&commands);
    rf_xbox_world_groups[0]=retained_indices.group_count;rf_xbox_world_groups[1]=retained_indices.ranges;
    rf_xbox_retained_world[4]=retained_indices.count;rf_xbox_retained_world[5]=retained_indices.faces;
    rf_xbox_retained_world[6]=retained_indices.group_count;return RF_OK;
}
