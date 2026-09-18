#include "rf/geomod_authored_post.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define MAX_BRUSHES 4096
#define MAX_BRUSH_FACES 32768
typedef struct brush_record {
    uint32_t uid, index, flags, texture_offset, textures, vertex_offset, vertices, face_offset, faces,
        corners;
    float position[3], basis[9], minimum[3], maximum[3];
} brush_record;
typedef struct face_owner {
    uint32_t id, brush;
} face_owner;
typedef struct cursor {
    const unsigned char *data;
    uint32_t size, at;
} cursor;
struct rf_geomod_authored_post {
    rf_geomod_authored_post_view view;
    float (*cavity_obstacles)[2][3];uint32_t cavity_obstacle_count;
};
static uint32_t u32(const unsigned char *p) {
    return p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}
static float f32(const unsigned char *p) {
    uint32_t n = u32(p);
    float f;
    memcpy(&f, &n, 4);
    return f;
}
static const unsigned char *take(cursor *c, uint32_t n) {
    const unsigned char *p;
    if (n > c->size - c->at)
        return NULL;
    p = c->data + c->at;
    c->at += n;
    return p;
}
static int number(cursor *c, uint32_t *n) {
    const unsigned char *p = take(c, 4);
    if (!p)
        return RF_FORMAT;
    *n = u32(p);
    return RF_OK;
}
/* RED4d0aa0 reorders file forward/right/up to runtime right/up/forward.
 * RED4b57b0 stores the rotated vector before adding brush position. */
static int transform(const brush_record *b, const unsigned char *input, float out[3]) {
    uint32_t i, j;
    float v[3];
    for (i = 0; i < 3; i++) {
        v[i] = f32(input + i * 4);
        if (!isfinite(v[i]))
            return RF_FORMAT;
    }
    for (i = 0; i < 3; i++) {
        double sum = 0;
        volatile float rounded;
        for (j = 0; j < 3; j++)
            sum += (double)v[j] * b->basis[j * 3 + i];
        rounded = (float)sum;
        out[i] = rounded + b->position[i];
        if (!isfinite(out[i]))
            return RF_FORMAT;
    }
    return RF_OK;
}
/* RED44d690, v180: six prefix bytes, mesh records, then five tail words.
 * Flags at tail+8 use44d870; the final word is not the CSG operation. */
static int parse(cursor *c, brush_record *b, uint32_t index) {
    const unsigned char *p;
    uint32_t i, j, n;
    int s;
    memset(b, 0, sizeof(*b));
    b->index = index;
    p = take(c, 52);
    if (!p)
        return RF_FORMAT;
    b->uid = u32(p);
    for (i = 0; i < 3; i++)
        b->position[i] = f32(p + 4 + i * 4);
    for (i = 0; i < 9; i++)
        b->basis[i] = f32(p + 16 + ((i + 3) % 9) * 4);
    for (i = 0; i < 3; i++)
        if (!isfinite(b->position[i]))
            return RF_FORMAT;
    for (i = 0; i < 9; i++)
        if (!isfinite(b->basis[i]))
            return RF_FORMAT;
    if (!take(c, 6))
        return RF_FORMAT;
    s = number(c, &b->textures);
    if (s)
        return s;
    b->texture_offset = c->at;
    for (i = 0; i < b->textures; i++) {
        p = take(c, 2);
        if (!p)
            return RF_FORMAT;
        n = p[0] | (uint32_t)p[1] << 8;
        if (!take(c, n))
            return RF_FORMAT;
    }
    if (!take(c, 16))
        return RF_FORMAT;
    s = number(c, &b->vertices);
    if (s)
        return s;
    b->vertex_offset = c->at;
    if (b->vertices > (c->size - c->at) / 12)
        return RF_FORMAT;
    for (i = 0; i < b->vertices; i++) {
        float point[3];
        p = take(c, 12);
        s = transform(b, p, point);
        if (s)
            return s;
        for (j = 0; j < 3; j++) {
            if (!i || point[j] < b->minimum[j])
                b->minimum[j] = point[j];
            if (!i || point[j] > b->maximum[j])
                b->maximum[j] = point[j];
        }
    }
    s = number(c, &b->faces);
    if (s)
        return s;
    b->face_offset = c->at;
    for (i = 0; i < b->faces; i++) {
        uint32_t stride;
        p = take(c, 56);
        if (!p)
            return RF_FORMAT;
        n = u32(p + 52);
        stride = u32(p + 20) == UINT32_MAX ? 12 : 20;
        if (n > (c->size - c->at) / stride || n > UINT32_MAX - b->corners)
            return RF_FORMAT;
        b->corners += n;
        for (j = 0; j < n; j++) {
            p = take(c, stride);
            if (u32(p) >= b->vertices || !isfinite(f32(p + 4)) || !isfinite(f32(p + 8)))
                return RF_FORMAT;
        }
    }
    p = take(c, 20);
    if (!p)
        return RF_FORMAT;
    b->flags = u32(p + 8);
    return RF_OK;
}
static int owner_compare(const void *a, const void *b) {
    uint32_t x = ((const face_owner *)a)->id, y = ((const face_owner *)b)->id;
    return x < y ? -1 : x > y;
}
static const face_owner *find_owner(const face_owner *a, uint32_t n, uint32_t id) {
    face_owner key = {id, 0};
    return bsearch(&key, a, n, sizeof(*a), owner_compare);
}
static int overlap(const brush_record *a, const brush_record *b) {
    uint32_t i;
    for (i = 0; i < 3; i++)
        if (a->minimum[i] > b->maximum[i] + 1e-5f || a->maximum[i] < b->minimum[i] - 1e-5f)
            return 0;
    return 1;
}
static int same(const float a[3], const float b[3]) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; }
static int geometry_source(const rf_geometry *g, uint32_t face, uint32_t *id) {
    uint32_t at;
    if (face >= g->faces || !g->face_offsets || g->bytes < 56)
        return RF_FORMAT;
    at = g->face_offsets[face];
    if (at > g->bytes - 56)
        return RF_FORMAT;
    *id = u32(g->data + at + 24);
    return RF_OK;
}
static int name_equal(const char *a, const char *b) {
    uint32_t i;
    for (i = 0;; i++) {
        unsigned char x = (unsigned char)a[i], y = (unsigned char)b[i];
        if (x >= 'A' && x <= 'Z')
            x += 32;
        if (y >= 'A' && y <= 'Z')
            y += 32;
        if (x != y)
            return 0;
        if (!x)
            return 1;
    }
}
static int texture(const unsigned char *data, const brush_record *b, uint32_t local, const rf_geometry *g,
                   uint32_t *index) {
    uint32_t at = b->texture_offset, i, n;
    char expected[256], actual[256];
    if (local >= b->textures)
        return RF_FORMAT;
    for (i = 0; i <= local; i++) {
        n = data[at] | (uint32_t)data[at + 1] << 8;
        at += 2;
        if (i == local) {
            if (!n || n >= sizeof(expected) || memchr(data + at, 0, n))
                return RF_FORMAT;
            memcpy(expected, data + at, n);
            expected[n] = 0;
            break;
        }
        at += n;
    }
    for (i = 0; i < g->textures; i++) {
        int s = rf_geometry_texture_name(g, i, actual, sizeof(actual));
        if (s)
            return s;
        if (name_equal(expected, actual)) {
            *index = i;
            return RF_OK;
        }
    }
    return RF_NOT_FOUND;
}
static int reference(const rf_geometry *g, uint32_t source, uint32_t *out) {
    uint32_t i, id, found = UINT32_MAX;
    for (i = 0; i < g->faces; i++) {
        rf_geometry_face f;
        int s = geometry_source(g, i, &id);
        if (s)
            return s;
        if (id != source)
            continue;
        s = rf_geometry_get_face(g, i, &f);
        if (s)
            return s;
        if (found == UINT32_MAX)
            found = i;
        if (f.room == 3) {
            *out = i;
            return RF_OK;
        }
    }
    *out = found;
    return RF_OK;
}
static int plane_mesh(const rf_geomod_mesh_view *m, float (*planes)[4]) {
    uint32_t i, j, k, a, b;
    for (i = 0; i < m->face_count; i++) {
        const rf_geomod_face *f = m->faces + i;
        const rf_geomod_vertex *v = m->vertices + f->first;
        double norm[3], length = 0;
        for (k = 0; k < 3; k++) {
            norm[k] = 0;
            for (j = 0; j < f->count; j++)
                norm[k] += (double)v[j].position[(k + 1) % 3] * v[(j + 1) % f->count].position[(k + 2) % 3] -
                           (double)v[j].position[(k + 2) % 3] * v[(j + 1) % f->count].position[(k + 1) % 3];
            length += norm[k] * norm[k];
        }
        if (length < 1e-24 || !isfinite(length))
            return RF_FORMAT;
        length = sqrt(length);
        planes[i][3] = 0;
        for (k = 0; k < 3; k++) {
            planes[i][k] = (float)(norm[k] / length);
            planes[i][3] -= planes[i][k] * v[0].position[k];
        }
        for (j = 0; j < m->vertex_count; j++) {
            double d = planes[i][3];
            for (k = 0; k < 3; k++)
                d += (double)planes[i][k] * m->vertices[j].position[k];
            if (d > 1e-5)
                return RF_FORMAT;
            if (j >= f->first && j < f->first + f->count && fabs(d) > 1e-5)
                return RF_FORMAT;
        }
        for (j = 0; j < f->count; j++) {
            uint32_t matches = 0;
            const float *x = v[j].position, *y = v[(j + 1) % f->count].position;
            if (same(x, y))
                return RF_FORMAT;
            for (a = 0; a < m->face_count; a++) {
                const rf_geomod_face *other = m->faces + a;
                for (b = 0; b < other->count; b++) {
                    const float *p = m->vertices[other->first + b].position,
                                *q = m->vertices[other->first + (b + 1) % other->count].position;
                    if (same(x, q) && same(y, p))
                        matches++;
                }
            }
            if (matches != 1)
                return RF_FORMAT;
        }
    }
    return RF_OK;
}
/* Bounded ordinary-world profile: original4dbeed synchronizes detail bit8
 * from the room, and4dc4cb/4dc86a reject detail/positive-portal candidates.
 * Collision filters alone do not enforce those gates during reconstructed CSG. */
static int ordinary_source(const rf_collision_face_filter *f) {
    return f->owner_present && !f->owner_kind && !(f->face_flags & 12u) && f->property_34 <= 0;
}
static int import_brush_oriented(const unsigned char *data, const brush_record *b, const rf_geometry *g,
                        rf_geomod_vertex *v, rf_geomod_face *f, rf_geomod_publication_origin *origins,
                        float (*planes)[4], rf_collision_face_filter *filters, uint32_t fallback, uint32_t reverse) {
    uint32_t i, j, at = b->face_offset, nv = 0;
    rf_geomod_mesh_view mesh;
    int s;
    for (i = 0; i < b->faces; i++) {
        const unsigned char *p = data + at;
        uint32_t count = u32(p + 52), stride = u32(p + 20) == UINT32_MAX ? 12 : 20, material, ref;
        uint32_t source = u32(p + 24);
        if (count < 3 || count > 64)
            return RF_FORMAT;
        s = texture(data, b, u32(p + 16), g, &material);
        if (s)
            return s;
        s = reference(g, source, &ref);
        if (s)
            return s;
        if(reverse==2) {
            uint32_t candidate;ref=UINT32_MAX;
            for(candidate=0;candidate<g->faces;candidate++) {
                uint32_t id;rf_geometry_face visible;rf_collision_face_filter filter;
                s=geometry_source(g,candidate,&id);if(s)return s;
                if(id!=source)continue;
                s=rf_geometry_get_face(g,candidate,&visible);if(s)return s;
                if(visible.room!=3)continue;
                s=rf_geometry_initial_collision_filter(g,candidate,0,&filter);if(s)return s;
                if(ordinary_source(&filter)){ref=candidate;break;}
            }
        }
        f[i] = (rf_geomod_face){nv, count, material, source};
        origins[i] = (rf_geomod_publication_origin){
            filters ? RF_GEOMOD_PUBLICATION_RETAINED : RF_GEOMOD_PUBLICATION_NEIGHBOR, b->uid, source, ref};
        if (filters) {
            uint32_t portal = u32(p + 36) & 65535;
            s = rf_geometry_initial_collision_filter(g, ref == UINT32_MAX ? fallback : ref, 0, filters + i);
            if (s)
                return s;
            if (!ordinary_source(filters + i))
                return RF_NOT_FOUND;
            filters[i].face_flags = u32(p + 40);
            filters[i].property_34 = portal >= 32768 ? (int32_t)portal - 65536 : (int32_t)portal;
            if (!ordinary_source(filters + i))
                return RF_NOT_FOUND;
        }
        at += 56;
        for (j = 0; j < count; j++) {
            p = data + at + j * stride;
            s = transform(b, data + b->vertex_offset + u32(p) * 12, v[nv + j].position);
            if (s)
                return s;
            v[nv + j].uv[0] = f32(p + 4);
            v[nv + j].uv[1] = f32(p + 8);
        }
        nv += count;
        at += count * stride;
    }
    if(reverse)for(i=0;i<b->faces;i++)for(j=0;j<f[i].count/2;j++) {
        rf_geomod_vertex temp=v[f[i].first+j];
        v[f[i].first+j]=v[f[i].first+f[i].count-1-j];v[f[i].first+f[i].count-1-j]=temp;
    }
    mesh = (rf_geomod_mesh_view){v, f, nv, b->faces, 0};
    s=plane_mesh(&mesh, planes);
    /* Mode2 validates an inward source through its reversed outward shell,
     * then restores the original corner/UV order and inward plane signs. */
    if(!s && reverse==2) {
        for(i=0;i<b->faces;i++) {
            for(j=0;j<f[i].count/2;j++) {
                rf_geomod_vertex temp=v[f[i].first+j];
                v[f[i].first+j]=v[f[i].first+f[i].count-1-j];v[f[i].first+f[i].count-1-j]=temp;
            }
            for(j=0;j<4;j++)planes[i][j]=-planes[i][j];
        }
    }
    return s;
}
static int import_brush(const unsigned char *data,const brush_record *b,const rf_geometry *g,
    rf_geomod_vertex *v,rf_geomod_face *f,rf_geomod_publication_origin *origins,
    float (*planes)[4],rf_collision_face_filter *filters,uint32_t fallback) {
    return import_brush_oriented(data,b,g,v,f,origins,planes,filters,fallback,0);
}
/* Hidden caps inherit room state from their own brush's visible surface,
 * retaining their authored flags/portal. Clipping copies this immutable row. */
static int import_neighbor_filters(const unsigned char *data,const brush_record *b,const rf_geometry *g,
    const rf_geomod_publication_origin *origins,rf_collision_face_filter *out) {
    uint32_t i,at=b->face_offset,fallback=UINT32_MAX;int status;
    for(i=0;i<b->faces;i++)if(origins[i].reference!=UINT32_MAX){fallback=origins[i].reference;break;}
    if(fallback==UINT32_MAX)return RF_NOT_FOUND;
    for(i=0;i<b->faces;i++) {
        const unsigned char *p=data+at;uint32_t portal=u32(p+36)&65535;
        status=rf_geometry_initial_collision_filter(g,origins[i].reference==UINT32_MAX?fallback:origins[i].reference,0,out+i);
        if(status)return status;
        out[i].face_flags=u32(p+40);
        out[i].property_34=portal>=32768?(int32_t)portal-65536:(int32_t)portal;
        {uint32_t accepted;status=rf_collision_face_accept(out+i,&accepted);if(status)return status;}
        at+=56+u32(p+52)*(u32(p+20)==UINT32_MAX?12:20);
    }
    return RF_OK;
}
static uint64_t aligned(uint64_t n) { return (n + sizeof(void *) - 1) & ~((uint64_t)sizeof(void *) - 1); }
static void *chunk(unsigned char *base, uint64_t *at, uint32_t n, size_t size) {
    void *p;
    *at = aligned(*at);
    p = base ? base + (size_t)*at : NULL;
    *at += (uint64_t)n * size;
    return p;
}
typedef struct beam_profile {uint32_t uid,roof,air,posts[2];} beam_profile;
static const beam_profile beam_profiles[]={
    {95,80,85,{93,94}},{98,82,86,{96,97}},
    {89,69,88,{73,77}},{90,69,88,{72,76}},{91,69,88,{74,78}},{92,69,88,{75,79}},
    {107,81,87,{100,104}},{108,81,87,{99,103}},{109,81,87,{101,105}},{110,81,87,{102,106}}
};
static const beam_profile *find_beam_profile(uint32_t uid) {
    uint32_t i;for(i=0;i<sizeof(beam_profiles)/sizeof(*beam_profiles);i++)if(beam_profiles[i].uid==uid)return beam_profiles+i;
    return NULL;
}
static int decode_profile(const void *input, uint32_t bytes, const rf_geometry *g,
                                   const rf_level_geomod_settings *settings, uint32_t source_uid, uint32_t cavity, uint32_t budget,
                                   rf_geomod_authored_post **out) {
    const unsigned char *data = input;
    cursor c = {data, bytes, 0};
    brush_record *records = NULL, *source = NULL, *near[3] = {0}, *roof_air=NULL;
    rf_geomod_publication_work *clip_work=NULL;
    rf_geomod_vertex *clipped_vertices=NULL;rf_geomod_face *clipped_faces=NULL;
    rf_geomod_publication_origin *clipped_origins=NULL;
    rf_geomod_publication_solid *void_owner=NULL;float (*void_planes)[4]=NULL;
    face_owner *owners = NULL;
    rf_geomod_authored_post *o = NULL;
    rf_geomod_vertex *sv, *wv, *nv;
    rf_geomod_face *sf, *wf, *nf;
    rf_geomod_publication_origin *so, *wo, *no;
    rf_collision_face_filter *filters,*neighbor_filters,*clipped_filters;
    rf_geomod_publication_solid *solids;
    float (*sp)[4], (*np)[4];
    uint32_t *replaced;
    uint32_t count, i, j, k, total_faces = 0, source_index = UINT32_MAX, nnear = 0, nfaces = 0, ncorners = 0,
                             wfaces = 0, wcorners = 0, fallback = UINT32_MAX, air = 0;
    uint64_t scratch, at, peak;
    const beam_profile *beam=find_beam_profile(source_uid);
    int s = RF_FORMAT;
    if (!input || !g || !g->data || !settings || !out || *out)
        return RF_RANGE;
    if (cavity ? source_uid!=66 : (!beam && source_uid != 93 && source_uid != 94 && source_uid != 96 && source_uid != 97))
        return RF_NOT_FOUND;
    if (bytes < 4)
        return RF_FORMAT;
    if (!memchr(settings->texture, 0, sizeof(settings->texture)))
        return RF_FORMAT;
    count = u32(data);
    if (!count || count > MAX_BRUSHES)
        return RF_FORMAT;
    scratch = (uint64_t)bytes + (uint64_t)count * sizeof(*records);
    if (scratch + sizeof(*o) > budget)
        return RF_RANGE;
    records = calloc(count, sizeof(*records));
    if (!records)
        return RF_IO;
    c.at = 4;
    for (i = 0; i < count; i++) {
        s = parse(&c, records + i, i);
        if (s)
            goto done;
        if (records[i].faces > MAX_BRUSH_FACES - total_faces) {
            s = RF_RANGE;
            goto done;
        }
        total_faces += records[i].faces;
        for (j = 0; j < i; j++)
            if (records[i].uid == records[j].uid) {
                s = RF_FORMAT;
                goto done;
            }
        if (records[i].uid == source_uid)
            source_index = i;
    }
    if (c.at != bytes || source_index == UINT32_MAX) {
        s = RF_FORMAT;
        goto done;
    }
    source = records + source_index;
    if (source->flags != (cavity?2u:0u) || source->faces < 4 || source->faces > 32) {
        s = RF_NOT_FOUND;
        goto done;
    }
    for (i = 0; i < count; i++)
        if (!cavity && i != source_index && overlap(source, records + i)) {
            brush_record *b = records + i;
            if (b->uid == 66 && b->flags == 2 && b->index < source_index) {
                air++;
                continue;
            }
            if(beam && b->uid==beam->air && b->flags==2 && b->index<source_index) {
                if(roof_air || b->faces!=5 || b->corners!=18 || source->maximum[1]!=b->minimum[1]){s=RF_NOT_FOUND;goto done;}
                roof_air=b;continue;
            }
            if ((beam ?
                 (b->uid!=beam->roof && b->uid!=beam->posts[0] && b->uid!=beam->posts[1]) :
                 (b->uid != 71 && b->uid != (source_uid <= 94 ? 95u : 98u) && b->uid != 70)) || b->flags || b->faces < 4 || b->faces > 32 ||
                nnear == 3) {
                s = RF_NOT_FOUND;
                goto done;
            }
            near[nnear++] = b;
            nfaces += b->faces;
            ncorners += b->corners;
        }
    if (!cavity && (air != 1 || nnear != 3 || (beam && !roof_air))) {
        s = RF_NOT_FOUND;
        goto done;
    }
    scratch += (uint64_t)total_faces * sizeof(*owners);
    if (scratch + sizeof(*o) > budget) {
        s = RF_RANGE;
        goto done;
    }
    owners = malloc((size_t)total_faces * sizeof(*owners));
    if (!owners) {
        s = RF_IO;
        goto done;
    }
    k = 0;
    for (i = 0; i < count; i++) {
        uint32_t offset = records[i].face_offset;
        for (j = 0; j < records[i].faces; j++) {
            const unsigned char *p = data + offset;
            owners[k++] = (face_owner){u32(p + 24), i};
            offset += 56 + u32(p + 52) * (u32(p + 20) == UINT32_MAX ? 12 : 20);
        }
    }
    qsort(owners, total_faces, sizeof(*owners), owner_compare);
    for (i = 1; i < total_faces; i++)
        if (owners[i].id == owners[i - 1].id) {
            s = RF_FORMAT;
            goto done;
        }
    for (i = 0; i < g->faces; i++) {
        uint32_t id;
        const face_owner *owner;
        rf_geometry_face f;
        s = geometry_source(g, i, &id);
        if (s)
            goto done;
        s = rf_geometry_get_face(g, i, &f);
        if (s)
            goto done;
        owner = find_owner(owners, total_faces, id);
        if (!owner && !(f.flags & 4)) {
            s = RF_FORMAT;
            goto done;
        }
        if (!owner || owner->brush != source_index)
            continue;
        if(cavity && f.room!=3)continue;
        if (f.room != 3 || f.corners < 3 || f.corners > 64) {
            s = RF_NOT_FOUND;
            goto done;
        }
        {
            rf_collision_face_filter filter;
            s = rf_geometry_initial_collision_filter(g, i, 0, &filter);
            if (s)
                goto done;
            if (!ordinary_source(&filter)) {
                if(cavity)continue;
                s = RF_NOT_FOUND;
                goto done;
            }
        }
        if (fallback == UINT32_MAX)
            fallback = i;
        wfaces++;
        wcorners += f.corners;
    }
    if (!wfaces || wfaces > 256 || source->corners > 2048 || ncorners > 6144 || wcorners > 16384) {
        s = RF_RANGE;
        goto done;
    }
    /* Exact packed owner, including alignment; payload and indices coexist here. */
    at = sizeof(*o);
#define ALLOCATE_FIELDS(base)                                                                                \
    sv = chunk(base, &at, source->corners, sizeof(*sv));                                                     \
    wv = chunk(base, &at, wcorners, sizeof(*wv));                                                            \
    nv = chunk(base, &at, ncorners, sizeof(*nv));                                                            \
    sf = chunk(base, &at, source->faces, sizeof(*sf));                                                       \
    wf = chunk(base, &at, wfaces, sizeof(*wf));                                                              \
    nf = chunk(base, &at, nfaces, sizeof(*nf));                                                              \
    sp = chunk(base, &at, source->faces, sizeof(*sp));                                                       \
    np = chunk(base, &at, nfaces, sizeof(*np));                                                              \
    solids = chunk(base, &at, nnear, sizeof(*solids));                                                       \
    so = chunk(base, &at, source->faces, sizeof(*so));                                                       \
    wo = chunk(base, &at, wfaces, sizeof(*wo));                                                              \
    no = chunk(base, &at, nfaces, sizeof(*no));                                                              \
    filters = chunk(base, &at, source->faces, sizeof(*filters));                                             \
    neighbor_filters = chunk(base, &at, nfaces, sizeof(*neighbor_filters));                                 \
    clipped_filters = chunk(base, &at, roof_air?128:0, sizeof(*clipped_filters));                             \
    replaced = chunk(base, &at, wfaces, sizeof(*replaced));                                               \
    clipped_vertices = chunk(base, &at, roof_air?512:0, sizeof(*clipped_vertices));                          \
    clipped_faces = chunk(base, &at, roof_air?128:0, sizeof(*clipped_faces));                                 \
    clipped_origins = chunk(base, &at, roof_air?128:0, sizeof(*clipped_origins));                             \
    void_planes = chunk(base, &at, roof_air?5:0, sizeof(*void_planes));                                      \
    void_owner = chunk(base, &at, roof_air?1:0, sizeof(*void_owner));
    ALLOCATE_FIELDS(NULL);
    if(cavity)chunk(NULL,&at,count-1,sizeof(float[2][3]));
    peak = scratch + at;
    if (peak > budget || at > UINT32_MAX) {
        s = RF_RANGE;
        goto done;
    }
    o = calloc(1, (size_t)at);
    if (!o) {
        s = RF_IO;
        goto done;
    }
    o->view.resident_bytes = (uint32_t)at;
    o->view.peak_bytes = (uint32_t)peak;
    at = sizeof(*o);
    ALLOCATE_FIELDS((unsigned char *)o);
    if(cavity) {
        o->cavity_obstacles=chunk((unsigned char *)o,&at,count-1,sizeof(*o->cavity_obstacles));
        for(i=0;i<count;i++)if(i!=source_index) {
            memcpy(o->cavity_obstacles[o->cavity_obstacle_count][0],records[i].minimum,12);
            memcpy(o->cavity_obstacles[o->cavity_obstacle_count++][1],records[i].maximum,12);
        }
    }
#undef ALLOCATE_FIELDS
    o->view.source = (rf_geomod_mesh_view){sv, sf, source->corners, source->faces, 0};
    o->view.windows = (rf_geomod_mesh_view){wv, wf, wcorners, wfaces, 0};
    o->view.neighbors = (rf_geomod_mesh_view){nv, nf, ncorners, nfaces, 0};
    o->view.source_planes = sp;
    o->view.solids = solids;
    o->view.source_origins = so;
    o->view.window_origins = wo;
    o->view.neighbor_origins = no;
    o->view.source_filters = filters;
    o->view.neighbor_filters = neighbor_filters;
    o->view.replaced_ids = replaced;
    o->view.source_uid = source_uid;
    o->view.room = 3;
    o->view.solid_count = nnear;
    o->view.replaced_count = wfaces;
    o->view.brush_count = count;
    o->view.authored_face_count = total_faces;
    o->view.settings = *settings;
    s = import_brush_oriented(data, source, g, sv, sf, so, sp, filters, fallback,cavity?2:0);
    if (s)
        goto done;
    j = k = 0;
    for (i = 0; i < nnear; i++) {
        uint32_t f;
        s = import_brush(data, near[i], g, nv + j, nf + k, no + k, np + k, NULL, 0);
        if (s)
            goto done;
        s=import_neighbor_filters(data,near[i],g,no+k,neighbor_filters+k);if(s)goto done;
        solids[i] = (rf_geomod_publication_solid){np + k, near[i]->faces, near[i]->uid};
        for (f = 0; f < near[i]->faces; f++)
            nf[k + f].first += j;
        j += near[i]->corners;
        k += near[i]->faces;
    }
    j = k = 0;
    for (i = 0; i < g->faces; i++) {
        uint32_t id, t;
        const face_owner *owner;
        rf_geometry_face f;
        s = geometry_source(g, i, &id);
        if (s)
            goto done;
        owner = find_owner(owners, total_faces, id);
        if (!owner || owner->brush != source_index)
            continue;
        s = rf_geometry_get_face(g, i, &f);
        if (s)
            goto done;
        if(cavity) {
            rf_collision_face_filter filter;
            if(f.room!=3)continue;
            s=rf_geometry_initial_collision_filter(g,i,0,&filter);if(s)goto done;
            if(!ordinary_source(&filter))continue;
        }
        {
            uint32_t source_face;
            for (source_face = 0; source_face < source->faces; source_face++)
                if (sf[source_face].source_face == id)
                    break;
            if (source_face == source->faces || sf[source_face].material != f.texture) {
                s = RF_FORMAT;
                goto done;
            }
        }
        wf[k] = (rf_geomod_face){j, f.corners, f.texture, id};
        wo[k] = (rf_geomod_publication_origin){0, source_uid, id, i};
        replaced[k++] = i;
        for (t = 0; t < f.corners; t++) {
            rf_geometry_corner corner;
            s = rf_geometry_get_corner(g, i, t, &corner);
            if (s)
                goto done;
            s = rf_geometry_vertex(g, corner.vertex, wv[j + t].position);
            if (s)
                goto done;
            memcpy(wv[j + t].uv, corner.uv, 8);
        }
        j += f.corners;
    }
    if(roof_air) {
        rf_geomod_vertex av[18];rf_geomod_face af[5];rf_geomod_publication_origin ao[5];
        rf_geomod_mesh_view clipped;
        uint64_t clip_peak=(uint64_t)bytes+o->view.resident_bytes+sizeof(*clip_work)+sizeof(av)+sizeof(af)+sizeof(ao);
        /* Import air before freeing the parse tables; subsequent clipping no
         * longer needs either table, so their allocations do not overlap. */
        if(peak+sizeof(av)+sizeof(af)+sizeof(ao)>budget || clip_peak>budget){s=RF_RANGE;goto done;}
        s=import_brush_oriented(data,roof_air,g,av,af,ao,void_planes,NULL,0,1);if(s)goto done;
        *void_owner=(rf_geomod_publication_solid){void_planes,5,beam->roof};
        free(owners);owners=NULL;free(records);records=NULL;
        clip_work=malloc(sizeof(*clip_work));if(!clip_work){s=RF_IO;goto done;}
        s=rf_geomod_publication_clip_neighbors(&o->view.neighbors,o->view.neighbor_origins,
            void_owner,1,clip_work,clipped_vertices,512,clipped_faces,128,clipped_origins,&clipped);if(s)goto done;
        for(i=0;i<clipped.face_count;i++) {
            for(j=0;j<nfaces;j++)if(no[j].owner==clipped_origins[i].owner && no[j].source_face==clipped_origins[i].source_face)break;
            if(j==nfaces){s=RF_FORMAT;goto done;}
            clipped_filters[i]=neighbor_filters[j];
        }
        o->view.neighbors=clipped;o->view.neighbor_origins=clipped_origins;o->view.neighbor_filters=clipped_filters;
        o->view.neighbor_voids=void_owner;o->view.neighbor_void_count=1;
        peak+=sizeof(av)+sizeof(af)+sizeof(ao);if(clip_peak>peak)peak=clip_peak;
        o->view.peak_bytes=(uint32_t)peak;
    }
    *out = o;
    o = NULL;
    s = RF_OK;
done:
    free(clip_work);
    free(o);
    free(owners);
    free(records);
    return s;
}
int rf_geomod_authored_post_decode_source(const void *input,uint32_t bytes,const rf_geometry *geometry,
    const rf_level_geomod_settings *settings,uint32_t uid,uint32_t budget,rf_geomod_authored_post **out) {
    return decode_profile(input,bytes,geometry,settings,uid,0,budget,out);
}
int rf_geomod_authored_cavity_decode(const void *input,uint32_t bytes,const rf_geometry *geometry,
    const rf_level_geomod_settings *settings,uint32_t budget,rf_geomod_authored_post **out) {
    return decode_profile(input,bytes,geometry,settings,66,1,budget,out);
}
static int open_profile(const rf_level *level, const rf_geometry *geometry,
                                        uint32_t source_uid,uint32_t cavity, uint32_t budget,
                                 rf_geomod_authored_post **out) {
    const rf_level_section *section;
    rf_level_geomod_settings settings;
    unsigned char *data;
    int status;
    if (!level || !geometry || !out || *out)
        return RF_RANGE;
    if (level->version != 180 || strcmp(level->entry.name, "ctf06.rfl"))
        return RF_NOT_FOUND;
    section = rf_level_find(level, 0x2000000);
    if (!section)
        return RF_NOT_FOUND;
    if (section->size > budget || section->size < 4)
        return RF_RANGE;
    status = rf_level_geomod_settings_read(level, &settings);
    if (status)
        return status;
    data = malloc(section->size);
    if (!data)
        return RF_IO;
    status = rf_level_read(level, section, 0, data, section->size);
    if (!status)
        status = decode_profile(data, section->size, geometry, &settings, source_uid,cavity, budget, out);
    free(data);
    return status;
}
int rf_geomod_authored_post_open_source(const rf_level *level,const rf_geometry *geometry,
    uint32_t uid,uint32_t budget,rf_geomod_authored_post **out) {
    return open_profile(level,geometry,uid,0,budget,out);
}
int rf_geomod_authored_cavity_open(const rf_level *level,const rf_geometry *geometry,
    uint32_t budget,rf_geomod_authored_post **out) {
    return open_profile(level,geometry,66,1,budget,out);
}
/* Existing scene/checkpoint profile remains UID94. */
int rf_geomod_authored_post_decode(const void *input, uint32_t bytes, const rf_geometry *geometry,
                                   const rf_level_geomod_settings *settings, uint32_t budget,
                                   rf_geomod_authored_post **out) {
    return rf_geomod_authored_post_decode_source(input, bytes, geometry, settings, 94, budget, out);
}
int rf_geomod_authored_post_open(const rf_level *level, const rf_geometry *geometry, uint32_t budget,
                                 rf_geomod_authored_post **out) {
    return rf_geomod_authored_post_open_source(level, geometry, 94, budget, out);
}
/* A pair may be treated as one region only across an exact reversed full
 * edge and only when every exterior edge bounds a convex union. This neither
 * fills gaps nor assumes that arbitrary coplanar windows form a solid wall. */
static int cavity_window_contains(const rf_geomod_authored_post_view *a,uint32_t index,
    uint32_t skip,const float plane[4],const double point[3]) {
    const rf_geomod_face *f=a->windows.faces+index;uint32_t j,k;
    for(j=0;j<f->count;j++)if(j!=skip) {
        const float *x=a->windows.vertices[f->first+j].position;
        const float *y=a->windows.vertices[f->first+(j+1)%f->count].position;
        double edge[3],offset[3],side=0,length=0;
        for(k=0;k<3;k++){edge[k]=(double)y[k]-x[k];offset[k]=point[k]-x[k];length+=edge[k]*edge[k];}
        for(k=0;k<3;k++)side+=plane[k]*(edge[(k+1)%3]*offset[(k+2)%3]-edge[(k+2)%3]*offset[(k+1)%3]);
        if(side< -1e-5*sqrt(length))return 0;
    }
    return 1;
}
static int cavity_window_pair(const rf_geomod_authored_post_view *a,uint32_t first,uint32_t second,
    const float plane[4],uint32_t skip[2]) {
    const rf_geomod_face *x=a->windows.faces+first,*y=a->windows.faces+second;uint32_t i,j,k,n;
    if(x->source_face!=y->source_face)return 0;
    for(i=0;i<x->count;i++)for(j=0;j<y->count;j++) {
        const float *p=a->windows.vertices[x->first+i].position,*q=a->windows.vertices[x->first+(i+1)%x->count].position;
        const float *r=a->windows.vertices[y->first+j].position,*t=a->windows.vertices[y->first+(j+1)%y->count].position;
        if(memcmp(p,t,12) || memcmp(q,r,12))continue;
        for(n=0;n<2;n++) {
            const rf_geomod_face *f=n?y:x;
            for(k=0;k<f->count;k++) {
                const float *v=a->windows.vertices[f->first+k].position;double point[3]={v[0],v[1],v[2]};
                if(!cavity_window_contains(a,first,i,plane,point) || !cavity_window_contains(a,second,j,plane,point))return 0;
            }
        }
        skip[0]=i;skip[1]=j;return 1;
    }
    return 0;
}
int rf_geomod_authored_cavity_admit(const rf_geomod_authored_post *o,const float minimum[3],
    const float maximum[3],uint32_t *reference) {
    const rf_geomod_authored_post_view *a;uint32_t i,j,k,corner;
    if(!o || !minimum || !maximum || !reference)return RF_RANGE;
    a=&o->view;if(a->source_uid!=66 || !o->cavity_obstacles || !o->cavity_obstacle_count)return RF_NOT_FOUND;
    for(k=0;k<3;k++)if(!isfinite(minimum[k]) || !isfinite(maximum[k]) || minimum[k]>maximum[k])return RF_RANGE;
    for(i=0;i<o->cavity_obstacle_count;i++) {
        for(k=0;k<3;k++)if(maximum[k]<o->cavity_obstacles[i][0][k]-1e-5f || minimum[k]>o->cavity_obstacles[i][1][k]+1e-5f)break;
        if(k==3)return RF_NOT_FOUND;
    }
    for(i=0;i<a->windows.face_count;i++) {
        const rf_geomod_face *window=a->windows.faces+i;const float *plane;double lo;float corridor[2][3];uint32_t accepted=1;
        for(j=0;j<a->source.face_count;j++)if(a->source.faces[j].source_face==window->source_face)break;
        if(j==a->source.face_count)return RF_FORMAT;plane=a->source_planes[j];lo=plane[3];
        memcpy(corridor[0],minimum,12);memcpy(corridor[1],maximum,12);
        for(k=0;k<3;k++)lo+=(double)plane[k]*(plane[k]<0?maximum[k]:minimum[k]);
        if(lo>0)continue; /* Cutter must reach the solid side, possibly behind an earlier cut. */
        {
            double projected[8][3];uint32_t partner;
            for(corner=0;corner<8;corner++) {
                double d=plane[3];
                for(k=0;k<3;k++){projected[corner][k]=(corner&(1u<<k))?maximum[k]:minimum[k];d+=plane[k]*projected[corner][k];}
                for(k=0;k<3;k++) {
                    double v=projected[corner][k]-d*plane[k];projected[corner][k]=v;
                    if(v<corridor[0][k])corridor[0][k]=(float)v;
                    if(v>corridor[1][k])corridor[1][k]=(float)v;
                }
                if(!cavity_window_contains(a,i,UINT32_MAX,plane,projected[corner]))accepted=0;
            }
            for(partner=i+1;!accepted && partner<a->windows.face_count;partner++) {
                uint32_t skip[2];if(!cavity_window_pair(a,i,partner,plane,skip))continue;
                accepted=1;
                for(corner=0;corner<8 && accepted;corner++)
                    if(!cavity_window_contains(a,i,skip[0],plane,projected[corner]) ||
                       !cavity_window_contains(a,partner,skip[1],plane,projected[corner]))accepted=0;
            }
        }
        /* A deep cutter cannot jump past an intervening authored brush. */
        for(j=0;j<o->cavity_obstacle_count && accepted;j++) {
            for(k=0;k<3;k++)if(corridor[1][k]<o->cavity_obstacles[j][0][k]-1e-5f ||
                corridor[0][k]>o->cavity_obstacles[j][1][k]+1e-5f)break;
            if(k==3)accepted=0;
        }
        if(accepted){*reference=a->window_origins[i].reference;return RF_OK;}
    }
    return RF_NOT_FOUND;
}
int rf_geomod_authored_post_get(const rf_geomod_authored_post *o, rf_geomod_authored_post_view *v) {
    if (!o || !v)
        return RF_RANGE;
    *v = o->view;
    return RF_OK;
}
void rf_geomod_authored_post_close(rf_geomod_authored_post **o) {
    if (o && *o) {
        free(*o);
        *o = NULL;
    }
}
