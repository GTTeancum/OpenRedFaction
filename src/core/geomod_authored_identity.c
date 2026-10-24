#include "rf/geomod_authored_identity.h"
#include <math.h>
#include <string.h>
/* SHA-256 shared with the other GeoMod digests; byte-identical output. */
#include "geomod_identity_sha.h"
static int name_valid(const char name[64])
{
    uint32_t i;if(!name[0])return 0;
    for(i=0;i<64;i++) {unsigned char c=(unsigned char)name[i];if(!c)return 1;
        if(c<32 || c>126 || c=='\\' || (c>='A'&&c<='Z'))return 0;}
    return 0;
}
static void name_hash(identity_sha *h,const char name[64])
{uint32_t n=(uint32_t)strlen(name);identity_sha_word(h,n);identity_sha_add(h,name,n);}
static int image_valid(const rf_geomod_identity_image *v)
{
    uint64_t bytes=(uint64_t)v->width*v->height*v->bytes_per_pixel;
    return name_valid(v->name) && v->width && v->height && v->width<=16384 && v->height<=16384 &&
        (v->bytes_per_pixel==2 || v->bytes_per_pixel==4) && bytes<=64u*1024u*1024u &&
        bytes==v->bytes && v->pixels;
}
static void image_hash(identity_sha *h,const rf_geomod_identity_image *v)
{
    name_hash(h,v->name);identity_sha_word(h,v->width);identity_sha_word(h,v->height);
    identity_sha_word(h,v->format);identity_sha_word(h,v->bytes_per_pixel);identity_sha_word(h,v->bytes);
    identity_sha_add(h,v->pixels,v->bytes);
}
static void filter_hash(identity_sha *h,const rf_collision_face_filter *f)
{
    identity_sha_word(h,f->query_flags);identity_sha_word(h,f->face_flags);
    identity_sha_word(h,(uint32_t)f->property_34);identity_sha_word(h,f->owner_present);
    identity_sha_word(h,f->owner_kind);identity_sha_word(h,f->owner_state);
}
static const rf_geomod_identity_material *material_find(const rf_geomod_authored_identity_input *v,uint32_t key)
{uint32_t i;for(i=0;i<v->material_count;i++)if(v->materials[i].compiled_material==key)return v->materials+i;return NULL;}
static const rf_geomod_identity_reference *reference_find(const rf_geomod_authored_identity_input *v,uint32_t key)
{uint32_t i;for(i=0;i<v->reference_count;i++)if(v->references[i].reference==key)return v->references+i;return NULL;}
static int reference_valid(const rf_geomod_identity_reference *r)
{
    uint32_t i;
    if(r->reference==UINT32_MAX || r->owner==UINT32_MAX || r->source_face==UINT32_MAX || r->unlit>1)return 0;
    if(r->unlit) {
        /* Unused semantic fields must be zero; do not inspect padding. */
        for(i=0;i<2;i++)if(r->projection.axes[i] || r->projection.scale[i]!=0 || r->projection.offset[i]!=0)return 0;
        return !r->chart.name[0] && !r->chart.width && !r->chart.height && !r->chart.format &&
            !r->chart.bytes_per_pixel && !r->chart.bytes && !r->chart.pixels;
    }
    if(!image_valid(&r->chart) || r->projection.axes[0]>2 || r->projection.axes[1]>2 ||
        r->projection.axes[0]==r->projection.axes[1])return 0;
    for(i=0;i<2;i++)if(!isfinite(r->projection.scale[i]) || !isfinite(r->projection.offset[i]))return 0;
    return 1;
}
static void reference_hash(identity_sha *h,const rf_geomod_identity_reference *r)
{
    uint32_t i;identity_sha_word(h,r->owner);identity_sha_word(h,r->source_face);
    filter_hash(h,&r->filter);identity_sha_word(h,r->unlit);
    if(!r->unlit) {
        for(i=0;i<2;i++)identity_sha_word(h,r->projection.axes[i]);
        for(i=0;i<2;i++)identity_sha_float(h,r->projection.scale[i]);
        for(i=0;i<2;i++)identity_sha_float(h,r->projection.offset[i]);
        image_hash(h,&r->chart);
    }
}
static int mesh_hash(identity_sha *h,const rf_geomod_authored_identity_input *v,
    const rf_geomod_mesh_view *mesh,const rf_geomod_publication_origin *origins,uint32_t kind)
{
    uint32_t i,j,k,next=0;
    if(kind==2 && v->source_mode==1 && !mesh->face_count && !mesh->vertex_count) {
        identity_sha_word(h,kind);identity_sha_word(h,0);identity_sha_word(h,0);return RF_OK;
    }
    if(!mesh->faces || !mesh->vertices || !origins || !mesh->face_count || mesh->face_count>768 ||
        !mesh->vertex_count || mesh->vertex_count>4096)return RF_RANGE;
    identity_sha_word(h,kind);identity_sha_word(h,mesh->face_count);identity_sha_word(h,mesh->vertex_count);
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *f=mesh->faces+i;const rf_geomod_publication_origin *o=origins+i;
        const rf_geomod_identity_material *m=material_find(v,f->material);
        const rf_geomod_identity_reference *r=NULL;
        if(f->first!=next || f->count<3 || f->count>64 || f->first>mesh->vertex_count ||
            f->count>mesh->vertex_count-f->first || !m || o->source_face==UINT32_MAX ||
            f->source_face!=o->source_face || o->owner==UINT32_MAX ||
            o->kind!=(kind==2?RF_GEOMOD_PUBLICATION_NEIGHBOR:RF_GEOMOD_PUBLICATION_RETAINED))return RF_FORMAT;
        if(kind<2 && o->owner!=v->asset->source_uid)return RF_FORMAT;
        if(kind==2) {
            for(j=0;j<v->asset->solid_count;j++)if(v->asset->solids[j].owner==o->owner)break;
            if(j==v->asset->solid_count)return RF_FORMAT;
        }
        if(o->reference!=UINT32_MAX) {
            r=reference_find(v,o->reference);
            if(!r || r->owner!=o->owner || r->source_face!=o->source_face || r->compiled_material!=f->material)return RF_FORMAT;
        } else if(kind==1)return RF_FORMAT;
        identity_sha_word(h,o->kind);identity_sha_word(h,o->owner);identity_sha_word(h,o->source_face);
        identity_sha_word(h,f->count);image_hash(h,&m->image);identity_sha_word(h,r?1:0);if(r)reference_hash(h,r);
        for(j=0;j<f->count;j++) {
            const rf_geomod_vertex *p=mesh->vertices+f->first+j;
            for(k=0;k<3;k++){if(!isfinite(p->position[k]))return RF_FORMAT;identity_sha_float(h,p->position[k]);}
            for(k=0;k<2;k++){if(!isfinite(p->uv[k]))return RF_FORMAT;identity_sha_float(h,p->uv[k]);}
        }
        next+=f->count;
    }
    return next==mesh->vertex_count?RF_OK:RF_FORMAT;
}
int rf_geomod_authored_identity(const rf_geomod_authored_identity_input *v,unsigned char out[32])
{
    identity_sha h;unsigned char digest[32];const rf_geomod_authored_post_view *a;uint32_t i,j,k;int status;
    if(!v || !out || !v->asset)return RF_RANGE;a=v->asset;
    if(!identity_sha_verified())return RF_FORMAT; /* Never publish a wrong save identity. */
    if(!v->compiled_section || !v->editor_section || !v->compiled_bytes || !v->editor_bytes ||
        v->compiled_bytes>64u*1024u*1024u || v->editor_bytes>64u*1024u*1024u ||
        !v->materials || !v->material_count || v->material_count>128 || !v->references ||
        !v->reference_count || v->reference_count>768 || (a->solid_count && !a->solids) || (!v->source_mode && !a->solid_count) || a->solid_count>32 ||
        !a->source_planes || !a->source_filters || !a->replaced_ids || !a->replaced_count || a->replaced_count>768 ||
        a->detail_guard_count>1 || (a->detail_guard_count && !a->detail_guards) ||
        a->neighbor_void_count>32 || (a->neighbor_void_count && !a->neighbor_voids))return RF_RANGE;
    if(!name_valid(v->level) || !name_valid(a->settings.texture) || v->source_mode>1 ||
        v->source_operation!=((v->source_mode && a->source_uid!=8123 && a->source_uid!=8219)?1u:2u) ||
        v->material_domain!=RF_GEOMOD_IDENTITY_COMPILED_MATERIALS || !v->loader_policy || !v->publication_policy ||
        !v->collision_policy || !v->material_policy || a->source_uid==UINT32_MAX || a->room==UINT32_MAX)return RF_FORMAT;
    if((a->detail_guard_count!=0)!=(v->loader_policy==5) ||
       (a->detail_guard_count && (v->publication_policy!=14 || v->source_mode || a->neighbor_void_count)))return RF_FORMAT;
    if(v->source_mode && (a->solid_count || a->neighbors.face_count || a->neighbors.vertex_count || a->neighbor_void_count))return RF_FORMAT;
    for(i=0;i<v->material_count;i++) {
        if(v->materials[i].compiled_material==UINT32_MAX || !image_valid(&v->materials[i].image))return RF_FORMAT;
        for(j=0;j<i;j++)if(v->materials[i].compiled_material==v->materials[j].compiled_material)return RF_FORMAT;
    }
    for(i=0;i<v->reference_count;i++) {
        if(!reference_valid(v->references+i) || !material_find(v,v->references[i].compiled_material))return RF_FORMAT;
        for(j=0;j<i;j++)if(v->references[i].reference==v->references[j].reference)return RF_FORMAT;
    }
    identity_sha_init(&h);identity_sha_add(&h,"RFAS",4);identity_sha_word(&h,a->detail_guard_count?3:a->neighbor_void_count?2:1);name_hash(&h,v->level);
    identity_sha_word(&h,v->compiled_bytes);identity_sha_add(&h,v->compiled_section,v->compiled_bytes);
    identity_sha_word(&h,v->editor_bytes);identity_sha_add(&h,v->editor_section,v->editor_bytes);
    identity_sha_word(&h,a->source_uid);identity_sha_word(&h,a->room);identity_sha_word(&h,v->source_operation);
    identity_sha_word(&h,v->source_mode);identity_sha_word(&h,v->loader_policy);identity_sha_word(&h,v->publication_policy);
    identity_sha_word(&h,v->collision_policy);identity_sha_word(&h,v->material_policy);
    identity_sha_word(&h,a->brush_count);identity_sha_word(&h,a->authored_face_count);
    name_hash(&h,a->settings.texture);identity_sha_word(&h,a->settings.hardness);
    status=mesh_hash(&h,v,&a->source,a->source_origins,0);if(status)return status;
    status=mesh_hash(&h,v,&a->windows,a->window_origins,1);if(status)return status;
    status=mesh_hash(&h,v,&a->neighbors,a->neighbor_origins,2);if(status)return status;
    for(i=0;i<a->source.face_count;i++) {
        for(j=0;j<4;j++){if(!isfinite(a->source_planes[i][j]))return RF_FORMAT;identity_sha_float(&h,a->source_planes[i][j]);}
        filter_hash(&h,a->source_filters+i);
        for(j=0;j<i;j++)if(a->source_origins[i].source_face==a->source_origins[j].source_face)return RF_FORMAT;
    }
    identity_sha_word(&h,a->solid_count);
    for(i=0;i<a->solid_count;i++) {
        const rf_geomod_publication_solid *s=a->solids+i;
        if(!s->planes || !s->count || s->count>32 || s->owner==UINT32_MAX || s->owner==a->source_uid)return RF_FORMAT;
        for(j=0;j<i;j++)if(s->owner==a->solids[j].owner)return RF_FORMAT;
        identity_sha_word(&h,s->owner);identity_sha_word(&h,s->count);
        for(j=0;j<s->count;j++)for(k=0;k<4;k++){if(!isfinite(s->planes[j][k]))return RF_FORMAT;identity_sha_float(&h,s->planes[j][k]);}
    }
    /* Preserve byte-for-byte v1 identities for existing solid-only sources. */
    if(a->neighbor_void_count) {
        identity_sha_add(&h,"VOID",4);identity_sha_word(&h,a->neighbor_void_count);
        for(i=0;i<a->neighbor_void_count;i++) {
            const rf_geomod_publication_solid *hole=a->neighbor_voids+i;
            uint32_t found=0;
            if(!hole->planes || !hole->count || hole->count>32)return RF_FORMAT;
            for(j=0;j<a->solid_count;j++)found|=a->solids[j].owner==hole->owner;
            if(!found)return RF_FORMAT;
            for(j=0;j<i;j++)if(a->neighbor_voids[j].owner==hole->owner)return RF_FORMAT;
            identity_sha_word(&h,hole->owner);identity_sha_word(&h,hole->count);
            for(j=0;j<hole->count;j++) {
                double norm=0;
                for(k=0;k<4;k++){if(!isfinite(hole->planes[j][k]))return RF_FORMAT;identity_sha_float(&h,hole->planes[j][k]);if(k<3)norm+=(double)hole->planes[j][k]*hole->planes[j][k];}
                if(fabs(norm-1)>1e-4)return RF_FORMAT;
            }
        }
    }
    if(a->detail_guard_count) {
        const rf_geomod_authored_detail_guard *g=a->detail_guards;
        uint32_t room,uid=rf_geomod_authored_post_detail(a->source_uid,&room);
        if(!uid || g->uid!=uid || g->room!=room || g->source_faces[0]==UINT32_MAX ||
           g->source_faces[1]==UINT32_MAX || g->source_faces[0]==g->source_faces[1])return RF_FORMAT;
        identity_sha_add(&h,"DGRD",4);identity_sha_word(&h,a->detail_guard_count);
        identity_sha_word(&h,g->uid);identity_sha_word(&h,g->room);
        for(i=0;i<2;i++)identity_sha_word(&h,g->source_faces[i]);
        for(i=0;i<3;i++) {
            if(!isfinite(g->minimum[i]) || !isfinite(g->maximum[i]) || g->minimum[i]>g->maximum[i])return RF_FORMAT;
            identity_sha_float(&h,g->minimum[i]);identity_sha_float(&h,g->maximum[i]);
        }
    }
    /* Suppression IDs are runtime lookup keys; hash stable window ownership
     * in declared replacement order instead, preserving multiplicity/splits. */
    identity_sha_word(&h,a->replaced_count);
    for(i=0;i<a->replaced_count;i++) {
        const rf_geomod_identity_reference *r=reference_find(v,a->replaced_ids[i]);uint32_t matches=0;
        if(!r || r->owner!=a->source_uid)return RF_FORMAT;
        for(j=0;j<i;j++)if(a->replaced_ids[i]==a->replaced_ids[j])return RF_FORMAT;
        for(j=0;j<a->windows.face_count;j++)matches+=a->window_origins[j].reference==a->replaced_ids[i];
        if(!matches)return RF_FORMAT;reference_hash(&h,r);identity_sha_word(&h,matches);
    }
    for(i=0;i<a->windows.face_count;i++) {
        for(j=0;j<a->replaced_count;j++)if(a->window_origins[i].reference==a->replaced_ids[j])break;
        if(j==a->replaced_count)return RF_FORMAT;
        for(j=0;j<a->source.face_count;j++)if(a->window_origins[i].source_face==a->source_origins[j].source_face)break;
        if(j==a->source.face_count)return RF_FORMAT;
        if(a->windows.faces[i].material!=a->source.faces[j].material)return RF_FORMAT;
    }
    identity_sha_end(&h,digest);memcpy(out,digest,32);return RF_OK;
}
