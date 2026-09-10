#include "rf/level.h"
#include "rf/timer.h"
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
#include <math.h>
#include <stdlib.h>

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
int rf_level_regions_begin(const rf_level *level,rf_level_entity_reader *reader)
{
    const rf_level_section *section;rf_level_entity_reader next;uint8_t raw[4];int status;
    if(!level || !reader || level->version!=180)return RF_RANGE;
    section=rf_level_find(level,0xd00);if(!section)return RF_NOT_FOUND;
    memset(&next,0,sizeof(next));next.level=level;next.section=*section;
    status=entity_read(&next,raw,4);if(status)return status;next.count=le32(raw);
    if(section->size<4 || next.count>(section->size-4)/73)return RF_FORMAT;
    *reader=next;return RF_OK;
}
int rf_level_region_next(rf_level_entity_reader *reader,rf_player_movement_region *region)
{
    rf_level_entity_reader r;rf_player_movement_region value;uint8_t raw[48];uint32_t i,bits;int status;
    if(!reader || !region || !reader->level)return RF_RANGE;
    if(reader->index>=reader->count)return reader->index==reader->count && reader->cursor==reader->section.size?RF_NOT_FOUND:RF_FORMAT;
    r=*reader;memset(&value,0,sizeof(value));
    status=entity_skip(&r,4);if(status)return status;
    status=entity_string(&r,NULL);if(status)return status;
    status=entity_read(&r,raw,48);if(status)return status;
    for(i=0;i<12;++i) {
        float v;bits=le32(raw+i*4);memcpy(&v,&bits,4);if(!isfinite(v))return RF_FORMAT;
        if(i<3)value.center[i]=v;else value.matrix[((i-3)/3+2)%3][(i-3)%3]=v;
    }
    status=entity_string(&r,NULL);if(status)return status;
    status=entity_skip(&r,1);if(status)return status;
    status=entity_read(&r,raw,16);if(status)return status;value.kind=(int32_t)le32(raw);
    for(i=0;i<3;++i) {
        bits=le32(raw+4+i*4);memcpy(value.size+i,&bits,4);
        if(!isfinite(value.size[i]) || value.size[i]<0)return RF_FORMAT;
    }
    ++r.index;if(r.index==r.count && r.cursor!=r.section.size)return RF_FORMAT;
    *reader=r;*region=value;return RF_OK;
}

void rf_level_owned_regions_close(rf_level_owned_regions *regions)
{
    if(regions){free(regions->items);memset(regions,0,sizeof(*regions));}
}
int rf_level_owned_regions_open(const rf_level *level,uint32_t budget,rf_level_owned_regions *result)
{
    rf_level_entity_reader reader;rf_player_movement_region record;
    rf_level_owned_regions value={0};uint64_t bytes;uint32_t i;int status;
    if(!level || !result || result->items || result->count || result->allocated_bytes)return RF_RANGE;
    status=rf_level_regions_begin(level,&reader);if(status)return status;
    value.count=reader.count;bytes=sizeof(value)+(uint64_t)value.count*sizeof(*value.items);
    if(bytes>budget)return RF_RANGE;
    while((status=rf_level_region_next(&reader,&record))==RF_OK){}
    if(status!=RF_NOT_FOUND)return status;
    value.allocated_bytes=(uint32_t)bytes;
    if(!value.count){*result=value;return RF_OK;}
    value.items=malloc((size_t)value.count*sizeof(*value.items));if(!value.items)return RF_RANGE;
    status=rf_level_regions_begin(level,&reader);if(status)goto failed;
    if(reader.count!=value.count){status=RF_FORMAT;goto failed;}
    for(i=0;i<value.count;++i) {
        status=rf_level_region_next(&reader,value.items+i);if(status)goto failed;
    }
    *result=value;return RF_OK;
failed:
    rf_level_owned_regions_close(&value);return status;
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

void rf_level_owned_entities_close(rf_level_owned_entities *entities)
{
    if(entities) {free(entities->storage);memset(entities,0,sizeof(*entities));}
}
int rf_level_owned_entities_open(const rf_level *level,uint32_t budget,rf_level_owned_entities *result)
{
    rf_level_owned_entities value={0};rf_level_entity_reader reader;rf_level_entity record;
    uint64_t bytes;uint32_t i;uint8_t *cursor,*end;int status;
    if(!level || !result || result->storage || result->items || result->count || result->allocated_bytes)return RF_RANGE;
    status=rf_level_entities_begin(level,&reader);if(status)return status;
    value.count=reader.count;bytes=sizeof(value)+(uint64_t)value.count*sizeof(*value.items);
    if(bytes>budget)return RF_RANGE;
    while((status=rf_level_entity_next(&reader,&record))==RF_OK) {
        bytes+=record.bytes;if(bytes>budget)return RF_RANGE;
    }
    if(status!=RF_NOT_FOUND)return status;
    value.allocated_bytes=(uint32_t)bytes;
    if(!value.count) {*result=value;return RF_OK;}
    value.storage=calloc(1,(size_t)(bytes-sizeof(value)));if(!value.storage)return RF_RANGE;
    value.items=(rf_level_owned_entity *)value.storage;
    cursor=(uint8_t *)(value.items+value.count);end=(uint8_t *)value.storage+bytes-sizeof(value);
    status=rf_level_entities_begin(level,&reader);if(status)goto failed;
    if(reader.count!=value.count) {status=RF_FORMAT;goto failed;}
    for(i=0;i<value.count;++i) {
        rf_level_owned_entity *item=value.items+i;
        status=rf_level_entity_next(&reader,&item->record);if(status)goto failed;
        if(item->record.bytes>(uint64_t)(end-cursor)) {status=RF_FORMAT;goto failed;}
        item->raw=cursor;
        status=rf_level_read(level,&reader.section,item->record.offset,cursor,item->record.bytes);if(status)goto failed;
        cursor+=item->record.bytes;
    }
    if(cursor!=end) {status=RF_FORMAT;goto failed;}
    *result=value;return RF_OK;
failed:
    rf_level_owned_entities_close(&value);return status;
}

static int entity_raw_skip(uint32_t size,uint32_t *cursor,uint32_t bytes)
{
    if(*cursor>size || bytes>size-*cursor)return RF_RANGE;
    *cursor+=bytes;return RF_OK;
}
static int entity_raw_string(const uint8_t *raw,uint32_t size,uint32_t *cursor)
{
    uint32_t start=*cursor,length;int status=entity_raw_skip(size,cursor,2);if(status)return status;
    length=raw[start]|(uint32_t)raw[start+1]<<8;
    return entity_raw_skip(size,cursor,length);
}
int rf_level_entity_spawn_read(const rf_level_owned_entity *entity,rf_level_entity_spawn *result)
{
    rf_level_entity_spawn value;const uint8_t *raw;uint32_t cursor=0,size,start,i;int status;
    if(!entity || !result || !entity->raw)return RF_RANGE;
    raw=entity->raw;size=entity->record.bytes;
    status=entity_raw_skip(size,&cursor,4);if(status)return status;
    if(le32(raw)!=(uint32_t)entity->record.uid)return RF_FORMAT;
    status=entity_raw_string(raw,size,&cursor);if(status)return status;
    status=entity_raw_skip(size,&cursor,48);if(status)return status;
    status=entity_raw_string(raw,size,&cursor);if(status)return status;
    start=cursor;status=entity_raw_skip(size,&cursor,13);if(status)return status;
    value.relationship_51c=le32(raw+start+1);value.friendliness=le32(raw+start+5);
    value.byte_28=raw[start+9];
    for(i=0;i<2;++i) {status=entity_raw_string(raw,size,&cursor);if(status)return status;}
    status=entity_raw_skip(size,&cursor,29);if(status)return status;
    for(i=0;i<7;++i) {status=entity_raw_string(raw,size,&cursor);if(status)return status;}
    status=entity_raw_skip(size,&cursor,18);if(status)return status;
    start=cursor;status=entity_raw_skip(size,&cursor,17);if(status)return status;
    value.creation_flags=(raw[start+1]?2u:0u)|(raw[start+15]?4u:0u);
    if(raw[start+16]>1)return RF_FORMAT;
    if(raw[start+16]) {status=entity_raw_skip(size,&cursor,4);if(status)return status;}
    for(i=0;i<2;++i) {status=entity_raw_string(raw,size,&cursor);if(status)return status;}
    if(cursor!=size)return RF_FORMAT;
    *result=value;return RF_OK;
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

/* Bounded v180 moving-group file reader, separate from runtime controllers. */
static int group_read(rf_level_group_reader *r,void *out,uint32_t size)
{
    int status;
    if(r->cursor>r->section.size || size>r->section.size-r->cursor)return RF_FORMAT;
    status=rf_level_read(r->level,&r->section,r->cursor,out,size);
    if(!status)r->cursor+=size;
    return status;
}
static int group_number(rf_level_group_reader *r,uint32_t *out)
{
    unsigned char raw[4];int status=group_read(r,raw,4);if(!status)*out=le32(raw);return status;
}
static int group_string(rf_level_group_reader *r,char out[256])
{
    unsigned char raw[2];uint32_t size;int status=group_read(r,raw,2);if(status)return status;
    size=raw[0]|(uint32_t)raw[1]<<8;if(size>=256)return RF_RANGE;
    status=group_read(r,out,size);if(status)return status;
    if(memchr(out,0,size))return RF_FORMAT;
    out[size]=0;return RF_OK;
}
static int group_floats(rf_level_group_reader *r,float *out,uint32_t count)
{
    uint32_t i,bits;int status;
    for(i=0;i<count;i++) {
        status=group_number(r,&bits);if(status)return status;
        if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;
        memcpy(out+i,&bits,4);
    }
    return RF_OK;
}
static int trigger_byte(rf_level_trigger_reader *r,uint32_t *out)
{
    unsigned char byte;int status=group_read(r,&byte,1);if(!status)*out=byte;return status;
}
int rf_level_triggers_begin(const rf_level *level,rf_level_trigger_reader *reader)
{
    rf_level_trigger_reader next={0};const rf_level_section *section;int status;
    if(!level || !reader)return RF_RANGE;
    if(level->version!=180)return RF_FORMAT;
    section=rf_level_find(level,0x60000);if(!section)return RF_NOT_FOUND;
    next.level=level;next.section=*section;
    status=group_number(&next,&next.count);if(status)return status;
    if((uint64_t)next.count*4>section->size-next.cursor || (!next.count && next.cursor!=section->size))return RF_FORMAT;
    *reader=next;return RF_OK;
}
int rf_level_trigger_next(rf_level_trigger_reader *reader,rf_level_trigger *trigger)
{
    rf_level_trigger_reader next;rf_level_trigger value={0};uint32_t i;int status;
    if(!reader || !trigger || !reader->level || reader->section.type!=0x60000)return RF_RANGE;
    if(reader->index>=reader->count)return reader->index==reader->count && reader->cursor==reader->section.size?RF_NOT_FOUND:RF_FORMAT;
    next=*reader;value.offset=next.cursor;
    if((status=group_number(&next,&value.uid)) || (status=group_string(&next,value.name)) ||
        (status=trigger_byte(&next,&value.header_byte)) || (status=group_number(&next,&value.shape)) ||
        (status=group_floats(&next,&value.timing,1)) || (status=group_number(&next,&value.unknown_word)) ||
        (status=trigger_byte(&next,&value.flags[0])) || (status=group_string(&next,value.script)) ||
        (status=trigger_byte(&next,&value.flags[1])) || (status=trigger_byte(&next,&value.value_byte)))return status;
    for(i=2;i<5;++i)if((status=trigger_byte(&next,value.flags+i)))return status;
    if((status=group_floats(&next,value.position,3)))return status;
    if(value.shape==0) {
        if((status=group_floats(&next,&value.radius,1)))return status;
    } else if(value.shape==1) {
        if((status=group_floats(&next,value.orientation_disk,9)) || (status=group_floats(&next,value.dimensions_disk,3)) ||
            (status=trigger_byte(&next,&value.box_flag)))return status;
    } else return RF_FORMAT;
    for(i=0;i<3;++i)if((status=group_number(&next,value.fields+i)))return status;
    if((status=trigger_byte(&next,&value.tail_flag)) || (status=group_floats(&next,value.values,2)) ||
        (status=group_number(&next,&value.tail_word)) || (status=group_number(&next,&value.link_count)))return status;
    value.link_offset=next.cursor;
    if((uint64_t)value.link_count*4>next.section.size-next.cursor)return RF_FORMAT;
    next.cursor+=value.link_count*4;value.bytes=next.cursor-value.offset;++next.index;
    if(next.index==next.count && next.cursor!=next.section.size)return RF_FORMAT;
    *reader=next;*trigger=value;return RF_OK;
}
int rf_level_trigger_link(const rf_level *level,const rf_level_trigger *trigger,uint32_t index,uint32_t *uid)
{
    const rf_level_section *section;unsigned char raw[4];uint64_t offset;int status;
    if(!level || !trigger || !uid || index>=trigger->link_count)return RF_RANGE;
    section=rf_level_find(level,0x60000);if(!section)return RF_NOT_FOUND;
    offset=(uint64_t)trigger->link_offset+(uint64_t)index*4;
    if(offset>section->size || section->size-offset<4)return RF_RANGE;
    status=rf_level_read(level,section,(uint32_t)offset,raw,4);if(!status)*uid=le32(raw);return status;
}
static int group_key(rf_level_group_reader *r,rf_level_group_key *key)
{
    float disk[9];uint32_t i;int status;
    memset(key,0,sizeof(*key));key->offset=r->cursor;
    if((status=group_number(r,&key->uid)) || (status=group_floats(r,key->position,3)) ||
       (status=group_floats(r,disk,9)) || (status=group_string(r,key->label)) ||
       (status=group_read(r,&key->flag,1)) || (status=group_floats(r,key->timing,5)))return status;
    for(i=0;i<9;i++)key->orientation[i/3][i%3]=disk[(i+3)%9];
    for(i=0;i<3;i++)if((status=group_number(r,key->links+i)))return status;
    status=group_floats(r,&key->rotation,1);key->bytes=r->cursor-key->offset;return status;
}
int rf_level_groups_begin(const rf_level *level,rf_level_group_reader *reader)
{
    rf_level_group_reader value={0};const rf_level_section *section;int status;
    if(!level || !reader)return RF_RANGE;
    if(level->version!=180)return RF_FORMAT;
    section=rf_level_find(level,0x3000);if(!section)return RF_NOT_FOUND;
    value.level=level;value.section=*section;status=group_number(&value,&value.count);if(status)return status;
    if((uint64_t)value.count*48>section->size-value.cursor)return RF_FORMAT;
    if(!value.count && value.cursor!=section->size)return RF_FORMAT;
    *reader=value;return RF_OK;
}
int rf_level_group_next(rf_level_group_reader *reader,rf_level_group *group)
{
    rf_level_group_reader r;rf_level_group g={0};rf_level_group_key key;uint32_t i,j;int status;
    if(!reader || !reader->level || !group)return RF_RANGE;
    r=*reader;
    if(r.index>=r.count)return r.cursor==r.section.size?RF_NOT_FOUND:RF_FORMAT;
    g.offset=r.cursor;
    if((status=group_string(&r,g.name)) || (status=group_read(&r,g.header,2)) ||
       (status=group_number(&r,&g.key_count)))return status;
    g.key_offset=r.cursor;
    if((uint64_t)g.key_count*91>r.section.size-r.cursor)return RF_FORMAT;
    for(i=0;i<g.key_count;i++)if((status=group_key(&r,&key)))return status;
    if((status=group_number(&r,&g.legacy_count)))return status;
    g.legacy_offset=r.cursor;
    if((uint64_t)g.legacy_count*52>r.section.size-r.cursor)return RF_FORMAT;
    for(i=0;i<g.legacy_count;i++) {
        uint32_t uid;float pose[12];
        if((status=group_number(&r,&uid)) || (status=group_floats(&r,pose,12)))return status;
    }
    if((status=group_read(&r,g.flags,6)) || (status=group_number(&r,&g.mode)) ||
       (status=group_number(&r,&g.unknown)))return status;
    for(i=0;i<4;i++)if((status=group_string(&r,g.sounds[i])) || (status=group_floats(&r,&g.sound_values[i],1)))return status;
    for(i=0;i<2;i++) {
        if((status=group_number(&r,g.ids_count+i)))return status;
        g.ids_offset[i]=r.cursor;
        if((uint64_t)g.ids_count[i]*4>r.section.size-r.cursor)return RF_FORMAT;
        for(j=0;j<g.ids_count[i];j++) {uint32_t uid;if((status=group_number(&r,&uid)))return status;}
    }
    g.bytes=r.cursor-g.offset;r.index++;
    if(r.index==r.count && r.cursor!=r.section.size)return RF_FORMAT;
    *group=g;*reader=r;return RF_OK;
}
int rf_level_group_key_at(const rf_level *level,const rf_level_group *group,uint32_t index,rf_level_group_key *key)
{
    rf_level_group_reader r;rf_level_group_key value={0};uint32_t i;int status;
    if(!group || !key || index>=group->key_count)return RF_RANGE;
    status=rf_level_groups_begin(level,&r);if(status)return status;
    r.cursor=group->key_offset;
    for(i=0;i<=index;i++)if((status=group_key(&r,&value)))return status;
    *key=value;return RF_OK;
}
int rf_level_group_id_at(const rf_level *level,const rf_level_group *group,uint32_t list,uint32_t index,uint32_t *uid)
{
    rf_level_group_reader r;uint64_t offset;uint32_t value;int status;
    if(!group || !uid || list>=2 || index>=group->ids_count[list])return RF_RANGE;
    status=rf_level_groups_begin(level,&r);if(status)return status;
    offset=(uint64_t)group->ids_offset[list]+(uint64_t)index*4;
    if(offset>r.section.size)return RF_FORMAT;
    r.cursor=(uint32_t)offset;status=group_number(&r,&value);if(!status)*uid=value;return status;
}

void rf_level_owned_groups_close(rf_level_owned_groups *groups)
{
    if(groups) {free(groups->storage);memset(groups,0,sizeof(*groups));}
}
int rf_level_owned_groups_open(const rf_level *level,uint32_t budget,rf_level_owned_groups *result)
{
    rf_level_owned_groups value={0};rf_level_group_reader reader,fields;
    rf_level_group record;uint64_t bytes,part;uint32_t i,j,list;int status;
    unsigned char *cursor,*end;
    if(!level || !result)return RF_RANGE;
    status=rf_level_groups_begin(level,&reader);if(status)return status;
    value.count=reader.count;bytes=sizeof(value)+(uint64_t)value.count*sizeof(*value.groups);
    if(bytes>budget)return RF_RANGE;
    while((status=rf_level_group_next(&reader,&record))==RF_OK) {
        bytes+=(uint64_t)record.key_count*sizeof(rf_level_group_key)+(uint64_t)record.legacy_count*sizeof(rf_level_group_legacy)+
               ((uint64_t)record.ids_count[0]+record.ids_count[1])*4;
        if(bytes>budget)return RF_RANGE;
    }
    if(status!=RF_NOT_FOUND)return status;
    value.allocated_bytes=(uint32_t)bytes;
    if(bytes>sizeof(value)) {
        value.storage=calloc(1,(size_t)(bytes-sizeof(value)));if(!value.storage)return RF_RANGE;
        value.groups=(rf_level_owned_group *)value.storage;
    }
    if(!value.count) {*result=value;return RF_OK;}
    cursor=(unsigned char *)(value.groups+value.count);end=(unsigned char *)value.storage+bytes-sizeof(value);
    status=rf_level_groups_begin(level,&reader);if(status)goto failed;
    if(reader.count!=value.count) {status=RF_FORMAT;goto failed;}
    for(i=0;i<value.count;i++) {
        rf_level_owned_group *g=value.groups+i;
        status=rf_level_group_next(&reader,&record);if(status)goto failed;
        part=(uint64_t)record.key_count*sizeof(*g->keys)+(uint64_t)record.legacy_count*sizeof(*g->legacy)+
             ((uint64_t)record.ids_count[0]+record.ids_count[1])*4;
        if(part>(uint64_t)(end-cursor)) {status=RF_FORMAT;goto failed;}
        g->record=record;g->keys=(rf_level_group_key *)cursor;cursor+=record.key_count*sizeof(*g->keys);
        g->legacy=(rf_level_group_legacy *)cursor;cursor+=record.legacy_count*sizeof(*g->legacy);
        fields=reader;fields.cursor=record.key_offset;
        for(j=0;j<record.key_count;j++)if((status=group_key(&fields,g->keys+j)))goto failed;
        fields.cursor=record.legacy_offset;
        for(j=0;j<record.legacy_count;j++)if((status=group_number(&fields,&g->legacy[j].uid)) ||
            (status=group_floats(&fields,g->legacy[j].pose,12)))goto failed;
        for(list=0;list<2;list++) {
            g->ids[list]=(uint32_t *)cursor;cursor+=record.ids_count[list]*4;fields.cursor=record.ids_offset[list];
            for(j=0;j<record.ids_count[list];j++)if((status=group_number(&fields,g->ids[list]+j)))goto failed;
        }
    }
    if(cursor!=end) {status=RF_FORMAT;goto failed;}
    *result=value;return RF_OK;
 failed:
    rf_level_owned_groups_close(&value);return status;
}
int rf_level_group_initial_flags(const rf_level_group *group,
    const rf_level_group_key *first,uint32_t *flags)
{
    uint32_t value,i,bits;
    if(!group || !first || !flags)return RF_RANGE;
    if(!group->key_count)return RF_NOT_FOUND;
    for(i=3;i<5;i++) {
        memcpy(&bits,first->timing+i,4);
        if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;
    }
    value=group->flags[2]?0x80000100u:0x80002000u;
    if(group->flags[0])value|=2;
    if(group->flags[1]) {
        value|=4;
        if(first->timing[3]!=0 || first->timing[4]!=0)value|=0x40;
    }
    for(i=3;i<6;i++)if(group->flags[i])value|=1u<<(i+7);
    *flags=value;return RF_OK;
}

static uint32_t group_object_find(const rf_group_object *objects,uint32_t count,uint32_t uid)
{
    uint32_t i;
    if(uid==UINT32_MAX)return UINT32_MAX;
    for(i=0;i<count;i++)if((uint32_t)objects[i].uid==uid &&
        (uid!=(uint32_t)-999 || !(objects[i].flags&2)))return i;
    return UINT32_MAX;
}
int rf_group_motion_activate(rf_group_motion_state *state,uint32_t key_count)
{
    if(!state)return RF_RANGE;
    if(state->next_key!=-1)return RF_OK;
    if(!key_count || key_count>INT32_MAX || state->current_key<0 ||
        (uint32_t)state->current_key>=key_count || (!(state->flags&4) && key_count<2))return RF_RANGE;
    if(state->flags&4) {
        state->next_key=0;
        if(state->flags&0x40) {state->phase=0;state->flags=(state->flags&~0x20u)|0x10;}
    } else if(state->flags&0x2000) {
        state->next_key=state->current_key+1;
        if((uint32_t)state->next_key>=key_count)state->next_key=0;
        state->terminal_key=state->mode==1?(int32_t)key_count-1:-1;
    } else {
        state->next_key=state->current_key-1;
        if(state->next_key<0)state->next_key=(int32_t)key_count-1;
        state->terminal_key=state->mode==1?0:-1;
    }
    state->flags|=8;if(state->mode==1)state->flags&=~1u;
    return RF_OK;
}
int rf_group_translation_arrive(rf_group_motion_state *state,uint32_t key_count,
    uint32_t *sound_requests)
{
    int32_t last;uint32_t sound=0;
    if(!state || !sound_requests || key_count<2 || key_count>INT32_MAX ||
        state->next_key<0 || (uint32_t)state->next_key>=key_count ||
        state->current_key!=state->next_key || state->mode>5 || (state->flags&4))return RF_RANGE;
    last=(int32_t)key_count-1;
    if(state->terminal_key==state->next_key) {
        state->current_key=state->next_key;state->next_key=-1;state->phase=0;
        if(!(state->flags&1))sound=RF_GROUP_SOUND_END;
        if(state->current_key==last)state->flags&=~0x2000u;
        else if(state->current_key==0)state->flags|=0x2000;
    } else {
        int forward=(state->flags&0x2000)!=0;
        state->next_key+=forward?1:-1;
        if(state->next_key<0 || state->next_key>last) {
            switch(state->mode) {
            case 1:
                state->current_key=forward?last:0;state->next_key=-1;
                state->flags^=0x2000;break;
            case 2:case 3:
                state->current_key=forward?last:0;state->next_key=forward?last-1:1;
                state->flags^=0x2000;
                if(state->mode==2)state->terminal_key=forward?0:last;
                break;
            case 4:case 5:
                state->current_key=forward?last:0;state->next_key=forward?0:last;
                state->terminal_key=state->mode==4?state->next_key:-1;break;
            }
        }
        if(state->flags&1) {sound=RF_GROUP_SOUND_START;state->flags&=~1u;}
        state->phase=0;
    }
    *sound_requests=sound;return RF_OK;
}
int rf_group_translation_integrate(const rf_group_translation_step *step,
    rf_group_translation_progress *result)
{
    rf_group_translation_progress out;float delta[3],target,braking;
    double length,acceleration=0,threshold;uint32_t i,bits;
    if(!step || !result)return RF_RANGE;
    for(i=0;i<14;i++)if(i!=10) {
        memcpy(&bits,(const unsigned char *)step+4*i,4);
        if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;
    }
    for(i=0;i<3;i++)delta[i]=step->from[i]-step->to[i];
    length=sqrt((double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2]);
    out.length=(float)length;
    target=(step->flags&0x400)?step->timing:(float)(length/step->timing);
    if(step->acceleration_time>0 && step->elapsed<=step->acceleration_time)
        acceleration=(double)target/step->acceleration_time;
    if(step->deceleration_time>0) {
        braking=(float)((double)target/step->deceleration_time);
        threshold=(double)out.length-(double)step->deceleration_time*step->deceleration_time*braking*.5;
        if((!(step->flags&0x2000) || threshold>0) && threshold<=step->distance)acceleration=-(double)braking;
    }
    if(acceleration==0 && step->elapsed==0)out.speed=target;
    else {
        out.speed=(float)((double)step->dt*acceleration+step->speed);
        if(out.speed<.4f)out.speed=.4f;
        else if(out.speed>target)out.speed=target;
    }
    out.elapsed=(float)((double)step->dt+step->elapsed);
    out.distance=(float)((double)step->dt*out.speed+step->distance);
    for(i=0;i<4;i++) {memcpy(&bits,(const unsigned char *)&out+4*i,4);if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;}
    *result=out;return RF_OK;
}
int rf_group_translation_position(const rf_group_translation_step *step,
    const rf_group_translation_progress *progress,const float position[3],
    float pending[3],uint32_t *arrival)
{
    float out[3],direction[3],velocity,displacement;double inverse;
    uint32_t i,bits,due;
    if(!step || !progress || !position || !pending || !arrival)return RF_RANGE;
    for(i=0;i<14;i++)if(i!=10) {
        memcpy(&bits,(const unsigned char *)step+4*i,4);
        if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;
    }
    for(i=0;i<4;i++) {memcpy(&bits,(const unsigned char *)progress+4*i,4);if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;}
    for(i=0;i<3;i++) {memcpy(&bits,position+i,4);if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;}
    due=step->timing==0 || (step->flags&1) || step->distance>=progress->length;
    if(due || progress->distance>=progress->length)memcpy(out,step->to,sizeof(out));
    else {
        for(i=0;i<3;i++)direction[i]=step->to[i]-step->from[i];
        inverse=1./sqrt((double)direction[0]*direction[0]+(double)direction[1]*direction[1]+(double)direction[2]*direction[2]);
        for(i=0;i<3;i++) {
            direction[i]=(float)(direction[i]*inverse);
            velocity=direction[i]*progress->speed;displacement=velocity*step->dt;
            out[i]=position[i]+displacement;
        }
    }
    for(i=0;i<3;i++) {memcpy(&bits,out+i,4);if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;}
    memcpy(pending,out,sizeof(out));*arrival=due;return RF_OK;
}
int rf_group_translation_tick_begin(rf_group_translation_runtime *runtime,
    const rf_level_group_key *keys,uint32_t key_count,float dt,int32_t now_ms,
    rf_group_translation_frame *frame)
{
    rf_group_translation_runtime next;rf_group_translation_frame f={0};
    const rf_level_group_key *from,*to;int status,expired;
    if(!runtime || !frame || (runtime->motion.flags&4))return RF_RANGE;
    next=*runtime;memset(next.velocity,0,sizeof(next.velocity));f.now_ms=now_ms;
    if((next.motion.flags&0x80) || next.motion.next_key==-1) {
        f.stage=RF_GROUP_TICK_IDLE;*runtime=next;*frame=f;return RF_OK;
    }
    if(!keys || next.motion.current_key<0 || next.motion.next_key<0 ||
        (uint32_t)next.motion.current_key>=key_count || (uint32_t)next.motion.next_key>=key_count)return RF_RANGE;
    from=keys+next.motion.current_key;to=keys+next.motion.next_key;
    next.motion.flags|=0x4008;next.object_flags|=0x4000000;
    memcpy(f.step.from,from->position,12);memcpy(f.step.to,to->position,12);
    f.step.timing=(next.motion.flags&0x2000)?from->timing[1]:to->timing[2];
    f.step.acceleration_time=from->timing[3];f.step.deceleration_time=from->timing[4];
    f.step.dt=dt;f.step.flags=next.motion.flags;f.step.speed=next.speed;
    f.step.elapsed=next.motion.phase;f.step.distance=next.distance;f.dwell=to->timing[0];
    status=rf_group_translation_integrate(&f.step,&f.progress);if(status)return status;
    next.speed=f.progress.speed;next.motion.phase=f.progress.elapsed;next.distance=f.progress.distance;
    status=rf_timer_expired(next.deadline,now_ms,&expired);if(status)return status;
    f.stage=expired?RF_GROUP_TICK_GATES:RF_GROUP_TICK_WAIT;
    *runtime=next;*frame=f;return RF_OK;
}
int rf_group_translation_tick_move(rf_group_translation_runtime *runtime,
    rf_group_translation_frame *frame)
{
    rf_group_translation_runtime next;uint32_t arrival;int status;
    if(!runtime || !frame || frame->stage!=RF_GROUP_TICK_GATES)return RF_RANGE;
    next=*runtime;
    status=rf_group_translation_position(&frame->step,&frame->progress,next.position,next.pending,&arrival);
    if(status)return status;
    if(arrival) {next.speed=0;next.distance=0;next.motion.current_key=next.motion.next_key;}
    *runtime=next;frame->stage=arrival?RF_GROUP_TICK_ARRIVAL:RF_GROUP_TICK_DONE;return RF_OK;
}
int rf_group_translation_tick_finish(rf_group_translation_runtime *runtime,
    rf_group_translation_frame *frame,uint32_t key_count,uint32_t *sounds)
{
    rf_group_translation_runtime next;uint32_t bits,requests;double delay;int status;
    if(!runtime || !frame || !sounds || frame->stage!=RF_GROUP_TICK_ARRIVAL)return RF_RANGE;
    next=*runtime;memcpy(&bits,&frame->dwell,4);if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;
    if(!(next.motion.flags&1) && frame->dwell>0) {
        delay=(double)frame->dwell*1000.+.5;
        if(delay>RF_TIMER_PERIOD)return RF_RANGE;
        status=rf_timer_set(&next.deadline,frame->now_ms,(int32_t)delay);if(status)return status;
        next.motion.flags|=1;requests=RF_GROUP_SOUND_END;
    } else {
        status=rf_group_translation_arrive(&next.motion,key_count,&requests);if(status)return status;
    }
    *runtime=next;*sounds=requests;frame->stage=RF_GROUP_TICK_DONE;return RF_OK;
}
static int group_finite(const void *values,uint32_t count)
{
    uint32_t i,bits;
    for(i=0;i<count;i++) {memcpy(&bits,(const unsigned char *)values+4*i,4);if((bits&0x7f800000u)==0x7f800000u)return 0;}
    return 1;
}
int rf_group_pose_set_position(rf_group_attached_pose *pose,const float position[3])
{
    rf_group_attached_pose next;uint32_t j;
    if(!pose || !position)return RF_RANGE;
    if(!group_finite(position,3) || !group_finite(&pose->radius,1))return RF_FORMAT;
    next=*pose;memcpy(next.position,position,12);memcpy(next.public_position,position,12);memcpy(next.pending,position,12);
    for(j=0;j<3;j++) {
        next.minimum[j]=next.radius>0?next.position[j]-next.radius:next.position[j];
        next.maximum[j]=next.radius>0?next.position[j]+next.radius:next.position[j];
    }
    if(!group_finite(next.minimum,6))return RF_FORMAT;
    next.flags|=0x4000000;*pose=next;return RF_OK;
}
int rf_group_controller_pose(const rf_level_group_key *first,rf_group_attached_pose *pose)
{
    rf_group_attached_pose value={0};
    if(!first || !pose)return RF_RANGE;
    if(!group_finite(first->position,12))return RF_FORMAT;
    value.flags=0x6000001;memcpy(value.base_position,first->position,12);memcpy(value.base_matrix,first->orientation,36);
    memcpy(value.position,first->position,12);memcpy(value.public_position,first->position,12);memcpy(value.pending,first->position,12);
    memcpy(value.input_matrix,first->orientation,36);memcpy(value.output_matrix,first->orientation,36);memcpy(value.pending_matrix,first->orientation,36);
    memcpy(value.minimum,first->position,12);memcpy(value.maximum,first->position,12);
    *pose=value;return RF_OK;
}
int rf_group_translation_initialize(rf_group_translation_runtime *runtime,
    rf_group_attached_pose *pose,uint32_t flags,uint32_t mode,
    const rf_level_group_key *selected,uint32_t index,uint32_t key_count,int32_t now_ms)
{
    rf_group_translation_runtime value={0};rf_group_attached_pose next;int status;
    if(!runtime || !pose || !selected || flags&4 || mode>5 || !key_count ||
       key_count>INT32_MAX || index>=key_count)return RF_RANGE;
    status=rf_timer_set(&value.deadline,now_ms,0);if(status)return status;
    next=*pose;status=rf_group_pose_set_position(&next,selected->position);if(status)return status;
    memset(next.velocity,0,12);value.motion.flags=flags;value.motion.mode=mode;
    value.motion.current_key=(int32_t)index;value.motion.next_key=-1;value.motion.terminal_key=-1;
    value.object_flags=next.flags;memcpy(value.position,next.position,12);memcpy(value.pending,next.pending,12);
    *pose=next;*runtime=value;return RF_OK;
}
int rf_group_translation_propagate(rf_group_attached_pose *pose,
    const rf_group_translation_contribution *contributions,uint32_t count,
    float dt,uint32_t force)
{
    rf_group_attached_pose next;float target[3],delta,displacement;uint32_t i,j,dirty=0;
    if(!pose || count>4 || (count && !contributions))return RF_RANGE;
    for(i=0;i<count;i++)dirty|=contributions[i].flags&0x80000008u;
    if(!count || (!force && !dirty))return RF_OK;
    if(!group_finite(&pose->radius,13) || (!force && (!group_finite(pose->position,3) || !group_finite(&dt,1) || dt==0)))return RF_FORMAT;
    for(i=0;i<count;i++) {
        if(contributions[i].flags&0x804)return RF_RANGE;
        if(!group_finite(contributions[i].first_key,6))return RF_FORMAT;
    }
    next=*pose;memcpy(target,pose->base_position,12);
    for(i=0;i<count;i++)for(j=0;j<3;j++) {
        delta=contributions[i].pending[j]-contributions[i].first_key[j];target[j]=target[j]+delta;
    }
    next.flags|=0x4000000;
    memcpy(next.input_matrix,next.base_matrix,36);memcpy(next.output_matrix,next.base_matrix,36);memcpy(next.pending_matrix,next.base_matrix,36);
    if(force) {
        memcpy(next.position,target,12);memcpy(next.public_position,target,12);memcpy(next.pending,target,12);memset(next.velocity,0,12);
    } else for(j=0;j<3;j++) {
        delta=target[j]-next.position[j];next.velocity[j]=delta/dt;
        displacement=next.velocity[j]*dt;next.pending[j]=next.position[j]+displacement;
    }
    for(j=0;j<3;j++) {
        next.minimum[j]=(next.position[j]<next.pending[j]?next.position[j]:next.pending[j])-next.radius;
        next.maximum[j]=(next.position[j]>next.pending[j]?next.position[j]:next.pending[j])+next.radius;
    }
    if(!group_finite(next.pending,6) || !group_finite(next.minimum,6))return RF_FORMAT;
    *pose=next;return RF_OK;
}
int rf_group_commit_positions(uint32_t *flags,rf_group_attached_pose *controller,
    const rf_group_controller_view *bindings,const rf_group_pose_slot *slots,
    uint32_t slot_count)
{
    uint32_t pass,list,j;int status;rf_group_attached_pose test;
    if(!flags)return RF_RANGE;
    if(!(*flags&0x80000008u))return RF_OK;
    if(!controller || !bindings || slot_count>1024 || (slot_count && !slots) ||
       (bindings->mover_count && !bindings->mover_handles) ||
       (bindings->general_count && !bindings->general_handles))return RF_RANGE;
    for(pass=0;pass<2;pass++) {
        rf_group_attached_pose *target=controller;
        if(!pass) {test=*target;target=&test;}
        status=rf_group_pose_set_position(target,target->pending);if(status)return status;
        for(list=0;list<2;list++) {
            const uint32_t *handles=list?bindings->general_handles:bindings->mover_handles;
            uint32_t length=list?bindings->general_count:bindings->mover_count;
            for(j=0;j<length;j++) {
                uint32_t handle=handles[j],index=handle&0xffffu;
                if(handle==UINT32_MAX || index>=slot_count || !slots[index].pose || slots[index].handle!=handle)continue;
                target=slots[index].pose;if(!pass) {test=*target;target=&test;}
                status=rf_group_pose_set_position(target,target->pending);if(status)return status;
            }
        }
    }
    *flags&=~0x80000008u;return RF_OK;
}
int rf_group_translation_bind_pose(rf_group_attached_pose *pose,uint32_t handle,
    const rf_group_controller_view *controllers,uint32_t count,float dt,uint32_t force)
{
    const rf_group_controller_view *selected[4];rf_group_translation_contribution values[4];
    uint32_t i,j,list,n=0,dirty=0;
    if(!pose || handle==UINT32_MAX || (handle&0xffffu)>=1024 || (count && !controllers))return RF_RANGE;
    for(i=0;i<count;i++) {
        const rf_group_controller_view *c=controllers+i;
        if(!c->runtime || (c->mover_count && !c->mover_handles) || (c->general_count && !c->general_handles))return RF_RANGE;
        for(list=0;list<2;list++) {
            const uint32_t *handles=list?c->general_handles:c->mover_handles;
            uint32_t length=list?c->general_count:c->mover_count;
            if(list && (pose->flags&0x8000000))continue;
            for(j=0;j<length;j++)if(handles[j]==handle) {
                if(n==4)return RF_RANGE;
                selected[n++]=c;dirty|=c->runtime->motion.flags&0x80000008u;
            }
        }
    }
    if(!n || (!force && !dirty))return RF_OK;
    for(i=0;i<n;i++) {
        const rf_group_controller_view *c=selected[i];if(!c->first_key)return RF_RANGE;
        memcpy(values[i].first_key,c->first_key->position,12);memcpy(values[i].pending,c->runtime->pending,12);values[i].flags=c->runtime->motion.flags;
    }
    return rf_group_translation_propagate(pose,values,n,dt,force);
}
int rf_group_attach_movers(rf_group_object *objects,uint32_t object_count,
    uint32_t controller_handle,uint32_t controller_flags,uint32_t global_mode,
    uint32_t *refs,uint32_t *ref_count,uint32_t *handles,uint32_t *handle_count,
    uint32_t handle_capacity,float *rotation)
{
    uint32_t i,accepted=0,bits,at=0,h;
    if(!ref_count || !handle_count || !rotation || (object_count && !objects) ||
        (*ref_count && !refs) || *handle_count>handle_capacity)return RF_RANGE;
    memcpy(&bits,rotation,4);if((bits&0x7f800000u)==0x7f800000u)return RF_FORMAT;
    for(i=0;i<*ref_count;i++) {
        uint32_t index=group_object_find(objects,object_count,refs[i]);
        if(index!=UINT32_MAX && objects[index].type==9)accepted++;
    }
    if(accepted>handle_capacity-*handle_count || (accepted && !handles))return RF_RANGE;
    h=*handle_count;
    for(i=0;i<*ref_count;i++) {
        uint32_t index=group_object_find(objects,object_count,refs[i]);rf_group_object *object;
        if(index==UINT32_MAX || objects[index].type!=9)continue;
        object=objects+index;refs[at++]=refs[i];handles[h++]=object->handle;
        object->parent=controller_handle;if(controller_flags&0x1000)object->flags|=0x40000;
        if((controller_flags&4) && !global_mode && !(controller_flags&0x2100))*rotation=-*rotation;
    }
    *ref_count=at;*handle_count=h;return RF_OK;
}

void rf_group_runtime_close(rf_group_runtime_collection *runtime)
{
    if(runtime) {free(runtime->items);memset(runtime,0,sizeof(*runtime));}
}
int rf_group_runtime_open(const rf_level_owned_groups *source,int32_t now_ms,
    uint32_t budget,rf_group_runtime_collection *result)
{
    rf_group_runtime_collection value={0};uint64_t bytes;uint32_t i;int status;int32_t clock_check;
    if(!source || !result || (source->count && !source->groups))return RF_RANGE;
    status=rf_timer_set(&clock_check,now_ms,0);if(status)return status;
    bytes=sizeof(value)+(uint64_t)source->count*sizeof(*value.items);if(bytes>budget)return RF_RANGE;
    value.count=source->count;value.allocated_bytes=(uint32_t)bytes;
    if(value.count) {value.items=calloc(value.count,sizeof(*value.items));if(!value.items)return RF_RANGE;}
    for(i=0;i<value.count;i++) {
        rf_group_runtime_entry *entry=value.items+i;const rf_level_owned_group *g=source->groups+i;
        entry->source=g;if(!g->record.key_count)continue;
        if(!g->keys) {status=RF_RANGE;goto failed;}
        status=rf_level_group_initial_flags(&g->record,g->keys,&entry->initial_flags);if(status)goto failed;
        status=rf_group_controller_pose(g->keys,&entry->pose);if(status)goto failed;
        if(entry->initial_flags&4) {entry->kind=RF_GROUP_RUNTIME_ROTATION_PENDING;continue;}
        if(g->record.unknown>=g->record.key_count) {status=RF_RANGE;goto failed;}
        status=rf_group_translation_initialize(&entry->translation,&entry->pose,entry->initial_flags,
            g->record.mode>5?1:g->record.mode,g->keys+g->record.unknown,g->record.unknown,g->record.key_count,now_ms);
        if(status)goto failed;entry->kind=RF_GROUP_RUNTIME_TRANSLATION;
    }
    *result=value;return RF_OK;
 failed:
    rf_group_runtime_close(&value);return status;
}

void rf_group_mover_memberships_close(rf_group_mover_memberships *memberships)
{
    if(memberships) {free(memberships->storage);memset(memberships,0,sizeof(*memberships));}
}
int rf_group_mover_memberships_open(const rf_group_runtime_collection *runtime,
    rf_group_object *objects,uint32_t object_count,const uint32_t *controller_handles,
    uint32_t global_mode,uint32_t budget,rf_group_mover_memberships *result)
{
    rf_group_mover_memberships value={0};uint64_t bytes,scratch_bytes,total=0;uint32_t i,max_refs=0,*cursor,*refs=NULL;
    rf_group_object *copy=NULL;void *scratch=NULL;int status;
    if(!runtime || !result || (runtime->count && (!runtime->items || !controller_handles)) || (object_count && !objects))return RF_RANGE;
    for(i=0;i<runtime->count;i++) {
        const rf_group_runtime_entry *e=runtime->items+i;const rf_level_owned_group *g=e->source;uint32_t n;
        if(!g)return RF_RANGE;if(e->kind==RF_GROUP_RUNTIME_EMPTY)continue;
        if(controller_handles[i]==UINT32_MAX || (controller_handles[i]&0xffffu)>=1024)return RF_RANGE;
        n=g->record.ids_count[1];if(n && !g->ids[1])return RF_RANGE;
        total+=n;if(n>max_refs)max_refs=n;
    }
    bytes=sizeof(value)+(uint64_t)runtime->count*sizeof(*value.items)+total*4;
    scratch_bytes=(uint64_t)object_count*sizeof(*objects)+(uint64_t)max_refs*4;
    if(bytes+scratch_bytes>budget)return RF_RANGE;
    value.count=runtime->count;value.allocated_bytes=(uint32_t)bytes;value.peak_bytes=(uint32_t)(bytes+scratch_bytes);
    if(bytes>sizeof(value)) {value.storage=calloc(1,(size_t)(bytes-sizeof(value)));if(!value.storage)return RF_RANGE;value.items=value.storage;}
    if(scratch_bytes) {
        scratch=malloc((size_t)scratch_bytes);if(!scratch) {status=RF_RANGE;goto failed;}
        copy=scratch;refs=(uint32_t *)((unsigned char *)scratch+(size_t)object_count*sizeof(*objects));
        if(object_count)memcpy(copy,objects,(size_t)object_count*sizeof(*objects));
    }
    cursor=value.count?(uint32_t *)(value.items+value.count):NULL;
    for(i=0;i<value.count;i++) {
        const rf_group_runtime_entry *e=runtime->items+i;const rf_level_owned_group *g=e->source;
        rf_group_mover_membership *m=value.items+i;uint32_t n;
        m->rotation_sign=1;if(e->kind==RF_GROUP_RUNTIME_EMPTY)continue;
        n=g->record.ids_count[1];m->handles=cursor;cursor+=n;if(n)memcpy(refs,g->ids[1],(size_t)n*4);
        status=rf_group_attach_movers(copy,object_count,controller_handles[i],e->initial_flags,global_mode,
            refs,&n,m->handles,&m->count,g->record.ids_count[1],&m->rotation_sign);if(status)goto failed;
    }
    if(object_count)memcpy(objects,copy,(size_t)object_count*sizeof(*objects));
    free(scratch);*result=value;return RF_OK;
 failed:
    free(scratch);rf_group_mover_memberships_close(&value);return status;
}

int rf_level_link_resolve(uint32_t uid,const rf_level_uid_object *objects,uint32_t object_count,
    const rf_level_uid_key *keys,uint32_t key_count,rf_level_link_target *target)
{
    rf_level_link_target result;uint32_t i;
    if(!target || (object_count && !objects) || (key_count && !keys))return RF_RANGE;
    result.value=uid;result.kind=0;result.index=UINT32_MAX;
    if(uid!=UINT32_MAX)for(i=0;i<object_count;++i) {
        if(objects[i].uid==uid && (uid!=(uint32_t)-999 || !(objects[i].flags&2))) {
            result.value=objects[i].handle;result.kind=1;result.index=i;
            *target=result;return RF_OK;
        }
    }
    for(i=0;i<key_count;++i)if(keys[i].uid==uid) {
        result.value=keys[i].handle;result.kind=2;result.index=i;break;
    }
    *target=result;return RF_OK;
}

void rf_level_owned_triggers_close(rf_level_owned_triggers *triggers)
{
    if(triggers) {free(triggers->storage);memset(triggers,0,sizeof(*triggers));}
}
int rf_level_owned_triggers_open(const rf_level *level,uint32_t budget,rf_level_owned_triggers *result)
{
    rf_level_owned_triggers value={0};rf_level_trigger_reader reader;rf_level_trigger record;
    uint64_t bytes;uint32_t i,j;unsigned char *cursor,*end;int status;
    if(!level || !result)return RF_RANGE;
    status=rf_level_triggers_begin(level,&reader);if(status)return status;
    value.count=reader.count;bytes=sizeof(value)+(uint64_t)value.count*sizeof(*value.items);
    if(bytes>budget)return RF_RANGE;
    while((status=rf_level_trigger_next(&reader,&record))==RF_OK) {
        bytes+=(uint64_t)record.link_count*4;if(bytes>budget)return RF_RANGE;
    }
    if(status!=RF_NOT_FOUND)return status;
    value.allocated_bytes=(uint32_t)bytes;
    if(!value.count) {*result=value;return RF_OK;}
    value.storage=calloc(1,(size_t)(bytes-sizeof(value)));if(!value.storage)return RF_RANGE;
    value.items=(rf_level_owned_trigger *)value.storage;
    cursor=(unsigned char *)(value.items+value.count);end=(unsigned char *)value.storage+bytes-sizeof(value);
    status=rf_level_triggers_begin(level,&reader);if(status)goto failed;
    if(reader.count!=value.count) {status=RF_FORMAT;goto failed;}
    for(i=0;i<value.count;++i) {
        rf_level_owned_trigger *item=value.items+i;
        status=rf_level_trigger_next(&reader,&item->record);if(status)goto failed;
        if((uint64_t)item->record.link_count*4>(uint64_t)(end-cursor)) {status=RF_FORMAT;goto failed;}
        item->links=(uint32_t *)cursor;cursor+=item->record.link_count*4;
        for(j=0;j<item->record.link_count;++j)
            if((status=rf_level_trigger_link(level,&item->record,j,item->links+j)))goto failed;
    }
    if(cursor!=end) {status=RF_FORMAT;goto failed;}
    *result=value;return RF_OK;
 failed:
    rf_level_owned_triggers_close(&value);return status;
}

static int event_name_equal(const char *a,const char *b)
{
    unsigned char x,y;
    do {
        x=(unsigned char)*a++;y=(unsigned char)*b++;
        if(x>='A' && x<='Z')x+=32;
        if(y>='A' && y<='Z')y+=32;
        if(x!=y)return 0;
    } while(x);
    return 1;
}
int rf_level_events_begin(const rf_level *level,rf_level_event_reader *reader)
{
    rf_level_event_reader next={0};const rf_level_section *section;int status;
    if(!level || !reader)return RF_RANGE;
    if(level->version!=180)return RF_FORMAT;
    section=rf_level_find(level,0x600);if(!section)return RF_NOT_FOUND;
    next.level=level;next.section=*section;
    status=group_number(&next,&next.count);if(status)return status;
    if((uint64_t)next.count*4>section->size-next.cursor || (!next.count && next.cursor!=section->size))return RF_FORMAT;
    *reader=next;return RF_OK;
}
int rf_level_event_next(rf_level_event_reader *reader,rf_level_event *event)
{
    rf_level_event_reader next;rf_level_event value={0};uint32_t i;int status;
    if(!reader || !event || !reader->level || reader->section.type!=0x600)return RF_RANGE;
    if(reader->index>=reader->count)return reader->index==reader->count && reader->cursor==reader->section.size?RF_NOT_FOUND:RF_FORMAT;
    next=*reader;value.offset=next.cursor;
    if((status=group_number(&next,&value.uid)) || (status=group_string(&next,value.type)) ||
        (status=group_floats(&next,value.position,3)) || (status=group_string(&next,value.name)) ||
        (status=trigger_byte(&next,&value.header_byte)) || (status=group_floats(&next,&value.delay,1)))return status;
    for(i=0;i<2;++i)if((status=trigger_byte(&next,value.flags+i)))return status;
    for(i=0;i<2;++i)if((status=group_number(&next,value.words+i)))return status;
    if((status=group_floats(&next,value.values,2)))return status;
    for(i=0;i<2;++i)if((status=group_string(&next,value.texts[i])))return status;
    if((status=group_number(&next,&value.link_count)))return status;
    value.link_offset=next.cursor;
    if((uint64_t)value.link_count*4>next.section.size-next.cursor)return RF_FORMAT;
    next.cursor+=value.link_count*4;
    value.has_orientation=event_name_equal(value.type,"Teleport") || event_name_equal(value.type,"Teleport_Player") ||
        event_name_equal(value.type,"Play_Vclip") || event_name_equal(value.type,"Alarm");
    if(value.has_orientation && (status=group_floats(&next,value.orientation_disk,9)))return status;
    for(i=0;i<4;++i)if((status=trigger_byte(&next,value.color_bytes+i)))return status;
    value.bytes=next.cursor-value.offset;++next.index;
    if(next.index==next.count && next.cursor!=next.section.size)return RF_FORMAT;
    *reader=next;*event=value;return RF_OK;
}
int rf_level_event_link(const rf_level *level,const rf_level_event *event,uint32_t index,uint32_t *uid)
{
    const rf_level_section *section;unsigned char raw[4];uint64_t offset;int status;
    if(!level || !event || !uid || index>=event->link_count)return RF_RANGE;
    section=rf_level_find(level,0x600);if(!section)return RF_NOT_FOUND;
    offset=(uint64_t)event->link_offset+(uint64_t)index*4;
    if(offset>section->size || section->size-offset<4)return RF_RANGE;
    status=rf_level_read(level,section,(uint32_t)offset,raw,4);if(!status)*uid=le32(raw);return status;
}

void rf_level_owned_events_close(rf_level_owned_events *events)
{
    if(events) {free(events->storage);memset(events,0,sizeof(*events));}
}
int rf_level_owned_events_open(const rf_level *level,uint32_t budget,rf_level_owned_events *result)
{
    rf_level_owned_events value={0};rf_level_event_reader reader;rf_level_event record;
    uint64_t bytes;uint32_t i,j;unsigned char *cursor,*end;int status;
    if(!level || !result)return RF_RANGE;
    status=rf_level_events_begin(level,&reader);if(status)return status;
    value.count=reader.count;bytes=sizeof(value)+(uint64_t)value.count*sizeof(*value.items);
    if(bytes>budget)return RF_RANGE;
    while((status=rf_level_event_next(&reader,&record))==RF_OK) {
        bytes+=(uint64_t)record.link_count*4;if(bytes>budget)return RF_RANGE;
    }
    if(status!=RF_NOT_FOUND)return status;
    value.allocated_bytes=(uint32_t)bytes;
    if(!value.count) {*result=value;return RF_OK;}
    value.storage=calloc(1,(size_t)(bytes-sizeof(value)));if(!value.storage)return RF_RANGE;
    value.items=(rf_level_owned_event *)value.storage;
    cursor=(unsigned char *)(value.items+value.count);end=(unsigned char *)value.storage+bytes-sizeof(value);
    status=rf_level_events_begin(level,&reader);if(status)goto failed;
    if(reader.count!=value.count) {status=RF_FORMAT;goto failed;}
    for(i=0;i<value.count;++i) {
        rf_level_owned_event *item=value.items+i;
        status=rf_level_event_next(&reader,&item->record);if(status)goto failed;
        if((uint64_t)item->record.link_count*4>(uint64_t)(end-cursor)) {status=RF_FORMAT;goto failed;}
        item->links=(uint32_t *)cursor;cursor+=item->record.link_count*4;
        for(j=0;j<item->record.link_count;++j)
            if((status=rf_level_event_link(level,&item->record,j,item->links+j)))goto failed;
    }
    if(cursor!=end) {status=RF_FORMAT;goto failed;}
    *result=value;return RF_OK;
 failed:
    rf_level_owned_events_close(&value);return status;
}
