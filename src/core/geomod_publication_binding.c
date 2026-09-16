#include "rf/geomod_publication_binding.h"
#include <math.h>
#include <string.h>
static int reference_valid(const rf_geomod_publication_binding_reference *r)
{
    uint32_t i;
    if(r->reference==UINT32_MAX || r->source_face==UINT32_MAX || r->owner==UINT32_MAX || r->material==UINT32_MAX)return 0;
    if(r->mapping==UINT32_MAX || r->image==UINT32_MAX)return r->mapping==r->image;
    for(i=0;i<2;i++)if(r->projection.axes[i]>2 || !isfinite(r->projection.scale[i]) || !isfinite(r->projection.offset[i]))return 0;
    return 1;
}
int rf_geomod_publication_bind(const rf_geomod_mesh_view *mesh,const rf_geomod_publication_origin *origins,
    const rf_geomod_publication_binding_reference *refs,uint32_t ref_count,uint32_t policy,
    rf_geomod_publication_bound_corner *out,uint32_t capacity,uint32_t budget,uint32_t *out_pending)
{
    uint32_t pass,i,j,k,cursor=0,pending=0;
    if(!mesh || !out_pending || policy>RF_GEOMOD_BINDING_DEFER_GENERATED ||
       (mesh->vertex_count && (!mesh->vertices || !out)) || (mesh->face_count && (!mesh->faces || !origins)) ||
       (ref_count && !refs) || mesh->vertex_count>capacity ||
       (uint64_t)mesh->vertex_count*sizeof(*out)>budget)return RF_RANGE;
    for(i=0;i<ref_count;i++) {
        if(!reference_valid(refs+i))return RF_FORMAT;
        for(j=0;j<i;j++)if(refs[i].reference==refs[j].reference)return RF_FORMAT;
    }
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *f=mesh->faces+i;
        if(f->first!=cursor || f->count<3 || f->count>64 || cursor>mesh->vertex_count || f->count>mesh->vertex_count-cursor)return RF_FORMAT;
        cursor+=f->count;
    }
    if(cursor!=mesh->vertex_count)return RF_FORMAT;
    /* Same pure calculation twice; all rejecting branches execute in pass0. */
    for(pass=0;pass<2;pass++)for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *face=mesh->faces+i;const rf_geomod_publication_origin *origin=origins+i;
        const rf_geomod_publication_binding_reference *ref=NULL;
        uint32_t generated=origin->kind==RF_GEOMOD_PUBLICATION_CRATER;
        if(origin->kind>RF_GEOMOD_PUBLICATION_NEIGHBOR)return RF_FORMAT;
        if(generated) {
            if(policy!=RF_GEOMOD_BINDING_DEFER_GENERATED)return RF_NOT_FOUND;
            if(face->material==UINT32_MAX)return RF_FORMAT;
            if(!pass)pending+=face->count;
        } else {
            for(j=0;j<ref_count;j++)if(refs[j].reference==origin->reference){ref=refs+j;break;}
            if(!ref)return RF_NOT_FOUND;
            if(ref->source_face!=origin->source_face || ref->owner!=origin->owner || face->source_face!=origin->source_face)return RF_FORMAT;
        }
        for(j=0;j<face->count;j++) {
            const rf_geomod_vertex *v=mesh->vertices+face->first+j;rf_geomod_publication_bound_corner value={0};int status;
            for(k=0;k<3;k++)if(!isfinite(v->position[k]))return RF_FORMAT;
            for(k=0;k<2;k++)if(!isfinite(v->uv[k]))return RF_FORMAT;
            memcpy(value.uv,v->uv,8);value.origin=*origin;
            value.material=generated?face->material:ref->material;
            value.mapping=generated?UINT32_MAX:ref->mapping;value.image=generated?UINT32_MAX:ref->image;
            value.status=generated?RF_GEOMOD_BINDING_GENERATED_PENDING:(ref->mapping==UINT32_MAX?RF_GEOMOD_BINDING_UNLIT:RF_GEOMOD_BINDING_SOURCE);
            if(value.status==RF_GEOMOD_BINDING_SOURCE){status=rf_lightmap_project(&ref->projection,v->position,value.lightmap_uv);if(status)return status;}
            if(pass)out[face->first+j]=value;
        }
    }
    *out_pending=pending;return RF_OK;
}
