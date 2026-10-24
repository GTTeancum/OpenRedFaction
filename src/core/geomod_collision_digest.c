#include "rf/geomod_collision_digest.h"
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
static int image_descriptor_valid(const rf_geomod_identity_image *v)
{
    uint64_t bytes=(uint64_t)v->width*v->height*v->bytes_per_pixel;
    return name_valid(v->name) && v->width && v->height && v->width<=16384 && v->height<=16384 &&
        (v->bytes_per_pixel==2 || v->bytes_per_pixel==4) && bytes<=64u*1024u*1024u &&
        bytes==v->bytes;
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
int rf_geomod_collision_digest(const rf_geomod_collision_digest_input *v,unsigned char out[32])
{
    unsigned char materials[128][32],digest[32];identity_sha h;const rf_collision_tree *tree;
    uint32_t n,i,j,k,canonical,published=0,last_reference=0,have_original=0;
    if(!v || !out || !v->composition || !v->rows || !v->source_identity ||
        (v->material_count && !v->materials) || v->material_count>128)return RF_RANGE;
    tree=v->composition->tree;n=v->row_count;
    if(!tree || !tree->faces || !tree->source_indices || !v->composition->face_ids || !n || n>8192 ||
        n!=tree->face_count || n!=v->composition->count)return RF_RANGE;
    if(v->room==UINT32_MAX || v->collision_policy!=1)return RF_FORMAT;
    for(i=0;i<v->material_count;i++) {
        const rf_geomod_digest_material *m=v->materials+i;
        if(m->key==UINT32_MAX || m->prehashed>1 || !image_descriptor_valid(&m->image) ||
            (m->prehashed?!!m->image.pixels:!m->image.pixels))return RF_FORMAT;
        for(j=0;j<i;j++)if(v->materials[i].key==v->materials[j].key)return RF_FORMAT;
        if(m->prehashed)memcpy(materials[i],m->content_digest,32);
        else {identity_sha_init(&h);identity_sha_add(&h,"RFCM",4);identity_sha_word(&h,1);
            image_hash(&h,&m->image);identity_sha_end(&h,materials[i]);}
    }
    for(i=0;i<n;i++) {
        const rf_geomod_collision_digest_row *r=v->rows+i;
        uint32_t hidden=r->domain==RF_GEOMOD_COLLISION_PUBLISHED && r->origin.kind==RF_GEOMOD_PUBLICATION_NEIGHBOR && r->origin.reference==UINT32_MAX;
        uint32_t id=hidden?r->metadata_id:r->origin.reference;
        if(tree->source_indices[i]>=n || r->canonical_order>=n || r->domain>RF_GEOMOD_COLLISION_PUBLISHED ||
            id==UINT32_MAX || id!=v->composition->face_ids[i] ||
            r->origin.owner==UINT32_MAX || r->fragment==UINT32_MAX || r->origin.kind>RF_GEOMOD_PUBLICATION_NEIGHBOR)return RF_FORMAT;
        if(r->domain!=RF_GEOMOD_COLLISION_PUBLISHED) {
            if(r->origin.kind!=RF_GEOMOD_PUBLICATION_RETAINED || r->origin.source_face==UINT32_MAX)return RF_FORMAT;
            if(r->domain==RF_GEOMOD_COLLISION_COMPILED && (r->origin.owner!=v->room ||
                r->origin.source_face!=r->origin.reference || r->fragment))return RF_FORMAT;
        } else if((r->origin.kind==RF_GEOMOD_PUBLICATION_CRATER)!=(r->origin.source_face==UINT32_MAX))return RF_FORMAT;
        for(j=0;j<i;j++) {
            const rf_geomod_collision_digest_row *p=v->rows+j;
            uint32_t other_hidden=p->domain==RF_GEOMOD_COLLISION_PUBLISHED && p->origin.kind==RF_GEOMOD_PUBLICATION_NEIGHBOR && p->origin.reference==UINT32_MAX;
            uint32_t other_id=other_hidden?p->metadata_id:p->origin.reference;
            if(id==other_id && (hidden || other_hidden) &&
                (!hidden || !other_hidden || r->origin.owner!=p->origin.owner || r->origin.source_face!=p->origin.source_face || r->material!=p->material))return RF_FORMAT;
            if(tree->source_indices[j]==tree->source_indices[i] || p->canonical_order==r->canonical_order)return RF_FORMAT;
            if(p->domain==r->domain && p->origin.kind==r->origin.kind && p->origin.owner==r->origin.owner &&
                p->origin.source_face==r->origin.source_face && p->fragment==r->fragment)return RF_FORMAT;
        }
    }
    identity_sha_init(&h);identity_sha_add(&h,"RFAC",4);identity_sha_word(&h,1);
    identity_sha_word(&h,v->collision_policy);identity_sha_add(&h,v->source_identity,32);
    identity_sha_word(&h,v->room);identity_sha_word(&h,n);
    for(canonical=0;canonical<n;canonical++) {
        const rf_geomod_collision_digest_row *r;const rf_collision_face *f;uint32_t source,material;
        for(source=0;source<n;source++)if(v->rows[source].canonical_order==canonical)break;
        if(source==n)return RF_FORMAT;r=v->rows+source;
        for(i=0;i<n;i++)if(tree->source_indices[i]==source)break;
        if(i==n)return RF_FORMAT;f=tree->faces+i;
        if(!f->vertices || f->count<3 || f->count>256)return RF_FORMAT;
        if(r->domain==RF_GEOMOD_COLLISION_PUBLISHED) {
            if(r->fragment!=published)return RF_FORMAT;published++;
        } else {
            if(published || (have_original && r->origin.reference<=last_reference))return RF_FORMAT;
            last_reference=r->origin.reference;have_original=1;
        }
        material=0;
        if(r->domain==RF_GEOMOD_COLLISION_COMPILED) {if(r->material==UINT32_MAX)return RF_FORMAT;}
        else {
            for(material=0;material<v->material_count;material++)if(v->materials[material].key==r->material)break;
            if(material==v->material_count)return RF_FORMAT;
        }
        identity_sha_word(&h,r->domain);identity_sha_word(&h,r->origin.kind);identity_sha_word(&h,r->origin.owner);
        identity_sha_word(&h,r->origin.source_face);identity_sha_word(&h,r->fragment);
        if(r->domain==RF_GEOMOD_COLLISION_COMPILED){identity_sha_add(&h,"RFCI",4);identity_sha_word(&h,r->material);}
        else identity_sha_add(&h,materials[material],32);
        identity_sha_word(&h,f->count);identity_sha_word(&h,f->triangle_surface);
        filter_hash(&h,&f->filter);
        for(j=0;j<4;j++){if(!isfinite(f->plane[j]))return RF_FORMAT;identity_sha_float(&h,f->plane[j]);}
        for(j=0;j<3;j++)if(!isfinite(f->minimum[j]) || !isfinite(f->maximum[j]) || f->minimum[j]>f->maximum[j])return RF_FORMAT;
        for(j=0;j<3;j++)identity_sha_float(&h,f->minimum[j]);
        for(j=0;j<3;j++)identity_sha_float(&h,f->maximum[j]);
        for(j=0;j<f->count;j++)for(k=0;k<3;k++) {
            if(!isfinite(f->vertices[j][k]))return RF_FORMAT;identity_sha_float(&h,f->vertices[j][k]);
        }
    }
    identity_sha_end(&h,digest);memcpy(out,digest,32);return RF_OK;
}
