#include "rf/geomod_retained_material_digest.h"
#include "rf/geomod_limits.h"
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
static int map_validate(const rf_geomod_retained_material_map *m)
{
    uint32_t j,axis=0,dims[2];float span[2],density[2]={4,4},adjusted[2];double length=0;int status;
    if(m->material_token || m->width<3 || m->width>64 || m->height<3 || m->height>64 ||
        m->x>512-m->width || m->y>512-m->height)return RF_FORMAT;
    for(j=0;j<4;j++)if(!isfinite(m->plane[j]))return RF_FORMAT;
    for(j=0;j<3;j++) {
        if(!isfinite(m->minimum[j]) || !isfinite(m->maximum[j]) || m->minimum[j]>m->maximum[j])return RF_FORMAT;
        length+=(double)m->plane[j]*m->plane[j];
    }
    if(fabs(length-1)>1e-4)return RF_FORMAT;
    for(j=1;j<3;j++)if(fabsf(m->plane[j])>fabsf(m->plane[axis]))axis=j;
    for(j=0;j<2;j++) {
        uint32_t a=(axis+j+1)%3;double scale;float expected_scale,expected_offset;
        if(m->projection.axes[j]!=a)return RF_FORMAT;
        span[j]=m->maximum[a]-m->minimum[a];if(!isfinite(span[j]) || !(span[j]>0))return RF_FORMAT;
        scale=((j?m->height:m->width)-2)/(double)span[j];
        expected_scale=(float)(scale/512);expected_offset=(float)(((j?m->y:m->x)+1-m->minimum[a]*scale)/512);
        if(!isfinite(m->projection.scale[j]) || m->projection.scale[j]<=0 || !isfinite(m->projection.offset[j]) ||
            m->projection.scale[j]!=expected_scale || m->projection.offset[j]!=expected_offset)return RF_FORMAT;
    }
    status=rf_geomod_lightmap_size(span,density,0,dims,adjusted);if(status)return status;
    return dims[0]==m->width && dims[1]==m->height?RF_OK:RF_FORMAT;
}
int rf_geomod_retained_material_digest(const rf_geomod_retained_material_input *v,unsigned char out[32])
{
    identity_sha h;unsigned char digest[32],rgb[64*3],packed[64*2];rf_random_state chain={1};
    uint32_t i,j,x=0,y=0,row=0;int status;
    if(!v || !out || !v->source_identity || !v->substrate_identity || v->map_count>RF_GEOMOD_LIGHTMAP_LIMIT || v->face_count>RF_GEOMOD_PUBLICATION_FACES ||
        (v->map_count && !v->maps) || (v->face_count && (!v->origins || !v->face_maps)))return RF_RANGE;
    if(v->material_policy!=1 || v->owner==UINT32_MAX || v->serial==UINT32_MAX || v->cuts>RF_GEOMOD_CUT_LIMIT || v->cuts>v->serial ||
        v->baked!=v->map_count || v->sample || v->owner_cuts!=v->cuts)return RF_FORMAT;
    if(v->owner_generation!=v->serial && (v->owner_generation || v->cuts || v->map_count))return RF_FORMAT;
    if(!v->cuts && v->map_count)return RF_FORMAT; /* Authored reset clears its journal. */
    if(!v->map_count && !v->cuts) {
        if(v->random>1 || (v->owner_generation && !v->random))return RF_FORMAT;
        chain.value=v->random;
    }
    identity_sha_init(&h);identity_sha_add(&h,"RFRM",4);identity_sha_word(&h,1);identity_sha_word(&h,v->material_policy);
    identity_sha_add(&h,v->source_identity,32);identity_sha_add(&h,v->substrate_identity,32);
    identity_sha_word(&h,v->owner);identity_sha_word(&h,v->serial);identity_sha_word(&h,v->cuts);
    identity_sha_word(&h,v->owner_generation);identity_sha_word(&h,v->owner_cuts);
    identity_sha_word(&h,v->map_count);identity_sha_word(&h,v->baked);identity_sha_word(&h,v->sample);
    identity_sha_word(&h,v->random);identity_sha_word(&h,v->x);identity_sha_word(&h,v->y);identity_sha_word(&h,v->row);
    identity_sha_word(&h,v->face_count);
    for(i=0;i<v->map_count;i++) {
        const rf_geomod_retained_material_map *m=v->maps+i;uint32_t remaining;
        status=map_validate(m);if(status)return status;
        if(x+m->width>512){y+=row;x=row=0;}
        if(y+m->height>512 || m->x!=x || m->y!=y || m->base_seed!=chain.value)return RF_FORMAT;
        x+=m->width;if(m->height>row)row=m->height;
        for(j=0;j<4;j++)identity_sha_float(&h,m->plane[j]);
        for(j=0;j<3;j++)identity_sha_float(&h,m->minimum[j]);
        for(j=0;j<3;j++)identity_sha_float(&h,m->maximum[j]);
        identity_sha_word(&h,m->material_token);identity_sha_word(&h,m->x);identity_sha_word(&h,m->y);
        identity_sha_word(&h,m->width);identity_sha_word(&h,m->height);identity_sha_word(&h,m->base_seed);
        for(j=0;j<2;j++)identity_sha_word(&h,m->projection.axes[j]);
        for(j=0;j<2;j++)identity_sha_float(&h,m->projection.scale[j]);
        for(j=0;j<2;j++)identity_sha_float(&h,m->projection.offset[j]);
        /* Hash base pixels in local row-major order, never dynamic atlas bytes.
         * Each pixel advances one original RNG draw, including unused maps. */
        remaining=m->width*m->height;identity_sha_word(&h,remaining*2);
        while(remaining) {
            uint32_t n=remaining>64?64:remaining;
            status=rf_geomod_light_noise(rgb,sizeof(rgb),n*3,n,1,&chain);if(status)return status;
            status=rf_lightmap_pack_1555(rgb,n*3,n,1,0,packed,n*2,sizeof(packed));if(status)return status;
            identity_sha_add(&h,packed,n*2);remaining-=n;
        }
    }
    if(x!=v->x || y!=v->y || row!=v->row || chain.value!=v->random)return RF_FORMAT;
    for(i=0;i<v->face_count;i++) {
        const rf_geomod_publication_origin *o=v->origins+i;uint32_t map=v->face_maps[i];
        if(o->owner==UINT32_MAX || o->kind>RF_GEOMOD_PUBLICATION_NEIGHBOR)return RF_FORMAT;
        if(o->kind==RF_GEOMOD_PUBLICATION_CRATER) {
            if(o->owner!=v->owner || o->source_face!=UINT32_MAX || map>=v->map_count)return RF_FORMAT;
        } else if(o->source_face==UINT32_MAX || map!=65535)return RF_FORMAT;
        identity_sha_word(&h,o->kind);identity_sha_word(&h,o->owner);identity_sha_word(&h,o->source_face);identity_sha_word(&h,map);
    }
    identity_sha_end(&h,digest);memcpy(out,digest,32);return RF_OK;
}
