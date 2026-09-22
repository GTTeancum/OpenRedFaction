#include "rf/campaign_pickups_checkpoint.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pickup checkpoint line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_campaign_pickups source,restored,before;
static unsigned char data[RF_CAMPAIGN_PICKUPS_CHECKPOINT_MAX_BYTES];
int main(void)
{
 unsigned char identity[32]={5};uint32_t a,b,c,n=0;
 CHECK(!rf_campaign_pickup_register(&source,"L1S1.rfl",123,&a));source.items[a].retired=1;
 CHECK(!rf_campaign_pickup_register(&source,"L1S2.rfl",123,&b));
 CHECK(!rf_campaign_pickups_checkpoint_encode(identity,&source,data,sizeof(data),&n));
 CHECK(!rf_campaign_pickups_checkpoint_decode(data,n,identity,&restored));
 CHECK(!rf_campaign_pickup_register(&restored,"L1S1.rfl",123,&c)&&c==a&&restored.items[c].retired);
 CHECK(!rf_campaign_pickup_register(&restored,"L1S2.rfl",123,&c)&&c==b&&!restored.items[c].retired);
 CHECK(restored.count==2&&!memcmp(&restored,&source,sizeof(source)));
 before=restored;data[n-1]^=1;
 CHECK(rf_campaign_pickups_checkpoint_decode(data,n,identity,&restored)==RF_FORMAT&&!memcmp(&before,&restored,sizeof(before)));
 CHECK(rf_campaign_pickups_checkpoint_decode(data,63,identity,&restored)==RF_FORMAT&&!memcmp(&before,&restored,sizeof(before)));
 puts("PASS pickup save retains collected items across re-registration and separate levels; corrupt decode is atomic");return 0;
}
