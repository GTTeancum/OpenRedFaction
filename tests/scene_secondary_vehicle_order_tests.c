#include <stdio.h>
#include "../src/diagnostic/scene_secondary_vehicle_order_checks.inc"
int main(void)
{
    uint32_t out[16];int status=scene_secondary_vehicle_order_checks(out);
    if(status){fprintf(stderr,"secondary vehicle orders: status=%d passed=%u/%u mask=%x line=%u\n",
        status,out[1],out[0],out[2],out[3]);return 1;}
    printf("secondary vehicle orders PASS: %u checks; authoritative type/owner identity, fixed target, moving player, persistent arrival, absence and atomic errors\n",out[1]);
    return 0;
}
