#include "rf/geomod.h"
#include "rf/effect.h"
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int rf_geomod_random_basis(rf_random_state *random,float basis[9])
{
    rf_random_state next;float v[9]={0};double inverse;uint32_t i;int status;
    if(!random || !basis)return RF_RANGE;
    next=*random;status=rf_particle_cone_sample(-1,&next,v+6);if(status)return status;
    /*4fcfa0; keep the sampled forward vector, including the vertical branch. */
    if(v[6]<.0001f && v[6]>-.0001f && v[8]<.0001f && v[8]>-.0001f) {
        v[0]=1;v[6]=v[8]=0;v[7]=v[7]<0?-1.f:1.f;v[5]=-v[7];
    } else {
        v[0]=v[8];v[2]=-v[6];
        inverse=1.0/sqrt(((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2]);
        for(i=0;i<3;i++)v[i]=(float)((double)v[i]*inverse);
        for(i=0;i<3;i++)v[3+i]=(float)((double)v[6+(i+1)%3]*v[(i+2)%3]-(double)v[6+(i+2)%3]*v[(i+1)%3]);
    }
    memcpy(basis,v,sizeof(v));*random=next;return RF_OK;
}

int rf_geomod_light_visible(const rf_geomod_terrain_view *terrain,
    const float light[3],const float sample[3],uint32_t *visible)
{
    float delta[3],limit;double length=0;uint32_t i,matched;rf_collision_tree_hit hit;int status;
    const rf_collision_tree *tree;
    if(!terrain || !terrain->tree || !light || !sample || !visible)return RF_RANGE;
    tree=terrain->tree;
    for(i=0;i<3;i++) {
        if(!isfinite(light[i]) || !isfinite(sample[i]))return RF_FORMAT;
        delta[i]=sample[i]-light[i];if(!isfinite(delta[i]))return RF_FORMAT;
        length+=(double)delta[i]*delta[i];
    }
    length=sqrt(length);if(length<=.001){*visible=1;return RF_OK;}
    limit=(float)(1.0-.001/length);
    status=rf_collision_thin_tree(tree->nodes,tree->node_count,tree->faces,tree->face_count,0x100b,
        light,delta,limit,tree->stack,tree->node_capacity,&hit,&matched);
    if(status)return status;*visible=!matched;return RF_OK;
}

int rf_geomod_planar_uv(const float normal[3],const float position[3],
    uint32_t width,uint32_t height,float uv[2])
{
    static const unsigned axes[3][2]={{2,1},{0,2},{1,0}};
    uint32_t i,major,u,v;float result[2],scale_u,scale_v;
    if(!normal || !position || !uv || !width || !height || width>INT32_MAX || height>INT32_MAX)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(normal[i]) || !isfinite(position[i]))return RF_FORMAT;
    if(normal[0]==0 && normal[1]==0 && normal[2]==0)return RF_FORMAT;
    /*4fa6d0 tie rules: Z wins a tie with the X/Y winner; Y wins X/Y. */
    major=fabsf(normal[0])<=fabsf(normal[1])?1:0;
    if(fabsf(normal[2])>=fabsf(normal[major]))major=2;
    u=axes[major][normal[major]>0?0:1];v=axes[major][normal[major]>0?1:0];
    scale_u=32.f/(float)width;scale_v=32.f/(float)height;
    result[0]=scale_u*position[u];result[1]=scale_v*position[v];
    if(!isfinite(result[0]) || !isfinite(result[1]))return RF_FORMAT;
    memcpy(uv,result,sizeof(result));return RF_OK;
}

int rf_geomod_hardness(const rf_geo_region *regions,uint32_t count,uint32_t stored_default,
    const float position[3],float scale,rf_geomod_hardness_result *out)
{
    rf_geomod_hardness_result result={0,1,0,0,0};uint32_t i,j,k;int shallow=0;
    if(!position || !out || (count && !regions) || count>4096 || stored_default>100)return RF_RANGE;
    if(!isfinite(scale) || scale<0)return RF_FORMAT;
    for(j=0;j<3;j++)if(!isfinite(position[j]))return RF_FORMAT;
    result.scale=scale;
    for(i=0;i<count;i++) {
        const rf_geo_region *r=regions+i;float delta[3];int inside=1;
        uint32_t type=r->flags&7;
        if((type!=2 && type!=4) || r->hardness>100)return RF_FORMAT;
        for(j=0;j<3;j++) {
            if(!isfinite(r->position[j]))return RF_FORMAT;
            delta[j]=position[j]-r->position[j];if(!isfinite(delta[j]))return RF_FORMAT;
        }
        if(type==2) {
            double length;if(!isfinite(r->radius) || r->radius<0)return RF_FORMAT;
            length=sqrt(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
            inside=length<r->radius;
        } else {
            /*52cac0 stores serialized forward/right/up as runtime rows R/U/F. */
            static const uint32_t row[3]={3,6,0};
            for(j=0;j<9;j++)if(!isfinite(r->file_basis[j]))return RF_FORMAT;
            for(j=0;j<3;j++) {
                float local;double dot=0;
                if(!isfinite(r->dimensions[j]) || r->dimensions[j]<0)return RF_FORMAT;
                for(k=0;k<3;k++)dot+=(double)delta[k]*r->file_basis[row[j]+k];
                local=(float)dot;
                if(local<-.5f*r->dimensions[j] || local>.5f*r->dimensions[j])inside=0;
            }
        }
        if(inside) {
            if(!result.matches || r->hardness>result.hardness)result.hardness=r->hardness;
            result.matches++;if(r->flags&64)result.flags|=0x10;if(r->flags&32)shallow=1;
        }
    }
    if(shallow)return RF_NOT_FOUND;
    if(!result.matches)result.hardness=stored_default?stored_default:55;
    if(result.hardness==100)result.allowed=0;
    else {
        float factor=(float)(1.0-(double)result.hardness*(double).01f);
        if(factor<0)factor=0;if(factor>1)factor=1;
        result.scale*=factor;
    }
    *out=result;return RF_OK;
}

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
    /* A separating plane proves no intersection before other planes can
     * needlessly fragment a distant polygon. Coplanar policy still runs below. */
    for(i=0;i<plane_count;i++) {
        int separated=1,positive=0;
        for(j=0;j<count;j++) {
            uint32_t k;double d=planes[i][3];
            for(k=0;k<3;k++)d+=(double)planes[i][k]*vertices[j].position[k];
            if(d< -1e-5){separated=0;break;}
            if(d>1e-5)positive=1;
        }
        if(separated && positive) {
            if(out && (capacity<count || fragment_capacity<1))return RF_RANGE;
            if(out){memcpy(out,vertices,count*sizeof(*out));fragments[0]=(rf_geomod_fragment){0,count};}
            *vertex_count=count;*fragment_count=1;return RF_OK;
        }
    }
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
static int mesh_polygon_bounds_separated(const rf_geomod_mesh_view *mesh,const rf_geomod_vertex *v,uint32_t count)
{
    uint32_t axis,i;
    for(axis=0;axis<3;axis++) {
        float lo=v[0].position[axis],hi=lo,cut_lo=mesh->vertices[0].position[axis],cut_hi=cut_lo;
        for(i=1;i<count;i++){lo=fminf(lo,v[i].position[axis]);hi=fmaxf(hi,v[i].position[axis]);}
        for(i=1;i<mesh->vertex_count;i++){cut_lo=fminf(cut_lo,mesh->vertices[i].position[axis]);cut_hi=fmaxf(cut_hi,mesh->vertices[i].position[axis]);}
        if((double)lo-cut_hi>1e-5 || (double)cut_lo-hi>1e-5)return 1;
    }
    return 0;
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
        uint32_t part,parts=work->star_count[c]?work->star_count[c]:1;
        int policy=owner==UINT32_MAX?0:c<owner?1:2;
        if(c==owner || mesh_polygon_bounds_separated(cutters+c,vertices,count))continue;
        for(part=0;part<parts && pieces;part++) {
            uint32_t next=bank^1,total=0,next_pieces=0;
            const float (*planes)[4]=work->star_count[c]?work->star_planes[c][part]:work->cut_planes[c];
            uint32_t plane_count=work->star_count[c]?4:cutters[c].face_count;
            for(i=0;i<pieces;i++) {
                const rf_geomod_fragment *face=work->fragments[bank]+i;uint32_t n,nf;
                status=polygon_subtract_policy(work->vertices[bank]+face->first,face->count,
                    planes,plane_count,work->split.vertices,64*32,
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
    }
    for(i=0;i<pieces;i++) {
        const rf_geomod_fragment *f=work->fragments[bank]+i;
        rf_geomod_vertex *v=work->vertices[bank]+f->first;
        if(owner!=UINT32_MAX)reverse_vertices(v,f->count);
        status=rf_geomod_storage_append(s,v,f->count,material,source_face);if(status)return status;
    }
    return RF_OK;
}

static int prepare_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work,int prepared)
{
    rf_geomod_mesh_view source;uint32_t i,c,n;int status;
    if(!s || !work || s->editing || count>RF_GEOMOD_CUT_LIMIT || (count && !cutters))return RF_RANGE;
    source=(rf_geomod_mesh_view){s->vertices[2],s->faces[2],s->nv[2],s->nf[2],0};
    status=convex_mesh_planes(&source,work->source_planes);if(status)return status;
    /* Validate the entire history before creating an edit, including cutters
     * obscured by previous cuts. Failed input must not silently change history. */
    if(!prepared)for(c=0;c<count;c++){work->star_count[c]=0;status=convex_mesh_planes(cutters+c,work->cut_planes[c]);if(status)return status;}
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

static int prepare_cavity_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work,int prepared)
{
    rf_geomod_mesh_view source;uint32_t i,c,j,n,pieces;int status;
    if(!s || !work || s->editing || count>RF_GEOMOD_CUT_LIMIT || (count && !cutters))return RF_RANGE;
    source=(rf_geomod_mesh_view){s->vertices[2],s->faces[2],s->nv[2],s->nf[2],0};
    status=convex_mesh_planes_oriented(&source,work->source_planes,1);if(status)return status;
    if(!prepared)for(c=0;c<count;c++){work->star_count[c]=0;status=convex_mesh_planes(cutters+c,work->cut_planes[c]);if(status)return status;}
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

int rf_geomod_storage_prepare_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work)
{return prepare_cuts(s,cutters,count,work,0);}
int rf_geomod_storage_prepare_cavity_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work)
{return prepare_cavity_cuts(s,cutters,count,work,0);}

static int same_position(const float a[3],const float b[3])
{return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];}
/* Every triangle and the strict kernel bound one tetrahedron. The union
 * preserves the supplied concave boundary; no convex hull is substituted. */
static int star_mesh_planes(const rf_geomod_mesh_view *mesh,const float kernel[3],float out[32][4][4])
{
    uint32_t i,j,k,other,e;int status;
    if(!mesh || !mesh->faces || mesh->face_count<4 || mesh->face_count>32)return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(k=0;k<3;k++)if(!isfinite(kernel[k]))return RF_FORMAT;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *f=mesh->faces+i;
        if(f->count!=3 || f->first>mesh->vertex_count || 3>mesh->vertex_count-f->first)return RF_FORMAT;
    }
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_vertex *v=mesh->vertices+mesh->faces[i].first;
        float points[4][3],scratch[4][4];float (*planes)[4]=out?out[i]:scratch;
        for(j=0;j<3;j++)memcpy(points[j],v[j].position,12);
        memcpy(points[3],kernel,12);
        for(j=0;j<3;j++) {
            uint32_t matched=0;
            for(other=0;other<mesh->face_count;other++)if(other!=i) {
                const rf_geomod_vertex *w=mesh->vertices+mesh->faces[other].first;
                for(e=0;e<3;e++) {
                    if(same_position(v[j].position,w[(e+1)%3].position) &&
                       same_position(v[(j+1)%3].position,w[e].position))matched++;
                    if(same_position(v[j].position,w[e].position) &&
                       same_position(v[(j+1)%3].position,w[(e+1)%3].position))return RF_FORMAT;
                }
            }
            if(matched!=1)return RF_FORMAT;
        }
        for(j=0;j<4;j++) {
            static const unsigned char indices[4][4]={{0,1,2,3},{0,3,1,2},{1,3,2,0},{2,3,0,1}};
            const float *a=points[indices[j][0]],*b=points[indices[j][1]],*c=points[indices[j][2]],*opposite=points[indices[j][3]];
            double ab[3],ac[3],normal[3],length=0,d=0,distance;
            for(k=0;k<3;k++){ab[k]=(double)b[k]-a[k];ac[k]=(double)c[k]-a[k];}
            for(k=0;k<3;k++){normal[k]=ab[(k+1)%3]*ac[(k+2)%3]-ab[(k+2)%3]*ac[(k+1)%3];length+=normal[k]*normal[k];}
            if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
            length=sqrt(length);
            for(k=0;k<3;k++){planes[j][k]=(float)(normal[k]/length);d-=(double)planes[j][k]*a[k];}
            planes[j][3]=(float)d;distance=planes[j][3];
            for(k=0;k<3;k++)distance+=(double)planes[j][k]*opposite[k];
            if(!isfinite(distance) || distance>=-1e-5)return RF_FORMAT;
        }
    }
    return RF_OK;
}
int rf_geomod_storage_prepare_star_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,const float (*kernels)[3],uint32_t count,
    uint32_t cavity,rf_geomod_multi_work *work)
{
    uint32_t c;int status;
    if(!s || !work || s->editing || cavity>1 || count>RF_GEOMOD_CUT_LIMIT || (count && (!cutters || !kernels)))return RF_RANGE;
    for(c=0;c<count;c++) {
        status=star_mesh_planes(cutters+c,kernels[c],work->star_planes[c]);if(status)return status;
        work->star_count[c]=cutters[c].face_count;
    }
    return cavity?prepare_cavity_cuts(s,cutters,count,work,1):prepare_cuts(s,cutters,count,work,1);
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
    rf_geomod_vertex cut_vertices[RF_GEOMOD_CUT_LIMIT][60];
    rf_geomod_face cut_faces[RF_GEOMOD_CUT_LIMIT][20];
    rf_geomod_mesh_view cuts[RF_GEOMOD_CUT_LIMIT];
    float kernels[RF_GEOMOD_CUT_LIMIT][3];uint32_t star_mask,mapping_width,mapping_height;
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
int rf_geomod_terrain_set_mapping(rf_geomod_terrain *t,uint32_t width,uint32_t height)
{
    if(!t || t->count || !width || !height || width>INT32_MAX || height>INT32_MAX)return RF_RANGE;
    t->mapping_width=width;t->mapping_height=height;return RF_OK;
}
static int terrain_map_pending(rf_geomod_terrain *t)
{
    rf_geomod_storage *s=t->mesh;uint32_t bank=s->current^1,i,j,k;int status;
    if(!t->mapping_width)return RF_OK;
    for(i=0;i<s->nf[bank];i++) {
        const rf_geomod_face *f=s->faces[bank]+i;double normal[3]={0},length=0;float n[3];
        rf_geomod_vertex *v=s->vertices[bank]+f->first;
        if(f->source_face!=UINT32_MAX)continue;
        for(j=0;j<f->count;j++)for(k=0;k<3;k++) {
            const float *a=v[j].position,*b=v[(j+1)%f->count].position;
            normal[k]+=(double)a[(k+1)%3]*b[(k+2)%3]-(double)a[(k+2)%3]*b[(k+1)%3];
        }
        for(k=0;k<3;k++)length+=normal[k]*normal[k];
        if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
        length=sqrt(length);for(k=0;k<3;k++)n[k]=(float)(normal[k]/length);
        for(j=0;j<f->count;j++) {
            status=rf_geomod_planar_uv(n,v[j].position,t->mapping_width,t->mapping_height,v[j].uv);
            if(status)return status;
        }
    }
    return RF_OK;
}
static int terrain_publish(rf_geomod_terrain *t,uint32_t count)
{
    rf_geomod_mesh_view pending;rf_collision_tree tree={0};uint32_t bank=t->bank^1,c;int status;
    for(c=0;c<count;c++) {
        if(t->star_mask&(1u<<c)) {
            status=star_mesh_planes(t->cuts+c,t->kernels[c],t->work.star_planes[c]);
            t->work.star_count[c]=t->cuts[c].face_count;
        } else {
            status=convex_mesh_planes(t->cuts+c,t->work.cut_planes[c]);t->work.star_count[c]=0;
        }
        if(status)return status;
    }
    status=t->cavity?prepare_cavity_cuts(t->mesh,t->cuts,count,&t->work,1):
        prepare_cuts(t->mesh,t->cuts,count,&t->work,1);
    if(status)return status;
    status=terrain_map_pending(t);if(status)goto failed;
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
    slot=t->count;t->star_mask&=~(1u<<slot);
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
/* Inscribed twenty-face sphere approximation: bounded and deliberately faceted.
 * Reuses the same transactional union history as box excavation. */
int rf_geomod_terrain_cut_crater(rf_geomod_terrain *t,const float center[3],float radius,uint32_t material)
{
    static const float points[12][3]={{-1,1.618033989f,0},{1,1.618033989f,0},{-1,-1.618033989f,0},{1,-1.618033989f,0},
        {0,-1,1.618033989f},{0,1,1.618033989f},{0,-1,-1.618033989f},{0,1,-1.618033989f},
        {1.618033989f,0,-1},{1.618033989f,0,1},{-1.618033989f,0,-1},{-1.618033989f,0,1}};
    static const unsigned char triangles[20][3]={{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
    uint32_t i,j,k,slot;float scale=radius/1.902113033f;
    if(!t || !center || material==UINT32_MAX || t->count==RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
    if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    for(k=0;k<3;k++)if(!isfinite(center[k]) || !isfinite(center[k]-radius) || !isfinite(center[k]+radius))return RF_FORMAT;
    slot=t->count;t->star_mask&=~(1u<<slot);
    for(i=0;i<20;i++) {
        float normal[3],a[3],b[3];uint32_t u,v;
        for(k=0;k<3;k++){a[k]=points[triangles[i][1]][k]-points[triangles[i][0]][k];b[k]=points[triangles[i][2]][k]-points[triangles[i][0]][k];}
        for(k=0;k<3;k++)normal[k]=a[(k+1)%3]*b[(k+2)%3]-a[(k+2)%3]*b[(k+1)%3];
        k=0;if(fabsf(normal[1])>fabsf(normal[k]))k=1;if(fabsf(normal[2])>fabsf(normal[k]))k=2;u=(k+1)%3;v=(k+2)%3;
        t->cut_faces[slot][i]=(rf_geomod_face){i*3,3,material,UINT32_MAX};
        for(j=0;j<3;j++) {
            rf_geomod_vertex *p=t->cut_vertices[slot]+i*3+j;
            for(k=0;k<3;k++)p->position[k]=center[k]+points[triangles[i][j]][k]*scale;
            p->uv[0]=p->position[u]*.25f;p->uv[1]=p->position[v]*.25f;
        }
    }
    t->cuts[slot]=(rf_geomod_mesh_view){t->cut_vertices[slot],t->cut_faces[slot],60,20,0};
    return terrain_publish(t,t->count+1);
}
int rf_geomod_terrain_cut_star(rf_geomod_terrain *t,
    const rf_geomod_mesh_view *cutter,const float kernel[3])
{
    uint32_t slot,i;int status;
    if(!t || !cutter || !kernel || t->count==RF_GEOMOD_CUT_LIMIT ||
       cutter->face_count>20 || cutter->vertex_count>60)return RF_RANGE;
    slot=t->count;
    status=star_mesh_planes(cutter,kernel,t->work.star_planes[slot]);if(status)return status;
    for(i=0;i<cutter->face_count;i++)if(cutter->faces[i].material==UINT32_MAX)return RF_FORMAT;
    memcpy(t->cut_vertices[slot],cutter->vertices,cutter->vertex_count*sizeof(rf_geomod_vertex));
    memcpy(t->cut_faces[slot],cutter->faces,cutter->face_count*sizeof(rf_geomod_face));
    for(i=0;i<cutter->face_count;i++)t->cut_faces[slot][i].source_face=UINT32_MAX;
    memcpy(t->kernels[slot],kernel,12);t->star_mask|=1u<<slot;
    t->cuts[slot]=(rf_geomod_mesh_view){t->cut_vertices[slot],t->cut_faces[slot],cutter->vertex_count,cutter->face_count,0};
    return terrain_publish(t,t->count+1);
}
static uint32_t geomod_u32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float geomod_float(const unsigned char *p)
{uint32_t word=geomod_u32(p);float value;memcpy(&value,&word,4);return value;}
int rf_geomod_template_decode(const void *data,uint32_t bytes,rf_geomod_template *out)
{
    const unsigned char *p=data;rf_geomod_template value={0};rf_geomod_mesh_view mesh;
    uint32_t i,j;int status;
    if(!data || !out)return RF_RANGE;
    if(bytes<28 || memcmp(p,"RFCT",4) || geomod_u32(p+4)!=1)return RF_FORMAT;
    value.face_count=geomod_u32(p+8);value.radius=geomod_float(p+12);
    if(value.face_count<4 || value.face_count>20 || bytes!=28+value.face_count*60 ||
       !isfinite(value.radius) || value.radius<=0)return RF_FORMAT;
    for(i=0;i<3;i++)value.kernel[i]=geomod_float(p+16+i*4);
    for(i=0;i<value.face_count;i++)value.faces[i]=(rf_geomod_face){i*3,3,0,UINT32_MAX};
    for(i=0;i<value.face_count*3;i++) {
        for(j=0;j<3;j++)value.vertices[i].position[j]=geomod_float(p+28+i*20+j*4);
        for(j=0;j<2;j++)value.vertices[i].uv[j]=geomod_float(p+40+i*20+j*4);
    }
    mesh=(rf_geomod_mesh_view){value.vertices,value.faces,value.face_count*3,value.face_count,0};
    status=star_mesh_planes(&mesh,value.kernel,NULL);if(status)return status;
    *out=value;return RF_OK;
}
int rf_geomod_template_load(const char *path,rf_geomod_template *out)
{
    unsigned char data[1229];FILE *file;size_t bytes;int failed;
    if(!path || !out)return RF_RANGE;
    file=fopen(path,"rb");if(!file)return RF_IO;
    bytes=fread(data,1,sizeof(data),file);failed=ferror(file);if(fclose(file))failed=1;
    if(failed)return RF_IO;
    return rf_geomod_template_decode(data,(uint32_t)bytes,out);
}
int rf_geomod_terrain_cut_template_scale(rf_geomod_terrain *t,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float scale,uint32_t material)
{
    rf_geomod_vertex vertices[60];rf_geomod_face faces[20];rf_geomod_mesh_view mesh;
    float kernel[3];uint32_t i,j,k;
    if(!t || !shape || !center || !basis || material==UINT32_MAX || shape->face_count<4 || shape->face_count>20)return RF_RANGE;
    if(!isfinite(scale) || scale<=0 || !isfinite(shape->radius) || shape->radius<=0)return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(basis[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=0;j<3;j++) {
        double dot=0;for(k=0;k<3;k++)dot+=(double)basis[i*3+k]*basis[j*3+k];
        if(fabs(dot-(i==j?1:0))>1e-4)return RF_FORMAT;
    }
    {double determinant=(double)basis[0]*(basis[4]*basis[8]-basis[5]*basis[7])-
        (double)basis[1]*(basis[3]*basis[8]-basis[5]*basis[6])+(double)basis[2]*(basis[3]*basis[7]-basis[4]*basis[6]);
     if(determinant<.999)return RF_FORMAT;}
    for(i=0;i<shape->face_count*3;i++) {
        for(j=0;j<3;j++)vertices[i].position[j]=(float)((((double)shape->vertices[i].position[2]*basis[6+j]+
            (double)shape->vertices[i].position[1]*basis[3+j])+(double)shape->vertices[i].position[0]*basis[j])*scale+center[j]);
        memcpy(vertices[i].uv,shape->vertices[i].uv,8);
    }
    for(j=0;j<3;j++)kernel[j]=(float)((((double)shape->kernel[2]*basis[6+j]+(double)shape->kernel[1]*basis[3+j])+
        (double)shape->kernel[0]*basis[j])*scale+center[j]);
    for(i=0;i<shape->face_count;i++)faces[i]=(rf_geomod_face){i*3,3,material,UINT32_MAX};
    mesh=(rf_geomod_mesh_view){vertices,faces,shape->face_count*3,shape->face_count,0};
    return rf_geomod_terrain_cut_star(t,&mesh,kernel);
}
int rf_geomod_terrain_cut_template(rf_geomod_terrain *t,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float radius,uint32_t material)
{
    if(!shape || !isfinite(radius) || radius<=0 || !isfinite(shape->radius) || shape->radius<=0)return RF_FORMAT;
    return rf_geomod_terrain_cut_template_scale(t,shape,center,basis,radius/shape->radius,material);
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
