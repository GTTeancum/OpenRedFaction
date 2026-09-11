#include "rf/level.h"
#include <fcntl.h>
#include <io.h>
#include <string.h>
int main(int argc,char **argv)
{
    rf_vpp archive;rf_level level;rf_level_entity_reader reader;rf_level_entity entity;int status;
    _Static_assert(sizeof(entity)==1084,"Entity probe wire layout");
    if(argc==4 && !strcmp(argv[3],"--ambient")) {
        rf_level_owned_ambient owned={0},small={0},zero={0};rf_level_ambient_reader cursor;
        rf_level_ambient_sound record;uint32_t i,bytes;
        _Static_assert(sizeof(record)==300,"Ambient wire layout");
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]))return 2;
        status=rf_level_owned_ambient_open(&level,65536,&owned);
        if(status==RF_NOT_FOUND) {
            if(memcmp(&owned,&zero,sizeof(owned)))return 4;
            fwrite(&owned.count,4,1,stdout);rf_vpp_close(&archive);return 0;
        }
        if(status || rf_level_ambient_begin(&level,&cursor))return 2;
        bytes=owned.allocated_bytes;
        if(rf_level_owned_ambient_open(&level,bytes-1,&small)!=RF_RANGE || memcmp(&small,&zero,sizeof(small)))return 4;
        if(rf_level_owned_ambient_open(&level,bytes,&owned)!=RF_RANGE)return 4;
        for(i=0;i<owned.count;++i) {
            rf_level_ambient_reader cut=cursor,saved;rf_level_ambient_sound sentinel,expected;
            if(rf_level_ambient_next(&cursor,&record) || memcmp(&record,owned.items+i,sizeof(record)))return 4;
            cut.section.size=record.offset+record.bytes-1;saved=cut;
            memset(&sentinel,0xa5,sizeof(sentinel));expected=sentinel;
            if(rf_level_ambient_next(&cut,&sentinel)!=RF_FORMAT || memcmp(&cut,&saved,sizeof(cut)) ||
                memcmp(&sentinel,&expected,sizeof(sentinel)))return 4;
        }
        if(rf_level_ambient_next(&cursor,&record)!=RF_NOT_FOUND)return 4;
        rf_level_owned_ambient_close(&owned);
        if(rf_level_owned_ambient_open(&level,bytes,&owned))return 4;
        rf_vpp_close(&archive);memset(&level,0xa5,sizeof(level));
        if(fwrite(&owned.count,4,1,stdout)!=1 || fwrite(owned.items,sizeof(record),owned.count,stdout)!=owned.count)return 3;
        fprintf(stderr,"%u\n",bytes);rf_level_owned_ambient_close(&owned);rf_level_owned_ambient_close(&owned);
        return memcmp(&owned,&zero,sizeof(owned))?4:0;
    }
    if(argc==4 && !strcmp(argv[3],"--runtime-forces")) {
        rf_physics_force_collection owned={0},small={0},zero={0};uint32_t bytes,i;
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]) ||
            rf_physics_forces_open(&level,65536,&owned))return 2;
        bytes=owned.allocated_bytes;
        if(rf_physics_forces_open(&level,bytes-1,&small)!=RF_RANGE || memcmp(&small,&zero,sizeof(small)))return 4;
        if(rf_physics_forces_open(&level,bytes,&owned)!=RF_RANGE)return 4;
        for(i=0;i<level.section_count;++i)if(level.sections[i].type==0x1100) {
            --level.sections[i].size;
            if(rf_physics_forces_open(&level,65536,&small)!=RF_FORMAT || memcmp(&small,&zero,sizeof(small)))return 4;
            ++level.sections[i].size;
        }
        rf_physics_forces_close(&owned);if(rf_physics_forces_open(&level,bytes,&owned))return 4;
        rf_vpp_close(&archive);memset(&level,0xa5,sizeof(level));
        if(fwrite(&owned.count,4,1,stdout)!=1 || fwrite(owned.items,sizeof(*owned.items),owned.count,stdout)!=owned.count)return 3;
        fprintf(stderr,"%u\n",bytes);rf_physics_forces_close(&owned);rf_physics_forces_close(&owned);
        return memcmp(&owned,&zero,sizeof(owned))?4:0;
    }
    if(argc==4 && !strcmp(argv[3],"--forces")) {
        rf_level_owned_forces owned={0},small={0},zero={0};rf_level_force_reader cursor;
        rf_level_force_region record;uint32_t i,bytes;
        _Static_assert(sizeof(record)==600,"Force wire layout");
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]) ||
            rf_level_owned_forces_open(&level,1024*1024,&owned) || rf_level_forces_begin(&level,&cursor))return 2;
        bytes=owned.allocated_bytes;
        if(rf_level_owned_forces_open(&level,bytes-1,&small)!=RF_RANGE || memcmp(&small,&zero,sizeof(small)))return 4;
        if(rf_level_owned_forces_open(&level,bytes,&owned)!=RF_RANGE)return 4;
        for(i=0;i<owned.count;++i) {
            rf_level_force_reader cut=cursor,saved;rf_level_force_region sentinel,expected;
            if(rf_level_force_next(&cursor,&record) || memcmp(&record,owned.items+i,sizeof(record)))return 4;
            cut.section.size=record.offset+record.bytes-1;saved=cut;
            memset(&sentinel,0xa5,sizeof(sentinel));expected=sentinel;
            if(rf_level_force_next(&cut,&sentinel)!=RF_FORMAT || memcmp(&cut,&saved,sizeof(cut)) ||
                memcmp(&sentinel,&expected,sizeof(sentinel)))return 4;
        }
        if(rf_level_force_next(&cursor,&record)!=RF_NOT_FOUND)return 4;
        rf_level_owned_forces_close(&owned);
        if(rf_level_owned_forces_open(&level,bytes,&owned))return 4;
        rf_vpp_close(&archive);memset(&level,0xa5,sizeof(level));
        if(fwrite(&owned.count,4,1,stdout)!=1 || fwrite(owned.items,sizeof(record),owned.count,stdout)!=owned.count)return 3;
        fprintf(stderr,"%u\n",bytes);
        rf_level_owned_forces_close(&owned);rf_level_owned_forces_close(&owned);
        return memcmp(&owned,&zero,sizeof(owned))?4:0;
    }
    if(argc==4 && (!strcmp(argv[3],"--owned-entities") || !strcmp(argv[3],"--entity-spawn"))) {
        rf_level_owned_entities owned={0},small={0},saved={0};uint32_t i,bytes;
        int spawn=!strcmp(argv[3],"--entity-spawn");
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]) ||
            rf_level_owned_entities_open(&level,1024*1024,&owned))return 2;
        bytes=owned.allocated_bytes;
        if(rf_level_owned_entities_open(&level,bytes-1,&small)!=RF_RANGE || memcmp(&small,&saved,sizeof(small)))return 4;
        rf_level_owned_entities_close(&owned);
        if(rf_level_owned_entities_open(&level,bytes,&owned))return 4;
        if(rf_level_owned_entities_open(&level,bytes,&owned)!=RF_RANGE)return 4;
        rf_vpp_close(&archive);memset(&level,0xa5,sizeof(level));
        for(i=0;i<owned.count;++i) {
            rf_level_owned_entity *item=owned.items+i;
            if(spawn) {
                rf_level_entity_spawn fields,sentinel;rf_level_owned_entity cut=*item;
                if(rf_level_entity_spawn_read(item,&fields))return 4;
                memset(&sentinel,0xa5,sizeof(sentinel));cut.record.bytes--;
                if(rf_level_entity_spawn_read(&cut,&sentinel)==RF_OK)return 4;
                {unsigned j;for(j=0;j<sizeof(sentinel);++j)if(((unsigned char *)&sentinel)[j]!=0xa5)return 4;}
                if(fwrite(&fields,sizeof(fields),1,stdout)!=1)return 3;
                continue;
            }
            if(fwrite(&item->record,sizeof(item->record),1,stdout)!=1 ||
                fwrite(item->raw,1,item->record.bytes,stdout)!=item->record.bytes)return 3;
        }
        fprintf(stderr,"%u\n",bytes);
        rf_level_owned_entities_close(&owned);rf_level_owned_entities_close(&owned);
        if(memcmp(&owned,&saved,sizeof(owned)))return 4;
        return 0;
    }
    if(argc==2 && !strcmp(argv[1],"--resolve-links")) {
        uint32_t header[3];rf_level_uid_object objects[32];rf_level_uid_key keys[32];rf_level_link_target target;
        _setmode(_fileno(stdin),_O_BINARY);_setmode(_fileno(stdout),_O_BINARY);
        while(fread(header,sizeof(header),1,stdin)==1) {
            if(header[1]>32 || header[2]>32 || fread(objects,sizeof(*objects),header[1],stdin)!=header[1] ||
                fread(keys,sizeof(*keys),header[2],stdin)!=header[2])return 2;
            if(rf_level_link_resolve(header[0],objects,header[1],keys,header[2],&target) ||
                fwrite(&target,sizeof(target),1,stdout)!=1)return 3;
        }
        return ferror(stdin)?3:0;
    }
    if(argc==4 && !strcmp(argv[3],"--owned-events")) {
        rf_level_owned_events owned={0},small={0},saved={0};uint32_t i,bytes;
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]) ||
            rf_level_owned_events_open(&level,1024*1024,&owned))return 2;
        bytes=owned.allocated_bytes;
        if(rf_level_owned_events_open(&level,bytes-1,&small)!=RF_RANGE || memcmp(&small,&saved,sizeof(small)))return 4;
        rf_level_owned_events_close(&owned);
        if(rf_level_owned_events_open(&level,bytes,&owned))return 4;
        rf_vpp_close(&archive);memset(&level,0xa5,sizeof(level));
        if(fwrite(&owned.count,4,1,stdout)!=1)return 3;
        for(i=0;i<owned.count;++i) {
            rf_level_owned_event *item=owned.items+i;
            if(fwrite(&item->record,sizeof(item->record),1,stdout)!=1 ||
                fwrite(item->links,4,item->record.link_count,stdout)!=item->record.link_count)return 3;
        }
        fprintf(stderr,"%u\n",bytes);
        rf_level_owned_events_close(&owned);rf_level_owned_events_close(&owned);
        if(memcmp(&owned,&saved,sizeof(owned)))return 4;
        return 0;
    }
    if(argc==4 && !strcmp(argv[3],"--owned-triggers")) {
        rf_level_owned_triggers owned={0},small={0},saved={0};uint32_t i,bytes;
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]) ||
            rf_level_owned_triggers_open(&level,1024*1024,&owned))return 2;
        bytes=owned.allocated_bytes;
        if(rf_level_owned_triggers_open(&level,bytes-1,&small)!=RF_RANGE || memcmp(&small,&saved,sizeof(small)))return 4;
        rf_level_owned_triggers_close(&owned);
        if(rf_level_owned_triggers_open(&level,bytes,&owned))return 4;
        rf_vpp_close(&archive);memset(&level,0xa5,sizeof(level));
        if(fwrite(&owned.count,4,1,stdout)!=1)return 3;
        for(i=0;i<owned.count;++i) {
            rf_level_owned_trigger *item=owned.items+i;
            if(fwrite(&item->record,sizeof(item->record),1,stdout)!=1 ||
                fwrite(item->links,4,item->record.link_count,stdout)!=item->record.link_count)return 3;
        }
        fprintf(stderr,"%u\n",bytes);
        rf_level_owned_triggers_close(&owned);rf_level_owned_triggers_close(&owned);
        if(memcmp(&owned,&saved,sizeof(owned)))return 4;
        return 0;
    }
    if(argc==4 && !strcmp(argv[3],"--events")) {
        rf_level_event_reader cursor;rf_level_event record={0};uint32_t i,uid;
        _Static_assert(sizeof(record)==1144,"Trigger probe wire layout");
        _setmode(_fileno(stdout),_O_BINARY);
        if(rf_vpp_open(&archive,argv[1]) || rf_level_open(&level,&archive,argv[2]) ||
            rf_level_events_begin(&level,&cursor))return 2;
        if(fwrite(&cursor.count,4,1,stdout)!=1)return 3;
        for(;;) {
            rf_level_event_reader before=cursor,bad,saved;rf_level_event output,unchanged;
            status=rf_level_event_next(&cursor,&record);if(status==RF_NOT_FOUND)break;if(status)return 3;
            bad=before;bad.section.size=record.offset+record.bytes-1;saved=bad;
            memset(&output,0xa5,sizeof(output));unchanged=output;
            if(rf_level_event_next(&bad,&output)!=RF_FORMAT || memcmp(&bad,&saved,sizeof(bad)) ||
                memcmp(&output,&unchanged,sizeof(output)))return 4;
            uid=0xabcdef01;
            if(rf_level_event_link(&level,&record,record.link_count,&uid)!=RF_RANGE || uid!=0xabcdef01)return 4;
            if(fwrite(&record,sizeof(record),1,stdout)!=1)return 3;
            for(i=0;i<record.link_count;++i) {
                if(rf_level_event_link(&level,&record,i,&uid) || fwrite(&uid,4,1,stdout)!=1)return 3;
            }
        }
        {rf_level_event copy=record;if(rf_level_event_next(&cursor,&record)!=RF_NOT_FOUND || memcmp(&copy,&record,sizeof(record)))return 4;}
        rf_vpp_close(&archive);return 0;
    }
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
