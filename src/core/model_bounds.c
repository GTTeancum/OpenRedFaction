#include "rf/model_bounds.h"
#include <math.h>
#include <string.h>
static double maximum_absolute(double a,double b)
{a=fabs(a);b=fabs(b);return a>b?a:b;}

int rf_model_bounds_build(const rf_model_geometry *g,uint32_t batch,uint32_t bones,rf_model_bounds *out)
{
    rf_model_bounds b={0};const rf_model_draw_batch *draw;uint32_t i,j,k;
    if(!g || !out || bones>50 || !g->batches || batch>=g->batch_count ||
       !g->vertices || !g->triangles || !g->reuse)return RF_RANGE;
    draw=g->batches+batch;
    if(draw->first_vertex>g->vertex_count || draw->vertices>g->vertex_count-draw->first_vertex ||
       draw->first_triangle>g->triangle_count || draw->triangles>g->triangle_count-draw->first_triangle)return RF_RANGE;
    b.bones=bones;
    for(i=0;i<draw->triangles;++i)for(j=0;j<3;++j) {
        uint32_t index=g->triangles[draw->first_triangle+i].indices[j],sum=0;
        const rf_model_vertex *v;
        if(index>=draw->vertices)return RF_FORMAT;
        while(g->reuse[draw->first_vertex+index]>0) {
            uint32_t distance=(uint32_t)g->reuse[draw->first_vertex+index];
            if(distance>index)return RF_FORMAT;index-=distance;
        }
        v=g->vertices+draw->first_vertex+index;
        for(k=0;k<3;++k)if(!isfinite(v->position[k]))return RF_FORMAT;
        for(k=0;k<(bones?4u:1u) && (!bones || v->weights[k]);++k) {
            uint32_t bone=bones?v->bones[k]:0,c,bit;
            if(bone>=(bones?bones:1u))return RF_FORMAT;
            bit=1u<<(bone%32);
            if(!(b.used[bone/32]&bit)) {
                memcpy(b.minimum[bone],v->position,12);memcpy(b.maximum[bone],v->position,12);
                b.used[bone/32]|=bit;
            } else for(c=0;c<3;++c) {
                if(v->position[c]<b.minimum[bone][c])b.minimum[bone][c]=v->position[c];
                if(v->position[c]>b.maximum[bone][c])b.maximum[bone][c]=v->position[c];
            }
            sum+=bones?v->weights[k]:256;
        }
        if(sum>256)return RF_NOT_FOUND;
        if(sum<256)b.include_origin=1;
    }
    *out=b;return RF_OK;
}

int rf_model_bounds_pose(const rf_model_bounds *b,const float (*matrices)[12],rf_model_box *out)
{
    double low[3]={0},high[3]={0};uint32_t i,j,k,any;
    if(!b || !out || b->bones>50 || (b->bones && !matrices))return RF_RANGE;
    any=b->include_origin!=0;
    for(i=0;i<(b->bones?b->bones:1u);++i)if(b->used[i/32]&(1u<<(i%32))) {
        if(b->bones)for(j=0;j<12;++j)if(!isfinite(matrices[i][j]))return RF_FORMAT;
        for(j=0;j<3;++j)if(!isfinite(b->minimum[i][j]) || !isfinite(b->maximum[i][j]) || b->minimum[i][j]>b->maximum[i][j])return RF_FORMAT;
        for(j=0;j<3;++j) {
            double a=b->bones?matrices[i][9+j]:0,z=a;
            for(k=0;k<3;++k) {
                double coefficient=b->bones?matrices[i][k*3+j]:(k==j?1:0);
                double x=coefficient*b->minimum[i][k],y=coefficient*b->maximum[i][k];
                a+=x<y?x:y;z+=x<y?y:x;
            }
            /* Enclose float skinning, including cancellation and weighted sums. */
            {double scale=1;
             if(b->bones) {
                scale+=fabs(matrices[i][9+j]);
                for(k=0;k<3;++k)scale+=fabs(matrices[i][k*3+j])*
                    maximum_absolute(b->minimum[i][k],b->maximum[i][k]);
             } else scale+=maximum_absolute(a,z);
             a-=scale*1e-5;z+=scale*1e-5;}
            if(!any || a<low[j])low[j]=a;
            if(!any || z>high[j])high[j]=z;
        }
        any=1;
    }
    for(i=0;i<3;++i) {
        out->center[i]=(float)((low[i]+high[i])*.5);
        out->extent[i]=(float)((high[i]-low[i])*.5+1e-5*(1+fabs(low[i])+fabs(high[i])));
    }
    out->populated=any;return RF_OK;
}
int rf_model_box_visible(const rf_model_box *box,const rf_model_projection *view,
    uint32_t width,uint32_t height,uint32_t *visible)
{
    float center[3],extent[3],delta[3],padding[3],planes[5][3];uint32_t i,j;
    if(!box || !view || !visible || !width || !height)return RF_RANGE;
    if(!view->perspective){*visible=1;return RF_OK;}
    for(i=0;i<9;++i)if(!isfinite(view->rotation[i]))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(view->camera[i]))return RF_FORMAT;
    for(i=0;i<4;++i)if(!isfinite(view->screen[i]))return RF_FORMAT;
    if(!box->populated){*visible=0;return RF_OK;}
    for(i=0;i<3;++i) {
        /* Overflow in a finite source's float box is uncertain: keep the draw. */
        if(!isfinite(box->center[i]) || !isfinite(box->extent[i])){*visible=1;return RF_OK;}
        if(box->extent[i]<0)return RF_FORMAT;
        delta[i]=box->center[i]-view->camera[i];
        padding[i]=box->extent[i]+(1+fabsf(box->center[i])+fabsf(view->camera[i]))*1e-5f;
    }
    for(i=0;i<3;++i) {
        const float *r=view->rotation+i*3;
        center[i]=(delta[0]*r[0]+delta[1]*r[1])+delta[2]*r[2];
        extent[i]=padding[0]*fabsf(r[0])+padding[1]*fabsf(r[1])+padding[2]*fabsf(r[2]);
        extent[i]+=(1+fabsf(center[i])+extent[i])*1e-5f;
        if(!isfinite(center[i]) || !isfinite(extent[i])){*visible=1;return RF_OK;}
    }
    memset(planes,0,sizeof(planes));planes[0][2]=1;
    planes[1][0]=view->screen[0];planes[1][2]=view->screen[2];
    planes[2][0]=-view->screen[0];planes[2][2]=(float)width-view->screen[2];
    planes[3][1]=view->screen[1];planes[3][2]=view->screen[3];
    planes[4][1]=-view->screen[1];planes[4][2]=(float)height-view->screen[3];
    for(i=0;i<5;++i) {
        float upper=0,rounding=1;
        for(j=0;j<3;++j) {
            float term=planes[i][j]*center[j],radius=fabsf(planes[i][j])*extent[j];
            upper+=term+radius;rounding+=fabsf(term)+radius+100*fabsf(planes[i][j]);
        }
        if(isfinite(upper) && upper < -rounding*1e-4f){*visible=0;return RF_OK;}
    }
    *visible=1;return RF_OK;
}

int rf_model_bounds_visible(const rf_model_bounds *bounds,const float (*matrices)[12],
    const rf_model_projection *view,uint32_t width,uint32_t height,uint32_t *visible)
{
    rf_model_box box;int status;
    if(!view || !visible || !width || !height)return RF_RANGE;
    status=rf_model_bounds_pose(bounds,matrices,&box);if(status)return status;
    return rf_model_box_visible(&box,view,width,height,visible);
}
