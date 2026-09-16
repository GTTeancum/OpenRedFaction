/* Include after authored/publication/lighting types. No scene or GPU mutation.
 * Explicit complete lighting owner is required, including cleared reset state.
 * Tail, expected digests and peak commit atomically. Borrowed inputs must remain
 * immutable; disjoint caller outputs have capacity for the complete face tail.
 */
#include "rf/geomod_collision_digest.h"
#include "rf/geomod_retained_material_digest.h"
#include "rf/authored_owner_extension.h"
typedef struct scene_authored_digest_work {
    rf_geomod_digest_material materials[128];
    rf_geomod_digest_chart charts[768];
    rf_geomod_retained_material_map maps[1024];
    rf_geomod_collision_digest_row rows[2048];
    uint32_t face_charts[768],map_charts[1024];
    uint16_t face_maps[768];
    unsigned char packed[64*64*2],rgb[64*3];
} scene_authored_digest_work;
static inline int scene_authored_digest_generated(scene_authored_digest_work *w,
    const scene_terrain_noise_map *map,uint32_t owner,uint32_t ordinal,rf_geomod_digest_chart *chart)
{
    rf_random_state random={0};uint32_t i,n,total;int status;
    if(map->width<4 || map->width>64 || map->height<4 || map->height>64)return RF_FORMAT;
    memset(chart,0,sizeof(*chart));chart->kind=RF_GEOMOD_DIGEST_GENERATED_CHART;
    chart->owner=owner;chart->source_face=UINT32_MAX;chart->retained_map=ordinal;
    chart->image.width=map->width;chart->image.height=map->height;chart->image.format=5;
    chart->image.bytes_per_pixel=2;chart->image.bytes=map->width*map->height*2;chart->image.pixels=w->packed;
    snprintf(chart->image.name,sizeof(chart->image.name),"ctf06.rfl/crater/%u/%u",owner,ordinal);
    for(i=0;i<2;i++) {
        uint32_t axis=map->binding.projection.axes[i],dim=i?map->height:map->width;float span;double scale;
        if(axis>2)return RF_FORMAT;span=map->maximum[axis]-map->minimum[axis];if(!(span>0))return RF_FORMAT;
        scale=(dim-2)/(double)span;chart->projection.axes[i]=axis;
        chart->projection.scale[i]=(float)(scale/dim);chart->projection.offset[i]=(float)((1-map->minimum[axis]*scale)/dim);
    }
    random.value=map->base_seed;total=map->width*map->height;
    for(i=0;i<total;i+=n) {
        n=total-i;if(n>64)n=64;
        status=rf_geomod_light_noise(w->rgb,sizeof(w->rgb),n*3,n,1,&random);if(status)return status;
        status=rf_lightmap_pack_1555(w->rgb,n*3,n,1,0,w->packed+i*2,n*2,sizeof(w->packed)-i*2);if(status)return status;
    }
    status=rf_geomod_image_content_digest(&chart->image,chart->content_digest);if(status)return status;
    chart->image.pixels=NULL;chart->prehashed=1;return RF_OK;
}
static inline int scene_authored_digest_capture(const scene_stream *s,
    const rf_geomod_terrain_view *publication,const rf_geomod_publication_origin *origins,
    const rf_collision_composition_view *composition,const scene_stream *lighting,uint32_t serial,
    uint16_t *face_maps,uint32_t face_map_capacity,rf_authored_owner_expected *out,uint32_t *peak_bytes)
{
    const scene_terrain_authored_assets *a;const rf_geomod_authored_identity_manifest *manifest;
    const scene_terrain_noise_owner *noise;const rf_collision_tree *base;
    scene_authored_digest_work *w=NULL;rf_authored_owner_expected result={0};
    rf_geomod_publication_digest_input pub={0};rf_geomod_collision_digest_input collision={0};
    rf_geomod_retained_material_input retained={0};uint32_t f,i,j,k,charts=0,materials,unchanged,found,peak;
    int status=RF_FORMAT;
    if(!s || !publication || !composition || !lighting || !out || !peak_bytes ||
       !s->terrain_authored || !s->terrain_publication || !s->geometry || !actor_follow_world ||
       !actor_follow_world->slots || !lighting->terrain_noise || !composition->tree || !composition->face_ids ||
       publication->mesh.face_count>768 || composition->count>2048 || composition->count<publication->mesh.face_count ||
       (publication->mesh.face_count && (!origins || !face_maps || !lighting->terrain_bindings || !publication->faces || !publication->mesh.faces || !publication->mesh.vertices)) ||
       face_map_capacity<publication->mesh.face_count || serial==UINT32_MAX || publication->cuts>serial)return RF_RANGE;
    a=s->terrain_authored;manifest=&a->identity_manifest;noise=lighting->terrain_noise;base=s->terrain_publication->base;
    if(!base || !base->source_indices || noise->count>1024 || !manifest->substrate ||
       !manifest->materials || !manifest->references || !manifest->material_count || manifest->material_count>=128 ||
       manifest->reference_count>768 || manifest->substrate->prehashed!=1 || manifest->substrate->image.pixels)return RF_FORMAT;
    peak=(uint32_t)sizeof(*w)+8192;if(peak>512*1024)return RF_RANGE;
    w=calloc(1,sizeof(*w));if(!w)return RF_IO;
    memset(w->map_charts,0xff,sizeof(w->map_charts));memset(w->face_maps,0xff,sizeof(w->face_maps));
    materials=manifest->material_count+1;
    for(i=0;i<manifest->material_count;i++) {
        w->materials[i]=manifest->materials[i];if(w->materials[i].key>=s->geometry->textures)goto done;
        w->materials[i].key=actor_follow_world->slots[w->materials[i].key];
    }
    w->materials[materials-1]=*manifest->substrate;w->materials[materials-1].key=s->terrain_material;
    for(i=0;i<noise->count;i++) {
        const scene_terrain_noise_map *m=noise->maps+i;rf_geomod_retained_material_map *v=w->maps+i;
        if(m->material!=s->terrain_material)goto done;
        memcpy(v->plane,m->plane,sizeof(v->plane));memcpy(v->minimum,m->minimum,sizeof(v->minimum));
        memcpy(v->maximum,m->maximum,sizeof(v->maximum));v->x=m->x;v->y=m->y;v->width=m->width;v->height=m->height;
        v->base_seed=m->base_seed;v->projection=m->binding.projection;
    }
    for(f=0;f<publication->mesh.face_count;f++) {
        const rf_geomod_face *face=publication->mesh.faces+f;const rf_geomod_publication_origin *o=origins+f;
        const rf_preview_surface_lightmap *binding=lighting->terrain_bindings+f;
        if(face->first>publication->mesh.vertex_count || face->count>publication->mesh.vertex_count-face->first || face->count<3)goto done;
        if(o->kind!=RF_GEOMOD_PUBLICATION_CRATER) {
            for(i=0;i<manifest->reference_count;i++)if(manifest->references[i].reference==o->reference)break;
            if(i==manifest->reference_count || charts==768)goto done;
            if(manifest->references[i].compiled_material>=s->geometry->textures ||
               actor_follow_world->slots[manifest->references[i].compiled_material]!=face->material)goto done;
            for(j=0;j<a->reference_count;j++)if(a->references[j].reference==o->reference)break;
            if(j==a->reference_count || binding->image!=a->references[j].image ||
               memcmp(&binding->projection,&a->references[j].projection,sizeof(binding->projection)))goto done;
            w->charts[charts]=manifest->references[i].chart;w->charts[charts].key=charts;
            w->face_charts[f]=charts++;
        } else {
            found=UINT32_MAX;
            for(i=0;i<noise->count;i++) {
                const scene_terrain_noise_map *m=noise->maps+i;
                if(binding->image!=m->binding.image || memcmp(&binding->projection,&m->binding.projection,sizeof(binding->projection)) ||
                   !scene_terrain_noise_plane(publication->faces[f].plane,m->plane))continue;
                for(j=0;j<face->count;j++) {
                    const float *point=publication->mesh.vertices[face->first+j].position;
                    for(k=0;k<3;k++)if(point[k]<m->minimum[k]-1e-4f || point[k]>m->maximum[k]+1e-4f)break;
                    if(k!=3)break;
                }
                if(j!=face->count)continue;if(found!=UINT32_MAX)goto done;found=i;
            }
            if(found==UINT32_MAX || face->material!=s->terrain_material)goto done;
            w->face_maps[f]=(uint16_t)found;
            if(w->map_charts[found]==UINT32_MAX) {
                if(charts==768)goto done;
                status=scene_authored_digest_generated(w,noise->maps+found,o->owner,found,w->charts+charts);if(status)goto done;
                status=RF_FORMAT;w->charts[charts].key=charts;w->map_charts[found]=charts++;
            }
            w->face_charts[f]=w->map_charts[found];
        }
    }
    result.uid=a->asset_view.source_uid;result.source_count=a->asset_view.source.face_count;result.neighbor_count=a->asset_view.solid_count;
    retained.source_identity=a->source_identity;retained.substrate_identity=manifest->substrate->content_digest;
    retained.maps=w->maps;retained.map_count=noise->count;retained.origins=origins;retained.face_maps=w->face_maps;
    retained.face_count=publication->mesh.face_count;retained.owner=result.uid;retained.serial=serial;retained.cuts=publication->cuts;
    retained.owner_generation=noise->generation;retained.owner_cuts=noise->cuts;retained.baked=noise->bake;
    retained.sample=noise->sample;retained.random=noise->random.value;retained.x=noise->x;retained.y=noise->y;retained.row=noise->row;retained.material_policy=1;
    status=rf_geomod_retained_material_digest(&retained,result.material_digest);if(status)goto done;
    pub.mesh=publication->mesh;pub.origins=origins;pub.face_charts=w->face_charts;
    pub.materials=w->materials;pub.material_count=publication->mesh.face_count?materials:0;
    pub.charts=w->charts;pub.chart_count=charts;pub.source_domain=RF_GEOMOD_DIGEST_COMPILED_SOURCE;
    pub.publication_policy=pub.material_policy=1;
    status=rf_geomod_publication_digest(&pub,result.publication_digest);if(status)goto done;
    status=RF_FORMAT;unchanged=composition->count-publication->mesh.face_count;
    if(unchanged!=base->face_count-(publication->cuts?a->asset_view.replaced_count:0))goto done;
    for(i=0;i<composition->count;i++) {
        rf_geomod_collision_digest_row *r=w->rows+i;r->canonical_order=i;
        if(i<unchanged) {
            rf_geometry_face face;uint32_t id=composition->face_ids[i],rank=0;
            for(j=0;j<base->face_count;j++)if(base->source_indices[j]==id)break;if(j==base->face_count)goto done;
            if(publication->cuts)for(j=0;j<a->asset_view.replaced_count;j++)if(a->asset_view.replaced_ids[j]==id)goto done;
            for(j=0;j<unchanged;j++){if(j!=i && composition->face_ids[j]==id)goto done;if(composition->face_ids[j]<id)rank++;}
            r->canonical_order=rank;
            status=rf_geometry_get_face(s->geometry,id,&face);if(status)goto done;status=RF_FORMAT;
            if(face.room!=a->asset_view.room)goto done;
            r->domain=RF_GEOMOD_COLLISION_COMPILED;r->material=face.texture;
            r->origin.kind=RF_GEOMOD_PUBLICATION_RETAINED;r->origin.owner=a->asset_view.room;r->origin.source_face=r->origin.reference=id;
        } else {
            j=i-unchanged;r->domain=RF_GEOMOD_COLLISION_PUBLISHED;r->fragment=j;r->origin=origins[j];
            r->material=publication->mesh.faces[j].material;
            if(r->origin.reference!=composition->face_ids[i])goto done;
        }
    }
    collision.composition=composition;collision.rows=w->rows;collision.row_count=composition->count;
    collision.materials=w->materials;collision.material_count=materials;collision.source_identity=a->source_identity;
    collision.room=a->asset_view.room;collision.collision_policy=1;
    status=rf_geomod_collision_digest(&collision,result.collision_digest);if(status)goto done;
    if(publication->mesh.face_count)memcpy(face_maps,w->face_maps,publication->mesh.face_count*sizeof(*face_maps));
    *out=result;*peak_bytes=peak;
done:
    free(w);return status;
}
