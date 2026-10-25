#include "rf/dxt.h"
#include <string.h>
#include <math.h>
/* Perceptual channel weights for the colour error (Rec. 601 luma). */
static const float dxt_weight[3]={.299f,.587f,.114f};
static uint16_t pack565(const float c[3])
{
    int r=(int)(c[0]*31.f/255.f+.5f),g=(int)(c[1]*63.f/255.f+.5f),b=(int)(c[2]*31.f/255.f+.5f);
    r=r<0?0:r>31?31:r;g=g<0?0:g>63?63:g;b=b<0?0:b>31?31:b;
    return (uint16_t)(r<<11|g<<5|b);
}
static void unpack565(uint16_t v,float c[3])
{
    int r=v>>11&31,g=v>>5&63,b=v&31;
    c[0]=(float)(r<<3|r>>2);c[1]=(float)(g<<2|g>>4);c[2]=(float)(b<<3|b>>2);
}
/* Palette for endpoints a,b in the given mode (4: c0>c1, 3: c0<=c1). */
static void palette(uint16_t a,uint16_t b,int four,float p[4][3])
{
    int k;unpack565(a,p[0]);unpack565(b,p[1]);
    for(k=0;k<3;k++) {
        if(four){p[2][k]=(2*p[0][k]+p[1][k])/3.f;p[3][k]=(p[0][k]+2*p[1][k])/3.f;}
        else {p[2][k]=(p[0][k]+p[1][k])*.5f;p[3][k]=0;}
    }
}
static float distance(const float a[3],const float b[3])
{
    float d=0;int k;
    for(k=0;k<3;k++){float e=a[k]-b[k];d+=dxt_weight[k]*e*e;}
    return d;
}
/* Choose indices for every used pixel; returns total error. */
static float assign(const float px[16][3],const uint8_t used[16],uint16_t a,uint16_t b,int four,uint8_t index[16])
{
    float p[4][3],total=0;int i,k,n=four?4:3;
    palette(a,b,four,p);
    for(i=0;i<16;i++) {
        float best=1e30f;int pick=0;
        if(!used[i]){index[i]=3;continue;}
        for(k=0;k<n;k++){float d=distance(px[i],p[k]);if(d<best){best=d;pick=k;}}
        index[i]=(uint8_t)pick;total+=best;
    }
    return total;
}
/* Least-squares endpoints for fixed indices (weights along the segment). */
static int refine(const float px[16][3],const uint8_t used[16],const uint8_t index[16],int four,float e0[3],float e1[3])
{
    static const float w4[4]={1,0,2.f/3.f,1.f/3.f},w3[3]={1,0,.5f};
    float aa=0,bb=0,ab=0,ax[3]={0},bx[3]={0},det;int i,k;
    for(i=0;i<16;i++) {
        float a,b;
        if(!used[i] || (!four && index[i]==3))continue;
        a=four?w4[index[i]]:w3[index[i]];b=1-a;
        aa+=a*a;bb+=b*b;ab+=a*b;
        for(k=0;k<3;k++){ax[k]+=a*px[i][k];bx[k]+=b*px[i][k];}
    }
    det=aa*bb-ab*ab;
    if(det<1e-6f && det>-1e-6f)return 0;
    for(k=0;k<3;k++) {
        e0[k]=(ax[k]*bb-bx[k]*ab)/det;e1[k]=(bx[k]*aa-ax[k]*ab)/det;
        e0[k]=e0[k]<0?0:e0[k]>255?255:e0[k];e1[k]=e1[k]<0?0:e1[k]>255?255:e1[k];
    }
    return 1;
}
static void store(uint16_t a,uint16_t b,const uint8_t index[16],uint8_t out[8])
{
    uint32_t bits=0;int i;
    for(i=0;i<16;i++)bits|=(uint32_t)index[i]<<(2*i);
    out[0]=(uint8_t)a;out[1]=(uint8_t)(a>>8);out[2]=(uint8_t)b;out[3]=(uint8_t)(b>>8);
    out[4]=(uint8_t)bits;out[5]=(uint8_t)(bits>>8);out[6]=(uint8_t)(bits>>16);out[7]=(uint8_t)(bits>>24);
}
/* Order endpoints for the mode: 4-colour needs a>b, 3-colour needs a<=b.
 * Swapping reverses the meaning of indices 0/1 and 2/3 (4) or 0/1 (3). */
static void order(uint16_t *a,uint16_t *b,int four,uint8_t index[16])
{
    int i;
    if(four ? *a>*b : *a<=*b)return;
    {uint16_t t=*a;*a=*b;*b=t;}
    for(i=0;i<16;i++) {
        if(four)index[i]^=1;
        else if(index[i]<2)index[i]^=1;
    }
}
static void colour_encode(const uint8_t rgba[64],int cutout,int force_four,uint8_t out[8])
{
    float px[16][3],mean[3]={0},cov[6]={0},axis[3]={1,1,1},lo=1e30f,hi=-1e30f,e0[3],e1[3],best_error;
    uint8_t used[16],index[16],best_index[16];int i,k,n=0,iter,transparent=0,four;
    uint16_t a,b,best_a,best_b;
    for(i=0;i<16;i++) {
        for(k=0;k<3;k++)px[i][k]=rgba[i*4+k];
        used[i]=!(cutout && rgba[i*4+3]<128);transparent|=!used[i];
        if(used[i]){for(k=0;k<3;k++)mean[k]+=px[i][k];n++;}
    }
    if(!n){memset(out,0,8);out[0]=0;out[2]=0xff;out[3]=0xff;for(i=4;i<8;i++)out[i]=0xff;return;}
    for(k=0;k<3;k++)mean[k]/=(float)n;
    for(i=0;i<16;i++)if(used[i]) {
        float d[3];for(k=0;k<3;k++)d[k]=px[i][k]-mean[k];
        cov[0]+=d[0]*d[0];cov[1]+=d[0]*d[1];cov[2]+=d[0]*d[2];cov[3]+=d[1]*d[1];cov[4]+=d[1]*d[2];cov[5]+=d[2]*d[2];
    }
    /* Principal axis by power iteration. */
    for(iter=0;iter<8;iter++) {
        float x=cov[0]*axis[0]+cov[1]*axis[1]+cov[2]*axis[2];
        float y=cov[1]*axis[0]+cov[3]*axis[1]+cov[4]*axis[2];
        float z=cov[2]*axis[0]+cov[4]*axis[1]+cov[5]*axis[2];
        float m=x*x+y*y+z*z;
        if(m<1e-12f)break;
        {float s=1.f/sqrtf(m);axis[0]=x*s;axis[1]=y*s;axis[2]=z*s;}
    }
    for(i=0;i<16;i++)if(used[i]) {
        float t=0;for(k=0;k<3;k++)t+=(px[i][k]-mean[k])*axis[k];
        if(t<lo)lo=t;if(t>hi)hi=t;
    }
    for(k=0;k<3;k++){e0[k]=mean[k]+axis[k]*hi;e1[k]=mean[k]+axis[k]*lo;}
    /* Opaque blocks try both modes (3-colour can win on two-tone blocks);
     * blocks with cutout pixels must use 3-colour. */
    best_error=1e30f;best_a=best_b=0;memset(best_index,0,16);
    for(four=transparent?0:1;four>=(force_four?1:0);four--) {
        float c0[3],c1[3];memcpy(c0,e0,12);memcpy(c1,e1,12);
        for(iter=0;iter<3;iter++) {
            float error;
            a=pack565(c0);b=pack565(c1);
            if(a==b && four){if(b>0)b--;else a++;}
            error=assign(px,used,a,b,four,index);
            if(error<best_error) {
                uint8_t ordered[16];uint16_t oa=a,ob=b;memcpy(ordered,index,16);
                order(&oa,&ob,four,ordered);
                if(four ? oa>ob : oa<=ob){best_error=error;best_a=oa;best_b=ob;memcpy(best_index,ordered,16);}
            }
            if(!refine(px,used,index,four,c0,c1))break;
        }
        if(transparent)break;
    }
    store(best_a,best_b,best_index,out);
}
void rf_dxt5_encode(const uint8_t rgba[64],uint8_t out[16])
{
    uint8_t lo=255,hi=0,index[16];uint64_t bits=0;int i,k;
    for(i=0;i<16;i++){uint8_t v=rgba[i*4+3];if(v<lo)lo=v;if(v>hi)hi=v;}
    if(hi==lo){memset(index,0,16);}
    else for(i=0;i<16;i++) {
        /* 8-alpha mode: a0=hi > a1=lo; index 0=a0, 1=a1, 2..7 interpolate. */
        int v=rgba[i*4+3],best=0,best_error=1<<30;
        for(k=0;k<8;k++) {
            int value=k==0?hi:k==1?lo:((8-k)*hi+(k-1)*lo)/7,e=value-v;e*=e;
            if(e<best_error){best_error=e;best=k;}
        }
        index[i]=(uint8_t)best;
    }
    out[0]=hi;out[1]=lo;
    for(i=0;i<16;i++)bits|=(uint64_t)index[i]<<(3*i);
    for(i=0;i<6;i++)out[2+i]=(uint8_t)(bits>>(8*i));
    /* DXT5 colour always decodes as 4-colour. */
    colour_encode(rgba,0,1,out+8);
}
void rf_dxt1_encode(const uint8_t rgba[64],int cutout,uint8_t out[8])
{colour_encode(rgba,cutout,0,out);}
void rf_dxt1_decode(const uint8_t in[8],uint8_t rgba[64])
{
    uint16_t a=(uint16_t)(in[0]|in[1]<<8),b=(uint16_t)(in[2]|in[3]<<8);float p[4][3];
    uint32_t bits=(uint32_t)in[4]|in[5]<<8|in[6]<<16|(uint32_t)in[7]<<24;int i,k,four=a>b;
    palette(a,b,four,p);
    for(i=0;i<16;i++) {
        uint32_t v=bits>>(2*i)&3;
        for(k=0;k<3;k++)rgba[i*4+k]=(uint8_t)(p[v][k]+.5f);
        rgba[i*4+3]=(uint8_t)(!four && v==3?0:255);
    }
}
void rf_dxt5_decode(const uint8_t in[16],uint8_t rgba[64])
{
    uint8_t a0=in[0],a1=in[1],values[8];uint64_t bits=0;int i;
    values[0]=a0;values[1]=a1;
    for(i=2;i<8;i++)values[i]=a0>a1?(uint8_t)(((8-i)*a0+(i-1)*a1)/7):(i<6?(uint8_t)(((6-i)*a0+(i-1)*a1)/5):(i==6?0:255));
    for(i=0;i<6;i++)bits|=(uint64_t)in[2+i]<<(8*i);
    {uint8_t colour[16];memcpy(colour,in+8,8);
     /* DXT5 colour is always 4-colour regardless of endpoint order. */
     uint16_t a=(uint16_t)(colour[0]|colour[1]<<8),b=(uint16_t)(colour[2]|colour[3]<<8);float p[4][3];
     uint32_t cb=(uint32_t)colour[4]|colour[5]<<8|colour[6]<<16|(uint32_t)colour[7]<<24;int k;
     palette(a,b,1,p);
     for(i=0;i<16;i++){uint32_t v=cb>>(2*i)&3;for(k=0;k<3;k++)rgba[i*4+k]=(uint8_t)(p[v][k]+.5f);}}
    for(i=0;i<16;i++)rgba[i*4+3]=values[bits>>(3*i)&7];
}
