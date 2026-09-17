#include "rf/debris_visibility.h"
#include "rf/vpp.h"
#include <math.h>
int rf_debris_room_rectangle(const rf_visibility_view *view,float width,float height,
    const float rectangle[4],rf_visibility_frustum *active)
{
    static const unsigned char xy[4][2]={{2,1},{0,1},{0,3},{2,3}};
    static const unsigned char edge[4][2]={{2,1},{0,3},{3,2},{1,0}};
    rf_visibility_frustum value;float points[4][3];uint32_t i,j;int status;
    if(!view || !rectangle || !active || !(view->perspective&255u) ||
       !isfinite(width) || !isfinite(height) || width<=0 || height<=0 ||
       active->count<4 || active->count>6)return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(rectangle[i]))return RF_RANGE;
    if(rectangle[0]>=rectangle[2] || rectangle[1]>=rectangle[3])return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(view->origin[i]) || !isfinite(view->scale[i]) || view->scale[i]<=0)return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(view->basis[i]))return RF_RANGE;
    value=*active;
    for(i=0;i<4;i++) {
        float ray[3];double inverse;
        ray[0]=((rectangle[xy[i][0]]-width*.5f)/(width*.5f))*(view->scale[2]/view->scale[0]);
        ray[1]=-((rectangle[xy[i][1]]-height*.5f)/(height*.5f))*(view->scale[2]/view->scale[1]);
        ray[2]=1;
        inverse=1.0/sqrt(((double)ray[0]*ray[0]+(double)ray[1]*ray[1])+(double)ray[2]*ray[2]);
        if(!isfinite(inverse) || inverse==0)return RF_RANGE;
        for(j=0;j<3;j++)ray[j]=(float)(ray[j]*inverse);
        for(j=0;j<3;j++)points[i][j]=view->origin[j]+(float)(((double)ray[0]*view->basis[j]+
            (double)ray[1]*view->basis[3+j])+(double)ray[2]*view->basis[6+j]);
    }
    for(i=0;i<4;i++) {
        status=rf_visibility_plane_points(view->origin,points[edge[i][0]],points[edge[i][1]],value.planes+i);
        if(status)return status;value.masks[i]=4u<<i;
    }
    *active=value;return RF_OK;
}
int rf_debris_room_render_admit(const rf_visibility_plane *planes,uint32_t count,
    const float minimum[3],const float maximum[3],uint32_t member,uint32_t *admitted)
{
    static const unsigned char high[8][3]={{1,1,0},{1,0,0},{0,0,0},{0,1,0},{1,1,1},{1,0,1},{0,0,1},{0,1,1}};
    uint32_t i,j,result=member;
    if(!minimum || !maximum || !admitted || member>1 || count>6 || (count && !planes))return RF_RANGE;
    for(j=0;j<3;j++)if(!isfinite(minimum[j]) || !isfinite(maximum[j]) || minimum[j]>maximum[j])return RF_RANGE;
    for(i=0;i<count;i++){
        if(planes[i].corner>=8 || !isfinite(planes[i].distance))return RF_RANGE;
        for(j=0;j<3;j++)if(!isfinite(planes[i].normal[j]))return RF_RANGE;
    }
    for(i=0;i<count;i++){
        float p[3];double distance;
        for(j=0;j<3;j++)p[j]=high[planes[i].corner][j]?maximum[j]:minimum[j];
        /* Original40a0b0 Z+Y+X accumulation followed by4163a0 distance. */
        distance=((double)p[2]*planes[i].normal[2]+(double)p[1]*planes[i].normal[1])+
            (double)p[0]*planes[i].normal[0]+planes[i].distance;
        if(distance>0)result=0;
    }
    *admitted=result;return RF_OK;
}
