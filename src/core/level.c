#include "rf/level.h"
int rf_level_entity_find(const rf_level *level,int32_t uid,rf_level_entity *entity)
{
    rf_level_entity_reader reader;rf_level_entity current,selected={0};int status,found=0;
    if(!level || !entity)return RF_RANGE;
    status=rf_level_entities_begin(level,&reader);if(status)return status;
    while((status=rf_level_entity_next(&reader,&current))==RF_OK) {
        if(current.uid==uid) {if(found)return RF_FORMAT;selected=current;found=1;}
    }
    if(status!=RF_NOT_FOUND)return status;
    if(!found)return RF_NOT_FOUND;
    *entity=selected;return RF_OK;
}
#include <string.h>

static uint32_t le32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static int entity_read(rf_level_entity_reader *r,void *data,uint32_t bytes)
{
    int status=rf_level_read(r->level,&r->section,r->cursor,data,bytes);
    if(!status)r->cursor+=bytes;return status;
}
static int entity_skip(rf_level_entity_reader *r,uint32_t bytes)
{
    if(r->cursor>r->section.size || bytes>r->section.size-r->cursor)return RF_RANGE;
    r->cursor+=bytes;return RF_OK;
}
static int entity_string(rf_level_entity_reader *r,char *out)
{
    uint8_t raw[2];uint32_t length;int status=entity_read(r,raw,2);if(status)return status;
    length=raw[0]|(uint32_t)raw[1]<<8;
    if(!out)return entity_skip(r,length);
    if(length>=256)return RF_RANGE;
    status=entity_read(r,out,length);if(status)return status;
    if(memchr(out,0,length))return RF_FORMAT;out[length]=0;return RF_OK;
}
int rf_level_entities_begin(const rf_level *level,rf_level_entity_reader *reader)
{
    const rf_level_section *section;rf_level_entity_reader next;uint8_t raw[4];int status;
    if(!level || !reader || level->version!=180)return RF_RANGE;
    section=rf_level_find(level,0x30000);if(!section)return RF_NOT_FOUND;
    memset(&next,0,sizeof(next));next.level=level;next.section=*section;
    status=entity_read(&next,raw,4);if(status)return status;next.count=le32(raw);
    if(next.count>(section->size-4)/155)return RF_FORMAT;
    *reader=next;return RF_OK;
}
int rf_level_entity_next(rf_level_entity_reader *reader,rf_level_entity *entity)
{
    rf_level_entity_reader r;rf_level_entity value;uint8_t raw[48],flags[17];unsigned i;int status;
    if(!reader || !entity || !reader->level)return RF_RANGE;
    if(reader->index>=reader->count)return reader->index==reader->count && reader->cursor==reader->section.size?RF_NOT_FOUND:RF_FORMAT;
    r=*reader;memset(&value,0,sizeof(value));value.offset=r.cursor;
    status=entity_read(&r,raw,4);if(status)return status;value.uid=(int32_t)le32(raw);
    status=entity_string(&r,value.class_name);if(status)return status;
    status=entity_read(&r,raw,48);if(status)return status;
    for(i=0;i<12;++i) {
        uint32_t bits=le32(raw+i*4);float v;
        if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;
        memcpy(&v,&bits,4);
        if(i<3)value.position[i]=v;else value.orientation[((i-3)/3+2)%3][(i-3)%3]=v;
    }
    status=entity_string(&r,value.script_name);if(status)return status;
    status=entity_skip(&r,13);if(status)return status; /* editor byte and three relationship integers */
    for(i=0;i<2;++i) {status=entity_string(&r,NULL);if(status)return status;}
    status=entity_skip(&r,29);if(status)return status; /* six bytes, two angles, three bytes, life/armor/FOV */
    for(i=0;i<7;++i) {status=entity_string(&r,i==3?value.state_animation:i==5?value.skin:NULL);if(status)return status;}
    status=entity_skip(&r,18);if(status)return status; /* two AI bytes and four reference integers */
    status=entity_read(&r,flags,17);if(status)return status;
    if(flags[16]>1)return RF_FORMAT;
    if(flags[16]) {status=entity_skip(&r,4);if(status)return status;}
    for(i=0;i<2;++i) {status=entity_string(&r,NULL);if(status)return status;}
    value.bytes=r.cursor-value.offset;++r.index;
    if(r.index==r.count && r.cursor!=r.section.size)return RF_FORMAT;
    *reader=r;*entity=value;return RF_OK;
}

static int read_at(rf_level *level, uint32_t *cursor, void *data, uint32_t size)
{
    int result = rf_vpp_read(level->archive, &level->entry, *cursor, data, size);
    if (result == RF_OK) *cursor += size;
    return result;
}

static int read_string(rf_level *level, uint32_t *cursor, char *string)
{
    unsigned char raw[2];
    uint32_t length;
    int result = read_at(level, cursor, raw, 2);
    if (result != RF_OK) return result;
    length = raw[0] | (uint32_t)raw[1] << 8;
    if (length >= RF_LEVEL_NAME_CAPACITY) return RF_RANGE;
    result = read_at(level, cursor, string, length);
    if (result != RF_OK) return result;
    if (memchr(string, 0, length)) return RF_FORMAT;
    string[length] = 0;
    return RF_OK;
}

const rf_level_section *rf_level_find(const rf_level *level, uint32_t type)
{
    uint32_t i;
    if (!level || !level->archive) return NULL;
    for (i = 0; i < level->section_count; ++i)
        if (level->sections[i].type == type) return &level->sections[i];
    return NULL;
}

int rf_level_read(const rf_level *level, const rf_level_section *section,
                  uint32_t offset, void *data, uint32_t size)
{
    if (!level || !level->archive || !section) return RF_RANGE;
    if (offset > section->size || size > section->size - offset ||
        (uint64_t)section->offset + 8 + section->size > level->entry.size) return RF_RANGE;
    return rf_vpp_read(level->archive, &level->entry, section->offset + 8 + offset, data, size);
}

static int load(rf_level *level, rf_vpp *archive, const char *name)
{
    unsigned char raw[48];
    uint32_t cursor = 0, count, player_offset, info_offset, i;
    const rf_level_section *player, *info;
    int result;
    level->archive = archive;
    result = rf_vpp_find(archive, name, &level->entry);
    if (result != RF_OK) return result;
    result = read_at(level, &cursor, raw, 28);
    if (result != RF_OK) return result;
    if (le32(raw) != 0xd4bada55u || le32(raw + 4) != 180) return RF_FORMAT;
    level->version = le32(raw + 4);
    level->timestamp = le32(raw + 8);
    player_offset = le32(raw + 12);
    info_offset = le32(raw + 16);
    count = le32(raw + 20);
    if (count > RF_LEVEL_MAX_SECTIONS) return RF_RANGE;
    result = read_string(level, &cursor, level->name);
    if (result != RF_OK) return result;
    result = read_string(level, &cursor, level->mod);
    if (result != RF_OK) return result;
    for (i = 0; i < count; ++i) {
        rf_level_section *section = &level->sections[i];
        uint32_t j;
        section->offset = cursor;
        result = read_at(level, &cursor, raw, 8);
        if (result != RF_OK) return result;
        section->type = le32(raw);
        section->size = le32(raw + 4);
        if (!section->type || section->size > level->entry.size - cursor) return RF_FORMAT;
        for (j = 0; j < i; ++j)
            if (level->sections[j].type == section->type) return RF_FORMAT;
        cursor += section->size;
    }
    result = read_at(level, &cursor, raw, 8);
    if (result != RF_OK) return result;
    if (le32(raw) != 0 || le32(raw + 4) != 0 || cursor != level->entry.size) return RF_FORMAT;
    level->section_count = count;
    player = rf_level_find(level, 0x70000);
    info = rf_level_find(level, 0x1000000);
    if (!player || !info || player->offset != player_offset || info->offset != info_offset || player->size != 48)
        return RF_FORMAT;
    result = rf_level_read(level, player, 0, raw, 48);
    if (result != RF_OK) return result;
    for (i = 0; i < 12; ++i) {
        uint32_t bits = le32(raw + 4 * i);
        float value;
        if ((bits & 0x7f800000u) == 0x7f800000u) return RF_FORMAT;
        _Static_assert(sizeof(float) == 4, "RFL requires binary32 floats");
        memcpy(&value, &bits, 4);
        if (i < 3) level->player_position[i] = value;
        else level->player_orientation[((i - 3) / 3 + 2) % 3][(i - 3) % 3] = value;
    }
    return RF_OK;
}

int rf_level_open(rf_level *level, rf_vpp *archive, const char *name)
{
    int result;
    if (!level) return RF_RANGE;
    memset(level, 0, sizeof(*level));
    if (!archive || !name) return RF_RANGE;
    result = load(level, archive, name);
    if (result != RF_OK) memset(level, 0, sizeof(*level));
    return result;
}
