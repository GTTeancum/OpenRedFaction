/* Borrow the same static resource/hand placement as scene_weapon_submit.
 * Authored Riot Shield: weapon_riotshield.v3d (compiled .v3m).
 * Highest LOD matches current held-weapon draw policy. No resource allocation.
 * Physical triangle admission uses existing54dd10 ray service, including
 * authored flag20 two-sidedness. Directional damage admission remains separate.
 */
#include "rf/model_file.h"
#include "rf/weapon.h"
#include <math.h>
#include <string.h>
static int scene_riot_shield_contact(const rf_static_render_resource *model,
    const rf_weapon_hand_placement *pose,const float start[3],const float end[3],
    float actor_fraction,rf_collision_model_response_hit *result,uint32_t *accepted)
{
    rf_collision_model_response_hit hit={0};float local[3],delta[3];
    uint32_t i,j,p,b,t,found=0;
    if(!model || !pose || !start || !end || !result || !accepted ||
       !isfinite(actor_fraction) || actor_fraction<0 || actor_fraction>1)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(pose->basis[i]))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(start[i]) || !isfinite(end[i]) || !isfinite(pose->position[i]))return RF_RANGE;
    /* World -> local: same rows as rf_model_local_view. Borrowed poses must
     * be rigid orthonormal hand transforms, as required by the renderer. */
    for(i=0;i<3;++i){
        double a=0,d=0;
        for(j=0;j<3;++j){a+=((double)start[j]-pose->position[j])*pose->basis[i*3+j];d+=((double)end[j]-start[j])*pose->basis[i*3+j];}
        local[i]=(float)a;delta[i]=(float)d;
        if(!isfinite(local[i]) || !isfinite(delta[i]))return RF_RANGE;
    }
    hit.time=actor_fraction;
    if((model->part_count && !model->parts) || (model->lod_count && !model->lods))return RF_FORMAT;
    for(p=0;p<model->part_count;++p){
        const rf_model_geometry *g;uint32_t lod=model->parts[p].first_lod;
        if(lod>=model->lod_count)return RF_FORMAT;g=&model->lods[lod].geometry;
        if(!g->batches || !g->triangles || !g->vertices || !g->reuse)return RF_FORMAT;
        for(b=0;b<g->batch_count;++b){
            const rf_model_draw_batch *batch=g->batches+b;
            if(batch->material==UINT32_MAX)continue;
            if(batch->first_triangle>g->triangle_count || batch->triangles>g->triangle_count-batch->first_triangle ||
               batch->first_vertex>g->vertex_count || batch->vertices>g->vertex_count-batch->first_vertex)return RF_FORMAT;
            for(t=0;t<batch->triangles;++t){
                const rf_model_triangle *record=g->triangles+batch->first_triangle+t;
                rf_collision_model_triangle triangle={0};float a[3],c[3];double length;
                for(i=0;i<3;++i){
                    uint32_t index=record->indices[i],steps=0;
                    if(index>=batch->vertices)return RF_FORMAT;
                    while(g->reuse[batch->first_vertex+index]>0){
                        uint32_t reuse=(uint32_t)g->reuse[batch->first_vertex+index];
                        if(reuse>index || ++steps>batch->vertices)return RF_FORMAT;index-=reuse;
                    }
                    memcpy(triangle.vertices[i],g->vertices[batch->first_vertex+index].position,12);
                    for(j=0;j<3;++j)if(!isfinite(triangle.vertices[i][j]))return RF_FORMAT;
                }
                /* Reconstruct planes from rendered vertices: some shipped
                 * static LODs store NaN planes. Never replace shape by bounds. */
                for(i=0;i<3;++i){a[i]=triangle.vertices[1][i]-triangle.vertices[0][i];c[i]=triangle.vertices[2][i]-triangle.vertices[0][i];}
                for(i=0;i<3;++i){uint32_t u=(i+1)%3,v=(i+2)%3;triangle.plane[i]=(float)((double)a[u]*c[v]-(double)a[v]*c[u]);}
                length=sqrt((double)triangle.plane[0]*triangle.plane[0]+(double)triangle.plane[1]*triangle.plane[1]+(double)triangle.plane[2]*triangle.plane[2]);
                if(!isfinite(length))return RF_FORMAT;if(length==0)continue;
                for(i=0;i<3;++i)triangle.plane[i]=(float)(triangle.plane[i]/length);
                triangle.plane[3]=-(triangle.plane[0]*triangle.vertices[0][0]+triangle.plane[1]*triangle.vertices[0][1]+triangle.plane[2]*triangle.vertices[0][2]);
                triangle.token=batch->first_triangle+t;
                found|=rf_collision_model_ray_triangle(&triangle,local,delta,record->flags&0x20u,&hit);
            }
        }
    }
    if(found){
        float point[3],normal[3];memcpy(point,hit.point,12);memcpy(normal,hit.normal,12);
        for(i=0;i<3;++i){
            hit.point[i]=pose->position[i];hit.normal[i]=0;
            for(j=0;j<3;++j){hit.point[i]+=point[j]*pose->basis[j*3+i];hit.normal[i]+=normal[j]*pose->basis[j*3+i];}
        }
        *result=hit;
    }
    *accepted=!!found;return RF_OK;
}
