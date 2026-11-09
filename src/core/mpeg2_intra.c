#include "rf/mpeg2_intra.h"
#include "rf/vpp.h"
#include <stdlib.h>
#include <string.h>

/* Intra-only MPEG-2 decoder (see rf/mpeg2_intra.h). Section numbers refer to
 * ISO/IEC 13818-2: 6.2 syntax, 7.2 VLC decoding, 7.3 scan, 7.4 inverse
 * quantisation (with 7.4.4 mismatch control), Annex A the IDCT definition.
 * The IDCT is the Loeffler-Ligtenberg-Moschytz factorisation in 13-bit fixed
 * point; it meets Annex A's accuracy class for 8-bit output. */

typedef struct mpeg2_code {uint16_t code;uint8_t length,value,level;} mpeg2_code;
#include "mpeg2_tables.inc"

#define MPEG2_EOB 63u
#define MPEG2_ESCAPE 62u
typedef struct mpeg2_entry {uint8_t length,value,level;} mpeg2_entry;
/* B.1 (11-bit index), B.12/B.13 (10-bit), DCT codes of up to 8 bits by
 * their top byte and longer ones (all begin with six zeros) by bits 6-15. */
static mpeg2_entry table_b1[2048],table_b12[1024],table_b13[1024];
static mpeg2_entry dct_short[2][256],dct_long[2][1024];
static int tables_ready;

static void table_fill(mpeg2_entry *table,uint32_t bits,const mpeg2_code *codes,uint32_t count)
{
    uint32_t i,k;
    for(i=0;i<count;i++){
        uint32_t shift=bits-codes[i].length,base=(uint32_t)codes[i].code<<shift;
        for(k=0;k<(1u<<shift);k++){
            table[base+k].length=codes[i].length;table[base+k].value=codes[i].value;table[base+k].level=codes[i].level;
        }
    }
}
static void table_fill_dct(uint32_t which,const mpeg2_code *codes,uint32_t count)
{
    uint32_t i,k;
    for(i=0;i<count;i++){
        uint32_t length=codes[i].length,base,shift;mpeg2_entry *t;
        if(length>8){shift=16-length;base=((uint32_t)codes[i].code<<shift)&0x3ffu;t=dct_long[which];}
        else{shift=8-length;base=(uint32_t)codes[i].code<<shift;t=dct_short[which];}
        for(k=0;k<(1u<<shift);k++){t[base+k].length=(uint8_t)length;t[base+k].value=codes[i].value;t[base+k].level=codes[i].level;}
    }
}
static void tables_build(void)
{
    if(tables_ready)return;
    table_fill(table_b1,11,mpeg2_b1,sizeof(mpeg2_b1)/sizeof(mpeg2_b1[0]));
    table_fill(table_b12,10,mpeg2_b12,sizeof(mpeg2_b12)/sizeof(mpeg2_b12[0]));
    table_fill(table_b13,10,mpeg2_b13,sizeof(mpeg2_b13)/sizeof(mpeg2_b13[0]));
    table_fill_dct(0,mpeg2_b14,sizeof(mpeg2_b14)/sizeof(mpeg2_b14[0]));
    table_fill_dct(1,mpeg2_b15,sizeof(mpeg2_b15)/sizeof(mpeg2_b15[0]));
    tables_ready=1;
}

/* 7.3.1 zig-zag scan: scan position -> raster position. */
static const uint8_t zigzag[64]={0,1,8,16,9,2,3,10,17,24,32,25,18,11,4,5,12,19,26,33,40,48,41,34,27,20,13,6,7,14,21,28,
    35,42,49,56,57,50,43,36,29,22,15,23,30,37,44,51,58,59,52,45,38,31,39,46,53,60,61,54,47,55,62,63};
/* 6.3.11 default intra quantiser matrix, raster order. */
static const uint8_t default_intra[64]={8,16,19,22,26,27,29,34,16,16,22,24,27,29,34,37,19,22,26,27,29,34,34,38,
    22,22,26,27,29,34,37,40,22,26,27,29,32,35,40,48,26,27,29,32,35,40,48,58,26,27,29,34,38,46,56,69,27,29,35,38,46,56,69,83};
/* Table 7-6 non-linear quantiser_scale. */
static const uint8_t nonlinear_scale[32]={0,1,2,3,4,5,6,7,8,10,12,14,16,18,20,22,24,28,32,36,40,44,48,52,56,64,72,80,88,96,104,112};

/* Bit reader over a bounded range; past the end it reads zeros. */
typedef struct mpeg2_bits {const uint8_t *p,*end;uint64_t cache;int32_t count,over;} mpeg2_bits;
static void bits_refill(mpeg2_bits *b)
{
    while(b->count<=56){uint64_t v=0;if(b->p<b->end)v=*b->p++;else ++b->over;b->cache|=v<<(56-b->count);b->count+=8;}
}
static void bits_init(mpeg2_bits *b,const uint8_t *p,const uint8_t *end){b->p=p;b->end=end;b->cache=0;b->count=0;b->over=0;bits_refill(b);}
static uint32_t bits_peek(mpeg2_bits *b,uint32_t n){return (uint32_t)(b->cache>>(64-n));}
static void bits_skip(mpeg2_bits *b,uint32_t n){b->cache<<=n;b->count-=(int32_t)n;if(b->count<32)bits_refill(b);}
static uint32_t bits_get(mpeg2_bits *b,uint32_t n){uint32_t v=bits_peek(b,n);bits_skip(b,n);return v;}
/* Bits left before the end (negative once reads run past it). */
static int64_t bits_left(const mpeg2_bits *b)
{return ((int64_t)(b->end-b->p))*8+b->count-(int64_t)b->over*8;}

int rf_mpeg2_intra_open(rf_mpeg2_intra *d,uint32_t max_width,uint32_t max_height)
{
    uint32_t w,h;
    if(!d || !max_width || !max_height || max_width>4096 || max_height>4096)return RF_RANGE;
    memset(d,0,sizeof(*d));tables_build();
    w=(max_width+15)&~15u;h=(max_height+31)&~31u;
    d->planes[0]=malloc(w*h);d->planes[1]=malloc(w/2*h/2);d->planes[2]=malloc(w/2*h/2);
    if(!d->planes[0] || !d->planes[1] || !d->planes[2]){rf_mpeg2_intra_close(d);return RF_RANGE;}
    d->stride[0]=w;d->stride[1]=d->stride[2]=w/2;d->capacity_width=w;d->capacity_height=h;
    memcpy(d->intra_matrix,default_intra,64);
    return RF_OK;
}
void rf_mpeg2_intra_close(rf_mpeg2_intra *d)
{
    if(!d)return;
    free(d->planes[0]);free(d->planes[1]);free(d->planes[2]);
    memset(d,0,sizeof(*d));
}

/* Annex A IDCT, separable, LLM factorisation; output clamped to 0..255. */
#define IDCT_BITS 13
#define IDCT_PASS 2
#define C_0_298631336 2446
#define C_0_390180644 3196
#define C_0_541196100 4433
#define C_0_765366865 6270
#define C_0_899976223 7373
#define C_1_175875602 9633
#define C_1_501321110 12299
#define C_1_847759065 15137
#define C_1_961570560 16069
#define C_2_053119869 16819
#define C_2_562915447 20995
#define C_3_072711026 25172
#define DESCALE(x,n) (((x)+(1<<((n)-1)))>>(n))
static void idct_put(const int16_t in[64],uint8_t *out,uint32_t stride)
{
    int32_t work[64];uint32_t i;
    for(i=0;i<8;i++){
        const int16_t *c=in+i;int32_t *w=work+i;
        int32_t t0,t1,t2,t3,t10,t11,t12,t13,z1,z2,z3,z4,z5;
        if(!c[8] && !c[16] && !c[24] && !c[32] && !c[40] && !c[48] && !c[56]){
            int32_t dc=c[0]*(1<<IDCT_PASS);
            w[0]=w[8]=w[16]=w[24]=w[32]=w[40]=w[48]=w[56]=dc;continue;
        }
        z2=c[16];z3=c[48];z1=(z2+z3)*C_0_541196100;
        t2=z1-z3*C_1_847759065;t3=z1+z2*C_0_765366865;
        t0=(c[0]+c[32])*(1<<IDCT_BITS);t1=(c[0]-c[32])*(1<<IDCT_BITS);
        t10=t0+t3;t13=t0-t3;t11=t1+t2;t12=t1-t2;
        t0=c[56];t1=c[40];t2=c[24];t3=c[8];
        z1=t0+t3;z2=t1+t2;z3=t0+t2;z4=t1+t3;z5=(z3+z4)*C_1_175875602;
        t0*=C_0_298631336;t1*=C_2_053119869;t2*=C_3_072711026;t3*=C_1_501321110;
        z1*=-C_0_899976223;z2*=-C_2_562915447;z3*=-C_1_961570560;z4*=-C_0_390180644;
        z3+=z5;z4+=z5;t0+=z1+z3;t1+=z2+z4;t2+=z2+z3;t3+=z1+z4;
        w[0]=DESCALE(t10+t3,IDCT_BITS-IDCT_PASS);w[56]=DESCALE(t10-t3,IDCT_BITS-IDCT_PASS);
        w[8]=DESCALE(t11+t2,IDCT_BITS-IDCT_PASS);w[48]=DESCALE(t11-t2,IDCT_BITS-IDCT_PASS);
        w[16]=DESCALE(t12+t1,IDCT_BITS-IDCT_PASS);w[40]=DESCALE(t12-t1,IDCT_BITS-IDCT_PASS);
        w[24]=DESCALE(t13+t0,IDCT_BITS-IDCT_PASS);w[32]=DESCALE(t13-t0,IDCT_BITS-IDCT_PASS);
    }
    for(i=0;i<8;i++){
        const int32_t *r=work+i*8;uint8_t *o=out+i*stride;
        int32_t t0,t1,t2,t3,t10,t11,t12,t13,z1,z2,z3,z4,z5,v[8],k;
        z2=r[2];z3=r[6];z1=(z2+z3)*C_0_541196100;
        t2=z1-z3*C_1_847759065;t3=z1+z2*C_0_765366865;
        t0=(r[0]+r[4])*(1<<IDCT_BITS);t1=(r[0]-r[4])*(1<<IDCT_BITS);
        t10=t0+t3;t13=t0-t3;t11=t1+t2;t12=t1-t2;
        t0=r[7];t1=r[5];t2=r[3];t3=r[1];
        z1=t0+t3;z2=t1+t2;z3=t0+t2;z4=t1+t3;z5=(z3+z4)*C_1_175875602;
        t0*=C_0_298631336;t1*=C_2_053119869;t2*=C_3_072711026;t3*=C_1_501321110;
        z1*=-C_0_899976223;z2*=-C_2_562915447;z3*=-C_1_961570560;z4*=-C_0_390180644;
        z3+=z5;z4+=z5;t0+=z1+z3;t1+=z2+z4;t2+=z2+z3;t3+=z1+z4;
        v[0]=DESCALE(t10+t3,IDCT_BITS+IDCT_PASS+3);v[7]=DESCALE(t10-t3,IDCT_BITS+IDCT_PASS+3);
        v[1]=DESCALE(t11+t2,IDCT_BITS+IDCT_PASS+3);v[6]=DESCALE(t11-t2,IDCT_BITS+IDCT_PASS+3);
        v[2]=DESCALE(t12+t1,IDCT_BITS+IDCT_PASS+3);v[5]=DESCALE(t12-t1,IDCT_BITS+IDCT_PASS+3);
        v[3]=DESCALE(t13+t0,IDCT_BITS+IDCT_PASS+3);v[4]=DESCALE(t13-t0,IDCT_BITS+IDCT_PASS+3);
        for(k=0;k<8;k++)o[k]=(uint8_t)(v[k]<0?0:v[k]>255?255:v[k]);
    }
}

/* One intra block (7.2.1 DC, 7.2.2 AC, 7.4 inverse quantisation). */
static int block_decode(rf_mpeg2_intra *d,mpeg2_bits *b,int16_t out[64],uint32_t chroma,int32_t *predictor,uint32_t scale)
{
    const mpeg2_entry *dc=chroma?&table_b13[bits_peek(b,10)]:&table_b12[bits_peek(b,10)];
    uint32_t size,i=0,which=d->intra_vlc_format;int32_t sum,value;
    if(!dc->length)return RF_FORMAT;
    bits_skip(b,dc->length);size=dc->value;
    if(size){
        int32_t diff=(int32_t)bits_get(b,size);
        if(diff<(1<<(size-1)))diff=diff-(1<<size)+1;
        *predictor+=diff;
    }
    memset(out,0,64*sizeof(*out));
    value=*predictor*(int32_t)(8u>>d->dc_precision);
    out[0]=(int16_t)value;sum=value;
    for(;;){
        uint32_t peek=bits_peek(b,16),run;int32_t level;const mpeg2_entry *e;
        e=(peek>>10)?&dct_short[which][peek>>8]:&dct_long[which][peek&0x3ffu];
        if(!e->length)return RF_FORMAT;
        bits_skip(b,e->length);
        if(e->value==MPEG2_EOB)break;
        if(e->value==MPEG2_ESCAPE){
            run=bits_get(b,6);level=(int32_t)bits_get(b,12);
            if(level>=2048)level-=4096;
            if(!level || level==-2048)return RF_FORMAT;
        }else{run=e->value;level=e->level;if(bits_get(b,1))level=-level;}
        i+=run+1;if(i>63)return RF_FORMAT;
        {
            uint32_t pos=zigzag[i];
            value=(level*2*(int32_t)scale*(int32_t)d->intra_matrix[pos])/32;
            if(value>2047)value=2047;else if(value<-2048)value=-2048;
            out[pos]=(int16_t)value;sum+=value;
        }
    }
    if(!(sum&1))out[63]^=1;
    return RF_OK;
}

static int slice_decode(rf_mpeg2_intra *d,const uint8_t *data,const uint8_t *end,uint32_t row)
{
    mpeg2_bits b;int32_t predictor[3];uint32_t code,address,scale,first=1;int16_t block[64];
    bits_init(&b,data,end);
    if(row>=d->mb_height)return RF_FORMAT;
    code=bits_get(&b,5);scale=d->q_scale_type?nonlinear_scale[code]:code*2;
    while(bits_get(&b,1))bits_skip(&b,8); /* extra_information_slice */
    predictor[0]=predictor[1]=predictor[2]=1<<(7+d->dc_precision);
    address=row*d->mb_width-1u;
    for(;;){
        uint32_t increment=0,x,y,field=0,k;
        if(bits_left(&b)<=0 || !bits_peek(&b,23))break; /* next start code or end */
        for(;;){
            const mpeg2_entry *e=&table_b1[bits_peek(&b,11)];
            if(!e->length)return RF_FORMAT;
            bits_skip(&b,e->length);
            if(e->value==34){increment+=33;continue;}
            increment+=e->value;break;
        }
        /* I pictures skip no macroblocks: only a slice's first jumps. */
        if(increment!=1 && !first)return RF_FORMAT;
        address+=increment;first=0;
        if(address>=d->mb_width*d->mb_height)return RF_FORMAT;
        /* Table B.2 (I pictures): 1 intra, 01 intra + quantiser scale. */
        if(!bits_get(&b,1)){
            if(!bits_get(&b,1))return RF_FORMAT;
            code=bits_get(&b,5);scale=d->q_scale_type?nonlinear_scale[code]:code*2;
        }
        if(!d->frame_pred_frame_dct)field=bits_get(&b,1); /* dct_type */
        x=(address%d->mb_width)*16;y=(address/d->mb_width)*16;
        for(k=0;k<4;k++){
            uint8_t *p;uint32_t stride=d->stride[0];
            int status=block_decode(d,&b,block,0,&predictor[0],scale);if(status)return status;
            if(field)p=d->planes[0]+(y+(k>>1))*stride+x+(k&1)*8,stride*=2;
            else p=d->planes[0]+(y+(k>>1)*8)*stride+x+(k&1)*8;
            idct_put(block,p,stride);
        }
        for(k=1;k<3;k++){
            int status=block_decode(d,&b,block,1,&predictor[k],scale);if(status)return status;
            idct_put(block,d->planes[k]+(y/2)*d->stride[k]+x/2,d->stride[k]);
        }
        if(bits_left(&b)<0)return RF_FORMAT;
    }
    return RF_OK;
}

/* Next start code at or after p: its address, or end. */
static const uint8_t *start_code(const uint8_t *p,const uint8_t *end)
{
    while(p+3<end){
        if(!p[0] && !p[1] && p[2]==1)return p;
        ++p;
    }
    return end;
}

int rf_mpeg2_intra_decode(rf_mpeg2_intra *d,const uint8_t *unit,uint32_t bytes,uint32_t *decoded)
{
    const uint8_t *end=unit+bytes,*p;uint32_t picture=0,refused=0,slices=0;
    if(!d || !d->planes[0] || !unit || !decoded)return RF_RANGE;
    *decoded=0;
    for(p=start_code(unit,end);p<end;){
        const uint8_t *next=start_code(p+4,end),*body=p+4;uint32_t code=p[3];mpeg2_bits b;
        if(p+4>end)break;
        bits_init(&b,body,next);
        if(code==0xb3){ /* 6.2.2.1 sequence header */
            uint32_t w=bits_get(&b,12),h=bits_get(&b,12),i;
            bits_skip(&b,4+4+18+1+10+1);
            if(bits_get(&b,1)){for(i=0;i<64;i++)d->intra_matrix[zigzag[i]]=(uint8_t)bits_get(&b,8);}
            else memcpy(d->intra_matrix,default_intra,64);
            if(!w || !h)return RF_FORMAT;
            d->width=w;d->height=h;d->sequence=1;d->extension=0;
        }else if(code==0xb5){ /* 6.2.2.2 / 6.2.3.1 extensions */
            uint32_t id=bits_get(&b,4);
            if(id==1){ /* sequence extension */
                uint32_t progressive,chroma;
                bits_skip(&b,8);progressive=bits_get(&b,1);chroma=bits_get(&b,2);
                d->width|=bits_get(&b,2)<<12;d->height|=bits_get(&b,2)<<12;
                if(chroma!=1)return RF_FORMAT;
                d->extension=1;d->mb_width=(d->width+15)/16;
                d->mb_height=progressive?(d->height+15)/16:2*((d->height+31)/32);
                if(d->mb_width*16>d->capacity_width || d->mb_height*16>d->capacity_height)return RF_RANGE;
            }else if(id==3){ /* quant matrix extension */
                uint32_t i;
                if(bits_get(&b,1)){for(i=0;i<64;i++)d->intra_matrix[zigzag[i]]=(uint8_t)bits_get(&b,8);}
            }else if(id==8 && picture){ /* picture coding extension */
                bits_skip(&b,16);
                d->dc_precision=bits_get(&b,2);d->picture_structure=bits_get(&b,2);
                bits_skip(&b,1);d->frame_pred_frame_dct=bits_get(&b,1);bits_skip(&b,1);
                d->q_scale_type=bits_get(&b,1);d->intra_vlc_format=bits_get(&b,1);d->alternate_scan=bits_get(&b,1);
                if(d->picture_structure!=3 || d->alternate_scan)refused=1;
            }
        }else if(code==0x00){ /* 6.2.3 picture header */
            uint32_t type;
            bits_skip(&b,10);type=bits_get(&b,3);
            if(picture)break; /* one picture per unit */
            picture=1;if(type!=1)refused=1;
        }else if(code>=0x01 && code<=0xaf){
            if(!picture || refused)goto next_code;
            if(!d->sequence || !d->extension)return RF_FORMAT;
            if(slice_decode(d,body,next,code-1u))++d->errors;
            ++slices;
        }
    next_code:
        p=next;
    }
    if(picture && refused)++d->skipped;
    if(picture && !refused && slices){++d->pictures;*decoded=1;}
    return RF_OK;
}
