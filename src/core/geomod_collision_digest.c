#include "rf/geomod_collision_digest.h"
#include <math.h>
#include <string.h>
/* Existing scene checkpoint SHA256 implementation, isolated unchanged here. */
static void identity_put(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static void identity_put_float(unsigned char *p,float f)
{uint32_t w;memcpy(&w,&f,4);identity_put(p,w);}
typedef struct identity_sha {uint32_t h[8],used;uint64_t bytes;unsigned char block[64];} identity_sha;
static uint32_t identity_rotr(uint32_t v,uint32_t n){return (v>>n)|(v<<(32-n));}
static void identity_sha_block(identity_sha *s)
{
    static const uint32_t k[64]={
        0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
        0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
        0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
        0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
        0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
        0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
        0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
        0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2};
    uint32_t w[64],i,a=s->h[0],b=s->h[1],c=s->h[2],d=s->h[3],e=s->h[4],f=s->h[5],g=s->h[6],h=s->h[7];
    for(i=0;i<16;i++){const unsigned char *p=s->block+i*4;w[i]=((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|p[3];}
    for(i=16;i<64;i++)w[i]=w[i-16]+(identity_rotr(w[i-15],7)^identity_rotr(w[i-15],18)^(w[i-15]>>3))+w[i-7]+(identity_rotr(w[i-2],17)^identity_rotr(w[i-2],19)^(w[i-2]>>10));
    for(i=0;i<64;i++) {
        uint32_t t=h+(identity_rotr(e,6)^identity_rotr(e,11)^identity_rotr(e,25))+((e&f)^(~e&g))+k[i]+w[i];
        uint32_t u=(identity_rotr(a,2)^identity_rotr(a,13)^identity_rotr(a,22))+((a&b)^(a&c)^(b&c));
        h=g;g=f;f=e;e=d+t;d=c;c=b;b=a;a=t+u;
    }
    s->h[0]+=a;s->h[1]+=b;s->h[2]+=c;s->h[3]+=d;s->h[4]+=e;s->h[5]+=f;s->h[6]+=g;s->h[7]+=h;
}
static void identity_sha_init(identity_sha *s)
{
    static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memset(s,0,sizeof(*s));memcpy(s->h,initial,sizeof(initial));
}
static void identity_sha_add(identity_sha *s,const void *data,uint32_t bytes)
{
    const unsigned char *p=data;s->bytes+=bytes;
    while(bytes){uint32_t n=64-s->used;if(n>bytes)n=bytes;memcpy(s->block+s->used,p,n);s->used+=n;p+=n;bytes-=n;if(s->used==64){identity_sha_block(s);s->used=0;}}
}
static void identity_sha_word(identity_sha *s,uint32_t w)
{unsigned char p[4];identity_put(p,w);identity_sha_add(s,p,4);}
static void identity_sha_float(identity_sha *s,float f)
{unsigned char p[4];identity_put_float(p,f);identity_sha_add(s,p,4);}
static void identity_sha_end(identity_sha *s,unsigned char out[32])
{
    uint64_t bits=s->bytes*8;unsigned char p[8];uint32_t i;
    p[0]=128;identity_sha_add(s,p,1);p[0]=0;
    while(s->used!=56)identity_sha_add(s,p,1);
    for(i=0;i<8;i++)p[i]=(unsigned char)(bits>>(56-i*8));identity_sha_add(s,p,8);
    for(i=0;i<8;i++){out[i*4]=(unsigned char)(s->h[i]>>24);out[i*4+1]=(unsigned char)(s->h[i]>>16);out[i*4+2]=(unsigned char)(s->h[i]>>8);out[i*4+3]=(unsigned char)s->h[i];}
}
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
