#include "rf/authored_reset_placement.h"
#include <math.h>

int rf_authored_reset_standing_check(const rf_collision_tree *original,
    const float (*planes)[4],uint32_t count,const rf_checkpoint_placement *p,
    float dt,float speed,rf_checkpoint_placement_result *out)
{
    rf_geomod_terrain_view full={0};rf_checkpoint_placement_result result;
    uint32_t i,j,k;int status;
    if(!original || !original->faces || !original->face_count || !planes || !count || count>32 || !p)return RF_RANGE;
    for(i=0;i<count;i++) {
        double norm=0;
        for(k=0;k<4;k++)if(!isfinite(planes[i][k]))return RF_FORMAT;
        for(k=0;k<3;k++)norm+=(double)planes[i][k]*planes[i][k];
        if(norm<.999 || norm>1.001)return RF_FORMAT;
    }
    /* This API consumes face views/count, not a convex-cavity terrain object.
     * Supplying the full immutable room preserves the surrounding solid union. */
    full.faces=original->faces;full.tree=original;full.mesh.face_count=original->face_count;
    status=rf_checkpoint_standing_check(&full,p,dt,speed,&result);
    if(status){if(status==RF_NOT_FOUND && out)*out=result;return status;}
    for(i=0;i<p->count;i++) {
        double center[3];uint32_t inside=1;
        for(k=0;k<3;k++)center[k]=(double)p->position[k]+(double)p->spheres[i].center[0]*p->basis[k]+
            (double)p->spheres[i].center[1]*p->basis[3+k]+(double)p->spheres[i].center[2]*p->basis[6+k];
        for(j=0;j<count;j++) {
            double distance=planes[j][3];
            for(k=0;k<3;k++)distance+=center[k]*planes[j][k];
            if(distance>0){inside=0;break;}
        }
        if(inside){result.sphere=i;result.reason=RF_CHECKPOINT_PLACEMENT_SOLID;if(out)*out=result;return RF_NOT_FOUND;}
    }
    if(out)*out=result;return RF_OK;
}
