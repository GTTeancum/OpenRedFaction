#include "rf/geomod_publication_digest.h"
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
    if(c->key==UINT32_MAX || c->owner==UINT32_MAX || c->kind>RF_GEOMOD_DIGEST_GENERATED_CHART)return 0;
    if(c->kind==RF_GEOMOD_DIGEST_GENERATED_CHART) {
        if(c->source_face!=UINT32_MAX || c->retained_map==UINT32_MAX)return 0;
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
        !v->mesh.face_count || v->mesh.face_count>768 || !v->mesh.vertex_count || v->mesh.vertex_count>4096 ||
        !v->materials || !v->material_count || v->material_count>128 || !v->charts || !v->chart_count || v->chart_count>768)return RF_RANGE;
    for(i=0;i<v->material_count;i++) {
        if(v->materials[i].key==UINT32_MAX || !resource_valid(&v->materials[i].image,v->materials[i].prehashed))return RF_FORMAT;
        for(j=0;j<i;j++)if(v->materials[i].key==v->materials[j].key)return RF_FORMAT;
    }
    for(i=0;i<v->chart_count;i++) {
        if(!chart_valid(v->charts+i))return RF_FORMAT;
        for(j=0;j<i;j++) {
            const rf_geomod_digest_chart *a=v->charts+i,*b=v->charts+j;
            if(a->key==b->key)return RF_FORMAT;
            if(a->kind==RF_GEOMOD_DIGEST_GENERATED_CHART && b->kind==RF_GEOMOD_DIGEST_GENERATED_CHART &&
                a->owner==b->owner && a->retained_map==b->retained_map)return RF_FORMAT;
        }
    }
    identity_sha_init(&h);identity_sha_add(&h,"RFAP",4);identity_sha_word(&h,1);
    identity_sha_word(&h,v->publication_policy);identity_sha_word(&h,v->material_policy);
    identity_sha_word(&h,v->mesh.face_count);identity_sha_word(&h,v->mesh.vertex_count);
    for(i=0;i<v->mesh.face_count;i++) {
        const rf_geomod_face *f=v->mesh.faces+i;const rf_geomod_publication_origin *o=v->origins+i;
        const rf_geomod_digest_material *m=NULL;const rf_geomod_digest_chart *c=NULL;uint32_t expected_source;unsigned char resource[32];
        if(f->first!=next || f->count<3 || f->count>64 || f->first>v->mesh.vertex_count ||
            f->count>v->mesh.vertex_count-f->first || o->kind>RF_GEOMOD_PUBLICATION_NEIGHBOR || o->owner==UINT32_MAX)return RF_FORMAT;
        if(o->kind==RF_GEOMOD_PUBLICATION_CRATER) {
            if(o->source_face!=UINT32_MAX)return RF_FORMAT;expected_source=UINT32_MAX;
        } else {
            if(o->source_face==UINT32_MAX || o->reference==UINT32_MAX)return RF_FORMAT;
            expected_source=v->source_domain==RF_GEOMOD_DIGEST_AUTHORED_SOURCE?o->source_face:o->reference;
        }
        if(f->source_face!=expected_source)return RF_FORMAT;
        for(j=0;j<v->material_count;j++)if(v->materials[j].key==f->material){m=v->materials+j;break;}
        for(j=0;j<v->chart_count;j++)if(v->charts[j].key==v->face_charts[i]){c=v->charts+j;break;}
        if(!m || !c || c->owner!=o->owner || c->source_face!=o->source_face)return RF_FORMAT;
        if((o->kind==RF_GEOMOD_PUBLICATION_CRATER)!=(c->kind==RF_GEOMOD_DIGEST_GENERATED_CHART))return RF_FORMAT;
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
