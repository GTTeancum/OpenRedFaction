#include "rf/authored_identity_capture.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
enum {CAPTURE_STACK_RESERVE=8192};
typedef struct identity_capture {
    const rf_geometry *geometry;const rf_lightmap_rgb_owner *rgb;rf_vpp *maps;uint32_t map_count;
    rf_geomod_authored_identity_input input;
    rf_geomod_identity_material *materials;rf_geomod_identity_reference *references;
    rf_geomod_digest_material *manifest_materials;rf_geomod_authored_chart_identity *manifest_references;
    unsigned char **chart_pixels,*editor;
    uint32_t material_capacity,reference_capacity,material_count,reference_count;
    uint32_t used,peak,budget,temporary_bytes;
    rf_image temporary;rf_geomod_identity_image substrate_pixels;rf_geomod_digest_material manifest_substrate;
} identity_capture;
static uint32_t word(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void canonical(char *s)
{for(;*s;s++){if(*s>='A'&&*s<='Z')*s=(char)(*s+'a'-'A');if(*s=='\\')*s='/';}}
static int allocate(identity_capture *c,uint32_t bytes,void **out)
{
    void *p;if(!bytes || bytes>c->budget-c->used)return RF_RANGE;
    p=calloc(1,bytes);if(!p)return RF_IO;c->used+=bytes;if(c->used>c->peak)c->peak=c->used;*out=p;return RF_OK;
}
static void close_temporary(identity_capture *c)
{rf_image_close(&c->temporary);c->used-=c->temporary_bytes;c->temporary_bytes=0;}
static void release(identity_capture *c)
{
    uint32_t i;if(!c)return;rf_image_close(&c->temporary);
    if(c->materials)for(i=0;i<c->material_capacity;i++)free((void *)c->materials[i].image.pixels);
    if(c->chart_pixels)for(i=0;i<c->rgb->count;i++)free(c->chart_pixels[i]);
    free(c->materials);free(c->references);free(c->chart_pixels);free(c->editor);
    free(c->manifest_materials);free(c->manifest_references);free((void *)c->substrate_pixels.pixels);free(c);
}
static int resource_capture(identity_capture *c,const char *name,rf_geomod_identity_image *output)
{
    uint32_t i,found=0,index=0,bpp,x,y,remaining;uint64_t bytes,rounded;int status;
    rf_vpp_entry selected={0},entry;void *memory;
    for(i=0;i<c->map_count;i++) {
        status=rf_vpp_find(c->maps+i,name,&entry);if(status==RF_NOT_FOUND)continue;if(status)return status;
        selected=entry;index=i;found++;
    }
    if(!found)return RF_NOT_FOUND;if(found!=1)return RF_FORMAT;
    /* Account the largest possible page-rounding increment before decoding,
     * so an accepted allocation cannot temporarily exceed the caller's cap. */
    remaining=c->budget-c->used;if(remaining<=4095)return RF_RANGE;
    status=rf_image_open(&c->temporary,c->maps+index,&selected,remaining-4095);if(status)return status;
    rounded=((uint64_t)c->temporary.bytes+4095u)&~(uint64_t)4095u;
    if(rounded>remaining)return RF_RANGE;
    c->temporary_bytes=(uint32_t)rounded;c->used+=c->temporary_bytes;if(c->used>c->peak)c->peak=c->used;
    bpp=rf_image_is_packed_1555(&c->temporary)?2:4;bytes=(uint64_t)c->temporary.width*c->temporary.height*bpp;
    if(!c->temporary.rgba || !bytes || bytes>UINT32_MAX || bytes!=c->temporary.bytes)return RF_FORMAT;
    if(!memchr(selected.name,0,sizeof(selected.name)) || strlen(selected.name)>=sizeof(output->name))return RF_FORMAT;
    strcpy(output->name,selected.name);canonical(output->name);
    output->width=c->temporary.width;output->height=c->temporary.height;output->format=c->temporary.source_format;
    output->bytes_per_pixel=bpp;output->bytes=(uint32_t)bytes;
    status=allocate(c,(uint32_t)bytes,&memory);if(status)return status;output->pixels=memory;
    for(y=0;y<c->temporary.height;y++)for(x=0;x<c->temporary.width;x++)
        memcpy((unsigned char *)memory+(y*c->temporary.width+x)*bpp,rf_image_pixel(&c->temporary,x,y),bpp);
    close_temporary(c);return RF_OK;
}
static int material_capture(identity_capture *c,uint32_t key)
{
    uint32_t i;char name[64];int status;rf_geomod_identity_material *m;
    for(i=0;i<c->material_count;i++)if(c->materials[i].compiled_material==key)return RF_OK;
    if(c->material_count>=c->material_capacity)return RF_RANGE;
    status=rf_geometry_texture_name(c->geometry,key,name,sizeof(name));if(status)return status;
    m=c->materials+c->material_count;m->compiled_material=key;
    status=resource_capture(c,name,&m->image);if(status)return status;c->material_count++;return RF_OK;
}
static int reference_capture(identity_capture *c,const rf_geomod_publication_origin *o,uint32_t material)
{
    rf_geomod_identity_reference *r;rf_geometry_face face;uint64_t offset;uint32_t i,index;int status;
    if(o->reference==UINT32_MAX)return RF_OK;
    for(i=0;i<c->reference_count;i++)if(c->references[i].reference==o->reference) {
        r=c->references+i;return r->owner==o->owner && r->source_face==o->source_face && r->compiled_material==material?RF_OK:RF_FORMAT;
    }
    if(c->reference_count>=c->reference_capacity || o->reference>=c->geometry->faces)return RF_RANGE;
    status=rf_geometry_get_face(c->geometry,o->reference,&face);if(status)return status;
    if(face.texture!=material)return RF_FORMAT;
    offset=c->geometry->face_offsets[o->reference];if(offset+28>c->geometry->bytes)return RF_RANGE;
    if(word(c->geometry->data+(uint32_t)offset+24)!=o->source_face)return RF_FORMAT;
    r=c->references+c->reference_count;r->reference=o->reference;r->owner=o->owner;
    r->source_face=o->source_face;r->compiled_material=material;
    status=rf_geometry_initial_collision_filter(c->geometry,o->reference,0,&r->filter);if(status)return status;
    r->unlit=face.lightmap_mapping==UINT32_MAX;
    if(!r->unlit) {
        rf_lightmap_mapping mapping;const rf_lightmap_rgb_image *rgb;uint32_t pixel;uint64_t bytes;void *memory;int length;
        if(face.lightmap_mapping>=c->geometry->mappings)return RF_RANGE;
        offset=(uint64_t)c->geometry->mapping_offset+(uint64_t)face.lightmap_mapping*96;
        if(offset+96>c->geometry->bytes)return RF_RANGE;index=word(c->geometry->data+(uint32_t)offset);
        /* Reject missing raw ownership before the general reader's modeled
         * original image0 fallback can conceal it. */
        if(index>=c->rgb->count || index>INT32_MAX)return RF_FORMAT;
        status=rf_geometry_get_lightmap_mapping(c->geometry,face.lightmap_mapping,c->rgb->count,&mapping);if(status)return status;
        if(mapping.image!=index)return RF_FORMAT;
        status=rf_geometry_lightmap_projection(c->geometry,face.lightmap_mapping,&r->projection);if(status)return status;
        rgb=c->rgb->images+index;
        if(!rgb->width || !rgb->height || rgb->width>4096 || rgb->height>4096 || !rgb->pixels ||
            (uint64_t)rgb->width*rgb->height*3!=rgb->bytes)return RF_FORMAT;
        bytes=(uint64_t)rgb->width*rgb->height*4;if(bytes>UINT32_MAX)return RF_RANGE;
        if(!c->chart_pixels[index]) {
            status=allocate(c,(uint32_t)bytes,&memory);if(status)return status;c->chart_pixels[index]=memory;
            for(pixel=0;pixel<rgb->width*rgb->height;pixel++) {
                memcpy(c->chart_pixels[index]+pixel*4,rgb->pixels+pixel*3,3);c->chart_pixels[index][pixel*4+3]=255;
            }
        }
        length=snprintf(r->chart.name,sizeof(r->chart.name),"ctf06.rfl/chart/%u/%u",o->owner,o->source_face);
        if(length<=0 || (uint32_t)length>=sizeof(r->chart.name))return RF_RANGE;
        r->chart.width=rgb->width;r->chart.height=rgb->height;r->chart.format=6;
        r->chart.bytes_per_pixel=4;r->chart.bytes=(uint32_t)bytes;r->chart.pixels=c->chart_pixels[index];
    }
    c->reference_count++;return RF_OK;
}
static int mesh_capture(identity_capture *c,const rf_geomod_mesh_view *mesh,const rf_geomod_publication_origin *origins)
{
    uint32_t i;int status;
    for(i=0;i<mesh->face_count;i++) {
        status=material_capture(c,mesh->faces[i].material);if(status)return status;
        status=reference_capture(c,origins+i,mesh->faces[i].material);if(status)return status;
    }
    return RF_OK;
}
static int manifest_prepare(identity_capture *c,const rf_geomod_authored_identity_manifest *manifest)
{
    uint32_t i;void *memory;int status;
    if(c->material_count>manifest->material_capacity || c->reference_count>manifest->reference_capacity)return RF_RANGE;
    status=allocate(c,c->material_count*sizeof(*c->manifest_materials),&memory);if(status)return status;c->manifest_materials=memory;
    status=allocate(c,c->reference_count*sizeof(*c->manifest_references),&memory);if(status)return status;c->manifest_references=memory;
    for(i=0;i<c->material_count;i++) {
        const rf_geomod_identity_material *source=c->materials+i;rf_geomod_digest_material *m=c->manifest_materials+i;
        m->key=source->compiled_material;m->image=source->image;
        status=rf_geomod_image_content_digest(&m->image,m->content_digest);if(status)return status;
        m->image.pixels=NULL;m->prehashed=1;
    }
    for(i=0;i<c->reference_count;i++) {
        const rf_geomod_identity_reference *source=c->references+i;rf_geomod_authored_chart_identity *r=c->manifest_references+i;
        r->reference=source->reference;r->compiled_material=source->compiled_material;r->filter=source->filter;
        r->chart.key=source->reference;r->chart.owner=source->owner;r->chart.source_face=source->source_face;
        r->chart.retained_map=UINT32_MAX;r->chart.projection=source->projection;
        if(source->unlit)r->chart.kind=RF_GEOMOD_DIGEST_UNLIT;
        else {
            r->chart.kind=RF_GEOMOD_DIGEST_SOURCE_CHART;r->chart.image=source->chart;
            status=rf_geomod_image_content_digest(&r->chart.image,r->chart.content_digest);if(status)return status;
            r->chart.image.pixels=NULL;r->chart.prehashed=1;
        }
    }
    if(manifest->substrate) {
        status=resource_capture(c,c->input.asset->settings.texture,&c->substrate_pixels);if(status)return status;
        c->manifest_substrate.image=c->substrate_pixels;
        status=rf_geomod_image_content_digest(&c->substrate_pixels,c->manifest_substrate.content_digest);if(status)return status;
        c->manifest_substrate.image.pixels=NULL;c->manifest_substrate.prehashed=1;
    }
    return RF_OK;
}
int rf_geomod_authored_identity_capture_manifest(const rf_level *level,const rf_geometry *geometry,
    const rf_geomod_authored_post_view *asset,rf_vpp *maps,uint32_t map_count,const rf_lightmap_rgb_owner *rgb,
    uint32_t budget,unsigned char digest[32],uint32_t *peak_bytes,rf_geomod_authored_identity_manifest *manifest)
{
    identity_capture *c=NULL;const rf_level_section *editor;unsigned char result[32];uint32_t capacity,peak;
    const rf_geomod_mesh_view *meshes[3];const rf_geomod_publication_origin *origins[3];uint32_t i,manifest_bytes=0;void *memory;int status;
    if(!level || !geometry || !asset || !maps || !map_count || map_count>32 || !rgb || !rgb->images ||
        !rgb->count || rgb->count>4096 || !digest || !peak_bytes || !geometry->data || !geometry->face_offsets)return RF_RANGE;
    if(manifest) {
        uint64_t bytes;
        if(!manifest->materials || !manifest->references || !manifest->material_capacity || manifest->material_capacity>128 ||
            !manifest->reference_capacity || manifest->reference_capacity>768)return RF_RANGE;
        bytes=sizeof(*manifest)+(uint64_t)manifest->material_capacity*sizeof(*manifest->materials)+
            (uint64_t)manifest->reference_capacity*sizeof(*manifest->references)+(manifest->substrate?sizeof(*manifest->substrate):0);
        if(bytes>UINT32_MAX)return RF_RANGE;manifest_bytes=(uint32_t)bytes;
    }
    if(level->version!=180 || strcmp(level->entry.name,"ctf06.rfl") || (asset->source_uid!=93 && asset->source_uid!=94 && asset->source_uid!=95 && asset->source_uid!=96 && asset->source_uid!=97) || asset->room!=3 ||
        asset->source.face_count!=6 || asset->solid_count!=3)return RF_NOT_FOUND;
    if(asset->source_uid==95 && (asset->neighbor_void_count!=1 || !asset->neighbor_voids || asset->neighbor_voids[0].owner!=80))return RF_NOT_FOUND;
    meshes[0]=&asset->source;meshes[1]=&asset->windows;meshes[2]=&asset->neighbors;
    origins[0]=asset->source_origins;origins[1]=asset->window_origins;origins[2]=asset->neighbor_origins;capacity=0;
    for(i=0;i<3;i++) {
        if(!meshes[i]->faces || !meshes[i]->vertices || !origins[i] || !meshes[i]->face_count || meshes[i]->face_count>768 ||
            !meshes[i]->vertex_count || meshes[i]->vertex_count>4096)return RF_RANGE;
        capacity+=meshes[i]->face_count;
    }
    if(capacity>768 || (uint64_t)budget<sizeof(*c)+CAPTURE_STACK_RESERVE+(uint64_t)manifest_bytes)return RF_RANGE;
    editor=rf_level_find(level,0x2000000);if(!editor || !editor->size)return RF_NOT_FOUND;
    c=calloc(1,sizeof(*c));if(!c)return RF_IO;
    c->geometry=geometry;c->rgb=rgb;c->maps=maps;c->map_count=map_count;c->budget=budget;
    c->used=c->peak=(uint32_t)sizeof(*c)+CAPTURE_STACK_RESERVE+manifest_bytes;c->reference_capacity=capacity;
    c->material_capacity=capacity<128?capacity:128;
    status=allocate(c,c->material_capacity*sizeof(*c->materials),&memory);if(status)goto done;c->materials=memory;
    status=allocate(c,capacity*sizeof(*c->references),&memory);if(status)goto done;c->references=memory;
    status=allocate(c,rgb->count*sizeof(*c->chart_pixels),&memory);if(status)goto done;c->chart_pixels=memory;
    status=allocate(c,editor->size,&memory);if(status)goto done;c->editor=memory;
    status=rf_level_read(level,editor,0,c->editor,editor->size);if(status)goto done;
    for(i=0;i<3;i++){status=mesh_capture(c,meshes[i],origins[i]);if(status)goto done;}
    c->input.asset=asset;strcpy(c->input.level,"ctf06.rfl");c->input.compiled_section=geometry->data;
    c->input.compiled_bytes=geometry->bytes;c->input.editor_section=c->editor;c->input.editor_bytes=editor->size;
    /* Immutable view comes from the bounded loader, whose selected-source flags0 guard
     * proves original44d870 operation2. No trailing property inference. */
    c->input.source_operation=2;c->input.loader_policy=c->input.collision_policy=c->input.material_policy=1;
    /* Existing posts retain reconstruction policy9 and RFAS identity v1.
     * Beam policy11 includes hollow-volume neighbors, inherited collision filters
     * and authored-material cap charts; loader3 fixes this live profile. */
    c->input.publication_policy=asset->source_uid==95?11:9;
    if(asset->source_uid==95)c->input.loader_policy=3;
    c->input.material_domain=RF_GEOMOD_IDENTITY_COMPILED_MATERIALS;
    c->input.materials=c->materials;c->input.material_count=c->material_count;
    c->input.references=c->references;c->input.reference_count=c->reference_count;
    status=rf_geomod_authored_identity(&c->input,result);if(status)goto done;
    if(manifest){status=manifest_prepare(c,manifest);if(status)goto done;}
    if(manifest) {
        memcpy(manifest->materials,c->manifest_materials,c->material_count*sizeof(*manifest->materials));
        memcpy(manifest->references,c->manifest_references,c->reference_count*sizeof(*manifest->references));
        manifest->material_count=c->material_count;manifest->reference_count=c->reference_count;manifest->resident_bytes=manifest_bytes;
        if(manifest->substrate)*manifest->substrate=c->manifest_substrate;
    }
    peak=c->peak;memcpy(digest,result,32);*peak_bytes=peak;
done:
    release(c);return status;
}
int rf_geomod_authored_identity_capture(const rf_level *level,const rf_geometry *geometry,
    const rf_geomod_authored_post_view *asset,rf_vpp *maps,uint32_t map_count,const rf_lightmap_rgb_owner *rgb,
    uint32_t budget,unsigned char digest[32],uint32_t *peak_bytes)
{return rf_geomod_authored_identity_capture_manifest(level,geometry,asset,maps,map_count,rgb,budget,digest,peak_bytes,NULL);}
