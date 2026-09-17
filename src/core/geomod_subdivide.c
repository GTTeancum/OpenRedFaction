#include "rf/geomod_piece_bank.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#define QUEUE 16
#define VERTICES 128
#define FACES 32
typedef struct subdivision_node {
    rf_geomod_vertex vertices[VERTICES];rf_geomod_face faces[FACES];
    rf_collision_face_filter filters[FACES];uint32_t nv,nf;
} subdivision_node;
typedef struct subdivision_work {
    subdivision_node nodes[QUEUE];
    rf_geomod_vertex local[VERTICES],cutter_vertices[24],extracted[1024];
    rf_geomod_face cutter_faces[6],extracted_faces[256],source_faces[FACES];
    uint32_t source_ids[FACES];
    uint32_t labels[256],map[256],scratch[9000],identity[FACES];
} subdivision_work;
static int copy_node(subdivision_node *node,const rf_geomod_mesh_view *mesh,
    const rf_collision_face_filter *filters)
{
    if(mesh->vertex_count>VERTICES || mesh->face_count>FACES || !mesh->vertex_count || !mesh->face_count)return RF_RANGE;
    memcpy(node->vertices,mesh->vertices,mesh->vertex_count*sizeof(*mesh->vertices));
    memcpy(node->faces,mesh->faces,mesh->face_count*sizeof(*mesh->faces));
    memcpy(node->filters,filters,mesh->face_count*sizeof(*filters));node->nv=mesh->vertex_count;node->nf=mesh->face_count;return RF_OK;
}
int rf_geomod_piece_subdivide(const rf_geomod_mesh_view *source,const rf_collision_face_filter *filters,
    const rf_collision_face_filter *generated,uint32_t material,float density,
    rf_random_state *random,uint32_t budget,rf_geomod_piece_bank **out,rf_geomod_subdivision_stats *stats)
{
    subdivision_work *work=NULL;rf_geomod_piece_bank *bank=NULL;rf_geomod_terrain *terrain=NULL;
    rf_geomod_subdivision_stats totals={0};rf_random_state next;uint32_t head=0,queued=1,resident,i;int status;
    if(!source || !source->vertices || !source->faces || !filters || !generated || !random || !out || *out || !stats ||
       !isfinite(density) || density<0 || material==UINT32_MAX || budget<=sizeof(*work))return RF_RANGE;
    work=calloc(1,sizeof(*work));if(!work)return RF_IO;next=*random;
    status=copy_node(work->nodes,source,filters);if(status)goto done;
    status=rf_geomod_piece_bank_open(2048,512,16,budget-sizeof(*work),&bank);if(status)goto done;
    resident=(uint32_t)sizeof(*work)+rf_geomod_piece_bank_bytes(bank);totals.peak_bytes=resident;
    for(i=0;i<FACES;i++)work->identity[i]=i;
    while(queued) {
        subdivision_node *node=work->nodes+head;rf_geomod_mesh_view mesh={node->vertices,node->faces,node->nv,node->nf,0};
        rf_geomod_piece_placement placement;rf_geomod_piece_shape shape;
        status=rf_geomod_mesh_recenter(mesh.vertices,mesh.vertex_count,work->local,&placement);if(status)goto done;
        status=rf_geomod_piece_shape_get(placement.minimum,placement.maximum,placement.radius,totals.attempts,&shape);if(status)goto done;
        if(!shape.subdivide) {
            status=rf_geomod_piece_bank_append_physical(bank,&mesh,work->identity,node->filters,node->nf,totals.terminal+1,density);
            if(status)goto done;totals.terminal++;head=(head+1)%QUEUE;queued--;continue;
        }
        {
            rf_geomod_piece_cutter pose;rf_geomod_mesh_view cutter;rf_geomod_terrain_view view;
            uint32_t words,components,largest,c;float parent_radius=placement.radius;
            status=rf_geomod_piece_cutter_prepare(shape.axis,shape.length,&next,&pose);if(status)goto done;
            for(i=0;i<3;i++)pose.offset[i]=(float)((double)pose.offset[i]+placement.origin[i]);
            status=rf_geomod_piece_cutter_mesh(&pose,material,work->cutter_vertices,work->cutter_faces);if(status)goto done;
            cutter=(rf_geomod_mesh_view){work->cutter_vertices,work->cutter_faces,24,6,0};
            /* Temporary terrain IDs must be unique and non-generated. Preserve
             * the external surface identity separately across every subdivision. */
            for(i=0;i<mesh.face_count;i++){work->source_faces[i]=mesh.faces[i];work->source_ids[i]=mesh.faces[i].source_face;work->source_faces[i].source_face=i;}
            mesh.faces=work->source_faces;
            status=rf_geomod_terrain_open(&mesh,node->filters,generated,0,1024,256,budget-resident,&terrain);if(status)goto done;
            totals.attempts++;
            status=rf_geomod_terrain_cut_convex(terrain,&cutter);if(status)goto done;
            status=rf_geomod_terrain_get(terrain,&view);if(status)goto done;
            if(resident+view.peak_bytes>totals.peak_bytes)totals.peak_bytes=resident+view.peak_bytes;
            status=rf_geomod_component_work_size(&view.mesh,&words);if(status)goto done;
            if(words>9000){status=RF_RANGE;goto done;}
            status=rf_geomod_mesh_components(&view.mesh,NULL,work->scratch,words,work->labels,&components,&largest);if(status)goto done;
            /* The original queues extracted labels1..N-1 before retained label0. */
            head=(head+1)%QUEUE;queued--;
            for(c=0;c<components;c++) {
                uint32_t label=c+1<components?c+1:0,offset,j;rf_geomod_mesh_view kept,piece;
                subdivision_node *child;
                status=rf_geomod_component_extract(&view.mesh,work->labels,label,work->extracted,1024,work->extracted_faces,256,work->map,&kept,&piece);
                if(status)goto done;
                if(piece.vertex_count>VERTICES || piece.face_count>FACES){status=RF_RANGE;goto done;}
                status=rf_geomod_mesh_recenter(piece.vertices,piece.vertex_count,work->local,&placement);if(status)goto done;
                if(placement.radius>parent_radius || queued==QUEUE){totals.discarded++;continue;}
                child=work->nodes+(head+queued)%QUEUE;offset=kept.face_count;
                /* Terrain owns a copy, so reusing a freed queue slot is safe. */
                memcpy(child->vertices,piece.vertices,piece.vertex_count*sizeof(*piece.vertices));
                memcpy(child->faces,piece.faces,piece.face_count*sizeof(*piece.faces));
                child->nv=piece.vertex_count;child->nf=piece.face_count;
                for(j=0;j<piece.face_count;j++) {
                    uint32_t id=child->faces[j].source_face;
                    if(id!=UINT32_MAX)child->faces[j].source_face=work->source_ids[id];
                    child->filters[j]=view.faces[work->map[offset+j]].filter;
                }
                queued++;
            }
            rf_geomod_terrain_close(&terrain);
        }
    }
    *out=bank;bank=NULL;*random=next;*stats=totals;status=RF_OK;
done:
    rf_geomod_terrain_close(&terrain);rf_geomod_piece_bank_close(&bank);free(work);return status;
}
