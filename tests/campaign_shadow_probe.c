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
    const rf_level_owned_lights *lights;float ambient[3];
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
/* Authored ambient and room overrides feed both masked/unmasked passes.
 * Directional scale1 is fixture state; directional-source levels reject. */
static int shade_mapping(shadow_context *context,const rf_geometry_vertex_faces *graph,
    const rf_geometry_shadow_storage *storage,uint32_t count,uint32_t hashes[3])
{
    const rf_geometry_shadow_job *job=&context->job;const rf_lightmap_mapping *mapping=job->mapping;
    rf_lightmap_sample_lighting view={0};rf_lightmap_accumulation accumulation;
    rf_vfx_light_source lights[63];const unsigned char *masks[63];unsigned char room_ambient[4];
    rf_geometry_lightmap_work work={0};uint32_t pixels=mapping->width*mapping->height,counts[2]={0},j,k,pass,max_degree=0;
    float *channels=NULL;unsigned char *rgb=NULL,*reference=NULL,*packed=NULL;int status=RF_OK;
    if(pixels>65536 || count>63)return RF_RANGE;
    channels=malloc((size_t)pixels*12);rgb=malloc((size_t)pixels*3);reference=malloc((size_t)pixels*3);packed=malloc((size_t)pixels*2);
    if(!channels || !rgb || !reference || !packed){status=RF_IO;goto done;}
    status=rf_geometry_room_ambient(job->geometry,mapping->room,room_ambient);if(status)goto done;
    view.sample=*job->sample;view.width=mapping->width;view.height=mapping->height;
    view.lights=lights;view.light_count=count;view.mask_bytes=storage->mask_stride;view.directional_scale=1;view.capacity=pixels;
    for(j=0;j<count;j++){lights[j]=context->lights->pool.sources[context->ids[j]].source;masks[j]=storage->masks+j*storage->mask_stride;}
    for(j=0;j<3;j++){view.channels[j]=channels+j*pixels;accumulation.channels[j]=view.channels[j];}
    accumulation.count=pixels;accumulation.width=view.width;accumulation.height=view.height;
    if(count && mapping->special) {
        status=rf_geometry_lightmap_polygons(job->geometry,NULL,job->faces,job->face_count,job->mapping_index,mapping->room,NULL,counts,counts+1);if(status)goto done;
        for(j=0;j<graph->vertices;j++){k=graph->offsets[j+1]-graph->offsets[j];if(k>max_degree)max_degree=k;}
        work.polygons=malloc((counts[0]+1)*sizeof(*work.polygons));work.vertices=malloc((counts[1]+1)*sizeof(*work.vertices));work.normals=malloc((max_degree+1)*sizeof(*work.normals));
        if(!work.polygons || !work.vertices || !work.normals){status=RF_IO;goto done;}
        work.polygon_capacity=counts[0];work.vertex_capacity=counts[1];work.normal_capacity=max_degree;
        status=rf_geometry_lightmap_polygons(job->geometry,graph,job->faces,job->face_count,job->mapping_index,mapping->room,&work,counts,counts+1);if(status)goto done;
    }
    for(pass=0;pass<2;pass++) {
        unsigned char dirty=0;uint32_t hash=2166136261u;rf_lightmap_rgb_upload upload={0};
        view.masks=pass?masks:NULL;
        if(!count) {status=rf_lightmap_fill_ambient(rgb,pixels*3,view.width*3,view.width,view.height,context->ambient,room_ambient,&dirty);if(status)goto done;}
        else {
            status=rf_lightmap_seed_ambient(&view,context->ambient,room_ambient);if(status)goto done;
            status=mapping->special?rf_lightmap_accumulate_special(&view,work.polygons,counts[0]):rf_lightmap_accumulate_samples(&view);if(status)goto done;
            status=rf_lightmap_resolve_rgb(&accumulation,rgb,pixels*3,view.width*3,&dirty);if(status)goto done;
        }
        if(dirty!=8){status=RF_FORMAT;goto done;}
        if(!pass)memcpy(reference,rgb,pixels*3);
        else {hashes[2]=0;for(j=0;j<pixels*3;j++)hashes[2]+=reference[j]!=rgb[j];}
        upload.rgb=rgb;upload.rgb_bytes=pixels*3;upload.rgb_pitch=view.width*3;upload.packed=packed;upload.packed_bytes=pixels*2;upload.packed_pitch=view.width*2;upload.width=view.width;upload.height=view.height;
        status=rf_lightmap_upload_rgb_1555(&upload,&dirty);if(status)goto done;
        if(dirty){status=RF_FORMAT;goto done;}
        for(j=0;j<pixels*2;j++)hash=(hash^packed[j])*16777619u;hashes[pass]=hash;
    }
done:
    free(channels);free(rgb);free(reference);free(packed);free(work.polygons);free(work.vertices);free(work.normals);return status;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0},textures[16];rf_level level;rf_geometry geometry={0};rf_lightmaps maps={0};
    rf_level_lighting lighting;float ambient[3];
    rf_geometry_vertex_faces graph={0};
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
    CHECK(rf_level_lighting_read(&level,&lighting));
    if(lighting.directional==1){fprintf(stderr,"Directional level lighting is not yet bound in this fixture.\n");status=RF_RANGE;goto done;}
    for(i=0;i<3;i++)ambient[i]=(float)((double)lighting.color[i]*(double)0.003921568859368563f);
    for(i=5;i<(uint32_t)argc;i++){CHECK(rf_vpp_open(textures+opened,argv[i]));opened++;}
    CHECK(rf_geometry_materials_open(&materials,&g,1,textures,opened,16u*1024u*1024u));
    images=malloc((geometry.textures+1)*sizeof(*images));faces=malloc((geometry.faces+1)*sizeof(*faces));
    if(!images || !faces){status=RF_IO;goto done;}
    CHECK(rf_geometry_material_shadow_images(&materials,0,&geometry,images,geometry.textures));
    for(i=0;i<geometry.faces;i++){rf_geometry_face f;faces[i]=i;CHECK(rf_geometry_get_face(&geometry,i,&f));if(f.corners>max_face)max_face=f.corners;}
    CHECK(rf_geometry_vertex_faces_open(&geometry,faces,geometry.faces,1024u*1024u,&graph));
    if(retained) {
        cached=malloc(geometry.faces*sizeof(*cached));scratch=malloc(max_face*sizeof(*scratch));
        if(!cached || !scratch){status=RF_IO;goto done;}
        for(i=0;i<geometry.faces;i++)CHECK(rf_geometry_shadow_face(&geometry,i,scratch,max_face,cached+i));
        free(scratch);scratch=NULL;
    }
    if(first>=geometry.mappings || limit>geometry.mappings-first){status=RF_RANGE;goto done;}
    puts("mapping,width,height,sources,callbacks,passes,backfacing,visited,eligible,accepted,changed_bytes,mask_hash,scratch_bytes,unmasked_packed_hash,masked_packed_hash,rgb_changed_bytes,special");
    for(i=first;i<first+limit;i++) {
        rf_lightmap_mapping mapping;rf_lightmap_sample_plane sample;uint32_t hashes[3]={0},counts[2],changed=0,changed_bytes=0,hash=2166136261u,clip;
        rf_lightmap_shadow_filter filter;rf_lightmap_shadow_dispatch dispatch;shadow_context context={0};
        CHECK(rf_geometry_lightmap_sample_binding(&geometry,&maps,i,&mapping,&sample));
        CHECK(rf_vfx_lights_box(lights->pool.sources,lights->pool.capacity,mapping.minimum,mapping.maximum,1,1,selected,1100,&n));
        if(!n) {
            context.job.geometry=&geometry;context.job.mapping=&mapping;context.job.sample=&sample;context.job.mapping_index=(int32_t)i;
            context.lights=lights;context.ids=selected;memcpy(context.ambient,ambient,12);
            CHECK(shade_mapping(&context,&graph,&storage,0,hashes));
            printf("%u,%u,%u,0,0,0,0,0,0,0,0,0,0,%u,%u,%u,%u\n",i,mapping.width,mapping.height,hashes[0],hashes[1],hashes[2],mapping.special);continue;
        }
        CHECK(rf_level_owned_light_shadow_modes(lights,selected,n,modes,63));
        CHECK(rf_geometry_shadow_receivers(&geometry,faces,geometry.faces,i,mapping.room,NULL,NULL,counts,counts+1));
        clip=max_face*64;if(clip<counts[1]*2)clip=counts[1]*2;
        CHECK(rf_geometry_shadow_storage_open(&storage,counts[0],counts[1],max_face,clip,mapping.width,mapping.height,n,1024u*1024u));
        CHECK(rf_geometry_shadow_receivers(&geometry,faces,geometry.faces,i,mapping.room,&sample,&storage.receivers,counts,counts+1));
        filter.receivers=storage.receivers.polygons;filter.receiver_count=counts[0];memcpy(filter.threshold,mapping.density,8);
        filter.work=&storage.clip;filter.intersection=storage.intersection;filter.capacity=clip;
        context.job.geometry=&geometry;context.job.faces=faces;context.job.face_count=geometry.faces;context.job.images=images;context.job.image_count=geometry.textures;
        context.job.mapping=&mapping;context.job.sample=&sample;context.job.mapping_index=(int32_t)i;context.job.filter=&filter;context.job.work=&storage.work;
        memcpy(context.ambient,ambient,12);context.cached=cached;context.lights=lights;context.ids=selected;
        dispatch.masks=storage.masks;dispatch.bytes=storage.mask_stride*n;dispatch.stride=storage.mask_stride;
        dispatch.width=mapping.width;dispatch.height=mapping.height;dispatch.source_modes=modes;dispatch.count=n;dispatch.dirty=2;dispatch.mode=1;
        status=rf_lightmap_shadow_dispatch_masks(&dispatch,render_shadow,&context,&changed);
        if(status){fprintf(stderr,"mapping %u (%ux%u), sources %u, receivers %u/%u\n",i,mapping.width,mapping.height,n,counts[0],counts[1]);goto done;}
        CHECK(shade_mapping(&context,&graph,&storage,n,hashes));
        for(j=0;j<dispatch.bytes;j++){unsigned char b=storage.masks[j];hash=(hash^b)*16777619u;if(b!=255)changed_bytes++;}
        printf("%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",i,mapping.width,mapping.height,n,context.callbacks,context.passes,context.backfacing,context.visited,context.eligible,context.accepted,changed_bytes,hash,storage.resident_bytes,hashes[0],hashes[1],hashes[2],mapping.special);
        rf_geometry_shadow_storage_close(&storage);
    }
done:
    rf_geometry_vertex_faces_close(&graph);rf_geometry_shadow_storage_close(&storage);free(images);free(faces);free(cached);free(scratch);rf_geometry_materials_close(&materials);rf_level_owned_lights_close(&lights);
    rf_lightmaps_close(&maps);rf_geometry_close(&geometry);while(opened)rf_vpp_close(textures+--opened);if(archive_open)rf_vpp_close(&archive);
    return status?1:0;
}
