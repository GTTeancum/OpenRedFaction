#include "rf/campaign.h"
#include <math.h>
#include <string.h>
#include <limits.h>
int rf_campaign_pickup_register(rf_campaign_pickups *state,const char *level,uint32_t uid,uint32_t *slot)
{
    char canonical[64]={0};uint32_t i,n,l;
    if(!state || !level || !slot || state->level_count>RF_CAMPAIGN_PICKUP_LEVELS || state->count>RF_CAMPAIGN_PICKUP_SLOTS)return RF_RANGE;
    for(n=0;n<64 && level[n];n++) {
        unsigned char c=(unsigned char)level[n];
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.'))return RF_FORMAT;
        canonical[n]=(char)(c>='A' && c<='Z'?c+32:c);
    }
    if(!n || n==64)return RF_FORMAT;
    for(l=0;l<state->level_count;l++)if(!strcmp(canonical,state->levels[l]))break;
    for(i=0;i<state->count;i++)if(state->items[i].level==l && state->items[i].uid==uid){*slot=i;return RF_OK;}
    if(state->count==RF_CAMPAIGN_PICKUP_SLOTS || l==RF_CAMPAIGN_PICKUP_LEVELS)return RF_RANGE;
    if(l==state->level_count){memcpy(state->levels[l],canonical,64);++state->level_count;}
    state->items[state->count].level=l;state->items[state->count].uid=uid;state->items[state->count].taken=0;
    *slot=state->count++;return RF_OK;
}
static int goal_find(const rf_campaign_goals *goals,const char *name,uint32_t *index)
{
    uint32_t i,j;
    if(!goals || !name || !index || goals->count>RF_CAMPAIGN_GOALS_MAX)return RF_RANGE;
    for(j=0;j<256 && name[j];j++);
    if(!j || j==256)return RF_FORMAT;
    for(i=0;i<goals->count;i++) {
        for(j=0;j<256;j++) {
            unsigned char a=(unsigned char)name[j],b=(unsigned char)goals->items[i].name[j];
            if(a>='A' && a<='Z')a+=32;
            if(b>='A' && b<='Z')b+=32;
            if(a!=b)break;
            if(!a){*index=i;return RF_OK;}
        }
    }
    return RF_NOT_FOUND;
}
int rf_campaign_goal_declare(rf_campaign_goals *goals,const char *name,uint32_t persistent)
{
    uint32_t i;int status=goal_find(goals,name,&i);rf_campaign_goal value={0};
    if(persistent>1)return RF_RANGE;
    if(status==RF_OK)return goals->items[i].persistent==persistent?RF_OK:RF_FORMAT;
    if(status!=RF_NOT_FOUND)return status;
    if(goals->count==RF_CAMPAIGN_GOALS_MAX)return RF_RANGE;
    strcpy(value.name,name);value.persistent=persistent;
    goals->items[goals->count++]=value;return RF_OK;
}
int rf_campaign_goal_adjust(rf_campaign_goals *goals,const char *name,uint32_t on)
{
    uint32_t i;int status=goal_find(goals,name,&i);int32_t value;
    if(status)return status;
    if(on>1)return RF_RANGE;
    value=goals->items[i].value;
    /* Explicit failure avoids C signed overflow; no authored campaign needs it. */
    if((on && value==INT32_MAX) || (!on && value==INT32_MIN))return RF_RANGE;
    goals->items[i].value=value+(on?1:-1);return RF_OK;
}
int rf_campaign_goal_check(const rf_campaign_goals *goals,const char *name,int32_t threshold,uint32_t *passed)
{
    uint32_t i;int status;
    if(!passed)return RF_RANGE;
    status=goal_find(goals,name,&i);
    if(status==RF_NOT_FOUND){*passed=0;return RF_OK;}
    if(status)return status;
    *passed=goals->items[i].value>=threshold;return RF_OK;
}
int rf_campaign_goals_next_section(rf_campaign_goals *goals)
{
    uint32_t i,count=0,old;
    if(!goals || goals->count>RF_CAMPAIGN_GOALS_MAX)return RF_RANGE;
    old=goals->count;
    for(i=0;i<old;i++)if(goals->items[i].persistent)goals->items[count++]=goals->items[i];
    memset(goals->items+count,0,(old-count)*sizeof(goals->items[0]));
    goals->count=count;return RF_OK;
}
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
