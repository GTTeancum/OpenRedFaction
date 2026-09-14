#include "rf/image.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"reduce line %u\n",(unsigned)__LINE__);return 1;}}while(0)
int main(void)
{
    rf_image image={4,2,32,7,0},saved;uint32_t x,y;unsigned char *p;
    CHECK(rf_image_allocate_pixels(&image)==RF_OK);
    for(y=0;y<2;y++)for(x=0;x<4;x++){p=rf_image_pixel(&image,x,y);p[0]=(unsigned char)(x*40);p[1]=(unsigned char)(y*80);p[2]=20;p[3]=(x&1)?255:0;}
    saved=image;CHECK(rf_image_reduce(&image,2,39)==RF_RANGE && !memcmp(&image,&saved,sizeof(image)));
    CHECK(rf_image_pixel(&image,3,1)[0]==120);
    CHECK(rf_image_reduce(&image,2,40)==RF_OK && image.width==2 && image.height==1 && image.bytes==8 && image.source_format==7);
    for(x=0;x<2;x++){p=rf_image_pixel(&image,x,0);CHECK(p[0]==20+80*x && p[1]==40 && p[2]==20 && p[3]==128);}
    saved=image;CHECK(rf_image_reduce(&image,2,8)==RF_OK && image.rgba==saved.rgba);
    CHECK(rf_image_reduce(&image,3,100)==RF_RANGE && !memcmp(&image,&saved,sizeof(image)));rf_image_close(&image);
    image=(rf_image){4,2,16,5,0};CHECK(rf_image_allocate_pixels(&image)==RF_OK);
    for(y=0;y<2;y++)for(x=0;x<4;x++){unsigned v=(x&1)?0x801f:0xfc00;p=rf_image_pixel(&image,x,y);p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);}
    CHECK(rf_image_reduce(&image,2,24)==RF_OK && image.bytes==8 && !rf_image_is_packed_1555(&image));
    p=rf_image_pixel(&image,0,0);CHECK(p[0]==128 && p[1]==0 && p[2]==128 && p[3]==255);rf_image_close(&image);
    image=(rf_image){1,4,16,6,0};CHECK(rf_image_allocate_pixels(&image)==RF_OK);
    for(y=0;y<4;y++)memset(rf_image_pixel(&image,0,y),(int)y*10,4);
    CHECK(rf_image_reduce(&image,2,24)==RF_OK && image.width==1 && image.height==2);
    CHECK(rf_image_pixel(&image,0,0)[0]==5 && rf_image_pixel(&image,0,1)[0]==25);rf_image_close(&image);rf_image_close(&image);
    puts("PASS RGBA/1555 box reduction, alpha, aspect ratio, no-op and transactional budget");return 0;
}
