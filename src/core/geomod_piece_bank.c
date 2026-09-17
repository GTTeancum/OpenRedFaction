#include "rf/geomod_piece_bank.h"
#include "rf/checkpoint_placement.h"
#include "rf/entity.h"
#include "rf/geomod_notify.h"
#include <float.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
struct rf_geomod_piece_bank {
    rf_geomod_owned_piece *pieces;rf_geomod_vertex *vertices;rf_geomod_face *faces;
    uint32_t *old_faces;rf_collision_face_filter *filters;
    rf_collision_face *collision;float (*positions)[3];
    uint32_t vc,fc,pc,nv,nf,np,bytes;
};
int rf_geomod_piece_bank_open(uint32_t vc,uint32_t fc,uint32_t pc,uint32_t budget,rf_geomod_piece_bank **out)
{
    uint64_t bytes;rf_geomod_piece_bank *bank;unsigned char *p;
    if(!out || *out || !vc || !fc || !pc)return RF_RANGE;
    bytes=sizeof(*bank)+(uint64_t)pc*sizeof(rf_geomod_owned_piece)+(uint64_t)vc*(sizeof(rf_geomod_vertex)+sizeof(*bank->positions))+
        (uint64_t)fc*(sizeof(rf_geomod_face)+sizeof(uint32_t)+sizeof(rf_collision_face_filter)+sizeof(rf_collision_face));
    if(bytes>budget || bytes>UINT32_MAX)return RF_RANGE;
    bank=calloc(1,(size_t)bytes);if(!bank)return RF_IO;
    p=(unsigned char *)(bank+1);bank->pieces=(rf_geomod_owned_piece *)p;p+=pc*sizeof(*bank->pieces);
    /* Pointer-bearing collision records precede float/word arrays so native
     * alignment is retained on both32-bit Xbox and64-bit PC. */
    bank->collision=(rf_collision_face *)p;p+=fc*sizeof(*bank->collision);
    bank->positions=(float (*)[3])p;p+=vc*sizeof(*bank->positions);
    bank->vertices=(rf_geomod_vertex *)p;p+=vc*sizeof(*bank->vertices);
    bank->faces=(rf_geomod_face *)p;p+=fc*sizeof(*bank->faces);
    bank->old_faces=(uint32_t *)p;p+=fc*sizeof(*bank->old_faces);
    bank->filters=(rf_collision_face_filter *)p;
    bank->vc=vc;bank->fc=fc;bank->pc=pc;bank->bytes=(uint32_t)bytes;*out=bank;return RF_OK;
}
void rf_geomod_piece_bank_close(rf_geomod_piece_bank **bank)
{if(bank){free(*bank);*bank=NULL;}}
uint32_t rf_geomod_piece_bank_count(const rf_geomod_piece_bank *bank)
{return bank?bank->np:0;}
uint32_t rf_geomod_piece_bank_bytes(const rf_geomod_piece_bank *bank)
{return bank?bank->bytes:0;}
int rf_geomod_piece_bank_get(const rf_geomod_piece_bank *bank,uint32_t index,rf_geomod_owned_piece *piece)
{
    if(!bank || !piece || index>=bank->np)return RF_RANGE;
    *piece=bank->pieces[index];return RF_OK;
}
/* A float center translation can bend an almost-collinear polygon past the
 * convex-face gate. Preserve its exact boundary by partitioning only rejected
 * polygons into center-fan triangles; never weld or move the boundary corners. */
static int rebind_translated_piece(rf_geomod_piece_bank *bank,rf_geomod_owned_piece *piece)
{
    uint8_t split[RF_GEOMOD_WORK_FACES]={0};uint32_t i,j,k,nv=0,nf=0,total_faces;int status;
    rf_geomod_vertex *vertices=bank->vertices+bank->nv;rf_geomod_face *faces=bank->faces+bank->nf;
    rf_collision_face_filter *filters=bank->filters+bank->nf;uint32_t *old=bank->old_faces+bank->nf;
    if(piece->mesh.face_count>RF_GEOMOD_WORK_FACES)return RF_RANGE;
    for(i=0;i<piece->mesh.face_count;i++) {
        rf_geomod_face f=faces[i];rf_collision_face bound;rf_geomod_mesh_view one={vertices+f.first,&f,f.count,1,0};f.first=0;
        status=rf_geomod_collision_faces(&one,filters+i,bank->positions+bank->nv,f.count,&bound,1);
        if(status && (status!=RF_FORMAT || f.count<4))return status;
        split[i]=status!=0;nv+=split[i]?3*f.count:f.count;nf+=split[i]?f.count:1;
    }
    if(nv>bank->vc-bank->nv || nf>bank->fc-bank->nf)return RF_RANGE;
    piece->mesh.vertex_count=nv;total_faces=nf;
    for(i=piece->mesh.face_count;i-->0;) {
        rf_geomod_face f=faces[i];rf_collision_face_filter filter=filters[i];uint32_t old_face=old[i];
        if(split[i]) {
            rf_geomod_vertex original[64],center={0};double sum[5]={0};
            memcpy(original,vertices+f.first,f.count*sizeof(*original));
            for(j=0;j<f.count;j++){for(k=0;k<3;k++)sum[k]+=original[j].position[k];for(k=0;k<2;k++)sum[k+3]+=original[j].uv[k];}
            for(k=0;k<3;k++)center.position[k]=(float)(sum[k]/f.count);for(k=0;k<2;k++)center.uv[k]=(float)(sum[k+3]/f.count);
            nv-=3*f.count;nf-=f.count;
            for(j=0;j<f.count;j++) {
                uint32_t at=nv+3*j;vertices[at]=center;vertices[at+1]=original[j];vertices[at+2]=original[(j+1)%f.count];
                faces[nf+j]=(rf_geomod_face){at,3,f.material,f.source_face};filters[nf+j]=filter;old[nf+j]=old_face;
            }
        } else {
            nv-=f.count;nf--;memmove(vertices+nv,vertices+f.first,f.count*sizeof(*vertices));f.first=nv;faces[nf]=f;filters[nf]=filter;old[nf]=old_face;
        }
    }
    piece->mesh.face_count=total_faces;
    return rf_geomod_collision_faces(&piece->mesh,filters,bank->positions+bank->nv,piece->mesh.vertex_count,
        bank->collision+bank->nf,piece->mesh.face_count);
}
static int append_piece(rf_geomod_piece_bank *bank,const rf_geomod_mesh_view *mesh,
    const uint32_t *old_faces,const rf_collision_face_filter *filters,uint32_t source_count,uint32_t id,const float *density)
{
    rf_geomod_owned_piece piece;uint32_t i,j,packed=0;int status;
    if(!bank || !mesh || !mesh->vertices || !mesh->faces || !old_faces || !filters || !mesh->vertex_count || !mesh->face_count)return RF_RANGE;
    if(bank->np==bank->pc || mesh->vertex_count>bank->vc-bank->nv || mesh->face_count>bank->fc-bank->nf)return RF_RANGE;
    for(i=0;i<bank->np;i++)if(bank->pieces[i].id==id)return RF_FORMAT;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *face=mesh->faces+i;
        if(face->first!=packed || face->count<3 || face->count>mesh->vertex_count-packed || old_faces[i]>=source_count)return RF_FORMAT;
        packed+=face->count;
    }
    if(packed!=mesh->vertex_count)return RF_FORMAT;
    for(i=0;i<mesh->vertex_count;i++)for(j=0;j<2;j++)if(!isfinite(mesh->vertices[i].uv[j]))return RF_FORMAT;
    memset(&piece,0,sizeof(piece));
    status=rf_geomod_mesh_recenter(mesh->vertices,mesh->vertex_count,bank->vertices+bank->nv,&piece.placement);if(status)return status;
    memcpy(bank->faces+bank->nf,mesh->faces,mesh->face_count*sizeof(*mesh->faces));
    memcpy(bank->old_faces+bank->nf,old_faces,mesh->face_count*sizeof(*old_faces));
    for(i=0;i<mesh->face_count;i++)bank->filters[bank->nf+i]=filters[old_faces[i]];
    piece.mesh=(rf_geomod_mesh_view){bank->vertices+bank->nv,bank->faces+bank->nf,mesh->vertex_count,mesh->face_count,mesh->generation};
    piece.old_faces=bank->old_faces+bank->nf;piece.filters=bank->filters+bank->nf;piece.id=id;
    status=rf_geomod_collision_faces(&piece.mesh,piece.filters,bank->positions+bank->nv,mesh->vertex_count,
        bank->collision+bank->nf,mesh->face_count);if(status)return status;
    piece.collision=bank->collision+bank->nf;
    piece.birth_radius=piece.placement.radius;
    if(density) {
        status=rf_physics_solid_mass_prepare(piece.collision,mesh->face_count,piece.placement.minimum,
            piece.placement.maximum,*density,&piece.mass);if(status)return status;
        for(i=0;i<mesh->vertex_count;i++)for(j=0;j<3;j++)
            bank->vertices[bank->nv+i].position[j]=(float)((double)bank->vertices[bank->nv+i].position[j]-piece.mass.center[j]);
        for(j=0;j<3;j++) {
            piece.placement.origin[j]=(float)((double)piece.placement.origin[j]+piece.mass.center[j]);
            piece.placement.minimum[j]=(float)((double)piece.placement.minimum[j]-piece.mass.center[j]);
            piece.placement.maximum[j]=(float)((double)piece.placement.maximum[j]-piece.mass.center[j]);
            if(!isfinite(piece.placement.origin[j]))return RF_RANGE;
        }
        piece.placement.radius=(float)((double)piece.placement.radius+sqrt(
            ((double)piece.mass.center[0]*piece.mass.center[0]+(double)piece.mass.center[1]*piece.mass.center[1])+
            (double)piece.mass.center[2]*piece.mass.center[2]));
        if(!isfinite(piece.placement.radius))return RF_RANGE;
        status=rf_geomod_collision_faces(&piece.mesh,piece.filters,bank->positions+bank->nv,mesh->vertex_count,
            bank->collision+bank->nf,mesh->face_count);
        if(status==RF_FORMAT)status=rebind_translated_piece(bank,&piece);
        if(status)return status;
        piece.mass_ready=1;
    }
    bank->pieces[bank->np++]=piece;bank->nv+=piece.mesh.vertex_count;bank->nf+=piece.mesh.face_count;
    return RF_OK;
}

int rf_geomod_piece_bank_append(rf_geomod_piece_bank *bank,const rf_geomod_mesh_view *mesh,
    const uint32_t *old_faces,const rf_collision_face_filter *filters,uint32_t source_count,uint32_t id)
{return append_piece(bank,mesh,old_faces,filters,source_count,id,NULL);}
int rf_geomod_piece_bank_append_physical(rf_geomod_piece_bank *bank,const rf_geomod_mesh_view *mesh,
    const uint32_t *old_faces,const rf_collision_face_filter *filters,uint32_t source_count,uint32_t id,float density)
{return append_piece(bank,mesh,old_faces,filters,source_count,id,&density);}
/* Port policy for an empty original occupancy grid: sample the actual closed
 * mesh, never a bounding-sphere substitute. Signed solid angle admits interior
 * samples of concave pieces; distance to all surface triangles bounds radius. */
static double piece_triangle_distance(const float p[3],const float a[3],const float b[3],const float c[3])
{
    double ab[3],ac[3],ap[3],normal[3],length=0,dot=0,best=DBL_MAX;uint32_t k,e;
    for(k=0;k<3;k++){ab[k]=(double)b[k]-a[k];ac[k]=(double)c[k]-a[k];ap[k]=(double)p[k]-a[k];}
    for(k=0;k<3;k++){normal[k]=ab[(k+1)%3]*ac[(k+2)%3]-ab[(k+2)%3]*ac[(k+1)%3];length+=normal[k]*normal[k];dot+=normal[k]*ap[k];}
    if(length>0) {
        double point[3],u=0,v=0,aa=0,bb=0,cc=0,den;
        for(k=0;k<3;k++){point[k]=ap[k]-normal[k]*dot/length;aa+=ab[k]*ab[k];bb+=ab[k]*ac[k];cc+=ac[k]*ac[k];u+=point[k]*ab[k];v+=point[k]*ac[k];}
        den=aa*cc-bb*bb;
        if(den>0){double s=(u*cc-v*bb)/den,t=(v*aa-u*bb)/den;if(s>=0 && t>=0 && s+t<=1)best=dot*dot/length;}
    }
    for(e=0;e<3;e++) {
        const float *x=e==0?a:e==1?b:c,*y=e==0?b:e==1?c:a;double n=0,t=0,error=0;
        for(k=0;k<3;k++){double d=(double)y[k]-x[k];n+=d*d;t+=((double)p[k]-x[k])*d;}
        t=n?t/n:0;if(t<0)t=0;if(t>1)t=1;
        for(k=0;k<3;k++){double d=(double)p[k]-x[k]-t*((double)y[k]-x[k]);error+=d*d;}
        if(error<best)best=error;
    }
    return best;
}
/* A convex face's centroid is on its boundary. Follow its inward normal to
 * the nearest other surface and sample the midpoint of that interior interval.
 * The caller still validates winding and nearest-surface radius. */
static int piece_face_seed(const rf_geomod_mesh_view *mesh,uint32_t index,float point[3])
{
    const rf_geomod_face *face=mesh->faces+index;double center[3]={0},direction[3]={0},nearest=DBL_MAX;
    uint32_t i,j,k;
    for(i=0;i<face->count;i++) {
        const float *a=mesh->vertices[face->first+i].position,*b=mesh->vertices[face->first+(i+1)%face->count].position;
        for(k=0;k<3;k++){center[k]+=(double)a[k]/face->count;direction[k]-=((double)a[(k+1)%3]-b[(k+1)%3])*((double)a[(k+2)%3]+b[(k+2)%3]);}
    }
    for(i=0;i<mesh->face_count;i++)if(i!=index) {
        const rf_geomod_face *other=mesh->faces+i;const float *a=mesh->vertices[other->first].position;
        for(j=1;j+1<other->count;j++) {
            const float *b=mesh->vertices[other->first+j].position,*c=mesh->vertices[other->first+j+1].position;
            double e[3],g[3],normal[3],den=0,num=0,aa=0,bb=0,cc=0,u=0,v=0,t,d;
            for(k=0;k<3;k++){e[k]=(double)b[k]-a[k];g[k]=(double)c[k]-a[k];}
            for(k=0;k<3;k++){normal[k]=e[(k+1)%3]*g[(k+2)%3]-e[(k+2)%3]*g[(k+1)%3];den+=normal[k]*direction[k];num+=normal[k]*((double)a[k]-center[k]);}
            if(den==0)continue;t=num/den;if(t<=0 || t>=nearest)continue;
            for(k=0;k<3;k++){double p=center[k]+direction[k]*t-a[k];aa+=e[k]*e[k];bb+=e[k]*g[k];cc+=g[k]*g[k];u+=p*e[k];v+=p*g[k];}
            d=aa*cc-bb*bb;if(d<=0)continue;
            {double s=(u*cc-v*bb)/d,q=(v*aa-u*bb)/d;if(s<0 || q<0 || s+q>1)continue;}
            nearest=t;
        }
    }
    if(nearest==DBL_MAX)return 0;
    for(k=0;k<3;k++)point[k]=(float)(center[k]+direction[k]*nearest*.5);
    return 1;
}
static int piece_empty_grid_spheres(const rf_geomod_owned_piece *piece,rf_physics_sphere spheres[64],uint32_t *count)
{
    uint32_t pass,sample,k,f,j,n=0;float low[3]={FLT_MAX,FLT_MAX,FLT_MAX},high[3]={-FLT_MAX,-FLT_MAX,-FLT_MAX};
    if(!piece->mesh.vertices || !piece->mesh.faces || !piece->mesh.vertex_count || !piece->mesh.face_count)return RF_RANGE;
    for(f=0;f<piece->mesh.face_count;f++) {
        const rf_geomod_face *face=piece->mesh.faces+f;
        if(face->count<3 || face->first>piece->mesh.vertex_count || face->count>piece->mesh.vertex_count-face->first)return RF_RANGE;
    }
    for(j=0;j<piece->mesh.vertex_count;j++)for(k=0;k<3;k++) {
        float v=piece->mesh.vertices[j].position[k];if(!isfinite(v))return RF_FORMAT;if(v<low[k])low[k]=v;if(v>high[k])high[k]=v;
    }
    for(pass=0;pass<2 && !n;pass++)for(sample=0;sample<(pass?piece->mesh.face_count:64) && n<64;sample++) {
        uint32_t index[3]={sample>>4,(sample>>2)&3,sample&3};float p[3];double angle=0,nearest=DBL_MAX;
        if(pass){if(!piece_face_seed(&piece->mesh,sample,p))continue;}
        else for(k=0;k<3;k++)p[k]=(float)((double)low[k]+((double)index[k]+.5)*((double)high[k]-low[k])*.25);
        for(f=0;f<piece->mesh.face_count;f++) {
            const rf_geomod_face *face=piece->mesh.faces+f;
            const float *a=piece->mesh.vertices[face->first].position;
            for(j=1;j+1<face->count;j++) {
                const float *b=piece->mesh.vertices[face->first+j].position,*c=piece->mesh.vertices[face->first+j+1].position;
                double u[3],v[3],w[3],lu=0,lv=0,lw=0,uv=0,vw=0,wu=0,det=0,distance;
                for(k=0;k<3;k++){u[k]=(double)a[k]-p[k];v[k]=(double)b[k]-p[k];w[k]=(double)c[k]-p[k];lu+=u[k]*u[k];lv+=v[k]*v[k];lw+=w[k]*w[k];uv+=u[k]*v[k];vw+=v[k]*w[k];wu+=w[k]*u[k];}
                for(k=0;k<3;k++)det+=u[k]*(v[(k+1)%3]*w[(k+2)%3]-v[(k+2)%3]*w[(k+1)%3]);
                lu=sqrt(lu);lv=sqrt(lv);lw=sqrt(lw);angle+=2*atan2(det,lu*lv*lw+uv*lw+vw*lu+wu*lv);
                distance=piece_triangle_distance(p,a,b,c);if(distance<nearest)nearest=distance;
            }
        }
        if(fabs(angle)>6.283185307179586 && nearest>0 && isfinite(nearest)) {
            float radius=(float)sqrt(nearest);uint32_t bits;
            /* Positive IEEE binary32 predecessor; NXDK nextafterf is a stub. */
            if(!isfinite(radius))return RF_RANGE;
            memcpy(&bits,&radius,4);if(bits)--bits;memcpy(&radius,&bits,4);
            if(radius>0){memset(spheres+n,0,sizeof(*spheres));memcpy(spheres[n].center,p,12);spheres[n].radius=radius;spheres[n].parameter_10=-1;n++;}
        }
    }
    if(!n)return RF_NOT_FOUND;*count=n;return RF_OK;
}
int rf_geomod_piece_body_open(const rf_geomod_owned_piece *piece,float elasticity,float friction,
    uint32_t budget,rf_physics_body *body)
{
    rf_physics_body_parameters p={0};rf_physics_sphere spheres[64];uint32_t count;float radius;int status;
    if(!piece || !piece->mass_ready || !body)return RF_RANGE;
    status=rf_physics_grid_spheres(piece->mass.cells,piece->mass.spacing,piece->mass.origin,spheres,64,&count,&radius);
    if(status)return status;
    if(!count){status=piece_empty_grid_spheres(piece,spheres,&count);if(status)return status;}
    p.coefficients[0]=elasticity;p.coefficients[1]=(float)((double)piece->birth_radius*(double).2f);p.coefficients[2]=friction;
    p.mass=piece->mass.mass;p.flags=0x8000003f;
    memcpy(p.local_tensor,piece->mass.inverse_tensor,36);memcpy(p.position,piece->placement.origin,12);
    p.orientation[0]=p.orientation[4]=p.orientation[8]=1;
    return rf_physics_body_open(&p,spheres,count,budget,body);
}

struct rf_geomod_piece_batch {
    rf_geomod_piece_bank *geometry;rf_physics_body *bodies;rf_geomod_piece_life *life;float *birth_health;
    uint32_t count,bytes,peak_bytes;
};
void rf_geomod_piece_batch_close(rf_geomod_piece_batch **owner)
{
    uint32_t i;rf_geomod_piece_batch *batch;
    if(!owner || !*owner)return;batch=*owner;
    for(i=0;i<batch->count;i++)rf_physics_body_close(batch->bodies+i);
    rf_geomod_piece_bank_close(&batch->geometry);free(batch);*owner=NULL;
}
uint32_t rf_geomod_piece_batch_count(const rf_geomod_piece_batch *batch)
{return batch?batch->count:0;}
uint32_t rf_geomod_piece_batch_alive(const rf_geomod_piece_batch *batch,uint32_t index)
{return batch && index<batch->count && !(batch->life[index].flags&2);}
uint32_t rf_geomod_piece_batch_bytes(const rf_geomod_piece_batch *batch)
{return batch?batch->bytes:0;}
uint32_t rf_geomod_piece_batch_peak_bytes(const rf_geomod_piece_batch *batch)
{return batch?batch->peak_bytes:0;}
int rf_geomod_piece_batch_get(rf_geomod_piece_batch *batch,uint32_t index,
    rf_geomod_owned_piece *piece,rf_physics_body **body)
{
    rf_geomod_owned_piece value;int status;
    if(!batch || !piece || !body || index>=batch->count)return RF_RANGE;
    if(!batch->geometry || !batch->bodies[index].allocated_bytes)return RF_NOT_FOUND;
    status=rf_geomod_piece_bank_get(batch->geometry,index,&value);if(status)return status;
    *piece=value;*body=batch->bodies+index;return RF_OK;
}
int rf_geomod_piece_batch_open(const rf_geomod_mesh_view *source,const rf_collision_face_filter *filters,
    const rf_collision_face_filter *generated,uint32_t material,float density,float elasticity,float friction,
    rf_random_state *random,uint32_t budget,rf_geomod_piece_batch **out)
{
    rf_geomod_piece_bank *geometry=NULL;rf_geomod_piece_batch *batch=NULL;
    rf_geomod_subdivision_stats stats;rf_random_state next;uint32_t count,i;uint64_t owner_bytes,resident;int status;
    if(!out || *out || !random || !isfinite(elasticity) || !isfinite(friction))return RF_RANGE;
    next=*random;
    status=rf_geomod_piece_subdivide(source,filters,generated,material,density,&next,budget,&geometry,&stats);if(status)return status;
    count=rf_geomod_piece_bank_count(geometry);owner_bytes=sizeof(*batch)+(uint64_t)count*(sizeof(rf_physics_body)+sizeof(rf_geomod_piece_life)+sizeof(float));
    resident=owner_bytes+rf_geomod_piece_bank_bytes(geometry);
    if(resident>budget){status=RF_RANGE;goto failed;}
    batch=calloc(1,(size_t)owner_bytes);if(!batch){status=RF_IO;goto failed;}
    batch->geometry=geometry;geometry=NULL;batch->bodies=(rf_physics_body *)(batch+1);batch->life=(rf_geomod_piece_life *)(batch->bodies+count);
    batch->birth_health=(float *)(batch->life+count);
    batch->bytes=(uint32_t)resident;batch->peak_bytes=stats.peak_bytes;
    for(i=0;i<count;i++) {
        rf_geomod_owned_piece piece;
        status=rf_geomod_piece_bank_get(batch->geometry,i,&piece);if(status)goto failed;
        status=rf_geomod_piece_life_init(piece.birth_radius,batch->life+i);if(status)goto failed;
        batch->birth_health[i]=batch->life[i].health;
        /* The body record is already in the fixed owner allocation. Only its
         * separately allocated spheres increase the concurrent resident bytes. */
        status=rf_geomod_piece_body_open(&piece,elasticity,friction,budget-batch->bytes+sizeof(rf_physics_body),batch->bodies+i);
        if(status)goto failed;
        batch->count++;batch->bytes+=batch->bodies[i].allocated_bytes-sizeof(rf_physics_body);
    }
    if(batch->bytes>batch->peak_bytes)batch->peak_bytes=batch->bytes;
    *random=next;*out=batch;return RF_OK;
failed:
    rf_geomod_piece_bank_close(&geometry);rf_geomod_piece_batch_close(&batch);return status;
}

static int piece_batch_sweep_except(const rf_geomod_piece_batch *batch,uint32_t excluded,float minimum_body_radius,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_piece_hit *result,uint32_t *matched)
{
    rf_geomod_piece_hit best={0};uint32_t found=0,i,k;float nearest=limit;int status;
    if(!batch || !start || !delta || !result || !matched || !isfinite(radius) || radius<0 ||
        !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(start[k]) || !isfinite(delta[k]))return RF_RANGE;
    for(i=0;i<batch->count;i++) {
        rf_geomod_owned_piece piece;rf_collision_sweep_tree_hit local;uint32_t hit;
        const rf_physics_body_state *body=&batch->bodies[i].state;
        if(i==excluded || !rf_geomod_piece_batch_alive(batch,i) || !(body->bounds.radius>minimum_body_radius))continue;
        status=rf_geomod_piece_bank_get(batch->geometry,i,&piece);if(status)return status;
        status=rf_collision_flat_faces(piece.collision,piece.mesh.face_count,flags&~4u,start,delta,
            body->position,(const float (*)[3])body->orientation,radius,nearest,&local,&hit);
        if(status)return status;
        if(!hit || (found && local.hit.fraction>=nearest))continue;
        status=rf_collision_contact_world(&local.hit,body->position,(const float (*)[3])body->orientation,&best.hit);
        if(status)return status;
        best.piece=i;best.face=local.face_index;best.edge=local.edge;
        nearest=local.hit.fraction;found=1;
    }
    if(found)*result=best;
    *matched=found;return RF_OK;
}

int rf_geomod_piece_batch_sweep(const rf_geomod_piece_batch *batch,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_piece_hit *result,uint32_t *matched)
{return piece_batch_sweep_except(batch,UINT32_MAX,-1,flags,start,delta,radius,limit,result,matched);}

typedef struct piece_registry_entry {
    rf_geomod_piece_batch *batch;uint32_t prefix,ordinal,before,after;
    rf_geomod_changed_box changed_box;
} piece_registry_entry;
struct rf_geomod_piece_registry {
    piece_registry_entry active[16],pending[16];uint32_t count,staged,begun,replace;
    uint32_t seed,budget,bytes,material,last_prefix,last_ordinal;rf_random_state random;
    float density,elasticity,friction;rf_collision_face_filter generated;
};
int rf_geomod_piece_registry_open(const rf_collision_face_filter *generated,uint32_t material,
    float density,float elasticity,float friction,uint32_t seed,uint32_t budget,rf_geomod_piece_registry **out)
{
    rf_geomod_piece_registry *r;
    if(!out || *out || !generated || budget<sizeof(*r) || material==UINT32_MAX ||
       !isfinite(density) || density<0 || !isfinite(elasticity) || !isfinite(friction))return RF_RANGE;
    r=calloc(1,sizeof(*r));if(!r)return RF_IO;
    r->generated=*generated;r->material=material;r->density=density;r->elasticity=elasticity;r->friction=friction;
    r->seed=seed;r->budget=budget;r->bytes=sizeof(*r);*out=r;return RF_OK;
}
void rf_geomod_piece_registry_abort(rf_geomod_piece_registry *r)
{
    uint32_t i;if(!r)return;
    for(i=0;i<r->staged;i++){r->bytes-=rf_geomod_piece_batch_bytes(r->pending[i].batch);rf_geomod_piece_batch_close(&r->pending[i].batch);}
    memset(r->pending,0,sizeof(r->pending));r->staged=r->begun=r->replace=0;
}
void rf_geomod_piece_registry_close(rf_geomod_piece_registry **owner)
{
    rf_geomod_piece_registry *r;uint32_t i;if(!owner || !*owner)return;r=*owner;
    rf_geomod_piece_registry_abort(r);
    for(i=0;i<r->count;i++)rf_geomod_piece_batch_close(&r->active[i].batch);
    free(r);*owner=NULL;
}
int rf_geomod_piece_registry_rewind(rf_geomod_piece_registry *r)
{if(!r || !r->begun)return RF_RANGE;r->random.value=r->seed;r->last_prefix=r->last_ordinal=0;return RF_OK;}
int rf_geomod_piece_registry_begin(rf_geomod_piece_registry *r,uint32_t replace)
{
    if(!r || r->begun || replace>1)return RF_RANGE;
    r->begun=1;r->replace=replace;return rf_geomod_piece_registry_rewind(r);
}
int rf_geomod_piece_registry_emit(const rf_geomod_mesh_view *mesh,const uint32_t *map,
    const rf_collision_face_filter *filters,uint32_t source_count,uint32_t prefix,uint32_t ordinal,void *opaque)
{
    rf_geomod_piece_registry *r=opaque;piece_registry_entry *entry;
    rf_collision_face_filter mapped[32];uint32_t i,pass;int status;
    if(!r || !mesh || !map || !filters || !prefix)return RF_RANGE;
    /* Read-only history validation can revisit already committed pieces without
     * opening an edit or touching body/RNG state. New identities still reject. */
    if(!r->begun) {
        for(i=0;i<r->count;i++)if(r->active[i].prefix==prefix && r->active[i].ordinal==ordinal)return RF_OK;
        return RF_NOT_FOUND;
    }
    if(prefix<r->last_prefix || (prefix==r->last_prefix && ordinal<=r->last_ordinal))return RF_RANGE;
    for(pass=0;pass<2;pass++) {
        piece_registry_entry *entries=pass?r->pending:r->active;
        uint32_t count=pass?r->staged:(r->replace?0:r->count);
        for(i=0;i<count;i++)if(entries[i].prefix==prefix && entries[i].ordinal==ordinal) {
            if(entries[i].before!=r->random.value)return RF_FORMAT;
            r->random.value=entries[i].after;r->last_prefix=prefix;r->last_ordinal=ordinal;return RF_OK;
        }
    }
    if(mesh->face_count>32 || r->staged+(r->replace?0:r->count)>=16)return RF_RANGE;
    for(i=0;i<mesh->face_count;i++){if(map[i]>=source_count)return RF_FORMAT;mapped[i]=filters[map[i]];}
    entry=r->pending+r->staged;entry->before=r->random.value;
    {
        rf_geomod_vertex local[128];rf_geomod_piece_placement placement;
        rf_geomod_changed_box bounds,boxes[32];uint32_t count=0;
        if(mesh->vertex_count>128)return RF_RANGE;
        status=rf_geomod_mesh_recenter(mesh->vertices,mesh->vertex_count,local,&placement);if(status)return status;
        memcpy(bounds.minimum,placement.minimum,12);memcpy(bounds.maximum,placement.maximum,12);
        status=rf_geomod_notify_append_fragment_box(&bounds,placement.origin,boxes,&count);if(status)return status;
        entry->changed_box=boxes[0];
    }
    status=rf_geomod_piece_batch_open(mesh,mapped,&r->generated,r->material,r->density,r->elasticity,r->friction,
        &r->random,r->budget-r->bytes,&entry->batch);if(status)return status;
    entry->prefix=prefix;entry->ordinal=ordinal;entry->after=r->random.value;
    r->bytes+=rf_geomod_piece_batch_bytes(entry->batch);r->staged++;r->last_prefix=prefix;r->last_ordinal=ordinal;return RF_OK;
}
void rf_geomod_piece_registry_commit(rf_geomod_piece_registry *r)
{
    uint32_t i;if(!r || !r->begun)return;
    if(r->replace) {
        for(i=0;i<r->count;i++){r->bytes-=rf_geomod_piece_batch_bytes(r->active[i].batch);rf_geomod_piece_batch_close(&r->active[i].batch);}
        memset(r->active,0,sizeof(r->active));r->count=0;
    }
    memcpy(r->active+r->count,r->pending,r->staged*sizeof(*r->pending));r->count+=r->staged;
    memset(r->pending,0,sizeof(r->pending));r->staged=r->begun=r->replace=0;
}
uint32_t rf_geomod_piece_registry_count(const rf_geomod_piece_registry *r){return r?r->count:0;}
int rf_geomod_piece_registry_changed_boxes(const rf_geomod_piece_registry *r,uint32_t first,
    rf_geomod_changed_box boxes[32],uint32_t *count)
{
    uint32_t i,n=r?r->count:0;
    if(!boxes || !count || first>n || (r && r->begun))return RF_RANGE;
    for(i=first;i<n;i++)boxes[i-first]=r->active[i].changed_box;
    *count=n-first;return RF_OK;
}
uint32_t rf_geomod_piece_registry_bytes(const rf_geomod_piece_registry *r){return r?r->bytes:0;}
int rf_geomod_piece_registry_get(rf_geomod_piece_registry *r,uint32_t i,rf_geomod_piece_batch **out)
{if(!r || !out || i>=r->count)return RF_RANGE;*out=r->active[i].batch;return RF_OK;}

#define PIECE_STATE_HEADER 16u
#define PIECE_STATE_RECORD 328u
static uint32_t piece_state_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static void piece_state_store(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
/* Explicit fields, independent of host struct padding and pointer width. */
static void piece_state_codec(rf_physics_body_state *s,unsigned char *p,uint32_t read)
{
    float *fields[]={s->coefficients,&s->mass,s->local_tensor,s->world_tensor,s->position,s->next_position,
        s->orientation,s->next_orientation,s->velocity,s->vector_c8,s->mass_vector_d4,s->vector_e0,s->vector_ec,
        &s->bounds.radius,s->bounds.minimum,s->bounds.maximum,s->vector_138,&s->scalar_144};
    const uint32_t counts[]={3,1,9,9,3,3,9,9,3,3,3,3,3,1,3,3,3,1};uint32_t i,j,w;
    uint32_t words[5];
    for(i=0;i<18;i++)for(j=0;j<counts[i];j++,p+=4) {
        if(read){w=piece_state_word(p);memcpy(fields[i]+j,&w,4);}
        else {memcpy(&w,fields[i]+j,4);piece_state_store(p,w);}
    }
    if(read) {
        for(i=0;i<5;i++)words[i]=piece_state_word(p+4*i);
        s->flags=words[0];s->state_124=words[1];memcpy(&s->reference_15c,words+2,4);
        s->word_164=words[3];s->word_168=words[4];
    } else {
        words[0]=s->flags;words[1]=s->state_124;memcpy(words+2,&s->reference_15c,4);
        words[3]=s->word_164;words[4]=s->word_168;
        for(i=0;i<5;i++)piece_state_store(p+4*i,words[i]);
    }
}
static int piece_state_valid(const rf_physics_body_state *s,const rf_physics_body_state *birth)
{
    unsigned char encoded[308];rf_physics_body_state copy=*s;uint32_t i,j,k,w;float f,tensor[9];
    piece_state_codec(&copy,encoded,0);
    for(i=0;i<72;i++){w=piece_state_word(encoded+4*i);memcpy(&f,&w,4);if(!isfinite(f))return RF_FORMAT;}
    if(s->mass<=0 || s->bounds.radius<0 || s->coefficients[1]<0 || s->coefficients[2]<0 ||
       s->scalar_144<0 || s->scalar_144>1 || s->mass!=birth->mass || memcmp(s->local_tensor,birth->local_tensor,36) ||
       s->bounds.radius!=birth->bounds.radius || s->coefficients[1]!=birth->coefficients[1] ||
       s->coefficients[2]!=birth->coefficients[2] || s->coefficients[0]<0 || (s->flags&0x40000100u) ||
       (s->flags&0x4000u) || s->reference_15c!=birth->reference_15c || s->word_168!=birth->word_168)return RF_FORMAT;
    if(rf_physics_tensor_world(s->local_tensor,s->orientation,tensor))return RF_FORMAT;
    for(i=0;i<9;i++)if(tensor[i]!=s->world_tensor[i])return RF_FORMAT;
    for(i=0;i<3;i++)if(s->bounds.minimum[i]>s->bounds.maximum[i])return RF_FORMAT;
    for(k=0;k<2;k++) {
        const float *m=k?s->next_orientation:s->orientation;
        for(i=0;i<3;i++)for(j=0;j<3;j++) {
            double dot=(double)m[3*i]*m[3*j]+(double)m[3*i+1]*m[3*j+1]+(double)m[3*i+2]*m[3*j+2];
            if(fabs(dot-(i==j?1:0))>.01)return RF_FORMAT;
        }
        if((double)m[0]*(m[4]*m[8]-m[5]*m[7])-(double)m[1]*(m[3]*m[8]-m[5]*m[6])+
           (double)m[2]*(m[3]*m[7]-m[4]*m[6])<.99)return RF_FORMAT;
    }
    return RF_OK;
}
int rf_geomod_piece_registry_state_size(const rf_geomod_piece_registry *r,uint32_t *bytes)
{
    uint64_t n=PIECE_STATE_HEADER;uint32_t i;
    if(!r || !bytes || r->begun)return RF_RANGE;
    for(i=0;i<r->count;i++)n+=(uint64_t)r->active[i].batch->count*PIECE_STATE_RECORD;
    if(n>UINT32_MAX)return RF_RANGE;*bytes=(uint32_t)n;return RF_OK;
}
int rf_geomod_piece_registry_state_encode(const rf_geomod_piece_registry *r,void *output,uint32_t bytes)
{
    unsigned char *p=output;uint32_t size,i,j;int status=rf_geomod_piece_registry_state_size(r,&size);
    if(status)return status;if(!output || bytes!=size)return RF_RANGE;
    for(i=0;i<r->count;i++)for(j=0;j<r->active[i].batch->count;j++) {
        const rf_physics_body_state *s=&r->active[i].batch->bodies[j].state;
        status=piece_state_valid(s,s);if(status)return status;
    }
    memcpy(p,"RFPB",4);piece_state_store(p+4,2);piece_state_store(p+8,size);
    piece_state_store(p+12,(size-PIECE_STATE_HEADER)/PIECE_STATE_RECORD);p+=PIECE_STATE_HEADER;
    for(i=0;i<r->count;i++)for(j=0;j<r->active[i].batch->count;j++,p+=PIECE_STATE_RECORD) {
        rf_physics_body_state s=r->active[i].batch->bodies[j].state;
        piece_state_store(p,r->active[i].prefix);piece_state_store(p+4,r->active[i].ordinal);piece_state_store(p+8,j);
        piece_state_codec(&s,p+12,0);
        {uint32_t health;memcpy(&health,&r->active[i].batch->life[j].health,4);piece_state_store(p+320,health);piece_state_store(p+324,r->active[i].batch->life[j].flags);}
    }
    return RF_OK;
}
int rf_geomod_piece_registry_state_decode(rf_geomod_piece_registry *r,const void *input,uint32_t bytes)
{
    const unsigned char *start=input,*p;uint32_t size,i,j,pass,version,stride,count;int status=rf_geomod_piece_registry_state_size(r,&size);
    if(status)return status;if(!input || bytes<PIECE_STATE_HEADER)return RF_FORMAT;
    version=piece_state_word(start+4);stride=version==1?320:version==2?328:0;
    count=(size-PIECE_STATE_HEADER)/PIECE_STATE_RECORD;
    if(!stride || bytes!=PIECE_STATE_HEADER+(uint64_t)count*stride || memcmp(start,"RFPB",4) ||
       piece_state_word(start+8)!=bytes || piece_state_word(start+12)!=count)return RF_FORMAT;
    for(pass=0;pass<2;pass++) {
        p=start+PIECE_STATE_HEADER;
        for(i=0;i<r->count;i++)for(j=0;j<r->active[i].batch->count;j++,p+=stride) {
            rf_geomod_piece_batch *batch=r->active[i].batch;
            rf_physics_body_state state={0},*target=&batch->bodies[j].state;
            rf_geomod_piece_life life,birth;uint32_t bits;
            if(piece_state_word(p)!=r->active[i].prefix || piece_state_word(p+4)!=r->active[i].ordinal || piece_state_word(p+8)!=j)return RF_FORMAT;
            birth.health=batch->birth_health[j];birth.flags=0;life=birth;
            if(version==2){bits=piece_state_word(p+320);memcpy(&life.health,&bits,4);life.flags=piece_state_word(p+324);}
            if(!isfinite(life.health) || life.health>birth.health || (life.flags&~0x6200002u) ||
               (life.health<=0 && !(life.flags&2)))return RF_FORMAT;
            /* Revival needs a history-rebuilt owner, not a collected tombstone. */
            if(!(life.flags&2) && !batch->bodies[j].allocated_bytes)return RF_FORMAT;
            piece_state_codec(&state,(unsigned char *)p+12,1);
            if(!pass){status=piece_state_valid(&state,target);if(status)return status;}
            else {*target=state;batch->life[j]=life;}
        }
    }
    return RF_OK;
}
int rf_geomod_piece_registry_damage(rf_geomod_piece_registry *r,uint32_t batch,uint32_t piece,float amount)
{
    if(!r || r->begun || batch>=r->count || piece>=r->active[batch].batch->count)return RF_RANGE;
    return rf_geomod_piece_life_damage(r->active[batch].batch->life+piece,amount);
}

int rf_geomod_piece_registry_collect_retired(rf_geomod_piece_registry *r,uint32_t *released)
{
    uint32_t b,i,total=0;
    if(!r || !released || r->begun)return RF_RANGE;
    for(b=0;b<r->count;b++) {
        rf_geomod_piece_batch *batch=r->active[b].batch;uint32_t live=0,freed=0;
        for(i=0;i<batch->count;i++) {
            rf_physics_body *body=batch->bodies+i;
            if(rf_geomod_piece_batch_alive(batch,i)){live++;continue;}
            if(body->allocated_bytes) {
                freed+=body->allocated_bytes-sizeof(*body);
                rf_physics_spheres_close(&body->spheres);body->allocated_bytes=0;
            }
        }
        if(!live && batch->geometry) {
            freed+=rf_geomod_piece_bank_bytes(batch->geometry);
            rf_geomod_piece_bank_close(&batch->geometry);
        }
        batch->bytes-=freed;r->bytes-=freed;total+=freed;
    }
    *released=total;return RF_OK;
}

static int piece_registry_sweep_except(const rf_geomod_piece_registry *r,uint32_t excluded_batch,uint32_t excluded_piece,float minimum_body_radius,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_registry_hit *out,uint32_t *matched)
{
    rf_geomod_registry_hit best={0};uint32_t i,k,found=0;float nearest=limit;int status;
    if(!start || !delta || !out || !matched || !isfinite(radius) || radius<0 || !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(start[k]) || !isfinite(delta[k]))return RF_RANGE;
    if(r && r->begun)return RF_RANGE;
    for(i=0;r && i<r->count;i++) {
        rf_geomod_piece_hit hit;uint32_t candidate;
        status=piece_batch_sweep_except(r->active[i].batch,i==excluded_batch?excluded_piece:UINT32_MAX,minimum_body_radius,flags,start,delta,radius,nearest,&hit,&candidate);if(status)return status;
        if(!candidate || (found && hit.hit.fraction>=nearest))continue;
        best.piece=hit;best.batch=i;nearest=hit.hit.fraction;found=1;
    }
    if(found)*out=best;*matched=found;return RF_OK;
}

int rf_geomod_piece_registry_sweep(const rf_geomod_piece_registry *r,uint32_t flags,
    const float start[3],const float delta[3],float radius,float limit,
    rf_geomod_registry_hit *out,uint32_t *matched)
{return piece_registry_sweep_except(r,UINT32_MAX,UINT32_MAX,-1,flags,start,delta,radius,limit,out,matched);}

typedef struct piece_body_query_context {
    const rf_geomod_piece_registry *registry;uint32_t material,excluded_batch,excluded_piece;
    rf_geomod_registry_hit selected;uint32_t sphere;float velocity[3],minimum_body_radius;
} piece_body_query_context;
static int piece_body_query(void *opaque,const rf_collision_body_request *request,
    rf_collision_body_candidate *candidate,uint32_t *matched)
{
    piece_body_query_context *context=opaque;rf_geomod_registry_hit hit;
    const rf_geomod_piece_batch *batch;rf_geomod_owned_piece piece;int status;
    status=piece_registry_sweep_except(context->registry,context->excluded_batch,context->excluded_piece,context->minimum_body_radius,request->flags,request->start,request->delta,
        request->radius,request->limit,&hit,matched);if(status || !*matched)return status;
    batch=context->registry->active[hit.batch].batch;
    status=rf_geomod_piece_bank_get(batch->geometry,hit.piece.piece,&piece);if(status)return status;
    candidate->hit=hit.piece.hit;candidate->material=context->material;
    candidate->texture=piece.mesh.faces[hit.piece.face].material;
    candidate->face_flags=piece.filters[hit.piece.face].face_flags;candidate->face_token=UINT32_MAX;
    context->selected=hit;context->sphere=request->sphere;
    memcpy(context->velocity,batch->bodies[hit.piece.piece].state.velocity,12);return RF_OK;
}
static int piece_body_sweep_filtered(const rf_geomod_piece_registry *registry,
    uint32_t excluded_batch,uint32_t excluded_piece,float minimum_body_radius,
    const rf_collision_body_query *query,uint32_t material,rf_geomod_registry_body_hit *out,uint32_t *matched)
{
    piece_body_query_context context={0};rf_geomod_registry_body_hit result={0};uint32_t found;int status;
    if(!out || !matched || (registry && registry->begun))return RF_RANGE;
    if(excluded_batch!=UINT32_MAX && (!registry || excluded_batch>=registry->count ||
       excluded_piece>=registry->active[excluded_batch].batch->count))return RF_RANGE;
    context.registry=registry;context.material=material;
    context.excluded_batch=excluded_batch;context.excluded_piece=excluded_piece;context.minimum_body_radius=minimum_body_radius;
    status=rf_collision_body_sweep(query,NULL,0,piece_body_query,&context,&result.contact,&found);if(status)return status;
    if(found) {
        result.batch=context.selected.batch;result.piece=context.selected.piece.piece;
        result.face=context.selected.piece.face;result.sphere=context.sphere;
        memcpy(result.contact.velocity,context.velocity,12);*out=result;
    }
    *matched=found;return RF_OK;
}

int rf_geomod_piece_registry_body_sweep_excluding(const rf_geomod_piece_registry *registry,
    uint32_t batch,uint32_t piece,const rf_collision_body_query *query,uint32_t material,
    rf_geomod_registry_body_hit *out,uint32_t *matched)
{return piece_body_sweep_filtered(registry,batch,piece,-1,query,material,out,matched);}

int rf_geomod_piece_registry_player_admitted_sweep(const rf_geomod_piece_registry *registry,
    const rf_collision_body_query *query,uint32_t material,rf_geomod_registry_body_hit *out,uint32_t *matched)
{return piece_body_sweep_filtered(registry,UINT32_MAX,UINT32_MAX,.5f,query,material,out,matched);}

int rf_geomod_piece_registry_body_sweep(const rf_geomod_piece_registry *registry,
    const rf_collision_body_query *query,uint32_t material,rf_geomod_registry_body_hit *out,uint32_t *matched)
{return rf_geomod_piece_registry_body_sweep_excluding(registry,UINT32_MAX,UINT32_MAX,query,material,out,matched);}

int rf_geomod_piece_registry_placement_check(const rf_geomod_piece_registry *r,const rf_checkpoint_placement *p)
{
    uint32_t b,i,n,k,j;int status;
    if(!p || !p->spheres || !p->count || p->count>8 || (r && r->begun))return RF_RANGE;
    for(b=0;r && b<r->count;b++)for(i=0;i<r->active[b].batch->count;i++) {
        const rf_geomod_piece_batch *batch=r->active[b].batch;rf_geomod_owned_piece piece;
        const rf_physics_body_state *body=&batch->bodies[i].state;
        if(!rf_geomod_piece_batch_alive(batch,i) || !(body->bounds.radius>.5f))continue;
        status=rf_geomod_piece_bank_get(batch->geometry,i,&piece);if(status)return status;
        for(n=0;n<p->count;n++) {
            double world[3];float local[3];
            for(k=0;k<3;k++) {
                world[k]=p->position[k];
                for(j=0;j<3;j++)world[k]+=(double)p->spheres[n].center[j]*p->basis[j*3+k];
                world[k]-=body->position[k];
            }
            for(k=0;k<3;k++)local[k]=(float)(world[0]*body->orientation[k*3]+world[1]*body->orientation[k*3+1]+world[2]*body->orientation[k*3+2]);
            if(body->bounds.radius<=1) {
                uint32_t q;
                for(q=0;q<batch->bodies[i].spheres.count;q++) {
                    const rf_physics_sphere *target=batch->bodies[i].spheres.items+q;
                    double distance=0,radius=(double)p->spheres[n].radius+target->radius-.002;
                    for(k=0;k<3;k++){double d=(double)local[k]-target->center[k];distance+=d*d;}
                    if(radius>0 && distance<radius*radius)return RF_NOT_FOUND;
                }
            } else {
                status=rf_checkpoint_solid_sphere_check(piece.collision,piece.mesh.face_count,p->query_flags,local,p->spheres[n].radius,NULL);
                if(status)return status;
            }
        }
    }
    return RF_OK;
}

static rf_damage_object *piece_life_lookup(void *context,uint32_t handle)
{(void)handle;return context;}
static uint32_t piece_life_predicate(void *context,uint32_t stage,uint32_t handle,const rf_damage_object *object)
{(void)context;(void)stage;(void)handle;(void)object;return 0;}
static float piece_life_effect(void *context,rf_damage_object *object,float amount,uint32_t source,int32_t kind,uint32_t extra)
{(void)context;(void)object;(void)amount;(void)source;(void)kind;(void)extra;return 0;}
int rf_geomod_piece_life_init(float radius,rf_geomod_piece_life *out)
{
    double health;
    if(!out)return RF_RANGE;
    if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    health=(double)radius*50;if(health>FLT_MAX)return RF_RANGE;
    out->health=(float)health;out->flags=0;return RF_OK;
}
int rf_geomod_piece_life_damage(rf_geomod_piece_life *life,float amount)
{
    rf_damage_object object;rf_damage_request request={0};float ignored;int status;
    rf_damage_backend backend={piece_life_lookup,piece_life_predicate,piece_life_effect,&object};
    if(!life)return RF_RANGE;
    if(!isfinite(life->health) || !isfinite(amount) || amount<0)return RF_FORMAT;
    if(life->flags&2)return RF_OK;
    object.type=3;object.flags=life->flags;object.health=life->health;
    request.amount=amount;request.source=UINT32_MAX;request.kind=3;request.auxiliary_uid=UINT32_MAX;
    status=rf_damage_dispatch_sp(0,&request,1,&backend,&ignored);if(status)return status;
    if(object.health<=0)object.flags|=2;
    life->health=object.health;life->flags=object.flags;return RF_OK;
}

static int piece_support(const rf_geomod_piece_registry *r,const rf_collision_actor_general_response *source,
    const rf_physics_ground_probe *probe,float limit,rf_checkpoint_support_hit *out,uint32_t *matched)
{
    rf_geomod_registry_body_hit hit;rf_checkpoint_support_hit result;const rf_physics_body_state *body;uint32_t i,found;int status;
    if(!probe || !out || !matched)return RF_RANGE;
    status=rf_geomod_piece_registry_player_ground(r,source,probe->start,probe->end,probe->query_flags,limit,1,&hit,&found);if(status)return status;
    if(found) {
        body=&r->active[hit.batch].batch->bodies[hit.piece].state;
        result.fraction=hit.contact.fraction;memcpy(result.normal,hit.contact.normal,12);result.stable=!(body->flags&0x80000000u);
        for(i=0;i<3;i++)if(body->velocity[i]!=0 || body->vector_c8[i]!=0)result.stable=0;
        *out=result;
    }
    *matched=found;return RF_OK;
}
int rf_geomod_piece_registry_support(void *context,const rf_physics_ground_probe *probe,float limit,
    rf_checkpoint_support_hit *out,uint32_t *matched)
{
    rf_collision_actor_general_response source={0};uint32_t i;
    if(!probe)return RF_RANGE;
    source.actor.spheres=&probe->sphere;source.actor.sphere_count=1;source.actor.mass=1;source.extent=fmaxf(probe->bounds.radius,probe->sphere.radius);
    for(i=0;i<3;i++)source.orientation[i*3+i]=source.next_orientation[i*3+i]=1;
    return piece_support(context,&source,probe,limit,out,matched);
}
int rf_geomod_piece_registry_player_support(void *context,const rf_physics_ground_probe *probe,float limit,
    rf_checkpoint_support_hit *out,uint32_t *matched)
{
    const rf_geomod_player_support_context *c=context;rf_collision_actor_general_response source={0};
    if(!c || !c->player || !probe || c->player->count>8)return RF_RANGE;
    source.actor.spheres=c->player->spheres;source.actor.sphere_count=(int32_t)c->player->count;source.actor.mass=1;
    source.extent=probe->bounds.radius;memcpy(source.orientation,c->player->basis,36);memcpy(source.next_orientation,c->player->basis,36);
    return piece_support(c->registry,&source,probe,limit,out,matched);
}

static int piece_registry_notify(rf_geomod_piece_registry *r,const rf_geomod_notify_change *change,
    const float center[3],uint32_t *woken,uint32_t apply)
{
    uint32_t pass,b,i,k,count=0;int status;
    if(!change || !center || !woken || (r && r->begun))return RF_RANGE;
    if(!isfinite(change->radius) || change->count>32 || (change->count&&!change->boxes))return RF_FORMAT;
    for(k=0;k<3;k++)if(!isfinite(center[k]))return RF_FORMAT;
    for(i=0;i<change->count;i++)for(k=0;k<3;k++)if(!isfinite(change->boxes[i].minimum[k]) ||
        !isfinite(change->boxes[i].maximum[k]) || change->boxes[i].minimum[k]>change->boxes[i].maximum[k])return RF_FORMAT;
    for(pass=0;pass<(apply?2u:1u);pass++)for(b=0;r && b<r->count;b++)for(i=0;i<r->active[b].batch->count;i++) {
        rf_geomod_piece_batch *batch=r->active[b].batch;rf_physics_body_state *body=&batch->bodies[i].state;
        rf_geomod_notify_object object={0};rf_geomod_notify_radial radial={0};rf_geomod_notify_result result;
        if(!rf_geomod_piece_batch_alive(batch,i))continue;
        object.kind=3;object.object_flags=batch->life[i].flags|0x400000u;object.physics_flags=body->flags;
        memcpy(object.bounds.minimum,body->bounds.minimum,12);memcpy(object.bounds.maximum,body->bounds.maximum,12);
        memcpy(radial.center,center,12);memcpy(radial.position,body->position,12);radial.body_radius=body->bounds.radius;
        status=rf_geomod_notify_object_change(&object,change,&radial,&result);if(status)return status;
        if(pass) {
            if(!(body->flags&0x80000000u) && (result.physics_flags&0x80000000u))count++;
            body->flags=result.physics_flags;batch->life[i].flags=result.object_flags&~0x400000u;
        }
    }
    *woken=count;return RF_OK;
}

int rf_geomod_piece_registry_notify_check(rf_geomod_piece_registry *r,const rf_geomod_notify_change *change,
    const float center[3])
{uint32_t ignored;return piece_registry_notify(r,change,center,&ignored,0);}
int rf_geomod_piece_registry_notify(rf_geomod_piece_registry *r,const rf_geomod_notify_change *change,
    const float center[3],uint32_t *woken)
{return piece_registry_notify(r,change,center,woken,1);}

static const float *piece_no_extra_velocity(void *context,uint32_t handle)
{(void)context;(void)handle;return NULL;}
static int piece_actor_sphere_contact(const rf_geomod_piece_registry *r,
    const rf_collision_actor_general_response *source,uint32_t admitted,float maximum_radius,uint32_t material,
    rf_collision_actor_contact *out,uint32_t *batch_index,uint32_t *piece_index,uint32_t *matched)
{
    rf_collision_actor_contact best;uint32_t b,i,k,found=0,best_batch=0,best_piece=0;
    if(!source || !out || !batch_index || !piece_index || !matched || source->kind!=0 ||
       (r && r->begun) || source->actor.sphere_count<0 ||
       (source->actor.sphere_count && !source->actor.spheres))return RF_RANGE;
    if(!isfinite(source->actor.contact.time) || source->actor.contact.time<0 || source->actor.contact.time>1 ||
       !isfinite(source->actor.mass) || source->actor.mass<=0)return RF_FORMAT;
    if(!isfinite(source->extent) || source->extent<0)return RF_FORMAT;
    for(k=0;k<3;k++)if(!isfinite(source->actor.minimum[k]) || !isfinite(source->actor.maximum[k]) ||
        source->actor.minimum[k]>source->actor.maximum[k] || !isfinite(source->actor.position[k]) ||
        !isfinite(source->actor.next_position[k]) || !isfinite(source->actor.velocity[k]))return RF_FORMAT;
    for(k=0;k<9;k++)if(!isfinite(source->orientation[k]) || !isfinite(source->next_orientation[k]))return RF_FORMAT;
    for(i=0;i<(uint32_t)source->actor.sphere_count;i++) {
        const rf_physics_sphere *sphere=source->actor.spheres+i;
        if(!isfinite(sphere->radius) || sphere->radius<0)return RF_FORMAT;
        for(k=0;k<3;k++)if(!isfinite(sphere->center[k]))return RF_FORMAT;
    }
    best=source->actor.contact;
    if(admitted)for(b=0;r && b<r->count;b++)for(i=0;i<r->active[b].batch->count;i++) {
        const rf_geomod_piece_batch *batch=r->active[b].batch;const rf_physics_body *body=batch->bodies+i;
        const rf_physics_body_state *state=&body->state;rf_collision_actor_general_response actor=*source,target={0};
        if(!rf_geomod_piece_batch_alive(batch,i) || !(state->bounds.radius>.5f) || state->bounds.radius>maximum_radius ||
           !((source->actor.body_flags|state->flags)&0x20u))continue;
        actor.actor.contact=best;
        memcpy(target.actor.minimum,state->bounds.minimum,12);memcpy(target.actor.maximum,state->bounds.maximum,12);
        memcpy(target.actor.position,state->position,12);memcpy(target.actor.next_position,state->next_position,12);
        memcpy(target.actor.velocity,state->velocity,12);target.actor.mass=state->mass;
        target.actor.handle=UINT32_MAX;target.actor.material=material;target.actor.body_flags=state->flags;
        target.actor.sphere_count=(int32_t)body->spheres.count;target.actor.spheres=body->spheres.items;target.actor.contact.time=1;
        memcpy(target.orientation,state->orientation,36);memcpy(target.next_orientation,state->next_orientation,36);
        target.extent=state->bounds.radius;target.kind=3;
        rf_collision_actors_general_response(&actor,&target,piece_no_extra_velocity,NULL);
        if(actor.actor.contact.time<best.time){best=actor.actor.contact;best_batch=b;best_piece=i;found=1;}
    }
    if(found){*out=best;*batch_index=best_batch;*piece_index=best_piece;}*matched=found;return RF_OK;
}

int rf_geomod_piece_registry_npc_contact(const rf_geomod_piece_registry *r,
    const rf_collision_actor_general_response *source,uint32_t use_kind,uint32_t material,
    rf_collision_actor_contact *out,uint32_t *batch,uint32_t *piece,uint32_t *matched)
{return piece_actor_sphere_contact(r,source,use_kind==1,FLT_MAX,material,out,batch,piece,matched);}

int rf_geomod_piece_registry_player_motion(const rf_geomod_piece_registry *r,
    const rf_collision_actor_general_response *source,const rf_collision_body_query *query,uint32_t material,
    rf_geomod_registry_body_hit *out,uint32_t *matched)
{
    rf_geomod_registry_body_hit result={0};rf_collision_actor_general_response limited;
    rf_collision_actor_contact contact;uint32_t found,sphere_found,batch,piece;int status;
    if(!source || !query || !out || !matched)return RF_RANGE;
    limited=*source;limited.actor.contact.time=query->limit;
    /* Validate actor data even if an earlier polygon hit would hide it. */
    status=piece_actor_sphere_contact(NULL,&limited,1,1,material,&contact,&batch,&piece,&sphere_found);if(status)return status;
    status=piece_body_sweep_filtered(r,UINT32_MAX,UINT32_MAX,1,query,material,&result,&found);if(status)return status;
    if(found)limited.actor.contact.time=result.contact.fraction;
    status=piece_actor_sphere_contact(r,&limited,1,1,material,&contact,&batch,&piece,&sphere_found);if(status)return status;
    if(sphere_found) {
        memset(&result,0,sizeof(result));result.batch=batch;result.piece=piece;result.sphere=result.face=UINT32_MAX;
        memcpy(result.contact.point,contact.point,12);memcpy(result.contact.normal,contact.normal,12);
        result.contact.fraction=contact.time;result.contact.material=contact.material;
        memcpy(&result.contact.reserved_20,&contact.inverse_mass,4);memcpy(result.contact.velocity,contact.velocity,12);
        result.contact.object_id=result.contact.texture=result.contact.face_token=UINT32_MAX;found=1;
    }
    if(found)*out=result;*matched=found;return RF_OK;
}

int rf_geomod_piece_registry_player_ground(const rf_geomod_piece_registry *r,
    const rf_collision_actor_general_response *source,const float start[3],const float end[3],
    uint32_t flags,float limit,uint32_t material,rf_geomod_registry_body_hit *out,uint32_t *matched)
{
    rf_collision_body_query query={0};rf_collision_body_sphere spheres[8];rf_geomod_registry_body_hit result={0};
    rf_collision_actor_general_response actor;rf_collision_actor_contact contact;uint32_t found,b,i,k,dummy;int status;
    if(!source || !start || !end || !out || !matched || source->actor.sphere_count<0 || source->actor.sphere_count>8)return RF_RANGE;
    actor=*source;actor.actor.contact.time=limit;
    status=piece_actor_sphere_contact(NULL,&actor,1,1,material,&contact,&b,&i,&dummy);if(status)return status;
    memcpy(query.start,start,12);memcpy(query.end,end,12);memcpy(query.matrix,source->orientation,36);
    query.radius=source->extent;query.flags=flags;query.limit=limit;query.spheres=spheres;query.count=(uint32_t)source->actor.sphere_count;
    for(i=0;i<query.count;i++){memcpy(spheres[i].center,source->actor.spheres[i].center,12);spheres[i].radius=source->actor.spheres[i].radius;}
    status=piece_body_sweep_filtered(r,UINT32_MAX,UINT32_MAX,1,&query,material,&result,&found);if(status)return status;
    if(found)actor.actor.contact.time=result.contact.fraction;
    for(b=0;r && b<r->count;b++)for(i=0;i<r->active[b].batch->count;i++) {
        const rf_geomod_piece_batch *batch=r->active[b].batch;const rf_physics_body *body=batch->bodies+i;
        rf_collision_actor_general_response target={0};
        if(!rf_geomod_piece_batch_alive(batch,i) || !(body->state.bounds.radius>.5f) || body->state.bounds.radius>1)continue;
        target.actor.spheres=body->spheres.items;target.actor.sphere_count=(int32_t)body->spheres.count;
        memcpy(target.actor.position,body->state.position,12);memcpy(target.orientation,body->state.orientation,36);
        contact=actor.actor.contact;
        if(!rf_collision_actors_ground_spheres(start,end,&actor,&target,&contact))continue;
        actor.actor.contact=contact;memset(&result,0,sizeof(result));result.batch=b;result.piece=i;result.face=result.sphere=UINT32_MAX;
        memcpy(result.contact.point,contact.point,12);memcpy(result.contact.normal,contact.normal,12);
        result.contact.fraction=contact.time;result.contact.material=material;
        for(k=0;k<3;k++)result.contact.velocity[k]=body->state.velocity[k];
        result.contact.object_id=result.contact.texture=result.contact.face_token=UINT32_MAX;found=1;
    }
    if(found)*out=result;*matched=found;return RF_OK;
}
