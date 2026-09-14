#include "rf/level.h"
#include <stdio.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"items line %d: %s\n",__LINE__,#x);return 1;}}while(0)
static rf_vpp archive;static uint32_t levels,total,peak,nonempty;
static int visit(const rf_vpp_entry *entry,void *context)
{
    rf_level level;rf_level_owned_items items={0},empty={0};rf_level_entity_reader r,saved;
    rf_level_item item,before;uint32_t i;int status;size_t len=strlen(entry->name);(void)context;
    if(len<4 || strcmp(entry->name+len-4,".rfl"))return RF_OK;
    CHECK(rf_level_open(&level,&archive,entry->name)==RF_OK);
    status=rf_level_owned_items_open(&level,256*1024,&items);if(status==RF_NOT_FOUND)return RF_OK;CHECK(status==RF_OK);
    levels++;total+=items.count;if(items.allocated_bytes>peak)peak=items.allocated_bytes;
    CHECK(rf_level_owned_items_open(&level,1,&empty)==RF_RANGE && !empty.items && !empty.count);
    if(items.count) {
        nonempty++;CHECK(rf_level_items_begin(&level,&r)==RF_OK);
        for(i=0;i<items.count;i++){CHECK(rf_level_item_next(&r,&item)==RF_OK);CHECK(!memcmp(&item,items.items+i,sizeof(item)));}
        CHECK(rf_level_item_next(&r,&item)==RF_NOT_FOUND);
        CHECK(rf_level_items_begin(&level,&r)==RF_OK);r.section.size--;
        do {saved=r;memset(&before,0xa5,sizeof(before));item=before;status=rf_level_item_next(&r,&item);}while(status==RF_OK);
        CHECK(status!=RF_NOT_FOUND && !memcmp(&saved,&r,sizeof(r)) && !memcmp(&before,&item,sizeof(item)));
        if(!strcmp(entry->name,"L1S1.rfl"))for(i=0;i<items.count;i++)printf("L1S1 item uid=%u class=%s quantity=%d flag=%u position=%g,%g,%g\n",items.items[i].uid,items.items[i].class_name,items.items[i].quantity,items.items[i].common_flag,items.items[i].position[0],items.items[i].position[1],items.items[i].position[2]);
    }
    rf_level_owned_items_close(&items);rf_level_owned_items_close(&items);CHECK(!items.items && !items.count);
    return RF_OK;
}
int main(int argc,char **argv)
{
    char path[1024];uint32_t i;CHECK(argc==2);
    for(i=1;i<=3;i++){snprintf(path,sizeof(path),"%s/levels%u.vpp",argv[1],i);CHECK(rf_vpp_open(&archive,path)==RF_OK);CHECK(rf_vpp_visit(&archive,visit,NULL)==RF_OK);rf_vpp_close(&archive);}
    {
        rf_level level;rf_level_owned_items items={0};
        snprintf(path,sizeof(path),"%s/levels1.vpp",argv[1]);CHECK(rf_vpp_open(&archive,path)==RF_OK);
        CHECK(rf_level_open(&level,&archive,"L1S1.rfl")==RF_OK);
        CHECK(rf_level_owned_items_open(&level,256*1024,&items)==RF_OK);rf_vpp_close(&archive);
        CHECK(items.count && items.items[0].uid==8842 && !strcmp(items.items[0].class_name,"Remote Charges") && items.items[0].quantity==3);
        rf_level_owned_items_close(&items);
    }
    CHECK(levels && total && nonempty);printf("PASS levels=%u nonempty=%u items=%u peak=%u\n",levels,nonempty,total,peak);return 0;
}
