/* Standalone immutable ctf06 identity capture. argv: Installed_Game directory.
 * No scene, game, renderer, replacement texture, input or save publication. */
#include "rf/geomod_authored_identity.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static uint32_t word(const unsigned char *p)
{return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static void canonical(char *s)
{for(;*s;s++){if(*s>='A'&&*s<='Z')*s=(char)(*s+'a'-'A');if(*s=='\\')*s='/';}}
typedef struct capture {
    rf_vpp levels,maps[5];rf_level level;rf_geometry geometry;
    rf_geomod_authored_post *owner;rf_geomod_authored_post_view asset;
    rf_lightmap_rgb_owner rgb;unsigned char **chart_pixels;
    rf_geomod_identity_material materials[128];uint32_t material_count;
    rf_geomod_identity_reference references[768];uint32_t reference_count;
    unsigned char *editor;uint32_t editor_bytes;
} capture;
static int capture_material(capture *c,uint32_t key)
{
    static const char *archives[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    uint32_t i,found=0,archive_index=0,x,y,bpp;rf_vpp_entry selected={0},entry;
    rf_image image={0};rf_geomod_identity_material *m;char name[64];int status;
    for(i=0;i<c->material_count;i++)if(c->materials[i].compiled_material==key)return 0;
    CHECK(c->material_count<128);CHECK(!rf_geometry_texture_name(&c->geometry,key,name,sizeof(name)));
    for(i=0;i<5;i++) {
        status=rf_vpp_find(c->maps+i,name,&entry);
        if(status==RF_NOT_FOUND)continue;
        CHECK(!status);selected=entry;archive_index=i;found++;
    }
    if(found!=1) {
        fprintf(stderr,"ASSET_OWNERSHIP material%u name%s matches%u (requires exact unique member; no fallback)\n",key,name,found);
        return 1;
    }
    CHECK(!rf_image_open(&image,c->maps+archive_index,&selected,16u*1024u*1024u));
    bpp=rf_image_is_packed_1555(&image)?2:4;
    m=c->materials+c->material_count;m->compiled_material=key;
    CHECK(strlen(selected.name)<sizeof(m->image.name));strcpy(m->image.name,selected.name);canonical(m->image.name);
    m->image.width=image.width;m->image.height=image.height;m->image.format=image.source_format;
    m->image.bytes_per_pixel=bpp;m->image.bytes=image.width*image.height*bpp;
    m->image.pixels=malloc(m->image.bytes);CHECK(m->image.pixels);
    for(y=0;y<image.height;y++)for(x=0;x<image.width;x++)
        memcpy((unsigned char *)m->image.pixels+(y*image.width+x)*bpp,rf_image_pixel(&image,x,y),bpp);
    printf("MATERIAL key%u name%s archive%s dimensions%ux%u format%u logical_bytes%u\n",key,m->image.name,
        archives[archive_index],image.width,image.height,image.source_format,m->image.bytes);
    rf_image_close(&image);c->material_count++;return 0;
}
static int capture_reference(capture *c,const rf_geomod_publication_origin *o,uint32_t material)
{
    rf_geomod_identity_reference *r;rf_geometry_face face;uint32_t i,index;uint64_t offset;
    if(o->reference==UINT32_MAX) {
        printf("HIDDEN_SOURCE kind%u owner%u source%u reference_absent1\n",o->kind,o->owner,o->source_face);return 0;
    }
    for(i=0;i<c->reference_count;i++)if(c->references[i].reference==o->reference) {
        r=c->references+i;CHECK(r->owner==o->owner && r->source_face==o->source_face && r->compiled_material==material);return 0;
    }
    CHECK(c->reference_count<768);CHECK(o->reference<c->geometry.faces);
    CHECK(!rf_geometry_get_face(&c->geometry,o->reference,&face));CHECK(face.texture==material);
    offset=c->geometry.face_offsets[o->reference];CHECK(offset+28<=c->geometry.bytes);
    CHECK(word(c->geometry.data+(uint32_t)offset+24)==o->source_face);
    r=c->references+c->reference_count;r->reference=o->reference;r->owner=o->owner;
    r->source_face=o->source_face;r->compiled_material=material;
    CHECK(!rf_geometry_initial_collision_filter(&c->geometry,o->reference,0,&r->filter));
    r->unlit=face.lightmap_mapping==UINT32_MAX;
    if(!r->unlit) {
        rf_lightmap_mapping mapping;const rf_lightmap_rgb_image *rgb;uint32_t pixel;
        CHECK(face.lightmap_mapping<c->geometry.mappings);
        offset=(uint64_t)c->geometry.mapping_offset+(uint64_t)face.lightmap_mapping*96;
        CHECK(offset+96<=c->geometry.bytes);index=word(c->geometry.data+(uint32_t)offset);
        /* The general reader models original fallback-to-image0. This capture
         * must reject invalid original ownership before invoking that reader. */
        if(index>=c->rgb.count || index>INT32_MAX) {
            fprintf(stderr,"MISSING_CHART reference%u mapping%u raw_image%u count%u\n",o->reference,face.lightmap_mapping,index,c->rgb.count);return 1;
        }
        CHECK(!rf_geometry_get_lightmap_mapping(&c->geometry,face.lightmap_mapping,c->rgb.count,&mapping));
        CHECK(mapping.image==index);CHECK(!rf_geometry_lightmap_projection(&c->geometry,face.lightmap_mapping,&r->projection));
        rgb=c->rgb.images+index;
        CHECK(rgb->bytes==(uint64_t)rgb->width*rgb->height*3 && rgb->pixels);
        if(!c->chart_pixels[index]) {
            c->chart_pixels[index]=malloc((size_t)rgb->width*rgb->height*4);CHECK(c->chart_pixels[index]);
            for(pixel=0;pixel<rgb->width*rgb->height;pixel++) {
                memcpy(c->chart_pixels[index]+pixel*4,rgb->pixels+pixel*3,3);c->chart_pixels[index][pixel*4+3]=255;
            }
        }
        /* Stable authored chart identity. Full immutable image content plus
         * projection is hashed; numeric runtime image/mapping handles are not. */
        CHECK(snprintf(r->chart.name,sizeof(r->chart.name),"ctf06.rfl/chart/%u/%u",o->owner,o->source_face)>0);
        r->chart.width=rgb->width;r->chart.height=rgb->height;r->chart.format=6;
        r->chart.bytes_per_pixel=4;r->chart.bytes=rgb->width*rgb->height*4;r->chart.pixels=c->chart_pixels[index];
        printf("REFERENCE id%u owner%u source%u material%u mapping%u image%u chart%s pixels%u\n",
            r->reference,r->owner,r->source_face,material,face.lightmap_mapping,index,r->chart.name,r->chart.bytes);
    } else printf("REFERENCE id%u owner%u source%u material%u explicit_unlit1\n",r->reference,r->owner,r->source_face,material);
    c->reference_count++;return 0;
}
static int capture_mesh(capture *c,const rf_geomod_mesh_view *m,const rf_geomod_publication_origin *o)
{uint32_t i;for(i=0;i<m->face_count;i++){CHECK(!capture_material(c,m->faces[i].material));CHECK(!capture_reference(c,o+i,m->faces[i].material));}return 0;}
int main(int argc,char **argv)
{
    static const char *archives[5]={"maps1.vpp","maps2.vpp","maps3.vpp","maps4.vpp","maps_en.vpp"};
    capture *c;char path[1024];uint32_t i,total=0;const rf_level_section *editor;
    rf_geomod_authored_identity_input input={0};unsigned char digest[32],again[32],sentinel[32];
    CHECK(argc==2);c=calloc(1,sizeof(*c));CHECK(c);
    CHECK(snprintf(path,sizeof(path),"%s/levelsm.vpp",argv[1])>0);CHECK(!rf_vpp_open(&c->levels,path));
    CHECK(!rf_level_open(&c->level,&c->levels,"ctf06.rfl"));CHECK(!rf_geometry_open(&c->geometry,&c->level,8*1024*1024));
    CHECK(!rf_geomod_authored_post_open(&c->level,&c->geometry,2*1024*1024,&c->owner));
    CHECK(!rf_geomod_authored_post_get(c->owner,&c->asset));CHECK(c->asset.source_uid==94);
    editor=rf_level_find(&c->level,0x2000000);CHECK(editor && editor->size && editor->size<=8*1024*1024);
    c->editor_bytes=editor->size;c->editor=malloc(editor->size);CHECK(c->editor);
    CHECK(!rf_level_read(&c->level,editor,0,c->editor,editor->size));
    CHECK(!rf_lightmap_rgb_open(&c->rgb,&c->level,16*1024*1024));
    c->chart_pixels=calloc(c->rgb.count,sizeof(*c->chart_pixels));CHECK(c->chart_pixels);
    for(i=0;i<5;i++) {CHECK(snprintf(path,sizeof(path),"%s/%s",argv[1],archives[i])>0);CHECK(!rf_vpp_open(c->maps+i,path));}
    CHECK(!capture_mesh(c,&c->asset.source,c->asset.source_origins));
    CHECK(!capture_mesh(c,&c->asset.windows,c->asset.window_origins));
    CHECK(!capture_mesh(c,&c->asset.neighbors,c->asset.neighbor_origins));
    input.asset=&c->asset;strcpy(input.level,"ctf06.rfl");input.compiled_section=c->geometry.data;
    input.compiled_bytes=c->geometry.bytes;input.editor_section=c->editor;input.editor_bytes=c->editor_bytes;
    /* Successful bounded loader requires UID94 flags0 at its line411; original
     *44d870 flags0 decodes operation2. Not inferred from a trailing property. */
    input.source_operation=2;input.source_mode=0;
    input.loader_policy=input.publication_policy=input.collision_policy=input.material_policy=1;
    input.material_domain=RF_GEOMOD_IDENTITY_COMPILED_MATERIALS;
    input.materials=c->materials;input.material_count=c->material_count;
    input.references=c->references;input.reference_count=c->reference_count;
    CHECK(!rf_geomod_authored_identity(&input,digest));
    printf("INSTALLED_SOURCE_SHA256 ");for(i=0;i<32;i++)printf("%02x",digest[i]);puts("");
    CHECK(!rf_geomod_authored_identity(&input,again) && !memcmp(digest,again,32));
    /* Actual data failure probes preserve output; no row is substituted. */
    memset(sentinel,0xa5,32);memcpy(again,sentinel,32);input.reference_count=0;
    CHECK(rf_geomod_authored_identity(&input,again)!=RF_OK && !memcmp(again,sentinel,32));input.reference_count=c->reference_count;
    CHECK(c->reference_count>1);{uint32_t old=c->references[1].reference;c->references[1].reference=c->references[0].reference;
        CHECK(rf_geomod_authored_identity(&input,again)!=RF_OK && !memcmp(again,sentinel,32));c->references[1].reference=old;}
    {unsigned char *pixel=(unsigned char *)c->materials[0].image.pixels;pixel[0]^=1;
     CHECK(!rf_geomod_authored_identity(&input,again) && memcmp(digest,again,32));pixel[0]^=1;}
    for(i=0;i<c->material_count;i++)total+=c->materials[i].image.bytes;
    printf("PASS installed_authored_identity materials%u references%u compiled_bytes%u editor_bytes%u logical_texture_bytes%u deterministic1 missing_duplicate_reject1 pixels_sensitive1\n",
        c->material_count,c->reference_count,input.compiled_bytes,input.editor_bytes,total);
    for(i=0;i<c->material_count;i++)free((void *)c->materials[i].image.pixels);
    for(i=0;i<c->rgb.count;i++)free(c->chart_pixels[i]);free(c->chart_pixels);rf_lightmap_rgb_close(&c->rgb);
    free(c->editor);rf_geomod_authored_post_close(&c->owner);rf_geometry_close(&c->geometry);
    for(i=0;i<5;i++)rf_vpp_close(c->maps+i);rf_vpp_close(&c->levels);free(c);return 0;
}
