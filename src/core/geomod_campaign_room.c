#include "rf/geomod_campaign_room.h"
#include <math.h>
#include <string.h>

int rf_geomod_campaign_room_scope_check(const rf_geometry *geometry,uint32_t room,
    const rf_collision_face *cutter,uint32_t cutter_count,
    rf_geometry_portal *portal_work,uint32_t portal_capacity,
    rf_geomod_campaign_room_scope *out)
{
    rf_geomod_campaign_room_scope result={0};float lo[3],hi[3];
    uint32_t i,j,k,portal_count;int status;
    if(!geometry || !geometry->data || room>=geometry->rooms || !cutter ||
       !cutter_count || cutter_count>64 || !portal_work || !portal_capacity || !out)return RF_RANGE;
    for(k=0;k<3;k++)lo[k]=cutter[0].minimum[k],hi[k]=cutter[0].maximum[k];
    for(i=0;i<cutter_count;i++)for(k=0;k<3;k++){
        if(!isfinite(cutter[i].minimum[k]) || !isfinite(cutter[i].maximum[k]) ||
           cutter[i].minimum[k]>cutter[i].maximum[k])return RF_FORMAT;
        if(cutter[i].minimum[k]<lo[k])lo[k]=cutter[i].minimum[k];
        if(cutter[i].maximum[k]>hi[k])hi[k]=cutter[i].maximum[k];
    }
    for(i=0;i<geometry->faces;i++){
        rf_geometry_face face;float flo[3],fhi[3];uint32_t overlaps=1;
        status=rf_geometry_get_face(geometry,i,&face);if(status)return status;
        if(face.room==room || face.room==UINT32_MAX)continue;
        if(face.corners<3 || face.corners>64)return RF_FORMAT;
        for(j=0;j<face.corners;j++){
            rf_geometry_corner corner;float position[3];
            status=rf_geometry_get_corner(geometry,i,j,&corner);if(status)return status;
            status=rf_geometry_vertex(geometry,corner.vertex,position);if(status)return status;
            for(k=0;k<3;k++){
                if(!isfinite(position[k]))return RF_FORMAT;
                if(!j)flo[k]=fhi[k]=position[k];
                if(position[k]<flo[k])flo[k]=position[k];
                if(position[k]>fhi[k])fhi[k]=position[k];
            }
        }
        for(k=0;k<3;k++)if(fhi[k]<lo[k] || flo[k]>hi[k])overlaps=0;
        if(overlaps){
            if(face.flags&8)result.other_detail_faces++;
            else result.other_solid_faces++;
        }
    }
    status=rf_geometry_portals(geometry,NULL,0,&portal_count);if(status)return status;
    if(portal_count>portal_capacity)return RF_RANGE;
    status=rf_geometry_portals(geometry,portal_work,portal_capacity,&portal_count);
    if(status)return status;
    for(i=0;i<portal_count;i++){
        uint32_t overlaps=1;const rf_geometry_portal *portal=portal_work+i;
        if(portal->rooms[0]!=room && portal->rooms[1]!=room)continue;
        result.portal_links++;
        for(k=0;k<3;k++)if(portal->maximum[k]<lo[k] || portal->minimum[k]>hi[k])overlaps=0;
        result.touching_portals+=overlaps;
    }
    *out=result;return RF_OK;
}

int rf_geomod_campaign_room_retain(const rf_geometry *geometry,uint32_t room,
    const rf_collision_face *cutter,uint32_t cutter_count,
    rf_geomod_solid_clip_work *work,rf_geomod_vertex *vertices,
    uint32_t vertex_capacity,rf_geomod_face *faces,uint8_t *unchanged,uint32_t face_capacity,
    rf_geomod_mesh_view *out)
{
    rf_geomod_mesh_view result;float cut_lo[3],cut_hi[3];
    uint32_t i,j,k,nv=0,nf=0;int status;
    if(!geometry || !geometry->data || room>=geometry->rooms || !cutter ||
       !cutter_count || cutter_count>64 || !work || !vertices || !faces || !unchanged || !out ||
       !vertex_capacity || !face_capacity)return RF_RANGE;
    for(k=0;k<3;k++)cut_lo[k]=cutter[0].minimum[k],cut_hi[k]=cutter[0].maximum[k];
    for(i=0;i<cutter_count;i++)for(k=0;k<3;k++){
        if(!isfinite(cutter[i].minimum[k]) || !isfinite(cutter[i].maximum[k]) ||
           cutter[i].minimum[k]>cutter[i].maximum[k])return RF_FORMAT;
        if(cutter[i].minimum[k]<cut_lo[k])cut_lo[k]=cutter[i].minimum[k];
        if(cutter[i].maximum[k]>cut_hi[k])cut_hi[k]=cutter[i].maximum[k];
    }
    for(i=0;i<geometry->faces;i++){
        rf_geometry_face original;rf_geomod_vertex polygon[64];
        rf_geomod_solid_clip_result clipped;rf_geomod_fragment whole;uint32_t near=1;
        float lo[3],hi[3];
        status=rf_geometry_get_face(geometry,i,&original);if(status)return status;
        if(original.room!=room)continue;
        if(original.corners<3 || original.corners>64)return RF_FORMAT;
        for(j=0;j<original.corners;j++){
            rf_geometry_corner corner;
            status=rf_geometry_get_corner(geometry,i,j,&corner);if(status)return status;
            status=rf_geometry_vertex(geometry,corner.vertex,polygon[j].position);if(status)return status;
            memcpy(polygon[j].uv,corner.uv,sizeof(corner.uv));
            for(k=0;k<3;k++){
                float p=polygon[j].position[k];
                if(!isfinite(p))return RF_FORMAT;
                if(!j)lo[k]=hi[k]=p;
                if(p<lo[k])lo[k]=p;if(p>hi[k])hi[k]=p;
            }
            for(k=0;k<2;k++)if(!isfinite(polygon[j].uv[k]))return RF_FORMAT;
        }
        for(k=0;k<3;k++)if(hi[k]<cut_lo[k] || lo[k]>cut_hi[k])near=0;
        if(near){
            status=rf_geomod_polygon_clip_outside_solid_bounded(polygon,original.corners,
                cutter,cutter_count,work,&clipped);if(status)return status;
        } else {
            whole=(rf_geomod_fragment){0,original.corners};
            clipped=(rf_geomod_solid_clip_result){polygon,&whole,original.corners,1,NULL};
        }
        for(j=0;j<clipped.fragment_count;j++){
            rf_geomod_fragment piece=clipped.fragments[j];uint32_t count=piece.count;
            uint32_t same=!near;
            if(count<3 || piece.first>clipped.vertex_count ||
               count>clipped.vertex_count-piece.first ||
               count>vertex_capacity-nv || nf==face_capacity)return RF_RANGE;
            if(near && clipped.fragment_count==1 && count==original.corners &&
               !memcmp(clipped.vertices+piece.first,polygon,count*sizeof(*polygon)))same=1;
            if(same){
                memcpy(vertices+nv,clipped.vertices+piece.first,count*sizeof(*vertices));
                faces[nf]=(rf_geomod_face){nv,count,original.texture,i};
                unchanged[nf++]=1;nv+=count;
            } else {
                rf_geomod_vertex repaired_vertices[256];rf_geomod_face repaired_faces[64];
                rf_geomod_face single={0,count,original.texture,i};
                rf_geomod_mesh_view input={clipped.vertices+piece.first,&single,count,1,0},repaired;
                uint32_t child;
                status=rf_geomod_partition_mesh(&input,repaired_vertices,256,repaired_faces,64,&repaired);
                if(status)return status;
                if(repaired.face_count>face_capacity-nf ||
                   repaired.vertex_count>vertex_capacity-nv)return RF_RANGE;
                memcpy(vertices+nv,repaired.vertices,repaired.vertex_count*sizeof(*vertices));
                for(child=0;child<repaired.face_count;child++){
                    faces[nf]=repaired.faces[child];faces[nf].first+=nv;
                    unchanged[nf++]=0;
                }
                nv+=repaired.vertex_count;
            }
        }
    }
    result=(rf_geomod_mesh_view){vertices,faces,nv,nf,0};
    *out=result;return RF_OK;
}

int rf_geomod_campaign_room_stage(const rf_geometry *geometry,
    const rf_geomod_mesh_view *retained,const uint8_t *unchanged,
    const rf_geomod_mesh_view *walls,const uint32_t *texture_slots,
    uint32_t texture_count,uint32_t wall_material,uint32_t material_capacity,
    rf_collision_face_filter generated_filter,
    uint32_t generated_face_id,rf_geomod_campaign_room_stage_work *work,
    uint32_t tree_budget,rf_geomod_mesh_view *mesh,rf_collision_tree *tree)
{
    rf_geomod_mesh_view staged;rf_collision_tree built={0};
    uint32_t i,nv,nf;int status;
    if(!geometry || !geometry->data || !retained || !walls || !unchanged || !work ||
       !work->vertices || !work->faces || !work->filters || !work->collision_faces ||
       !work->positions || !work->face_ids || !mesh || !tree || tree->storage ||
       !retained->vertices || !retained->faces || !walls->vertices || !walls->faces ||
       !texture_slots || texture_count<geometry->textures ||
       !material_capacity || wall_material>=material_capacity ||
       generated_face_id>=geometry->faces || !tree_budget)return RF_RANGE;
    if(retained->vertex_count>work->vertex_capacity ||
       walls->vertex_count>work->vertex_capacity-retained->vertex_count ||
       retained->face_count>work->face_capacity ||
       walls->face_count>work->face_capacity-retained->face_count)return RF_RANGE;
    nv=retained->vertex_count+walls->vertex_count;
    nf=retained->face_count+walls->face_count;
    for(i=0;i<retained->face_count;i++){
        const rf_geomod_face *f=retained->faces+i;
        if(f->count<3 || f->first>retained->vertex_count ||
           f->count>retained->vertex_count-f->first ||
           f->source_face>=geometry->faces || f->material>=geometry->textures ||
           texture_slots[f->material]>=material_capacity)return RF_FORMAT;
    }
    for(i=0;i<walls->face_count;i++){
        const rf_geomod_face *f=walls->faces+i;
        if(f->count<3 || f->first>walls->vertex_count ||
           f->count>walls->vertex_count-f->first ||
           f->source_face!=UINT32_MAX)return RF_FORMAT;
    }
    memcpy(work->vertices,retained->vertices,retained->vertex_count*sizeof(*work->vertices));
    memcpy(work->vertices+retained->vertex_count,walls->vertices,
        walls->vertex_count*sizeof(*work->vertices));
    memcpy(work->faces,retained->faces,retained->face_count*sizeof(*work->faces));
    for(i=0;i<retained->face_count;i++)
        work->faces[i].material=texture_slots[retained->faces[i].material];
    for(i=0;i<walls->face_count;i++){
        work->faces[retained->face_count+i]=walls->faces[i];
        work->faces[retained->face_count+i].first+=retained->vertex_count;
        work->faces[retained->face_count+i].material=wall_material;
    }
    staged=(rf_geomod_mesh_view){work->vertices,work->faces,nv,nf,0};
    for(i=0;i<nf;i++){
        const rf_geomod_face *f=work->faces+i;
        if(i<retained->face_count){
            status=rf_geometry_initial_collision_filter(geometry,f->source_face,0,
                work->filters+i);
            work->face_ids[i]=f->source_face;
        }else{
            work->filters[i]=generated_filter;work->face_ids[i]=generated_face_id;
            status=RF_OK;
        }
        if(status)return status;
        if(i<retained->face_count && unchanged[i]){
            status=rf_geometry_collision_face(geometry,f->source_face,work->filters+i,
                work->positions+f->first,f->count,work->collision_faces+i);
        }else{
            rf_geomod_face local=*f;rf_geomod_mesh_view one;
            local.first=0;
            one=(rf_geomod_mesh_view){staged.vertices+f->first,&local,f->count,1,0};
            status=rf_geomod_collision_faces(&one,work->filters+i,
                work->positions+f->first,f->count,work->collision_faces+i,1);
        }
        if(status)return status;
    }
    status=rf_collision_tree_open(work->collision_faces,nf,tree_budget,&built);
    if(status)return status;
    *mesh=staged;*tree=built;return RF_OK;
}
