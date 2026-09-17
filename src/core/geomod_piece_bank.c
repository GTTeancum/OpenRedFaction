#include "rf/geomod_piece_bank.h"
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
int rf_geomod_piece_body_open(const rf_geomod_owned_piece *piece,float elasticity,float friction,
    uint32_t budget,rf_physics_body *body)
{
    rf_physics_body_parameters p={0};rf_physics_sphere spheres[64];uint32_t count;float radius;int status;
    if(!piece || !piece->mass_ready || !body)return RF_RANGE;
    status=rf_physics_grid_spheres(piece->mass.cells,piece->mass.spacing,piece->mass.origin,spheres,64,&count,&radius);
    if(status)return status;
    p.coefficients[0]=elasticity;p.coefficients[1]=(float)((double)piece->birth_radius*(double).2f);p.coefficients[2]=friction;
    p.mass=piece->mass.mass;p.flags=0x8000003f;
    memcpy(p.local_tensor,piece->mass.inverse_tensor,36);memcpy(p.position,piece->placement.origin,12);
    p.orientation[0]=p.orientation[4]=p.orientation[8]=1;
    return rf_physics_body_open(&p,spheres,count,budget,body);
}

struct rf_geomod_piece_batch {
    rf_geomod_piece_bank *geometry;rf_physics_body *bodies;
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
uint32_t rf_geomod_piece_batch_bytes(const rf_geomod_piece_batch *batch)
{return batch?batch->bytes:0;}
uint32_t rf_geomod_piece_batch_peak_bytes(const rf_geomod_piece_batch *batch)
{return batch?batch->peak_bytes:0;}
int rf_geomod_piece_batch_get(rf_geomod_piece_batch *batch,uint32_t index,
    rf_geomod_owned_piece *piece,rf_physics_body **body)
{
    rf_geomod_owned_piece value;int status;
    if(!batch || !piece || !body || index>=batch->count)return RF_RANGE;
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
    count=rf_geomod_piece_bank_count(geometry);owner_bytes=sizeof(*batch)+(uint64_t)count*sizeof(rf_physics_body);
    resident=owner_bytes+rf_geomod_piece_bank_bytes(geometry);
    if(resident>budget){status=RF_RANGE;goto failed;}
    batch=calloc(1,(size_t)owner_bytes);if(!batch){status=RF_IO;goto failed;}
    batch->geometry=geometry;geometry=NULL;batch->bodies=(rf_physics_body *)(batch+1);
    batch->bytes=(uint32_t)resident;batch->peak_bytes=stats.peak_bytes;
    for(i=0;i<count;i++) {
        rf_geomod_owned_piece piece;
        status=rf_geomod_piece_bank_get(batch->geometry,i,&piece);if(status)goto failed;
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
