#include "rf/campaign.h"
#include <math.h>
#include <string.h>
#include <limits.h>
static int object_register(char (*levels)[64],uint32_t *level_count,rf_campaign_object_record *items,uint32_t *count,uint32_t capacity,const char *level,uint32_t uid,uint32_t *slot)
{
    char canonical[64]={0};uint32_t i,n,l;
    if(!level || !slot || (*level_count)>RF_CAMPAIGN_PICKUP_LEVELS || (*count)>capacity)return RF_RANGE;
    for(n=0;n<64 && level[n];n++) {
        unsigned char c=(unsigned char)level[n];
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.'))return RF_FORMAT;
        canonical[n]=(char)(c>='A' && c<='Z'?c+32:c);
    }
    if(!n || n==64)return RF_FORMAT;
    for(l=0;l<(*level_count);l++)if(!strcmp(canonical,levels[l]))break;
    for(i=0;i<(*count);i++)if(items[i].level==l && items[i].uid==uid){*slot=i;return RF_OK;}
    if((*count)==capacity || l==RF_CAMPAIGN_PICKUP_LEVELS)return RF_RANGE;
    if(l==(*level_count)){memcpy(levels[l],canonical,64);++(*level_count);}
    items[(*count)].level=l;items[(*count)].uid=uid;items[(*count)].retired=0;
    *slot=(*count)++;return RF_OK;
}
int rf_campaign_pickup_register(rf_campaign_pickups *state,const char *level,uint32_t uid,uint32_t *slot)
{
    if(!state)return RF_RANGE;
    return object_register(state->levels,&state->level_count,state->items,&state->count,RF_CAMPAIGN_PICKUP_SLOTS,level,uid,slot);
}
int rf_campaign_actor_register(rf_campaign_actors *state,const char *level,uint32_t uid,uint32_t *slot)
{
    if(!state)return RF_RANGE;
    return object_register(state->levels,&state->level_count,state->items,&state->count,RF_CAMPAIGN_ACTOR_SLOTS,level,uid,slot);
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
    if(source->catalog_hash!=catalog_hash || (source->weapon>=64 && source->weapon!=UINT32_MAX) ||
       !isfinite(source->health) || source->health<=0 ||
       !isfinite(source->armor) || source->armor<0)return RF_FORMAT;
    for(i=0;i<64;i++)if(source->inventory.owned[i]>1 || source->inventory.loaded[i]<0)return RF_FORMAT;
    for(i=0;i<32;i++)if(source->inventory.reserve[i]<0)return RF_FORMAT;
    if(source->weapon!=UINT32_MAX && !source->inventory.owned[source->weapon])return RF_FORMAT;
    *destination=*source;return RF_OK;
}

static int local_goal_level(const char *level,char key[64])
{
    uint32_t i;if(!level)return RF_RANGE;
    memset(key,0,64);
    for(i=0;i<64 && level[i];i++) {
        unsigned char c=(unsigned char)level[i];
        if(!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='_' || c=='-' || c=='.'))return RF_FORMAT;
        key[i]=(char)(c>='A' && c<='Z'?c+32:c);
    }
    return !i || i==64?RF_FORMAT:RF_OK;
}
static int local_goal_name_equal(const char *a,const char *b)
{
    uint32_t i;for(i=0;i<256;i++) {
        unsigned char x=a[i],y=b[i];
        if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;if(!x)return 1;
    }
    return 0;
}
int rf_campaign_local_goals_save(rf_campaign_local_goals *store,const char *level,const rf_campaign_goals *goals)
{
    char key[64];uint32_t i,j,needed=0,slots[RF_CAMPAIGN_GOALS_MAX];int status;
    if(!store || !goals || store->count>RF_CAMPAIGN_LOCAL_GOALS_MAX || goals->count>RF_CAMPAIGN_GOALS_MAX)return RF_RANGE;
    status=local_goal_level(level,key);if(status)return status;
    for(i=0;i<store->count;i++)if(!store->items[i].level[0] || !memchr(store->items[i].level,0,64) ||
        !store->items[i].name[0] || !memchr(store->items[i].name,0,256))return RF_FORMAT;
    /* Preflight all writes so exhausted capacity cannot partly update a section. */
    for(i=0;i<goals->count;i++) {
        if(goals->items[i].persistent>1 || !goals->items[i].name[0] || !memchr(goals->items[i].name,0,256))return RF_FORMAT;
        if(goals->items[i].persistent)continue;
        for(j=0;j<store->count;j++)if(!strcmp(store->items[j].level,key) && local_goal_name_equal(store->items[j].name,goals->items[i].name))break;
        slots[i]=j==store->count?store->count+needed++:j;
    }
    if(needed>RF_CAMPAIGN_LOCAL_GOALS_MAX-store->count)return RF_RANGE;
    for(i=0;i<goals->count;i++)if(!goals->items[i].persistent) {
        j=slots[i];memcpy(store->items[j].level,key,64);memcpy(store->items[j].name,goals->items[i].name,256);store->items[j].value=goals->items[i].value;
    }
    store->count+=needed;return RF_OK;
}
int rf_campaign_local_goals_restore(const rf_campaign_local_goals *store,const char *level,rf_campaign_goals *goals)
{
    char key[64];uint32_t i,j;int status;
    if(!store || !goals || store->count>RF_CAMPAIGN_LOCAL_GOALS_MAX || goals->count>RF_CAMPAIGN_GOALS_MAX)return RF_RANGE;
    status=local_goal_level(level,key);if(status)return status;
    for(i=0;i<store->count;i++)if(!store->items[i].level[0] || !memchr(store->items[i].level,0,64) ||
        !store->items[i].name[0] || !memchr(store->items[i].name,0,256))return RF_FORMAT;
    for(i=0;i<goals->count;i++)if(goals->items[i].persistent>1 || !goals->items[i].name[0] || !memchr(goals->items[i].name,0,256))return RF_FORMAT;
    for(i=0;i<goals->count;i++)if(!goals->items[i].persistent) {
        for(j=0;j<store->count;j++)if(!strcmp(store->items[j].level,key) && local_goal_name_equal(store->items[j].name,goals->items[i].name)) {
            goals->items[i].value=store->items[j].value;break;
        }
    }
    return RF_OK;
}

int rf_campaign_trigger_register(rf_campaign_triggers *state,const char *level,uint32_t uid,uint32_t *slot)
{
    if(!state)return RF_RANGE;
    return object_register(state->levels,&state->level_count,state->items,&state->count,RF_CAMPAIGN_TRIGGER_SLOTS,level,uid,slot);
}

int rf_campaign_actor_drop_emit(rf_campaign_actors *store,uint32_t slot,int32_t weapon,int32_t quantity,const float position[3])
{
    rf_campaign_weapon_drop *drop;uint32_t i;
    if(!store || store->count>RF_CAMPAIGN_ACTOR_SLOTS || slot>=store->count ||
       weapon<0 || weapon>=64 || quantity<0 || !position)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(position[i]))return RF_RANGE;
    drop=store->drops+slot;if(drop->state>2)return RF_FORMAT;if(drop->state)return RF_OK;
    drop->weapon=weapon;drop->quantity=quantity;memcpy(drop->position,position,12);drop->state=1;return RF_OK;
}
