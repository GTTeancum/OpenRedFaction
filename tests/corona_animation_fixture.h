#ifndef RF_CORONA_ANIMATION_FIXTURE_H
#define RF_CORONA_ANIMATION_FIXTURE_H
#include "rf/effect.h"
#include <string.h>
static const uint32_t corona_animation_times[8]={0,66,67,134,200,267,334,667};
static void corona_animation_quad(rf_particle_draw_vertex v[4])
{
    uint32_t j;memset(v,0,4*sizeof(*v));
    for(j=0;j<4;++j){v[j].screen[0]=32+((j==1 || j==2)?128:0);v[j].screen[1]=32+(j>=2?128:0);
        v[j].depth=1000;v[j].reciprocal_w=1;v[j].argb=0x80ffffff;
        v[j].uv[0]=(j==1 || j==2)?1:0;v[j].uv[1]=j>=2?1:0;}
}
#endif
