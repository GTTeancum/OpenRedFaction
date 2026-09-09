#include "rf/geometry.h"
#include <stdlib.h>
#include <string.h>

static uint32_t u32(const unsigned char *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static float f32(const unsigned char *p)
{
    uint32_t bits = u32(p);
    float value;
    _Static_assert(sizeof(float) == 4, "binary32 required");
    memcpy(&value, &bits, 4);
    return value;
}
static int finite_words(const unsigned char *p, uint32_t n)
{
    uint32_t i;
    for (i = 0; i < n; ++i)
        if ((u32(p + i * 4) & 0x7f800000u) == 0x7f800000u) return 0;
    return 1;
}
typedef struct cursor { rf_geometry *g; uint32_t at, budget; int error; } cursor;
static const unsigned char *take(cursor *c, uint64_t n)
{
    const unsigned char *p;
    if (c->error) return NULL;
    if (n > c->g->bytes - c->at) { c->error = RF_FORMAT; return NULL; }
    p = c->g->data + c->at;
    c->at += (uint32_t)n;
    return p;
}
static uint32_t number(cursor *c)
{
    const unsigned char *p = take(c, 4);
    return p ? u32(p) : 0;
}
static void string(cursor *c)
{
    const unsigned char *p = take(c, 2);
    if (p) take(c, p[0] | (uint32_t)p[1] << 8);
}
static uint32_t *index_array(cursor *c, uint32_t count, uint32_t minimum_record_bytes)
{
    uint32_t *array;
    uint64_t bytes = (uint64_t)count * 4;
    if (c->error || !count) return NULL;
    if ((uint64_t)count * minimum_record_bytes > c->g->bytes - c->at) {
        c->error = RF_FORMAT; return NULL;
    }
    if (bytes > c->budget - c->g->allocated_bytes) { c->error = RF_RANGE; return NULL; }
    array = (uint32_t *)malloc((size_t)bytes);
    if (!array) { c->error = RF_RANGE; return NULL; }
    c->g->allocated_bytes += (uint32_t)bytes;
    return array;
}
static int parse(cursor *c)
{
    rf_geometry *g = c->g;
    uint32_t i, j, count;
    const unsigned char *p;
    take(c, 6);
    g->textures = number(c);
    g->texture_offsets = index_array(c, g->textures, 2);
    for (i = 0; i < g->textures && !c->error; ++i) { g->texture_offsets[i] = c->at; string(c); }
    count = number(c); take(c, (uint64_t)count * 12);
    g->rooms = number(c);
    g->room_offsets = index_array(c, g->rooms, 42);
    for (i = 0; i < g->rooms && !c->error; ++i) {
        g->room_offsets[i] = c->at;
        p = take(c, 40);
        if (!p) break;
        if (!finite_words(p + 4, 6) || !finite_words(p + 36, 1)) { c->error = RF_FORMAT; break; }
        string(c);
        if (p[32]) { take(c, 8); string(c); take(c, 37); }
        if (p[33]) take(c, 4);
    }
    count = number(c);
    if ((uint64_t)count * 8 > g->bytes - c->at) c->error = RF_FORMAT;
    for (i = 0; i < count && !c->error; ++i) {
        uint32_t links;
        number(c); links = number(c); take(c, (uint64_t)links * 4);
    }
    count = number(c); take(c, (uint64_t)count * 32);
    g->vertices = number(c); g->vertices_offset = c->at;
    p = take(c, (uint64_t)g->vertices * 12);
    if (p && !finite_words(p, g->vertices * 3)) c->error = RF_FORMAT;
    g->faces = number(c);
    g->face_offsets = index_array(c, g->faces, 92);
    for (i = 0; i < g->faces && !c->error; ++i) {
        uint32_t texture, mapping, room, corners, stride;
        g->face_offsets[i] = c->at;
        p = take(c, 56);
        if (!p) break;
        texture = u32(p + 16); mapping = u32(p + 20); room = u32(p + 48); corners = u32(p + 52);
        stride = mapping == UINT32_MAX ? 12 : 20;
        if (!finite_words(p, 4) || (texture != UINT32_MAX && texture >= g->textures) ||
            room >= g->rooms || corners < 3 || (uint64_t)g->corners + corners > UINT32_MAX) {
            c->error = RF_FORMAT; break;
        }
        p = take(c, (uint64_t)corners * stride);
        if (!p) break;
        for (j = 0; j < corners; ++j)
            if (u32(p + j * stride) >= g->vertices || !finite_words(p + j * stride + 4, stride / 4 - 1)) {
                c->error = RF_FORMAT; break;
            }
        g->corners += corners;
    }
    g->mappings = number(c); g->mapping_offset = c->at;
    take(c, (uint64_t)g->mappings * 96);
    /* Unknown final word and any following records stay resident, uninterpreted. */
    g->tail_offset = c->at;
    take(c, 4);
    if (!c->error) for (i = 0; i < g->faces; ++i) {
        uint32_t mapping = u32(g->data + g->face_offsets[i] + 20);
        if (mapping != UINT32_MAX && mapping >= g->mappings) return RF_FORMAT;
    }
    return c->error;
}

int rf_geometry_open(rf_geometry *g, const rf_level *level, uint32_t budget)
{
    const rf_level_section *section;
    cursor c;
    int result;
    if (!g) return RF_RANGE;
    memset(g, 0, sizeof(*g));
    if (!level || level->version != 180) return RF_FORMAT;
    section = rf_level_find(level, 0x100);
    if (!section) return RF_NOT_FOUND;
    if (!section->size || section->size > budget) return RF_RANGE;
    g->data = (unsigned char *)malloc(section->size);
    if (!g->data) return RF_RANGE;
    g->allocated_bytes = g->bytes = section->size;
    result = rf_level_read(level, section, 0, g->data, g->bytes);
    if (result == RF_OK) { c.g = g; c.at = 0; c.budget = budget; c.error = RF_OK; result = parse(&c); }
    if (result != RF_OK) rf_geometry_close(g);
    return result;
}
void rf_geometry_close(rf_geometry *g)
{
    if (!g) return;
    free(g->texture_offsets); free(g->room_offsets); free(g->face_offsets); free(g->data);
    memset(g, 0, sizeof(*g));
}
int rf_geometry_vertex(const rf_geometry *g, uint32_t index, float position[3])
{
    uint32_t i;
    if (!g || !g->data || !position || index >= g->vertices) return RF_RANGE;
    for (i = 0; i < 3; ++i) position[i] = f32(g->data + g->vertices_offset + index * 12 + i * 4);
    return RF_OK;
}
int rf_geometry_lightmap(const rf_geometry *g, uint32_t mapping, uint32_t image_count, uint32_t *image)
{
    uint32_t index;
    if (!g || !g->data || !image || mapping >= g->mappings) return RF_RANGE;
    index = u32(g->data + g->mapping_offset + mapping*96);
    if (index >= image_count) return RF_FORMAT;
    *image = index;
    return RF_OK;
}
int rf_geometry_texture_name(const rf_geometry *g, uint32_t index, char *name, uint32_t capacity)
{
    const unsigned char *p;
    uint32_t length;
    if (!g || !g->data || !name || !capacity || index >= g->textures) return RF_RANGE;
    name[0] = 0;
    p = g->data + g->texture_offsets[index];
    length = p[0] | (uint32_t)p[1] << 8;
    if (length >= capacity) return RF_RANGE;
    if (memchr(p + 2, 0, length)) return RF_FORMAT;
    memcpy(name, p + 2, length); name[length] = 0;
    return RF_OK;
}
int rf_geometry_get_face(const rf_geometry *g, uint32_t index, rf_geometry_face *face)
{
    const unsigned char *p;
    uint32_t i;
    if (!g || !g->data || !face || index >= g->faces) return RF_RANGE;
    p = g->data + g->face_offsets[index];
    for (i = 0; i < 4; ++i) face->plane[i] = f32(p + i * 4);
    face->texture = u32(p + 16); face->lightmap_mapping = u32(p + 20);
    face->portal = u32(p + 36); face->flags = u32(p + 40); face->room = u32(p + 48); face->corners = u32(p + 52);
    return RF_OK;
}
int rf_geometry_get_corner(const rf_geometry *g, uint32_t index, uint32_t corner, rf_geometry_corner *result)
{
    const unsigned char *p;
    uint32_t stride;
    if (!g || !g->data || !result || index >= g->faces) return RF_RANGE;
    p = g->data + g->face_offsets[index];
    if (corner >= u32(p + 52)) return RF_RANGE;
    stride = u32(p + 20) == UINT32_MAX ? 12 : 20;
    p += 56 + corner * stride;
    result->vertex = u32(p); result->uv[0] = f32(p + 4); result->uv[1] = f32(p + 8);
    result->lightmap_uv[0] = stride == 20 ? f32(p + 12) : 0;
    result->lightmap_uv[1] = stride == 20 ? f32(p + 16) : 0;
    return RF_OK;
}

int rf_geometry_collision_face(const rf_geometry *geometry,uint32_t index,
    const rf_collision_face_filter *filter,float (*scratch)[3],uint32_t capacity,
    rf_collision_face *face)
{
    rf_geometry_face source;rf_geometry_corner corner;rf_collision_face value;
    uint32_t i,j,accepted;int status;
    if(!filter || !scratch || !face)return RF_RANGE;
    status=rf_collision_face_accept(filter,&accepted);if(status)return status;
    status=rf_geometry_get_face(geometry,index,&source);if(status)return status;
    if(!source.corners || source.corners>capacity || source.corners>65536)return RF_RANGE;
    for(i=0;i<source.corners;i++) {
        status=rf_geometry_get_corner(geometry,index,i,&corner);if(status)return status;
        status=rf_geometry_vertex(geometry,corner.vertex,scratch[i]);if(status)return status;
        for(j=0;j<3;j++) {
            if(!i || scratch[i][j]<value.minimum[j])value.minimum[j]=scratch[i][j];
            if(!i || scratch[i][j]>value.maximum[j])value.maximum[j]=scratch[i][j];
        }
    }
    /* 4dfe20 finalizer at 4e002b: binary32 0x38d1b717 on both sides. */
    for(j=0;j<3;j++) {value.minimum[j]-=0.0001f;value.maximum[j]+=0.0001f;}
    memcpy(value.plane,source.plane,sizeof(value.plane));value.vertices=scratch;
    value.count=source.corners;value.filter=*filter;*face=value;return RF_OK;
}

int rf_geometry_initial_collision_filter(const rf_geometry *geometry,uint32_t index,
    uint32_t query_flags,rf_collision_face_filter *filter)
{
    rf_geometry_face face;rf_collision_face_filter value;const unsigned char *room;
    uint32_t portal;int status;
    if(!filter)return RF_RANGE;
    status=rf_geometry_get_face(geometry,index,&face);if(status)return status;
    if(face.room>=geometry->rooms)return RF_FORMAT;
    room=geometry->data+geometry->room_offsets[face.room];portal=face.portal&0xffffu;
    value.query_flags=query_flags;value.face_flags=face.flags;
    value.property_34=portal>=0x8000u?(int32_t)portal-65536:(int32_t)portal;
    value.owner_present=1;value.owner_kind=room[34];value.owner_state=f32(room+36)>0?0:1;
    *filter=value;return RF_OK;
}

void rf_geometry_collision_room_close(rf_geometry_collision_room *room)
{
    if(room) {rf_collision_tree_close(&room->tree);free(room->vertices);memset(room,0,sizeof(*room));}
}
int rf_geometry_collision_room_open(const rf_geometry *geometry,uint32_t room,
    uint32_t budget,rf_geometry_collision_room *result)
{
    rf_geometry_collision_room value={0};rf_collision_face *faces=NULL;
    uint32_t *indices=NULL,count=0,i,at=0,vertex=0;uint64_t corners=0,base,temporary;
    rf_geometry_face face;int status;
    if(!geometry || !geometry->data || !result || room>=geometry->rooms)return RF_RANGE;
    for(i=0;i<geometry->faces;i++) {
        status=rf_geometry_get_face(geometry,i,&face);if(status)return status;
        if(face.room==room) {count++;corners+=face.corners;}
    }
    base=sizeof(value)+corners*12;temporary=(uint64_t)count*(sizeof(*faces)+sizeof(*indices));
    if(base+temporary>budget)return RF_RANGE;
    value.room=room;
    if(count) {
        value.vertices=(float(*)[3])malloc((size_t)(corners*12));
        faces=(rf_collision_face*)malloc((size_t)temporary);
        if(!value.vertices || !faces) {status=RF_IO;goto done;}
        indices=(uint32_t*)(faces+count);
    }
    for(i=0;i<geometry->faces;i++) {
        rf_collision_face_filter filter;
        status=rf_geometry_get_face(geometry,i,&face);if(status)goto done;
        if(face.room!=room)continue;
        status=rf_geometry_initial_collision_filter(geometry,i,0,&filter);if(status)goto done;
        status=rf_geometry_collision_face(geometry,i,&filter,value.vertices+vertex,face.corners,faces+at);if(status)goto done;
        indices[at++]=i;vertex+=face.corners;
    }
    status=rf_collision_tree_open(faces,count,(uint32_t)(budget-base-temporary+sizeof(value.tree)),&value.tree);
    if(status)goto done;
    for(i=0;i<count;i++)value.tree.source_indices[i]=indices[value.tree.source_indices[i]];
    value.allocated_bytes=(uint32_t)(base+value.tree.allocated_bytes-sizeof(value.tree));
    value.peak_bytes=(uint32_t)(base+temporary+value.tree.peak_bytes-sizeof(value.tree));
 done:
    free(faces);
    if(status) {rf_geometry_collision_room_close(&value);return status;}
    *result=value;return RF_OK;
}
