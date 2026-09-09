#include "rf/level.h"
#include <fcntl.h>
#include <io.h>
int main(int argc,char **argv)
{
    rf_vpp archive;rf_level level;rf_level_entity_reader reader;rf_level_entity entity;int status;
    _Static_assert(sizeof(entity)==1084,"Entity probe wire layout");
    if(argc!=3)return 2;
    _setmode(_fileno(stdout),_O_BINARY);
    status=rf_vpp_open(&archive,argv[1]);if(status)return 2;
    status=rf_level_open(&level,&archive,argv[2]);
    if(!status)status=rf_level_entities_begin(&level,&reader);
    if(!status)while((status=rf_level_entity_next(&reader,&entity))==RF_OK)
        if(fwrite(&entity,sizeof(entity),1,stdout)!=1) {status=RF_IO;break;}
    rf_vpp_close(&archive);return status==RF_NOT_FOUND?0:3;
}
