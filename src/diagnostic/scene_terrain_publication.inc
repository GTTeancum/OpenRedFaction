/* Include after scene_stream and scene_terrain_authored.inc.
 * Required forward declaration/field in scene.c:
 * typedef struct scene_terrain_publication_owner scene_terrain_publication_owner;
 * scene_terrain_publication_owner *terrain_publication;
 * No source asset pointers escape their level owner. No renderer upload occurs.
 * Caller stages generated atlas/noise and draw subdivision before finish.
 * Close the overlay BEFORE closing this owner (trees borrow these banks).
 */
#include "rf/geomod_publication_binding.h"
#include "rf/collision_composition.h"
#define SCENE_PUBLICATION_BUDGET (4u*1024u*1024u)
typedef struct scene_publication_bank {
    rf_geomod_vertex vertices[RF_GEOMOD_PUBLICATION_VERTICES];
    rf_geomod_face faces[RF_GEOMOD_PUBLICATION_FACES];
    rf_geomod_publication_origin origins[RF_GEOMOD_PUBLICATION_FACES];
    rf_geomod_publication_bound_corner corners[RF_GEOMOD_PUBLICATION_VERTICES];
    rf_collision_face_filter filters[RF_GEOMOD_PUBLICATION_FACES];
    rf_collision_face bound[RF_GEOMOD_PUBLICATION_FACES];
    float positions[RF_GEOMOD_PUBLICATION_VERTICES][3];
    rf_preview_surface_lightmap bindings[RF_GEOMOD_PUBLICATION_FACES];
    uint32_t ids[RF_GEOMOD_PUBLICATION_FACES];
    rf_geomod_mesh_view mesh;uint32_t cuts,generated;
} scene_publication_bank;
struct scene_terrain_publication_owner {
    rf_geomod_publication_work work;scene_publication_bank banks[2];
    rf_collision_composition *composition;const rf_collision_tree *base;
    rf_geomod_publication_binding_reference references[256];
    uint32_t replaced_ids[256],reference_count,replaced_count;
    uint32_t active,pending,has_pending,resident_bytes,peak_bytes;
};
uint32_t rf_scene_terrain_publication[8]; /* activefaces,vertices,cuts,generation,resident,peak,pending,status */
static void scene_terrain_publication_close(scene_terrain_publication_owner **ptr)
{
    scene_terrain_publication_owner *p;if(!ptr || !*ptr)return;p=*ptr;
    rf_collision_composition_close(&p->composition);free(p);*ptr=NULL;
}
static inline int scene_terrain_publication_open(scene_stream *s)
{
    scene_terrain_publication_owner *p;rf_collision_composition_view cv;uint32_t room,capacity,i,j,k,n;int status;
    if(!s || !s->terrain_authored || !s->collision || s->terrain_publication || s->terrain_collision.storage)return RF_RANGE;
    room=s->terrain_authored->asset_view.room;if(room>=s->collision->room_count)return RF_FORMAT;
    p=calloc(1,sizeof(*p));if(!p)return RF_IO;
    p->base=&s->collision->rooms[room].tree;capacity=p->base->face_count+RF_GEOMOD_PUBLICATION_FACES;
    if(capacity<p->base->face_count || sizeof(*p)>=SCENE_PUBLICATION_BUDGET){status=RF_RANGE;goto failed;}
    n=s->terrain_source_count?s->terrain_source_count:1;
    if(n>4 || (s->terrain_source_count && !s->terrain_sources)){status=RF_RANGE;goto failed;}
    for(i=0;i<n;i++) {
        scene_terrain_authored_assets *a=s->terrain_source_count?s->terrain_sources[i].authored:s->terrain_authored;
        if(!a || a->asset_view.room!=room){status=RF_FORMAT;goto failed;}
        if(a->asset_view.replaced_count>256-p->replaced_count){status=RF_RANGE;goto failed;}
        for(j=0;j<a->asset_view.replaced_count;j++)p->replaced_ids[p->replaced_count++]=a->asset_view.replaced_ids[j];
        for(j=0;j<a->reference_count;j++) {
            const rf_geomod_publication_binding_reference *r=a->references+j;
            for(k=0;k<p->reference_count;k++)if(p->references[k].reference==r->reference)break;
            if(k<p->reference_count) {
                if(memcmp(p->references+k,r,sizeof(*r))){status=RF_FORMAT;goto failed;}
            } else {
                if(p->reference_count==256){status=RF_RANGE;goto failed;}
                p->references[p->reference_count++]=*r;
            }
        }
    }
    status=rf_collision_composition_open(p->base,p->base->source_indices,
        p->replaced_ids,p->replaced_count,
        capacity,SCENE_PUBLICATION_BUDGET-(uint32_t)sizeof(*p),NULL,&p->composition);if(status)goto failed;
    status=rf_geometry_collision_overlay_open(s->collision,room,capacity,128*1024,&s->terrain_collision);if(status)goto failed;
    status=rf_collision_composition_get(p->composition,&cv);if(status)goto failed_overlay;
    p->resident_bytes=(uint32_t)sizeof(*p)+cv.resident_bytes+s->terrain_collision.resident_bytes;
    p->peak_bytes=(uint32_t)sizeof(*p)+cv.peak_bytes+s->terrain_collision.resident_bytes;
    if(p->peak_bytes>SCENE_PUBLICATION_BUDGET){status=RF_RANGE;goto failed_overlay;}
    s->terrain_publication=p;memset(rf_scene_terrain_publication,0,sizeof(rf_scene_terrain_publication));return RF_OK;
failed_overlay:rf_geometry_collision_overlay_close(&s->terrain_collision);
failed:scene_terrain_publication_close(&p);return status;
}
static inline void scene_terrain_publication_abort(scene_stream *s)
{
    if(s && s->terrain_publication){rf_collision_composition_abort(s->terrain_publication->composition);s->terrain_publication->has_pending=0;}
}
/* Synchronous private preparation. candidate/cutters may be borrowed from a
 * history-check visitor; no input pointer escapes this call. Inactive banks and
 * pending composition change, but active scene/core/overlay and telemetry do not.
 * Abort before leaving a validation visitor. saved_serial is the scene revision,
 * not the independently replayed core generation. Caller validates save identity. */
static int scene_terrain_publication_prepare_from(scene_stream *s,
    const rf_geomod_terrain_view *candidate,const rf_geomod_publication_cut *cuts,
    uint32_t count,uint32_t saved_serial)
{
    scene_terrain_publication_owner *p;scene_publication_bank *b;scene_terrain_authored_assets *a;
    rf_geomod_terrain_view terrain;rf_geomod_publication_job job={0};
    uint32_t i,j,pending;int status;
    if(!s || !(p=s->terrain_publication) || !(a=s->terrain_authored) || p->has_pending ||
       !candidate || count!=candidate->cuts || count>RF_GEOMOD_CUT_LIMIT ||
       (count && !cuts) || count>saved_serial || saved_serial==UINT32_MAX)return RF_RANGE;
    terrain=*candidate;
    p->pending=p->active^1u;b=p->banks+p->pending;b->cuts=terrain.cuts;b->generated=0;
    b->mesh=(rf_geomod_mesh_view){b->vertices,b->faces,0,0,saved_serial};
    if(!terrain.cuts && !s->terrain_source_count){status=rf_collision_composition_prepare_reset(p->composition);if(status)return status;p->has_pending=1;return RF_OK;}
    job.terrain=terrain.mesh;job.windows=a->windows;job.neighbors=a->neighbors;
    job.window_origins=a->asset_view.window_origins;job.neighbor_origins=a->asset_view.neighbor_origins;
    job.crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,a->asset_view.source_uid,UINT32_MAX,s->terrain_fallback};
    job.source_planes=a->asset_view.source_planes;job.source_plane_count=a->source.face_count;
    job.solids=a->asset_view.solids;job.solid_count=a->asset_view.solid_count;job.cuts=cuts;job.cut_count=terrain.cuts;
    if(s->terrain_source_count) {
        rf_geomod_publication_job jobs[4];rf_geomod_publication_cut other_cuts[4][RF_GEOMOD_CUT_LIMIT];
        uint32_t source,cut,total=0,selected=0;
        if(s->terrain_source_count>4 || !s->terrain_sources)return RF_RANGE;
        for(source=0;source<s->terrain_source_count;source++) {
            scene_terrain_source_owner *owner=s->terrain_sources+source;
            scene_terrain_authored_assets *asset=owner->authored;rf_geomod_terrain_view view;
            if(!asset || !owner->terrain)return RF_RANGE;
            if(asset==a){jobs[source]=job;selected++;}
            else {
                status=rf_geomod_terrain_get(owner->terrain,&view);if(status)return status;
                if(view.cuts>RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
                memset(jobs+source,0,sizeof(*jobs));jobs[source].terrain=view.mesh;
                jobs[source].windows=asset->windows;jobs[source].neighbors=asset->neighbors;
                jobs[source].window_origins=asset->asset_view.window_origins;jobs[source].neighbor_origins=asset->asset_view.neighbor_origins;
                jobs[source].source_planes=asset->asset_view.source_planes;jobs[source].source_plane_count=asset->source.face_count;
                jobs[source].solids=asset->asset_view.solids;jobs[source].solid_count=asset->asset_view.solid_count;
                jobs[source].crater_origin=(rf_geomod_publication_origin){RF_GEOMOD_PUBLICATION_CRATER,
                    asset->asset_view.source_uid,UINT32_MAX,asset->asset_view.replaced_ids[0]};
                jobs[source].cuts=other_cuts[source];jobs[source].cut_count=view.cuts;
                for(cut=0;cut<view.cuts;cut++) {
                    rf_geomod_publication_cut *c=other_cuts[source]+cut;
                    status=rf_geomod_terrain_cutter_get(owner->terrain,cut,&c->mesh,c->kernel,&c->star);if(status)return status;
                }
            }
            total+=jobs[source].cut_count;
        }
        if(selected!=1 || total>saved_serial)return RF_FORMAT;
        b->cuts=total;
        status=rf_geomod_publication_build_groups(jobs,s->terrain_source_count,saved_serial,&p->work,
            b->vertices,RF_GEOMOD_PUBLICATION_VERTICES,b->faces,RF_GEOMOD_PUBLICATION_FACES,b->origins,&b->mesh);
    } else status=rf_geomod_publication_build(&job,&p->work,b->vertices,RF_GEOMOD_PUBLICATION_VERTICES,
        b->faces,RF_GEOMOD_PUBLICATION_FACES,b->origins,&b->mesh);
    if(status)return status;
    b->mesh.generation=saved_serial;
    status=rf_geomod_publication_bind(&b->mesh,b->origins,p->references,p->reference_count,
        RF_GEOMOD_BINDING_DEFER_GENERATED,b->corners,RF_GEOMOD_PUBLICATION_VERTICES,sizeof(b->corners),&pending);if(status)return status;
    b->generated=pending;
    for(i=0;i<b->mesh.face_count;i++) {
        const rf_geomod_publication_origin *o=b->origins+i;const rf_geomod_publication_binding_reference *r=NULL;
        b->ids[i]=o->reference;b->bindings[i].image=UINT32_MAX;
        if(o->reference==UINT32_MAX)return RF_FORMAT;
        for(j=0;j<p->base->face_count;j++)if(p->base->source_indices[j]==o->reference)break;
        if(j==p->base->face_count)return RF_NOT_FOUND;
        b->filters[i]=p->base->faces[j].filter;b->filters[i].query_flags=0;
        if(o->kind==RF_GEOMOD_PUBLICATION_CRATER) {
            b->faces[i].source_face=UINT32_MAX;b->filters[i].face_flags=256;
        } else {
            for(j=0;j<p->reference_count;j++)if(p->references[j].reference==o->reference){r=p->references+j;break;}
            if(!r)return RF_NOT_FOUND;
            b->faces[i].source_face=o->reference; /* existing draw/lighting expects compiled IDs */
            b->bindings[i].image=r->image;b->bindings[i].projection=r->projection;
            if(r->image!=UINT32_MAX && r->image>=s->light_rgb.count)return RF_FORMAT;
        }
        if(b->faces[i].material>=s->materials->count)return RF_FORMAT;
    }
    status=rf_geomod_collision_faces(&b->mesh,b->filters,b->positions,RF_GEOMOD_PUBLICATION_VERTICES,
        b->bound,RF_GEOMOD_PUBLICATION_FACES);if(status)return status;
    status=rf_collision_composition_prepare(p->composition,b->bound,b->ids,b->mesh.face_count);if(status)return status;
    p->has_pending=1;return RF_OK;
}
/* Existing ordinary-edit entry point: same active-core history and telemetry. */
static int scene_terrain_publication_prepare(scene_stream *s)
{
    rf_geomod_terrain_view terrain;rf_geomod_publication_cut cuts[RF_GEOMOD_CUT_LIMIT];
    uint32_t i;int status;
    if(!s || !s->terrain_publication || !s->terrain_authored || s->terrain_publication->has_pending)return RF_RANGE;
    status=rf_geomod_terrain_get(s->terrain,&terrain);if(status)return status;
    if(terrain.cuts>RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
    for(i=0;i<terrain.cuts;i++) {
        status=rf_geomod_terrain_cutter_get(s->terrain,i,&cuts[i].mesh,cuts[i].kernel,&cuts[i].star);
        if(status)return status;
    }
    status=scene_terrain_publication_prepare_from(s,&terrain,cuts,terrain.cuts,terrain.mesh.generation);
    if(!status && terrain.cuts)rf_scene_terrain_publication[6]=1;
    return status;
}
/* Pending candidate is mutable only for caller-owned lighting staging.
 * Do not mutate mesh topology/positions after prepare: pending tree borrows it.
 * Copied source bindings must be restored if legacy lighting zeros them. */
static inline int scene_terrain_publication_candidate(scene_stream *s,rf_geomod_terrain_view *out,
    const rf_geomod_publication_origin **origins,rf_preview_surface_lightmap **bindings)
{
    scene_terrain_publication_owner *p;scene_publication_bank *b;rf_collision_composition_view cv;int status;
    if(!s || !(p=s->terrain_publication) || !p->has_pending || !out)return RF_RANGE;
    status=rf_collision_composition_pending(p->composition,&cv);if(status)return status;b=p->banks+p->pending;
    *out=(rf_geomod_terrain_view){b->mesh,b->bound,cv.tree,b->cuts,(uint32_t)sizeof(*p)+cv.resident_bytes,
        (uint32_t)sizeof(*p)+cv.peak_bytes};if(origins)*origins=b->origins;if(bindings)*bindings=b->bindings;return RF_OK;
}
/* complete supplies one final binding per candidate face. Images must already
 * be uploaded/retained by a STAGED owner; image_count is that owner's valid ID
 * extent, not a promise that unuploaded IDs will be filled later. Source binding
 * words must stay exact. This function performs no light baking/upload itself. */
static int scene_terrain_publication_finish(scene_stream *s,const rf_preview_surface_lightmap *complete,
    uint32_t count,uint32_t image_count)
{
    scene_terrain_publication_owner *p;scene_publication_bank *b;rf_collision_composition_view cv;
    uint32_t i,j;int status;
    if(!s || !(p=s->terrain_publication) || !p->has_pending)return RF_RANGE;b=p->banks+p->pending;
    if(count!=b->mesh.face_count || (count && !complete))return RF_RANGE;
    for(i=0;i<count;i++) {
        const rf_preview_surface_lightmap *lm=complete+i;
        if(b->origins[i].kind!=RF_GEOMOD_PUBLICATION_CRATER) {
            if(memcmp(lm,b->bindings+i,sizeof(*lm)))return RF_FORMAT;
        } else if(lm->image==UINT32_MAX)return RF_NOT_FOUND;
        if(lm->image!=UINT32_MAX) {
            if(lm->image>=image_count)return RF_RANGE;
            if(lm->projection.axes[0]==lm->projection.axes[1])return RF_FORMAT;
            for(j=0;j<2;j++)if(lm->projection.axes[j]>2 || !isfinite(lm->projection.scale[j]) || !isfinite(lm->projection.offset[j]))return RF_FORMAT;
        }
    }
    status=rf_collision_composition_pending(p->composition,&cv);if(status)return status;
    if((uint64_t)sizeof(*p)+cv.peak_bytes+s->terrain_collision.resident_bytes>SCENE_PUBLICATION_BUDGET)return RF_RANGE;
    /* Bind first; commit releases the old tree only after overlay replacement. */
    status=rf_geometry_collision_overlay_bind(&s->terrain_collision,cv.tree,cv.face_ids,cv.count);if(status)return status;
    status=rf_collision_composition_commit(p->composition);if(status)return status; /* no allocation */
    if(count && complete!=b->bindings)memcpy(b->bindings,complete,count*sizeof(*complete));
    p->active=p->pending;p->has_pending=0;
    p->resident_bytes=(uint32_t)sizeof(*p)+cv.resident_bytes+s->terrain_collision.resident_bytes;
    p->peak_bytes=(uint32_t)sizeof(*p)+cv.peak_bytes+s->terrain_collision.resident_bytes;
    rf_scene_terrain_publication[0]=b->mesh.face_count;rf_scene_terrain_publication[1]=b->mesh.vertex_count;
    rf_scene_terrain_publication[2]=b->cuts;rf_scene_terrain_publication[3]=b->mesh.generation;
    rf_scene_terrain_publication[4]=p->resident_bytes;rf_scene_terrain_publication[5]=p->peak_bytes;
    rf_scene_terrain_publication[6]=0;rf_scene_terrain_publication[7]=0;return RF_OK;
}
static inline int scene_terrain_publication_view(scene_stream *s,rf_geomod_terrain_view *out)
{
    scene_terrain_publication_owner *p;scene_publication_bank *b;rf_collision_composition_view cv;int status;
    if(!s || !(p=s->terrain_publication) || !out)return RF_RANGE;
    status=rf_collision_composition_get(p->composition,&cv);if(status)return status;b=p->banks+p->active;
    *out=(rf_geomod_terrain_view){b->mesh,b->bound,cv.tree,b->cuts,p->resident_bytes,p->peak_bytes};return RF_OK;
}
/* Only a no-generated-map candidate can use the convenience bind. A crater
 * returns NOT_FOUND while keeping a pending candidate for explicit staging. */
static inline int scene_terrain_publication_bind(scene_stream *s)
{
    scene_terrain_publication_owner *p;scene_publication_bank *b;int status=scene_terrain_publication_prepare(s);
    if(status)return status;p=s->terrain_publication;b=p->banks+p->pending;
    if(b->generated)return RF_NOT_FOUND;
    return scene_terrain_publication_finish(s,b->bindings,b->mesh.face_count,s->light_rgb.count);
}
