#include "rf/motion_file.h"
#include <string.h>
#include <fcntl.h>
#include <io.h>
static int visit(const rf_vpp_entry *entry, void *context)
{
    rf_motion_file file; rf_motion_track track; uint32_t i,j;
    rf_motion_rotation_key rotation; rf_motion_position_key position;
    size_t length=strlen(entry->name);
    if (length<4 || strcmp(entry->name+length-4,".rfa")) return RF_OK;
    if (rf_motion_file_open(&file,(rf_vpp *)context,entry->name)!=RF_OK) return RF_FORMAT;
    if (fwrite(entry->name,61,1,stdout)!=1 || fwrite(file.header,80,1,stdout)!=1) return RF_IO;
    for (i=0;i<file.header[6];++i) {
        if (rf_motion_file_track(&file,i,&track)!=RF_OK) return RF_FORMAT;
        if (fwrite(&track.offset,4,1,stdout)!=1 || fwrite(&track.size,4,1,stdout)!=1 ||
            fwrite(&track.rotation_count,4,1,stdout)!=1 || fwrite(&track.position_count,4,1,stdout)!=1 ||
            fwrite(&track.envelope.weight,4,1,stdout)!=1) return RF_IO;
        for (j=0;j<track.rotation_count;++j) {
            if (rf_motion_file_rotation(&file,i,j,&rotation)!=RF_OK) return RF_FORMAT;
            if (fwrite(&rotation,16,1,stdout)!=1) return RF_IO;
        }
        if (rf_motion_file_rotation(&file,i,track.rotation_count,&rotation)!=RF_RANGE) return RF_FORMAT;
        for (j=0;j<track.position_count;++j) {
            if (rf_motion_file_position(&file,i,j,&position)!=RF_OK) return RF_FORMAT;
            if (fwrite(&position,40,1,stdout)!=1) return RF_IO;
        }
        if (rf_motion_file_position(&file,i,track.position_count,&position)!=RF_RANGE) return RF_FORMAT;
    }
    return RF_OK;
}
int main(int argc,char **argv)
{
    rf_vpp archive; int status;
    _Static_assert(sizeof(rf_motion_rotation_key)==16,"Rotation layout");
    _Static_assert(sizeof(rf_motion_position_key)==40,"Position layout");
    if (argc!=2) return 1;
    _setmode(_fileno(stdout),_O_BINARY);
    if (rf_vpp_open(&archive,argv[1])!=RF_OK) return 2;
    status=rf_vpp_visit(&archive,visit,&archive); rf_vpp_close(&archive);
    return status==RF_OK ? 0 : 3;
}
