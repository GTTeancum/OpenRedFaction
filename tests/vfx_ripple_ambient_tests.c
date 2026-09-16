#include "rf/effect.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do { if(!(c)) { fprintf(stderr,"line %d: %s\n",__LINE__,#c);return 1; } } while(0)
/* Original461950 ambient initialization + full4daff0/4da8b0 zero-light
 * instruction cases in liquid_vfx_ambient_lifecycle.py; no assets required.
 * Explicit CHECK remains enabled in Release builds (not assert). */
int main(void)
{
    static const unsigned char authored[][3]={{0,0,0},{16,32,64},{64,96,127},{128,192,255}};
    static const unsigned char expected[][3]={{0,0,0},{32,64,128},{128,192,254},{255,255,255}};
    const float point[3]={0,0,0},normal[3]={0,1,0};
    float ambient[3];unsigned char rgb[3];unsigned i,k;
    for(i=0;i<sizeof(authored)/sizeof(authored[0]);i++) {
        for(k=0;k<3;k++)ambient[k]=(float)((double)authored[i][k]*(double)0.003921568859368563f);
        CHECK(rf_vfx_lighting(point,normal,ambient,.25f,NULL,0,rgb)==RF_OK);
        CHECK(!memcmp(rgb,expected[i],sizeof(rgb)));
    }
    puts("VFX ripple ambient: four original-instruction cases passed");return 0;
}
