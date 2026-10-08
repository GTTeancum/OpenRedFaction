#include <stdio.h>
#include "../src/diagnostic/scene_vehicle_static_edges_checks.inc"
int main(void)
{
    uint32_t out[16];int status=scene_vehicle_static_edges_checks(out);
    if(status){fprintf(stderr,"vehicle static edges: status=%d passed=%u/%u mask=%x line=%u\n",
        status,out[1],out[0],out[2],out[3]);return 1;}
    puts("vehicle static edges PASS: tangent/outward entry, initial overlap, retreat/tangent freedom, recorded frame92/94 segments, nearest composition, filters and atomic errors");
    return 0;
}
