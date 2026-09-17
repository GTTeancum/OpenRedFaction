#include "rf/checkpoint_placement.h"
#include <float.h>
#include <math.h>
#include <string.h>
#ifndef RF_IMAGE_XBOX_NATIVE
#include <stdio.h>
#include <stdlib.h>
#endif
typedef struct placement_work {
    const rf_geomod_terrain_view *candidate;const rf_checkpoint_placement *placement;
    double minimum[3],maximum[3],center[3],distance;
    rf_collision_room_query query;uint32_t mode,retry,faces,replaced;
    rf_physics_ground_probe ground;float fraction,normal[3];
    const rf_collision_face *classification_face;
} placement_work;
static double dot3(const double a[3],const double b[3])
{return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static int basis_valid(const float *b)
{
    uint32_t i,j,k;double determinant;
    for(i=0;i<9;i++)if(!isfinite(b[i]))return 0;
    for(i=0;i<3;i++)for(j=i;j<3;j++){
        double d=0;for(k=0;k<3;k++)d+=(double)b[i*3+k]*b[j*3+k];
        if(fabs(d-(i==j?1:0))>1e-4)return 0;
    }
    determinant=(double)b[0]*((double)b[4]*b[8]-(double)b[5]*b[7])-
        (double)b[1]*((double)b[3]*b[8]-(double)b[5]*b[6])+
        (double)b[2]*((double)b[3]*b[7]-(double)b[4]*b[6]);
    return fabs(determinant-1)<1e-4;
}
static int face_distance(const rf_collision_face *f,const double *center,double *minimum)
{
    double d=f->plane[3],norm=0,best=DBL_MAX;float projected[3];uint32_t i,k,inside;int status;
    for(k=0;k<3;k++){d+=center[k]*f->plane[k];norm+=(double)f->plane[k]*f->plane[k];}
    for(k=0;k<3;k++){
        double p=center[k]-d*f->plane[k]/norm;
        if(!isfinite(p)||fabs(p)>FLT_MAX)return RF_RANGE;projected[k]=(float)p;
    }
    status=rf_collision_polygon_contains(f->plane,projected,f->vertices,f->count,&inside);if(status)return status;
    if(inside)best=d*d/norm;
    for(i=0;i<f->count;i++){
        const float *a=f->vertices[i],*b=f->vertices[(i+1)%f->count];double edge[3],offset[3],delta[3],length,t,ds;
        for(k=0;k<3;k++){edge[k]=(double)b[k]-a[k];offset[k]=center[k]-a[k];}
        length=dot3(edge,edge);t=length?dot3(offset,edge)/length:0;
        if(t<0)t=0;if(t>1)t=1;
        for(k=0;k<3;k++)delta[k]=offset[k]-t*edge[k];ds=dot3(delta,delta);
        if(ds<best)best=ds;
    }
    if(best<*minimum)*minimum=best;return RF_OK;
}
static int visit_faces(placement_work *w,const rf_collision_face *faces,uint32_t count)
{
    uint32_t i,j,k,accepted;int status;
    if(count&&!faces)return RF_RANGE;
    for(i=0;i<count;i++){
        const rf_collision_face *f=faces+i;rf_collision_face_filter filter=f->filter;
        filter.query_flags=w->placement->query_flags;
        status=rf_collision_face_accept(&filter,&accepted);if(status)return status;if(!accepted)continue;
        if(w->mode==0){
            double norm=0;
            if(!f->vertices||f->count<3||f->count>65536)return RF_FORMAT;
            for(k=0;k<4;k++)if(!isfinite(f->plane[k]))return RF_FORMAT;
            for(k=0;k<3;k++){
                norm+=(double)f->plane[k]*f->plane[k];
                if(!isfinite(f->minimum[k])||!isfinite(f->maximum[k])||f->minimum[k]>f->maximum[k])return RF_FORMAT;
                if(f->minimum[k]<w->minimum[k])w->minimum[k]=f->minimum[k];
                if(f->maximum[k]>w->maximum[k])w->maximum[k]=f->maximum[k];
            }
            if(norm<.999||norm>1.001)return RF_FORMAT;
            for(j=0;j<f->count;j++)for(k=0;k<3;k++)if(!isfinite(f->vertices[j][k]))return RF_FORMAT;
            if(w->faces==UINT32_MAX)return RF_RANGE;w->faces++;
        }else if(w->mode==1){status=face_distance(f,w->center,&w->distance);if(status)return status;}
        else if(w->mode==3){
            rf_collision_face face=*f;rf_collision_sweep_hit hit;float start[3],delta[3];uint32_t matched;
            face.filter.query_flags=w->ground.query_flags;
            for(k=0;k<3;k++){start[k]=(float)((double)w->ground.start[k]+w->ground.sphere.center[k]);delta[k]=(float)((double)w->ground.end[k]-w->ground.start[k]);}
            status=rf_collision_sweep_face(&face,start,delta,delta,w->ground.sphere.radius,w->fraction,&hit,&matched);if(status)return status;
            if(matched){w->fraction=hit.hit.fraction;memcpy(w->normal,hit.hit.normal,12);}
        }else{
            uint32_t retry,previous_hits=w->query.hits;
            status=rf_collision_room_query_face(&w->query,f,1,&retry);if(status)return status;
            if(w->query.hits!=previous_hits)w->classification_face=f;
            if(retry)w->retry=1;
        }
    }
    return RF_OK;
}
static int visit_world(placement_work *w)
{
    const rf_geometry_collision_world *world=w->placement->world;uint32_t i,j;int status;
    for(i=0;i<world->primary_count;i++){
        uint32_t parent_id=world->primary[i];const rf_collision_room_view *parent=world->views+parent_id;
        if(parent->skip&&!(w->placement->query_flags&8u))continue;
        for(j=0;;j++){
            uint32_t id=j?world->children[parent->first_child+j-1]:parent_id;
            if(id==w->placement->replaced_room){w->replaced=1;status=visit_faces(w,w->candidate->faces,w->candidate->mesh.face_count);}
            else{const rf_collision_tree *tree=world->views[id].tree;status=visit_faces(w,tree->faces,tree->face_count);}
            if(status)return status;if(j==parent->child_count)break;
        }
    }
    return RF_OK;
}
int rf_checkpoint_placement_check(const rf_geomod_terrain_view *candidate,const rf_checkpoint_placement *p,rf_checkpoint_placement_result *out)
{
    placement_work w;rf_checkpoint_placement_result result={UINT32_MAX,RF_CHECKPOINT_PLACEMENT_FITS,0};
    const rf_geometry_collision_world *world;uint32_t i,j,attempt;int status;
    if(!candidate||!p||!p->world||!p->spheres||!p->count||p->count>8)return RF_RANGE;
    world=p->world;
    if(p->replaced_room>=world->room_count||!world->views||!world->primary_count||!world->primary||
        (world->child_count&&!world->children)||(p->query_flags&0x1180u))return RF_RANGE;
    if(!basis_valid(p->basis))return RF_FORMAT;
    for(i=0;i<3;i++)if(!isfinite(p->position[i]))return RF_FORMAT;
    for(i=0;i<p->count;i++){
        if(!isfinite(p->spheres[i].radius)||p->spheres[i].radius<=.002)return RF_FORMAT;
        for(j=0;j<3;j++)if(!isfinite(p->spheres[i].center[j]))return RF_FORMAT;
    }
    for(i=0;i<world->room_count;i++){
        const rf_collision_room_view *r=world->views+i;
        if(!r->tree||r->skip>255||r->first_child>world->child_count||r->child_count>world->child_count-r->first_child)return RF_RANGE;
    }
    for(i=0;i<world->primary_count;i++)if(world->primary[i]>=world->room_count)return RF_RANGE;
    for(i=0;i<world->child_count;i++)if(world->children[i]>=world->room_count)return RF_RANGE;
    memset(&w,0,sizeof(w));w.candidate=candidate;w.placement=p;
    for(i=0;i<3;i++){w.minimum[i]=DBL_MAX;w.maximum[i]=-DBL_MAX;}
    status=visit_world(&w);if(status)return status;if(!w.replaced||!w.faces)return RF_RANGE;
    for(i=0;i<p->count;i++){
        double length=1,r=(double)p->spheres[i].radius-.002;float direction[3]={0,1,0},cosine=.9753f;
        result.sphere=i;
        for(j=0;j<3;j++){
            w.center[j]=(double)p->position[j]+(double)p->spheres[i].center[0]*p->basis[j]+
                (double)p->spheres[i].center[1]*p->basis[3+j]+(double)p->spheres[i].center[2]*p->basis[6+j];
            if(!isfinite(w.center[j])||fabs(w.center[j])>FLT_MAX)return RF_RANGE;
            length+=fmax(fabs(w.center[j]-w.minimum[j]),fabs(w.center[j]-w.maximum[j]));
        }
        if(!isfinite(length)||length>FLT_MAX)return RF_RANGE;
        for(j=0;j<3;j++)if(w.center[j]<w.minimum[j] || w.center[j]>w.maximum[j]) {
            result.reason=RF_CHECKPOINT_PLACEMENT_SOLID;goto rejected;
        }
        w.mode=1;w.distance=DBL_MAX;status=visit_world(&w);if(status)return status;
        if(w.distance<r*r){result.reason=RF_CHECKPOINT_PLACEMENT_SURFACE;goto rejected;}
        for(attempt=0;attempt<16;attempt++){
            status=rf_collision_room_direction(direction,cosine,direction);if(status)return status;
            memset(&w.query,0,sizeof(w.query));w.retry=0;w.classification_face=NULL;
            for(j=0;j<3;j++){
                volatile float part=direction[j]*(float)length;
                w.query.start[j]=(float)w.center[j];w.query.direction[j]=direction[j];w.query.endpoint[j]=w.query.start[j]+part;
                if(!isfinite(w.query.endpoint[j]))return RF_RANGE;
            }
            w.mode=2;status=visit_world(&w);if(status)return status;
            /* A ray through an opening is inconclusive, not evidence of solid.
             * Retry direction, but never retry away an actual back-face hit. */
            if(!w.retry && w.query.selected_face)break;
            result.retries++;cosine-=.13579f;if(cosine<-1)cosine=-1;
        }
        if(w.retry || !w.query.selected_face){result.reason=RF_CHECKPOINT_PLACEMENT_AMBIGUOUS;goto rejected;}
        if(!w.query.selected_face||!w.query.front){result.reason=RF_CHECKPOINT_PLACEMENT_SOLID;goto rejected;}
    }
    result.sphere=UINT32_MAX;if(out)*out=result;return RF_OK;
rejected:
#ifndef RF_IMAGE_XBOX_NATIVE
    if(getenv("RF_CHECKPOINT_PLACEMENT_TRACE") && result.reason==RF_CHECKPOINT_PLACEMENT_SOLID) {
        const rf_collision_face *face=w.classification_face;
        printf("PLACEMENT_SOLID_TRACE sphere%u selected%u front%u distance %.9g center %.9g %.9g %.9g endpoint %.9g %.9g %.9g plane %.9g %.9g %.9g %.9g\n",
            result.sphere,w.query.selected_face,w.query.front,(double)w.query.distance,w.center[0],w.center[1],w.center[2],
            (double)w.query.endpoint[0],(double)w.query.endpoint[1],(double)w.query.endpoint[2],
            face?(double)face->plane[0]:0,face?(double)face->plane[1]:0,face?(double)face->plane[2]:0,face?(double)face->plane[3]:0);
    }
#endif
    if(out)*out=result;return RF_NOT_FOUND;
}

int rf_checkpoint_standing_check_with_support(const rf_geomod_terrain_view *candidate,const rf_checkpoint_placement *p,float dt,float class_speed,
    rf_checkpoint_support_query query,void *context,rf_checkpoint_placement_result *out)
{
    placement_work w;rf_checkpoint_placement_result result;uint32_t stable=1;int status;
    if(!isfinite(dt)||dt<=0||!isfinite(class_speed)||class_speed<0)return RF_RANGE;
    status=rf_checkpoint_placement_check(candidate,p,&result);
    if(status){if(status==RF_NOT_FOUND&&out)*out=result;return status;}
    if(fabsf(p->basis[1])>1e-4f||fabsf(p->basis[3])>1e-4f||fabsf(p->basis[4]-1)>1e-4f||fabsf(p->basis[5])>1e-4f||fabsf(p->basis[7])>1e-4f)return RF_FORMAT;
    memset(&w,0,sizeof(w));w.candidate=candidate;w.placement=p;w.mode=3;w.fraction=1;
    status=rf_physics_ground_prepare(p->spheres,p->count,p->position,p->query_flags,0,dt,class_speed,0,&w.ground);if(status)return status;
    status=visit_world(&w);if(status)return status;
    if(query) {
        rf_checkpoint_support_hit hit;uint32_t matched=0,i;
        status=query(context,&w.ground,w.fraction,&hit,&matched);if(status)return status;
        if(matched>1)return RF_FORMAT;
        if(matched) {
            if(!isfinite(hit.fraction)||hit.fraction<0||hit.fraction>w.fraction||hit.stable>1)return RF_FORMAT;
            for(i=0;i<3;i++)if(!isfinite(hit.normal[i]))return RF_FORMAT;
            if(hit.fraction<w.fraction){w.fraction=hit.fraction;memcpy(w.normal,hit.normal,12);stable=hit.stable;}
        }
    }
    if(!stable||w.fraction>=1||w.normal[1]<.5f){result.sphere=w.ground.sphere_index;result.reason=RF_CHECKPOINT_PLACEMENT_UNSUPPORTED;if(out)*out=result;return RF_NOT_FOUND;}
    if(out)*out=result;return RF_OK;
}

int rf_checkpoint_standing_check(const rf_geomod_terrain_view *candidate,const rf_checkpoint_placement *p,float dt,float class_speed,rf_checkpoint_placement_result *out)
{return rf_checkpoint_standing_check_with_support(candidate,p,dt,class_speed,NULL,NULL,out);}

int rf_checkpoint_solid_sphere_check(const rf_collision_face *faces,uint32_t count,
    uint32_t flags,const float center[3],float radius,rf_checkpoint_placement_result *out)
{
    rf_checkpoint_placement p={0};placement_work w={0};
    rf_checkpoint_placement_result result={UINT32_MAX,RF_CHECKPOINT_PLACEMENT_FITS,0};
    float direction[3]={0,1,0},cosine=.9753f;double length=1,r;uint32_t i,attempt;int status;
    if(!faces || !count || !center || !isfinite(radius) || radius<=.002 || (flags&0x1180u))return RF_RANGE;
    p.query_flags=flags;w.placement=&p;
    for(i=0;i<3;i++){if(!isfinite(center[i]))return RF_FORMAT;w.center[i]=center[i];w.minimum[i]=DBL_MAX;w.maximum[i]=-DBL_MAX;}
    status=visit_faces(&w,faces,count);if(status)return status;
    if(!w.faces){if(out)*out=result;return RF_OK;}
    w.mode=1;w.distance=DBL_MAX;status=visit_faces(&w,faces,count);if(status)return status;
    r=(double)radius-.002;
    if(w.distance<r*r){result.reason=RF_CHECKPOINT_PLACEMENT_SURFACE;goto rejected;}
    for(i=0;i<3;i++)length+=fmax(fabs(w.center[i]-w.minimum[i]),fabs(w.center[i]-w.maximum[i]));
    if(!isfinite(length) || length>FLT_MAX)return RF_RANGE;
    for(attempt=0;attempt<16;attempt++) {
        status=rf_collision_room_direction(direction,cosine,direction);if(status)return status;
        memset(&w.query,0,sizeof(w.query));w.retry=0;
        for(i=0;i<3;i++) {
            volatile float part=direction[i]*(float)length;
            w.query.start[i]=center[i];w.query.direction[i]=direction[i];w.query.endpoint[i]=center[i]+part;
            if(!isfinite(w.query.endpoint[i]))return RF_RANGE;
        }
        w.mode=2;status=visit_faces(&w,faces,count);if(status)return status;if(!w.retry)break;
        result.retries++;cosine-=.13579f;if(cosine<-1)cosine=-1;
    }
    if(w.retry){result.reason=RF_CHECKPOINT_PLACEMENT_AMBIGUOUS;goto rejected;}
    if(w.query.selected_face && !w.query.front){result.reason=RF_CHECKPOINT_PLACEMENT_SOLID;goto rejected;}
    if(out)*out=result;return RF_OK;
rejected:
    if(out)*out=result;return RF_NOT_FOUND;
}
