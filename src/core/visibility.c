#include "rf/visibility.h"
#include <math.h>
#include <string.h>
static int valid(const rf_visibility *s)
{
    return s && (!s->count || (s->rooms && s->order)) && s->visible_count<=s->count;
}
int rf_visibility_camera_setup(const rf_visibility_camera_parameters *p,rf_visibility_camera *out)
{
    rf_visibility_camera value;rf_visibility_view_scale scales;uint32_t i,j;int status;
    if(!p || !out || !isfinite(p->depth_offset))return RF_RANGE;
    status=rf_visibility_view_scale_build(&p->viewport,&scales);if(status)return status;
    value=*out;
    memcpy(value.view.origin,p->origin,sizeof(p->origin));memcpy(value.view.basis,p->basis,sizeof(p->basis));
    memcpy(value.view.scale,scales.scale,sizeof(scales.scale));
    value.view.near_distance=p->near_distance;value.view.far_distance=p->viewport.far_distance;
    value.view.perspective=p->viewport.perspective;value.view.far_enabled=p->far_enabled;
    status=rf_visibility_frustum_build(&value.view,&value.frustum);if(status)return status;
    memcpy(value.projection.origin,p->origin,sizeof(p->origin));
    for(i=0;i<3;i++)for(j=0;j<3;j++) {
        value.projection.matrix[i*3+j]=p->basis[i*3+j]*scales.scale[i];
        if(!isfinite(value.projection.matrix[i*3+j]))return RF_RANGE;
    }
    value.projection.flat_depth=scales.flat_depth;value.projection.perspective=p->viewport.perspective;
    value.projection.clip.enabled=p->clip_enabled;value.projection.clip.depth_enabled=p->viewport.perspective;
    value.projection.clip.far_enabled=p->far_enabled;value.projection.clip.far_distance=value.frustum.scaled_far;
    value.projection.projection.clamp=p->projection_clamp;value.projection.projection.depth_offset=p->depth_offset;
    value.projection.projection.half_width=scales.half[0];value.projection.projection.half_height=scales.half[1];
    value.projection.projection.origin_x=p->viewport.x;value.projection.projection.origin_y=p->viewport.y;
    *out=value;return RF_OK;
}
int rf_visibility_view_scale_build(const rf_visibility_viewport *v,rf_visibility_view_scale *out)
{
    rf_visibility_view_scale value;float aspect,factor,far;double depth;uint32_t i;
    if(!v || !out || v->width<=0 || v->height<=0 || !isfinite(v->pixel_aspect) || v->pixel_aspect<=0 ||
       !isfinite(v->fov) || v->fov<=0 || !isfinite(v->far_distance) || ((v->perspective&255u) && v->fov>=180))return RF_RANGE;
    aspect=(float)(((double)v->height*v->pixel_aspect)/v->width);
    value.half[0]=(float)((double)v->width*0.5);value.half[1]=(float)((double)v->height*0.5);
    value.center[0]=(float)((double)v->x+value.half[0]);value.center[1]=(float)((double)v->y+value.half[1]);
    if(!(v->perspective&255u))factor=(float)((double)v->fov*0.5);
    else factor=v->fov<2?v->fov:(float)tan((double)v->fov*0.01745329238474369049*0.5);
    far=v->far_distance<31.25f?31.25f:v->far_distance>=1000.0f?1000.0f:v->far_distance;
    value.flat_depth=0.9800000190734863f;
    depth=(v->perspective&255u)?(double)value.flat_depth/far:value.flat_depth;
    value.scale[2]=(float)depth;value.scale[0]=(float)(depth/factor);
    value.scale[1]=(float)(depth/((double)factor*aspect));
    value.inverse_depth_scale=(float)(1.0/depth);
    for(i=0;i<3;i++)if(!isfinite(value.scale[i]) || value.scale[i]<=0)return RF_RANGE;
    if(!isfinite(value.inverse_depth_scale))return RF_RANGE;
    *out=value;return RF_OK;
}
int rf_visibility_plane_normal(const float normal[3],const float point[3],rf_visibility_plane *plane)
{
    static const uint32_t corners[8]={4,0,5,1,7,3,6,2};
    rf_visibility_plane value;uint32_t i,bits=0;
    if(!normal || !point || !plane)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(normal[i]) || !isfinite(point[i]))return RF_RANGE;
        value.normal[i]=normal[i];bits=(bits<<1)|(normal[i]>0);
    }
    value.distance=(float)(-(((double)normal[2]*point[2]+(double)normal[1]*point[1])+(double)normal[0]*point[0]));
    if(!isfinite(value.distance))return RF_RANGE;
    value.corner=corners[bits];*plane=value;return RF_OK;
}
int rf_visibility_plane_points(const float a[3],const float b[3],const float c[3],rf_visibility_plane *plane)
{
    float first[3],second[3],normal[3];double inverse;uint32_t i;
    if(!a || !b || !c || !plane)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(a[i]) || !isfinite(b[i]) || !isfinite(c[i]))return RF_RANGE;
        first[i]=b[i]-a[i];second[i]=c[i]-b[i];
    }
    for(i=0;i<3;i++) {
        uint32_t j=(i+1)%3,k=(i+2)%3;
        normal[i]=(float)((double)first[j]*second[k]-(double)first[k]*second[j]);
    }
    inverse=1.0/sqrt(((double)normal[0]*normal[0]+(double)normal[1]*normal[1])+(double)normal[2]*normal[2]);
    if(!isfinite(inverse))return RF_RANGE;
    for(i=0;i<3;i++)normal[i]=(float)(normal[i]*inverse);
    return rf_visibility_plane_normal(normal,a,plane);
}
int rf_visibility_frustum_build(const rf_visibility_view *v,rf_visibility_frustum *out)
{
    rf_visibility_frustum value;float corners[4][3],normal[3],point[3],sx,sy;
    static const int signs[4][2]={{1,1},{-1,1},{-1,-1},{1,-1}};
    static const unsigned char edge[4][2]={{2,1},{0,3},{3,2},{1,0}};
    uint32_t i,j;int status;
    if(!v || !out || !isfinite(v->far_distance) || !isfinite(v->near_distance))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(v->origin[i]) || !isfinite(v->scale[i]) || v->scale[i]<=0)return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(v->basis[i]))return RF_RANGE;
    value=*out;value.scaled_far=v->far_distance*v->scale[2];value.scaled_near=v->near_distance*v->scale[2];
    sx=v->scale[2]/v->scale[0];sy=v->scale[2]/v->scale[1];
    if(!isfinite(sx) || !isfinite(sy) || !isfinite(value.scaled_far) || !isfinite(value.scaled_near))return RF_RANGE;
    for(i=0;i<4;i++)for(j=0;j<3;j++) {
        float y=v->basis[3+j]*(signs[i][1]>0?sy:-sy),x=v->basis[j]*(signs[i][0]>0?sx:-sx);
        corners[i][j]=((v->origin[j]+v->basis[6+j])+y)+x;
    }
    for(i=0;i<4;i++) {
        if(v->perspective&255u)status=rf_visibility_plane_points(v->origin,corners[edge[i][0]],corners[edge[i][1]],value.planes+i);
        else {
            uint32_t axis=i/2,corner=i==0||i==3?1:i==1?0:2;
            for(j=0;j<3;j++)normal[j]=(i&1u)?v->basis[axis*3+j]:-v->basis[axis*3+j];
            status=rf_visibility_plane_normal(normal,corners[corner],value.planes+i);
        }
        if(status)return status;value.masks[i]=4u<<i;
    }
    value.count=4;
    if(v->perspective&255u) {
        for(j=0;j<3;j++)normal[j]=-v->basis[6+j];
        status=rf_visibility_plane_normal(normal,v->origin,value.planes+4);if(status)return status;
        value.masks[4]=128;value.count=5;
        if(v->far_enabled&255u) {
            for(j=0;j<3;j++)point[j]=v->origin[j]+(float)(v->basis[6+j]*v->far_distance);
            status=rf_visibility_plane_normal(v->basis+6,point,value.planes+5);if(status)return status;
            value.masks[5]=2;value.count=6;
        }
    }
    *out=value;return RF_OK;
}
int rf_visibility_portal_classify(const float camera[3],const float minimum[3],const float maximum[3],
    const rf_visibility_plane *planes,uint32_t count,uint32_t *action)
{
    static const unsigned char high[8][3]={{1,1,0},{1,0,0},{0,0,0},{0,1,0},{1,1,1},{1,0,1},{0,0,1},{0,1,1}};
    uint32_t i,j;int inside=1;
    if(!camera || !minimum || !maximum || !action || (count && !planes))return RF_RANGE;
    for(i=0;i<3;i++) {
        float low,upper;
        if(!isfinite(camera[i]) || !isfinite(minimum[i]) || !isfinite(maximum[i]) || minimum[i]>maximum[i])return RF_RANGE;
        low=minimum[i]-1.0f;upper=maximum[i]+1.0f;
        if(camera[i]<low || camera[i]>upper)inside=0;
    }
    if(inside){*action=RF_PORTAL_FULL_VIEW;return RF_OK;}
    for(i=0;i<count;i++) {
        float point[3];double distance;
        if(planes[i].corner>=8 || !isfinite(planes[i].distance))return RF_RANGE;
        for(j=0;j<3;j++) {
            if(!isfinite(planes[i].normal[j]))return RF_RANGE;
            point[j]=high[planes[i].corner][j]?maximum[j]:minimum[j];
        }
        distance=((double)point[2]*planes[i].normal[2]+(double)point[1]*planes[i].normal[1])+
            (double)point[0]*planes[i].normal[0]+planes[i].distance;
        if(distance>0){*action=RF_PORTAL_REJECT;return RF_OK;}
    }
    *action=RF_PORTAL_PROJECT;return RF_OK;
}
int rf_visibility_begin_render(rf_visibility *s)
{
    uint32_t i;if(!valid(s))return RF_RANGE;
    for(i=0;i<s->count;i++)s->rooms[i].visible=0;
    return RF_OK;
}
int rf_visibility_begin_view(rf_visibility *s)
{
    uint32_t i;if(!valid(s))return RF_RANGE;
    for(i=0;i<s->count;i++){s->rooms[i].visited=0;s->rooms[i].depth=255;}
    s->visible_count=0;return RF_OK;
}
int rf_visibility_visit(rf_visibility *s,uint32_t index,const float rectangle[4],uint32_t depth)
{
    rf_room_visibility value;uint32_t i,position;
    if(!valid(s) || index>=s->count || !rectangle)return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(rectangle[i]))return RF_RANGE;
    value=s->rooms[index];position=s->visible_count;
    if(value.visited) {
        for(position=0;position<s->visible_count;position++)if(s->order[position]==index)break;
        if(position==s->visible_count)return RF_RANGE;
        for(i=0;i<4;i++)value.rectangle[i]=i<2?
            (rectangle[i]<value.rectangle[i]?rectangle[i]:value.rectangle[i]):
            (rectangle[i]>value.rectangle[i]?rectangle[i]:value.rectangle[i]);
    } else {
        if(s->visible_count==s->count)return RF_RANGE;
        memcpy(value.rectangle,rectangle,sizeof(value.rectangle));s->visible_count++;
    }
    for(i=position;i+1<s->visible_count;i++)s->order[i]=s->order[i+1];
    s->order[s->visible_count-1]=index;
    value.visible=1;value.visited=1;value.depth=depth;s->rooms[index]=value;
    return RF_OK;
}

int rf_visibility_portals_begin_view(rf_visibility_portal_cache *cache,uint32_t count)
{
    uint32_t i;if(count && !cache)return RF_RANGE;
    for(i=0;i<count;i++)cache[i].valid=0;
    return RF_OK;
}
static int portal_resolve(rf_visibility_portal_view *view,uint32_t index)
{
    rf_visibility_portal_cache *cache=view->cache+index;
    rf_visibility_portal value=view->portals[index];uint32_t action;int status;
    if(cache->valid)return RF_OK;
    status=rf_visibility_portal_classify(view->camera->view.origin,cache->minimum,cache->maximum,
        view->camera->frustum.planes,view->camera->frustum.count,&action);
    if(status)return status;
    if(action==RF_PORTAL_FULL_VIEW) {
        value.rejected=0;value.rectangle[0]=0;value.rectangle[1]=0;
        value.rectangle[2]=(float)view->width;value.rectangle[3]=(float)view->height;
    } else if(action==RF_PORTAL_REJECT)value.rejected=1;
    else {
        rf_visibility_screen_bounds result;
        result.visible=0;memcpy(result.rectangle,value.rectangle,sizeof(result.rectangle));
        status=rf_visibility_box_project(&view->camera->projection,cache->minimum,cache->maximum,&result);
        if(status)return status;
        value.rejected=!result.visible;memcpy(value.rectangle,result.rectangle,sizeof(value.rectangle));
    }
    view->portals[index]=value;cache->valid=1;return RF_OK;
}
static int traverse(rf_visibility *s,const rf_visibility_room_links *rooms,
    const uint32_t *links,uint32_t link_count,const rf_visibility_portal *portals,
    uint32_t portal_count,uint32_t start,uint32_t special,uint32_t flags,
    const float rectangle[4],rf_visibility_frame scratch[257],rf_visibility_portal_view *view)
{
    uint32_t i,j,top=0;
    if(!valid(s) || !rooms || !scratch || !rectangle || start>=s->count ||
       (link_count && !links) || (portal_count && !portals))return RF_RANGE;
    for(i=0;i<4;i++)if(!isfinite(rectangle[i]))return RF_RANGE;
    for(i=0;i<s->count;i++) {
        if(rooms[i].first>link_count || rooms[i].count>link_count-rooms[i].first)return RF_RANGE;
        for(j=0;j<rooms[i].count;j++) {
            uint32_t p=links[rooms[i].first+j];
            if(p>=portal_count || (portals[p].rooms[0]!=i && portals[p].rooms[1]!=i))return RF_RANGE;
        }
    }
    for(i=0;i<portal_count;i++) {
        if(portals[i].rooms[0]>=s->count || portals[i].rooms[1]>=s->count)return RF_RANGE;
        for(j=0;j<4;j++)if(!isfinite(portals[i].rectangle[j]))return RF_RANGE;
    }
    scratch[0].room=start;scratch[0].depth=0;scratch[0].cursor=UINT32_MAX;
    memcpy(scratch[0].rectangle,rectangle,16);
    for(;;) {
        rf_visibility_frame *f=scratch+top;const rf_visibility_room_links *r=rooms+f->room;
        if(f->cursor==UINT32_MAX) {
            int status;
            f->cursor=0;
            if(r->blocked)f->cursor=r->count;
            else {
                status=rf_visibility_visit(s,f->room,f->rectangle,f->depth);if(status)return status;
                if((flags&1u) || (r->detail && f->room!=special))f->cursor=r->count;
            }
        }
        if(f->cursor==r->count) {
            if(!top)return RF_OK;
            s->rooms[f->room].depth=255;--top;continue;
        }
        {
            uint32_t index=links[r->first+f->cursor++];
            const rf_visibility_portal *p=portals+index;
            uint32_t next=p->rooms[p->rooms[0]==f->room?1:0];float clip[4];
            if((int32_t)s->rooms[f->room].depth>(int32_t)s->rooms[next].depth)continue;
            if(view){int status=portal_resolve(view,index);if(status)return status;}
            if(p->rejected)continue;
            for(i=0;i<4;i++)clip[i]=i<2?
                (f->rectangle[i]>p->rectangle[i]?f->rectangle[i]:p->rectangle[i]):
                (f->rectangle[i]<p->rectangle[i]?f->rectangle[i]:p->rectangle[i]);
            if(!(clip[0]<clip[2] && clip[1]<clip[3]))continue;
            if(top==256)return RF_RANGE;
            scratch[top+1].room=next;scratch[top+1].depth=f->depth+1;scratch[top+1].cursor=UINT32_MAX;
            memcpy(scratch[++top].rectangle,clip,16);
        }
    }
}
int rf_visibility_traverse(rf_visibility *s,const rf_visibility_room_links *rooms,
    const uint32_t *links,uint32_t link_count,const rf_visibility_portal *portals,
    uint32_t portal_count,uint32_t start,uint32_t special,uint32_t flags,
    const float rectangle[4],rf_visibility_frame scratch[257])
{
    return traverse(s,rooms,links,link_count,portals,portal_count,start,special,flags,rectangle,scratch,NULL);
}
int rf_visibility_traverse_projected(rf_visibility *s,const rf_visibility_room_links *rooms,
    const uint32_t *links,uint32_t link_count,rf_visibility_portal_view *view,
    uint32_t start,uint32_t special,uint32_t flags,const float rectangle[4],rf_visibility_frame scratch[257])
{
    if(!view || !view->camera || (view->count && (!view->cache || !view->portals)) ||
       view->width<=0 || view->height<=0 || view->camera->frustum.count>6)return RF_RANGE;
    return traverse(s,rooms,links,link_count,view->portals,view->count,start,special,flags,rectangle,scratch,view);
}
