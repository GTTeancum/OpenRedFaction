#ifndef RF_CORONA_PIXEL_FIXTURE_H
#define RF_CORONA_PIXEL_FIXTURE_H
#include "rf/effect.h"
#include <string.h>
static const unsigned char corona_texels[16]={128,0,0,128,0,128,0,128,0,0,128,128,128,128,128,128};
static void corona_pixel_fixture(uint32_t index,rf_particle_draw_vertex v[4])
{
    static const float uv[6]={-.25f,0,.25f,.75f,1,1.25f};uint32_t j;
    memset(v,0,4*sizeof(*v));
    for(j=0;j<4;++j){v[j].screen[0]=32+((j==1 || j==2)?80:0);v[j].screen[1]=48+(j>=2?80:0);
        v[j].depth=16777215;v[j].reciprocal_w=1;v[j].argb=0x80ffffff;v[j].fog=0;
        v[j].uv[0]=uv[index%6];v[j].uv[1]=uv[index/6];}
}
#endif
