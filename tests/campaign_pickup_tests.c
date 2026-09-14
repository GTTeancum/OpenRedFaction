#include "rf/campaign.h"
#include "rf/level.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pickup persistence line %u\n",(unsigned)__LINE__);return RF_FORMAT;}}while(0)
static rf_campaign_pickups state,before;
typedef struct context {rf_vpp *archive;uint32_t levels,items;} context;
static int visit(const rf_vpp_entry *entry,void *opaque)
{
    context *c=opaque;rf_level level;rf_level_owned_items items={0};uint32_t i,slot,again,old;
    size_t n=strlen(entry->name);if(n<4 || strcmp(entry->name+n-4,".rfl"))return RF_OK;
    CHECK(rf_level_open(&level,c->archive,entry->name)==RF_OK);
    if(!rf_level_find(&level,0x40000)){c->levels++;return RF_OK;}
    CHECK(rf_level_owned_items_open(&level,1024*1024,&items)==RF_OK);
    for(i=0;i<items.count;i++) {
        CHECK(rf_campaign_pickup_register(&state,entry->name,items.items[i].uid,&slot)==RF_OK);
        CHECK(!state.items[slot].taken);state.items[slot].taken=1;
    }
    old=state.count;
    /* Reopen the authored list after releasing its owner; keys cannot borrow it. */
    rf_level_owned_items_close(&items);
    CHECK(rf_level_owned_items_open(&level,1024*1024,&items)==RF_OK);
    for(i=0;i<items.count;i++) {
        CHECK(rf_campaign_pickup_register(&state,entry->name,items.items[i].uid,&again)==RF_OK);
        CHECK(state.items[again].taken && state.count==old);
    }
    c->levels++;c->items+=items.count;rf_level_owned_items_close(&items);return RF_OK;
}
int main(int argc,char **argv)
{
    rf_vpp archive={0};context c={&archive,0,0};char path[1024],name[64];uint32_t i,a,b,slot=123;
    CHECK(argc==2);
    for(i=1;i<=3;i++) {
        snprintf(path,sizeof(path),"%s/levels%u.vpp",argv[1],i);
        CHECK(rf_vpp_open(&archive,path)==RF_OK);
        CHECK(rf_vpp_visit(&archive,visit,&c)==RF_OK);rf_vpp_close(&archive);
    }
    printf("PASS %u levels, %u authored pickups fit %u-byte persistent owner and survive list replacement\n",c.levels,c.items,(unsigned)sizeof(state));
    memset(&state,0,sizeof(state));
    CHECK(rf_campaign_pickup_register(&state,"L1S1.rfl",7,&a)==RF_OK);state.items[a].taken=1;
    CHECK(rf_campaign_pickup_register(&state,"l1s1.RFL",7,&b)==RF_OK && a==b && state.items[b].taken);
    CHECK(rf_campaign_pickup_register(&state,"L1S2.rfl",7,&b)==RF_OK && a!=b && !state.items[b].taken);
    before=state;memset(name,'x',sizeof(name));
    CHECK(rf_campaign_pickup_register(&state,name,7,&slot)==RF_FORMAT && slot==123 && !memcmp(&before,&state,sizeof(state)));
    for(i=state.count;i<RF_CAMPAIGN_PICKUP_SLOTS;i++)CHECK(rf_campaign_pickup_register(&state,"L1S2.rfl",i+100,&slot)==RF_OK);
    before=state;slot=123;
    CHECK(rf_campaign_pickup_register(&state,"new.rfl",1,&slot)==RF_RANGE && slot==123 && !memcmp(&before,&state,sizeof(state)));
    memset(&state,0,sizeof(state));CHECK(rf_campaign_pickup_register(&state,"L1S1.rfl",7,&slot)==RF_OK && !state.items[slot].taken);
    puts("PASS case folding, cross-level UID isolation, full-capacity rollback and new campaign reset");return 0;
}
