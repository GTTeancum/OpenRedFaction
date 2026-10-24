/* Shared SHA-256 (FIPS 180-4) for GeoMod identity and digest translation units.
 * Replaces four identical private copies of the scene checkpoint loop. Output
 * is byte-identical; only throughput changes. Whole 64-byte blocks are
 * compressed directly from the caller's buffer (no staging copy), the message
 * schedule lives in a 16-word ring, and the 64 rounds are unrolled with
 * register renaming instead of eight moves per round. Ch/Maj use the standard
 * equivalent reduced forms: g^(e&(f^g)) and (a&b)|(c&(a|b)).
 * Measured cause (L1S2 late-load stage 60->61): the authored identity hashes
 * each face's full 256x256 RGBA material and 128x128 RGBA chart, about 60 MB
 * for the UID8123/UID8219 pair, on a 733 MHz CPU. Include only from .c files. */
#ifndef RF_GEOMOD_IDENTITY_SHA_H
#define RF_GEOMOD_IDENTITY_SHA_H
#include <stddef.h>
#include <stdint.h>
#include <string.h>
static inline void identity_put(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static inline void identity_put_float(unsigned char *p,float f)
{uint32_t w;memcpy(&w,&f,4);identity_put(p,w);}
typedef struct identity_sha {uint32_t h[8],used;uint64_t bytes;unsigned char block[64];} identity_sha;
static inline uint32_t identity_sha_load(const unsigned char *p)
{
#if defined(__GNUC__) && defined(__BYTE_ORDER__) && defined(__ORDER_LITTLE_ENDIAN__) && __BYTE_ORDER__==__ORDER_LITTLE_ENDIAN__
    uint32_t v;memcpy(&v,p,4);return __builtin_bswap32(v);
#else
    return ((uint32_t)p[0]<<24)|((uint32_t)p[1]<<16)|((uint32_t)p[2]<<8)|(uint32_t)p[3];
#endif
}
#define IDENTITY_SHA_ROTR(v,n) (((v)>>(n))|((v)<<(32-(n))))
#define IDENTITY_SHA_ROUND(a,b,c,d,e,f,g,h,i) do { \
        uint32_t t1_=(h)+(IDENTITY_SHA_ROTR(e,6)^IDENTITY_SHA_ROTR(e,11)^IDENTITY_SHA_ROTR(e,25))+ \
            ((g)^((e)&((f)^(g))))+k[i]+w[(i)&15]; \
        uint32_t t2_=(IDENTITY_SHA_ROTR(a,2)^IDENTITY_SHA_ROTR(a,13)^IDENTITY_SHA_ROTR(a,22))+ \
            (((a)&(b))|((c)&((a)|(b)))); \
        (d)+=t1_;(h)=t1_+t2_; \
    } while(0)
/* Ring slot i&15 holds W[i-16] until it is replaced by W[i]. */
#define IDENTITY_SHA_EXPAND(i) \
    (w[(i)&15]+=(IDENTITY_SHA_ROTR(w[((i)-2)&15],17)^IDENTITY_SHA_ROTR(w[((i)-2)&15],19)^(w[((i)-2)&15]>>10))+ \
        w[((i)-7)&15]+(IDENTITY_SHA_ROTR(w[((i)-15)&15],7)^IDENTITY_SHA_ROTR(w[((i)-15)&15],18)^(w[((i)-15)&15]>>3)))
#define IDENTITY_SHA_ROUNDX(a,b,c,d,e,f,g,h,i) do {IDENTITY_SHA_EXPAND(i);IDENTITY_SHA_ROUND(a,b,c,d,e,f,g,h,i);} while(0)
/* Eight rounds return every role to its original variable. */
#define IDENTITY_SHA_R8(i,X) \
    X(a,b,c,d,e,f,g,h,(i)+0);X(h,a,b,c,d,e,f,g,(i)+1);X(g,h,a,b,c,d,e,f,(i)+2);X(f,g,h,a,b,c,d,e,(i)+3); \
    X(e,f,g,h,a,b,c,d,(i)+4);X(d,e,f,g,h,a,b,c,(i)+5);X(c,d,e,f,g,h,a,b,(i)+6);X(b,c,d,e,f,g,h,a,(i)+7)
static inline void identity_sha_blocks(uint32_t state[8],const unsigned char *p,size_t blocks)
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
    while(blocks--) {
        uint32_t w[16],a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        unsigned n;
        for(n=0;n<16;n++)w[n]=identity_sha_load(p+n*4);
        IDENTITY_SHA_R8(0,IDENTITY_SHA_ROUND);IDENTITY_SHA_R8(8,IDENTITY_SHA_ROUND);
        IDENTITY_SHA_R8(16,IDENTITY_SHA_ROUNDX);IDENTITY_SHA_R8(24,IDENTITY_SHA_ROUNDX);
        IDENTITY_SHA_R8(32,IDENTITY_SHA_ROUNDX);IDENTITY_SHA_R8(40,IDENTITY_SHA_ROUNDX);
        IDENTITY_SHA_R8(48,IDENTITY_SHA_ROUNDX);IDENTITY_SHA_R8(56,IDENTITY_SHA_ROUNDX);
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
        p+=64;
    }
}
#undef IDENTITY_SHA_R8
#undef IDENTITY_SHA_ROUNDX
#undef IDENTITY_SHA_EXPAND
#undef IDENTITY_SHA_ROUND
#undef IDENTITY_SHA_ROTR
static inline void identity_sha_init(identity_sha *s)
{
    static const uint32_t initial[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    memset(s,0,sizeof(*s));memcpy(s->h,initial,sizeof(initial));
}
static inline void identity_sha_add(identity_sha *s,const void *data,uint32_t bytes)
{
    const unsigned char *p=data;s->bytes+=bytes;
    if(s->used) {
        uint32_t n=64-s->used;if(n>bytes)n=bytes;
        memcpy(s->block+s->used,p,n);s->used+=n;p+=n;bytes-=n;
        if(s->used<64)return;
        identity_sha_blocks(s->h,s->block,1);s->used=0;
    }
    if(bytes>=64){uint32_t n=bytes/64;identity_sha_blocks(s->h,p,n);p+=(size_t)n*64;bytes-=n*64;}
    if(bytes){memcpy(s->block,p,bytes);s->used=bytes;}
}
static inline void identity_sha_word(identity_sha *s,uint32_t w)
{unsigned char p[4];identity_put(p,w);identity_sha_add(s,p,4);}
static inline void identity_sha_float(identity_sha *s,float f)
{unsigned char p[4];identity_put_float(p,f);identity_sha_add(s,p,4);}
static inline void identity_sha_end(identity_sha *s,unsigned char out[32])
{
    uint64_t bits=s->bytes*8;unsigned char p[8];uint32_t i;
    p[0]=128;identity_sha_add(s,p,1);p[0]=0;
    while(s->used!=56)identity_sha_add(s,p,1);
    for(i=0;i<8;i++)p[i]=(unsigned char)(bits>>(56-i*8));identity_sha_add(s,p,8);
    for(i=0;i<8;i++){out[i*4]=(unsigned char)(s->h[i]>>24);out[i*4+1]=(unsigned char)(s->h[i]>>16);out[i*4+2]=(unsigned char)(s->h[i]>>8);out[i*4+3]=(unsigned char)s->h[i];}
}
/* FIPS 180-2 known answers through both the staged and direct-block paths.
 * A failing build must reject authored identities rather than invalidate
 * saves silently. Evaluated once per translation unit. */
static inline int identity_sha_verified(void)
{
    static int state; /* 0 unchecked, 1 good, 2 failed */
    static const unsigned char abc[32]={0xba,0x78,0x16,0xbf,0x8f,0x01,0xcf,0xea,0x41,0x41,0x40,0xde,0x5d,0xae,0x22,0x23,
        0xb0,0x03,0x61,0xa3,0x96,0x17,0x7a,0x9c,0xb4,0x10,0xff,0x61,0xf2,0x00,0x15,0xad};
    static const unsigned char long_digest[32]={0xcf,0x5b,0x16,0xa7,0x78,0xaf,0x83,0x80,0x03,0x6c,0xe5,0x9e,0x7b,0x04,0x92,0x37,
        0x0b,0x24,0x9b,0x11,0xe8,0xf0,0x7a,0x51,0xaf,0xac,0x45,0x03,0x7a,0xfe,0xe9,0xd1};
    static const char message[]="abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnopjklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu";
    identity_sha h;unsigned char out[32];int good;
    if(state)return state==1;
    identity_sha_init(&h);identity_sha_add(&h,"abc",3);identity_sha_end(&h,out);good=!memcmp(out,abc,32);
    identity_sha_init(&h);identity_sha_add(&h,message,112);identity_sha_end(&h,out);good=good && !memcmp(out,long_digest,32);
    identity_sha_init(&h);identity_sha_add(&h,message,1);identity_sha_add(&h,message+1,111);identity_sha_end(&h,out);
    good=good && !memcmp(out,long_digest,32);
    state=good?1:2;return good;
}
#endif
