#include "rf/motion_file.h"
#include <string.h>
#include <fcntl.h>
#include <io.h>
static int sample_mode;
static uint32_t sample_count;
static int verify_samples(rf_motion_file *file)
{
    /* Development-only full tracks form an independent linear-scan reference. */
    static rf_motion_rotation_key rotations[32767];
    static rf_motion_position_key positions[32767];
    rf_motion_track track; rf_motion_sample expected,actual;
    uint32_t i,j,k; int32_t ticks[5];
    for (i=0;i<file->header[6];++i) {
        if (rf_motion_file_track(file,i,&track)!=RF_OK) return RF_FORMAT;
        for (j=0;j<track.rotation_count;++j)
            if (rf_motion_file_rotation(file,i,j,&rotations[j])!=RF_OK) return RF_FORMAT;
        for (j=0;j<track.position_count;++j)
            if (rf_motion_file_position(file,i,j,&positions[j])!=RF_OK) return RF_FORMAT;
        ticks[0]=track.envelope.start_tick-1; ticks[1]=track.envelope.start_tick;
        ticks[2]=(int32_t)(((int64_t)track.envelope.start_tick+track.envelope.end_tick)/2);
        ticks[3]=track.envelope.end_tick; ticks[4]=track.envelope.end_tick+1;
        for (k=0;k<5;++k) {
            if (rf_motion_sample_rotation(rotations,track.rotation_count,ticks[k],expected.rotation)!=RF_OK ||
                rf_motion_sample_position(positions,track.position_count,ticks[k],expected.position)!=RF_OK ||
                rf_motion_sample_weight(&track.envelope,ticks[k],k&1,&expected.weight)!=RF_OK ||
                rf_motion_file_sample(file,i,ticks[k],k&1,&actual)!=RF_OK ||
                memcmp(&actual,&expected,sizeof(actual))) {
                fprintf(stderr,"Mismatch %s track %u tick %d\n",file->entry.name,i,ticks[k]);
                { uint32_t a[8],b[8],v; memcpy(a,&actual,32); memcpy(b,&expected,32);
                  for(v=0;v<8;++v) fprintf(stderr,"%08x %08x\n",a[v],b[v]); }
                return RF_FORMAT;
            }
            ++sample_count;
        }
    }
    return RF_OK;
}
static int visit(const rf_vpp_entry *entry, void *context)
{
    rf_motion_file file; rf_motion_track track; uint32_t i,j;
    rf_motion_rotation_key rotation; rf_motion_position_key position;
    size_t length=strlen(entry->name);
    if (length<4 || strcmp(entry->name+length-4,".rfa")) return RF_OK;
    if (rf_motion_file_open(&file,(rf_vpp *)context,entry->name)!=RF_OK) return RF_FORMAT;
    if (sample_mode) return verify_samples(&file);
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
    if (argc!=2 && argc!=3) return 1;
    if (argc==3) { if (strcmp(argv[2],"--sample")) return 1; sample_mode=1; }
    _setmode(_fileno(stdout),_O_BINARY);
    if (rf_vpp_open(&archive,argv[1])!=RF_OK) return 2;
    status=rf_vpp_visit(&archive,visit,&archive); rf_vpp_close(&archive);
    if (sample_mode) fprintf(stderr,"%u\n",sample_count);
    return status==RF_OK ? 0 : 3;
}
