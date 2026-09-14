/* Real-asset integration fixture, not campaign rendering or an original oracle.
 * Uses initial world faces in file order and enabled authored lights selected
 * against each mapping box. Movers, live face removal and room routing pending. */
#include "rf/geometry.h"
#include "rf/material.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct shadow_context {
    rf_geometry_shadow_job job;const rf_lightmap_shadow_face *cached;
    const rf_level_owned_lights *lights;
    const uint32_t *ids;
    uint32_t callbacks,passes,backfacing,visited,eligible,accepted;
} shadow_context;
static int render_shadow(void *opaque,uint32_t index,uint32_t mode,unsigned char *mask,uint32_t bytes)
{
    shadow_context *c=opaque;rf_lightmap_shadow_source source={0};rf_geometry_shadow_source_result r;
    const rf_vfx_light_source *s=&c->lights->pool.sources[c->ids[index]].source;int status;
    if(mode!=1)return RF_RANGE;
    source.kind=s->type;source.radius=s->radius;memcpy(source.position,s->position,12);memcpy(source.end,s->end,12);
    status=c->cached?rf_geometry_shadow_source_mask_cached(&c->job,&source,c->cached,c->job.geometry->faces,0,mask,bytes,&r):rf_geometry_shadow_source_mask(&c->job,&source,0,mask,bytes,&r);
    if(status){fprintf(stderr,"source %u failed %d\n",c->ids[index],status);return status;}
    c->callbacks++;c->passes+=r.passes;c->backfacing+=r.backfacing;
    c->visited+=r.faces.visited;c->eligible+=r.faces.eligible;c->accepted+=r.faces.accepted;return RF_OK;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0},textures[16];rf_level level;rf_geometry geometry={0};rf_lightmaps maps={0};
    rf_geometry_materials materials={0};rf_level_owned_lights *lights=NULL;rf_random_state rng={123};
    rf_geometry_shadow_storage storage={0};const rf_geometry *g=&geometry;const rf_image **images=NULL;
    rf_lightmap_shadow_face *cached=NULL;float (*scratch)[3]=NULL;int retained=argc>1 && !strcmp(argv[1],"--retained");
    uint32_t *faces=NULL,selected[1100],modes[63],i,j,n,first,limit,max_face=3,opened=0;int status=RF_OK,archive_open=0;
    if(retained){argc--;argv++;}
    if(argc<6 || argc>21){fprintf(stderr,"archive level first_mapping count texture_archives...\n");return 2;}
    first=(uint32_t)strtoul(argv[3],NULL,10);limit=(uint32_t)strtoul(argv[4],NULL,10);
#define CHECK(call) do { status=(call);if(status){fprintf(stderr,"line %u status %d\n",(unsigned)__LINE__,status);goto done;} }while(0)
    CHECK(rf_vpp_open(&archive,argv[1]));archive_open=1;CHECK(rf_level_open(&level,&archive,argv[2]));
    CHECK(rf_geometry_open(&geometry,&level,8u*1024u*1024u));CHECK(rf_lightmaps_open(&maps,&level,16u*1024u*1024u));
    CHECK(rf_level_owned_lights_open(&level,1024u*1024u,1,1,&rng,&lights));
    for(i=5;i<(uint32_t)argc;i++){CHECK(rf_vpp_open(textures+opened,argv[i]));opened++;}
    CHECK(rf_geometry_materials_open(&materials,&g,1,textures,opened,16u*1024u*1024u));
    images=malloc((geometry.textures+1)*sizeof(*images));faces=malloc((geometry.faces+1)*sizeof(*faces));
    if(!images || !faces){status=RF_IO;goto done;}
    CHECK(rf_geometry_material_shadow_images(&materials,0,&geometry,images,geometry.textures));
    for(i=0;i<geometry.faces;i++){rf_geometry_face f;faces[i]=i;CHECK(rf_geometry_get_face(&geometry,i,&f));if(f.corners>max_face)max_face=f.corners;}
    if(retained) {
        cached=malloc(geometry.faces*sizeof(*cached));scratch=malloc(max_face*sizeof(*scratch));
        if(!cached || !scratch){status=RF_IO;goto done;}
        for(i=0;i<geometry.faces;i++)CHECK(rf_geometry_shadow_face(&geometry,i,scratch,max_face,cached+i));
        free(scratch);scratch=NULL;
    }
    if(first>=geometry.mappings || limit>geometry.mappings-first){status=RF_RANGE;goto done;}
    puts("mapping,width,height,sources,callbacks,passes,backfacing,visited,eligible,accepted,changed_bytes,mask_hash,scratch_bytes");
    for(i=first;i<first+limit;i++) {
        rf_lightmap_mapping mapping;rf_lightmap_sample_plane sample;uint32_t counts[2],changed=0,changed_bytes=0,hash=2166136261u,clip;
        rf_lightmap_shadow_filter filter;rf_lightmap_shadow_dispatch dispatch;shadow_context context={0};
        CHECK(rf_geometry_lightmap_sample_binding(&geometry,&maps,i,&mapping,&sample));
        CHECK(rf_vfx_lights_box(lights->pool.sources,lights->pool.capacity,mapping.minimum,mapping.maximum,1,1,selected,1100,&n));
        if(!n){printf("%u,%u,%u,0,0,0,0,0,0,0,0,0,0\n",i,mapping.width,mapping.height);continue;}
        CHECK(rf_level_owned_light_shadow_modes(lights,selected,n,modes,63));
        CHECK(rf_geometry_shadow_receivers(&geometry,faces,geometry.faces,i,mapping.room,NULL,NULL,counts,counts+1));
        clip=max_face*64;if(clip<counts[1]*2)clip=counts[1]*2;
        CHECK(rf_geometry_shadow_storage_open(&storage,counts[0],counts[1],max_face,clip,mapping.width,mapping.height,n,1024u*1024u));
        CHECK(rf_geometry_shadow_receivers(&geometry,faces,geometry.faces,i,mapping.room,&sample,&storage.receivers,counts,counts+1));
        filter.receivers=storage.receivers.polygons;filter.receiver_count=counts[0];memcpy(filter.threshold,mapping.density,8);
        filter.work=&storage.clip;filter.intersection=storage.intersection;filter.capacity=clip;
        context.job.geometry=&geometry;context.job.faces=faces;context.job.face_count=geometry.faces;context.job.images=images;context.job.image_count=geometry.textures;
        context.job.mapping=&mapping;context.job.sample=&sample;context.job.mapping_index=(int32_t)i;context.job.filter=&filter;context.job.work=&storage.work;
        context.cached=cached;context.lights=lights;context.ids=selected;
        dispatch.masks=storage.masks;dispatch.bytes=storage.mask_stride*n;dispatch.stride=storage.mask_stride;
        dispatch.width=mapping.width;dispatch.height=mapping.height;dispatch.source_modes=modes;dispatch.count=n;dispatch.dirty=2;dispatch.mode=1;
        status=rf_lightmap_shadow_dispatch_masks(&dispatch,render_shadow,&context,&changed);
        if(status){fprintf(stderr,"mapping %u (%ux%u), sources %u, receivers %u/%u\n",i,mapping.width,mapping.height,n,counts[0],counts[1]);goto done;}
        for(j=0;j<dispatch.bytes;j++){unsigned char b=storage.masks[j];hash=(hash^b)*16777619u;if(b!=255)changed_bytes++;}
        printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",i,mapping.width,mapping.height,n,context.callbacks,context.passes,context.backfacing,context.visited,context.eligible,context.accepted,changed_bytes,hash,storage.resident_bytes);
        rf_geometry_shadow_storage_close(&storage);
    }
done:
    rf_geometry_shadow_storage_close(&storage);free(images);free(faces);free(cached);free(scratch);rf_geometry_materials_close(&materials);rf_level_owned_lights_close(&lights);
    rf_lightmaps_close(&maps);rf_geometry_close(&geometry);while(opened)rf_vpp_close(textures+--opened);if(archive_open)rf_vpp_close(&archive);
    return status?1:0;
}
