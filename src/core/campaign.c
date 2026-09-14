#include "rf/campaign.h"
#include <math.h>
int rf_campaign_player_copy(rf_campaign_player_state *destination,
    const rf_campaign_player_state *source,uint32_t catalog_hash)
{
    uint32_t i;
    if(!destination || !source)return RF_RANGE;
    if(source->catalog_hash!=catalog_hash || source->weapon>=64 ||
       !isfinite(source->health) || source->health<=0 ||
       !isfinite(source->armor) || source->armor<0)return RF_FORMAT;
    for(i=0;i<64;i++)if(source->inventory.owned[i]>1 || source->inventory.loaded[i]<0)return RF_FORMAT;
    for(i=0;i<32;i++)if(source->inventory.reserve[i]<0)return RF_FORMAT;
    if(!source->inventory.owned[source->weapon])return RF_FORMAT;
    *destination=*source;return RF_OK;
}
