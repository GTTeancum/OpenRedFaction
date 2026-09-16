#include "rf/geomod_authored_identity.h"
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
    if(!v->compiled_section || !v->editor_section || !v->compiled_bytes || !v->editor_bytes ||
        v->compiled_bytes>64u*1024u*1024u || v->editor_bytes>64u*1024u*1024u ||
        !v->materials || !v->material_count || v->material_count>128 || !v->references ||
        !v->reference_count || v->reference_count>768 || !a->solids || !a->solid_count || a->solid_count>32 ||
        !a->source_planes || !a->source_filters || !a->replaced_ids || !a->replaced_count || a->replaced_count>768)return RF_RANGE;
    if(!name_valid(v->level) || !name_valid(a->settings.texture) || v->source_mode || v->source_operation!=2 ||
        v->material_domain!=RF_GEOMOD_IDENTITY_COMPILED_MATERIALS || !v->loader_policy || !v->publication_policy ||
        !v->collision_policy || !v->material_policy || a->source_uid==UINT32_MAX || a->room==UINT32_MAX)return RF_FORMAT;
    for(i=0;i<v->material_count;i++) {
        if(v->materials[i].compiled_material==UINT32_MAX || !image_valid(&v->materials[i].image))return RF_FORMAT;
        for(j=0;j<i;j++)if(v->materials[i].compiled_material==v->materials[j].compiled_material)return RF_FORMAT;
    }
    for(i=0;i<v->reference_count;i++) {
        if(!reference_valid(v->references+i) || !material_find(v,v->references[i].compiled_material))return RF_FORMAT;
        for(j=0;j<i;j++)if(v->references[i].reference==v->references[j].reference)return RF_FORMAT;
    }
    identity_sha_init(&h);identity_sha_add(&h,"RFAS",4);identity_sha_word(&h,1);name_hash(&h,v->level);
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
