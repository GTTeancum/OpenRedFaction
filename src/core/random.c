#include "rf/random.h"
int rf_random_next(rf_random_state *state,uint32_t *result)
{
    uint32_t next;
    if(!state || !result)return RF_RANGE;
    next=state->value*UINT32_C(214013)+UINT32_C(2531011);
    state->value=next;*result=(next>>16)&0x7fffu;return RF_OK;
}
