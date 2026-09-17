/* Include after authored/publication/lighting types. No scene or GPU mutation.
 * Explicit complete lighting owner is required, including cleared reset state.
 * Tail, expected digests and peak commit atomically. Borrowed inputs must remain
 * immutable; disjoint caller outputs have capacity for the complete face tail.
 */
#include "rf/geomod_collision_digest.h"
#include "rf/geomod_retained_material_digest.h"
#include "rf/authored_owner_extension.h"
/* Preserve the existing1280-face allowance for unchanged room geometry. */
#define SCENE_AUTHORED_COLLISION_ROWS (1280+RF_GEOMOD_PUBLICATION_FACES)
#ifndef SCENE_AUTHORED_DIGEST_BUDGET
#define SCENE_AUTHORED_DIGEST_BUDGET (512*1024)
#endif
typedef struct scene_authored_digest_work {
    rf_geomod_digest_material materials[128];
    uint32_t authored_tokens[128];
    rf_geomod_digest_chart charts[RF_GEOMOD_PUBLICATION_FACES];
    rf_geomod_retained_material_map maps[RF_GEOMOD_LIGHTMAP_LIMIT];
    rf_geomod_collision_digest_row rows[SCENE_AUTHORED_COLLISION_ROWS];
    uint32_t face_charts[RF_GEOMOD_PUBLICATION_FACES],map_charts[RF_GEOMOD_LIGHTMAP_LIMIT];
    uint16_t face_maps[RF_GEOMOD_PUBLICATION_FACES];
    unsigned char packed[64*64*2],rgb[64*3];
} scene_authored_digest_work;
static inline int scene_authored_digest_generated(scene_authored_digest_work *w,
    const scene_terrain_noise_map *map,const rf_geomod_publication_origin *origin,uint32_t ordinal,rf_geomod_digest_chart *chart)
{
    rf_random_state random={0};uint32_t i,n,total;int status;
    if(map->width<4 || map->width>64 || map->height<4 || map->height>64)return RF_FORMAT;
    memset(chart,0,sizeof(*chart));chart->kind=RF_GEOMOD_DIGEST_GENERATED_CHART;
    chart->owner=origin->owner;chart->source_face=origin->source_face;chart->retained_map=ordinal;
    if(origin->kind==RF_GEOMOD_PUBLICATION_NEIGHBOR)chart->kind=RF_GEOMOD_DIGEST_AUTHORED_CHART;
    chart->image.width=map->width;chart->image.height=map->height;chart->image.format=5;
    chart->image.bytes_per_pixel=2;chart->image.bytes=map->width*map->height*2;chart->image.pixels=w->packed;
    if(chart->kind==RF_GEOMOD_DIGEST_AUTHORED_CHART)snprintf(chart->image.name,sizeof(chart->image.name),"ctf06.rfl/authored/%u/%u/%u",origin->owner,origin->source_face,ordinal);
    else snprintf(chart->image.name,sizeof(chart->image.name),"ctf06.rfl/crater/%u/%u",origin->owner,ordinal);
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
    rf_geomod_retained_material_source sources[4]={{0}};
    unsigned char collection_identity[32];uint32_t collection=s && s->terrain_source_count>1,source_index,source_count=0,material_policy=1,token_count=0;
    int status=RF_FORMAT;
    if(!s || !publication || !composition || !lighting || !out || !peak_bytes ||
       !s->terrain_authored || !s->terrain_publication || !s->geometry || !actor_follow_world ||
       !actor_follow_world->slots || !lighting->terrain_noise || !composition->tree || !composition->face_ids ||
       publication->mesh.face_count>RF_GEOMOD_PUBLICATION_FACES || composition->count>SCENE_AUTHORED_COLLISION_ROWS || composition->count<publication->mesh.face_count ||
       (publication->mesh.face_count && (!origins || !face_maps || !lighting->terrain_bindings || !publication->faces || !publication->mesh.faces || !publication->mesh.vertices)) ||
       face_map_capacity<publication->mesh.face_count || serial==UINT32_MAX || (!collection && publication->cuts>serial) || s->terrain_source_count>4 ||
       (collection && !s->terrain_sources))return RF_RANGE;
    a=collection?s->terrain_sources[0].authored:s->terrain_authored;if(!a)return RF_FORMAT;manifest=&a->identity_manifest;noise=lighting->terrain_noise;base=s->terrain_publication->base;
    if(!base || !base->source_indices || noise->count>RF_GEOMOD_LIGHTMAP_LIMIT || !manifest->substrate ||
       !manifest->materials || !manifest->references || !manifest->material_count || manifest->material_count>=128 ||
       manifest->reference_count>768 || manifest->substrate->prehashed!=1 || manifest->substrate->image.pixels)return RF_FORMAT;
    peak=(uint32_t)sizeof(*w)+8192;if(peak>SCENE_AUTHORED_DIGEST_BUDGET)return RF_RANGE;
    w=calloc(1,sizeof(*w));if(!w)return RF_IO;
    memset(w->map_charts,0xff,sizeof(w->map_charts));memset(w->face_maps,0xff,sizeof(w->face_maps));
    materials=0;source_count=collection?s->terrain_source_count:1;
    for(source_index=0;source_index<source_count;source_index++) {
        const scene_terrain_authored_assets *asset=collection?s->terrain_sources[source_index].authored:a;
        const rf_geomod_authored_identity_manifest *m;
        if(!asset || asset->asset_view.room!=a->asset_view.room)goto done;m=&asset->identity_manifest;
        if(asset->asset_view.source_uid==95)material_policy=2;
        if(!m->materials || !m->references || !m->substrate || !m->material_count || m->material_count>=128 ||
           m->reference_count>768 || m->substrate->prehashed!=1 || m->substrate->image.pixels ||
           memcmp(m->substrate->content_digest,manifest->substrate->content_digest,32))goto done;
        if(collection) {
            rf_geomod_terrain_view core;rf_geomod_terrain *terrain=asset==s->terrain_authored?s->terrain:s->terrain_sources[source_index].terrain;
            status=rf_geomod_terrain_get(terrain,&core);if(status)goto done;status=RF_FORMAT;
            sources[source_index].uid=asset->asset_view.source_uid;sources[source_index].cuts=core.cuts;sources[source_index].identity=asset->source_identity;
        }
        for(i=0;i<m->material_count;i++) {
            rf_geomod_digest_material material=m->materials[i];
            if(material.key>=s->geometry->textures || material.key==UINT32_MAX-1)goto done;
            for(j=0;j<token_count;j++)if(w->authored_tokens[j]==material.key+1)break;
            if(j==token_count){if(token_count==128)goto done;w->authored_tokens[token_count++]=material.key+1;}
            material.key=actor_follow_world->slots[material.key];
            for(j=0;j<materials;j++)if(w->materials[j].key==material.key)break;
            if(j<materials) {
                if(material.prehashed!=1 || w->materials[j].prehashed!=1 || memcmp(material.content_digest,w->materials[j].content_digest,32))goto done;
            } else {if(materials>=127)goto done;w->materials[materials++]=material;}
        }
    }
    w->materials[materials]=*manifest->substrate;w->materials[materials++].key=s->terrain_material;
    if(collection) {
        checkpoint_sha hash;checkpoint_sha_init(&hash);checkpoint_sha_add(&hash,"RFSC",4);checkpoint_sha_word(&hash,1);checkpoint_sha_word(&hash,source_count);
        for(i=0;i<source_count;i++){checkpoint_sha_word(&hash,sources[i].uid);checkpoint_sha_add(&hash,sources[i].identity,32);}
        checkpoint_sha_end(&hash,collection_identity);
    }
    for(i=0;i<noise->count;i++) {
        const scene_terrain_noise_map *m=noise->maps+i;rf_geomod_retained_material_map *v=w->maps+i;
        if(m->material!=s->terrain_material) {
            uint32_t token=UINT32_MAX;
            if(material_policy!=2)goto done;
            for(j=0;j<token_count;j++)if(actor_follow_world->slots[w->authored_tokens[j]-1]==m->material && w->authored_tokens[j]<token)token=w->authored_tokens[j];
            if(token==UINT32_MAX)goto done;v->material_token=token;
        }
        memcpy(v->plane,m->plane,sizeof(v->plane));memcpy(v->minimum,m->minimum,sizeof(v->minimum));
        memcpy(v->maximum,m->maximum,sizeof(v->maximum));v->x=m->x;v->y=m->y;v->width=m->width;v->height=m->height;
        v->base_seed=m->base_seed;v->projection=m->binding.projection;
    }
    for(f=0;f<publication->mesh.face_count;f++) {
        const rf_geomod_face *face=publication->mesh.faces+f;const rf_geomod_publication_origin *o=origins+f;
        const rf_preview_surface_lightmap *binding=lighting->terrain_bindings+f;
        if(face->first>publication->mesh.vertex_count || face->count>publication->mesh.vertex_count-face->first || face->count<3)goto done;
        if(o->kind!=RF_GEOMOD_PUBLICATION_CRATER && o->reference!=UINT32_MAX) {
            const scene_terrain_authored_assets *reference_asset=a;
            const rf_geomod_authored_identity_manifest *reference_manifest=manifest;
            for(source_index=0;source_index<source_count;source_index++) {
                reference_asset=collection?s->terrain_sources[source_index].authored:a;reference_manifest=&reference_asset->identity_manifest;
                for(i=0;i<reference_manifest->reference_count;i++) {
                    const rf_geomod_digest_chart *chart=&reference_manifest->references[i].chart;
                    if(reference_manifest->references[i].reference==o->reference && chart->owner==o->owner && chart->source_face==o->source_face)break;
                }
                if(i<reference_manifest->reference_count)break;
            }
            if(source_index==source_count || charts==RF_GEOMOD_PUBLICATION_FACES)goto done;
            if(reference_manifest->references[i].compiled_material>=s->geometry->textures ||
               actor_follow_world->slots[reference_manifest->references[i].compiled_material]!=face->material)goto done;
            for(j=0;j<reference_asset->reference_count;j++)if(reference_asset->references[j].reference==o->reference)break;
            if(j==reference_asset->reference_count || binding->image!=reference_asset->references[j].image ||
               memcmp(&binding->projection,&reference_asset->references[j].projection,sizeof(binding->projection)))goto done;
            w->charts[charts]=reference_manifest->references[i].chart;w->charts[charts].key=charts;
            w->face_charts[f]=charts++;
        } else {
            found=UINT32_MAX;
            for(i=0;i<noise->count;i++) {
                const scene_terrain_noise_map *m=noise->maps+i;
                if(m->material!=face->material || binding->image!=m->binding.image || memcmp(&binding->projection,&m->binding.projection,sizeof(binding->projection)) ||
                   !scene_terrain_noise_plane(publication->faces[f].plane,m->plane))continue;
                for(j=0;j<face->count;j++) {
                    const float *point=publication->mesh.vertices[face->first+j].position;
                    for(k=0;k<3;k++)if(point[k]<m->minimum[k]-1e-4f || point[k]>m->maximum[k]+1e-4f)break;
                    if(k!=3)break;
                }
                if(j!=face->count)continue;if(found!=UINT32_MAX)goto done;found=i;
            }
            if(found==UINT32_MAX || (o->kind==RF_GEOMOD_PUBLICATION_CRATER && face->material!=s->terrain_material))goto done;
            w->face_maps[f]=(uint16_t)found;
            if(collection || material_policy==2) {
                uint32_t kind=o->kind==RF_GEOMOD_PUBLICATION_CRATER?RF_GEOMOD_DIGEST_GENERATED_CHART:RF_GEOMOD_DIGEST_AUTHORED_CHART;
                for(i=0;i<charts;i++)if(w->charts[i].kind==kind && w->charts[i].source_face==o->source_face &&
                    w->charts[i].owner==o->owner && w->charts[i].retained_map==found)break;
                w->map_charts[found]=i<charts?i:UINT32_MAX;
            }
            if(w->map_charts[found]==UINT32_MAX) {
                if(charts==RF_GEOMOD_PUBLICATION_FACES)goto done;
                status=scene_authored_digest_generated(w,noise->maps+found,o,found,w->charts+charts);if(status)goto done;
                status=RF_FORMAT;w->charts[charts].key=charts;w->map_charts[found]=charts++;
            }
            w->face_charts[f]=w->map_charts[found];
        }
    }
    result.uid=a->asset_view.source_uid;result.source_count=a->asset_view.source.face_count;result.neighbor_count=a->asset_view.solid_count;
    if(collection) {
        result.source_count=result.neighbor_count=0;
        for(i=0;i<source_count;i++){result.source_count+=s->terrain_sources[i].authored->asset_view.source.face_count;result.neighbor_count+=s->terrain_sources[i].authored->asset_view.solid_count;}
    }
    retained.source_identity=a->source_identity;retained.substrate_identity=manifest->substrate->content_digest;
    retained.maps=w->maps;retained.map_count=noise->count;retained.origins=origins;retained.face_maps=w->face_maps;
    retained.face_count=publication->mesh.face_count;retained.owner=result.uid;retained.serial=serial;retained.cuts=publication->cuts;
    retained.owner_generation=noise->generation;retained.owner_cuts=noise->cuts;retained.baked=noise->bake;
    retained.sample=noise->sample;retained.random=noise->random.value;retained.x=noise->x;retained.y=noise->y;retained.row=noise->row;retained.material_policy=material_policy;
    if(material_policy==2){retained.authored_material_tokens=w->authored_tokens;retained.authored_material_count=token_count;}
    status=collection?rf_geomod_retained_material_collection_digest(&retained,sources,source_count,result.material_digest):
        rf_geomod_retained_material_digest(&retained,result.material_digest);if(status)goto done;
    pub.mesh=publication->mesh;pub.origins=origins;pub.face_charts=w->face_charts;
    pub.materials=w->materials;pub.material_count=publication->mesh.face_count?materials:0;
    pub.charts=w->charts;pub.chart_count=charts;pub.source_domain=RF_GEOMOD_DIGEST_COMPILED_SOURCE;
    pub.publication_policy=pub.material_policy=1;
    status=rf_geomod_publication_digest(&pub,result.publication_digest);if(status)goto done;
    status=RF_FORMAT;unchanged=composition->count-publication->mesh.face_count;
    if(unchanged!=base->face_count-(publication->cuts?s->terrain_publication->replaced_count:0))goto done;
    for(i=0;i<composition->count;i++) {
        rf_geomod_collision_digest_row *r=w->rows+i;r->canonical_order=i;
        if(i<unchanged) {
            rf_geometry_face face;uint32_t id=composition->face_ids[i],rank=0;
            for(j=0;j<base->face_count;j++)if(base->source_indices[j]==id)break;if(j==base->face_count)goto done;
            if(publication->cuts)for(j=0;j<s->terrain_publication->replaced_count;j++)if(s->terrain_publication->replaced_ids[j]==id)goto done;
            for(j=0;j<unchanged;j++){if(j!=i && composition->face_ids[j]==id)goto done;if(composition->face_ids[j]<id)rank++;}
            r->canonical_order=rank;
            status=rf_geometry_get_face(s->geometry,id,&face);if(status)goto done;status=RF_FORMAT;
            if(face.room!=a->asset_view.room)goto done;
            r->domain=RF_GEOMOD_COLLISION_COMPILED;r->material=face.texture;
            r->origin.kind=RF_GEOMOD_PUBLICATION_RETAINED;r->origin.owner=a->asset_view.room;r->origin.source_face=r->origin.reference=id;
        } else {
            j=i-unchanged;r->domain=RF_GEOMOD_COLLISION_PUBLISHED;r->fragment=j;r->origin=origins[j];
            r->material=publication->mesh.faces[j].material;
            if(r->origin.reference==UINT32_MAX) {
                const scene_terrain_publication_owner *p=s->terrain_publication;
                if(material_policy!=2 || r->origin.kind!=RF_GEOMOD_PUBLICATION_NEIGHBOR)goto done;
                for(k=0;k<p->runtime_count;k++)if(p->runtime_origins[k].owner==r->origin.owner && p->runtime_origins[k].source_face==r->origin.source_face)break;
                if(k==p->runtime_count || p->runtime_surfaces[k].id!=composition->face_ids[i] ||
                    p->runtime_surfaces[k].texture>=s->geometry->textures || actor_follow_world->slots[p->runtime_surfaces[k].texture]!=r->material)goto done;
                r->metadata_id=p->runtime_surfaces[k].id;
            } else if(r->origin.reference!=composition->face_ids[i])goto done;
        }
    }
    collision.composition=composition;collision.rows=w->rows;collision.row_count=composition->count;
    collision.materials=w->materials;collision.material_count=materials;collision.source_identity=collection?collection_identity:a->source_identity;
    collision.room=a->asset_view.room;collision.collision_policy=1;
    status=rf_geomod_collision_digest(&collision,result.collision_digest);if(status)goto done;
    if(publication->mesh.face_count)memcpy(face_maps,w->face_maps,publication->mesh.face_count*sizeof(*face_maps));
    *out=result;*peak_bytes=peak;
done:
    free(w);return status;
}
