#include "rf/motion_file.h"
#include <math.h>
#include <string.h>
int rf_motion_marker_register(rf_motion_cache_record *record,const char *name,float frame)
{
    uint32_t length=0,i,empty=2;double tick;int32_t value;
    if(!record || !name || !isfinite(frame))return RF_RANGE;
    while(length<16 && name[length])++length;
    if(length==16)return RF_RANGE;
    for(i=0;i<2;++i) {
        const char *slot=(const char*)record->bytes+0x40+i*20;
        if(!*slot){if(empty==2)empty=i;continue;}
        if(!memchr(slot,0,16))return RF_RANGE;
        if(!strcmp(slot,name))return RF_OK;
    }
    if(empty==2)return RF_OK;
    tick=(double)frame*(double)0.03333333507180214f;
    tick=tick*30.0;tick=tick*160.0;
    if(tick<-2147483648.0 || tick>=2147483648.0)return RF_RANGE;
    value=(int32_t)tick;
    memcpy(record->bytes+0x40+empty*20,name,length+1);
    memcpy(record->bytes+0x50+empty*20,&value,4);return RF_OK;
}
int rf_motion_compiled_filename(const char *authored,char compiled[64])
{
    uint32_t length=0,stem=0;int dot=0;
    if(!authored || !compiled)return RF_RANGE;
    while(length<64 && authored[length]) {
        if(!dot && authored[length]=='.') {stem=length;dot=1;}
        ++length;
    }
    if(length==64)return RF_RANGE;
    if(!dot)stem=length;
    if(stem>59)return RF_RANGE;
    memmove(compiled,authored,length+1);memcpy(compiled+stem,".rfa",5);return RF_OK;
}
static uint32_t get32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24;
}
static int cache_stem(const char *name,uint32_t *length,uint32_t *stem)
{
    uint32_t n=0,last=0;int dot=0;
    while(n<60 && name[n]) {
        if((unsigned char)name[n]>127)return RF_FORMAT;
        if(name[n]=='.') {last=n;dot=1;}++n;
    }
    if(n==60)return RF_RANGE;*length=n;*stem=dot?last:n;return RF_OK;
}
int rf_motion_cache_acquire(rf_motion_cache_record *records,uint32_t capacity,
    const char *name,uint32_t *index)
{
    uint32_t length,stem,i,j,first=UINT32_MAX,selected=UINT32_MAX,reference;int status;
    if(!records || !name || !index || capacity>800)return RF_RANGE;
    status=cache_stem(name,&length,&stem);if(status)return status;
    for(i=0;i<capacity;++i) {
        const char *stored=(const char*)records[i].bytes;uint32_t size,end;
        if(!*stored) {if(first==UINT32_MAX)first=i;continue;}
        status=cache_stem(stored,&size,&end);if(status)return status;
        if(stem!=end)continue;
        for(j=0;j<stem;++j) {
            unsigned a=(unsigned char)name[j],b=(unsigned char)stored[j];
            if(a>='A' && a<='Z')a+=32;if(b>='A' && b<='Z')b+=32;
            if(a!=b)break;
        }
        if(j==stem) {selected=i;break;}
    }
    if(selected==UINT32_MAX) {
        if(first==UINT32_MAX)return RF_RANGE;selected=first;
        memset(records[selected].bytes+0x40,0,0x2c);
        records[selected].bytes[0x6c]=0;
        memset(records[selected].bytes+0x70,0,12);
        memmove(records[selected].bytes,name,length+1);
    }
    reference=get32(records[selected].bytes+0x70)+1;
    memcpy(records[selected].bytes+0x70,&reference,4);*index=selected;return RF_OK;
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
static int validate_ticks(const rf_motion_file *file, uint32_t offset, uint32_t count, uint32_t stride)
{
    unsigned char raw[4]; uint32_t i; int32_t previous=0,current; int status;
    for (i=0;i<count;++i) {
        status=rf_vpp_read(file->archive,&file->entry,offset+i*stride,raw,4);
        if (status!=RF_OK) return status;
        current=signed32(get32(raw));
        if (i && current<=previous) return RF_FORMAT;
        previous=current;
    }
    return RF_OK;
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
        status=validate_ticks(&candidate,track.offset+8,track.rotation_count,16);
        if (status!=RF_OK) return status;
        status=validate_ticks(&candidate,track.offset+8+track.rotation_count*16,track.position_count,40);
        if (status!=RF_OK) return status;
    }
    if (!candidate.header[6] && candidate.header[18]!=80) return RF_FORMAT;
    *file=candidate; return RF_OK;
}
static int find_pair(const rf_motion_file *file, uint32_t offset, uint32_t count, uint32_t stride,
                     int32_t tick, uint32_t *first)
{
    uint32_t low=0, high=count; unsigned char raw[4]; int status;
    if (count<2) { *first=0; return RF_OK; }
    while (low<high) {
        uint32_t middle=low+(high-low)/2;
        status=rf_vpp_read(file->archive,&file->entry,offset+middle*stride,raw,4);
        if (status!=RF_OK) return status;
        if (signed32(get32(raw))<=tick) low=middle+1;
        else high=middle;
    }
    if (!low) low=1;
    if (low==count) low=count-1;
    *first=low-1; return RF_OK;
}
int rf_motion_file_sample(const rf_motion_file *file, uint32_t index, int32_t tick, int bypass_fades, rf_motion_sample *out)
{
    rf_motion_track track; rf_motion_sample result;
    rf_motion_rotation_key rotations[2]; rf_motion_position_key positions[2];
    uint32_t first,n,i; int status;
    if (!out) return RF_RANGE;
    status=rf_motion_file_track(file,index,&track); if (status!=RF_OK) return status;
    status=find_pair(file,track.offset+8,track.rotation_count,16,tick,&first); if (status!=RF_OK) return status;
    n=track.rotation_count<2 ? track.rotation_count : 2;
    for (i=0;i<n;++i) {
        status=rf_motion_file_rotation(file,index,first+i,&rotations[i]); if (status!=RF_OK) return status;
    }
    status=rf_motion_sample_rotation(rotations,n,tick,result.rotation); if (status!=RF_OK) return status;
    status=find_pair(file,track.offset+8+track.rotation_count*16,track.position_count,40,tick,&first);
    if (status!=RF_OK) return status;
    n=track.position_count<2 ? track.position_count : 2;
    for (i=0;i<n;++i) {
        status=rf_motion_file_position(file,index,first+i,&positions[i]); if (status!=RF_OK) return status;
    }
    if (n==2 && first>0 && tick==positions[0].tick)
        status=rf_motion_interpolate_position(&positions[0],&positions[1],0,result.position);
    else status=rf_motion_sample_position(positions,n,tick,result.position);
    if (status!=RF_OK) return status;
    status=rf_motion_sample_weight(&track.envelope,tick,bypass_fades,&result.weight); if (status!=RF_OK) return status;
    *out=result; return RF_OK;
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
