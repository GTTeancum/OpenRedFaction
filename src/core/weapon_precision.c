#include "rf/weapon_precision.h"
#include <math.h>
#include <string.h>

int rf_weapon_precision_select(const rf_object_registry *registry,
    const rf_hitscan_candidate *items,uint32_t count,
    const float start[3],const float delta[3],float world_fraction,
    uint32_t mode,rf_hitscan_trace trace,void *context,rf_weapon_precision_result *result)
{
    rf_weapon_precision_result value;uint32_t i;
    if(!result || mode>RF_WEAPON_PRECISION_RAIL || !isfinite(world_fraction) ||
       world_fraction<0 || world_fraction>1)return RF_RANGE;
    memset(&value,0,sizeof(value));
    if(mode==RF_WEAPON_PRECISION_SNIPER){
        rf_hitscan_selection hit;int status=rf_hitscan_select(registry,items,count,
            start,delta,world_fraction,trace,context,&hit);
        if(status)return status;
        if(hit.matched){value.hits[0]=hit;value.count=1;}
    }else{
        /* Validate even an empty snapshot with the shared selection contract. */
        rf_hitscan_selection hit;int status=rf_hitscan_select(registry,items,0,
            start,delta,1,trace,context,&hit);
        if(status)return status;
        if(count && !items)return RF_RANGE;
        for(i=0;i<count;++i){
            uint32_t j,pos;
            status=rf_hitscan_select(registry,items+i,1,start,delta,1,trace,context,&hit);
            if(status)return status;
            if(!hit.matched)continue;
            hit.index=i;
            for(j=0;j<value.count;++j)if(value.hits[j].handle==hit.handle)break;
            if(j<value.count){
                if(value.hits[j].fraction<=hit.fraction)continue;
                for(;j+1<value.count;++j)value.hits[j]=value.hits[j+1];
                --value.count;
            }
            for(pos=0;pos<value.count;++pos)if(hit.fraction<value.hits[pos].fraction)break;
            if(value.count==RF_WEAPON_PRECISION_HITS){
                value.truncated=1;
                if(pos==value.count)continue;
            }else ++value.count;
            for(j=value.count-1;j>pos;--j)value.hits[j]=value.hits[j-1];
            value.hits[pos]=hit;
        }
    }
    *result=value;return RF_OK;
}
