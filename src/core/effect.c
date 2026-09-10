#include "rf/effect.h"
int rf_vclip_name_lookup(const char *const names[64],const char *name)
{
    unsigned i;if(!names || !name || !*name)return -1;
    for(i=0;i<64;++i) {
        const unsigned char *a=(const unsigned char *)names[i],*b=(const unsigned char *)name;
        if(!a)continue;
        for(;;) {
            unsigned x=*a++,y=*b++;
            if(x>='A' && x<='Z')x+='a'-'A';
            if(y>='A' && y<='Z')y+='a'-'A';
            if(x!=y)break;
            if(!x)return (int)i;
        }
    }
    return -1;
}
int rf_effect_set_enabled(rf_effect_pair *pairs,uint32_t count,int32_t index,
    uint32_t override_mode,int32_t enabled,int32_t now_ms)
{
    rf_effect_switch **objects; int32_t stamp; int status; unsigned i;
    if (!pairs || index<0 || (uint32_t)index>=count) return RF_RANGE;
    objects=pairs[index].objects[(uint8_t)override_mode!=0];
    if (!objects[0] || !objects[1]) return RF_OK;
    for (i=0;i<2;++i) {
        if (!enabled) objects[i]->enabled=0;
        else if (objects[i]->enabled!=1) {
            status=rf_timer_set(&stamp,now_ms,0); if (status!=RF_OK) return status;
            objects[i]->enabled=1; objects[i]->started=stamp;
        }
    }
    return RF_OK;
}
