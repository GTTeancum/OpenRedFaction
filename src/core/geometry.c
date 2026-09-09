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
typedef struct cursor { rf_geometry *g; uint32_t at, budget; int error, allow_unowned; } cursor;
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
    count = number(c);g->room_link_records=count;g->room_links_offset=c->at;
    if ((uint64_t)count * 8 > g->bytes - c->at) c->error = RF_FORMAT;
    for (i = 0; i < count && !c->error; ++i) {
        uint32_t parent,links;
        parent=number(c);links=number(c);p=take(c,(uint64_t)links*4);
        if(parent>=g->rooms)c->error=RF_FORMAT;
        if(p)for(j=0;j<links;j++)if(u32(p+j*4)>=g->rooms) {c->error=RF_FORMAT;break;}
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
            (room >= g->rooms && !(c->allow_unowned && room == UINT32_MAX)) || corners < 3 || (uint64_t)g->corners + corners > UINT32_MAX) {
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
    if (result == RF_OK) { c.g = g; c.at = 0; c.budget = budget; c.error = RF_OK; c.allow_unowned = 0; result = parse(&c); }
    if (result != RF_OK) rf_geometry_close(g);
    return result;
}
void rf_geometry_close(rf_geometry *g)
{
    if (!g) return;
    free(g->texture_offsets); free(g->room_offsets); free(g->face_offsets); free(g->data);
    memset(g, 0, sizeof(*g));
}
void rf_geometry_movers_close(rf_geometry_movers *m)
{
    uint32_t i;
    if(!m)return;
    for(i=0;i<m->count;i++) {
        m->items[i].geometry.data=NULL;
        rf_geometry_close(&m->items[i].geometry);
    }
    free(m->items);free(m->data);memset(m,0,sizeof(*m));
}
int rf_geometry_movers_open(const rf_level *level,uint32_t budget,rf_geometry_movers *result)
{
    rf_geometry_movers m={0};const rf_level_section *section;
    uint32_t count,i,j,at=4;uint64_t bytes;int status;
    if(!result)return RF_RANGE;
    if(!level || level->version!=180)return RF_FORMAT;
    section=rf_level_find(level,0x2000);if(!section)return RF_NOT_FOUND;
    if(section->size<4)return RF_FORMAT;
    bytes=(uint64_t)sizeof(m)+section->size;
    if(bytes>budget)return RF_RANGE;
    m.data=(unsigned char *)malloc(section->size);if(!m.data)return RF_RANGE;
    m.allocated_bytes=(uint32_t)bytes;
    status=rf_level_read(level,section,0,m.data,section->size);if(status)goto fail;
    count=u32(m.data);
    /* Even an empty geometry needs a header, count fields and trailer. */
    if((uint64_t)count*64>section->size-4) {status=RF_FORMAT;goto fail;}
    bytes=(uint64_t)count*sizeof(*m.items);
    if(bytes>budget-m.allocated_bytes) {status=RF_RANGE;goto fail;}
    if(count) {
        m.items=(rf_geometry_mover *)calloc(count,sizeof(*m.items));
        if(!m.items) {status=RF_RANGE;goto fail;}
    }
    m.allocated_bytes+=(uint32_t)bytes;m.count=count;
    for(i=0;i<count;i++) {
        rf_geometry_mover *item=m.items+i;rf_geometry *g=&item->geometry;
        cursor c;const unsigned char *p;
        item->offset=at;
        if(section->size-at<52) {status=RF_FORMAT;goto fail;}
        p=m.data+at;if(!finite_words(p+4,12)) {status=RF_FORMAT;goto fail;}
        memcpy(&item->uid,p,4);
        for(j=0;j<3;j++)item->position[j]=f32(p+4+j*4);
        for(j=0;j<9;j++)item->orientation[j/3][j%3]=f32(p+16+((j+3)%9)*4);
        at+=52;item->geometry_offset=at;g->data=m.data+at;g->bytes=section->size-at;
        c.g=g;c.at=0;c.budget=budget-m.allocated_bytes;c.error=RF_OK;c.allow_unowned=1;
        status=parse(&c);if(status)goto fail;
        /* 4ed520's pre-v181 legacy array follows its count at tail_offset. */
        take(&c,(uint64_t)u32(g->data+g->tail_offset)*12);
        if(c.error) {status=c.error;goto fail;}
        g->bytes=c.at;at+=c.at;m.allocated_bytes+=g->allocated_bytes;
        if(section->size-at<12) {status=RF_FORMAT;goto fail;}
        for(j=0;j<3;j++)item->trailer[j]=u32(m.data+at+j*4);
        at+=12;item->bytes=at-item->offset;
    }
    if(at!=section->size) {status=RF_FORMAT;goto fail;}
    *result=m;return RF_OK;
fail:
    rf_geometry_movers_close(&m);return status;
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
    if(face.room!=UINT32_MAX && face.room>=geometry->rooms)return RF_FORMAT;
    portal=face.portal&0xffffu;
    value.query_flags=query_flags;value.face_flags=face.flags;
    value.property_34=portal>=0x8000u?(int32_t)portal-65536:(int32_t)portal;
    value.owner_present=value.owner_kind=value.owner_state=0;
    if(face.room!=UINT32_MAX) {
        room=geometry->data+geometry->room_offsets[face.room];
        value.owner_present=1;value.owner_kind=room[34];value.owner_state=f32(room+36)>0?0:1;
    }
    *filter=value;return RF_OK;
}
void rf_geometry_collision_flat_close(rf_geometry_collision_flat *flat)
{
    if(flat) {free(flat->faces);free(flat->vertices);memset(flat,0,sizeof(*flat));}
}
int rf_geometry_collision_flat_open(const rf_geometry *g,uint32_t budget,
    rf_geometry_collision_flat *result)
{
    rf_geometry_collision_flat value={0};uint64_t corners=0,bytes;uint32_t i,at=0;
    rf_geometry_face face;int status;
    if(!g || !g->data || !result)return RF_RANGE;
    if(g->rooms)return RF_FORMAT;
    for(i=0;i<g->faces;i++) {
        status=rf_geometry_get_face(g,i,&face);if(status)return status;
        if(face.room!=UINT32_MAX)return RF_FORMAT;
        corners+=face.corners;
    }
    bytes=sizeof(value)+(uint64_t)g->faces*sizeof(*value.faces)+corners*12;
    if(bytes>budget)return RF_RANGE;
    if(g->faces) {
        value.faces=(rf_collision_face *)malloc((size_t)g->faces*sizeof(*value.faces));
        value.vertices=(float(*)[3])malloc((size_t)(corners*12));
        if(!value.faces || !value.vertices) {status=RF_RANGE;goto fail;}
    }
    for(i=0;i<g->faces;i++) {
        rf_collision_face_filter filter;
        status=rf_geometry_get_face(g,i,&face);if(status)goto fail;
        status=rf_geometry_initial_collision_filter(g,i,0,&filter);if(status)goto fail;
        status=rf_geometry_collision_face(g,i,&filter,value.vertices+at,face.corners,value.faces+i);
        if(status)goto fail;
        at+=face.corners;
    }
    value.count=g->faces;value.allocated_bytes=(uint32_t)bytes;*result=value;return RF_OK;
fail:
    rf_geometry_collision_flat_close(&value);return status;
}
int rf_geometry_room_children(const rf_geometry *geometry,uint32_t room,
    uint32_t *indices,uint32_t capacity,uint32_t *count)
{
    uint32_t i,j,total=0,at;const unsigned char *p;
    if(!geometry || !geometry->data || !count || room>=geometry->rooms)return RF_RANGE;
    p=geometry->data+geometry->room_links_offset;
    for(i=0;i<geometry->room_link_records;i++) {
        uint32_t parent=u32(p),n=u32(p+4);p+=8;
        if(parent==room) {if(n>UINT32_MAX-total)return RF_RANGE;total+=n;}
        p+=(size_t)n*4;
    }
    if(total>capacity || (total && !indices))return RF_RANGE;
    p=geometry->data+geometry->room_links_offset;at=0;
    for(i=0;i<geometry->room_link_records;i++) {
        uint32_t parent=u32(p),n=u32(p+4);p+=8;
        if(parent==room)for(j=0;j<n;j++)indices[at++]=u32(p+j*4);
        p+=(size_t)n*4;
    }
    *count=total;return RF_OK;
}
void rf_geometry_collision_movers_close(rf_geometry_collision_movers *m)
{
    uint32_t i;if(!m)return;
    for(i=0;i<m->count;i++)rf_geometry_collision_flat_close(m->owned+i);
    free(m->storage);memset(m,0,sizeof(*m));
}
int rf_geometry_collision_movers_open(const rf_geometry_movers *source,
    const uint32_t *object_ids,uint32_t budget,rf_geometry_collision_movers *result)
{
    rf_geometry_collision_movers value={0};uint64_t base,retained,peak;
    float (*vertices)[3]=NULL;uint32_t i,j;int status;
    if(!source || !result || (source->count && (!source->items || !object_ids)))return RF_RANGE;
    base=(uint64_t)source->count*(sizeof(*value.owned)+sizeof(*value.views)+sizeof(*value.uids)+sizeof(*value.poses));
    retained=peak=base+sizeof(value);if(peak>budget)return RF_RANGE;
    if(source->count) {
        value.storage=calloc(1,(size_t)base);if(!value.storage)return RF_RANGE;
        value.owned=(rf_geometry_collision_flat *)value.storage;
        value.views=(rf_collision_solid_view *)(value.owned+source->count);
        value.uids=(int32_t *)(value.views+source->count);
        value.poses=(rf_group_attached_pose *)(value.uids+source->count);
    }
    value.count=source->count;
    for(i=0;i<source->count;i++) {
        const rf_geometry_mover *m=source->items+i;const rf_geometry *g=&m->geometry;
        rf_collision_solid_view *view=value.views+i;rf_collision_bounds bounds;
        uint64_t scratch=(uint64_t)g->vertices*12;
        if(!g->data || g->rooms || !g->vertices) {status=RF_FORMAT;goto fail;}
        if(retained+scratch>budget) {status=RF_RANGE;goto fail;}
        if(retained+scratch>peak)peak=retained+scratch;
        vertices=(float(*)[3])malloc((size_t)scratch);if(!vertices) {status=RF_RANGE;goto fail;}
        for(j=0;j<g->vertices;j++) {status=rf_geometry_vertex(g,j,vertices[j]);if(status)goto fail;}
        status=rf_collision_vertex_bounds(vertices,g->vertices,&bounds);
        free(vertices);vertices=NULL;if(status)goto fail;
        status=rf_geometry_collision_flat_open(g,(uint32_t)(budget-retained+sizeof(*value.owned)),value.owned+i);
        if(status)goto fail;
        retained+=value.owned[i].allocated_bytes-sizeof(*value.owned);if(retained>peak)peak=retained;
        value.uids[i]=m->uid;view->object_id=object_ids[i];
        memcpy(view->input_origin,m->position,12);memcpy(view->output_origin,m->position,12);
        memcpy(view->input_matrix,m->orientation,36);memcpy(view->output_matrix,m->orientation,36);
        for(j=0;j<3;j++) {view->minimum[j]=m->position[j]-bounds.origin_radius;view->maximum[j]=m->position[j]+bounds.origin_radius;}
        if(!finite_words((const unsigned char *)view->minimum,30)) {status=RF_FORMAT;goto fail;}
        view->flat_faces=value.owned[i].faces;view->flat_count=value.owned[i].count;
        /* 486ee6 base pose + 49f051 physics pose; mover factory passes flags 0. */
        value.poses[i].flags=0x6000000;value.poses[i].radius=bounds.origin_radius;
        memcpy(value.poses[i].base_position,m->position,12);memcpy(value.poses[i].base_matrix,m->orientation,36);
        memcpy(value.poses[i].position,m->position,12);memcpy(value.poses[i].public_position,m->position,12);memcpy(value.poses[i].pending,m->position,12);
        memcpy(value.poses[i].input_matrix,m->orientation,36);memcpy(value.poses[i].output_matrix,m->orientation,36);memcpy(value.poses[i].pending_matrix,m->orientation,36);
        memcpy(value.poses[i].minimum,view->minimum,12);memcpy(value.poses[i].maximum,view->maximum,12);
    }
    value.allocated_bytes=(uint32_t)retained;value.peak_bytes=(uint32_t)peak;*result=value;return RF_OK;
fail:
    free(vertices);rf_geometry_collision_movers_close(&value);return status;
}
int rf_geometry_primary_rooms(const rf_geometry *geometry,uint32_t *indices,
    uint32_t capacity,uint32_t *count)
{
    uint32_t i,total=0,at=0;
    if(!geometry || !geometry->data || !count)return RF_RANGE;
    for(i=0;i<geometry->rooms;i++)if(!geometry->data[geometry->room_offsets[i]+34])total++;
    if(total>capacity || (total && !indices))return RF_RANGE;
    for(i=0;i<geometry->rooms;i++)if(!geometry->data[geometry->room_offsets[i]+34])indices[at++]=i;
    *count=total;return RF_OK;
}
int rf_geometry_collision_movers_sync(rf_geometry_collision_movers *movers)
{
    uint32_t i;
    if(!movers || (movers->count && (!movers->poses || !movers->views)))return RF_RANGE;
    for(i=0;i<movers->count;i++) {
        const rf_group_attached_pose *pose=movers->poses+i;rf_collision_solid_view *view=movers->views+i;
        memcpy(view->minimum,pose->minimum,12);memcpy(view->maximum,pose->maximum,12);
        memcpy(view->input_origin,pose->public_position,12);memcpy(view->output_origin,pose->position,12);
        memcpy(view->input_matrix,pose->input_matrix,36);memcpy(view->output_matrix,pose->output_matrix,36);
    }
    return RF_OK;
}
int rf_geometry_collision_movers_propagate(rf_geometry_collision_movers *movers,
    const rf_group_controller_view *controllers,uint32_t count,float dt,uint32_t force)
{
    uint32_t i;int status;rf_group_attached_pose test;
    if(!movers || (movers->count && (!movers->poses || !movers->views)))return RF_RANGE;
    for(i=0;i<movers->count;i++) {
        test=movers->poses[i];status=rf_group_translation_bind_pose(&test,movers->views[i].object_id,controllers,count,dt,force);if(status)return status;
    }
    for(i=0;i<movers->count;i++) {
        rf_group_attached_pose *pose=movers->poses+i;rf_collision_solid_view *view=movers->views+i;
        status=rf_group_translation_bind_pose(pose,view->object_id,controllers,count,dt,force);if(status)return status;
    }
    return rf_geometry_collision_movers_sync(movers);
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
    for(i=0;i<3;i++) {
        value.minimum[i]=f32(geometry->data+geometry->room_offsets[room]+4+i*4);
        value.maximum[i]=f32(geometry->data+geometry->room_offsets[room]+16+i*4);
    }
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
        {uint32_t j;for(j=0;j<3;j++) {
            if(faces[at].minimum[j]<value.minimum[j])value.minimum[j]=faces[at].minimum[j];
            if(faces[at].maximum[j]>value.maximum[j])value.maximum[j]=faces[at].maximum[j];
        }}
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
void rf_geometry_collision_world_close(rf_geometry_collision_world *world)
{
    uint32_t i;if(!world)return;
    for(i=0;i<world->room_count;i++)rf_geometry_collision_room_close(world->rooms+i);
    free(world->storage);memset(world,0,sizeof(*world));
}
int rf_geometry_collision_world_open(const rf_geometry *geometry,uint32_t budget,
    rf_geometry_collision_world *world)
{
    rf_geometry_collision_world value={0};uint64_t links=0,bytes,retained,peak;
    const unsigned char *p;uint32_t i,at=0;int status;
    if(!geometry || !geometry->data || !world)return RF_RANGE;
    p=geometry->data+geometry->room_links_offset;
    for(i=0;i<geometry->room_link_records;i++) {uint32_t n=u32(p+4);links+=n;p+=8+(size_t)n*4;}
    bytes=sizeof(value)+(uint64_t)geometry->rooms*(sizeof(*value.rooms)+sizeof(*value.views)+4)+links*4;
    if(bytes>budget || links>UINT32_MAX)return RF_RANGE;
    if(bytes>sizeof(value)) {
        value.storage=malloc((size_t)(bytes-sizeof(value)));if(!value.storage)return RF_IO;
        memset(value.storage,0,(size_t)(bytes-sizeof(value)));
        value.rooms=(rf_geometry_collision_room*)value.storage;value.views=(rf_collision_room_view*)(value.rooms+geometry->rooms);
        value.primary=(uint32_t*)(value.views+geometry->rooms);value.children=value.primary+geometry->rooms;
    }
    value.room_count=geometry->rooms;value.child_count=(uint32_t)links;retained=peak=bytes;
    status=rf_geometry_primary_rooms(geometry,value.primary,geometry->rooms,&value.primary_count);if(status)goto fail;
    for(i=0;i<geometry->rooms;i++) {
        rf_geometry_collision_room *room=value.rooms+i;rf_collision_room_view *view=value.views+i;
        status=rf_geometry_collision_room_open(geometry,i,(uint32_t)(budget-retained+sizeof(*room)),room);if(status)goto fail;
        bytes=retained+room->peak_bytes-sizeof(*room);if(bytes>peak)peak=bytes;
        retained+=room->allocated_bytes-sizeof(*room);
        memcpy(view->minimum,room->minimum,24);view->tree=&room->tree;view->skip=0;view->first_child=at;
        status=rf_geometry_room_children(geometry,i,value.children+at,value.child_count-at,&view->child_count);if(status)goto fail;
        at+=view->child_count;
    }
    value.allocated_bytes=(uint32_t)retained;value.peak_bytes=(uint32_t)peak;*world=value;return RF_OK;
 fail:
    rf_geometry_collision_world_close(&value);return status;
}
int rf_geometry_collision_world_ray(const rf_geometry_collision_world *world,
    uint32_t flags,const float start[3],const float delta[3],float limit,
    rf_geometry_world_hit *result,uint32_t *matched)
{
    rf_collision_room_hit hit;uint32_t found;int status;
    if(!world || !result || !matched)return RF_RANGE;
    status=rf_collision_thin_rooms(world->views,world->room_count,world->primary,world->primary_count,
        world->children,world->child_count,flags,start,delta,limit,&hit,&found);if(status)return status;
    if(found) {
        rf_geometry_world_hit value;value.hit=hit.tree.hit;value.room=hit.room;value.hits=hit.tree.hits;
        value.face=world->rooms[hit.room].tree.source_indices[hit.tree.face_index];*result=value;
    }
    *matched=found;return RF_OK;
}

int rf_geometry_collision_world_sweep(const rf_geometry_collision_world *world,
    uint32_t flags,const float start[3],const float delta[3],float radius,float limit,
    rf_geometry_world_sweep_hit *result,uint32_t *matched)
{
    rf_collision_sweep_room_hit hit;uint32_t found;int status;
    if(!world || !result || !matched)return RF_RANGE;
    status=rf_collision_sweep_rooms(world->views,world->room_count,world->primary,world->primary_count,
        world->children,world->child_count,flags,start,delta,radius,limit,&hit,&found);if(status)return status;
    if(found) {
        rf_geometry_world_sweep_hit value;value.hit=hit.tree.hit;value.room=hit.room;value.hits=hit.tree.hits;value.edge=hit.tree.edge;
        value.face=world->rooms[hit.room].tree.source_indices[hit.tree.face_index];*result=value;
    }
    *matched=found;return RF_OK;
}

int rf_geometry_collision_ray(const rf_geometry_collision_world *world,
    const rf_geometry_collision_movers *movers,const float start[3],const float end[3],
    uint32_t flags,rf_collision_solid_hit *result,uint32_t *matched)
{
    rf_collision_solid_view stationary={0};rf_collision_solid_hit value;uint32_t hit;int status;
    if(!world || !movers || !matched)return RF_RANGE;
    stationary.rooms=world->views;stationary.room_count=world->room_count;
    stationary.primary=world->primary;stationary.primary_count=world->primary_count;
    stationary.children=world->children;stationary.child_count=world->child_count;
    status=rf_collision_ray_solids(movers->views,movers->count,&stationary,start,end,flags,result?&value:NULL,&hit);
    if(status)return status;
    if(hit && result) {
        if(value.solid_index==UINT32_MAX) {
            const rf_collision_tree *tree;
            if(value.room>=world->room_count)return RF_FORMAT;
            tree=&world->rooms[value.room].tree;
            if(value.face_index>=tree->face_count)return RF_FORMAT;
            value.face_index=tree->source_indices[value.face_index];
        }
        *result=value;
    }
    *matched=hit;return RF_OK;
}
