#include "rf/geomod.h"
#include <stdio.h>
#include <stdlib.h>
int main(int argc,char **argv)
{
    rf_random_state state;float basis[9];uint32_t i;
    if(argc!=2)return 1;
    state.value=(uint32_t)strtoul(argv[1],NULL,0);
    if(rf_geomod_random_basis(&state,basis))return 2;
    printf("%u",state.value);
    for(i=0;i<9;i++)printf(" %.9g",basis[i]);
    puts("");return 0;
}
