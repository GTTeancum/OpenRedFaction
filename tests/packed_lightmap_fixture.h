#include "rf/scene_preview.h"
static int packed_lightmap_fixture(rf_image *image,rf_particle_draw_vertex vertices[4])
{
    uint32_t x,y,j;int status;
    memset(image,0,sizeof(*image));image->width=32;image->height=2;image->bytes=128;image->source_format=5;
    status=rf_image_allocate_pixels(image);if(status)return status;
    for(y=0;y<2;++y)for(x=0;x<32;++x){uint32_t value=(y?0:0x8000)|(x<<10)|((31-x)<<5)|(x^15);
        unsigned char *pixel=rf_image_pixel(image,x,y);pixel[0]=(unsigned char)value;pixel[1]=(unsigned char)(value>>8);}
    memset(vertices,0,4*sizeof(*vertices));
    for(j=0;j<4;++j){vertices[j].screen[0]=64+((j==1 || j==2)?512:0);vertices[j].screen[1]=64+(j>=2?128:0);
        vertices[j].depth=1000;vertices[j].reciprocal_w=1;vertices[j].argb=0xffffffff;vertices[j].fog=0xff000000;
        vertices[j].uv[0]=(j==1 || j==2)?1:0;vertices[j].uv[1]=j>=2?1:0;}
    return RF_OK;
}
