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
            bank->collision+bank->nf,mesh->face_count);if(status)return status;
        piece.mass_ready=1;
    }
    bank->pieces[bank->np++]=piece;bank->nv+=mesh->vertex_count;bank->nf+=mesh->face_count;
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
