#include "rf/level.h"
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    rf_vpp archive;rf_level level;rf_level_entity_reader reader;rf_level_entity entity;int status;
    _Static_assert(sizeof(entity)==1084,"Entity probe wire layout");
    if(argc==4 && !strcmp(argv[3],"--triggers")) {
        rf_level_trigger_reader cursor;rf_level_trigger record={0};uint32_t i,uid;
        _Static_assert(sizeof(record)==668,"Trigger probe wire layout");
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]) ||
            rf_level_triggers_begin(&level,&cursor))return 2;
        if(fwrite(&cursor.count,4,1,stdout)!=1)return 3;
        for(;;) {
            rf_level_trigger_reader before=cursor,bad,saved;rf_level_trigger output,unchanged;
            status=rf_level_trigger_next(&cursor,&record);if(status==RF_NOT_FOUND)break;if(status)return 3;
            bad=before;bad.section.size=record.offset+record.bytes-1;saved=bad;
            memset(&output,0xa5,sizeof(output));unchanged=output;
            if(rf_level_trigger_next(&bad,&output)!=RF_FORMAT || memcmp(&bad,&saved,sizeof(bad)) ||
                memcmp(&output,&unchanged,sizeof(output)))return 4;
            uid=0xabcdef01;
            if(rf_level_trigger_link(&level,&record,record.link_count,&uid)!=RF_RANGE || uid!=0xabcdef01)return 4;
            if(fwrite(&record,sizeof(record),1,stdout)!=1)return 3;
            for(i=0;i<record.link_count;++i) {
                if(rf_level_trigger_link(&level,&record,i,&uid) || fwrite(&uid,4,1,stdout)!=1)return 3;
            }
        }
        {rf_level_trigger copy=record;if(rf_level_trigger_next(&cursor,&record)!=RF_NOT_FOUND || memcmp(&copy,&record,sizeof(record)))return 4;}
        rf_vpp_close(&archive);return 0;
    }
    if(argc!=3)return 2;
    _setmode(_fileno(stdout),_O_BINARY);
    status=rf_vpp_open(&archive,argv[1]);if(status)return 2;
    status=rf_level_open(&level,&archive,argv[2]);
    if(!status)status=rf_level_entities_begin(&level,&reader);
    if(!status)while((status=rf_level_entity_next(&reader,&entity))==RF_OK)
        if(fwrite(&entity,sizeof(entity),1,stdout)!=1) {status=RF_IO;break;}
    rf_vpp_close(&archive);return status==RF_NOT_FOUND?0:3;
}
