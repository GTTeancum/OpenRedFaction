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
    rf_collision_face bound_faces[256];rf_collision_face_filter bound_filters[256];float positions[1024][3];
    uint32_t labels[256],map[256],scratch[9000],identity[FACES];
} subdivision_work;
static int copy_node(subdivision_node *node,const rf_geomod_mesh_view *mesh,
    const rf_collision_face_filter *filters)
{
    if(mesh->vertex_count>VERTICES || mesh->face_count>FACES || !mesh->vertex_count || !mesh->face_count)return RF_RANGE;
    /* Split exact T-junctions using existing authored/cut positions. This is
     * edge subdivision, not epsilon welding: no position is moved and a nearby
     * off-edge vertex is never admitted. Interpolate only this face's UVs. */
    node->nv=0;node->nf=mesh->face_count;
    for(uint32_t f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;uint32_t first=node->nv;
        if(face->count<3 || face->count>64 || face->first>mesh->vertex_count ||
            face->count>mesh->vertex_count-face->first)return RF_FORMAT;
        node->faces[f]=*face;node->faces[f].first=first;node->filters[f]=filters[f];
        for(uint32_t e=0;e<face->count;e++) {
            const rf_geomod_vertex *a=mesh->vertices+face->first+e;
            const rf_geomod_vertex *b=mesh->vertices+face->first+(e+1)%face->count;
            double d[3],parameters[VERTICES];uint32_t indices[VERTICES],n=0,axis=0,k;
            for(k=0;k<3;k++){d[k]=(double)b->position[k]-a->position[k];if(fabs(d[k])>fabs(d[axis]))axis=k;}
            if(d[axis]==0)return RF_FORMAT;
            for(uint32_t v=0;v<mesh->vertex_count;v++) {
                double q[3],t;uint32_t j;
                for(k=0;k<3;k++)q[k]=(double)mesh->vertices[v].position[k]-a->position[k];
                t=q[axis]/d[axis];if(!(t>0 && t<1))continue;
                for(k=0;k<3;k++)if(q[k]*d[axis]!=q[axis]*d[k])break;
                if(k<3)continue;
                for(j=0;j<n && parameters[j]<t;j++){}
                if(j<n && parameters[j]==t)continue;
                memmove(parameters+j+1,parameters+j,(n-j)*sizeof(*parameters));
                memmove(indices+j+1,indices+j,(n-j)*sizeof(*indices));
                parameters[j]=t;indices[j]=v;n++;
            }
            if(node->nv+1+n>VERTICES || node->nv+1+n-first>64)return RF_RANGE;
            node->vertices[node->nv++]=*a;
            for(k=0;k<n;k++) {
                rf_geomod_vertex *v=node->vertices+node->nv++;
                memcpy(v->position,mesh->vertices[indices[k]].position,sizeof(v->position));
                for(uint32_t j=0;j<2;j++)v->uv[j]=(float)((double)a->uv[j]+parameters[k]*((double)b->uv[j]-a->uv[j]));
            }
        }
        node->faces[f].count=node->nv-first;
    }
    return RF_OK;
}
int rf_geomod_piece_subdivide(const rf_geomod_mesh_view *source,const rf_collision_face_filter *filters,
    const rf_collision_face_filter *generated,uint32_t material,float density,
    rf_random_state *random,uint32_t budget,rf_geomod_piece_bank **out,rf_geomod_subdivision_stats *stats)
{
    subdivision_work *work=NULL;rf_geomod_piece_bank *bank=NULL;rf_geomod_storage *storage=NULL;
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
            /* Temporary source IDs identify per-face filters. Preserve
             * the external surface identity separately across every subdivision. */
            for(i=0;i<mesh.face_count;i++){work->source_faces[i]=mesh.faces[i];work->source_ids[i]=mesh.faces[i].source_face;work->source_faces[i].source_face=i;}
            mesh.faces=work->source_faces;
            status=rf_geomod_storage_open(&mesh,1024,256,budget-resident,&storage);if(status)goto done;
            totals.attempts++;
            {
                uint32_t scratch_bytes,storage_bytes=rf_geomod_storage_bytes(storage),j;
                status=rf_geomod_storage_prepare_solid_cut(storage,&cutter,budget-resident-storage_bytes,&scratch_bytes);if(status)goto done;
                if(resident+storage_bytes+scratch_bytes>totals.peak_bytes)totals.peak_bytes=resident+storage_bytes+scratch_bytes;
                status=rf_geomod_storage_commit(storage);if(status)goto done;
                status=rf_geomod_storage_view(storage,&view.mesh);if(status)goto done;
                for(j=0;j<view.mesh.face_count;j++) {
                    uint32_t id=view.mesh.faces[j].source_face;
                    work->bound_filters[j]=id==UINT32_MAX?*generated:node->filters[id];
                }
                status=rf_geomod_collision_faces(&view.mesh,work->bound_filters,work->positions,1024,work->bound_faces,256);if(status)goto done;
                view.faces=work->bound_faces;
            }
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
                /* Storage owns a copy, so reusing a freed queue slot is safe. */
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
            rf_geomod_storage_close(&storage);
        }
    }
    *out=bank;bank=NULL;*random=next;*stats=totals;status=RF_OK;
done:
    rf_geomod_storage_close(&storage);rf_geomod_piece_bank_close(&bank);free(work);return status;
}
