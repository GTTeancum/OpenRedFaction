#include "rf/motion_file.h"
#include <math.h>
#include <string.h>
static uint32_t get32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static int32_t signed32(uint32_t bits)
{
    return bits <= INT32_MAX ? (int32_t)bits : (int32_t)((int64_t)bits-4294967296LL);
}
static int16_t signed16(const unsigned char *p)
{
    int32_t value=p[0] | (int32_t)p[1]<<8;
    return (int16_t)(value<32768 ? value : value-65536);
}
static float get_float(const unsigned char *p)
{
    uint32_t bits=get32(p); float result;
    memcpy(&result,&bits,4); return result;
}
int rf_motion_file_track(const rf_motion_file *file, uint32_t index, rf_motion_track *out)
{
    unsigned char raw[8]; rf_motion_track track; uint32_t end; int status;
    if (!file || !file->archive || !out || index>=file->header[6]) return RF_RANGE;
    status=rf_vpp_read(file->archive,&file->entry,80+index*4,raw,index+1<file->header[6] ? 8 : 4);
    if (status!=RF_OK) return status;
    track.offset=get32(raw);
    end=index+1<file->header[6] ? get32(raw+4) : file->header[18];
    if (track.offset<80+file->header[6]*4 || end>file->header[18] || end<track.offset || end-track.offset<8)
        return RF_FORMAT;
    track.size=end-track.offset;
    status=rf_vpp_read(file->archive,&file->entry,track.offset,raw,8);
    if (status!=RF_OK) return status;
    track.rotation_count=raw[4] | (uint32_t)raw[5]<<8;
    track.position_count=raw[6] | (uint32_t)raw[7]<<8;
    if (track.rotation_count>INT16_MAX || track.position_count>INT16_MAX ||
        track.size!=8+track.rotation_count*16+track.position_count*40) return RF_FORMAT;
    track.envelope.weight=get_float(raw);
    track.envelope.start_tick=signed32(file->header[4]); track.envelope.end_tick=signed32(file->header[5]);
    track.envelope.fade_in=signed32(file->header[9]); track.envelope.fade_out=signed32(file->header[10]);
    if (!isfinite(track.envelope.weight)) return RF_FORMAT;
    *out=track; return RF_OK;
}
int rf_motion_file_open(rf_motion_file *file, rf_vpp *archive, const char *name)
{
    rf_motion_file candidate; rf_motion_track track; unsigned char raw[80]; uint32_t i; int status;
    if (!file) return RF_RANGE;
    memset(file,0,sizeof(*file)); memset(&candidate,0,sizeof(candidate));
    if (!archive || !name) return RF_RANGE;
    status=rf_vpp_find(archive,name,&candidate.entry); if (status!=RF_OK) return status;
    if (candidate.entry.size<80) return RF_FORMAT;
    status=rf_vpp_read(archive,&candidate.entry,0,raw,80); if (status!=RF_OK) return status;
    for (i=0;i<20;++i) candidate.header[i]=get32(raw+i*4);
    if (candidate.header[0]!=0x46564d56 || (candidate.header[1]!=7 && candidate.header[1]!=8) ||
        candidate.header[6]>(candidate.entry.size-80)/4 ||
        candidate.header[18]<80+candidate.header[6]*4 || candidate.header[18]>candidate.header[19] ||
        candidate.header[19]>candidate.entry.size ||
        signed32(candidate.header[5])<signed32(candidate.header[4]) ||
        signed32(candidate.header[9])<0 || signed32(candidate.header[10])<0) return RF_FORMAT;
    candidate.archive=archive;
    for (i=0;i<candidate.header[6];++i) {
        status=rf_motion_file_track(&candidate,i,&track); if (status!=RF_OK) return status;
        if (!i && track.offset!=80+candidate.header[6]*4) return RF_FORMAT;
    }
    if (!candidate.header[6] && candidate.header[18]!=80) return RF_FORMAT;
    *file=candidate; return RF_OK;
}
int rf_motion_file_rotation(const rf_motion_file *file, uint32_t index, uint32_t key, rf_motion_rotation_key *out)
{
    rf_motion_track track; rf_motion_rotation_key result; unsigned char raw[16]; uint32_t i; int status;
    if (!out) return RF_RANGE;
    status=rf_motion_file_track(file,index,&track); if (status!=RF_OK) return status;
    if (key>=track.rotation_count) return RF_RANGE;
    status=rf_vpp_read(file->archive,&file->entry,track.offset+8+key*16,raw,16); if (status!=RF_OK) return status;
    result.tick=signed32(get32(raw));
    for (i=0;i<4;++i) result.packed[i]=signed16(raw+4+i*2);
    if (raw[12]>127 || raw[13]>127) return RF_FORMAT;
    result.incoming=(int8_t)raw[12]; result.outgoing=(int8_t)raw[13];
    result.reserved[0]=raw[14]; result.reserved[1]=raw[15];
    *out=result; return RF_OK;
}
int rf_motion_file_position(const rf_motion_file *file, uint32_t index, uint32_t key, rf_motion_position_key *out)
{
    rf_motion_track track; rf_motion_position_key result; unsigned char raw[40]; uint32_t i; int status;
    if (!out) return RF_RANGE;
    status=rf_motion_file_track(file,index,&track); if (status!=RF_OK) return status;
    if (key>=track.position_count) return RF_RANGE;
    status=rf_vpp_read(file->archive,&file->entry,track.offset+8+track.rotation_count*16+key*40,raw,40);
    if (status!=RF_OK) return status;
    result.tick=signed32(get32(raw));
    for (i=0;i<3;++i) {
        result.position[i]=get_float(raw+4+i*4); result.incoming[i]=get_float(raw+16+i*4); result.outgoing[i]=get_float(raw+28+i*4);
        if (!isfinite(result.position[i]) || !isfinite(result.incoming[i]) || !isfinite(result.outgoing[i])) return RF_FORMAT;
    }
    *out=result; return RF_OK;
}
