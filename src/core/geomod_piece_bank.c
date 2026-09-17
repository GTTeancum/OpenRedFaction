#include "rf/geomod_piece_bank.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
struct rf_geomod_piece_bank {
    rf_geomod_owned_piece *pieces;rf_geomod_vertex *vertices;rf_geomod_face *faces;
    uint32_t *old_faces;rf_collision_face_filter *filters;
    uint32_t vc,fc,pc,nv,nf,np,bytes;
};
int rf_geomod_piece_bank_open(uint32_t vc,uint32_t fc,uint32_t pc,uint32_t budget,rf_geomod_piece_bank **out)
{
    uint64_t bytes;rf_geomod_piece_bank *bank;unsigned char *p;
    if(!out || *out || !vc || !fc || !pc)return RF_RANGE;
    bytes=sizeof(*bank)+(uint64_t)pc*sizeof(rf_geomod_owned_piece)+(uint64_t)vc*sizeof(rf_geomod_vertex)+
        (uint64_t)fc*(sizeof(rf_geomod_face)+sizeof(uint32_t)+sizeof(rf_collision_face_filter));
    if(bytes>budget || bytes>UINT32_MAX)return RF_RANGE;
    bank=calloc(1,(size_t)bytes);if(!bank)return RF_IO;
    p=(unsigned char *)(bank+1);bank->pieces=(rf_geomod_owned_piece *)p;p+=pc*sizeof(*bank->pieces);
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
int rf_geomod_piece_bank_append(rf_geomod_piece_bank *bank,const rf_geomod_mesh_view *mesh,
    const uint32_t *old_faces,const rf_collision_face_filter *filters,uint32_t source_count,uint32_t id)
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
    bank->pieces[bank->np++]=piece;bank->nv+=mesh->vertex_count;bank->nf+=mesh->face_count;
    return RF_OK;
}
