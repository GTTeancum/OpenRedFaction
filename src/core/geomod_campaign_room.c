#include "rf/geomod_campaign_room.h"
#include <math.h>
#include <string.h>

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
