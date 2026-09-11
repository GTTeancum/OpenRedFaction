#include "burn_retarget_fixture.h"
int main(void)
{
    uint32_t out[8],i;int status=burn_retarget_fixture(out);
    for(i=0;i<8;i++)printf("%u%s",out[i],i==7?"\n":" ");
    return status?1:0;
}
