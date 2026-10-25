/* DXT1/DXT5 block codec: gradients and flat blocks round-trip closely,
 * cutout pixels stay transparent, opaque DXT1 never decodes transparent,
 * and DXT5 keeps smooth alpha. */
#include "rf/dxt.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"dxt line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static double block_error(const unsigned char a[64],const unsigned char b[64],int alpha)
{
    double e=0;int i,k;
    for(i=0;i<16;i++)for(k=0;k<(alpha?4:3);k++){double d=(double)a[i*4+k]-b[i*4+k];e+=d*d;}
    return sqrt(e/(16.0*(alpha?4:3)));
}
int main(void)
{
    unsigned char src[64],dst[64],block[16];int i,trial;double worst=0,total=0;
    for(i=0;i<16;i++){src[i*4]=(unsigned char)(i*16);src[i*4+1]=(unsigned char)(255-i*16);src[i*4+2]=80;src[i*4+3]=255;}
    rf_dxt1_encode(src,0,block);rf_dxt1_decode(block,dst);CHECK(block_error(src,dst,0)<25); /* 16 steps in 4 levels */
    for(i=0;i<16;i++)CHECK(dst[i*4+3]==255);
    for(i=0;i<16;i++){src[i*4]=200;src[i*4+1]=40;src[i*4+2]=40;src[i*4+3]=255;}
    rf_dxt1_encode(src,0,block);rf_dxt1_decode(block,dst);CHECK(block_error(src,dst,0)<4);
    for(i=0;i<16;i++)src[i*4+3]=(unsigned char)(i&1?0:255);
    rf_dxt1_encode(src,1,block);rf_dxt1_decode(block,dst);
    for(i=0;i<16;i++)CHECK((dst[i*4+3]==0)==(src[i*4+3]==0));
    for(i=0;i<16;i++){src[i*4]=(unsigned char)(i*10);src[i*4+1]=(unsigned char)(i*5);src[i*4+2]=(unsigned char)(255-i*12);src[i*4+3]=(unsigned char)(i*17);}
    rf_dxt5_encode(src,block);rf_dxt5_decode(block,dst);CHECK(block_error(src,dst,1)<20);
    srand(7);
    for(trial=0;trial<2000;trial++) {
        /* Random two-colour-ish noisy blocks, as in real texture detail. */
        int base[3]={rand()%256,rand()%256,rand()%256},k;double e;
        for(i=0;i<16;i++)for(k=0;k<3;k++){int v=base[k]+(i%4)*(rand()%24)-(rand()%12);src[i*4+k]=(unsigned char)(v<0?0:v>255?255:v);}
        for(i=0;i<16;i++)src[i*4+3]=255;
        rf_dxt1_encode(src,0,block);rf_dxt1_decode(block,dst);
        for(i=0;i<16;i++)CHECK(dst[i*4+3]==255);
        e=block_error(src,dst,0);total+=e;if(e>worst)worst=e;
    }
    printf("PASS dxt: noisy blocks mean RMS %.2f worst %.2f\n",total/2000,worst);
    CHECK(total/2000<12); /* Regression bound for this synthetic noise. */
    return 0;
}
