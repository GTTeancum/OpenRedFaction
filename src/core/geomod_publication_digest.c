#include "rf/geomod_publication_digest.h"
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
static int resource_valid(const rf_geomod_identity_image *image,uint32_t prehashed)
{return prehashed<=1 && image_descriptor_valid(image) && (prehashed?!image->pixels:!!image->pixels);}
static void resource_digest(const rf_geomod_identity_image *image,uint32_t prehashed,const unsigned char stored[32],unsigned char out[32])
{
    identity_sha h;if(prehashed){memcpy(out,stored,32);return;}
    identity_sha_init(&h);identity_sha_add(&h,"RFCM",4);identity_sha_word(&h,1);image_hash(&h,image);identity_sha_end(&h,out);
}
int rf_geomod_image_content_digest(const rf_geomod_identity_image *image,unsigned char out[32])
{
    unsigned char value[32];if(!image || !out)return RF_RANGE;
    if(!resource_valid(image,0))return RF_FORMAT;resource_digest(image,0,NULL,value);memcpy(out,value,32);return RF_OK;
}
static int chart_valid(const rf_geomod_digest_chart *c)
{
    uint32_t i;
    if(c->key==UINT32_MAX || c->owner==UINT32_MAX || c->kind>RF_GEOMOD_DIGEST_AUTHORED_CHART)return 0;
    if(c->kind==RF_GEOMOD_DIGEST_GENERATED_CHART || c->kind==RF_GEOMOD_DIGEST_AUTHORED_CHART) {
        if((c->kind==RF_GEOMOD_DIGEST_GENERATED_CHART)!=(c->source_face==UINT32_MAX) || c->retained_map==UINT32_MAX)return 0;
        if(c->image.width<4 || c->image.width>64 || c->image.height<4 || c->image.height>64 ||
            c->image.format!=5 || c->image.bytes_per_pixel!=2)return 0;
    } else if(c->source_face==UINT32_MAX || c->retained_map!=UINT32_MAX)return 0;
    if(c->kind==RF_GEOMOD_DIGEST_UNLIT) {
        if(c->prehashed)return 0;
        for(i=0;i<32;i++)if(c->content_digest[i])return 0;
        for(i=0;i<2;i++)if(c->projection.axes[i] || c->projection.scale[i]!=0 || c->projection.offset[i]!=0)return 0;
        return !c->image.name[0] && !c->image.width && !c->image.height && !c->image.format &&
            !c->image.bytes_per_pixel && !c->image.bytes && !c->image.pixels;
    }
    if(!resource_valid(&c->image,c->prehashed) || c->projection.axes[0]>2 || c->projection.axes[1]>2 ||
        c->projection.axes[0]==c->projection.axes[1])return 0;
    for(i=0;i<2;i++)if(!isfinite(c->projection.scale[i]) || !isfinite(c->projection.offset[i]))return 0;
    return 1;
}
static void chart_hash(identity_sha *h,const rf_geomod_digest_chart *c)
{
    uint32_t i;unsigned char digest[32];identity_sha_word(h,c->kind);identity_sha_word(h,c->owner);
    identity_sha_word(h,c->source_face);identity_sha_word(h,c->retained_map);
    if(c->kind!=RF_GEOMOD_DIGEST_UNLIT) {
        for(i=0;i<2;i++)identity_sha_word(h,c->projection.axes[i]);
        for(i=0;i<2;i++)identity_sha_float(h,c->projection.scale[i]);
        for(i=0;i<2;i++)identity_sha_float(h,c->projection.offset[i]);
        resource_digest(&c->image,c->prehashed,c->content_digest,digest);identity_sha_add(h,digest,32);
    }
}
int rf_geomod_publication_digest(const rf_geomod_publication_digest_input *v,unsigned char out[32])
{
    identity_sha h;unsigned char digest[32];uint32_t i,j,k,next=0;
    if(!v || !out)return RF_RANGE;
    if(v->source_domain>RF_GEOMOD_DIGEST_COMPILED_SOURCE || v->publication_policy!=1 || v->material_policy!=1)return RF_FORMAT;
    /* Reset has no replacement publication; immutable source identity and
     * original full-room collision digest are separate required domains. */
    if(!v->mesh.face_count && !v->mesh.vertex_count) {
        if(v->material_count || v->chart_count)return RF_FORMAT;
        identity_sha_init(&h);identity_sha_add(&h,"RFAP",4);identity_sha_word(&h,1);
        identity_sha_word(&h,v->publication_policy);identity_sha_word(&h,v->material_policy);
        identity_sha_word(&h,0);identity_sha_word(&h,0);identity_sha_end(&h,digest);memcpy(out,digest,32);return RF_OK;
    }
    if(!v->mesh.vertices || !v->mesh.faces || !v->origins || !v->face_charts ||
        !v->mesh.face_count || v->mesh.face_count>RF_GEOMOD_PUBLICATION_FACES || !v->mesh.vertex_count || v->mesh.vertex_count>RF_GEOMOD_PUBLICATION_VERTICES ||
        !v->materials || !v->material_count || v->material_count>128 || !v->charts || !v->chart_count || v->chart_count>RF_GEOMOD_PUBLICATION_FACES)return RF_RANGE;
    for(i=0;i<v->material_count;i++) {
        if(v->materials[i].key==UINT32_MAX || !resource_valid(&v->materials[i].image,v->materials[i].prehashed))return RF_FORMAT;
        for(j=0;j<i;j++)if(v->materials[i].key==v->materials[j].key)return RF_FORMAT;
    }
    for(i=0;i<v->chart_count;i++) {
        if(!chart_valid(v->charts+i))return RF_FORMAT;
        for(j=0;j<i;j++) {
            const rf_geomod_digest_chart *a=v->charts+i,*b=v->charts+j;
            if(a->key==b->key)return RF_FORMAT;
            if(a->kind>=RF_GEOMOD_DIGEST_GENERATED_CHART && b->kind>=RF_GEOMOD_DIGEST_GENERATED_CHART &&
                a->owner==b->owner && a->retained_map==b->retained_map)return RF_FORMAT;
        }
    }
    identity_sha_init(&h);identity_sha_add(&h,"RFAP",4);identity_sha_word(&h,1);
    identity_sha_word(&h,v->publication_policy);identity_sha_word(&h,v->material_policy);
    identity_sha_word(&h,v->mesh.face_count);identity_sha_word(&h,v->mesh.vertex_count);
    for(i=0;i<v->mesh.face_count;i++) {
        const rf_geomod_face *f=v->mesh.faces+i;const rf_geomod_publication_origin *o=v->origins+i;
        const rf_geomod_digest_material *m=NULL;const rf_geomod_digest_chart *c=NULL;uint32_t expected_source,hidden;unsigned char resource[32];
        if(f->first!=next || f->count<3 || f->count>64 || f->first>v->mesh.vertex_count ||
            f->count>v->mesh.vertex_count-f->first || o->kind>RF_GEOMOD_PUBLICATION_NEIGHBOR || o->owner==UINT32_MAX)return RF_FORMAT;
        hidden=o->kind==RF_GEOMOD_PUBLICATION_NEIGHBOR && o->reference==UINT32_MAX;
        if(o->kind==RF_GEOMOD_PUBLICATION_CRATER) {
            if(o->source_face!=UINT32_MAX)return RF_FORMAT;expected_source=UINT32_MAX;
        } else {
            if(o->source_face==UINT32_MAX || (o->reference==UINT32_MAX && !hidden))return RF_FORMAT;
            expected_source=v->source_domain==RF_GEOMOD_DIGEST_AUTHORED_SOURCE?o->source_face:o->reference;
        }
        if(f->source_face!=expected_source)return RF_FORMAT;
        for(j=0;j<v->material_count;j++)if(v->materials[j].key==f->material){m=v->materials+j;break;}
        for(j=0;j<v->chart_count;j++)if(v->charts[j].key==v->face_charts[i]){c=v->charts+j;break;}
        if(!m || !c || c->owner!=o->owner || c->source_face!=o->source_face)return RF_FORMAT;
        if((o->kind==RF_GEOMOD_PUBLICATION_CRATER)!=(c->kind==RF_GEOMOD_DIGEST_GENERATED_CHART))return RF_FORMAT;
        if(hidden!=(c->kind==RF_GEOMOD_DIGEST_AUTHORED_CHART))return RF_FORMAT;
        identity_sha_word(&h,o->kind);identity_sha_word(&h,o->owner);identity_sha_word(&h,o->source_face);
        identity_sha_word(&h,f->count);resource_digest(&m->image,m->prehashed,m->content_digest,resource);
        identity_sha_add(&h,resource,32);chart_hash(&h,c);
        for(j=0;j<f->count;j++) {
            const rf_geomod_vertex *p=v->mesh.vertices+f->first+j;
            for(k=0;k<3;k++){if(!isfinite(p->position[k]))return RF_FORMAT;identity_sha_float(&h,p->position[k]);}
            for(k=0;k<2;k++){if(!isfinite(p->uv[k]))return RF_FORMAT;identity_sha_float(&h,p->uv[k]);}
        }
        next+=f->count;
    }
    if(next!=v->mesh.vertex_count)return RF_FORMAT;
    identity_sha_end(&h,digest);memcpy(out,digest,32);return RF_OK;
}
