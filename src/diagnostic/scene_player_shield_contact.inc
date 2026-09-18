/* Physical FP shield triangles only. Explicit port policy: unprojected
 * model units, minus the render model_camera, placed at eye/eye_basis.
 * FOV and independent overlay depth are never collision inputs. No heap.
 * Call after exactly one rf_player_weapon_step for the current frame.
 */
#include "rf/player_weapon.h"
#include <math.h>
#include <string.h>
static int scene_player_shield_bones(const rf_player_weapon *w,uint8_t selected[50])
{
    uint32_t i,j;int32_t root=-1;
    if(!w || !selected || !w->bone_count || w->bone_count>50)return RF_RANGE;
    for(i=0;i<w->bone_count;++i){
        if(!memchr(w->bones[i].name,0,sizeof(w->bones[i].name)))return RF_FORMAT;
        if(!strcmp(w->bones[i].name,"fk-shield"))root=(int32_t)i;
    }
    if(root<0)return RF_NOT_FOUND;memset(selected,0,50);
    for(i=0;i<w->bone_count;++i){
        int32_t bone=(int32_t)i;
        for(j=0;j<=w->bone_count && bone>=0;++j){
            if((uint32_t)bone>=w->bone_count || j==w->bone_count)return RF_FORMAT;
            if(bone==root){selected[i]=1;break;}bone=w->bones[bone].parent;
        }
    }
    return RF_OK;
}
/* Resolve render reuse, then require every active influence to belong to the
 * shield subtree. A hand/shield mixed vertex is excluded, never made solid. */
static int scene_player_shield_triangle(const rf_player_weapon *w,const rf_model_draw_batch *batch,
    const rf_model_triangle *record,const uint8_t selected[50],rf_collision_model_triangle *triangle,uint32_t *eligible)
{
    uint32_t i,j;rf_collision_model_triangle value={0};float a[3],b[3];double length;
    *eligible=0;
    for(i=0;i<3;++i){
        uint32_t index=record->indices[i],steps=0;const rf_model_vertex *v;float pair[6];int status;
        if(index>=batch->vertices)return RF_FORMAT;
        while(w->geometry.reuse[batch->first_vertex+index]>0){
            uint32_t reuse=(uint32_t)w->geometry.reuse[batch->first_vertex+index];
            if(reuse>index || ++steps>batch->vertices)return RF_FORMAT;index-=reuse;
        }
        v=w->geometry.vertices+batch->first_vertex+index;
        if(!v->weights[0])return RF_OK;
        for(j=0;j<4 && v->weights[j];++j){
            if(v->bones[j]>=w->bone_count)return RF_FORMAT;
            if(!selected[v->bones[j]])return RF_OK;
        }
        status=rf_model_render_vertex_pair(v->position,v->normal,v->weights,v->bones,w->prepared,w->bone_count,pair);if(status)return status;
        for(j=0;j<3;++j){if(!isfinite(pair[j]))return RF_FORMAT;value.vertices[i][j]=pair[j];}
    }
    for(i=0;i<3;++i){a[i]=value.vertices[1][i]-value.vertices[0][i];b[i]=value.vertices[2][i]-value.vertices[0][i];}
    for(i=0;i<3;++i){uint32_t j1=(i+1)%3,k=(i+2)%3;value.plane[i]=(float)((double)a[j1]*b[k]-(double)a[k]*b[j1]);}
    length=sqrt((double)value.plane[0]*value.plane[0]+(double)value.plane[1]*value.plane[1]+(double)value.plane[2]*value.plane[2]);
    if(!isfinite(length))return RF_FORMAT;if(length==0)return RF_OK;
    for(i=0;i<3;++i)value.plane[i]=(float)(value.plane[i]/length);
    value.plane[3]=-(value.plane[0]*value.vertices[0][0]+value.plane[1]*value.vertices[0][1]+value.plane[2]*value.vertices[0][2]);
    if(!isfinite(value.plane[3]))return RF_FORMAT;
    *triangle=value;*eligible=1;return RF_OK;
}
static int scene_player_shield_contact(const rf_player_weapon *w,const float model_camera[3],
    const float eye[3],const float eye_basis[9],const float start[3],const float end[3],
    float limit,rf_collision_model_response_hit *result,uint32_t *accepted)
{
    uint8_t selected[50];rf_collision_model_response_hit hit={0};float local[3],delta[3];
    const rf_model_geometry *g;uint32_t i,j,b,t,found=0;int status;
    if(!w || !w->initialized || !model_camera || !eye || !eye_basis || !start || !end || !result || !accepted ||
       !isfinite(limit) || limit<0 || limit>1)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(eye_basis[i]))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(start[i]) || !isfinite(end[i]) || !isfinite(eye[i]) || !isfinite(model_camera[i]))return RF_RANGE;
    status=scene_player_shield_bones(w,selected);if(status)return status;
    for(i=0;i<3;++i){
        double p=0,d=0;
        for(j=0;j<3;++j){p+=((double)start[j]-eye[j])*eye_basis[i*3+j];d+=((double)end[j]-start[j])*eye_basis[i*3+j];}
        local[i]=(float)(p+model_camera[i]);delta[i]=(float)d;
        if(!isfinite(local[i]) || !isfinite(delta[i]))return RF_RANGE;
    }
    g=&w->geometry;if(!g->batches || !g->vertices || !g->triangles || !g->reuse)return RF_FORMAT;
    hit.time=limit;
    for(b=0;b<g->batch_count;++b){
        const rf_model_draw_batch *batch=g->batches+b;
        if(batch->material==UINT32_MAX)continue;
        if(batch->first_vertex>g->vertex_count || batch->vertices>g->vertex_count-batch->first_vertex ||
           batch->first_triangle>g->triangle_count || batch->triangles>g->triangle_count-batch->first_triangle)return RF_FORMAT;
        for(t=0;t<batch->triangles;++t){
            const rf_model_triangle *record=g->triangles+batch->first_triangle+t;
            rf_collision_model_triangle triangle;uint32_t eligible;
            status=scene_player_shield_triangle(w,batch,record,selected,&triangle,&eligible);if(status)return status;
            if(!eligible)continue;triangle.token=batch->first_triangle+t;
            found|=rf_collision_model_ray_triangle(&triangle,local,delta,record->flags&0x20u,&hit);
        }
    }
    if(found){
        float point[3],normal[3];memcpy(point,hit.point,12);memcpy(normal,hit.normal,12);
        for(i=0;i<3;++i){
            double p=eye[i],n=0;
            for(j=0;j<3;++j){p+=((double)point[j]-model_camera[j])*eye_basis[j*3+i];n+=(double)normal[j]*eye_basis[j*3+i];}
            hit.point[i]=(float)p;hit.normal[i]=(float)n;
        }
        *result=hit;
    }
    *accepted=!!found;return RF_OK;
}
