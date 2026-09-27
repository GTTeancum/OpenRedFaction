#include "rf/geomod_campaign_wall.h"

int rf_geomod_campaign_wall_build(const rf_geomod_mesh_view *cutter,
    const rf_geomod_solid_clip_source *air,uint32_t air_count,
    rf_geomod_solid_union_work *work,rf_geomod_vertex *vertices,
    uint32_t vertex_capacity,rf_geomod_face *faces,uint32_t face_capacity,
    rf_geomod_mesh_view *out)
{
    rf_geomod_mesh_view result;uint32_t i,j,nv=0,nf=0;int status;
    if(!cutter || !cutter->vertices || !cutter->faces || !cutter->face_count ||
       !work || !vertices || !faces || !out || !vertex_capacity || !face_capacity ||
       (air_count && !air))return RF_RANGE;
    for(i=0;i<cutter->face_count;i++){
        const rf_geomod_face *input=cutter->faces+i;
        rf_geomod_solid_clip_result clipped;
        if(input->count<3 || input->count>64 || input->first>cutter->vertex_count ||
           input->count>cutter->vertex_count-input->first || input->material==UINT32_MAX)return RF_FORMAT;
        status=rf_geomod_polygon_clip_outside_union(cutter->vertices+input->first,
            input->count,air,air_count,work,&clipped);
        if(status)return status;
        for(j=0;j<clipped.fragment_count;j++){
            const rf_geomod_fragment *fragment=clipped.fragments+j;
            uint32_t k;
            if(fragment->count<3 || fragment->first>clipped.vertex_count ||
               fragment->count>clipped.vertex_count-fragment->first)return RF_FORMAT;
            if(nf==face_capacity || fragment->count>vertex_capacity-nv)return RF_RANGE;
            /* Cutter normals point out of removed material. The surviving
             * rock boundary points back into the new cavity. */
            for(k=0;k<fragment->count;k++)
                vertices[nv+k]=clipped.vertices[fragment->first+fragment->count-1-k];
            faces[nf++]=(rf_geomod_face){nv,fragment->count,input->material,UINT32_MAX};
            nv+=fragment->count;
        }
    }
    result=(rf_geomod_mesh_view){vertices,faces,nv,nf,cutter->generation};
    *out=result;return RF_OK;
}
