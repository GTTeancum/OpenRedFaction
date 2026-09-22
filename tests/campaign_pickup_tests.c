#include "rf/campaign.h"
#include "rf/level.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"pickup persistence line %u\n",(unsigned)__LINE__);return RF_FORMAT;}}while(0)
static rf_campaign_pickups state,before;
static rf_campaign_actors actors;
typedef struct context {rf_vpp *archive;uint32_t levels,items;} context;
static int visit(const rf_vpp_entry *entry,void *opaque)
{
    context *c=opaque;rf_level level;rf_level_owned_items items={0};uint32_t i,slot,again,old;
    size_t n=strlen(entry->name);if(n<4 || strcmp(entry->name+n-4,".rfl"))return RF_OK;
    CHECK(rf_level_open(&level,c->archive,entry->name)==RF_OK);
    {rf_level_entity_reader reader;rf_level_entity entity;int rc=rf_level_entities_begin(&level,&reader);
     CHECK(rc==RF_OK || rc==RF_NOT_FOUND);
     if(rc==RF_OK)while((rc=rf_level_entity_next(&reader,&entity))==RF_OK) {
        CHECK(rf_campaign_actor_register(&actors,entry->name,(uint32_t)entity.uid,&slot)==RF_OK);
        actors.items[slot].retired=1;
        CHECK(rf_campaign_actor_register(&actors,entry->name,(uint32_t)entity.uid,&again)==RF_OK && slot==again && actors.items[again].retired);
     }
     CHECK(rc==RF_NOT_FOUND);}
    if(!rf_level_find(&level,0x40000)){c->levels++;return RF_OK;}
    CHECK(rf_level_owned_items_open(&level,1024*1024,&items)==RF_OK);
    for(i=0;i<items.count;i++) {
        CHECK(rf_campaign_pickup_register(&state,entry->name,items.items[i].uid,&slot)==RF_OK);
        CHECK(!state.items[slot].retired);state.items[slot].retired=1;
    }
    old=state.count;
    /* Reopen the authored list after releasing its owner; keys cannot borrow it. */
    rf_level_owned_items_close(&items);
    CHECK(rf_level_owned_items_open(&level,1024*1024,&items)==RF_OK);
    for(i=0;i<items.count;i++) {
        CHECK(rf_campaign_pickup_register(&state,entry->name,items.items[i].uid,&again)==RF_OK);
        CHECK(state.items[again].retired && state.count==old);
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
    printf("PASS %u authored actor keys fit %u-byte independent retirement owner\n",actors.count,(unsigned)sizeof(actors));
    {
        float point[3]={1,2,3},bad[3]={NAN,2,3};rf_campaign_weapon_drop saved;
        memset(&actors,0,sizeof(actors));
        CHECK(rf_campaign_actor_register(&actors,"L2S3.rfl",2114,&a)==RF_OK);
        CHECK(rf_campaign_actor_drop_emit(&actors,a,8,42,point)==RF_OK);
        saved=actors.drops[a];actors.items[a].retired=1;
        CHECK(rf_campaign_actor_register(&actors,"l2s3.RFL",2114,&b)==RF_OK && a==b);
        CHECK(!memcmp(&saved,actors.drops+b,sizeof(saved)));
        CHECK(rf_campaign_actor_drop_emit(&actors,a,3,16,point)==RF_OK && !memcmp(&saved,actors.drops+a,sizeof(saved)));
        actors.drops[a].state=2;saved=actors.drops[a];
        CHECK(rf_campaign_actor_drop_emit(&actors,a,8,42,point)==RF_OK && !memcmp(&saved,actors.drops+a,sizeof(saved)));
        CHECK(rf_campaign_actor_drop_emit(&actors,a,64,42,point)==RF_RANGE);
        CHECK(rf_campaign_actor_drop_emit(&actors,a,8,-1,point)==RF_RANGE);
        CHECK(rf_campaign_actor_drop_emit(&actors,a,8,42,bad)==RF_RANGE);
        CHECK(rf_campaign_actor_drop_emit(&actors,actors.count,8,42,point)==RF_RANGE);
        CHECK(!memcmp(&saved,actors.drops+a,sizeof(saved)));
        CHECK(rf_campaign_actor_register(&actors,"L2S2a.rfl",2114,&b)==RF_OK && b!=a && !actors.drops[b].state);
        CHECK(rf_campaign_actor_drop_emit(&actors,b,8,0,point)==RF_OK);
        CHECK(actors.drops[b].state==1 && actors.drops[b].weapon==8 && actors.drops[b].quantity==0);
        saved=actors.drops[b];actors.items[b].retired=1;
        CHECK(rf_campaign_actor_register(&actors,"l2s2A.RFL",2114,&a)==RF_OK && a==b);
        CHECK(!memcmp(&saved,actors.drops+a,sizeof(saved)));
        CHECK(rf_campaign_actor_drop_emit(&actors,a,8,42,point)==RF_OK && !memcmp(&saved,actors.drops+a,sizeof(saved)));
        actors.drops[a].state=2;saved=actors.drops[a];
        CHECK(rf_campaign_actor_drop_emit(&actors,a,8,0,point)==RF_OK && !memcmp(&saved,actors.drops+a,sizeof(saved)));
        puts("PASS loaded/empty weapon drops survive retirement, reject negatives, isolate sections and cannot replenish or respawn");
    }
    memset(&state,0,sizeof(state));
    CHECK(rf_campaign_pickup_register(&state,"L1S1.rfl",7,&a)==RF_OK);state.items[a].retired=1;
    CHECK(rf_campaign_pickup_register(&state,"l1s1.RFL",7,&b)==RF_OK && a==b && state.items[b].retired);
    CHECK(rf_campaign_pickup_register(&state,"L1S2.rfl",7,&b)==RF_OK && a!=b && !state.items[b].retired);
    before=state;memset(name,'x',sizeof(name));
    CHECK(rf_campaign_pickup_register(&state,name,7,&slot)==RF_FORMAT && slot==123 && !memcmp(&before,&state,sizeof(state)));
    for(i=state.count;i<RF_CAMPAIGN_PICKUP_SLOTS;i++)CHECK(rf_campaign_pickup_register(&state,"L1S2.rfl",i+100,&slot)==RF_OK);
    before=state;slot=123;
    CHECK(rf_campaign_pickup_register(&state,"new.rfl",1,&slot)==RF_RANGE && slot==123 && !memcmp(&before,&state,sizeof(state)));
    memset(&state,0,sizeof(state));CHECK(rf_campaign_pickup_register(&state,"L1S1.rfl",7,&slot)==RF_OK && !state.items[slot].retired);
    puts("PASS case folding, cross-level UID isolation, full-capacity rollback and new campaign reset");return 0;
}
