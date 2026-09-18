#include "rf/geomod_publication.h"
#include <math.h>
#include <string.h>
static int mesh_valid(const rf_geomod_mesh_view *m) {
    uint32_t i, j;
    if ((m->vertex_count && !m->vertices) || (m->face_count && !m->faces))
        return RF_RANGE;
    for (i = 0; i < m->vertex_count; i++) {
        for (j = 0; j < 3; j++)
            if (!isfinite(m->vertices[i].position[j]))
                return RF_FORMAT;
        for (j = 0; j < 2; j++)
            if (!isfinite(m->vertices[i].uv[j]))
                return RF_FORMAT;
    }
    for (i = 0; i < m->face_count; i++) {
        const rf_geomod_face *f = m->faces + i;
        if (f->count < 3 || f->count > 64 || f->first > m->vertex_count ||
            f->count > m->vertex_count - f->first)
            return RF_FORMAT;
    }
    return RF_OK;
}
static int planes_valid(const float (*p)[4], uint32_t n) {
    uint32_t i, k;
    if (!p || !n || n > 32)
        return RF_RANGE;
    for (i = 0; i < n; i++) {
        double d = 0;
        for (k = 0; k < 4; k++)
            if (!isfinite(p[i][k]))
                return RF_FORMAT;
        for (k = 0; k < 3; k++)
            d += (double)p[i][k] * p[i][k];
        if (fabs(d - 1) > 1e-4)
            return RF_FORMAT;
    }
    return RF_OK;
}
static int plane(const rf_geomod_vertex *v, uint32_t n, float p[4]) {
    uint32_t i, k;
    double q[3], len = 0;
    for (k = 0; k < 3; k++) {
        q[k] = 0;
        for (i = 0; i < n; i++)
            q[k] += (double)v[i].position[(k + 1) % 3] * v[(i + 1) % n].position[(k + 2) % 3] -
                    (double)v[i].position[(k + 2) % 3] * v[(i + 1) % n].position[(k + 1) % 3];
        len += q[k] * q[k];
    }
    if (!isfinite(len) || len < 1e-24)
        return RF_FORMAT;
    len = sqrt(len);
    p[3] = 0;
    for (k = 0; k < 3; k++) {
        p[k] = (float)(q[k] / len);
        p[3] -= p[k] * v[0].position[k];
    }
    return RF_OK;
}
static float distance(const float p[4], const float v[3]) {
    return (float)((double)p[0] * v[0] + (double)p[1] * v[1] + (double)p[2] * v[2] + p[3]);
}
static int append(rf_geomod_publication_bank *b, const rf_geomod_vertex *v, uint32_t n, rf_geomod_face f) {
    if (n < 3)
        return RF_OK;
    if (n > RF_GEOMOD_PUBLICATION_VERTICES - b->nv || b->nf == RF_GEOMOD_PUBLICATION_FACES)
        return RF_RANGE;
    f.first = b->nv;
    f.count = n;
    memcpy(b->vertices + b->nv, v, n * sizeof(*v));
    b->nv += n;
    b->faces[b->nf++] = f;
    return RF_OK;
}
static int result(rf_geomod_publication_work *w, const rf_geomod_vertex *v, uint32_t n, rf_geomod_face f,
                  rf_geomod_publication_origin origin) {
    rf_geomod_mesh_view source, partitioned;
    uint32_t first_vertex = w->result.nv, first_face = w->result.nf, i;
    int s;
    if (n < 3) return RF_OK;
    if (first_vertex == RF_GEOMOD_PUBLICATION_VERTICES || first_face == RF_GEOMOD_PUBLICATION_FACES)
        return RF_RANGE;
    f.first = 0; f.count = n;
    source = (rf_geomod_mesh_view){v, &f, n, 1, 0};
    /* Neighbor/window clipping can bend a mathematically straight edge after
     * float rounding. Preserve its vertices/UVs and partition for the same
     * strict convex collision contract used by the terrain core. */
    s = rf_geomod_partition_mesh(&source, w->result.vertices + first_vertex,
        RF_GEOMOD_PUBLICATION_VERTICES - first_vertex, w->result.faces + first_face,
        RF_GEOMOD_PUBLICATION_FACES - first_face, &partitioned);
    if (s) return s;
    origin.source_face = f.source_face;
    for (i = 0; i < partitioned.face_count; i++) {
        w->result.faces[first_face + i].first += first_vertex;
        w->origins[first_face + i] = origin;
    }
    w->result.nv += partitioned.vertex_count;
    w->result.nf += partitioned.face_count;
    return RF_OK;
}
/* Keep positive halfspace, including wholly coplanar polygons. */
static int clip(rf_geomod_publication_work *w, uint32_t *n, const float p[4]) {
    uint32_t a, b;
    int s;
    if (!*n)
        return RF_OK;
    s = rf_geomod_polygon_split(w->polygon[0], *n, p, w->polygon[1], 64, w->polygon[2], 64, &a, &b);
    if (s)
        return s;
    *n = a;
    memcpy(w->polygon[0], w->polygon[1], a * sizeof(rf_geomod_vertex));
    return RF_OK;
}
static int clip_negative(rf_geomod_publication_work *w, uint32_t *n, const float (*p)[4], uint32_t count) {
    uint32_t i, k;
    int s;
    for (i = 0; i < count && *n; i++) {
        float inverse[4];
        for (k = 0; k < 4; k++)
            inverse[k] = -p[i][k];
        s = clip(w, n, inverse);
        if (s)
            return s;
    }
    return RF_OK;
}
static int subtract(rf_geomod_publication_work *w, uint32_t *bank, const float (*planes)[4], uint32_t count) {
    rf_geomod_publication_bank *a = w->banks + *bank, *b = w->banks + 1 - *bank;
    uint32_t i, j, nv, nf;
    int s;
    b->nv = b->nf = 0;
    for (i = 0; i < a->nf; i++) {
        rf_geomod_face f = a->faces[i];
        s = rf_geomod_polygon_subtract(a->vertices + f.first, f.count, planes, count, w->split_vertices, 2048,
                                       w->fragments, 128, &nv, &nf);
        if (s)
            return s;
        for (j = 0; j < nf; j++) {
            s = append(b, w->split_vertices + w->fragments[j].first, w->fragments[j].count, f);
            if (s)
                return s;
        }
    }
    *bank = 1 - *bank;
    return RF_OK;
}
/* P minus (solid minus void) = (P minus solid) union (P intersect
 * solid intersect void). The two retained regions have disjoint interiors. */
static int subtract_neighbor(rf_geomod_publication_work *w,uint32_t *bank,
    const rf_geomod_publication_solid *solid,const rf_geomod_publication_job *job) {
    const rf_geomod_publication_solid *hole=NULL;
    rf_geomod_publication_bank *a=w->banks+*bank,*b=w->banks+1-*bank;
    uint32_t i,j,n,nv,nf;int status;
    for(i=0;i<job->neighbor_void_count;i++)if(job->neighbor_voids[i].owner==solid->owner)hole=job->neighbor_voids+i;
    if(!hole)return subtract(w,bank,solid->planes,solid->count);
    b->nv=b->nf=0;
    for(i=0;i<a->nf;i++) {
        rf_geomod_face f=a->faces[i];
        status=rf_geomod_polygon_subtract(a->vertices+f.first,f.count,solid->planes,solid->count,
            w->split_vertices,2048,w->fragments,128,&nv,&nf);if(status)return status;
        for(j=0;j<nf;j++) {
            status=append(b,w->split_vertices+w->fragments[j].first,w->fragments[j].count,f);if(status)return status;
        }
        /* Existing subtraction retains opposite-facing coplanar contact.
         * It already includes the opening in that case; adding it again
         * would duplicate area on the shared boundary. */
        {float surface[4];uint32_t q,k,kept_contact=0;
         status=plane(a->vertices+f.first,f.count,surface);if(status)return status;
         for(q=0;q<solid->count && !kept_contact;q++) {
             double dot=0;uint32_t coplanar=1;
             for(k=0;k<3;k++)dot+=(double)surface[k]*solid->planes[q][k];
             if(dot>=0)continue;
             for(k=0;k<f.count;k++)if(fabsf(distance(solid->planes[q],a->vertices[f.first+k].position))>1e-5f){coplanar=0;break;}
             kept_contact=coplanar;
         }
         if(kept_contact)continue;}
        n=f.count;memcpy(w->polygon[0],a->vertices+f.first,n*sizeof(rf_geomod_vertex));
        status=clip_negative(w,&n,solid->planes,solid->count);if(status)return status;
        status=clip_negative(w,&n,hole->planes,hole->count);if(status)return status;
        status=append(b,w->polygon[0],n,f);if(status)return status;
    }
    *bank=1-*bank;return RF_OK;
}
static int emit_bank(rf_geomod_publication_work *w, uint32_t bank, rf_geomod_publication_origin origin) {
    uint32_t i;
    int s;
    for (i = 0; i < w->banks[bank].nf; i++) {
        rf_geomod_face f = w->banks[bank].faces[i];
        s = result(w, w->banks[bank].vertices + f.first, f.count, f, origin);
        if (s)
            return s;
    }
    return RF_OK;
}
static int prepare_cuts(const rf_geomod_publication_job *j, rf_geomod_publication_work *w) {
    uint32_t c, f, e, k, a, b;
    int s;
    w->tetra_count = 0;
    for (c = 0; c < j->cut_count; c++) {
        const rf_geomod_publication_cut *cut = j->cuts + c;
        if (!cut->star)
            return RF_NOT_FOUND;
        if (cut->mesh.face_count > 20 || cut->mesh.vertex_count > 60 || cut->mesh.face_count < 4)
            return RF_RANGE;
        s = mesh_valid(&cut->mesh);
        if (s)
            return s;
        for (k = 0; k < 3; k++)
            if (!isfinite(cut->kernel[k]))
                return RF_FORMAT;
        s = rf_geomod_seed_adjacency(&cut->mesh, w->adjacency, 60);
        if (s)
            return s;
        for (f = 0; f < cut->mesh.face_count; f++) {
            rf_geomod_vertex tetra[4], tri[3];
            float outer[4];
            const rf_geomod_face *face = cut->mesh.faces + f;
            if (face->count != 3)
                return RF_FORMAT;
            memcpy(tetra, cut->mesh.vertices + face->first, 3 * sizeof(*tetra));
            memset(tetra + 3, 0, sizeof(*tetra));
            memcpy(tetra[3].position, cut->kernel, 12);
            s = plane(tetra, 3, outer);
            if (s)
                return s;
            if (distance(outer, cut->kernel) >= -1e-6f)
                return RF_FORMAT;
            for (e = 0; e < 4; e++) {
                float *p = w->tetra[w->tetra_count][e];
                a = 0;
                for (b = 0; b < 4; b++)
                    if (b != e)
                        tri[a++] = tetra[b];
                s = plane(tri, 3, p);
                if (s)
                    return s;
                if (distance(p, tetra[e].position) > 0)
                    for (k = 0; k < 4; k++)
                        p[k] = -p[k];
            }
            w->tetra_count++;
        }
    }
    return RF_OK;
}
typedef struct connected_context {
    const rf_geomod_publication_job *jobs,*current;
    uint32_t count;
    rf_geomod_publication_connected_work *work;
} connected_context;
static const rf_geomod_publication_job *connected_owner(const connected_context *c,uint32_t owner) {
    uint32_t i;if(c)for(i=0;i<c->count;i++)if(c->jobs[i].crater_origin.owner==owner)return c->jobs+i;
    return NULL;
}
static int emit_connected(rf_geomod_publication_work *,uint32_t,rf_geomod_publication_origin,const connected_context *);
static int publication_append(const rf_geomod_publication_job *j, rf_geomod_publication_work *w,const connected_context *context,uint32_t cavity) {
    uint32_t i, k, a, b, n, bank;
    int s;
    if (!j || !w ||
        j->solid_count > RF_GEOMOD_PUBLICATION_NEIGHBORS || j->cut_count > RF_GEOMOD_CUT_LIMIT ||
        j->neighbor_void_count > RF_GEOMOD_PUBLICATION_NEIGHBORS || (j->neighbor_void_count && !j->neighbor_voids) ||
        (j->solid_count && !j->solids) || (j->cut_count && !j->cuts) ||
        (j->windows.face_count && !j->window_origins) || (j->neighbors.face_count && !j->neighbor_origins))
        return RF_RANGE;
    s = mesh_valid(&j->terrain);
    if (s)
        return s;
    s = mesh_valid(&j->windows);
    if (s)
        return s;
    s = mesh_valid(&j->neighbors);
    if (s)
        return s;
    s = planes_valid(j->source_planes, j->source_plane_count);
    if (s)
        return s;
    for (i = 0; i < j->solid_count; i++) {
        s = planes_valid(j->solids[i].planes, j->solids[i].count);
        if (s)
            return s;
        for (k = 0; k < i; k++)
            if (j->solids[k].owner == j->solids[i].owner)
                return RF_FORMAT;
    }
    for(i=0;i<j->neighbor_void_count;i++) {
        uint32_t found=0;
        s=planes_valid(j->neighbor_voids[i].planes,j->neighbor_voids[i].count);if(s)return s;
        for(k=0;k<j->solid_count;k++)found|=j->solids[k].owner==j->neighbor_voids[i].owner;
        if(!found)return RF_FORMAT;
        for(k=0;k<i;k++)if(j->neighbor_voids[k].owner==j->neighbor_voids[i].owner)return RF_FORMAT;
    }
    s = prepare_cuts(j, w);
    if (s)
        return s;
    for (i = 0; i < j->terrain.face_count; i++) {
        rf_geomod_face f = j->terrain.faces[i];
        if (f.source_face == UINT32_MAX) {
            bank = 0;
            w->banks[0].nv = w->banks[0].nf = 0;
            s = append(w->banks, j->terrain.vertices + f.first, f.count, f);
            if (s)
                return s;
            for (k = 0; k < j->solid_count; k++) {
                if(connected_owner(context,j->solids[k].owner))continue;
                s = subtract_neighbor(w, &bank, j->solids+k, j);
                if (s)
                    return s;
            }
            {
                rf_geomod_publication_origin o = j->crater_origin;
                o.kind = RF_GEOMOD_PUBLICATION_CRATER;
                s = emit_connected(w, bank, o, context);
                if (s)
                    return s;
            }
        } else
            for (k = 0; k < j->windows.face_count; k++) {
                const rf_geomod_face *window = j->windows.faces + k;
                float normal[4];
                const rf_geomod_vertex *boundary=cavity?j->terrain.vertices+f.first:j->windows.vertices+window->first;
                uint32_t boundary_count=cavity?f.count:window->count;
                if (window->source_face != f.source_face)
                    continue;
                s = plane(j->windows.vertices + window->first, window->count, normal);
                if (s)
                    return s;
                n = cavity?window->count:f.count;
                memcpy(w->polygon[0], cavity?j->windows.vertices+window->first:j->terrain.vertices+f.first, n * sizeof(rf_geomod_vertex));
                for (a = 0; a < n; a++)
                    if (fabsf(distance(normal, w->polygon[0][a].position)) > 1e-4f)
                        return RF_FORMAT;
                for (a = 0; a < boundary_count && n; a++) {
                    float p[4], edge[3], len = 0;
                    const float *x = boundary[a].position,
                                *y = boundary[(a + 1) % boundary_count].position;
                    for (b = 0; b < 3; b++)
                        edge[b] = y[b] - x[b];
                    for (b = 0; b < 3; b++) {
                        p[b] =
                            normal[(b + 1) % 3] * edge[(b + 2) % 3] - normal[(b + 2) % 3] * edge[(b + 1) % 3];
                        len += p[b] * p[b];
                    }
                    if (len < 1e-20f)
                        return RF_FORMAT;
                    len = sqrtf(len);
                    p[3] = 0;
                    for (b = 0; b < 3; b++) {
                        p[b] /= len;
                        p[3] -= p[b] * x[b];
                    }
                    s = clip(w, &n, p);
                    if (s)
                        return s;
                }
                f.material = window->material;
                {
                    rf_geomod_publication_origin o = j->window_origins[k];
                    o.kind = RF_GEOMOD_PUBLICATION_RETAINED;
                    s = result(w, w->polygon[0], n, f, o);
                    if (s)
                        return s;
                }
            }
    }
    /* Exposed neighbor pieces: source overlap intersect cut union, first cut wins.
     * Subtract preceding tetrahedra as well as preceding cutters to avoid duplicated
     * floor patches across repeated/overlapping cuts. */
    for (i = 0; i < j->neighbors.face_count; i++) {
        rf_geomod_face f = j->neighbors.faces[i];
        rf_geomod_publication_origin origin = j->neighbor_origins[i];
        uint32_t has_owner = 0;
        origin.kind = RF_GEOMOD_PUBLICATION_NEIGHBOR;
        for (k = 0; k < j->solid_count; k++)
            has_owner |= j->solids[k].owner == origin.owner;
        if (!has_owner)
            return RF_FORMAT;
        for (k = 0; k < w->tetra_count; k++) {
            n = f.count;
            memcpy(w->polygon[0], j->neighbors.vertices + f.first, n * sizeof(rf_geomod_vertex));
            s = clip_negative(w, &n, j->source_planes, j->source_plane_count);
            if (s)
                return s;
            s = clip_negative(w, &n, w->tetra[k], 4);
            if (s)
                return s;
            if (n < 3)
                continue;
            bank = 0;
            w->banks[0].nv = w->banks[0].nf = 0;
            s = append(w->banks, w->polygon[0], n, f);
            if (s)
                return s;
            for (a = 0; a < j->solid_count; a++)
                if (j->solids[a].owner != origin.owner) {
                    if(connected_owner(context,j->solids[a].owner))continue;
                    s = subtract_neighbor(w, &bank, j->solids+a, j);
                    if (s)
                        return s;
                }
            for (a = 0; a < k; a++) {
                s = subtract(w, &bank, w->tetra[a], 4);
                if (s)
                    return s;
            }
            s = emit_connected(w, bank, origin, context);
            if (s)
                return s;
        }
    }
    return RF_OK;
}
static int publication_copy(rf_geomod_publication_work *w, uint32_t generation,
    rf_geomod_vertex *vertices, uint32_t vc, rf_geomod_face *faces, uint32_t fc,
    rf_geomod_publication_origin *origins, rf_geomod_mesh_view *out) {
    if (w->result.nv > vc || w->result.nf > fc)
        return RF_RANGE;
    memcpy(vertices, w->result.vertices, w->result.nv * sizeof(*vertices));
    memcpy(faces, w->result.faces, w->result.nf * sizeof(*faces));
    memcpy(origins, w->origins, w->result.nf * sizeof(*origins));
    *out = (rf_geomod_mesh_view){vertices, faces, w->result.nv, w->result.nf, generation};
    return RF_OK;
}

int rf_geomod_publication_build(const rf_geomod_publication_job *j, rf_geomod_publication_work *w,
    rf_geomod_vertex *vertices, uint32_t vc, rf_geomod_face *faces, uint32_t fc,
    rf_geomod_publication_origin *origins, rf_geomod_mesh_view *out) {
    int status;
    if (!j || !w || !vertices || !faces || !origins || !out) return RF_RANGE;
    w->result.nv = w->result.nf = 0;
    status = publication_append(j, w, NULL,0);
    if (status) return status;
    return publication_copy(w, j->terrain.generation, vertices, vc, faces, fc, origins, out);
}
int rf_geomod_publication_build_cavity(const rf_geomod_publication_job *j,rf_geomod_publication_work *w,
    rf_geomod_vertex *vertices,uint32_t vc,rf_geomod_face *faces,uint32_t fc,
    rf_geomod_publication_origin *origins,rf_geomod_mesh_view *out) {
    int status;
    if(!j || !w || !vertices || !faces || !origins || !out)return RF_RANGE;
    /* Local cavity publication cannot apply the outward neighbor-solid path. */
    if(j->solid_count || j->neighbors.face_count || j->neighbor_void_count)return RF_NOT_FOUND;
    w->result.nv=w->result.nf=0;
    status=publication_append(j,w,NULL,1);if(status)return status;
    return publication_copy(w,j->terrain.generation,vertices,vc,faces,fc,origins,out);
}
int rf_geomod_publication_build_groups(const rf_geomod_publication_job *jobs, uint32_t count,
    uint32_t generation, rf_geomod_publication_work *w,
    rf_geomod_vertex *vertices, uint32_t vc, rf_geomod_face *faces, uint32_t fc,
    rf_geomod_publication_origin *origins, rf_geomod_mesh_view *out) {
    uint32_t i, k; int status;
    if (!jobs || !count || count > 32 || !w || !vertices || !faces || !origins || !out) return RF_RANGE;
    for (i = 0; i < count; i++) {
        if (jobs[i].crater_origin.owner == UINT32_MAX) return RF_FORMAT;
        for (k = 0; k < i; k++)
            if (jobs[i].crater_origin.owner == jobs[k].crater_origin.owner) return RF_FORMAT;
    }
    w->result.nv = w->result.nf = 0;
    for (i = 0; i < count; i++) {
        status = publication_append(jobs + i, w, NULL,0);
        if (status) return status;
    }
    return publication_copy(w, generation, vertices, vc, faces, fc, origins, out);
}

int rf_geomod_publication_clip_neighbors(const rf_geomod_mesh_view *mesh,
    const rf_geomod_publication_origin *input_origins,
    const rf_geomod_publication_solid *voids, uint32_t count,
    rf_geomod_publication_work *w, rf_geomod_vertex *vertices, uint32_t vc,
    rf_geomod_face *faces, uint32_t fc, rf_geomod_publication_origin *origins,
    rf_geomod_mesh_view *out) {
    uint32_t i,j,bank; int status;
    if(!mesh || !w || !vertices || !faces || !origins || !out ||
       (mesh->face_count && !input_origins) || (count && !voids) ||
       count>RF_GEOMOD_PUBLICATION_NEIGHBORS)return RF_RANGE;
    status=mesh_valid(mesh);if(status)return status;
    for(i=0;i<count;i++) {
        status=planes_valid(voids[i].planes,voids[i].count);if(status)return status;
        if(voids[i].owner==UINT32_MAX)return RF_FORMAT;
    }
    w->result.nv=w->result.nf=0;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *face=mesh->faces+i;
        if(input_origins[i].owner==UINT32_MAX || input_origins[i].kind>RF_GEOMOD_PUBLICATION_NEIGHBOR)return RF_FORMAT;
        bank=0;w->banks[0].nv=w->banks[0].nf=0;
        status=append(w->banks,mesh->vertices+face->first,face->count,*face);if(status)return status;
        for(j=0;j<count;j++)if(voids[j].owner==input_origins[i].owner) {
            status=subtract(w,&bank,voids[j].planes,voids[j].count);if(status)return status;
        }
        status=emit_bank(w,bank,input_origins[i]);if(status)return status;
    }
    return publication_copy(w,mesh->generation,vertices,vc,faces,fc,origins,out);
}

int rf_geomod_publication_cut_neighbors(const rf_geomod_mesh_view *mesh,
    const rf_geomod_publication_origin *input_origins,uint32_t owner,
    const rf_geomod_publication_cut *cuts,uint32_t count,
    rf_geomod_publication_work *w,rf_geomod_vertex *vertices,uint32_t vc,
    rf_geomod_face *faces,uint32_t fc,rf_geomod_publication_origin *origins,
    rf_geomod_mesh_view *out) {
    rf_geomod_publication_job job={0};uint32_t i,k,bank;int status;
    if(!mesh || !w || !vertices || !faces || !origins || !out ||
       owner==UINT32_MAX || (mesh->face_count && !input_origins) ||
       (count && !cuts) || count>RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
    status=mesh_valid(mesh);if(status)return status;
    job.cuts=cuts;job.cut_count=count;
    status=prepare_cuts(&job,w);if(status)return status;
    w->result.nv=w->result.nf=0;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *f=mesh->faces+i;
        if(input_origins[i].owner==UINT32_MAX || input_origins[i].kind>RF_GEOMOD_PUBLICATION_NEIGHBOR)return RF_FORMAT;
        bank=0;w->banks[0].nv=w->banks[0].nf=0;
        status=append(w->banks,mesh->vertices+f->first,f->count,*f);if(status)return status;
        if(input_origins[i].owner==owner)for(k=0;k<w->tetra_count;k++) {
            status=subtract(w,&bank,w->tetra[k],4);if(status)return status;
        }
        status=emit_bank(w,bank,input_origins[i]);if(status)return status;
    }
    return publication_copy(w,mesh->generation,vertices,vc,faces,fc,origins,out);
}

int rf_geomod_publication_occlude_neighbor(const rf_geomod_mesh_view *mesh,
    const rf_geomod_publication_origin *input_origins,const rf_geomod_publication_solid *solid,
    const rf_geomod_publication_solid *hole,const rf_geomod_publication_cut *cuts,uint32_t count,
    rf_geomod_publication_work *w,rf_geomod_vertex *vertices,uint32_t vc,
    rf_geomod_face *faces,uint32_t fc,rf_geomod_publication_origin *origins,rf_geomod_mesh_view *out) {
    rf_geomod_publication_job job={0};uint32_t i,k,q,n,bank;int status;
    if(!mesh || !solid || !w || !vertices || !faces || !origins || !out ||
       (mesh->face_count && !input_origins) || (count && !cuts) || count>RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
    if(solid->owner==UINT32_MAX || (hole && hole->owner!=solid->owner))return RF_FORMAT;
    status=mesh_valid(mesh);if(status)return status;
    status=planes_valid(solid->planes,solid->count);if(status)return status;
    if(hole){status=planes_valid(hole->planes,hole->count);if(status)return status;}
    job.cuts=cuts;job.cut_count=count;status=prepare_cuts(&job,w);if(status)return status;
    w->result.nv=w->result.nf=0;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *f=mesh->faces+i;uint32_t kept_contact=0;float surface[4];
        if(input_origins[i].owner==UINT32_MAX || input_origins[i].kind>RF_GEOMOD_PUBLICATION_NEIGHBOR)return RF_FORMAT;
        bank=0;w->banks[0].nv=w->banks[0].nf=0;
        status=append(w->banks,mesh->vertices+f->first,f->count,*f);if(status)return status;
        status=subtract(w,&bank,solid->planes,solid->count);if(status)return status;
        status=emit_bank(w,bank,input_origins[i]);if(status)return status;
        /* Opposite-facing contact is already retained by subtraction. */
        status=plane(mesh->vertices+f->first,f->count,surface);if(status)return status;
        for(q=0;q<solid->count && !kept_contact;q++) {
            double dot=0;uint32_t coplanar=1;
            for(k=0;k<3;k++)dot+=(double)surface[k]*solid->planes[q][k];
            if(dot>=0)continue;
            for(k=0;k<f->count;k++)if(fabsf(distance(solid->planes[q],mesh->vertices[f->first+k].position))>1e-5f){coplanar=0;break;}
            kept_contact=coplanar;
        }
        if(kept_contact)continue;
        if(hole) {
            n=f->count;memcpy(w->polygon[0],mesh->vertices+f->first,n*sizeof(rf_geomod_vertex));
            status=clip_negative(w,&n,solid->planes,solid->count);if(status)return status;
            status=clip_negative(w,&n,hole->planes,hole->count);if(status)return status;
            status=result(w,w->polygon[0],n,*f,input_origins[i]);if(status)return status;
        }
        /* First tetrahedron wins: disjoint union of openings inside the solid,
         * excluding the authored void and all earlier cutter tetrahedra. */
        for(q=0;q<w->tetra_count;q++) {
            n=f->count;memcpy(w->polygon[0],mesh->vertices+f->first,n*sizeof(rf_geomod_vertex));
            status=clip_negative(w,&n,solid->planes,solid->count);if(status)return status;
            status=clip_negative(w,&n,w->tetra[q],4);if(status)return status;
            bank=0;w->banks[0].nv=w->banks[0].nf=0;
            status=append(w->banks,w->polygon[0],n,*f);if(status)return status;
            if(hole){status=subtract(w,&bank,hole->planes,hole->count);if(status)return status;}
            for(k=0;k<q;k++){status=subtract(w,&bank,w->tetra[k],4);if(status)return status;}
            status=emit_bank(w,bank,input_origins[i]);if(status)return status;
        }
    }
    return publication_copy(w,mesh->generation,vertices,vc,faces,fc,origins,out);
}

static int emit_connected(rf_geomod_publication_work *w,uint32_t bank,
    rf_geomod_publication_origin origin,const connected_context *c) {
    uint32_t i,k,a;int status;
    if(!c)return emit_bank(w,bank,origin);
    for(i=0;i<w->banks[bank].nf;i++) {
        const rf_geomod_face *f=w->banks[bank].faces+i;
        rf_geomod_publication_connected_work *cw=c->work;
        rf_geomod_mesh_view mesh;uint32_t current=0;
        const rf_geomod_publication_job *owner=connected_owner(c,origin.owner);
        cw->banks[0].nv=cw->banks[0].nf=0;
        status=append(cw->banks,w->banks[bank].vertices+f->first,f->count,*f);if(status)return status;
        cw->origins[0][0]=origin;cw->origins[0][0].source_face=f->source_face;
        mesh=(rf_geomod_mesh_view){cw->banks[0].vertices,cw->banks[0].faces,f->count,1,0};
        if(origin.kind==RF_GEOMOD_PUBLICATION_NEIGHBOR && owner && owner->cut_count) {
            status=rf_geomod_publication_cut_neighbors(&mesh,cw->origins[current],origin.owner,owner->cuts,owner->cut_count,
                &cw->filter,cw->banks[1].vertices,RF_GEOMOD_PUBLICATION_VERTICES,cw->banks[1].faces,
                RF_GEOMOD_PUBLICATION_FACES,cw->origins[1],&mesh);if(status)return status;current=1;
        }
        for(k=0;k<c->current->solid_count;k++) {
            const rf_geomod_publication_solid *solid=c->current->solids+k,*hole=NULL;
            const rf_geomod_publication_job *other=connected_owner(c,solid->owner);
            uint32_t next=1-current;
            if(!other || (origin.kind==RF_GEOMOD_PUBLICATION_NEIGHBOR && solid->owner==origin.owner))continue;
            for(a=0;a<c->current->neighbor_void_count;a++)if(c->current->neighbor_voids[a].owner==solid->owner)hole=c->current->neighbor_voids+a;
            status=rf_geomod_publication_occlude_neighbor(&mesh,cw->origins[current],solid,hole,other->cuts,other->cut_count,
                &cw->filter,cw->banks[next].vertices,RF_GEOMOD_PUBLICATION_VERTICES,cw->banks[next].faces,
                RF_GEOMOD_PUBLICATION_FACES,cw->origins[next],&mesh);if(status)return status;current=next;
        }
        for(k=0;k<mesh.face_count;k++) {
            const rf_geomod_face *face=mesh.faces+k;
            status=result(w,mesh.vertices+face->first,face->count,*face,cw->origins[current][k]);if(status)return status;
        }
    }
    return RF_OK;
}
int rf_geomod_publication_build_connected(const rf_geomod_publication_job *jobs,uint32_t count,
    uint32_t generation,rf_geomod_publication_connected_work *w,rf_geomod_vertex *vertices,uint32_t vc,
    rf_geomod_face *faces,uint32_t fc,rf_geomod_publication_origin *origins,rf_geomod_mesh_view *out) {
    connected_context context={jobs,NULL,count,w};uint32_t i,j;int status;
    if(!jobs || !count || count>4 || !w || !vertices || !faces || !origins || !out)return RF_RANGE;
    for(i=0;i<count;i++) {
        if(jobs[i].crater_origin.owner==UINT32_MAX)return RF_FORMAT;
        for(j=0;j<i;j++)if(jobs[j].crater_origin.owner==jobs[i].crater_origin.owner)return RF_FORMAT;
    }
    w->publication.result.nv=w->publication.result.nf=0;
    for(i=0;i<count;i++) {
        context.current=jobs+i;
        status=publication_append(jobs+i,&w->publication,&context,0);if(status)return status;
    }
    return publication_copy(&w->publication,generation,vertices,vc,faces,fc,origins,out);
}
