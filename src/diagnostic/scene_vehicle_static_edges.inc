#ifndef RF_SCENE_VEHICLE_STATIC_EDGES_INCLUDED
#define RF_SCENE_VEHICLE_STATIC_EDGES_INCLUDED
#include "rf/geometry.h"
#include <math.h>
#include <string.h>
/* Fighter physics-only complement, not a change to reconstructed 4dec10:
 * that routine gates 5072e0 edges on a forward 5071b0 plane hit. A sphere
 * translating parallel to / away from the plane can still enter its edge.
 * Retain 499ed0's sphere transform, 4df1c0 room order and 4dec10 filters;
 * call the existing 5072e0 primitive without that plane gate. Already touching
 * or overlapping spheres use the finite edge's closest point at time zero:
 * pose rounding can put the entering root before 5072e0's -.05 clamp. Both
 * paths require radial approach, leaving retreat and tangency free.
 * No material lookup: this supplies response only.
 * Prepared world geometry stays immutable; calls sharing tree scratch must
 * be serialized. No allocation/global state. Errors preserve both outputs;
 * misses and ties preserve the existing contact. Scratch may change. Texture
 * query modes are unsupported, as in the untextured shared world sweep. */
static int scene_vehicle_static_edges_box(const float minimum[3],const float maximum[3],
    const float start[3],const float end[3],float radius,uint32_t *found)
{
    float lo[3],hi[3],point[3];
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(minimum[i])||!isfinite(maximum[i])||minimum[i]>maximum[i])return RF_FORMAT;
        lo[i]=minimum[i]-radius;hi[i]=maximum[i]+radius;
    }
    return rf_collision_segment_box(lo,hi,start,end,point,found);
}
static int scene_vehicle_static_edges_room(const rf_collision_room_view *room,
    const float lo[3],const float hi[3])
{
    for(uint32_t i=0;i<3;i++)if(lo[i]>room->maximum[i]||hi[i]<room->minimum[i])return 0;
    return 1;
}
static int scene_vehicle_static_edges_tree(const rf_collision_tree *tree,uint32_t room,
    uint32_t sphere,uint32_t flags,const float start[3],const float delta[3],const float end[3],
    float radius,float *limit,rf_geometry_body_hit *value,uint32_t *any)
{
    uint32_t used=0,visited=0,yes;int status;
    if((tree->node_count&&(!tree->nodes||!tree->stack||tree->node_capacity<tree->node_count))||
       (tree->face_count&&(!tree->faces||!tree->source_indices)))return RF_RANGE;
    if(tree->node_count)tree->stack[used++]=0;
    while(used){
        uint32_t index=tree->stack[--used];const rf_collision_node *node;
        if(index>=tree->node_count)return RF_RANGE;
        if(++visited>tree->node_count)return RF_FORMAT;
        node=tree->nodes+index;
        if(node->first_face>tree->face_count||node->face_count>tree->face_count-node->first_face||
           (node->left!=UINT32_MAX&&node->left>=tree->node_count)||
           (node->right!=UINT32_MAX&&node->right>=tree->node_count))return RF_RANGE;
        status=scene_vehicle_static_edges_box(node->minimum,node->maximum,start,end,radius,&yes);
        if(status)return status;if(!yes)continue;
        for(uint32_t f=0;f<node->face_count;f++){
            uint32_t face_index=node->first_face+f;const rf_collision_face *face=tree->faces+face_index;
            rf_collision_face_filter filter=face->filter;filter.query_flags=flags;
            status=rf_collision_face_accept(&filter,&yes);if(status)return status;if(!yes)continue;
            status=scene_vehicle_static_edges_box(face->minimum,face->maximum,start,end,radius,&yes);
            if(status)return status;if(!yes)continue;
            if(!face->vertices||face->count<3||face->count>65536)return RF_RANGE;
            for(uint32_t e=0;e<face->count;e++){
                const float *a=face->vertices[e],*b=face->vertices[(e+1)%face->count];
                float fraction,point[3];double normal[3],edge[3],closest[3],along=0,edge2=0,length2=0,approach=0;
                for(uint32_t j=0;j<3;j++){
                    if(!isfinite(a[j])||!isfinite(b[j]))return RF_FORMAT;
                    edge[j]=(double)b[j]-a[j];edge2+=edge[j]*edge[j];along+=((double)start[j]-a[j])*edge[j];
                }
                along=edge2>0?along/edge2:0;if(along<0)along=0;if(along>1)along=1;
                for(uint32_t j=0;j<3;j++){
                    closest[j]=a[j]+along*edge[j];normal[j]=(double)start[j]-closest[j];
                    length2+=normal[j]*normal[j];approach+=(double)delta[j]*normal[j];
                }
                if(length2<=(double)radius*radius){
                    if(!(*limit>0)||!(length2>0)||!(approach<0))continue;
                    fraction=0;for(uint32_t j=0;j<3;j++)point[j]=(float)closest[j];
                }else{
                    status=rf_collision_sphere_edge(start,delta,radius,a,b,*limit,&fraction,point,&yes);
                    if(status)return status;if(!yes)continue;
                    length2=approach=0;
                    for(uint32_t j=0;j<3;j++){
                        volatile float moved=delta[j]*fraction;volatile float center=start[j]+moved;
                        normal[j]=(double)center-point[j];
                        if(!isfinite(normal[j]))return RF_FORMAT;
                        length2+=normal[j]*normal[j];approach+=(double)delta[j]*normal[j];
                    }
                }
                if(!(length2>0)||!(approach<0))continue;
                double length=sqrt(length2);
                memset(value,0,sizeof(*value));value->contact.fraction=fraction;
                memcpy(value->contact.point,point,12);
                for(uint32_t j=0;j<3;j++)value->contact.normal[j]=(float)(normal[j]/length);
                value->contact.object_id=value->solid=UINT32_MAX;
                value->face=value->contact.face_token=tree->source_indices[face_index];
                value->room=room;value->sphere=sphere;value->hits=value->edge=1;
                *limit=fraction;*any=1;
                if(flags&1u)return RF_OK;
            }
        }
        /* Node faces, then right before left, matching the shared sweep. */
        if(node->left!=UINT32_MAX){if(used==tree->node_capacity)return RF_RANGE;tree->stack[used++]=node->left;}
        if(node->right!=UINT32_MAX){if(used==tree->node_capacity)return RF_RANGE;tree->stack[used++]=node->right;}
    }
    return RF_OK;
}
static int scene_vehicle_static_edges_compose(const rf_geometry_collision_world *world,
    const rf_collision_body_query *query,rf_geometry_body_hit *result,uint32_t *found)
{
    rf_geometry_body_hit value;float delta[3],limit;uint32_t any=0,flags;int status;
    if(!world||!query||!result||!found||(query->count&&!query->spheres)||
       (world->room_count&&!world->views)||(world->primary_count&&!world->primary)||
       (world->child_count&&!world->children))return RF_RANGE;
    if(!isfinite(query->limit)||query->limit<0||query->limit>1||
       !isfinite(query->radius)||query->radius<0)return RF_FORMAT;
    flags=(query->flags|4u)&~0x1000u; /* Only the solid pass; liquid stays in the common query. */
    if(query->radius<.05f)flags|=0x100u;
    if(flags&0x180u)return RF_NOT_FOUND;
    limit=query->limit;
    if(*found){
        if(!isfinite(result->contact.fraction)||result->contact.fraction<0||result->contact.fraction>query->limit)return RF_FORMAT;
        limit=result->contact.fraction;
    }
    for(uint32_t i=0;i<3;i++){
        if(!isfinite(query->start[i])||!isfinite(query->end[i]))return RF_FORMAT;
        delta[i]=query->end[i]-query->start[i];if(!isfinite(delta[i]))return RF_FORMAT;
        for(uint32_t j=0;j<3;j++)if(!isfinite(query->matrix[i][j]))return RF_FORMAT;
    }
    for(uint32_t s=0;s<query->count;s++){
        if(!isfinite(query->spheres[s].radius)||query->spheres[s].radius<0)return RF_FORMAT;
        for(uint32_t i=0;i<3;i++)if(!isfinite(query->spheres[s].center[i]))return RF_FORMAT;
    }
    for(uint32_t r=0;r<world->room_count;r++){
        const rf_collision_room_view *room=world->views+r;
        if(!room->tree||room->skip>255||room->first_child>world->child_count||
           room->child_count>world->child_count-room->first_child)return RF_RANGE;
        for(uint32_t i=0;i<3;i++)if(!isfinite(room->minimum[i])||!isfinite(room->maximum[i])||
            room->minimum[i]>room->maximum[i])return RF_FORMAT;
    }
    for(uint32_t i=0;i<world->primary_count;i++)if(world->primary[i]>=world->room_count)return RF_RANGE;
    for(uint32_t i=0;i<world->child_count;i++)if(world->children[i]>=world->room_count)return RF_RANGE;
    if(!limit||(delta[0]==0&&delta[1]==0&&delta[2]==0))return RF_OK;
    for(uint32_t s=0;s<query->count;s++){
        const rf_collision_body_sphere *sphere=query->spheres+s;float start[3],end[3],lo[3],hi[3];
        if(sphere->radius<.0001f)continue; /* Thin queries already have no sphere edge. */
        for(uint32_t i=0;i<3;i++){
            volatile float rotated=(float)(((long double)sphere->center[2]*query->matrix[2][i]+
                (long double)sphere->center[1]*query->matrix[1][i])+(long double)sphere->center[0]*query->matrix[0][i]);
            start[i]=rotated+query->start[i];end[i]=start[i]+delta[i];
            lo[i]=(start[i]<end[i]?start[i]:end[i])-sphere->radius;
            hi[i]=(start[i]>end[i]?start[i]:end[i])+sphere->radius;
            if(!isfinite(start[i])||!isfinite(end[i])||!isfinite(lo[i])||!isfinite(hi[i]))return RF_FORMAT;
        }
        for(uint32_t p=0;p<world->primary_count;p++){
            const rf_collision_room_view *parent=world->views+world->primary[p];
            if((parent->skip&&!(flags&8u))||!scene_vehicle_static_edges_room(parent,lo,hi))continue;
            for(uint32_t c=0;;c++){
                uint32_t r=c?world->children[parent->first_child+c-1]:world->primary[p];
                const rf_collision_room_view *room=world->views+r;
                if(scene_vehicle_static_edges_room(room,lo,hi)){
                    status=scene_vehicle_static_edges_tree(room->tree,r,s,flags,start,delta,end,
                        sphere->radius,&limit,&value,&any);if(status)return status;
                    if(any&&(flags&1u))goto done;
                }
                if(c==parent->child_count)break;
            }
        }
    }
 done:
    if(any){*result=value;*found=1;}return RF_OK;
}
#endif
