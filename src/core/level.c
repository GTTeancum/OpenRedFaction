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
