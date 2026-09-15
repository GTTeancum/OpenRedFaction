#include "rf/geomod.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>

static int append(rf_geomod_vertex *out,uint32_t *count,const rf_geomod_vertex *v)
{
    if(*count==RF_GEOMOD_POLYGON_LIMIT)return RF_RANGE;
    out[(*count)++]=*v;return RF_OK;
}
int rf_geomod_polygon_split(const rf_geomod_vertex *vertices,uint32_t count,
    const float plane[4],rf_geomod_vertex *front,uint32_t front_capacity,
    rf_geomod_vertex *back,uint32_t back_capacity,uint32_t *front_count,uint32_t *back_count)
{
    rf_geomod_vertex f[RF_GEOMOD_POLYGON_LIMIT],b[RF_GEOMOD_POLYGON_LIMIT];
    double distances[RF_GEOMOD_POLYGON_LIMIT],norm=0;
    int sides[RF_GEOMOD_POLYGON_LIMIT];uint32_t i,j,nf=0,nb=0,positive=0,negative=0;
    if(!vertices || !plane || !front_count || !back_count || front_count==back_count || count<3 || count>RF_GEOMOD_POLYGON_LIMIT)return RF_RANGE;
    for(j=0;j<4;j++)if(!isfinite(plane[j]))return RF_FORMAT;
    for(j=0;j<3;j++)norm+=(double)plane[j]*plane[j];
    if(fabs(norm-1)>1e-4)return RF_FORMAT;
    for(i=0;i<count;i++) {
        double d=plane[3];
        for(j=0;j<3;j++){if(!isfinite(vertices[i].position[j]))return RF_FORMAT;d+=(double)plane[j]*vertices[i].position[j];}
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_FORMAT;
        distances[i]=d;sides[i]=d>1e-5?1:d< -1e-5?-1:0;
        positive+=sides[i]>0;negative+=sides[i]<0;
    }
    if(!negative){memcpy(f,vertices,count*sizeof(*f));nf=count;}
    else if(!positive){memcpy(b,vertices,count*sizeof(*b));nb=count;}
    else for(i=0;i<count;i++) {
        uint32_t next=(i+1)%count;rf_geomod_vertex cut;double t;
        if(sides[i]>=0 && append(f,&nf,vertices+i))return RF_RANGE;
        if(sides[i]<=0 && append(b,&nb,vertices+i))return RF_RANGE;
        if(sides[i]*sides[next]>=0)continue;
        t=distances[i]/(distances[i]-distances[next]);
        for(j=0;j<3;j++) {
            cut.position[j]=(float)((1-t)*vertices[i].position[j]+t*vertices[next].position[j]);
            if(!isfinite(cut.position[j]))return RF_FORMAT;
        }
        for(j=0;j<2;j++) {
            cut.uv[j]=(float)((1-t)*vertices[i].uv[j]+t*vertices[next].uv[j]);
            if(!isfinite(cut.uv[j]))return RF_FORMAT;
        }
        if(append(f,&nf,&cut) || append(b,&nb,&cut))return RF_RANGE;
    }
    if((front && front_capacity<nf) || (back && back_capacity<nb))return RF_RANGE;
    if(front && nf)memcpy(front,f,nf*sizeof(*front));
    if(back && nb)memcpy(back,b,nb*sizeof(*back));
    *front_count=nf;*back_count=nb;return RF_OK;
}

static int polygon_subtract_policy(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count,int boundary_policy)
{
    rf_geomod_vertex current[64],front[64],back[64];
    double normal[3]={0},normal_length=0;
    uint32_t pass,i,j,left,nf,nb,total=0,pieces=0,required=0,required_pieces=0;int status;
    if(!vertices || count<3 || count>64 || !planes || !plane_count || plane_count>32 ||
       !vertex_count || !fragment_count || vertex_count==fragment_count || (!!out != !!fragments))return RF_RANGE;
    /* Validate even planes beyond an early empty intersection. */
    for(i=0;i<plane_count;i++) {
        double norm=0;
        for(j=0;j<4;j++)if(!isfinite(planes[i][j]))return RF_FORMAT;
        for(j=0;j<3;j++)norm+=(double)planes[i][j]*planes[i][j];
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
    }
    for(i=0;i<count;i++) {
        uint32_t next=(i+1)%count;
        for(j=0;j<3;j++) {
            uint32_t a=(j+1)%3,b=(j+2)%3;
            if(!isfinite(vertices[i].position[j]))return RF_FORMAT;
            normal[j]+=(double)vertices[i].position[a]*vertices[next].position[b]-(double)vertices[i].position[b]*vertices[next].position[a];
        }
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_FORMAT;
    }
    for(j=0;j<3;j++)normal_length+=normal[j]*normal[j];
    if(!isfinite(normal_length) || normal_length<=1e-24)return RF_FORMAT;
    for(pass=0;pass<(out?2u:1u);pass++) {
        memcpy(current,vertices,count*sizeof(*current));left=count;total=pieces=0;
        for(i=0;i<plane_count && left;i++) {
            status=rf_geomod_polygon_split(current,left,planes[i],front,64,back,64,&nf,&nb);
            if(status)return status;
            if(nf && !nb) {
                uint32_t k;int coplanar=1;double alignment=0;
                for(j=0;j<left;j++) {
                    double d=planes[i][3];
                    for(k=0;k<3;k++)d+=(double)planes[i][k]*current[j].position[k];
                    if(fabs(d)>1e-5){coplanar=0;break;}
                }
                for(k=0;k<3;k++)alignment+=normal[k]*planes[i][k];
                /* Source surfaces keep opposite-facing contact. Union caps
                 * remove internal contact and assign coincident outer caps
                 * to one owner: policy1 removes both, policy2 keeps same-facing. */
                if(coplanar && (boundary_policy==1 || (boundary_policy==2?alignment<0:alignment>0))) {
                    nf=0;nb=left;memcpy(back,current,left*sizeof(*back));
                }
            }
            if(nf) {
                if(pass) {
                    memcpy(out+total,front,nf*sizeof(*out));
                    fragments[pieces].first=total;fragments[pieces].count=nf;
                }
                total+=nf;pieces++;
            }
            memcpy(current,back,nb*sizeof(*current));left=nb;
        }
        if(!pass) {
            required=total;required_pieces=pieces;
            if(out && (capacity<total || fragment_capacity<pieces))return RF_RANGE;
        }
    }
    *vertex_count=required;*fragment_count=required_pieces;return RF_OK;
}

int rf_geomod_polygon_subtract(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count)
{
    return polygon_subtract_policy(vertices,count,planes,plane_count,out,capacity,
        fragments,fragment_capacity,vertex_count,fragment_count,0);
}

int rf_geomod_interior_face(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,
    uint32_t capacity,uint32_t *out_count)
{
    rf_geomod_vertex current[64],front[64],back[64];uint32_t i,j,k,left=count,nf,nb;int status;
    if(!vertices || count<3 || count>64 || !planes || !plane_count || plane_count>32 || !out_count)return RF_RANGE;
    for(i=0;i<plane_count;i++) {
        double norm=0;
        for(j=0;j<4;j++)if(!isfinite(planes[i][j]))return RF_FORMAT;
        for(j=0;j<3;j++)norm+=(double)planes[i][j]*planes[i][j];
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
    }
    for(i=0;i<count;i++) {
        for(j=0;j<3;j++)if(!isfinite(vertices[i].position[j]))return RF_FORMAT;
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_FORMAT;
    }
    memcpy(current,vertices,count*sizeof(*current));
    for(i=0;i<plane_count && left;i++) {
        int on_boundary=1;
        for(j=0;j<left;j++) {
            double d=planes[i][3];
            for(k=0;k<3;k++)d+=(double)planes[i][k]*current[j].position[k];
            if(fabs(d)>1e-5){on_boundary=0;break;}
        }
        if(on_boundary){left=0;break;}
        status=rf_geomod_polygon_split(current,left,planes[i],front,64,back,64,&nf,&nb);
        if(status)return status;
        left=nb;memcpy(current,back,nb*sizeof(*current));
    }
    if(out && capacity<left)return RF_RANGE;
    if(out)for(i=0;i<left;i++)out[i]=current[left-1-i];
    *out_count=left;return RF_OK;
}

struct rf_geomod_storage {
    rf_geomod_vertex *vertices[3];rf_geomod_face *faces[3];
    uint32_t nv[3],nf[3],vertex_capacity,face_capacity,bytes,current,editing,generation;
};
static int storage_vertices(const rf_geomod_vertex *v,uint32_t n)
{
    uint32_t i,j;if(n && !v)return RF_RANGE;
    for(i=0;i<n;i++) {
        for(j=0;j<3;j++)if(!isfinite(v[i].position[j]))return RF_FORMAT;
        for(j=0;j<2;j++)if(!isfinite(v[i].uv[j]))return RF_FORMAT;
    }
    return RF_OK;
}
int rf_geomod_storage_open(const rf_geomod_mesh_view *source,uint32_t vc,uint32_t fc,
    uint32_t budget,rf_geomod_storage **out)
{
    rf_geomod_storage *s;unsigned char *p;uint64_t bytes;uint32_t i;int status;
    if(!source || !out || *out || !vc || !fc || source->vertex_count>vc || source->face_count>fc ||
       (source->face_count && !source->faces))return RF_RANGE;
    status=storage_vertices(source->vertices,source->vertex_count);if(status)return status;
    for(i=0;i<source->face_count;i++) {
        const rf_geomod_face *f=source->faces+i;
        if(f->count<3 || f->count>64 || f->first>source->vertex_count || f->count>source->vertex_count-f->first)return RF_FORMAT;
    }
    bytes=sizeof(*s)+(2ull*vc+source->vertex_count)*sizeof(rf_geomod_vertex)+(2ull*fc+source->face_count)*sizeof(rf_geomod_face);
    if(bytes>budget || bytes>UINT32_MAX)return RF_RANGE;
    s=calloc(1,(size_t)bytes);if(!s)return RF_IO;
    s->bytes=(uint32_t)bytes;s->vertex_capacity=vc;s->face_capacity=fc;s->generation=1;p=(unsigned char *)(s+1);
    for(i=0;i<3;i++) {
        s->vertices[i]=(rf_geomod_vertex *)p;p+=(i==2?source->vertex_count:vc)*sizeof(rf_geomod_vertex);
        s->faces[i]=(rf_geomod_face *)p;p+=(i==2?source->face_count:fc)*sizeof(rf_geomod_face);
        if(i!=1) {
            if(source->vertex_count)memcpy(s->vertices[i],source->vertices,source->vertex_count*sizeof(rf_geomod_vertex));
            if(source->face_count)memcpy(s->faces[i],source->faces,source->face_count*sizeof(rf_geomod_face));
            s->nv[i]=source->vertex_count;s->nf[i]=source->face_count;
        }
    }
    *out=s;return RF_OK;
}
void rf_geomod_storage_close(rf_geomod_storage **s)
{if(s){free(*s);*s=NULL;}}
uint32_t rf_geomod_storage_bytes(const rf_geomod_storage *s)
{return s?s->bytes:0;}
int rf_geomod_storage_view(const rf_geomod_storage *s,rf_geomod_mesh_view *out)
{
    if(!s || !out)return RF_RANGE;
    out->vertices=s->vertices[s->current];out->faces=s->faces[s->current];
    out->vertex_count=s->nv[s->current];out->face_count=s->nf[s->current];out->generation=s->generation;return RF_OK;
}
int rf_geomod_storage_begin(rf_geomod_storage *s)
{
    uint32_t next;if(!s || s->editing || s->generation==UINT32_MAX)return RF_RANGE;
    next=s->current^1;s->nv[next]=s->nf[next]=0;s->editing=1;return RF_OK;
}
int rf_geomod_storage_pending(const rf_geomod_storage *s,rf_geomod_mesh_view *out)
{
    uint32_t next;if(!s || !s->editing || !out)return RF_RANGE;next=s->current^1;
    out->vertices=s->vertices[next];out->faces=s->faces[next];out->vertex_count=s->nv[next];
    out->face_count=s->nf[next];out->generation=s->generation+1;return RF_OK;
}
int rf_geomod_storage_append(rf_geomod_storage *s,const rf_geomod_vertex *v,uint32_t n,uint32_t material,uint32_t source_face)
{
    uint32_t next;rf_geomod_face face;int status;
    if(!s || !s->editing || n<3 || n>64)return RF_RANGE;
    next=s->current^1;
    if(n>s->vertex_capacity-s->nv[next] || s->nf[next]==s->face_capacity)return RF_RANGE;
    status=storage_vertices(v,n);if(status)return status;
    face.first=s->nv[next];face.count=n;face.material=material;face.source_face=source_face;
    memcpy(s->vertices[next]+face.first,v,n*sizeof(*v));s->faces[next][s->nf[next]++]=face;s->nv[next]+=n;return RF_OK;
}
int rf_geomod_storage_commit(rf_geomod_storage *s)
{
    if(!s || !s->editing || s->generation==UINT32_MAX)return RF_RANGE;
    s->current^=1;s->generation++;s->editing=0;return RF_OK;
}
void rf_geomod_storage_abort(rf_geomod_storage *s)
{if(s)s->editing=0;}
int rf_geomod_storage_reset(rf_geomod_storage *s)
{
    uint32_t next;if(!s || s->editing || s->generation==UINT32_MAX)return RF_RANGE;
    next=s->current^1;
    if(s->nv[2])memcpy(s->vertices[next],s->vertices[2],s->nv[2]*sizeof(rf_geomod_vertex));
    if(s->nf[2])memcpy(s->faces[next],s->faces[2],s->nf[2]*sizeof(rf_geomod_face));
    s->nv[next]=s->nv[2];s->nf[next]=s->nf[2];s->current=next;s->generation++;return RF_OK;
}

static int convex_mesh_planes_oriented(const rf_geomod_mesh_view *mesh,float planes[32][4],int inward)
{
    uint32_t i,j,k;int status;
    if(!mesh || mesh->face_count<4 || mesh->face_count>32 || !mesh->faces)return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *face=mesh->faces+i;double normal[3]={0},length=0,offset=0;
        if(face->count<3 || face->count>64 || face->first>mesh->vertex_count || face->count>mesh->vertex_count-face->first)return RF_FORMAT;
        for(j=0;j<face->count;j++) {
            const float *a=mesh->vertices[face->first+j].position,*b=mesh->vertices[face->first+(j+1)%face->count].position;
            for(k=0;k<3;k++)normal[k]+=(double)a[(k+1)%3]*b[(k+2)%3]-(double)a[(k+2)%3]*b[(k+1)%3];
        }
        for(k=0;k<3;k++)length+=normal[k]*normal[k];
        if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
        length=sqrt(length);
        for(k=0;k<3;k++){planes[i][k]=(float)(normal[k]/length)*(inward?-1.f:1.f);offset-=(double)planes[i][k]*mesh->vertices[face->first].position[k];}
        planes[i][3]=(float)offset;if(!isfinite(planes[i][3]))return RF_FORMAT;
        for(j=0;j<mesh->vertex_count;j++) {
            double distance=planes[i][3];for(k=0;k<3;k++)distance+=(double)planes[i][k]*mesh->vertices[j].position[k];
            if(distance>1e-5 || (j>=face->first && j<face->first+face->count && fabs(distance)>1e-5))return RF_FORMAT;
        }
    }
    return RF_OK;
}
static int convex_mesh_planes(const rf_geomod_mesh_view *mesh,float planes[32][4])
{return convex_mesh_planes_oriented(mesh,planes,0);}
int rf_geomod_storage_prepare_convex_cut(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutter,rf_geomod_cut_work *work)
{
    rf_geomod_mesh_view source;float source_planes[32][4],cut_planes[32][4];uint32_t i,j,n,pieces;int status;
    if(!s || !cutter || !work || s->editing)return RF_RANGE;
    rf_geomod_storage_view(s,&source);
    status=convex_mesh_planes(&source,source_planes);if(status)return status;
    status=convex_mesh_planes(cutter,cut_planes);if(status)return status;
    status=rf_geomod_storage_begin(s);if(status)return status;
    for(i=0;i<source.face_count;i++) {
        const rf_geomod_face *face=source.faces+i;
        status=rf_geomod_polygon_subtract(source.vertices+face->first,face->count,cut_planes,cutter->face_count,
            work->vertices,64*32,work->fragments,32,&n,&pieces);if(status)goto failed;
        for(j=0;j<pieces;j++) {
            const rf_geomod_fragment *f=work->fragments+j;
            status=rf_geomod_storage_append(s,work->vertices+f->first,f->count,face->material,face->source_face);if(status)goto failed;
        }
    }
    for(i=0;i<cutter->face_count;i++) {
        const rf_geomod_face *face=cutter->faces+i;
        status=rf_geomod_interior_face(cutter->vertices+face->first,face->count,source_planes,source.face_count,work->vertices,64*32,&n);if(status)goto failed;
        if(n){status=rf_geomod_storage_append(s,work->vertices,n,face->material,UINT32_MAX);if(status)goto failed;}
    }
    return RF_OK;
failed:
    rf_geomod_storage_abort(s);return status;
}

static void reverse_vertices(rf_geomod_vertex *vertices,uint32_t count)
{
    uint32_t i;for(i=0;i<count/2;i++) {
        rf_geomod_vertex v=vertices[i];vertices[i]=vertices[count-1-i];vertices[count-1-i]=v;
    }
}
/* Process one outward face through the cutter union. owner==UINT32_MAX is
 * original terrain; otherwise this is an outward cutter face, reversed only
 * after all exclusions, so the boundary policy sees the cutter's true normal. */
static int subtract_history_face(rf_geomod_storage *s,const rf_geomod_vertex *vertices,
    uint32_t count,uint32_t material,uint32_t source_face,uint32_t owner,
    const rf_geomod_mesh_view *cutters,uint32_t cutter_count,rf_geomod_multi_work *work)
{
    uint32_t bank=0,pieces=1,c,i,j;int status;
    memcpy(work->vertices[0],vertices,count*sizeof(*vertices));
    work->fragments[0][0]=(rf_geomod_fragment){0,count};
    for(c=0;c<cutter_count && pieces;c++) {
        uint32_t next=bank^1,total=0,next_pieces=0;
        int policy=owner==UINT32_MAX?0:c<owner?1:2;
        if(c==owner)continue;
        for(i=0;i<pieces;i++) {
            const rf_geomod_fragment *face=work->fragments[bank]+i;uint32_t n,nf;
            status=polygon_subtract_policy(work->vertices[bank]+face->first,face->count,
                work->cut_planes[c],cutters[c].face_count,work->split.vertices,64*32,
                work->split.fragments,32,&n,&nf,policy);if(status)return status;
            if(n>RF_GEOMOD_WORK_VERTICES-total || nf>RF_GEOMOD_WORK_FRAGMENTS-next_pieces)return RF_RANGE;
            memcpy(work->vertices[next]+total,work->split.vertices,n*sizeof(*vertices));
            for(j=0;j<nf;j++) {
                rf_geomod_fragment f=work->split.fragments[j];f.first+=total;
                work->fragments[next][next_pieces++]=f;
            }
            total+=n;
        }
        bank=next;pieces=next_pieces;
    }
    for(i=0;i<pieces;i++) {
        const rf_geomod_fragment *f=work->fragments[bank]+i;
        rf_geomod_vertex *v=work->vertices[bank]+f->first;
        if(owner!=UINT32_MAX)reverse_vertices(v,f->count);
        status=rf_geomod_storage_append(s,v,f->count,material,source_face);if(status)return status;
    }
    return RF_OK;
}

int rf_geomod_storage_prepare_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work)
{
    rf_geomod_mesh_view source;uint32_t i,c,n;int status;
    if(!s || !work || s->editing || count>RF_GEOMOD_CUT_LIMIT || (count && !cutters))return RF_RANGE;
    source=(rf_geomod_mesh_view){s->vertices[2],s->faces[2],s->nv[2],s->nf[2],0};
    status=convex_mesh_planes(&source,work->source_planes);if(status)return status;
    /* Validate the entire history before creating an edit, including cutters
     * obscured by previous cuts. Failed input must not silently change history. */
    for(c=0;c<count;c++){status=convex_mesh_planes(cutters+c,work->cut_planes[c]);if(status)return status;}
    status=rf_geomod_storage_begin(s);if(status)return status;
    for(i=0;i<source.face_count;i++) {
        const rf_geomod_face *f=source.faces+i;
        status=subtract_history_face(s,source.vertices+f->first,f->count,f->material,
            f->source_face,UINT32_MAX,cutters,count,work);if(status)goto failed;
    }
    for(c=0;c<count;c++)for(i=0;i<cutters[c].face_count;i++) {
        const rf_geomod_face *f=cutters[c].faces+i;
        status=rf_geomod_interior_face(cutters[c].vertices+f->first,f->count,
            work->source_planes,source.face_count,work->split.vertices,64*32,&n);if(status)goto failed;
        if(!n)continue;
        reverse_vertices(work->split.vertices,n);
        status=subtract_history_face(s,work->split.vertices,n,f->material,UINT32_MAX,
            c,cutters,count,work);if(status)goto failed;
    }
    return RF_OK;
failed:
    rf_geomod_storage_abort(s);return status;
}

int rf_geomod_storage_prepare_cavity_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work)
{
    rf_geomod_mesh_view source;uint32_t i,c,j,n,pieces;int status;
    if(!s || !work || s->editing || count>RF_GEOMOD_CUT_LIMIT || (count && !cutters))return RF_RANGE;
    source=(rf_geomod_mesh_view){s->vertices[2],s->faces[2],s->nv[2],s->nf[2],0};
    status=convex_mesh_planes_oriented(&source,work->source_planes,1);if(status)return status;
    for(c=0;c<count;c++){status=convex_mesh_planes(cutters+c,work->cut_planes[c]);if(status)return status;}
    status=rf_geomod_storage_begin(s);if(status)return status;
    for(i=0;i<source.face_count;i++) {
        const rf_geomod_face *f=source.faces+i;
        status=subtract_history_face(s,source.vertices+f->first,f->count,f->material,
            f->source_face,UINT32_MAX,cutters,count,work);if(status)goto failed;
    }
    for(c=0;c<count;c++)for(i=0;i<cutters[c].face_count;i++) {
        const rf_geomod_face *f=cutters[c].faces+i;
        /* Keep cutter boundaries outside the original empty room. Contact
         * between cavity and cutter is internal, for either plane orientation. */
        status=polygon_subtract_policy(cutters[c].vertices+f->first,f->count,
            work->source_planes,source.face_count,work->seed.vertices,64*32,
            work->seed.fragments,32,&n,&pieces,1);if(status)goto failed;
        for(j=0;j<pieces;j++) {
            const rf_geomod_fragment *part=work->seed.fragments+j;
            status=subtract_history_face(s,work->seed.vertices+part->first,part->count,
                f->material,UINT32_MAX,c,cutters,count,work);if(status)goto failed;
        }
    }
    return RF_OK;
failed:
    rf_geomod_storage_abort(s);return status;
}

static int collision_mesh_face(const rf_geomod_mesh_view *mesh,uint32_t index,
    const rf_collision_face_filter *filter,rf_collision_face *out)
{
    const rf_geomod_face *f=mesh->faces+index;rf_collision_face value={0};
    double normal[3]={0},length=0;uint32_t i,j,accepted;int status;
    if(f->count<3 || f->count>64 || f->first>mesh->vertex_count || f->count>mesh->vertex_count-f->first)return RF_FORMAT;
    status=rf_collision_face_accept(filter,&accepted);if(status)return status;
    for(i=0;i<f->count;i++) {
        const float *a=mesh->vertices[f->first+i].position,*b=mesh->vertices[f->first+(i+1)%f->count].position;
        for(j=0;j<3;j++) {
            normal[j]+=(double)a[(j+1)%3]*b[(j+2)%3]-(double)a[(j+2)%3]*b[(j+1)%3];
            if(!i || a[j]<value.minimum[j])value.minimum[j]=a[j];
            if(!i || a[j]>value.maximum[j])value.maximum[j]=a[j];
        }
    }
    for(j=0;j<3;j++)length+=normal[j]*normal[j];
    if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
    length=sqrt(length);
    for(j=0;j<3;j++) {
        value.plane[j]=(float)(normal[j]/length);
        value.plane[3]-=value.plane[j]*mesh->vertices[f->first].position[j];
        /* Same bound expansion as rf_geometry_collision_face /4e002b. */
        value.minimum[j]-=.0001f;value.maximum[j]+=.0001f;
        if(!isfinite(value.minimum[j]) || !isfinite(value.maximum[j]))return RF_FORMAT;
    }
    if(!isfinite(value.plane[3]))return RF_FORMAT;
    for(i=0;i<f->count;i++) {
        const float *a=mesh->vertices[f->first+i].position,*b=mesh->vertices[f->first+(i+1)%f->count].position;
        double distance=value.plane[3],edge[3],size=0;uint32_t k;
        for(j=0;j<3;j++){distance+=(double)value.plane[j]*a[j];edge[j]=(double)b[j]-a[j];size+=edge[j]*edge[j];}
        if(fabs(distance)>1e-5 || size<=1e-24)return RF_FORMAT;
        /* Every vertex must lie on the inward side of each directed edge. */
        for(k=0;k<f->count;k++) {
            const float *p=mesh->vertices[f->first+k].position;double inward=0;
            for(j=0;j<3;j++)inward+=value.plane[j]*(edge[(j+1)%3]*(p[(j+2)%3]-a[(j+2)%3])-edge[(j+2)%3]*(p[(j+1)%3]-a[(j+1)%3]));
            if(inward< -1e-5*sqrt(size))return RF_FORMAT;
        }
    }
    value.count=f->count;value.filter=*filter;*out=value;return RF_OK;
}
int rf_geomod_collision_faces(const rf_geomod_mesh_view *mesh,
    const rf_collision_face_filter *filters,float (*positions)[3],uint32_t vc,
    rf_collision_face *faces,uint32_t fc)
{
    uint32_t i;rf_collision_face face;int status;
    if(!mesh || vc<mesh->vertex_count || fc<mesh->face_count ||
       (mesh->vertex_count && !positions) || (mesh->face_count && (!mesh->faces || !filters || !faces)))return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(i=0;i<mesh->face_count;i++){status=collision_mesh_face(mesh,i,filters+i,&face);if(status)return status;}
    for(i=0;i<mesh->vertex_count;i++)memcpy(positions[i],mesh->vertices[i].position,12);
    for(i=0;i<mesh->face_count;i++) {
        collision_mesh_face(mesh,i,filters+i,&face);face.vertices=positions+mesh->faces[i].first;faces[i]=face;
    }
    return RF_OK;
}

struct rf_geomod_terrain {
    rf_geomod_storage *mesh;rf_geomod_multi_work work;
    rf_geomod_vertex cut_vertices[RF_GEOMOD_CUT_LIMIT][24];
    rf_geomod_face cut_faces[RF_GEOMOD_CUT_LIMIT][6];
    rf_geomod_mesh_view cuts[RF_GEOMOD_CUT_LIMIT];
    rf_collision_face_filter original_filters[32],generated_filter,*filters;
    rf_collision_face *faces[2];float (*positions[2])[3];rf_collision_tree tree;
    uint32_t bank,count,cavity,vc,fc,base_bytes,budget,peak_bytes;
};
static int terrain_bind(rf_geomod_terrain *t,const rf_geomod_mesh_view *mesh,uint32_t bank,
    rf_collision_tree *tree)
{
    uint32_t i,j,used;int status;
    for(i=0;i<mesh->face_count;i++) {
        uint32_t id=mesh->faces[i].source_face;
        t->filters[i]=t->generated_filter;
        if(id!=UINT32_MAX) {
            for(j=0;j<t->mesh->nf[2];j++)if(t->mesh->faces[2][j].source_face==id)break;
            if(j==t->mesh->nf[2])return RF_FORMAT;
            t->filters[i]=t->original_filters[j];
        }
    }
    status=rf_geomod_collision_faces(mesh,t->filters,t->positions[bank],t->vc,t->faces[bank],t->fc);if(status)return status;
    used=t->base_bytes+t->tree.allocated_bytes;
    if(used>t->budget)return RF_RANGE;
    status=rf_collision_tree_open(t->faces[bank],mesh->face_count,t->budget-used,tree);if(status)return status;
    if(used+tree->peak_bytes>t->peak_bytes)t->peak_bytes=used+tree->peak_bytes;
    return RF_OK;
}
void rf_geomod_terrain_close(rf_geomod_terrain **terrain)
{
    if(terrain && *terrain) {
        rf_geomod_terrain *t=*terrain;
        rf_collision_tree_close(&t->tree);rf_geomod_storage_close(&t->mesh);free(t);*terrain=NULL;
    }
}
int rf_geomod_terrain_open(const rf_geomod_mesh_view *source,
    const rf_collision_face_filter *filters,const rf_collision_face_filter *generated_filter,
    uint32_t cavity,uint32_t vc,uint32_t fc,uint32_t budget,rf_geomod_terrain **out)
{
    rf_geomod_terrain *t;rf_geomod_mesh_view mesh;rf_collision_tree tree={0};
    uint64_t bytes=sizeof(*t)+(uint64_t)vc*24+(uint64_t)fc*(2*sizeof(rf_collision_face)+sizeof(rf_collision_face_filter));
    unsigned char *p;uint32_t i,j,accepted;int status;
    if(!source || !filters || !generated_filter || !out || *out || cavity>1 || source->face_count>32 ||
       !source->faces || !vc || !fc || source->face_count>fc || source->vertex_count>vc || bytes>budget)return RF_RANGE;
    status=rf_collision_face_accept(generated_filter,&accepted);if(status)return status;
    for(i=0;i<source->face_count;i++) {
        if(source->faces[i].source_face==UINT32_MAX)return RF_FORMAT;
        for(j=0;j<i;j++)if(source->faces[i].source_face==source->faces[j].source_face)return RF_FORMAT;
        status=rf_collision_face_accept(filters+i,&accepted);if(status)return status;
    }
    t=calloc(1,(size_t)bytes);if(!t)return RF_IO;
    t->vc=vc;t->fc=fc;t->budget=budget;t->base_bytes=(uint32_t)bytes;t->cavity=cavity;
    memcpy(t->original_filters,filters,source->face_count*sizeof(*filters));t->generated_filter=*generated_filter;
    p=(unsigned char *)(t+1);
    for(i=0;i<2;i++) {
        t->positions[i]=(float(*)[3])p;p+=(size_t)vc*12;
        /* Position bytes are multiples of12. Align native pointer-bearing
         * face arrays by placing both position banks before them below. */
    }
    /* calloc base/owner alignment plus24*vc preserves pointer alignment. */
    for(i=0;i<2;i++){t->faces[i]=(rf_collision_face *)p;p+=(size_t)fc*sizeof(rf_collision_face);}
    t->filters=(rf_collision_face_filter *)p;
    status=rf_geomod_storage_open(source,vc,fc,budget-t->base_bytes,&t->mesh);if(status)goto failed;
    t->base_bytes+=rf_geomod_storage_bytes(t->mesh);
    status=convex_mesh_planes_oriented(source,t->work.source_planes,cavity);if(status)goto failed;
    rf_geomod_storage_view(t->mesh,&mesh);
    status=terrain_bind(t,&mesh,0,&tree);if(status)goto failed;
    t->tree=tree;*out=t;return RF_OK;
failed:
    rf_collision_tree_close(&tree);rf_geomod_terrain_close(&t);return status;
}
static int terrain_publish(rf_geomod_terrain *t,uint32_t count)
{
    rf_geomod_mesh_view pending;rf_collision_tree tree={0};uint32_t bank=t->bank^1;int status;
    status=t->cavity?rf_geomod_storage_prepare_cavity_cuts(t->mesh,t->cuts,count,&t->work):
        rf_geomod_storage_prepare_cuts(t->mesh,t->cuts,count,&t->work);
    if(status)return status;
    status=rf_geomod_storage_pending(t->mesh,&pending);if(status)goto failed;
    status=terrain_bind(t,&pending,bank,&tree);if(status)goto failed;
    status=rf_geomod_storage_commit(t->mesh);if(status)goto failed;
    rf_collision_tree_close(&t->tree);t->tree=tree;t->bank=bank;t->count=count;return RF_OK;
failed:
    rf_collision_tree_close(&tree);rf_geomod_storage_abort(t->mesh);return status;
}
int rf_geomod_terrain_cut_box(rf_geomod_terrain *t,const float center[3],const float extent[3],uint32_t material)
{
    float lo[3],hi[3];uint32_t axis,side,j,slot;
    const int u[4]={-1,1,1,-1},v[4]={-1,-1,1,1};
    if(!t || !center || !extent || material==UINT32_MAX || t->count==RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
    for(j=0;j<3;j++) {
        if(!isfinite(center[j]) || !isfinite(extent[j]) || extent[j]<=0)return RF_FORMAT;
        lo[j]=center[j]-extent[j];hi[j]=center[j]+extent[j];
        if(!isfinite(lo[j]) || !isfinite(hi[j]) || lo[j]>=hi[j])return RF_FORMAT;
    }
    slot=t->count;
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
        uint32_t face=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        t->cut_faces[slot][face]=(rf_geomod_face){face*4,4,material,UINT32_MAX};
        for(j=0;j<4;j++) {
            uint32_t k=side?j:3-j;rf_geomod_vertex *p=t->cut_vertices[slot]+face*4+j;
            p->position[axis]=side?hi[axis]:lo[axis];
            p->position[a]=u[k]>0?hi[a]:lo[a];p->position[b]=v[k]>0?hi[b]:lo[b];
            /* Stable world-scale planar UVs, provisional excavation material mapping. */
            p->uv[0]=p->position[a];p->uv[1]=p->position[b];
        }
    }
    t->cuts[slot]=(rf_geomod_mesh_view){t->cut_vertices[slot],t->cut_faces[slot],24,6,0};
    return terrain_publish(t,t->count+1);
}
int rf_geomod_terrain_reset(rf_geomod_terrain *t)
{return t?terrain_publish(t,0):RF_RANGE;}
int rf_geomod_terrain_get(const rf_geomod_terrain *t,rf_geomod_terrain_view *out)
{
    rf_geomod_terrain_view value;if(!t || !out)return RF_RANGE;
    rf_geomod_storage_view(t->mesh,&value.mesh);value.faces=t->faces[t->bank];value.tree=&t->tree;
    value.cuts=t->count;value.resident_bytes=t->base_bytes+t->tree.allocated_bytes;value.peak_bytes=t->peak_bytes;
    *out=value;return RF_OK;
}
