#include <string.h>
#include <stdlib.h>
#include "rf/effect.h"
#include "rf/collision.h"
#include "rf/visibility.h"
#include "rf/image.h"
#include "rf/model.h"
#include <math.h>
#include <float.h>
static void volume_beam_normalize(float v[3])
{
    double length=sqrt(((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2]);unsigned i;
    if(!(length>0)){v[0]=1;v[1]=v[2]=0;return;}
    length=1.0/length;for(i=0;i<3;++i)v[i]=(float)(length*v[i]);
}
int rf_volume_beam_build(const float camera[3],const float end[3],const float start[3],
    float width,rf_particle_billboard_vertex vertices[4])
{
    float axis[3],view[3],side[3],half;rf_particle_billboard_vertex value[4];unsigned i,j;
    if(!camera || !end || !start || !vertices)return RF_RANGE;
    if(!isfinite(width))return RF_FORMAT;
    for(i=0;i<3;++i) {
        float sum,mid;if(!isfinite(camera[i]) || !isfinite(end[i]) || !isfinite(start[i]))return RF_FORMAT;
        axis[i]=(float)((double)end[i]-start[i]);sum=(float)((double)end[i]+start[i]);mid=(float)((double)sum*.5);
        view[i]=(float)((double)camera[i]-mid);if(!isfinite(axis[i]) || !isfinite(view[i]))return RF_FORMAT;
    }
    volume_beam_normalize(axis);volume_beam_normalize(view);
    for(i=0;i<3;++i){j=(i+1)%3;side[i]=(float)((double)axis[j]*view[(i+2)%3]-(double)axis[(i+2)%3]*view[j]);}
    volume_beam_normalize(side);half=(float)((double)width*.5);
    for(i=0;i<3;++i) {
        float offset=(float)((double)side[i]*half);
        value[0].position[i]=(float)((double)end[i]+offset);value[1].position[i]=(float)((double)start[i]+offset);
        value[2].position[i]=(float)((double)start[i]-offset);value[3].position[i]=(float)((double)end[i]-offset);
    }
    for(i=0;i<4;++i){value[i].uv[0]=i>=2?1:0;value[i].uv[1]=(i==1 || i==2)?1:0;
        for(j=0;j<3;++j)if(!isfinite(value[i].position[j]))return RF_FORMAT;}
    memcpy(vertices,value,sizeof(value));return RF_OK;
}
int rf_particle_cone_sample(float cosine_min,rf_random_state *random,float direction[3])
{
    rf_random_state next;uint32_t draw;float z,radial,value[3];double angle;
    if(!random || !direction || !isfinite(cosine_min) || cosine_min < -1 || cosine_min > 1)return RF_RANGE;
    next=*random;rf_random_next(&next,&draw);
    z=(float)((1.0-(double)cosine_min)*((double)draw/32768.0)+cosine_min);
    rf_random_next(&next,&draw);angle=((double)draw/32768.0)*(double)6.2831853071795864769f;
    radial=(float)sqrt(1.0-(double)z*z);
    value[0]=(float)(cos(angle)*radial);value[1]=(float)(sin(angle)*radial);value[2]=z;
    direction[0]=value[0];direction[1]=value[1];direction[2]=value[2];*random=next;return RF_OK;
}

int rf_particle_cone_oriented(const float axis[3],float cosine_min,
    rf_random_state *random,float direction[3])
{
    float right[3],up[3],forward[3],local[3],value[3];rf_random_state next;
    double inverse;unsigned i;int status;
    if(!axis || !random || !direction)return RF_RANGE;
    for(i=0;i<3;i++){if(!isfinite(axis[i]))return RF_RANGE;forward[i]=axis[i];}
    if(axis[0]<0.0001f && axis[0]>-0.0001f && axis[2]<0.0001f && axis[2]>-0.0001f) {
        right[0]=1;right[1]=right[2]=0;up[0]=up[1]=0;
        up[2]=axis[1]<0?1.0f:-1.0f;
        forward[0]=forward[2]=0;forward[1]=axis[1]<0?-1.0f:1.0f;
    } else {
        right[0]=axis[2];right[1]=0;right[2]=-axis[0];
        inverse=1.0/sqrt(((double)right[0]*right[0]+(double)right[1]*right[1])+(double)right[2]*right[2]);
        for(i=0;i<3;i++)right[i]=(float)((double)right[i]*inverse);
        up[0]=(float)((double)forward[1]*right[2]-(double)forward[2]*right[1]);
        up[1]=(float)((double)forward[2]*right[0]-(double)forward[0]*right[2]);
        up[2]=(float)((double)forward[0]*right[1]-(double)forward[1]*right[0]);
    }
    next=*random;status=rf_particle_cone_sample(cosine_min,&next,local);if(status!=RF_OK)return status;
    for(i=0;i<3;i++) {
        value[i]=(float)(((double)local[2]*forward[i]+(double)local[1]*up[i])+(double)local[0]*right[i]);
        if(!isfinite(value[i]))return RF_RANGE;
    }
    for(i=0;i<3;i++)direction[i]=value[i];*random=next;return RF_OK;
}

int rf_bitmap_animation_frame(uint32_t now,uint32_t started,uint32_t fps,
    uint32_t count,uint32_t loop,int32_t *frame)
{
    float rate;double phase;uint32_t selected;
    if(!frame || !count || count>255 || fps>INT32_MAX)return RF_RANGE;
    rate=(float)((double)fps*(double).001f);
    phase=(double)(uint32_t)(now-started)*rate;
    if(!isfinite(phase) || phase>=2147483648.0)return RF_RANGE;
    selected=(uint32_t)phase;
    if(selected>=count) {
        if(!loop){*frame=-1;return RF_OK;}
        selected%=count;if(loop==2 && (selected&1u))selected=count-selected-1;
    }
    *frame=(int32_t)selected;return RF_OK;
}
uint32_t rf_particle_render_mode(uint32_t flags,uint32_t normal_mode,uint32_t glow_mode)
{
    uint32_t mode=(flags&2u)?glow_mode:normal_mode;
    return (flags&0x2000u)?mode&~(31u<<20):mode;
}
int rf_particle_texture_decode(uint32_t mode,uint32_t lod_bias,rf_particle_texture_states *s)
{
    uint32_t color=(mode>>5)&31u,alpha=(mode>>10)&31u;
    rf_particle_texture_states value={12,{
        {0,19,0},{0,13,3},{0,14,3},{0,17,2},{0,16,2},
        {0,1,0},{0,2,2},{0,3,0},{0,4,0},{0,5,2},{0,6,0},{1,1,1}}};
    if(!s)return RF_RANGE;
    if((mode&31u)!=1 && (mode&31u)!=2)return RF_NOT_FOUND;
    if((mode&31u)==1)value.writes[1].value=value.writes[2].value=1;
    value.writes[0].value=lod_bias;
    value.writes[5].value=color==3?7u:color==4?5u:color==2?4u:2u;
    value.writes[8].value=alpha==3?4u:((mode&31u)==1 && alpha==0)?3u:2u;
    if((mode&31u)==1 && alpha!=0 && alpha!=2 && alpha!=3) {
        memmove(value.writes+8,value.writes+9,3*sizeof(value.writes[0]));
        memset(value.writes+11,0,sizeof(value.writes[0]));value.count=11;
    }
    *s=value;return RF_OK;
}

static void particle_render_write(rf_particle_render_states *s,uint32_t state,uint32_t value)
{
    s->writes[s->count].state=state;s->writes[s->count].value=value;s->count++;
}
int rf_particle_render_decode(uint32_t mode,const rf_particle_render_environment *e,
    rf_particle_render_states *s)
{
    uint32_t color=(mode>>5)&31u,alpha=(mode>>10)&31u,blend=(mode>>15)&31u;
    uint32_t depth=(mode>>20)&31u,fog=(mode>>25)&31u,z,compare;
    static const uint32_t factors[8][2]={{0,0},{2,2},{5,2},{5,6},{5,2},{9,1},{10,1},{9,3}};
    if(!e || !s)return RF_RANGE;
    s->count=0;
    if(color<=4)s->vertex_color=color!=1;
    if(alpha<=3)s->vertex_alpha=alpha!=2;
    if(blend==0)particle_render_write(s,27,0);
    else if(blend<=7 && (blend!=5 || (e->blend_caps&256u))) {
        particle_render_write(s,27,1);
        if(blend==3 && !(e->blend_caps&16u))particle_render_write(s,19,12);
        else {
            particle_render_write(s,19,factors[blend][0]);
            particle_render_write(s,20,factors[blend][1]);
        }
    }
    z=e->depth_kind==0?1u:e->depth_kind==1?2u:0u;
    compare=e->depth_kind==0?7u:4u;
    if(depth<=4) {
        particle_render_write(s,15,0);
        particle_render_write(s,7,(depth==0 || depth==3)?0:z);
        particle_render_write(s,14,depth>=3);
        if(depth==1 || depth==2 || depth==4)particle_render_write(s,23,depth==2?3:compare);
    } else if(depth==5) {
        particle_render_write(s,7,z);particle_render_write(s,14,1);
        particle_render_write(s,23,compare);particle_render_write(s,15,1);
        particle_render_write(s,24,16);particle_render_write(s,25,7);
    }
    if(fog<=3) {
        uint32_t enabled=fog<3 && (e->fog_enabled&255u)!=0;
        s->vertex_fog=enabled && e->fog_kind==2;
        particle_render_write(s,28,enabled);
    }
    return RF_OK;
}

static uint32_t particle_clip_code(const rf_particle_clip_environment *clip,const float p[3])
{
    uint32_t code=0;float x=p[0],y=p[1],z=p[2];
    if(clip->enabled&255u) {
        if(x>z)code|=8;
        if(y>z)code|=32;
        if(x<-z)code|=4;
        if(y<-z)code|=16;
        if(clip->depth_enabled&255u) {
            if(z<=0)code|=128;
            if((clip->far_enabled&255u) && z>clip->far_distance)code|=2;
        }
    }
    return code;
}
int rf_volume_beam_project(const rf_visibility_camera *camera,const float end[3],const float start[3],
    float width,rf_particle_screen_polygon *out)
{
    rf_particle_billboard_vertex vertices[4];int status;if(!camera || !out)return RF_RANGE;
    status=rf_volume_beam_build(camera->projection.origin,end,start,width,vertices);if(status)return status;
    return rf_particle_world_quad(camera,vertices,out);
}
int rf_particle_world_quad(const rf_visibility_camera *camera,const rf_particle_billboard_vertex vertices[4],
    rf_particle_screen_polygon *out)
{
    rf_particle_billboard_packet packet={0};
    rf_particle_clipped_polygon clipped={0};rf_particle_screen_polygon value={0};
    const rf_visibility_projection *view;uint32_t i,j;int status;
    if(!camera || !vertices || !out)return RF_RANGE;
    view=&camera->projection;
    if(!isfinite(view->flat_depth) || !isfinite(view->clip.far_distance))return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(view->matrix[i]))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(view->origin[i]))return RF_RANGE;
    for(i=0;i<4;++i) {
        for(j=0;j<3;++j)if(!isfinite(vertices[i].position[j]))return RF_RANGE;
        for(j=0;j<2;++j)if(!isfinite(vertices[i].uv[j]))return RF_RANGE;
    }
    packet.clip_and=255;
    for(i=0;i<4;i++) {
        float delta[3];
        for(j=0;j<3;j++)delta[j]=vertices[i].position[j]-view->origin[j];
        packet.vertices[i].vertex=vertices[i];
        for(j=0;j<3;j++) {
            packet.vertices[i].vertex.position[j]=(float)(((double)view->matrix[j*3]*delta[0]+
                (double)view->matrix[j*3+1]*delta[1])+(double)view->matrix[j*3+2]*delta[2]);
            if(!isfinite(packet.vertices[i].vertex.position[j]))return RF_RANGE;
        }
        if(!(view->perspective&255u))packet.vertices[i].vertex.position[2]=view->flat_depth;
        packet.vertices[i].clip=particle_clip_code(&view->clip,packet.vertices[i].vertex.position);
        packet.clip_and&=packet.vertices[i].clip;packet.clip_or|=packet.vertices[i].clip;
    }
    if(packet.clip_and){*out=value;return RF_OK;}
    if((view->projection.clamp&255u) && packet.clip_or) {
        status=rf_particle_billboard_clip(&view->clip,&packet,&clipped);if(status)return status;
        if(!clipped.count || clipped.clip_and){*out=value;return RF_OK;}
    } else {
        clipped.count=4;for(i=0;i<4;i++)clipped.vertices[i]=packet.vertices[i];
    }
    for(i=0;i<clipped.count;i++) {
        rf_particle_projected_point point={0};
        memcpy(point.camera,clipped.vertices[i].vertex.position,sizeof(point.camera));point.clip=(uint8_t)clipped.vertices[i].clip;
        status=rf_particle_project(&view->projection,&point);if(status)return status;
        if(point.flags&2u){*out=(rf_particle_screen_polygon){0};return RF_OK;}
        memcpy(value.vertices[i].camera,point.camera,sizeof(point.camera));
        memcpy(value.vertices[i].screen,point.screen,sizeof(point.screen));
        memcpy(value.vertices[i].uv,clipped.vertices[i].vertex.uv,sizeof(value.vertices[i].uv));
        value.vertices[i].reciprocal_z=point.reciprocal_z;
    }
    value.count=clipped.count;*out=value;return RF_OK;
}
static double corona_dot(const float a[3],const float b[3])
{return ((double)a[0]*b[0]+(double)a[1]*b[1])+(double)a[2]*b[2];}
int rf_corona_oriented_build(const float camera[3],const float forward[3],
    const float first[3],const float second[3],float size,
    rf_particle_billboard_vertex vertices[4],uint32_t *kind)
{
    static const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    float delta[3],a[3],b[3],da[3],db[3],z1,z2,n[3],ray[3],projected[3],axis[3],side[3];
    const float *near;float extent;double inv,t,den;uint32_t i,j;
    rf_particle_billboard_vertex result[4]={0};
    if(!camera || !forward || !first || !second || !vertices || !kind || !isfinite(size))return RF_RANGE;
    for(i=0;i<3;++i){if(!isfinite(camera[i]) || !isfinite(forward[i]) || !isfinite(first[i]) || !isfinite(second[i]))return RF_RANGE;
        delta[i]=(float)((double)second[i]-first[i]);da[i]=(float)((double)first[i]-camera[i]);db[i]=(float)((double)second[i]-camera[i]);}
    if(corona_dot(delta,delta)<.001){*kind=1;return RF_OK;}
    z1=(float)corona_dot(da,forward);z2=(float)corona_dot(db,forward);
    if(z1<.16f && z2<.16f){*kind=0;return RF_OK;}
    memcpy(a,first,12);memcpy(b,second,12);
    if(z1<.16f){float factor=(float)(((double).16f-z1)/(fabs((double)z1)+fabs((double)z2)));
        for(i=0;i<3;++i)a[i]=(float)((double)first[i]+(float)((double)factor*delta[i]));z1=.16f;}
    if(z2<.16f){float factor=(float)(((double).16f-z2)/(fabs((double)z1)+fabs((double)z2)));
        for(i=0;i<3;++i)b[i]=(float)((double)second[i]-(float)((double)factor*delta[i]));z2=.16f;}
    near=z1<z2?a:b;
    for(i=0;i<3;++i){n[i]=-forward[i];ray[i]=(float)((double)(z1<z2?b[i]:a[i])-camera[i]);}
    inv=1.0/sqrt(corona_dot(ray,ray));if(!isfinite(inv))return RF_RANGE;
    for(i=0;i<3;++i)ray[i]=(float)((double)ray[i]*inv);
    den=corona_dot(ray,n);if(den==0){*kind=0;return RF_OK;}
    {float offset=(float)-corona_dot(n,near);const float *far=z1<z2?b:a;
        t=(float)(-(corona_dot(n,far)+offset)/(float)den);
        for(i=0;i<3;++i)projected[i]=(float)((double)far[i]+(float)(t*ray[i]));}
    for(i=0;i<3;++i)axis[i]=(float)((double)projected[i]-near[i]);
    t=sqrt(corona_dot(axis,axis));
    if(t<=0){axis[0]=1;axis[1]=axis[2]=0;}else{inv=1.0/t;for(i=0;i<3;++i)axis[i]=(float)((double)axis[i]*inv);}
    side[0]=(float)((double)n[1]*axis[2]-(double)n[2]*axis[1]);
    side[1]=(float)((double)n[2]*axis[0]-(double)n[0]*axis[2]);
    side[2]=(float)((double)n[0]*axis[1]-(double)n[1]*axis[0]);
    extent=(float)((double)size*.5f);
    for(i=0;i<3;++i){axis[i]=(float)((double)axis[i]*extent);side[i]=(float)((double)side[i]*extent);
        result[0].position[i]=(float)((double)(float)((double)near[i]-axis[i])-side[i]);
        result[1].position[i]=(float)((double)(float)((double)near[i]-axis[i])+side[i]);
        result[2].position[i]=(float)((double)(float)((double)projected[i]+axis[i])+side[i]);
        result[3].position[i]=(float)((double)(float)((double)projected[i]+axis[i])-side[i]);}
    for(i=0;i<4;++i){for(j=0;j<3;++j)if(!isfinite(result[i].position[j]))return RF_RANGE;memcpy(result[i].uv,uv[i],8);}
    memcpy(vertices,result,sizeof(result));*kind=2;return RF_OK;
}
int rf_particle_world_stretch(const rf_visibility_camera *camera,const float position[3],
    const float previous[3],float radius,uint32_t width,uint32_t height,rf_particle_screen_polygon *out)
{
    rf_particle_billboard_vertex vertices[4];const rf_visibility_projection *view;
    uint32_t fallback,i;int status;
    if(!camera || !out || !width || !height)return RF_RANGE;
    view=&camera->projection;
    if(!isfinite(view->flat_depth) || !isfinite(view->clip.far_distance))return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(view->matrix[i]))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(view->origin[i]))return RF_RANGE;
    status=rf_particle_stretch_build(position,previous,view->matrix+6,radius,vertices,&fallback);if(status)return status;
    if(fallback)return rf_particle_world_billboard(camera,position,0,radius,width,height,out);
    return rf_particle_world_quad(camera,vertices,out);
}
int rf_visibility_box_project(const rf_visibility_projection *view,const float minimum[3],
    const float maximum[3],rf_visibility_screen_bounds *output)
{
    static const unsigned char high[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    static const unsigned char faces[6][4]={{3,2,1,0},{4,5,6,7},{7,6,2,3},{0,1,5,4},{1,2,6,5},{4,7,3,0}};
    rf_particle_clipped_vertex corners[8]={0};rf_visibility_screen_bounds result;
    uint32_t i,j,k,mask=255;
    if(!view || !minimum || !maximum || !output)return RF_RANGE;
    if(!isfinite(view->flat_depth) || !isfinite(view->clip.far_distance))return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(view->matrix[i]))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(view->origin[i]) || !isfinite(minimum[i]) ||
        !isfinite(maximum[i]) || minimum[i]>maximum[i])return RF_RANGE;
    result=*output;result.visible=0;
    for(i=0;i<8;i++) {
        float delta[3];
        for(j=0;j<3;j++)delta[j]=(high[i][j]?maximum[j]:minimum[j])-view->origin[j];
        for(j=0;j<3;j++) {
            corners[i].vertex.position[j]=(float)(((double)view->matrix[j*3]*delta[0]+
                (double)view->matrix[j*3+1]*delta[1])+(double)view->matrix[j*3+2]*delta[2]);
            if(!isfinite(corners[i].vertex.position[j]))return RF_RANGE;
        }
        if(!(view->perspective&255u))corners[i].vertex.position[2]=view->flat_depth;
        corners[i].clip=particle_clip_code(&view->clip,corners[i].vertex.position);mask&=corners[i].clip;
    }
    if(mask){*output=result;return RF_OK;}
    for(i=0;i<6;i++) {
        rf_particle_billboard_packet packet={0};rf_particle_clipped_polygon polygon={0};float points[12][2];int rejected=0,status;
        packet.clip_and=255;
        for(j=0;j<4;j++) {
            packet.vertices[j]=corners[faces[i][j]];
            packet.clip_and&=packet.vertices[j].clip;packet.clip_or|=packet.vertices[j].clip;
        }
        if(packet.clip_and)continue;
        status=rf_particle_billboard_clip(&view->clip,&packet,&polygon);if(status)return status;
        if(polygon.clip_and)continue;
        for(j=0;j<polygon.count;j++) {
            rf_particle_projected_point point={0};
            for(k=0;k<3;k++)point.camera[k]=polygon.vertices[j].vertex.position[k];
            point.clip=(uint8_t)polygon.vertices[j].clip;
            status=rf_particle_project(&view->projection,&point);if(status)return status;
            if(point.flags&2u){rejected=1;break;}
            points[j][0]=point.screen[0];points[j][1]=point.screen[1];
        }
        if(rejected)continue;
        for(j=0;j<polygon.count;j++) {
            if(!result.visible) {
                result.rectangle[0]=result.rectangle[2]=points[j][0];
                result.rectangle[1]=result.rectangle[3]=points[j][1];result.visible=1;
            }
            for(k=0;k<2;k++) {
                if(points[j][k]<=result.rectangle[k])result.rectangle[k]=points[j][k];
                if(result.rectangle[k+2]<=points[j][k])result.rectangle[k+2]=points[j][k];
            }
        }
    }
    *output=result;return RF_OK;
}

static int particle_clip_intersection(uint32_t plane,const rf_particle_clipped_vertex *a,
    const rf_particle_clipped_vertex *b,const rf_particle_clip_environment *e,rf_particle_clipped_vertex *v)
{
    double t,denominator,numerator;unsigned i;
    if(plane==2) {
        denominator=(double)b->vertex.position[2]-a->vertex.position[2];
        t=denominator==0?1:((double)e->far_distance-a->vertex.position[2])/denominator;
    } else {
        unsigned axis=(plane&12u)?0:1;
        double av=a->vertex.position[axis],bv=b->vertex.position[axis];
        if(plane&20u){av=-av;bv=-bv;}
        numerator=av-a->vertex.position[2];
        denominator=(numerator-bv)+b->vertex.position[2];
        if(denominator==0)return RF_RANGE;
        t=numerator/denominator;
    }
    for(i=0;i<2;i++)v->vertex.position[i]=(float)(((double)b->vertex.position[i]-a->vertex.position[i])*t+a->vertex.position[i]);
    v->vertex.position[2]=plane==2?e->far_distance:v->vertex.position[(plane&48u)?1:0];
    if(plane&20u)v->vertex.position[2]=-v->vertex.position[2];
    for(i=0;i<2;i++)v->vertex.uv[i]=(float)(((double)b->vertex.uv[i]-a->vertex.uv[i])*t+a->vertex.uv[i]);
    for(i=0;i<3;i++)if(!isfinite(v->vertex.position[i]))return RF_RANGE;
    for(i=0;i<2;i++)if(!isfinite(v->vertex.uv[i]))return RF_RANGE;
    v->clip=particle_clip_code(e,v->vertex.position);return RF_OK;
}
int rf_particle_billboard_clip(const rf_particle_clip_environment *e,
    const rf_particle_billboard_packet *packet,rf_particle_clipped_polygon *polygon)
{
    rf_particle_clipped_vertex vertices[52];uint32_t arrays[2][14],free_ids[48],used=0;
    rf_particle_clipped_polygon result={0};
    uint32_t count=4,source=0,or_code=0,and_code=255,plane,i,j;
    if(!e || !packet || !polygon || !isfinite(e->far_distance) || !isfinite(packet->depth))return RF_RANGE;
    for(i=0;i<48;i++)free_ids[i]=i+4;
    for(i=0;i<4;i++) {
        vertices[i]=packet->vertices[i];arrays[0][i]=i;
        if(vertices[i].clip&~190u)return RF_NOT_FOUND;
        for(j=0;j<3;j++)if(!isfinite(vertices[i].vertex.position[j]))return RF_RANGE;
        for(j=0;j<2;j++)if(!isfinite(vertices[i].vertex.uv[j]))return RF_RANGE;
        or_code|=vertices[i].clip;and_code&=vertices[i].clip;
    }
    for(plane=2;plane<=32;plane*=2)if(or_code&plane) {
        uint32_t output=0;or_code=0;and_code=255;
        arrays[source][count]=arrays[source][0];arrays[source][count+1]=arrays[source][1];
        /* Preserve original temporary-pool reuse: earlier freed records may
         * still be referenced by neighbors in this pass. Value copies would
         * silently remove duplicate intersections produced by that ordering. */
        for(i=1;i<=count;i++) {
            uint32_t id=arrays[source][i];
            if(!(vertices[id].clip&plane)) {
                if(output==12)return RF_RANGE;
                arrays[source^1u][output++]=id;
                or_code|=vertices[id].clip;and_code&=vertices[id].clip;
            } else {
                for(j=0;j<2;j++) {
                    uint32_t neighbor=arrays[source][j?i+1:i-1];
                    if(!(vertices[neighbor].clip&plane)) {
                        uint32_t created;int status;
                        if(output==12 || used>=47)return RF_RANGE;
                        created=free_ids[used++];
                        status=particle_clip_intersection(plane,&vertices[neighbor],&vertices[id],e,&vertices[created]);
                        if(status!=RF_OK)return status;
                        arrays[source^1u][output++]=created;
                        or_code|=vertices[created].clip;and_code&=vertices[created].clip;
                    }
                }
                if(id>=4){if(!used)return RF_RANGE;free_ids[--used]=id;}
            }
        }
        source^=1u;count=output;
        if(and_code)break;
    }
    result.count=count;result.depth=packet->depth;result.clip_and=and_code;result.clip_or=or_code;
    for(i=0;i<count;i++)result.vertices[i]=vertices[arrays[source][i]];
    *polygon=result;return RF_OK;
}

int rf_particle_billboard_prepare(const float center[3],float angle,float radius,
    uint32_t width,uint32_t height,const float scale[3],
    const rf_particle_clip_environment *clip,rf_particle_billboard_packet *packet)
{
    rf_particle_billboard_vertex vertices[4];rf_particle_billboard_packet value;
    double bias;unsigned i;int status;
    if(!center || !scale || !clip || !packet)return RF_RANGE;
    if(!isfinite(scale[2]) || !isfinite(clip->far_distance))return RF_RANGE;
    status=rf_particle_billboard_build(center,angle,radius,width,height,scale,vertices);
    if(status!=RF_OK)return status;
    bias=(double)scale[2]*radius;
    value.depth=bias<center[2]?(float)((double)center[2]-bias):center[2];
    if(!isfinite(value.depth))return RF_RANGE;
    value.clip_and=255;value.clip_or=0;
    for(i=0;i<4;i++) {
        uint32_t code=particle_clip_code(clip,vertices[i].position);
        value.vertices[i].vertex=vertices[i];value.vertices[i].clip=code;
        value.clip_and&=code;value.clip_or|=code;
    }
    *packet=value;return RF_OK;
}

int rf_particle_project(const rf_particle_projection *projection,rf_particle_projected_point *point)
{
    rf_particle_projected_point value;float inverse,x;double y;unsigned i;
    if(!projection || !point)return RF_RANGE;
    if(point->flags&3u)return RF_OK;
    for(i=0;i<3;i++)if(!isfinite(point->camera[i]))return RF_RANGE;
    if(!isfinite(projection->depth_offset) || !isfinite(projection->half_width) || !isfinite(projection->half_height))return RF_RANGE;
    value=*point;
    if((projection->clamp&255u) && value.camera[2]<=0) {value.flags|=2;*point=value;return RF_OK;}
    value.flags|=1;inverse=value.camera[2]==0?FLT_MAX:(float)(1.0/value.camera[2]);value.reciprocal_z=inverse;
    if(projection->depth_offset!=0 && (double)projection->depth_offset*20.0<value.camera[2])
        value.reciprocal_z=(float)(1.0/((double)value.camera[2]-projection->depth_offset));
    x=(float)((double)inverse*value.camera[0]+1.0);y=1.0-(double)inverse*value.camera[1];
    if(projection->clamp&255u) {if(x<=0)x=0;else if(x>=2)x=2;if(y<=0)y=0;else if(y>=2)y=2;}
    value.screen[0]=(float)((double)projection->half_width*x+projection->origin_x);
    value.screen[1]=(float)((double)projection->half_height*y+projection->origin_y);
    *point=value;return RF_OK;
}
int rf_particle_billboard_project(const rf_particle_projection *projection,
    const rf_particle_clip_environment *environment,const rf_particle_billboard_packet *packet,
    rf_particle_screen_polygon *polygon)
{
    rf_particle_clipped_polygon clipped;rf_particle_screen_polygon result={0};
    uint32_t and_code=255,or_code=0,i,j;int status;
    if(!projection || !environment || !packet || !polygon)return RF_RANGE;
    if(!isfinite(packet->depth) || !isfinite(projection->depth_offset) ||
       !isfinite(projection->half_width) || !isfinite(projection->half_height))return RF_RANGE;
    for(i=0;i<4;i++) {
        and_code&=packet->vertices[i].clip;or_code|=packet->vertices[i].clip;
        for(j=0;j<3;j++)if(!isfinite(packet->vertices[i].vertex.position[j]))return RF_RANGE;
        for(j=0;j<2;j++)if(!isfinite(packet->vertices[i].vertex.uv[j]))return RF_RANGE;
    }
    if(and_code){*polygon=result;return RF_OK;}
    if((projection->clamp&255u) && or_code) {
        status=rf_particle_billboard_clip(environment,packet,&clipped);
        if(status!=RF_OK)return status;
        if(!clipped.count || clipped.clip_and){*polygon=result;return RF_OK;}
    } else {
        clipped.count=4;
        for(i=0;i<4;i++)clipped.vertices[i]=packet->vertices[i];
    }
    for(i=0;i<clipped.count;i++) {
        rf_particle_projected_point point={0};
        for(j=0;j<3;j++)point.camera[j]=clipped.vertices[i].vertex.position[j];
        point.clip=(uint8_t)clipped.vertices[i].clip;
        status=rf_particle_project(projection,&point);if(status!=RF_OK)return status;
        if(point.flags&2u){*polygon=(rf_particle_screen_polygon){0};return RF_OK;}
        /* Screen coordinates were computed using the original corner depth. */
        point.camera[2]=packet->depth;point.reciprocal_z=(float)(1.0/(double)packet->depth);
        for(j=0;j<3;j++)result.vertices[i].camera[j]=point.camera[j];
        for(j=0;j<2;j++) {
            result.vertices[i].screen[j]=point.screen[j];
            result.vertices[i].uv[j]=clipped.vertices[i].vertex.uv[j];
        }
        result.vertices[i].reciprocal_z=point.reciprocal_z;
    }
    result.count=clipped.count;*polygon=result;return RF_OK;
}

int rf_particle_vertex_encode(const rf_particle_vertex_environment *e,
    const rf_particle_screen_vertex *v,rf_particle_draw_vertex *output)
{
    rf_particle_draw_vertex value;uint32_t rgb[3],alpha,i;float fog;
    union {float f;uint32_t u;} rounded;
    if(!e || !v || !output)return RF_RANGE;
    if(!isfinite(e->depth_scale) || !isfinite(e->reciprocal_scale) || !isfinite(e->fog_scale) ||
       !isfinite(v->camera[2]) || isnan(v->reciprocal_z))return RF_RANGE;
    for(i=0;i<2;i++)if(!isfinite(e->uv_scale[i]) || !isfinite(v->uv[i]) || isnan(v->screen[i]))return RF_RANGE;
    for(i=0;i<3;i++)rgb[i]=(e->vertex_color&255u)?(e->rgba>>(i*8))&255u:255u;
    alpha=(e->vertex_alpha&255u)?e->rgba>>24:255u;
    if(e->color_transform&255u) {
        float sum=(float)(rgb[0]+rgb[1]+rgb[2]);
        for(i=0;i<3;i++) {
            double transformed=(double)e->color_scale[i]*sum;int32_t channel;
            if(!isfinite(transformed) || transformed<INT32_MIN || transformed>=2147483648.0)return RF_RANGE;
            channel=(int32_t)transformed;rgb[i]=channel<0?0u:channel>255?255u:(uint32_t)channel;
        }
    }
    value.argb=(alpha<<24)|(rgb[0]<<16)|(rgb[1]<<8)|rgb[2];
    fog=(float)(255.0-(double)e->fog_scale*v->camera[2]);
    if(fog<0)fog=0;else if(fog>255)fog=255;
    rounded.f=(float)((double)fog+12582912.0);value.fog=(rounded.u&255u)<<24;
    value.depth=(float)((double)e->depth_scale*v->reciprocal_z);
    value.reciprocal_w=(float)((double)e->reciprocal_scale*v->reciprocal_z);
    for(i=0;i<2;i++){value.screen[i]=v->screen[i];value.uv[i]=(float)((double)e->uv_scale[i]*v->uv[i]);}
    *output=value;return RF_OK;
}

static void stretch_normalize(float v[3])
{
    double length=sqrt(((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2]);unsigned i;
    if(length<=0){v[0]=1;v[1]=v[2]=0;return;}
    for(i=0;i<3;i++)v[i]=(float)((1.0/length)*v[i]);
}
static void stretch_cross(const float a[3],const float b[3],float out[3])
{
    out[0]=(float)((double)a[1]*b[2]-(double)a[2]*b[1]);
    out[1]=(float)((double)a[2]*b[0]-(double)a[0]*b[2]);
    out[2]=(float)((double)a[0]*b[1]-(double)a[1]*b[0]);
}
int rf_particle_stretch_build(const float position[3],const float previous[3],
    const float forward[3],float radius,rf_particle_billboard_vertex out[4],uint32_t *fallback)
{
    rf_particle_billboard_vertex value[4]={0};float delta[3],axis[3],side[3],back[3],half,extent;double dot;unsigned i;
    if(!position || !previous || !forward || !out || !fallback || !isfinite(radius) || radius<0)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(previous[i]) || !isfinite(forward[i]))return RF_RANGE;
        axis[i]=delta[i]=(float)(previous[i]-position[i]);back[i]=-forward[i];
        if(!isfinite(delta[i]))return RF_RANGE;
    }
    dot=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2];
    if(dot<0.001){memset(out,0,sizeof(value));*fallback=1;return RF_OK;}
    stretch_normalize(axis);stretch_cross(back,axis,side);stretch_normalize(side);
    stretch_cross(side,back,axis);stretch_normalize(axis);half=(float)(radius*.5);
    dot=((double)axis[2]*delta[2]+(double)axis[1]*delta[1])+(double)axis[0]*delta[0];
    extent=(float)(dot+half);
    for(i=0;i<3;i++) {
        value[0].position[i]=(float)(position[i]+(float)(extent*axis[i]));
        value[1].position[i]=(float)(position[i]+(float)(half*side[i]));
        value[2].position[i]=(float)(position[i]-(float)(half*axis[i]));
        value[3].position[i]=(float)(position[i]-(float)(half*side[i]));
    }
    value[1].uv[0]=value[2].uv[0]=value[2].uv[1]=value[3].uv[1]=1;
    memcpy(out,value,sizeof(value));*fallback=0;return RF_OK;
}
int rf_particle_billboard_build(const float center[3],float angle,float radius,
    uint32_t width,uint32_t height,const float scale[2],rf_particle_billboard_vertex out[4])
{
    rf_particle_billboard_vertex value[4];double w=radius,h=radius,hc;float sn,cs,hs,wc,ws;unsigned i;
    double dx[4],dy[4];
    if(!center || !scale || !out || !width || !height || width>INT32_MAX || height>INT32_MAX ||
       !isfinite(angle) || !isfinite(radius) || radius<0)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(center[i]))return RF_RANGE;
    if(!isfinite(scale[0]) || !isfinite(scale[1]))return RF_RANGE;
    sn=(float)sin((double)angle);cs=(float)cos((double)angle);
    if(width<height)h=(double)height*radius/width;
    else if(height<width)w=(double)width*radius/height;
    hs=(float)(h*sn);wc=(float)(w*cs);ws=(float)(w*sn);hc=h*cs;
    dx[0]=-(double)wc-hs;dy[0]=hc-ws;
    dx[1]=(double)wc-hs;dy[1]=hc+ws;
    dx[2]=(double)wc+hs;dy[2]=(double)ws-hc;
    dx[3]=(double)hs-wc;dy[3]=-(double)ws-hc;
    for(i=0;i<4;i++) {
        value[i].position[0]=(float)(dx[i]*scale[0]+center[0]);
        value[i].position[1]=(float)(dy[i]*scale[1]+center[1]);
        value[i].position[2]=center[2];
        value[i].uv[0]=(i==1 || i==2)?1.0f:0.0f;value[i].uv[1]=i>=2?1.0f:0.0f;
        if(!isfinite(value[i].position[0]) || !isfinite(value[i].position[1]))return RF_RANGE;
    }
    for(i=0;i<4;i++)out[i]=value[i];return RF_OK;
}
int rf_particle_frame_index(const rf_particle *particle,uint32_t *frame)
{
    int32_t count;double value,denominator;
    if(!particle || !frame)return RF_RANGE;
    count=(int16_t)particle->frame_count;
    if(count<=1){*frame=0;return RF_OK;}
    if(!isfinite(particle->age) || particle->age<0)return RF_RANGE;
    if(particle->flags&0x100u) {
        if(particle->age>=2147483648.0)return RF_RANGE;
        value=((double)particle->age-floor(particle->age))*15.0/count+0.5;
    } else {
        denominator=particle->life;
        if(particle->secondary&4u)denominator*=particle->age_to_finish_vbm;
        if(!isfinite(denominator) || denominator<=0)return RF_RANGE;
        value=(double)particle->age/denominator*count+0.5;
    }
    if(!isfinite(value) || value>=2147483648.0)return RF_RANGE;
    value=floor(value);if(value<0)value=0;if(value>=count)value=count-1;
    *frame=(uint32_t)value;return RF_OK;
}
int rf_particle_blood_prepare(const float position[3],float damage,uint32_t bitmap,
    uint32_t frame_count,rf_particle_spawn *result)
{
    rf_particle_spawn value={0};uint32_t i;
    if(!position || !result || !isfinite(damage) || damage<0 || !frame_count)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(position[i]))return RF_RANGE;
    memcpy(value.position,position,12);value.radius=(float)(sqrt((double)damage)*(double).05f);
    if(!isfinite(value.radius))return RF_RANGE;
    value.life=.5f;value.bitmap=bitmap;value.frame_count=frame_count;value.color=0xff7f7f7fu;
    *result=value;return RF_OK;
}

int rf_particle_initialize(const rf_particle_spawn *spawn,uint32_t pool,
    uint32_t owner,uint32_t room,uint32_t emitter,rf_random_state *random,
    rf_particle *particle)
{
    rf_particle value;rf_random_state rng;uint32_t draw;unsigned i;
    if(!spawn || !random || !particle || pool>1)return RF_RANGE;
    value=*particle;rng=*random;
    value.owner=owner;
    for(i=0;i<3;i++) {
        value.position[i]=value.previous_position[i]=spawn->position[i];
        value.velocity[i]=spawn->velocity[i];
    }
    value.age=0;value.color=value.color_current=spawn->color;
    value.color_destination=spawn->color_destination;value.life=spawn->life;
    value.radius=spawn->radius;value.growth=spawn->growth;
    value.acceleration=spawn->acceleration;value.gravity=spawn->gravity_scale*9.8f;
    value.bitmap=spawn->bitmap;value.frame_count=(uint16_t)spawn->frame_count;
    value.secondary=(uint16_t)spawn->secondary;value.pool=(uint8_t)pool;
    value.orientation=0;
    if(spawn->flags&0x200u) {
        rf_random_next(&rng,&draw);
        value.orientation=(float)((double)6.283185482025146484375f*((double)draw/32768.0));
    }
    value.flags=spawn->flags|1u;if(!room)value.flags&=~0x10u;
    value.age_to_finish_vbm=spawn->age_to_finish_vbm;value.copied_48=spawn->copied_48;
    value.room=room;value.emitter=emitter;
    *particle=value;*random=rng;return RF_OK;
}
int rf_particle_emitter_tick(const rf_particle_cycle *cycle,uint32_t global_enabled,float dt,
    uint32_t timer_due,rf_random_state *random,rf_particle_emitter_clock *clock,
    rf_particle_emitter_actions *actions)
{
    rf_particle_emitter_clock next;rf_particle_emitter_actions out={0};rf_random_state rng;int status;
    if(!cycle || !random || !clock || !actions)return RF_RANGE;
    next=*clock;rng=*random;
    if(global_enabled&255u) {
        if(!isfinite(dt) || !isfinite(next.elapsed) || !isfinite(next.duration))return RF_RANGE;
        if(next.flags&0x20u) {
            next.elapsed+=dt;if(!isfinite(next.elapsed))return RF_RANGE;
            if(next.elapsed>=next.duration) {
                next.elapsed=0;next.enabled=(next.enabled&~255u)|((next.enabled&255u)==0);
                status=rf_particle_cycle_duration(cycle,next.enabled,&rng,&next.duration);if(status)return status;
                out.toggled=1;
            }
        }
        if(next.enabled&255u) {
            out.timer_checked=(next.flags&4u)==0;
            out.emit=!out.timer_checked || (timer_due&255u)!=0;
        }
    }
    *clock=next;*random=rng;*actions=out;return RF_OK;
}
int rf_particle_cycle_duration(const rf_particle_cycle *cycle,unsigned enabled,
    rf_random_state *random,float *duration)
{
    float center,variance,value;double offset;uint32_t draw;rf_random_state next;
    if(!cycle || !random || !duration)return RF_RANGE;
    center=(enabled&255u)?cycle->on_time:cycle->off_time;
    variance=(enabled&255u)?cycle->on_variance:cycle->off_variance;
    if(!isfinite(center) || !isfinite(variance))return RF_RANGE;
    next=*random;rf_random_next(&next,&draw);
    offset=((double)draw/32768.0)*variance;
    value=(float)(((double)center-variance)+(offset+offset));
    if(!isfinite(value))return RF_RANGE;
    if(value<0.1f)value=0.1f;
    *random=next;*duration=value;return RF_OK;
}
int rf_particle_definition_prepare(const rf_particle_definition *authored,
    rf_particle_definition *result)
{
    rf_particle_definition value;double x,y,z,inverse;
    if(!authored || !result)return RF_RANGE;
    value=*authored;x=value.direction[0];y=value.direction[1];z=value.direction[2];
    inverse=1.0/sqrt((x*x+y*y)+z*z);
    value.direction[0]=(float)(x*inverse);value.direction[1]=(float)(y*inverse);value.direction[2]=(float)(z*inverse);
    *result=value;return RF_OK;
}
static int particle_contains(const char *text,const char *needle)
{
    for(;*text;++text) {
        const unsigned char *a=(const unsigned char *)text,*b=(const unsigned char *)needle;
        while(*a && *b) {
            unsigned x=*a,y=*b;if(x>='A' && x<='Z')x+=32;if(y>='A' && y<='Z')y+=32;
            if(x!=y)break;++a;++b;
        }
        if(!*b)return 1;
    }
    return 0;
}
int rf_particle_flags_read(const char *emitter,const char *particle,rf_particle_text_flags *result)
{
    static const struct {const char *text;unsigned bits,secondary;} entries[]={
        {"glow",2,0},{"clr_change",4,0},{"gravity",8,0},{"collide",16,0},
        {"collide_liquid",0x400,0},{"collide_and_die",0x800,0},{"wind",0xf0000000u,0},
        {"accelerate",0x40,0},{"loop",0x100,0},{"explode",0x80,0},{"random_orient",0x200,0},
        {"vel_stretch",0x4000,0},{"no_z_check",0x2000,0},
        {"damages",1,1},{"hold_last_frame",4,1},{"fire_damage",8,1}};
    rf_particle_text_flags value={0};unsigned i;
    if(!emitter || !particle || !result)return RF_RANGE;
    if(particle_contains(emitter,"immediate"))value.emitter|=2;
    if(particle_contains(emitter,"continuous"))value.emitter|=4;
    if(particle_contains(emitter,"dirdepend"))value.emitter|=8;
    if(particle_contains(emitter,"dont_move_with_parent"))value.emitter|=0x40;
    if(particle_contains(emitter,"accel_with_parent"))value.emitter|=0x80;
    for(i=0;i<sizeof(entries)/sizeof(entries[0]);++i)if(particle_contains(particle,entries[i].text)) {
        if(entries[i].secondary)value.secondary|=entries[i].bits;else value.particle|=entries[i].bits;
    }
    *result=value;return RF_OK;
}
int rf_particle_flags_pack(rf_particle_text_flags *flags,unsigned present,const int values[4])
{
    unsigned i;
    if(!flags || !values)return RF_RANGE;
    for(i=0;i<3;++i)if(present&(1u<<i))flags->particle|=((unsigned)values[i]&15u)<<(16+4*i);
    if(present&8u)flags->secondary|=((unsigned)values[3]&15u)<<12;
    return RF_OK;
}
int rf_particle_cycle_read(rf_particle_text_flags *flags,unsigned initially_on,
    unsigned alternate,const rf_particle_cycle *authored,rf_particle_cycle *result)
{
    rf_particle_cycle value={1,0,1,0};
    if(!flags || !authored || !result)return RF_RANGE;
    if((initially_on&255u)==1u)flags->emitter|=0x10;
    if((alternate&255u)==1u) {flags->emitter|=0x20;value=*authored;}
    *result=value;return RF_OK;
}
int rf_vclip_name_lookup(const char *const names[64],const char *name)
{
    unsigned i;if(!names || !name || !*name)return -1;
    for(i=0;i<64;++i) {
        const unsigned char *a=(const unsigned char *)names[i],*b=(const unsigned char *)name;
        if(!a)continue;
        for(;;) {
            unsigned x=*a++,y=*b++;
            if(x>='A' && x<='Z')x+='a'-'A';
            if(y>='A' && y<='Z')y+='a'-'A';
            if(x!=y)break;
            if(!x)return (int)i;
        }
    }
    return -1;
}
int rf_effect_set_enabled(rf_effect_pair *pairs,uint32_t count,int32_t index,
    uint32_t override_mode,int32_t enabled,int32_t now_ms)
{
    rf_effect_switch **objects; int32_t stamp; int status; unsigned i;
    if (!pairs || index<0 || (uint32_t)index>=count) return RF_RANGE;
    objects=pairs[index].objects[(uint8_t)override_mode!=0];
    if (!objects[0] || !objects[1]) return RF_OK;
    for (i=0;i<2;++i) {
        if (!enabled) objects[i]->enabled=0;
        else if (objects[i]->enabled!=1) {
            status=rf_timer_set(&stamp,now_ms,0); if (status!=RF_OK) return status;
            objects[i]->enabled=1; objects[i]->started=stamp;
        }
    }
    return RF_OK;
}

#include "rf/corpse_effect.h"
static void corpse_surface_unlink(rf_corpse_surface_effect **head,rf_corpse_surface_effect *node)
{
    if(node->next==node)*head=NULL;
    else {
        if(*head==node)*head=node->next;
        node->previous->next=node->next;node->next->previous=node->previous;
    }
    node->next=node->previous=NULL;
}
static void corpse_surface_append(rf_corpse_surface_effect **head,rf_corpse_surface_effect *node)
{
    if(!*head){node->next=node->previous=node;*head=node;}
    else {
        node->next=*head;node->previous=(*head)->previous;
        (*head)->previous->next=node;(*head)->previous=node;
    }
}
/*4fcfa0, also used by the reconstructed cone orientation above. */
static void corpse_surface_basis(const float normal[3],float basis[9])
{
    double inverse;uint32_t i;
    memset(basis,0,36);memcpy(basis+6,normal,12);
    if(normal[0]<.0001f && normal[0]>-.0001f && normal[2]<.0001f && normal[2]>-.0001f) {
        basis[0]=1;basis[6]=basis[8]=0;basis[7]=normal[1]<0?-1.0f:1.0f;basis[5]=-basis[7];
    } else {
        basis[0]=normal[2];basis[2]=-normal[0];
        inverse=1.0/sqrt(((double)basis[0]*basis[0]+(double)basis[1]*basis[1])+(double)basis[2]*basis[2]);
        for(i=0;i<3;++i)basis[i]=(float)((double)basis[i]*inverse);
        for(i=0;i<3;++i)basis[3+i]=(float)((double)normal[(i+1)%3]*basis[(i+2)%3]-(double)normal[(i+2)%3]*basis[(i+1)%3]);
    }
}
int rf_corpse_surface_create(rf_corpse_surface_pool *pool,const rf_corpse_surface_source *source,
    uint32_t enabled,const char *attachment,float growth_time,float max_extent,
    const rf_corpse_surface_backend *backend)
{
    rf_corpse_surface_effect *node,*walk;rf_corpse_surface_hit hit;
    uint32_t metadata,seen=0,matched=0,color,i,descriptor;int32_t index;int status;
    float greatest=-1,point[3],basis[9],offset,rate;
    if(!enabled)return RF_OK;
    if(!pool || !source || !attachment || !backend || !backend->metadata || !backend->lookup ||
       !backend->place || !backend->surface || !backend->color || !pool->capacity ||
       !isfinite(growth_time) || growth_time<=0 || !isfinite(max_extent))return RF_RANGE;
    descriptor=source->descriptor;metadata=backend->metadata(backend->context,source->model);
    if(!descriptor || !metadata)return RF_OK;
    node=pool->free;
    if(!node) {
        walk=pool->active;
        if(!walk)return RF_RANGE;
        do {
            if(!walk || ++seen>pool->capacity || !isfinite(walk->elapsed))return RF_RANGE;
            if(walk->elapsed>greatest){greatest=walk->elapsed;node=walk;}
            walk=walk->next;
        } while(walk!=pool->active);
        if(!node)return RF_RANGE;
        corpse_surface_unlink(&pool->active,node);corpse_surface_append(&pool->free,node);
    }
    index=backend->lookup(backend->context,metadata,attachment,0);
    if(index==-1)index=backend->lookup(backend->context,metadata,attachment,1);
    if(index==-1)return RF_OK;
    status=backend->place(backend->context,source,index,point);if(status)return status;
    for(i=0;i<3;++i)if(!isfinite(point[i]))return RF_RANGE;
    status=backend->surface(backend->context,descriptor,point,&hit,&matched);if(status)return status;
    if(!matched)return RF_OK;
    for(i=0;i<3;++i)if(!isfinite(hit.point[i]) || !isfinite(hit.normal[i]))return RF_RANGE;
    corpse_surface_basis(hit.normal,basis);
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_RANGE;
    for(i=0;i<3;++i) {
        offset=(float)((double)hit.normal[i]*(double).01f);
        point[i]=(float)((double)hit.point[i]+offset);
        if(!isfinite(point[i]))return RF_RANGE;
    }
    memcpy(node->position,point,12);
    status=backend->color(backend->context,hit.face,hit.point,&color);if(status)return status;
    node->color=color;memcpy(node->basis,basis,36);
    rate=(float)((double)1.5707963705062866211f/growth_time);
    node->descriptor=descriptor;node->elapsed=0;node->max_extent=max_extent;
    node->growth_time=growth_time;node->extent=0;node->growth_rate=rate;
    corpse_surface_unlink(&pool->free,node);corpse_surface_append(&pool->active,node);
    return RF_OK;
}
int rf_corpse_source_effects(rf_corpse_surface_pool *pool,const rf_corpse_surface_source *source,
    const uint32_t *flags,uint32_t enabled,const rf_corpse_surface_backend *backend)
{
    int status;if(!flags)return RF_RANGE;
    if(*flags&0x08000000u) {
        status=rf_corpse_surface_create(pool,source,enabled,"eye",5,.25f,backend);if(status)return status;
    }
    if(*flags&0x10000000u)return rf_corpse_surface_create(pool,source,enabled,"spine",8,.5f,backend);
    return RF_OK;
}

int rf_corpse_surface_reset(rf_corpse_surface_pool *pool,rf_corpse_surface_effect slots[RF_CORPSE_SURFACE_CAPACITY])
{
    uint32_t i;if(!pool || !slots)return RF_RANGE;
    for(i=0;i<RF_CORPSE_SURFACE_CAPACITY;++i) {
        slots[i].next=slots+(i+1)%RF_CORPSE_SURFACE_CAPACITY;
        slots[i].previous=slots+(i+RF_CORPSE_SURFACE_CAPACITY-1)%RF_CORPSE_SURFACE_CAPACITY;
    }
    pool->free=slots;pool->active=NULL;pool->capacity=RF_CORPSE_SURFACE_CAPACITY;return RF_OK;
}
int rf_corpse_surface_tick(rf_corpse_surface_pool *pool,float dt)
{
    rf_corpse_surface_effect *node;uint32_t count=0;
    if(!pool || !isfinite(dt) || dt<0)return RF_RANGE;
    node=pool->active;
    while(node) {
        if(count++>=pool->capacity)return RF_RANGE;
        node->elapsed=(float)((double)node->elapsed+dt);
        node=node->next;if(node==pool->active)break;
    }
    return RF_OK;
}

int rf_corpse_surface_build_quad(rf_corpse_surface_effect *effect,rf_corpse_surface_quad *quad)
{
    static const float uv[4][2]={{0,0},{1,0},{1,1},{0,1}};
    static const int signs[4][2]={{-1,1},{1,1},{1,-1},{-1,-1}};
    rf_corpse_surface_quad result;float extent;uint32_t i,j;
    if(!effect || !quad || !isfinite(effect->elapsed) || effect->elapsed<0 ||
       !isfinite(effect->growth_time) || effect->growth_time<=0 ||
       !isfinite(effect->growth_rate) || effect->growth_rate<0 ||
       !isfinite(effect->max_extent) || effect->max_extent<0)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(effect->position[i]))return RF_RANGE;
    for(i=0;i<6;++i)if(!isfinite(effect->basis[i]))return RF_RANGE;
    extent=effect->elapsed<effect->growth_time?
        (float)(sin((double)effect->growth_rate*effect->elapsed)*effect->max_extent):effect->max_extent;
    for(i=0;i<4;++i)for(j=0;j<3;++j) {
        float a=(float)((double)effect->basis[j]*extent);
        float b=(float)((double)effect->basis[j+3]*extent);
        float center=(float)((double)effect->position[j]+signs[i][0]*(double)a);
        result.vertices[i][j]=(float)((double)center+signs[i][1]*(double)b);
        if(!isfinite(result.vertices[i][j]))return RF_RANGE;
    }
    memcpy(result.uv,uv,sizeof(uv));result.color=effect->color|0xff000000u;
    effect->extent=extent;*quad=result;return RF_OK;
}

int rf_corpse_surface_collect_room(const rf_corpse_surface_pool *pool,uint32_t descriptor,
    const rf_corpse_surface_queue *queue)
{
    const rf_corpse_surface_effect *node;uint32_t visited=0,i;
    if(!pool || !queue || !queue->frustum || !queue->count || !queue->callback ||
       queue->capacity>2048 || *queue->count>queue->capacity ||
       (queue->capacity && !queue->records))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(queue->world_offset[i]))return RF_RANGE;
    node=pool->active;
    while(node) {
        if(visited++>=pool->capacity)return RF_RANGE;
        if(node->descriptor==descriptor) {
            rf_render_queue_record record={0};float cull[3];uint32_t accepted;int status;
            if((uintptr_t)node>UINT32_MAX)return RF_RANGE;
            record.object=(uint32_t)(uintptr_t)node;
            memcpy(record.position,node->position,sizeof(record.position));record.radius=node->extent;
            record.sorted=1;record.lighting_flag=1;record.callback=queue->callback;
            for(i=0;i<3;++i)cull[i]=(float)((double)node->position[i]+queue->world_offset[i]);
            status=rf_render_queue_append(queue->frustum,cull,&record,queue->records,
                queue->capacity,queue->count,&accepted);if(status)return status;
        }
        node=node->next;if(node==pool->active)break;
    }
    return RF_OK;
}

int rf_corpse_surface_prepare_draw(rf_corpse_surface_effect *effect,const rf_visibility_camera *camera,
    const rf_particle_vertex_environment *environment,rf_particle_draw_vertex *vertices,uint32_t *count)
{
    rf_corpse_surface_quad quad;rf_particle_billboard_vertex world[4];
    rf_particle_screen_polygon polygon;rf_particle_draw_vertex result[12];
    rf_particle_vertex_environment color;uint32_t i;int status;
    if(!effect || !camera || !environment || !vertices || !count)return RF_RANGE;
    status=rf_corpse_surface_build_quad(effect,&quad);if(status)return status;
    for(i=0;i<4;++i){memcpy(world[i].position,quad.vertices[i],12);memcpy(world[i].uv,quad.uv[i],8);}
    status=rf_particle_world_quad(camera,world,&polygon);if(status)return status;
    color=*environment;color.rgba=quad.color;
    for(i=0;i<polygon.count;++i){status=rf_particle_vertex_encode(&color,polygon.vertices+i,result+i);if(status)return status;}
    memcpy(vertices,result,polygon.count*sizeof(*vertices));*count=polygon.count;return RF_OK;
}

int rf_corpse_surface_draw(rf_corpse_surface_effect *effect,const rf_visibility_camera *camera,
    const rf_particle_vertex_environment *environment,const rf_image *image,uint32_t mode,
    rf_corpse_surface_draw_sink sink,void *context)
{
    rf_particle_draw_vertex vertices[12];uint32_t count;int status;
    if(!sink || !image || !image->rgba || !image->width || !image->height)return RF_RANGE;
    status=rf_corpse_surface_prepare_draw(effect,camera,environment,vertices,&count);if(status)return status;
    if(!count)return RF_OK;
    return sink(context,vertices,count,image,mode);
}

int rf_vfx_header_read(const void *data,uint32_t bytes,rf_vfx_header *result)
{
    const unsigned char *p=data;rf_vfx_header v={0};uint32_t i,at=8;
    static const uint32_t since[30]={0x30008,0,0,0,0,0,0,0,0x3000f,0x40000,
        0x40002,0x40003,0x40005,0,0,0,0,0,0x3000d,0x30009,0x30009,
        0x30009,0x30009,0x30009,0,0,0,0,0,0x3000f};
    if(!data || !result || bytes<8)return RF_RANGE;
    if(memcmp(p,"VSFX",4))return RF_FORMAT;
    v.version=(uint32_t)p[4]|((uint32_t)p[5]<<8)|((uint32_t)p[6]<<16)|((uint32_t)p[7]<<24);
    if(v.version<0x30000 || v.version>0x7fffffffu || (v.version>=0x40000 && v.version<0x40005))return RF_FORMAT;
    for(i=0;i<30;++i) {
        if(i==13 && v.version<0x3000a) {
            if(bytes-at<4)return RF_FORMAT;at+=4;
        }
        if(v.version>=since[i]) {
            if(bytes-at<4)return RF_FORMAT;
            v.values[i]=(uint32_t)p[at]|((uint32_t)p[at+1]<<8)|((uint32_t)p[at+2]<<16)|((uint32_t)p[at+3]<<24);at+=4;
        } else if(i==10)v.values[i]=0x80;
        else if(i==18)v.values[i]=v.values[2];
        else if(i==19)v.values[i]=v.values[17];
    }
    if(v.version<0x40000)v.values[9]=v.values[14]+1u+v.values[5];
    if(v.version<0x40003)v.values[11]=v.values[9];
    if(v.version<0x40005)v.values[12]=v.values[9]*(v.values[1]+(v.version<0x3000c?1u:0u));
    v.bytes=at;*result=v;return RF_OK;
}

static uint32_t vfx_word(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
void rf_vfx_directory_close(rf_vfx_directory *owner)
{if(owner){free(owner->chunks);memset(owner,0,sizeof(*owner));}}
int rf_vfx_directory_open(rf_vpp *archive,const char *name,uint32_t budget,rf_vfx_directory *out)
{
    rf_vfx_directory v={0},empty={0};unsigned char raw[128];uint32_t size,at,pass,i;int status;
    if(!archive || !name || !out || memcmp(out,&empty,sizeof(empty)) || budget<sizeof(v))return RF_RANGE;
    status=rf_vpp_find(archive,name,&v.entry);if(status)return status;
    size=v.entry.size<sizeof(raw)?v.entry.size:sizeof(raw);
    status=rf_vpp_read(archive,&v.entry,0,raw,size);if(status)return status;
    status=rf_vfx_header_read(raw,size,&v.header);if(status)return status;
    v.archive=archive;
    for(pass=0;pass<2;++pass) {
        at=v.header.bytes;i=0;
        while(at<v.entry.size) {
            uint32_t length;
            if(v.entry.size-at<8){status=RF_FORMAT;goto failed;}
            status=rf_vpp_read(archive,&v.entry,at,raw,8);if(status)goto failed;
            length=vfx_word(raw+4);
            if(length<4 || length>v.entry.size-at-4){status=RF_FORMAT;goto failed;}
            if(sizeof(v)+(uint64_t)(i+1)*sizeof(*v.chunks)>budget){status=RF_RANGE;goto failed;}
            if(pass) {
                if(i>=v.count){status=RF_FORMAT;goto failed;}
                v.chunks[i].type=vfx_word(raw);v.chunks[i].offset=at+8;v.chunks[i].bytes=length-4;
            }
            ++i;at+=4+length;
        }
        if(!pass) {
            v.count=i;
            if(i){v.chunks=malloc((size_t)i*sizeof(*v.chunks));if(!v.chunks){status=RF_IO;goto failed;}}
        } else if(i!=v.count){status=RF_FORMAT;goto failed;}
    }
    v.allocated_bytes=(uint32_t)(sizeof(v)+(uint64_t)v.count*sizeof(*v.chunks));*out=v;return RF_OK;
failed:
    rf_vfx_directory_close(&v);return status;
}
int rf_vfx_chunk_read(const rf_vfx_directory *owner,uint32_t index,uint32_t offset,void *out,uint32_t bytes)
{
    const rf_vfx_chunk *chunk;
    if(!owner || !owner->archive || !owner->chunks || !out)return RF_RANGE;
    if(index>=owner->count)return RF_NOT_FOUND;chunk=owner->chunks+index;
    if(offset>chunk->bytes || bytes>chunk->bytes-offset)return RF_RANGE;
    return rf_vpp_read(owner->archive,&owner->entry,chunk->offset+offset,out,bytes);
}

int rf_vfx_face_read(const void *data,uint32_t bytes,uint32_t version,rf_vfx_face *out)
{
    const unsigned char *p=data;rf_vfx_face v={0};uint32_t at=0,i,j;float color;double scaled;
    if(!data || !out)return RF_RANGE;
    if(version<0x30000 || version>0x7fffffffu || (version>=0x40000 && version<0x40005))return RF_FORMAT;
    v.bytes=version<0x3000d?120:96;if(bytes<v.bytes)return RF_FORMAT;
    for(i=0;i<3;++i){v.indices[i]=vfx_word(p+at);at+=4;}
    if(version<0x3000d)for(i=0;i<3;++i)for(j=0;j<2;++j) {
        uint32_t word=vfx_word(p+at);at+=4;memcpy(v.legacy_uv+j*3+i,&word,4);
        if(!isfinite(v.legacy_uv[j*3+i]))return RF_RANGE;
    }
    for(i=0;i<3;++i)for(j=0;j<3;++j) {
        uint32_t word=vfx_word(p+at);at+=4;memcpy(&color,&word,4);scaled=(double)color*255.0;
        if(!isfinite(scaled) || scaled< -2147483648.0 || scaled>=2147483648.0)return RF_RANGE;
        v.colors[j*3+i]=(unsigned char)(int32_t)scaled;
    }
    for(i=0;i<3;++i){uint32_t word=vfx_word(p+at);at+=4;memcpy(v.vector_60+i,&word,4);if(!isfinite(v.vector_60[i]))return RF_RANGE;}
    for(i=0;i<3;++i){uint32_t word=vfx_word(p+at);at+=4;memcpy(v.vector_6c+i,&word,4);if(!isfinite(v.vector_6c[i]))return RF_RANGE;}
    {uint32_t word=vfx_word(p+at);at+=4;memcpy(&v.scalar_78,&word,4);if(!isfinite(v.scalar_78))return RF_RANGE;}
    v.material=vfx_word(p+at);at+=4;if(version<0x40000)--v.material;
    for(i=0;i<4;++i){v.words_80[i]=vfx_word(p+at);at+=4;}
    *out=v;return RF_OK;
}

int rf_vfx_mesh_timing_read(const void *data,uint32_t bytes,uint32_t version,uint32_t flags,rf_vfx_mesh_timing *out)
{
    const unsigned char *p=data;rf_vfx_mesh_timing v={0};uint32_t at=0,rate=15,a,b;
    if(!data || !out)return RF_RANGE;
    if(version<0x30000 || version>0x7fffffffu || (version>=0x40000 && version<0x40005))return RF_FORMAT;
    v.bytes=(version>=0x30009?4:0)+(version>=0x40004?12:8);if(bytes<v.bytes)return RF_FORMAT;
    if(version>=0x30009){rate=vfx_word(p);at=4;}
    v.flags=((flags&3u)|(rate<<2))&0xffffu;rate=v.flags>>2;
    a=vfx_word(p+at);b=vfx_word(p+at+4);
    if(version<0x40004) {
        int32_t first,last;if(!rate)return RF_RANGE;memcpy(&first,&a,4);memcpy(&last,&b,4);
        v.start=(float)((double)first/(double)rate);v.end=(float)((double)last/(double)rate);
        v.samples=b-a+(version>=0x3000c?1u:0u);
    } else {
        memcpy(&v.start,&a,4);memcpy(&v.end,&b,4);v.samples=vfx_word(p+at+8);
        if(!isfinite(v.start) || !isfinite(v.end))return RF_RANGE;
    }
    *out=v;return RF_OK;
}
int rf_vfx_mesh_prefix_read(const void *data,uint32_t bytes,uint32_t version,rf_vfx_mesh_prefix *out)
{
    const unsigned char *p=data;rf_vfx_mesh_prefix v={0};uint32_t at=0,n,i,enabled,face_bytes;const unsigned char *end,*parent;
    int status;rf_vfx_face face;
    if(!data || !out)return RF_RANGE;
    if(version<0x30000 || version>0x7fffffffu || (version>=0x40000 && version<0x40005))return RF_FORMAT;
    end=memchr(p,0,bytes);if(!end || (uint32_t)(end-p)>64)return RF_FORMAT;
    n=(uint32_t)(end-p);memcpy(v.name,p,n);at=n+1;
    end=memchr(p+at,0,bytes-at);if(!end || (uint32_t)(end-(p+at))>255)return RF_FORMAT;
    parent=p+at;n=(uint32_t)(end-parent);at+=n+1;
    if(!n)memcpy(v.parent,"Scene Root",11);
    else {
        const unsigned char *dash=memchr(parent,'-',n);if(dash)parent=dash+1;
        n=(uint32_t)(end-parent);if(n>64)return RF_FORMAT;memcpy(v.parent,parent,n);
    }
    if(bytes-at<5)return RF_FORMAT;enabled=p[at++]!=0;v.vertices=vfx_word(p+at);at+=4;
    if(version<0x3000a) {
        uint64_t skip=(uint64_t)v.vertices*12;if(skip>bytes-at)return RF_FORMAT;at+=(uint32_t)skip;
    }
    if(bytes-at<4)return RF_FORMAT;v.faces=vfx_word(p+at);at+=4;v.face_offset=at;
    face_bytes=version<0x3000d?120:96;if(v.faces>(bytes-at)/face_bytes)return RF_FORMAT;
    for(i=0;i<v.faces;++i) {
        uint32_t j;status=rf_vfx_face_read(p+at,face_bytes,version,&face);if(status)return status;
        for(j=0;j<3;++j)if(face.indices[j]>=v.vertices)return RF_FORMAT;at+=face_bytes;
    }
    status=rf_vfx_mesh_timing_read(p+at,bytes-at,version,enabled,&v.timing);if(status)return status;
    v.bytes=at+v.timing.bytes;*out=v;return RF_OK;
}

typedef struct vfx_material_cursor {const unsigned char *data;uint32_t bytes,at;int failed;} vfx_material_cursor;
static uint32_t vfx_material_word(vfx_material_cursor *c)
{
    uint32_t v;if(c->failed || c->bytes-c->at<4){c->failed=1;return 0;}
    v=vfx_word(c->data+c->at);c->at+=4;return v;
}
static uint32_t vfx_material_byte(vfx_material_cursor *c)
{if(c->failed || c->at==c->bytes){c->failed=1;return 0;}return c->data[c->at++];}
static void vfx_material_string(vfx_material_cursor *c,char *out)
{
    const unsigned char *end;uint32_t n;if(c->failed)return;
    end=memchr(c->data+c->at,0,c->bytes-c->at);
    if(!end || (n=(uint32_t)(end-(c->data+c->at)))>32){c->failed=1;return;}
    memcpy(out,c->data+c->at,n+1);c->at+=n+1;
}
static void vfx_material_array(vfx_material_cursor *c,uint32_t count,uint32_t *offset)
{
    if(c->failed || count>0x7fffffffu || count>(c->bytes-c->at)/4){c->failed=1;return;}
    *offset=c->at;c->at+=count*4;
}
int rf_vfx_material_read(const void *data,uint32_t bytes,uint32_t version,rf_vfx_material_view *out)
{
    rf_vfx_material_view v={0};vfx_material_cursor c={data,bytes,0,0};char *name;uint32_t i,type;
    if(!data || !out)return RF_RANGE;
    if(version<0x30000 || version>0x7fffffffu || (version>=0x40000 && version<0x40005))return RF_FORMAT;
    v.words[4]=v.words[17]=v.words[45]=UINT32_MAX;
    type=v.words[0]=vfx_material_word(&c);v.words[30]=version>=0x40003?vfx_material_word(&c):15;
    if(type>2)goto done;
    if(type==2) {
        unsigned char *color=(unsigned char *)(v.words+2);
        color[0]=(unsigned char)(version>=0x40006?(vfx_material_byte(&c)!=0):1);
        for(i=1;i<4;++i)color[i]=(unsigned char)vfx_material_word(&c);
    } else {
        ((unsigned char *)(v.words+2))[0]=(unsigned char)(vfx_material_byte(&c)!=0);
        name=(char *)(v.words+5);vfx_material_string(&c,name);
        if(!strcmp(name,"$original_map_rgb"))v.words[1]|=2;
        else if(strcmp(name,"$original_map"))v.bitmap_requests|=1;
        for(i=14;i<17;++i)v.words[i]=vfx_material_word(&c);
        if(type==1) {
            name=(char *)(v.words+18);vfx_material_string(&c,name);
            if(!strcmp(name,"$original_map_rgb"))v.words[1]|=4;
            else if(*name && strcmp(name,"$original_map"))v.bitmap_requests|=2;
            for(i=27;i<30;++i)v.words[i]=vfx_material_word(&c);
            v.words[31]=vfx_material_word(&c);if(version<0x40003)v.words[30]=vfx_material_word(&c);
            if(v.words[31])vfx_material_array(&c,v.words[31],v.words+32);
        } else v.words[28]=0x3f800000;
        for(i=33;i<36;++i)v.words[i]=vfx_material_word(&c);
        name=(char *)(v.words+36);vfx_material_string(&c,name);if(*name)v.bitmap_requests|=4;
    }
    v.words[46]=version>=0x40003?vfx_material_word(&c):1;
    vfx_material_array(&c,v.words[46],v.words+47);
    if(version>=0x40005){v.words[48]=vfx_material_word(&c);vfx_material_array(&c,v.words[48],v.words+49);}
done:
    if(c.failed)return RF_FORMAT;v.bytes=c.at;*out=v;return RF_OK;
}
int rf_vfx_material_sample(const void *data,uint32_t bytes,const rf_vfx_material_view *view,
    uint32_t track,uint32_t index,uint32_t *out)
{
    static const uint32_t counts[3]={31,46,48},offsets[3]={32,47,49};uint32_t at,word;float value;
    if(!data || !view || !out || track>2)return RF_RANGE;
    if(index>=view->words[counts[track]])return RF_NOT_FOUND;
    at=view->words[offsets[track]];
    if(at>bytes || (uint64_t)index*4+4>bytes-at)return RF_RANGE;
    word=vfx_word((const unsigned char *)data+at+index*4);
    if(!track) {
        memcpy(&value,&word,4);if(!isfinite(value))return RF_RANGE;
        if(value<=0)value=0;if(value>1)value=1;memcpy(&word,&value,4);
    }
    *out=word;return RF_OK;
}

int rf_vfx_embedded_material_read(const void *data,uint32_t bytes,uint32_t version,
    uint32_t mesh_flags,uint32_t samples,rf_vfx_embedded_material_view *out)
{
    rf_vfx_embedded_material_view v={0};rf_vfx_material_view *m=&v.material;
    vfx_material_cursor c={data,bytes,0,0};uint32_t i,type;char *name;
    if(!data || !out || samples>0x7fffffffu)return RF_RANGE;
    if(version<0x30000 || version>=0x40000)return RF_FORMAT;
    m->words[4]=m->words[17]=m->words[45]=UINT32_MAX;m->words[30]=(mesh_flags&0xffffu)>>2;
    type=m->words[0]=vfx_material_word(&c);
    if(type==2) {
        unsigned char *color=(unsigned char *)m->words+9;
        for(i=0;i<3;++i)color[i]=(unsigned char)vfx_material_word(&c);color[3]=255;
    } else {
        if(version>=0x30003)((unsigned char *)(m->words+2))[0]=(unsigned char)(vfx_material_byte(&c)!=0);
        name=(char *)(m->words+5);vfx_material_string(&c,name);m->bitmap_requests|=1;
        if(version>=0x30012)for(i=14;i<17;++i)m->words[i]=vfx_material_word(&c);
        else m->words[15]=0x3f800000;
        if(type==1) {
            name=(char *)(m->words+18);vfx_material_string(&c,name);if(*name)m->bitmap_requests|=2;
            if(version>=0x30012)for(i=27;i<30;++i)m->words[i]=vfx_material_word(&c);
            else m->words[28]=0x3f800000;
        } else m->words[28]=0x3f800000;
        if(version<0x30012) {
            m->words[14]=m->words[27]=vfx_material_word(&c);
            m->words[16]=m->words[29]=vfx_material_word(&c);
        }
        if(version>=0x30007) {
            for(i=33;i<36;++i)m->words[i]=vfx_material_word(&c);
            name=(char *)(m->words+36);vfx_material_string(&c,name);if(*name)m->bitmap_requests|=4;
        }
        if(type==1){m->words[31]=samples;if(samples)vfx_material_array(&c,samples,m->words+32);}
    }
    m->words[46]=1;m->words[47]=UINT32_MAX;
    if(version>=0x30011)v.color_word=vfx_material_word(&c);
    m->words[48]=samples;m->words[49]=UINT32_MAX;
    if(c.failed)return RF_FORMAT;m->bytes=c.at;*out=v;return RF_OK;
}

int rf_vfx_edge_read(const void *data,uint32_t bytes,uint32_t faces,rf_vfx_edge_view *out)
{
    rf_vfx_edge_view v={0};vfx_material_cursor c={data,bytes,0,0};uint32_t i;float value;
    if(!data || !out)return RF_RANGE;
    v.words[0]=vfx_material_word(&c);v.words[1]=vfx_material_word(&c);
    for(i=7;i<9;++i){v.words[i]=vfx_material_word(&c);memcpy(&value,v.words+i,4);if(!isfinite(value))return RF_FORMAT;}
    v.words[9]=vfx_material_word(&c);v.words[10]=c.at;
    if(c.failed || v.words[9]>0x7fffffffu || (uint64_t)v.words[9]*4>bytes-c.at)return RF_FORMAT;
    for(i=0;i<v.words[9];++i)if(vfx_material_word(&c)>=faces)return RF_FORMAT;
    if(c.failed)return RF_FORMAT;v.bytes=c.at;*out=v;return RF_OK;
}
int rf_vfx_mesh_edges_read(const void *data,uint32_t bytes,uint32_t version,
    uint32_t initial_flags,uint32_t mesh_flags,uint32_t faces,rf_vfx_mesh_edges *out)
{
    rf_vfx_mesh_edges v={0};vfx_material_cursor c={data,bytes,0,0};uint32_t i,word;rf_vfx_edge_view edge;
    if(!data || !out)return RF_RANGE;
    if(version<0x30000 || version>0x7fffffffu || (version>=0x40000 && version<0x40005))return RF_FORMAT;
    for(i=0;i<3;++i){word=vfx_material_word(&c);memcpy(v.center+i,&word,4);if(!isfinite(v.center[i]))return RF_FORMAT;}
    word=vfx_material_word(&c);memcpy(&v.radius,&word,4);if(!isfinite(v.radius))return RF_FORMAT;
    v.flags=initial_flags;
    if(version<0x30002){word=vfx_material_word(&c);v.flags|=(word&3)<<4;}
    v.flags|=vfx_material_word(&c);v.legacy[0]=v.legacy[1]=-1;
    if(version==0x3000a && (v.flags&1))for(i=0;i<2;++i){word=vfx_material_word(&c);memcpy(v.legacy+i,&word,4);if(!isfinite(v.legacy[i]))return RF_FORMAT;}
    v.count=vfx_material_word(&c);v.edge_offset=c.at;
    if(c.failed || v.count>0x7fffffffu || (uint64_t)v.count*20>bytes-c.at)return RF_FORMAT;
    for(i=0;i<v.count;++i){if(rf_vfx_edge_read(c.data+c.at,bytes-c.at,faces,&edge))return RF_FORMAT;c.at+=edge.bytes;}
    word=version>=0x30009?(vfx_material_byte(&c)!=0):0;
    v.mesh_flags=((mesh_flags&0xfffdu)|(word<<1))&0xffffu;
    if(c.failed)return RF_FORMAT;v.bytes=c.at;*out=v;return RF_OK;
}

static void vfx_frame_floats(vfx_material_cursor *c,float *out,uint32_t count)
{
    uint32_t i,word;for(i=0;i<count;++i){word=vfx_material_word(c);memcpy(out+i,&word,4);if(!isfinite(out[i]))c->failed=1;}
}
int rf_vfx_frame_read(const void *data,uint32_t bytes,const rf_vfx_frame_config *cfg,rf_vfx_frame_view *out)
{
    rf_vfx_frame_view v={0};vfx_material_cursor c={data,bytes,0,0};uint64_t span;uint32_t i;float value;
    if(!data || !cfg || !out)return RF_RANGE;
    if(cfg->version<0x30000 || cfg->version>0x7fffffffu || (cfg->version>=0x40000 && cfg->version<0x40005))return RF_FORMAT;
    if((cfg->flags&4) || cfg->index==0) {
        v.present|=1;vfx_frame_floats(&c,v.vectors,6);v.vertex_offset=c.at;span=(uint64_t)cfg->vertices*6;
        if(c.failed || span>bytes-c.at)return RF_FORMAT;v.vertex_bytes=(uint32_t)span;c.at+=(uint32_t)span;
        if(cfg->flags&0x801) {
            v.present|=2;
            if(cfg->version>=0x3000b)vfx_frame_floats(&c,v.extra,2);
            else {v.extra[0]=cfg->legacy[0];v.extra[1]=cfg->legacy[1];if(!isfinite(v.extra[0]) || !isfinite(v.extra[1]))return RF_FORMAT;}
            if((cfg->flags&0x800) && cfg->index==0){v.present|=4;if(cfg->version>=0x40001)vfx_frame_floats(&c,v.direction,3);}
        }
    }
    if(cfg->version>=0x3000d && ((cfg->flags&0x100) || cfg->index==0)) {
        v.present|=8;v.uv_offset=c.at;span=(uint64_t)cfg->faces*24;
        if(c.failed || span>bytes-c.at)return RF_FORMAT;v.uv_bytes=(uint32_t)span;
        for(i=0;i<v.uv_bytes/4;++i)vfx_frame_floats(&c,&value,1);
    }
    if(!(cfg->flags&4) && (!(cfg->mesh_flags&2) || (cfg->version<0x3000e && cfg->index==0))) {
        v.present|=16;vfx_frame_floats(&c,v.transform,10);
    }
    if(cfg->version<0x30009)(void)vfx_material_byte(&c);
    if(cfg->version<0x40005) {
        v.present|=32;vfx_frame_floats(&c,&v.opacity,1);
        if(v.opacity<=0)v.opacity=0;if(v.opacity>1)v.opacity=1;
    }
    if(c.failed)return RF_FORMAT;v.bytes=c.at;*out=v;return RF_OK;
}

int rf_vfx_key_read(const void *data,uint32_t bytes,uint32_t track,rf_vfx_key *out)
{
    rf_vfx_key v={0};vfx_material_cursor c={data,bytes,0,0};uint32_t i,word;float value;double scaled;int32_t integer;uint16_t half;
    if(!data || !out || track>2)return RF_RANGE;
    if(bytes<40)return RF_FORMAT;v.words[0]=vfx_material_word(&c);
    for(i=0;i<9;++i) {
        word=vfx_material_word(&c);memcpy(&value,&word,4);if(!isfinite(value))return RF_FORMAT;
        if(track!=1)v.words[i+1]=word;
        else {
            scaled=(double)value*(i<4?16383.0:1.0);
            if(scaled<-2147483648.0 || scaled>2147483647.0)return RF_RANGE;
            integer=(int32_t)scaled;
            if(i<4){half=(uint16_t)integer;memcpy((unsigned char *)v.words+4+i*2,&half,2);}
            else ((unsigned char *)v.words)[12+i-4]=(unsigned char)integer;
        }
    }
    *out=v;return RF_OK;
}
int rf_vfx_key_tracks_read(const void *data,uint32_t bytes,uint32_t version,
    const float *legacy_base7,rf_vfx_key_tracks *out)
{
    rf_vfx_key_tracks v={0};vfx_material_cursor c={data,bytes,0,0};rf_vfx_key key;uint32_t t,i;
    if(!data || !out || (version<0x3000a && !legacy_base7))return RF_RANGE;
    if(version<0x30000 || version>0x7fffffffu || (version>=0x40000 && version<0x40005))return RF_FORMAT;
    if(version>=0x3000a)vfx_frame_floats(&c,v.base,10);
    else {for(i=0;i<7;++i){if(!isfinite(legacy_base7[i]))return RF_FORMAT;v.base[i]=legacy_base7[i];}for(i=7;i<10;++i)v.base[i]=1;}
    for(t=0;t<3;++t) {
        v.counts[t]=vfx_material_word(&c)&0xffffu;v.offsets[t]=c.at;
        if(c.failed || (uint64_t)v.counts[t]*40>bytes-c.at)return RF_FORMAT;
        for(i=0;i<v.counts[t];++i){if(rf_vfx_key_read(c.data+c.at,bytes-c.at,t,&key))return RF_FORMAT;c.at+=40;}
    }
    if(c.failed)return RF_FORMAT;v.bytes=c.at;*out=v;return RF_OK;
}

void rf_vfx_mesh_close(rf_vfx_mesh **out)
{if(out){free(*out);*out=NULL;}}
int rf_vfx_mesh_open(const void *data,uint32_t bytes,uint32_t version,
    uint32_t global_materials,const float *legacy_base7,uint32_t budget,rf_vfx_mesh **out)
{
    rf_vfx_mesh_prefix prefix;rf_vfx_mesh *v;rf_vfx_face face;rf_vfx_embedded_material_view material;
    rf_vfx_frame_config cfg={0};uint64_t total;uint32_t at,i,j,face_bytes;int status;
    if(!data || !out || *out)return RF_RANGE;
    status=rf_vfx_mesh_prefix_read(data,bytes,version,&prefix);if(status)return status;
    total=sizeof(*v)+(uint64_t)prefix.timing.samples*sizeof(*v->frames)+bytes;
    if(total>budget)return RF_RANGE;
    v=malloc((size_t)total);if(!v)return RF_IO;memset(v,0,sizeof(*v));v->prefix=prefix;v->version=version;v->bytes=bytes;v->allocated_bytes=(uint32_t)total;
    v->frames=(rf_vfx_frame_view *)(v+1);v->data=(unsigned char *)(v->frames+prefix.timing.samples);memcpy(v->data,data,bytes);
    at=prefix.bytes;if(at>bytes || bytes-at<4){status=RF_FORMAT;goto failed;}
    v->materials=vfx_word(v->data+at);at+=4;v->material_offset=at;
    if(v->materials>0x7fffffffu || (uint64_t)v->materials*4>bytes-at){status=RF_FORMAT;goto failed;}
    for(i=0;i<v->materials;++i) {
        if(version<0x40000) {
            status=rf_vfx_embedded_material_read(v->data+at,bytes-at,version,prefix.timing.flags,prefix.timing.samples,&material);
            if(status)goto failed;at+=material.material.bytes;
        } else {if(vfx_word(v->data+at)>=global_materials){status=RF_FORMAT;goto failed;}at+=4;}
    }
    status=rf_vfx_mesh_edges_read(v->data+at,bytes-at,version,0,prefix.timing.flags,prefix.faces,&v->edges);if(status)goto failed;
    v->edges.edge_offset+=at;at+=v->edges.bytes;
    face_bytes=version<0x3000d?120:96;
    for(i=0;i<prefix.faces;++i) {
        uint32_t pos=prefix.face_offset+i*face_bytes;
        status=rf_vfx_face_read(v->data+pos,bytes-pos,version,&face);if(status)goto failed;
        if(face.material>=v->materials){status=RF_FORMAT;goto failed;}
        for(j=1;j<4;++j)if(face.words_80[j]>=v->edges.count){status=RF_FORMAT;goto failed;}
    }
    cfg.version=version;cfg.flags=v->edges.flags;cfg.mesh_flags=v->edges.mesh_flags;cfg.vertices=prefix.vertices;cfg.faces=prefix.faces;
    cfg.legacy[0]=v->edges.legacy[0];cfg.legacy[1]=v->edges.legacy[1];
    for(i=0;i<prefix.timing.samples;++i) {
        rf_vfx_frame_view *frame=v->frames+i;cfg.index=i;
        status=rf_vfx_frame_read(v->data+at,bytes-at,&cfg,frame);if(status)goto failed;
        if(frame->present&1)frame->vertex_offset+=at;if(frame->present&8)frame->uv_offset+=at;at+=frame->bytes;
    }
    if(v->edges.mesh_flags&2) {
        status=rf_vfx_key_tracks_read(v->data+at,bytes-at,version,legacy_base7,&v->keys);if(status)goto failed;
        for(i=0;i<3;++i)v->keys.offsets[i]+=at;at+=v->keys.bytes;
    }
    if(at!=bytes){status=RF_FORMAT;goto failed;}*out=v;return RF_OK;
failed:
    free(v);return status;
}

int rf_vfx_frame_select(const rf_vfx_mesh_timing *timing,uint32_t flags,float effect_frame,rf_vfx_frame_cursor *out)
{
    rf_vfx_frame_cursor v={0};float seconds;double position;uint32_t first;
    if(!timing || !out || !timing->samples || timing->samples>0x7ffffffeu || !isfinite(effect_frame) || !isfinite(timing->start))return RF_RANGE;
    seconds=(float)((double)effect_frame*(double)0.06666667014360428f);
    position=((double)seconds-(double)timing->start)*((timing->flags&0xffffu)>>2);v.position=(float)position;
    if(!isfinite(v.position))return RF_RANGE;
    if(position<0 || (double)timing->samples<(double)v.position){*out=v;return RF_OK;}
    if(v.position>=2147483647.0)return RF_RANGE;
    first=(uint32_t)floor((double)v.position);v.fraction=(flags&2)?0:(float)((double)v.position-first);
    v.first=first<timing->samples?first:timing->samples-1;
    v.second=first+1<timing->samples?first+1:timing->samples-1;v.active=1;*out=v;return RF_OK;
}
int rf_vfx_vertex_decode(const void *data,uint32_t bytes,const float vectors[6],float out[3])
{
    const unsigned char *p=data;float v[3],product;uint32_t i;int16_t component;
    if(!data || !vectors || !out || bytes<6)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(vectors[i]) || !isfinite(vectors[i+3]))return RF_RANGE;
        memcpy(&component,p+i*2,2);product=(float)((double)component*vectors[i+3]);v[i]=(float)((double)product+vectors[i]);
        if(!isfinite(v[i]))return RF_RANGE;
    }
    memcpy(out,v,sizeof(v));return RF_OK;
}
int rf_vfx_mesh_vertex(const rf_vfx_mesh *mesh,uint32_t frame,uint32_t vertex,float out[3])
{
    const rf_vfx_frame_view *view;uint64_t at;
    if(!mesh || !mesh->data || !mesh->frames || !out || frame>=mesh->prefix.timing.samples || vertex>=mesh->prefix.vertices)return RF_RANGE;
    view=mesh->frames+((mesh->edges.flags&4)?frame:0);
    if(!(view->present&1) || (uint64_t)vertex*6+6>view->vertex_bytes)return RF_FORMAT;
    at=(uint64_t)view->vertex_offset+(uint64_t)vertex*6;if(at+6>mesh->bytes)return RF_FORMAT;
    return rf_vfx_vertex_decode(mesh->data+(size_t)at,6,view->vectors,out);
}

int rf_vfx_uv_sample(const float first[6],const float second[6],float fraction,
    int interpolate,float out[6])
{
    float v[6];uint32_t i;double inverse;
    if(!first || !out || (interpolate && !second) || !isfinite(fraction) || fraction<0 || fraction>1)return RF_RANGE;
    inverse=1.0-(double)fraction;
    for(i=0;i<6;++i) {
        if(!isfinite(first[i]) || (interpolate && !isfinite(second[i])))return RF_RANGE;
        v[i]=interpolate?(float)((double)first[i]*inverse+(double)second[i]*fraction):first[i];
        if(!isfinite(v[i]))return RF_RANGE;
    }
    memcpy(out,v,sizeof(v));return RF_OK;
}
static int vfx_owned_uv(const rf_vfx_mesh *mesh,uint32_t frame,uint32_t face,float out[6])
{
    const rf_vfx_frame_view *v=mesh->frames+frame;uint64_t at;uint32_t i,j,word;
    if(!(v->present&8) || (uint64_t)face*24+24>v->uv_bytes)return RF_FORMAT;
    at=(uint64_t)v->uv_offset+(uint64_t)face*24;if(at+24>mesh->bytes)return RF_FORMAT;
    for(i=0;i<3;++i)for(j=0;j<2;++j){word=vfx_word(mesh->data+(size_t)at+(i*2+j)*4);memcpy(out+j*3+i,&word,4);}
    return RF_OK;
}
int rf_vfx_mesh_uv(const rf_vfx_mesh *mesh,const rf_vfx_frame_cursor *cursor,uint32_t face,float out[6])
{
    float first[6],second[6];uint32_t a,b;rf_vfx_face legacy;int status;
    if(!mesh || !cursor || !out || !mesh->data || !mesh->frames || face>=mesh->prefix.faces)return RF_RANGE;
    if(!cursor->active)return RF_NOT_FOUND;
    if(cursor->first>=mesh->prefix.timing.samples || cursor->second>=mesh->prefix.timing.samples)return RF_RANGE;
    if(mesh->version<0x3000d) {
        uint64_t at=(uint64_t)mesh->prefix.face_offset+(uint64_t)face*120;if(at>mesh->bytes)return RF_FORMAT;
        status=rf_vfx_face_read(mesh->data+(size_t)at,mesh->bytes-(uint32_t)at,mesh->version,&legacy);if(status)return status;
        return rf_vfx_uv_sample(legacy.legacy_uv,NULL,cursor->fraction,0,out);
    }
    a=(mesh->edges.flags&0x100)?cursor->first:0;b=(mesh->edges.flags&0x100)?cursor->second:0;
    status=vfx_owned_uv(mesh,a,face,first);if(status)return status;
    if(a!=b){status=vfx_owned_uv(mesh,b,face,second);if(status)return status;}
    return rf_vfx_uv_sample(first,a!=b?second:NULL,cursor->fraction,a!=b,out);
}

int rf_vfx_morph_read(const float first[8],const void *first_vertex,
    const float second[8],const void *second_vertex,float fraction,int interpolate,rf_vfx_morph_sample *out)
{
    rf_vfx_morph_sample v={0};float a[3],b[3],inverse,left,right;uint32_t i;int status;
    if(!first || !first_vertex || !out || (interpolate && (!second || !second_vertex)) || !isfinite(fraction) || fraction<0 || fraction>1)return RF_RANGE;
    status=rf_vfx_vertex_decode(first_vertex,6,first,a);if(status)return status;
    if(interpolate){status=rf_vfx_vertex_decode(second_vertex,6,second,b);if(status)return status;}
    inverse=(float)(1.0-(double)fraction);
    for(i=0;i<3;++i) {
        if(interpolate) {
            left=(float)((double)first[i]*inverse);right=(float)((double)second[i]*fraction);v.center[i]=(float)((double)left+right);
            left=(float)((double)a[i]*inverse);right=(float)((double)b[i]*fraction);v.vertex[i]=(float)((double)left+right);
        } else {v.center[i]=first[i];v.vertex[i]=a[i];}
        if(!isfinite(v.center[i]) || !isfinite(v.vertex[i]))return RF_RANGE;
    }
    for(i=0;i<2;++i) {
        if(!isfinite(first[i+6]) || (interpolate && !isfinite(second[i+6])))return RF_RANGE;
        v.extra[i]=interpolate?(float)((double)first[i+6]*inverse+(double)second[i+6]*fraction):first[i+6];
        if(!isfinite(v.extra[i]))return RF_RANGE;
    }
    *out=v;return RF_OK;
}
int rf_vfx_mesh_morph(const rf_vfx_mesh *mesh,const rf_vfx_frame_cursor *cursor,uint32_t vertex,rf_vfx_morph_sample *out)
{
    const rf_vfx_frame_view *a,*b;uint64_t first,second;float av[8],bv[8];
    if(!mesh || !cursor || !out || !mesh->data || !mesh->frames || vertex>=mesh->prefix.vertices)return RF_RANGE;
    if(!cursor->active || !(mesh->edges.flags&4))return RF_NOT_FOUND;
    if(cursor->first>=mesh->prefix.timing.samples || cursor->second>=mesh->prefix.timing.samples)return RF_RANGE;
    a=mesh->frames+cursor->first;b=mesh->frames+cursor->second;
    if(!(a->present&1) || !(b->present&1) || (uint64_t)vertex*6+6>a->vertex_bytes || (uint64_t)vertex*6+6>b->vertex_bytes)return RF_FORMAT;
    first=(uint64_t)a->vertex_offset+(uint64_t)vertex*6;second=(uint64_t)b->vertex_offset+(uint64_t)vertex*6;
    if(first+6>mesh->bytes || second+6>mesh->bytes)return RF_FORMAT;
    memcpy(av,a->vectors,24);memcpy(av+6,a->extra,8);memcpy(bv,b->vectors,24);memcpy(bv+6,b->extra,8);
    return rf_vfx_morph_read(av,mesh->data+(size_t)first,bv,mesh->data+(size_t)second,cursor->fraction,cursor->first!=cursor->second,out);
}

static float vfx_key_float(const unsigned char *p)
{uint32_t word=vfx_word(p);float value;memcpy(&value,&word,4);return value;}
static float vfx_key_product(float a,float b)
{return (float)((double)a*b);}
int rf_vfx_vector_key_sample(const void *data,uint32_t bytes,uint32_t count,int32_t time,float out[3])
{
    const unsigned char *p=data,*a,*b;float v[3]={0},t,s,q0,q1,q2,q3;uint32_t i,j,next=0;int32_t first,last,previous,current;int64_t numerator,denominator;
    if(!out || (!data && count) || count>65535 || (uint64_t)count*40>bytes)return RF_RANGE;
    if(!count){memcpy(out,v,sizeof(v));return RF_OK;}
    memcpy(&previous,p,4);
    for(i=0;i<count;++i) {
        memcpy(&current,p+i*40,4);if(current<previous)return RF_FORMAT;previous=current;
        for(j=1;j<10;++j)if(!isfinite(vfx_key_float(p+i*40+j*4)))return RF_RANGE;
    }
    memcpy(&first,p,4);memcpy(&last,p+(count-1)*40,4);
    if(time<=first || time>=last) {
        a=p+(time<=first?0:count-1)*40;memcpy(v,a+4,12);memcpy(out,v,sizeof(v));return RF_OK;
    }
    do {memcpy(&current,p+next*40,4);if(time<current)break;++next;}while(next<count);
    if(!next || next>=count)return RF_FORMAT;a=p+(next-1)*40;b=p+next*40;memcpy(&first,a,4);memcpy(&last,b,4);
    numerator=(int64_t)time-first;denominator=(int64_t)last-first;
    if(numerator<0 || numerator>INT32_MAX || denominator<=0 || denominator>INT32_MAX)return RF_RANGE;
    t=(float)((double)numerator/(double)denominator);s=(float)(1.0-(double)t);
    for(i=0;i<3;++i) {
        q0=vfx_key_product(vfx_key_product(vfx_key_product(vfx_key_float(a+4+i*4),s),s),s);
        q1=vfx_key_product(vfx_key_product(vfx_key_product(vfx_key_product(vfx_key_float(a+28+i*4),3),t),s),s);
        q2=vfx_key_product(vfx_key_product(vfx_key_product(vfx_key_product(vfx_key_float(b+16+i*4),3),t),t),s);
        q3=vfx_key_product(vfx_key_product(vfx_key_product(vfx_key_float(b+4+i*4),t),t),t);
        v[i]=(float)((double)q0+q1);v[i]=(float)((double)v[i]+q2);v[i]=(float)((double)v[i]+q3);if(!isfinite(v[i]))return RF_RANGE;
    }
    memcpy(out,v,sizeof(v));return RF_OK;
}
int rf_vfx_mesh_vector_key(const rf_vfx_mesh *mesh,uint32_t track,int32_t time,float out[3])
{
    uint32_t at;
    if(!mesh || !mesh->data || !out || (track!=0 && track!=2))return RF_RANGE;
    if(!(mesh->edges.mesh_flags&2))return RF_NOT_FOUND;
    at=mesh->keys.offsets[track];if(at>mesh->bytes)return RF_FORMAT;
    return rf_vfx_vector_key_sample(mesh->data+at,mesh->bytes-at,mesh->keys.counts[track],time,out);
}

int rf_vfx_rotation_key_sample(const void *data,uint32_t bytes,uint32_t count,int32_t time,float out[4])
{
    const unsigned char *p=data;rf_vfx_key a,b,key;float result[4]={0,0,0,1},t;uint32_t i,upper;int32_t previous,current,first,last;int64_t delta,numerator;int16_t qa[4],qb[4],packed[4];int status;
    if(!out || (!data && count) || count>65535 || (uint64_t)count*40>bytes)return RF_RANGE;
    if(!count){memcpy(out,result,sizeof(result));return RF_OK;}
    memcpy(&previous,p,4);
    for(i=0;i<count;++i) {
        memcpy(&current,p+i*40,4);if(current<previous)return RF_FORMAT;previous=current;
        status=rf_vfx_key_read(p+i*40,bytes-i*40,1,&key);if(status)return status;
        if(((const unsigned char *)key.words)[15]>127 || ((const unsigned char *)key.words)[16]>127)return RF_FORMAT;
    }
    if(count==1){status=rf_vfx_key_read(p,bytes,1,&a);if(status)return status;return rf_motion_decode_rotation((const unsigned char *)a.words+4,8,out);}
    memcpy(&first,p,4);memcpy(&last,p+(count-1)*40,4);
    if(time<=first){upper=1;t=0;}
    else if(time>=last){upper=count-1;t=1;}
    else {
        upper=1;do {memcpy(&current,p+upper*40,4);if(time<current)break;++upper;}while(upper<count);
        if(upper>=count)return RF_FORMAT;memcpy(&first,p+(upper-1)*40,4);memcpy(&last,p+upper*40,4);
        delta=(int64_t)last-first;numerator=(int64_t)time-first;if(delta<=0 || delta>INT32_MAX || numerator<0 || numerator>INT32_MAX)return RF_RANGE;
        t=(float)((double)numerator/(double)delta);
    }
    status=rf_vfx_key_read(p+(upper-1)*40,bytes-(upper-1)*40,1,&a);if(status)return status;
    status=rf_vfx_key_read(p+upper*40,bytes-upper*40,1,&b);if(status)return status;
    status=rf_motion_rotation_ease(t,(int8_t)((const unsigned char *)a.words)[16],(int8_t)((const unsigned char *)b.words)[15],&t);if(status)return status;
    memcpy(qa,(const unsigned char *)a.words+4,8);memcpy(qb,(const unsigned char *)b.words+4,8);
    status=rf_motion_interpolate_rotation(qa,qb,t,packed);if(status)return status;
    return rf_motion_decode_rotation(packed,sizeof(packed),out);
}
int rf_vfx_mesh_rotation_key(const rf_vfx_mesh *mesh,int32_t time,float out[4])
{
    uint32_t at;
    if(!mesh || !mesh->data || !out)return RF_RANGE;if(!(mesh->edges.mesh_flags&2))return RF_NOT_FOUND;
    at=mesh->keys.offsets[1];if(at>mesh->bytes)return RF_FORMAT;
    return rf_vfx_rotation_key_sample(mesh->data+at,mesh->bytes-at,mesh->keys.counts[1],time,out);
}

int rf_vfx_transform_point(const float transform[10],const float point[3],float out[3])
{
    float matrix[12],scaled[3],value[3],rotated;unsigned i;int status;
    if(!transform || !point || !out)return RF_RANGE;
    for(i=0;i<10;++i)if(!isfinite(transform[i]))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(point[i]))return RF_FORMAT;
    status=rf_model_attachment_transform(transform+3,transform,matrix);if(status)return status;
    for(i=0;i<3;++i) {
        scaled[i]=(float)((double)point[i]*transform[i+7]);
        if(!isfinite(scaled[i]))return RF_RANGE;
    }
    for(i=0;i<3;++i) {
        /*4facb0 accumulates Z, Y, X; 40a350 adds translation after the spill. */
        rotated=(float)(((double)scaled[2]*matrix[i+6]+(double)scaled[1]*matrix[i+3])+(double)scaled[0]*matrix[i]);
        value[i]=(float)((double)rotated+transform[i]);
        if(!isfinite(value[i]))return RF_RANGE;
    }
    memcpy(out,value,sizeof(value));return RF_OK;
}

static float vfx_transform_blend(float a,float b,float t)
{
    float inverse=(float)(1.0-(double)t),left=(float)((double)a*inverse),right=(float)((double)b*t);
    return (float)((double)left+right);
}
int rf_vfx_transform_sample(const float frame[8],const void *vertex,const float first[10],const float second[10],float fraction,int interpolate,uint32_t flags,rf_vfx_morph_sample *out)
{
    rf_vfx_morph_sample value={0};float point[3],a[3],b[3],scale;unsigned i;int status;
    if(!frame || !vertex || !first || !out || (interpolate && !second) || !isfinite(fraction) || fraction<0 || fraction>1)return RF_RANGE;
    for(i=0;i<8;++i)if(!isfinite(frame[i]))return RF_FORMAT;
    status=rf_vfx_vertex_decode(vertex,6,frame,point);if(status)return status;
    status=rf_vfx_transform_point(first,frame,a);if(status)return status;
    if(interpolate){status=rf_vfx_transform_point(second,frame,b);if(status)return status;}
    for(i=0;i<3;++i)value.center[i]=interpolate?vfx_transform_blend(a[i],b[i],fraction):a[i];
    status=rf_vfx_transform_point(first,point,a);if(status)return status;
    if(interpolate){status=rf_vfx_transform_point(second,point,b);if(status)return status;}
    for(i=0;i<3;++i)value.vertex[i]=interpolate?vfx_transform_blend(a[i],b[i],fraction):a[i];
    if((flags&0x801) && frame[6]>=0)for(i=0;i<2;++i) {
        scale=interpolate?vfx_transform_blend(first[7+i*2],second[7+i*2],fraction):first[7+i*2];
        value.extra[i]=(float)fabs((double)scale*frame[6+i]);
    }
    for(i=0;i<3;++i)if(!isfinite(value.center[i]) || !isfinite(value.vertex[i]))return RF_RANGE;
    if(!isfinite(value.extra[0]) || !isfinite(value.extra[1]))return RF_RANGE;
    *out=value;return RF_OK;
}
int rf_vfx_mesh_transform(const rf_vfx_mesh *mesh,const rf_vfx_frame_cursor *cursor,uint32_t vertex,rf_vfx_morph_sample *out)
{
    const rf_vfx_frame_view *frame,*a,*b;float vectors[8];uint64_t at;
    if(!mesh || !cursor || !out || !mesh->data || !mesh->frames || vertex>=mesh->prefix.vertices)return RF_RANGE;
    if(!cursor->active || (mesh->edges.flags&4) || (mesh->edges.mesh_flags&2))return RF_NOT_FOUND;
    if(cursor->first>=mesh->prefix.timing.samples || cursor->second>=mesh->prefix.timing.samples)return RF_RANGE;
    frame=mesh->frames;a=frame+cursor->first;b=frame+cursor->second;
    if(!(frame->present&1) || !(a->present&16) || !(b->present&16) || (uint64_t)vertex*6+6>frame->vertex_bytes)return RF_FORMAT;
    at=(uint64_t)frame->vertex_offset+(uint64_t)vertex*6;if(at+6>mesh->bytes)return RF_FORMAT;
    memcpy(vectors,frame->vectors,24);memcpy(vectors+6,frame->extra,8);
    return rf_vfx_transform_sample(vectors,mesh->data+(size_t)at,a->transform,b->transform,cursor->fraction,cursor->first!=cursor->second,mesh->edges.flags,out);
}

int rf_vfx_keyed_sample(const float frame[8],const void *vertex,const float base[10],const float key[10],uint32_t flags,rf_vfx_morph_sample *out)
{
    rf_vfx_morph_sample value={0};float point[3],local[3];unsigned i;int status;
    if(!frame || !vertex || !base || !key || !out)return RF_RANGE;
    for(i=0;i<8;++i)if(!isfinite(frame[i]))return RF_FORMAT;
    status=rf_vfx_vertex_decode(vertex,6,frame,point);if(status)return status;
    status=rf_vfx_transform_point(base,frame,local);if(status)return status;
    status=rf_vfx_transform_point(key,local,value.center);if(status)return status;
    status=rf_vfx_transform_point(base,point,local);if(status)return status;
    status=rf_vfx_transform_point(key,local,value.vertex);if(status)return status;
    /*53fce9/53fcfb keep both products before the final float store. */
    if((flags&0x801) && frame[6]>=0) {
        value.extra[0]=(float)fabs(((double)key[7]*base[7])*frame[6]);
        value.extra[1]=(float)fabs(((double)key[9]*frame[7])*base[9]);
    }
    if(!isfinite(value.extra[0]) || !isfinite(value.extra[1]))return RF_RANGE;
    *out=value;return RF_OK;
}

int rf_vfx_key_time(float effect_frame,float start_seconds,int32_t *out)
{
    float seconds;double now,start;int64_t difference;
    if(!out || !isfinite(effect_frame) || !isfinite(start_seconds))return RF_RANGE;
    seconds=(float)((double)effect_frame*0.06666667014360428f);
    now=trunc(((double)seconds*30.0)*160.0);start=trunc(((double)start_seconds*30.0)*160.0);
    if(!isfinite(now) || now<INT32_MIN || now>INT32_MAX || start<INT32_MIN || start>INT32_MAX)return RF_RANGE;
    difference=(int64_t)(int32_t)now-(int64_t)(int32_t)start;
    if(difference<INT32_MIN || difference>INT32_MAX)return RF_RANGE;
    *out=(int32_t)difference;return RF_OK;
}

static int vfx_mesh_key_pose(const rf_vfx_mesh *mesh,int32_t time,float key[10])
{
    const rf_vfx_frame_view *frame=mesh->frames;int status;
    if(mesh->keys.counts[0]){status=rf_vfx_mesh_vector_key(mesh,0,time,key);if(status)return status;}
    else {if(!(frame->present&16))return RF_FORMAT;memcpy(key,frame->transform,12);}
    if(mesh->keys.counts[1]){status=rf_vfx_mesh_rotation_key(mesh,time,key+3);if(status)return status;}
    else {if(!(frame->present&16))return RF_FORMAT;memcpy(key+3,frame->transform+3,16);}
    if(mesh->keys.counts[2]){status=rf_vfx_mesh_vector_key(mesh,2,time,key+7);if(status)return status;}
    else {if(!(frame->present&16))return RF_FORMAT;memcpy(key+7,frame->transform+7,12);}
    return RF_OK;
}
int rf_vfx_mesh_keyed(const rf_vfx_mesh *mesh,int32_t time,uint32_t vertex,rf_vfx_morph_sample *out)
{
    const rf_vfx_frame_view *frame;float key[10]={0,0,0,0,0,0,1,1,1,1},vectors[8];uint64_t at;int status;
    if(!mesh || !mesh->data || !mesh->frames || !out || !mesh->prefix.timing.samples || vertex>=mesh->prefix.vertices)return RF_RANGE;
    if((mesh->edges.flags&4) || !(mesh->edges.mesh_flags&2))return RF_NOT_FOUND;
    frame=mesh->frames;
    if(!(frame->present&1) || (uint64_t)vertex*6+6>frame->vertex_bytes)return RF_FORMAT;
    at=(uint64_t)frame->vertex_offset+(uint64_t)vertex*6;if(at+6>mesh->bytes)return RF_FORMAT;
    status=vfx_mesh_key_pose(mesh,time,key);if(status)return status;
    memcpy(vectors,frame->vectors,24);memcpy(vectors+6,frame->extra,8);
    return rf_vfx_keyed_sample(vectors,mesh->data+(size_t)at,mesh->keys.base,key,mesh->edges.flags,out);
}

int rf_vfx_mesh_sample(const rf_vfx_mesh *mesh,float effect_frame,uint32_t vertex,rf_vfx_morph_sample *out)
{
    rf_vfx_mesh_timing timing;rf_vfx_frame_cursor cursor;int status;int32_t time;
    if(!mesh || !out)return RF_RANGE;
    timing=mesh->prefix.timing;timing.flags=mesh->edges.mesh_flags;
    status=rf_vfx_frame_select(&timing,mesh->edges.flags,effect_frame,&cursor);if(status)return status;
    if(!cursor.active)return RF_NOT_FOUND;
    if(mesh->edges.flags&4)return rf_vfx_mesh_morph(mesh,&cursor,vertex,out);
    if(mesh->edges.mesh_flags&2) {
        status=rf_vfx_key_time(effect_frame,timing.start,&time);if(status)return status;
        return rf_vfx_mesh_keyed(mesh,time,vertex,out);
    }
    return rf_vfx_mesh_transform(mesh,&cursor,vertex,out);
}

int rf_vfx_parent_sample(const rf_vfx_morph_sample *sample,const float parent[12],rf_vfx_morph_sample *out)
{
    rf_vfx_morph_sample value;const float *point;float *target,rotated;unsigned i,j;
    if(!sample || !parent || !out)return RF_RANGE;
    for(i=0;i<12;++i)if(!isfinite(parent[i]))return RF_FORMAT;
    if(!isfinite(sample->extra[0]) || !isfinite(sample->extra[1]))return RF_FORMAT;
    value=*sample;
    for(j=0;j<2;++j) {
        point=j?sample->vertex:sample->center;target=j?value.vertex:value.center;
        for(i=0;i<3;++i)if(!isfinite(point[i]))return RF_FORMAT;
        for(i=0;i<3;++i) {
            rotated=(float)(((double)point[2]*parent[i+6]+(double)point[1]*parent[i+3])+(double)point[0]*parent[i]);
            target[i]=(float)((double)rotated+parent[i+9]);
            if(!isfinite(target[i]))return RF_RANGE;
        }
    }
    *out=value;return RF_OK;
}

int rf_vfx_bone_parent_sample(const rf_vfx_morph_sample *sample,const float (*pose)[12],uint32_t count,int32_t index,rf_vfx_morph_sample *out)
{
    rf_model_bone_query query;rf_vfx_morph_sample value;const float *point,*m;float *target;unsigned i,j;int status;
    if(!sample || !out)return RF_RANGE;
    status=rf_model_query_bone(pose,count,index,&query);if(status)return status;
    if(!isfinite(sample->extra[0]) || !isfinite(sample->extra[1]))return RF_FORMAT;
    value=*sample;m=query.basis;
    for(j=0;j<2;++j) {
        point=j?sample->vertex:sample->center;target=j?value.vertex:value.center;
        for(i=0;i<3;++i)if(!isfinite(point[i]))return RF_FORMAT;
        /*4ff020: X accumulates Y,Z,X; Y/Z accumulate Y,X,Z. */
        target[0]=(float)((((double)m[3]*point[1]+(double)m[6]*point[2])+(double)m[0]*point[0])+query.position[0]);
        for(i=1;i<3;++i)target[i]=(float)((((double)m[3+i]*point[1]+(double)m[i]*point[0])+(double)m[6+i]*point[2])+query.position[i]);
        for(i=0;i<3;++i)if(!isfinite(target[i]))return RF_RANGE;
    }
    *out=value;return RF_OK;
}

int rf_vfx_face_normal(const float vertices[9],float out[3])
{
    float a[3],b[3],normal[3];double length;unsigned i;
    if(!vertices || !out)return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(vertices[i]))return RF_FORMAT;
    for(i=0;i<3;++i){a[i]=(float)((double)vertices[i+3]-vertices[i]);b[i]=(float)((double)vertices[i+6]-vertices[i+3]);}
    for(i=0;i<3;++i)normal[i]=(float)((double)a[(i+1)%3]*b[(i+2)%3]-(double)a[(i+2)%3]*b[(i+1)%3]);
    length=sqrt(((double)normal[0]*normal[0]+(double)normal[1]*normal[1])+(double)normal[2]*normal[2]);
    if(!(length>0) || !isfinite(length))return RF_RANGE;
    length=1.0/length;for(i=0;i<3;++i)normal[i]=(float)((double)normal[i]*length);
    memcpy(out,normal,sizeof(normal));return RF_OK;
}

int rf_vfx_face_facing(const float normal[3],const float point[3],const float origin[3],const float forward[3],uint32_t perspective,uint32_t *out)
{
    float delta[3];double dot;unsigned i;
    if(!normal || !point || !origin || !forward || !out || perspective>1)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(normal[i]) || !isfinite(point[i]) || !isfinite(origin[i]) || !isfinite(forward[i]))return RF_FORMAT;
        delta[i]=perspective?(float)((double)origin[i]-point[i]):forward[i];
        if(!isfinite(delta[i]))return RF_RANGE;
    }
    dot=((double)delta[0]*normal[0]+(double)delta[1]*normal[1])+(double)delta[2]*normal[2];
    *out=perspective?(dot>0):(dot<=0);return RF_OK;
}

static int vfx_sort_after(const rf_vfx_sort_record *a,const rf_vfx_sort_record *b,uint32_t extended)
{
    unsigned i;
    if(extended)for(i=0;i<3;++i)if(a->key[i]<b->key[i] && (double)b->key[i]-a->key[i]>(double)0.003000000026077032f)return 1;
    return a->key[0]<b->key[0];
}
int rf_vfx_sort(rf_vfx_sort_record *records,uint32_t count,uint32_t extended)
{
    uint32_t gap,i,j,k;rf_vfx_sort_record temporary;
    if((!records && count) || extended>1 || count>INT32_MAX || count>SIZE_MAX/sizeof(*records))return RF_RANGE;
    for(i=0;i<count;++i)for(k=0;k<3;++k)if(!isfinite(records[i].key[k]))return RF_FORMAT;
    for(gap=count/2;gap;gap/=2)for(i=gap;i<count;++i) {
        j=i;
        while(j>=gap && vfx_sort_after(records+j-gap,records+j,extended)) {
            temporary=records[j-gap];records[j-gap]=records[j];records[j]=temporary;j-=gap;
        }
    }
    return RF_OK;
}

int rf_vfx_face_append(const float depth[3],int32_t material,uint32_t item,rf_vfx_sort_record *records,uint32_t capacity,uint32_t *count)
{
    double bias,a,b,c,ab,bc,selected;float key;unsigned i;
    if(!depth || !records || !count || *count>=capacity || capacity>SIZE_MAX/sizeof(*records))return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(depth[i]))return RF_FORMAT;
    bias=(double)material*61.0;
    /* First two biased depths spill to binary32; third remains extended. */
    a=(float)(bias+depth[0]);b=(float)(bias+depth[1]);c=bias+depth[2];
    ab=a>b?a:b;bc=b>c?b:c;
    selected=ab>bc?(a>b?a:b):(b>c?b:c);key=(float)selected;
    if(!isfinite(key))return RF_RANGE;
    records[*count].key[0]=key;records[*count].item=item;++*count;return RF_OK;
}

int rf_vfx_face_prepare(const rf_vfx_face_input *input,rf_vfx_face_output *out)
{
    rf_vfx_face_output value={0};rf_vfx_sort_record record={{0,0,0},0};uint32_t count=0;unsigned i;int status;
    if(!input || !out || input->perspective>1)return RF_RANGE;
    for(i=0;i<3;++i)if(input->clip[i]>255)return RF_RANGE;
    if(input->clip[0]&input->clip[1]&input->clip[2]){*out=value;return RF_OK;}
    status=rf_vfx_face_normal(input->vertices,value.normal);if(status)return status;
    status=rf_vfx_face_facing(value.normal,input->vertices,input->origin,input->forward,input->perspective,&value.visible);if(status)return status;
    if(value.visible) {
        status=rf_vfx_face_append(input->depth,input->material,0,&record,1,&count);if(status)return status;
        value.depth=record.key[0];
    }
    *out=value;return RF_OK;
}

int rf_vfx_world_face(const rf_visibility_camera *camera,const float vertices[9],int32_t material,rf_vfx_face_output *out)
{
    rf_vfx_face_input input={0};const rf_visibility_projection *view;float delta[3],position[3];unsigned i,j;
    if(!camera || !vertices || !out)return RF_RANGE;
    view=&camera->projection;
    if(!isfinite(view->flat_depth) || !isfinite(view->clip.far_distance))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(view->matrix[i]) || !isfinite(vertices[i]))return RF_FORMAT;
    for(i=0;i<3;++i)if(!isfinite(view->origin[i]))return RF_FORMAT;
    memcpy(input.vertices,vertices,36);memcpy(input.origin,view->origin,12);memcpy(input.forward,view->matrix+6,12);
    input.perspective=(view->perspective&255u)!=0;input.material=material;
    for(i=0;i<3;++i) {
        for(j=0;j<3;++j)delta[j]=(float)((double)vertices[i*3+j]-view->origin[j]);
        for(j=0;j<3;++j) {
            position[j]=(float)(((double)view->matrix[j*3]*delta[0]+(double)view->matrix[j*3+1]*delta[1])+(double)view->matrix[j*3+2]*delta[2]);
            if(!isfinite(position[j]))return RF_RANGE;
        }
        if(!input.perspective)position[2]=view->flat_depth;
        input.depth[i]=position[2];input.clip[i]=particle_clip_code(&view->clip,position);
    }
    return rf_vfx_face_prepare(&input,out);
}

int rf_vfx_instance_open(const rf_vfx_mesh *mesh,uint32_t budget,rf_vfx_instance **out)
{
    uint64_t bytes;rf_vfx_instance *value;
    if(!mesh || !mesh->data || !mesh->frames || !out || *out || !mesh->prefix.vertices || !mesh->prefix.timing.samples)return RF_RANGE;
    bytes=sizeof(*value)+(uint64_t)mesh->prefix.vertices*12+(uint64_t)mesh->prefix.faces*24;
    if(bytes>budget || bytes>UINT32_MAX || bytes>SIZE_MAX)return RF_RANGE;
    value=malloc((size_t)bytes);if(!value)return RF_IO;
    memset(value,0,(size_t)bytes);value->mesh=mesh;value->vertices=(float *)(value+1);
    value->uv=value->vertices+(size_t)mesh->prefix.vertices*3;value->allocated_bytes=(uint32_t)bytes;
    *out=value;return RF_OK;
}
void rf_vfx_instance_close(rf_vfx_instance **instance)
{
    if(instance && *instance){free(*instance);*instance=NULL;}
}
int rf_vfx_instance_update(rf_vfx_instance *instance,float effect_frame)
{
    const rf_vfx_mesh *mesh;rf_vfx_mesh_timing timing;rf_vfx_frame_cursor cursor;rf_vfx_morph_sample sample;
    uint32_t i,keyed;int status;int32_t time;float key[10],vectors[8];const rf_vfx_frame_view *frame;
    if(!instance)return RF_RANGE;
    instance->active=0;mesh=instance->mesh;
    if(!mesh || !mesh->data || !mesh->frames || !instance->vertices || !instance->uv)return RF_RANGE;
    timing=mesh->prefix.timing;timing.flags=mesh->edges.mesh_flags;
    status=rf_vfx_frame_select(&timing,mesh->edges.flags,effect_frame,&cursor);if(status)return status;
    if(!cursor.active)return RF_OK;
    keyed=!(mesh->edges.flags&4) && (mesh->edges.mesh_flags&2);frame=mesh->frames;
    if(keyed) {
        uint64_t bytes=(uint64_t)mesh->prefix.vertices*6;
        if(!(frame->present&1) || bytes>frame->vertex_bytes || (uint64_t)frame->vertex_offset+bytes>mesh->bytes)return RF_FORMAT;
        status=rf_vfx_key_time(effect_frame,timing.start,&time);if(status)return status;
        status=vfx_mesh_key_pose(mesh,time,key);if(status)return status;
        memcpy(vectors,frame->vectors,24);memcpy(vectors+6,frame->extra,8);
    }
    for(i=0;i<mesh->prefix.vertices;++i) {
        if(keyed)status=rf_vfx_keyed_sample(vectors,mesh->data+frame->vertex_offset+(size_t)i*6,mesh->keys.base,key,mesh->edges.flags,&sample);
        else if(mesh->edges.flags&4)status=rf_vfx_mesh_morph(mesh,&cursor,i,&sample);
        else status=rf_vfx_mesh_transform(mesh,&cursor,i,&sample);
        if(status)return status;
        memcpy(instance->vertices+(size_t)i*3,sample.vertex,12);
        if(!i){memcpy(instance->center,sample.center,12);memcpy(instance->extra,sample.extra,8);}
    }
    for(i=0;i<mesh->prefix.faces;++i) {
        status=rf_vfx_mesh_uv(mesh,&cursor,i,instance->uv+(size_t)i*6);if(status)return status;
    }
    instance->active=1;return RF_OK;
}

int rf_vfx_instance_faces(const rf_visibility_camera *camera,const rf_vfx_instance *instance,rf_vfx_face_output *faces,rf_vfx_sort_record *order,uint32_t capacity,uint32_t *count)
{
    const rf_vfx_mesh *mesh;rf_vfx_face face;float vertices[9];uint32_t i,j,n=0,stride;uint64_t at;int status;int32_t material;
    if(!count)return RF_RANGE;*count=0;
    if(!camera || !instance || !instance->mesh)return RF_RANGE;
    if(!instance->active)return RF_OK;
    mesh=instance->mesh;
    if(!mesh->data || !instance->vertices || capacity<mesh->prefix.faces || ((!faces || !order) && mesh->prefix.faces))return RF_RANGE;
    stride=mesh->version<0x3000d?120:96;
    for(i=0;i<mesh->prefix.faces;++i) {
        at=(uint64_t)mesh->prefix.face_offset+(uint64_t)i*stride;
        if(at+stride>mesh->bytes)return RF_FORMAT;
        status=rf_vfx_face_read(mesh->data+(size_t)at,stride,mesh->version,&face);if(status)return status;
        for(j=0;j<3;++j) {
            if(face.indices[j]>=mesh->prefix.vertices)return RF_FORMAT;
            memcpy(vertices+j*3,instance->vertices+(size_t)face.indices[j]*3,12);
        }
        memcpy(&material,&face.material,4);
        status=rf_vfx_world_face(camera,vertices,material,faces+i);if(status)return status;
        if(faces[i].visible){order[n].key[0]=faces[i].depth;order[n].item=i;++n;}
    }
    status=rf_vfx_sort(order,n,1);if(status)return status;
    *count=n;return RF_OK;
}

static float vfx_material_scalar(const unsigned char *samples,uint32_t index,uint32_t stride,int clamp)
{
    float value;memcpy(&value,samples+(size_t)index*stride,4);
    if(clamp){if(value<=0)value=0;if(value>1)value=1;}return value;
}
static int vfx_material_track_bytes(const unsigned char *samples,uint32_t count,int32_t rate,
    float effect_frame,uint32_t stride,int clamp,float *out)
{
    double position,fraction,value;float stored,result;uint32_t index,i;
    if(!samples || !count || count>INT32_MAX || stride<4 || count>SIZE_MAX/stride || rate<0 || !out || !isfinite(effect_frame) || effect_frame<0)return RF_RANGE;
    for(i=0;i<count;++i)if(!isfinite(vfx_material_scalar(samples,i,stride,0)))return RF_FORMAT;
    position=((double)rate*effect_frame)*(double)0.06666667014360428f;stored=(float)position;
    if(!isfinite(stored) || position>=2147483646.0)return RF_RANGE;
    index=(uint32_t)floor(position);fraction=(double)stored-index;
    value=index+1<count?((1.0-fraction)*vfx_material_scalar(samples,index,stride,clamp)+fraction*vfx_material_scalar(samples,index+1,stride,clamp)):vfx_material_scalar(samples,count-1,stride,clamp);
    result=(float)value;if(!isfinite(result))return RF_RANGE;
    if(result>1)result=1;if(!(result>0))result=0;
    *out=result;return RF_OK;
}
int rf_vfx_material_track(const float *samples,uint32_t count,int32_t rate,float effect_frame,float *out)
{return vfx_material_track_bytes((const unsigned char *)samples,count,rate,effect_frame,4,0,out);}

int rf_vfx_material_evaluate(const void *data,uint32_t bytes,const rf_vfx_material_view *view,
    uint32_t track,float effect_frame,float *out)
{
    static const uint32_t counts[3]={31,46,48},offsets[3]={32,47,49};uint32_t count,at;int32_t rate;
    if(!data || !view || !out || track>2)return RF_RANGE;
    count=view->words[counts[track]];at=view->words[offsets[track]];
    if(!count)return RF_NOT_FOUND;
    if(at>bytes || (uint64_t)count*4>bytes-at)return RF_RANGE;
    memcpy(&rate,view->words+30,4);
    return vfx_material_track_bytes((const unsigned char *)data+at,count,rate,effect_frame,4,track==0,out);
}

int rf_vfx_mesh_material_evaluate(const rf_vfx_mesh *mesh,uint32_t material,uint32_t track,
    float effect_frame,float *out)
{
    rf_vfx_embedded_material_view view;uint32_t at,i,count;int status;int32_t rate;float color;
    if(!mesh || !mesh->data || !out || track>2)return RF_RANGE;
    if(mesh->version>=0x40000 || material>=mesh->materials)return RF_NOT_FOUND;
    at=mesh->material_offset;
    for(i=0;i<=material;++i) {
        if(at>mesh->bytes)return RF_RANGE;
        status=rf_vfx_embedded_material_read(mesh->data+at,mesh->bytes-at,mesh->version,
            mesh->prefix.timing.flags,mesh->prefix.timing.samples,&view);
        if(status)return status;
        if(i!=material)at+=view.material.bytes;
    }
    if(!track)return rf_vfx_material_evaluate(mesh->data+at,mesh->bytes-at,&view.material,0,effect_frame,out);
    memcpy(&rate,view.material.words+30,4);
    if(track==1){memcpy(&color,&view.color_word,4);return rf_vfx_material_track(&color,1,rate,effect_frame,out);}
    count=mesh->prefix.timing.samples;
    if(!count)return RF_NOT_FOUND;
    if(!mesh->frames || count>SIZE_MAX/sizeof(*mesh->frames))return RF_RANGE;
    for(i=0;i<count;++i)if(!(mesh->frames[i].present&32))return RF_FORMAT;
    return vfx_material_track_bytes((const unsigned char *)&mesh->frames[0].opacity,count,rate,
        effect_frame,sizeof(*mesh->frames),0,out);
}

void rf_vfx_material_bank_close(rf_vfx_material_bank **out)
{if(out){free(*out);*out=NULL;}}
int rf_vfx_material_bank_open(const rf_vfx_directory *directory,uint32_t budget,rf_vfx_material_bank **out)
{
    rf_vfx_material_bank *bank;uint64_t bytes=0,total;uint32_t count=0,i,j,at=0,index=0;int status;
    if(!directory || !out || *out || (directory->count && !directory->chunks))return RF_RANGE;
    for(i=0;i<directory->count;++i)if(directory->chunks[i].type==0x4c54414du){++count;bytes+=directory->chunks[i].bytes;}
    total=sizeof(*bank)+(uint64_t)count*sizeof(*bank->views)+bytes;
    if(total>budget || total>SIZE_MAX)return RF_RANGE;
    bank=malloc((size_t)total);if(!bank)return RF_IO;memset(bank,0,(size_t)total);
    bank->count=count;bank->bytes=(uint32_t)bytes;bank->allocated_bytes=(uint32_t)total;
    bank->views=(rf_vfx_material_view *)(bank+1);bank->data=(unsigned char *)(bank->views+count);
    for(i=0;i<directory->count;++i)if(directory->chunks[i].type==0x4c54414du) {
        rf_vfx_material_view *view=bank->views+index++;uint32_t size=directory->chunks[i].bytes;
        status=rf_vfx_chunk_read(directory,i,0,bank->data+at,size);if(status)goto failed;
        status=rf_vfx_material_read(bank->data+at,size,directory->header.version,view);if(status)goto failed;
        if(view->bytes!=size){status=RF_FORMAT;goto failed;}
        for(j=0;j<3;++j){static const uint32_t c[3]={31,46,48},o[3]={32,47,49};if(view->words[c[j]])view->words[o[j]]+=at;}
        at+=size;
    }
    *out=bank;return RF_OK;
failed:free(bank);return status;
}
int rf_vfx_mesh_material_sample(const rf_vfx_mesh *mesh,const rf_vfx_material_bank *bank,
    uint32_t material,uint32_t track,float effect_frame,float *out)
{
    uint64_t at;uint32_t id;
    if(!mesh || !mesh->data || !out || track>2)return RF_RANGE;
    if(mesh->version<0x40000)return rf_vfx_mesh_material_evaluate(mesh,material,track,effect_frame,out);
    if(material>=mesh->materials)return RF_NOT_FOUND;
    if(!bank || !bank->views || !bank->data)return RF_RANGE;
    at=(uint64_t)mesh->material_offset+(uint64_t)material*4;
    if(at>mesh->bytes || mesh->bytes-at<4)return RF_RANGE;
    id=vfx_word(mesh->data+(size_t)at);if(id>=bank->count)return RF_FORMAT;
    return rf_vfx_material_evaluate(bank->data,bank->bytes,bank->views+id,track,effect_frame,out);
}

int rf_vfx_texture_frame(uint32_t count,float duration,int32_t start,float speed,
    uint32_t mode,float time,uint32_t normalized,uint32_t *out)
{
    double position,integral;int32_t index;uint32_t bits;
    if(!out || !count || count>INT32_MAX || normalized>1)return RF_RANGE;
    if(count==1){*out=0;return RF_OK;}
    if(!isfinite(time))return RF_RANGE;
    if(normalized)position=(double)count*time;
    else {
        if(!isfinite(duration) || duration<=0 || !isfinite(speed))return RF_RANGE;
        position=(((double)count/duration)*speed)*((double)time-start)*(double)0.06666667014360428f;
    }
    integral=floor(position);if(!isfinite(integral) || integral<INT32_MIN || integral>INT32_MAX)return RF_RANGE;
    index=(int32_t)integral;
    if(normalized){bits=(uint32_t)index-(uint32_t)start;memcpy(&index,&bits,4);}
    else if(index<0)index=0;
    if(index>=(int32_t)count) {
        if(mode==0 || mode==2)index%=(int32_t)count;
        else index=(int32_t)count-1;
    }
    if(normalized && index<0){index+=(int32_t)count;if(index<0)index=0;}
    *out=(uint32_t)index;return RF_OK;
}

int rf_vfx_texture_duration(uint32_t count,uint32_t rate,float *out)
{
    float stored,result;
    if(!out || !count || count>255)return RF_RANGE;
    if(count==1){*out=1;return RF_OK;}
    if(!rate || rate>INT32_MAX)return RF_RANGE;
    stored=(float)((double)rate*(double)0.0010000000474974513f);
    result=(float)(((double)count/stored)*(double)0.0010000000474974513f);
    if(!isfinite(result) || result<=0)return RF_RANGE;*out=result;return RF_OK;
}

int rf_vfx_material_color(const rf_vfx_material_view *view,const unsigned char lighting[3],
    float brightness,uint32_t mesh_flags,unsigned char out[3])
{
    unsigned char color[3];uint32_t i,minimum;float scaled;
    if(!view || !lighting || !out)return RF_RANGE;
    if(mesh_flags&0x10)memset(color,255,3);
    else {
        if(!isfinite(brightness) || brightness<0 || brightness>1)return RF_RANGE;
        scaled=(float)((double)brightness*255.0);minimum=(uint32_t)((double)scaled+0.5);
        for(i=0;i<3;++i)color[i]=lighting[i]<minimum?(unsigned char)minimum:lighting[i];
    }
    if(view->words[0]==2) {
        const unsigned char *tint=(const unsigned char *)view->words+9;
        for(i=0;i<3;++i)color[i]=(unsigned char)((uint32_t)((double)((uint32_t)color[i]*tint[i])*(double)0.003921568859368563f));
    }
    memcpy(out,color,3);return RF_OK;
}

int rf_vfx_point_light(const float position[3],const float normal[3],const float light[3],
    float radius,uint32_t soften,float out[2])
{
    float direction[3],basis[3],result[2];double length,reciprocal,value;uint32_t i;
    if(!position || !normal || !light || !out || soften>1 || !isfinite(radius) || radius<0)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]) || !isfinite(normal[i]) || !isfinite(light[i]))return RF_RANGE;
        direction[i]=light[i]-position[i];if(!isfinite(direction[i]))return RF_RANGE;
    }
    length=sqrt(((double)direction[0]*direction[0]+(double)direction[1]*direction[1])+(double)direction[2]*direction[2]);
    if(length==0){direction[0]=1;direction[1]=direction[2]=0;length=1;}
    else {reciprocal=1.0/length;for(i=0;i<3;++i)direction[i]=(float)(direction[i]*reciprocal);}
    result[1]=(float)length;if(!isfinite(result[1]))return RF_RANGE;result[0]=0;
    if(length<radius) {
        for(i=0;i<3;++i) {
            basis[i]=normal[i];
            if(soften){float doubled=direction[i]*2.0f;basis[i]=doubled+normal[i];basis[i]*=0.3333333432674408f;}
        }
        value=((double)basis[0]*direction[0]+(double)basis[1]*direction[1])+(double)basis[2]*direction[2];
        result[0]=(float)value;if(!isfinite(result[0]))return RF_RANGE;
    }
    memcpy(out,result,8);return RF_OK;
}

int rf_vfx_cone_light(const float position[3],const float normal[3],const float light[3],
    const float axis[3],float radius,uint32_t soften,float out[3])
{
    float direction[3],basis[3],result[3]={0,0,0};double length,reciprocal;uint32_t i;
    if(!position || !normal || !light || !axis || !out || soften>1 || !isfinite(radius) || radius<0)return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(position[i]) || !isfinite(normal[i]) || !isfinite(light[i]) || !isfinite(axis[i]))return RF_RANGE;
        direction[i]=light[i]-position[i];if(!isfinite(direction[i]))return RF_RANGE;
    }
    length=sqrt(((double)direction[0]*direction[0]+(double)direction[1]*direction[1])+(double)direction[2]*direction[2]);
    if(length==0){direction[0]=1;direction[1]=direction[2]=0;length=1;}
    else {reciprocal=1.0/length;for(i=0;i<3;++i)direction[i]=(float)(direction[i]*reciprocal);}
    result[2]=(float)length;if(!isfinite(result[2]))return RF_RANGE;
    if(length<radius) {
        for(i=0;i<3;++i){basis[i]=normal[i];if(soften){basis[i]=direction[i]+normal[i];basis[i]*=.5f;}}
        result[0]=(float)(((double)basis[0]*direction[0]+(double)basis[1]*direction[1])+(double)basis[2]*direction[2]);
        result[1]=(float)(((double)direction[0]*axis[0]+(double)direction[1]*axis[1])+(double)direction[2]*axis[2]);
        if(!isfinite(result[0]) || !isfinite(result[1]))return RF_RANGE;
    }
    memcpy(out,result,12);return RF_OK;
}

int rf_vfx_light_add(uint32_t profile,float distance,float radius,float gain,
    const float color[3],const float accumulated[3],float out[3])
{
    double value;float result[3];uint32_t i;
    if(!color || !accumulated || !out || profile>3 || !isfinite(distance) || !isfinite(radius) ||
        !isfinite(gain) || distance<0 || radius<=0 || distance>radius)return RF_RANGE;
    value=((double)radius-distance)/radius;
    if(profile==1)value*=value;
    else if(profile==2)value=cos((1.0-value)*(double)1.5707963705062866f);
    else if(profile==3)value=sqrt(value);
    value*=gain;
    for(i=0;i<3;++i) {
        if(!isfinite(color[i]) || !isfinite(accumulated[i]))return RF_RANGE;
        result[i]=(float)(value*color[i]+accumulated[i]);if(!isfinite(result[i]))return RF_RANGE;
    }
    memcpy(out,result,12);return RF_OK;
}

/* Preserve the original unspilled blue reciprocal/product at the byte boundary. */
static uint32_t vfx_blue_byte(float blue,float peak)
{
    uint32_t result;float multiplier=255.0f;
#if defined(_MSC_VER) && defined(_M_IX86)
    unsigned short saved,nearest,truncation;
    __asm { fnstcw saved }
    nearest=(unsigned short)((saved&~0x0f00u)|0x0300u);truncation=(unsigned short)(nearest|0x0c00u);
    __asm {
        fldcw nearest
        fld1
        fdiv peak
        fmul blue
        fmul multiplier
        fldcw truncation
        fistp result
        fldcw saved
    }
#elif defined(__i386__) && (defined(__GNUC__) || defined(__clang__))
    unsigned short saved,nearest,truncation;
    __asm__ volatile("fnstcw %0":"=m"(saved));
    nearest=(unsigned short)((saved&~0x0f00u)|0x0300u);truncation=(unsigned short)(nearest|0x0c00u);
    __asm__ volatile("fldcw %1; fld1; fdivs %2; fmuls %3; fmuls %4; fldcw %5; fistpl %0; fldcw %6"
        :"=m"(result):"m"(nearest),"m"(peak),"m"(blue),"m"(multiplier),"m"(truncation),"m"(saved):"st");
#else
    result=(uint32_t)((1.0L/peak)*blue*255.0L);
#endif
    return result;
}
int rf_vfx_light_rgb(const float accumulated[3],const float ambient[3],float gain,unsigned char out[3])
{
    double value[3],peak,scale;unsigned char result[3];uint32_t i;
    if(!accumulated || !ambient || !out || !isfinite(gain))return RF_RANGE;
    for(i=0;i<3;++i) {
        if(!isfinite(accumulated[i]) || accumulated[i]<0 || !isfinite(ambient[i]) || ambient[i]<0 || ambient[i]>1)return RF_RANGE;
        value[i]=accumulated[i];
    }
    peak=value[0]>value[1]?value[0]:value[1];if(value[2]>peak)peak=value[2];
    if(peak>1) {
        scale=1.0/peak;value[0]=(float)(value[0]*scale);value[1]=(float)(value[1]*scale);value[2]*=scale;
    }
    if(gain>=0)for(i=0;i<3;++i) {
        value[i]=(float)(value[i]*gain);
        if(gain>1){if(value[i]>1)value[i]=1;}
        else if(value[i]<ambient[i])value[i]=ambient[i];
    }
    for(i=0;i<3;++i)result[i]=(unsigned char)(uint32_t)(value[i]*255.0);
    if(gain<0 && peak>1)result[2]=(unsigned char)vfx_blue_byte(accumulated[2],(float)peak);
    memcpy(out,result,3);return RF_OK;
}

int rf_vfx_point_lighting(const float position[3],const float normal[3],const float ambient[3],
    const rf_vfx_point_source *lights,uint32_t count,unsigned char out[3])
{
    float accumulated[3],geometry[2];uint32_t i,j;int status;
    if(!position || !normal || !ambient || !out || (!lights && count) || count>SIZE_MAX/sizeof(*lights))return RF_RANGE;
    for(j=0;j<3;++j) {
        if(!isfinite(position[j]) || !isfinite(normal[j]) || !isfinite(ambient[j]) || ambient[j]<0 || ambient[j]>1)return RF_RANGE;
        accumulated[j]=ambient[j];
    }
    for(i=0;i<count;++i) {
        const rf_vfx_point_source *light=lights+i;
        if(light->profile>3)return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(light->color[j]) || light->color[j]<0)return RF_RANGE;
        status=rf_vfx_point_light(position,normal,light->position,light->radius,0,geometry);if(status)return status;
        if(geometry[0]>0 && geometry[1]<light->radius) {
            status=rf_vfx_light_add(light->profile,geometry[1],light->radius,geometry[0],light->color,accumulated,accumulated);if(status)return status;
        }
    }
    return rf_vfx_light_rgb(accumulated,ambient,2,out);
}

static double vfx_falloff(uint32_t profile,float distance,float radius)
{
    double value=((double)radius-distance)/radius;
    if(profile==1)return value*value;
    if(profile==2)return cos((1.0-value)*(double)1.5707963705062866f);
    if(profile==3)return sqrt(value);return value;
}
int rf_vfx_light_accumulate(const float position[3],const float normal[3],const float initial[3],
    float directional_scale,const rf_vfx_light_source *lights,uint32_t count,const unsigned char *weights,uint32_t soften,float out[3])
{
    float rgb[3],g[3];uint32_t i,j;int status;
    if(!position || !normal || !initial || !out || (!lights && count) || count>SIZE_MAX/sizeof(*lights) ||
        !isfinite(directional_scale) || soften>1)return RF_RANGE;
    for(j=0;j<3;++j){if(!isfinite(position[j]) || !isfinite(normal[j]) || !isfinite(initial[j]))return RF_RANGE;rgb[j]=initial[j];}
    for(i=0;i<count;++i) {
        const rf_vfx_light_source *l=lights+i;double gain=0,weight;float color[3];
        if(weights && !weights[i])continue;
        weight=(double)(weights?weights[i]:255)*0.0039215688593685626983642578125;
        if(l->type<1 || l->type>4 || l->profile>3)return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(l->position[j]) || !isfinite(l->end[j]) || !isfinite(l->axis[j]) || !isfinite(l->color[j]) || l->color[j]<0)return RF_RANGE;
        for(j=0;j<3;j++)color[j]=(float)((double)weight*l->color[j]);
        if(!isfinite(l->radius) || l->radius<0)return RF_RANGE;
        if(l->type==1) {
            gain=(((double)normal[0]*l->position[0]+(double)normal[1]*l->position[1])+(double)normal[2]*l->position[2])*directional_scale;
            if(!(gain>0))continue;
        } else if(l->type==2) {
            status=rf_vfx_point_light(position,normal,l->position,l->radius,soften,g);if(status)return status;
            if(!(g[0]>0 && g[1]<l->radius))continue;
            gain=vfx_falloff(l->profile,g[1],l->radius)*g[0];
        } else if(l->type==3) {
            float distance;double cone;
            if(!isfinite(l->cone_scale) || l->cone_scale<0 || l->cone_scale>1 || !isfinite(l->inner) || !isfinite(l->outer) || l->inner>l->outer)return RF_RANGE;
            status=rf_vfx_cone_light(position,normal,l->position,l->axis,l->radius,soften,g);if(status)return status;
            if(!(g[0]>0 && g[2]<l->radius && g[1]<l->outer))continue;
            distance=(float)((1.0-l->cone_scale)*g[2]);cone=1;
            if(g[1]>=l->inner){cone=1.0-((double)g[1]-l->inner)/((double)l->outer-l->inner);if(l->squared&255u)cone*=cone;}
            gain=(cone*vfx_falloff(l->profile,distance,l->radius))*g[0];
        } else {
            float d[3],v[3],closest[3],length_stored,projection,distance;double length,along;
            for(j=0;j<3;++j){d[j]=l->end[j]-l->position[j];v[j]=position[j]-l->position[j];if(!isfinite(d[j]) || !isfinite(v[j]))return RF_RANGE;}
            length=sqrt(((double)d[0]*d[0]+(double)d[1]*d[1])+(double)d[2]*d[2]);
            if(length==0){length=1;d[0]=1;d[1]=d[2]=0;}else for(j=0;j<3;++j)d[j]=(float)(d[j]*(1.0/length));
            length_stored=(float)length;along=((double)d[0]*v[0]+(double)d[1]*v[1])+(double)d[2]*v[2];projection=(float)along;
            for(j=0;j<3;++j) {
                if(along<0)closest[j]=l->position[j];
                else if(projection>length_stored)closest[j]=l->end[j];
                else {float offset=d[j]*projection;closest[j]=l->position[j]+offset;}
                v[j]=closest[j]-position[j];
            }
            length=sqrt(((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2]);distance=(float)length;
            if(!(length<l->radius))continue;gain=vfx_falloff(l->profile,distance,l->radius);
        }
        for(j=0;j<3;++j){rgb[j]=(float)(gain*color[j]+rgb[j]);if(!isfinite(rgb[j]))return RF_RANGE;}
    }
    memcpy(out,rgb,sizeof(rgb));return RF_OK;
}

int rf_vfx_lighting(const float position[3],const float normal[3],const float ambient[3],
    float directional_scale,const rf_vfx_light_source *lights,uint32_t count,unsigned char out[3])
{
    float rgb[3];uint32_t j;int status;
    if(!ambient || !out)return RF_RANGE;
    for(j=0;j<3;j++)if(!isfinite(ambient[j]) || ambient[j]<0 || ambient[j]>1)return RF_RANGE;
    status=rf_vfx_light_accumulate(position,normal,ambient,directional_scale,lights,count,NULL,0,rgb);if(status)return status;
    return rf_vfx_light_rgb(rgb,ambient,2,out);
}

int rf_vfx_light_transform(const rf_vfx_light_source *light,const float origin[3],const float basis[9],rf_vfx_light_source *out)
{
    rf_vfx_light_source result;uint32_t i,j,slot;float delta[3];
    if(!light || !origin || !basis || !out || light->type<1 || light->type>4)return RF_RANGE;
    for(i=0;i<3;++i)if(!isfinite(origin[i]))return RF_RANGE;
    for(i=0;i<9;++i)if(!isfinite(basis[i]))return RF_RANGE;
    result=*light;
    for(slot=0;slot<3;++slot) {
        const float *source=slot==0?light->position:slot==1?light->end:light->axis;
        float *target=slot==0?result.position:slot==1?result.end:result.axis;
        if((slot==1 && light->type!=4) || (slot==2 && light->type!=3))continue;
        for(i=0;i<3;++i){if(!isfinite(source[i]))return RF_RANGE;delta[i]=source[i];if(slot!=2 && light->type!=1)delta[i]-=origin[i];if(!isfinite(delta[i]))return RF_RANGE;}
        for(j=0;j<3;++j){target[j]=(float)(((double)delta[2]*basis[j*3+2]+(double)delta[1]*basis[j*3+1])+(double)delta[0]*basis[j*3]);if(!isfinite(target[j]))return RF_RANGE;}
    }
    *out=result;return RF_OK;
}

int rf_vfx_lights_sphere(const rf_vfx_light_candidate *lights,uint32_t count,
    const float center[3],float radius,uint32_t include_class,uint32_t include_other,
    uint32_t *indices,uint32_t capacity,uint32_t *selected)
{
    uint32_t i,j,n=0;
    if(!center || !selected || (count && (!lights || !indices)) || capacity<count ||
        count>SIZE_MAX/sizeof(*lights) || !isfinite(radius) || radius<0 || include_class>1 || include_other>1)return RF_RANGE;
    for(j=0;j<3;++j)if(!isfinite(center[j]))return RF_RANGE;
    /* Validate before writing caller storage. Bound coordinate differences to
     * finite floats; all subsequent squared arithmetic is in double. */
    for(i=0;i<count;++i) {
        const rf_vfx_light_candidate *c=lights+i;const rf_vfx_light_source *l=&c->source;
        double extent=0;
        if(c->reserved[0] || c->reserved[1] || !isfinite(l->radius) || l->radius<0)return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(l->color[j]) || !isfinite(l->position[j]) ||
            !isfinite((float)(l->position[j]-center[j])) || (l->type==4 &&
            (!isfinite(l->end[j]) || !isfinite((float)(l->end[j]-l->position[j])))))return RF_RANGE;
        for(j=0;j<3;++j) {
            if(fabs((double)l->position[j])+fabs((double)center[j])>FLT_MAX/8.0)return RF_RANGE;
            if(l->type==4){double d=(float)(l->end[j]-l->position[j]);extent+=d*d;}
        }
        if(l->type==4 && (extent>(FLT_MAX/8.0)*(FLT_MAX/8.0) || (extent!=0 && extent<(double)FLT_MIN*FLT_MIN)))return RF_RANGE;
    }
    for(i=0;i<count;++i) {
        const rf_vfx_light_candidate *c=lights+i;const rf_vfx_light_source *l=&c->source;
        float delta[3];double squared,sum;
        if(!c->enabled || !(l->color[0]!=0 || l->color[1]!=0 || l->color[2]!=0) ||
            (!include_class && c->light_class) || (!include_other && !c->light_class))continue;
        if(l->type==1){indices[n++]=i;continue;}
        if(l->type<2 || l->type>4)continue;
        for(j=0;j<3;++j)delta[j]=l->position[j]-center[j];
        if(l->type==4) {
            float d[3],length,reciprocal,projection,closest;
            for(j=0;j<3;++j)d[j]=l->end[j]-l->position[j];
            length=(float)sqrt(((double)d[0]*d[0]+(double)d[1]*d[1])+(double)d[2]*d[2]);
            if(length!=0) {
                reciprocal=1.0f/length;
                for(j=0;j<3;++j)d[j]*=reciprocal;
                projection=(float)(-(((double)d[0]*delta[0]+(double)d[1]*delta[1])+(double)d[2]*delta[2]));
                if(projection<0)projection=0;if(projection>length)projection=length;
                for(j=0;j<3;++j){float offset=d[j]*projection;closest=l->position[j]+offset;delta[j]=closest-center[j];}
            }
        }
        squared=((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2];
        sum=(double)radius+l->radius;if(squared<sum*sum)indices[n++]=i;
    }
    *selected=n;return RF_OK;
}

static int vfx_lights_box(const rf_vfx_light_candidate *lights,uint32_t count,
    const float minimum[3],const float maximum[3],uint32_t include_class,uint32_t include_other,
    uint32_t *indices,uint32_t capacity,uint32_t *selected,uint32_t filter)
{
    uint32_t i,j,n=0;
    if(!minimum || !maximum || !selected || (count && (!lights || !indices)) || capacity<count ||
        count>SIZE_MAX/sizeof(*lights) || include_class>1 || include_other>1)return RF_RANGE;
    for(j=0;j<3;++j)if(!isfinite(minimum[j]) || !isfinite(maximum[j]) || minimum[j]>maximum[j])return RF_RANGE;
    for(i=0;i<count;++i) {
        const rf_vfx_light_candidate *c=lights+i;const rf_vfx_light_source *l=&c->source;
        if(c->reserved[0] || c->reserved[1] || !isfinite(l->radius) || l->radius<0)return RF_RANGE;
        for(j=0;j<3;++j)if(!isfinite(l->color[j]) || !isfinite(l->position[j]) ||
            !isfinite((float)(minimum[j]-l->radius)) || !isfinite((float)(maximum[j]+l->radius)) ||
            (l->type==4 && !isfinite(l->end[j])))return RF_RANGE;
    }
    for(i=0;i<count;++i) {
        const rf_vfx_light_candidate *c=lights+i;const rf_vfx_light_source *l=&c->source;
        float lo[3],hi[3],scratch[3];uint32_t hit=1;
        if(filter && (!c->enabled || !(l->color[0]!=0 || l->color[1]!=0 || l->color[2]!=0) ||
            (!include_class && c->light_class) || (!include_other && !c->light_class)))continue;
        if(l->type==1){indices[n++]=i;continue;}
        if(l->type<2 || l->type>4)continue;
        for(j=0;j<3;++j){lo[j]=minimum[j]-l->radius;hi[j]=maximum[j]+l->radius;}
        if(l->type==4)rf_collision_segment_box(lo,hi,l->position,l->end,scratch,&hit);
        else for(j=0;j<3;++j)if(l->position[j]<lo[j] || l->position[j]>hi[j])hit=0;
        if(hit)indices[n++]=i;
    }
    *selected=n;return RF_OK;
}

int rf_vfx_lights_box(const rf_vfx_light_candidate *lights,uint32_t count,
    const float minimum[3],const float maximum[3],uint32_t include_class,uint32_t include_other,
    uint32_t *indices,uint32_t capacity,uint32_t *selected)
{
    return vfx_lights_box(lights,count,minimum,maximum,include_class,include_other,indices,capacity,selected,1);
}
int rf_vfx_light_cache_refresh(rf_vfx_light_cache *cache,uint32_t generation,
    const rf_vfx_light_candidate *lights,uint32_t count,const float minimum[3],const float maximum[3])
{
    uint32_t selected;int status;
    if(!cache || cache->valid>1 || cache->count>cache->capacity ||
        (cache->capacity && !cache->indices))return RF_RANGE;
    if(cache->valid && cache->generation==generation)return RF_OK;
    status=vfx_lights_box(lights,count,minimum,maximum,1,1,cache->indices,cache->capacity,&selected,0);
    if(status)return status;
    cache->generation=generation;cache->count=selected;cache->valid=1;return RF_OK;
}

int rf_vfx_lights_prepare(const rf_vfx_light_candidate *sources,uint32_t source_count,
    const rf_vfx_light_cache *cache,const rf_vfx_light_query *query,rf_vfx_light_source *out,
    uint32_t capacity,uint32_t *selected)
{
    uint32_t i,n=0,index,accepted;int status;
    if(!selected)return RF_RANGE;*selected=0;
    if(!cache || !query || cache->valid!=1 || cache->count>cache->capacity ||
        capacity<cache->count || (cache->count && (!cache->indices || !out || !sources)) ||
        source_count>SIZE_MAX/sizeof(*sources) || query->mode>1 || query->transformed>1)return RF_RANGE;
    /* Validate the query even for an empty cache. */
    if(query->mode==0)status=rf_vfx_lights_sphere(NULL,0,query->center,query->radius,query->include_class,query->include_other,NULL,0,&accepted);
    else status=rf_vfx_lights_box(NULL,0,query->center,query->maximum,query->include_class,query->include_other,NULL,0,&accepted);
    if(status)return status;
    for(i=0;i<cache->count;++i) {
        uint32_t source=cache->indices[i];if(source>=source_count)return RF_RANGE;
        if(query->mode==0)status=rf_vfx_lights_sphere(sources+source,1,query->center,query->radius,query->include_class,query->include_other,&index,1,&accepted);
        else status=rf_vfx_lights_box(sources+source,1,query->center,query->maximum,query->include_class,query->include_other,&index,1,&accepted);
        if(status)return status;if(!accepted)continue;
        if(query->transformed) {status=rf_vfx_light_transform(&sources[source].source,query->origin,query->basis,out+n);if(status)return status;}
        else out[n]=sources[source].source;
        ++n;
    }
    *selected=n;return RF_OK;
}

int rf_vfx_light_create(const rf_vfx_light_definition *definition,rf_vfx_light_candidate *out)
{
    rf_vfx_light_candidate value;rf_vfx_light_source *l=&value.source;uint32_t j;
    if(!definition || !out || definition->type<2 || definition->type>4 || definition->profile>3 ||
        !isfinite(definition->radius) || definition->radius<0 || !isfinite(definition->intensity) || definition->intensity<0)return RF_RANGE;
    memset(&value,0,sizeof(value));l->type=definition->type;l->profile=definition->profile;
    value.enabled=1;value.light_class=(unsigned char)definition->light_class;l->radius=definition->radius;
    for(j=0;j<3;++j) {
        if(!isfinite(definition->position[j]) || !isfinite(definition->color[j]) || definition->color[j]<0)return RF_RANGE;
        l->position[j]=definition->position[j];l->color[j]=definition->intensity*definition->color[j];if(!isfinite(l->color[j]))return RF_RANGE;
        if(l->type==3){if(!isfinite(definition->axis[j]))return RF_RANGE;l->axis[j]=definition->axis[j];}
        if(l->type==4){if(!isfinite(definition->end[j]))return RF_RANGE;l->end[j]=definition->end[j];}
    }
    if(l->type==4){l->radius-=0.1f;if(l->radius<=0)l->radius=0.1f;}
    if(l->type==3) {
        if(!isfinite(definition->inner_angle) || !isfinite(definition->outer_angle) ||
            definition->inner_angle<0 || definition->inner_angle>definition->outer_angle || definition->outer_angle>6.283185307179586 ||
            !isfinite(definition->cone_scale) || definition->cone_scale<0 || definition->cone_scale>1)return RF_RANGE;
        l->inner=(float)-cos((double)definition->inner_angle*0.5);l->outer=(float)-cos((double)definition->outer_angle*0.5);
        if(l->inner>l->outer)return RF_RANGE;l->cone_scale=definition->cone_scale;l->squared=definition->squared&255u;
    }
    *out=value;return RF_OK;
}

int rf_vfx_light_pool_init(rf_vfx_light_pool *pool,rf_vfx_light_candidate *sources,rf_vfx_light_link *links,uint32_t capacity)
{
    if(!pool || !sources || !links || !capacity || capacity>1100)return RF_RANGE;
    memset(pool,0,sizeof(*pool));memset(sources,0,(size_t)capacity*sizeof(*sources));memset(links,0,(size_t)capacity*sizeof(*links));
    pool->capacity=capacity;pool->sources=sources;pool->links=links;
    pool->first[0]=pool->first[1]=pool->last[0]=pool->last[1]=UINT32_MAX;return RF_OK;
}
static int vfx_light_pool_valid(const rf_vfx_light_pool *p)
{return p && p->sources && p->links && p->capacity && p->capacity<=1100 && p->count<=p->capacity;}
static int vfx_light_pool_id(const rf_vfx_light_pool *p,uint32_t id)
{return vfx_light_pool_valid(p) && id<p->capacity && p->sources[id].source.type!=0;}
int rf_vfx_light_pool_create(rf_vfx_light_pool *pool,const rf_vfx_light_definition *definition,uint32_t world,uint32_t *id)
{
    rf_vfx_light_candidate value;uint32_t i,list,tail;int status;
    if(!vfx_light_pool_valid(pool) || !id || world>1 || pool->count==pool->capacity)return RF_RANGE;
    status=rf_vfx_light_create(definition,&value);if(status)return status;
    for(i=0;i<pool->capacity && pool->sources[i].source.type;++i){}if(i==pool->capacity)return RF_RANGE;
    list=value.light_class && !world?1:0;tail=pool->last[list];pool->sources[i]=value;
    pool->links[i].references=0;pool->links[i].previous=tail;pool->links[i].next=UINT32_MAX;pool->links[i].list=list;
    if(tail==UINT32_MAX)pool->first[list]=i;else pool->links[tail].next=i;
    pool->last[list]=i;++pool->count;++pool->generation;pool->active=pool->active_count=0;*id=i;return RF_OK;
}
int rf_vfx_light_pool_retain(rf_vfx_light_pool *pool,uint32_t id)
{
    if(!vfx_light_pool_id(pool,id) || pool->links[id].references==INT32_MAX)return RF_RANGE;
    ++pool->links[id].references;return RF_OK;
}
int rf_vfx_light_pool_release_update(rf_vfx_light_pool *pool,uint32_t id,uint32_t update,
    rf_vfx_light_release_notify notify,void *context)
{
    rf_vfx_light_link *link;
    if(!vfx_light_pool_id(pool,id) || pool->links[id].references==INT32_MIN)return RF_RANGE;
    link=pool->links+id;
    if(--link->references<1) {
        ++pool->generation;
        if(notify && (pool->sources[id].light_class || (update&255u)==1u))notify(context,pool,id,update);
        if(link->previous==UINT32_MAX)pool->first[link->list]=link->next;else pool->links[link->previous].next=link->next;
        if(link->next==UINT32_MAX)pool->last[link->list]=link->previous;else pool->links[link->next].previous=link->previous;
        pool->sources[id].source.type=0;link->previous=link->next=UINT32_MAX;--pool->count;
    }
    pool->active=pool->active_count=0;return RF_OK;
}
int rf_vfx_light_pool_release(rf_vfx_light_pool *pool,uint32_t id)
{
    return rf_vfx_light_pool_release_update(pool,id,0,NULL,NULL);
}
int rf_vfx_light_pool_move(rf_vfx_light_pool *pool,uint32_t id,const float position[3])
{
    uint32_t j;if(!vfx_light_pool_id(pool,id) || !position)return RF_RANGE;
    for(j=0;j<3;++j)if(!isfinite(position[j]))return RF_RANGE;
    memcpy(pool->sources[id].source.position,position,12);++pool->generation;return RF_OK;
}
int rf_vfx_light_pool_enable(rf_vfx_light_pool *pool,uint32_t id,unsigned char enabled)
{
    if(!vfx_light_pool_id(pool,id))return RF_RANGE;pool->sources[id].enabled=enabled;++pool->generation;return RF_OK;
}
int rf_vfx_light_pool_cache(const rf_vfx_light_pool *pool,uint32_t world,
    const float minimum[3],const float maximum[3],rf_vfx_light_cache *cache)
{
    uint32_t id,n=0,walk=0,index,accepted;int status;
    if(!vfx_light_pool_valid(pool) || world>1 || !cache || cache->valid>1 || cache->count>cache->capacity ||
        (cache->capacity && !cache->indices))return RF_RANGE;
    if(cache->valid && cache->generation==pool->generation)return RF_OK;
    if(cache->capacity<pool->count)return RF_RANGE;
    /* Preflight geometry before writing any cache indices. */
    status=vfx_lights_box(NULL,0,minimum,maximum,1,1,NULL,0,&accepted,0);if(status)return status;
    for(id=pool->first[world?0:1];id!=UINT32_MAX;id=pool->links[id].next) {
        if(!vfx_light_pool_id(pool,id) || ++walk>pool->count)return RF_RANGE;
        status=vfx_lights_box(pool->sources+id,1,minimum,maximum,1,1,&index,1,&accepted,0);if(status)return status;
    }
    for(id=pool->first[world?0:1];id!=UINT32_MAX;id=pool->links[id].next) {
        vfx_lights_box(pool->sources+id,1,minimum,maximum,1,1,&index,1,&accepted,0);
        if(accepted)cache->indices[n++]=id;
    }
    cache->count=n;cache->generation=pool->generation;cache->valid=1;return RF_OK;
}

int rf_vfx_light_pool_color(rf_vfx_light_pool *pool,uint32_t id,float intensity,const float color[3],uint32_t *visibility_update)
{
    float rgb[3];uint32_t j,visibility;
    if(!vfx_light_pool_id(pool,id) || !color || !visibility_update || !isfinite(intensity))return RF_RANGE;
    for(j=0;j<3;++j){if(!isfinite(color[j]))return RF_RANGE;rgb[j]=(float)((double)intensity*color[j]);if(!isfinite(rgb[j]))return RF_RANGE;}
    visibility=pool->sources[id].light_class!=0;memcpy(pool->sources[id].source.color,rgb,12);
    if(!visibility)++pool->generation;*visibility_update=visibility;return RF_OK;
}

void rf_vfx_geometry_asset_close(rf_vfx_geometry_asset **out)
{
    uint32_t i;rf_vfx_geometry_asset *a;if(!out || !(a=*out))return;
    for(i=0;i<32;i++){rf_vfx_instance_close(a->instances+i);rf_vfx_mesh_close(a->meshes+i);}
    rf_vfx_material_bank_close(&a->material_bank);free(a);*out=NULL;
}
int rf_vfx_geometry_asset_open(rf_vpp *archive,const char *name,uint32_t budget,rf_vfx_geometry_asset **out)
{
    rf_vfx_directory directory={0};rf_vfx_geometry_asset *a=NULL;unsigned char *scratch=NULL;
    uint32_t i,used=sizeof(*a),temporary,maximum=0,mesh_count=0,global_materials=0,version4;int status;
    if(!archive || !name || !out || *out || budget<used)return RF_RANGE;
    status=rf_vfx_directory_open(archive,name,budget-used,&directory);if(status)return status;
    if(directory.header.version<0x3000a || !directory.count){status=RF_FORMAT;goto done;}
    version4=directory.header.version>=0x40000;
    for(i=0;i<directory.count;i++) {
        if(directory.chunks[i].type==0x4f584653u) {
            if(++mesh_count>32){status=RF_FORMAT;goto done;}
            if(directory.chunks[i].bytes>maximum)maximum=directory.chunks[i].bytes;
        } else if(version4 && directory.chunks[i].type==0x4c54414du)++global_materials;
        else {status=RF_FORMAT;goto done;}
    }
    if(!mesh_count || (version4 && directory.header.values[2]!=mesh_count)){status=RF_FORMAT;goto done;}
    if((uint64_t)used+directory.allocated_bytes+maximum>budget){status=RF_RANGE;goto done;}
    temporary=directory.allocated_bytes+maximum;
    a=calloc(1,sizeof(*a));if(!a){status=RF_IO;goto done;}
    a->version=directory.header.version;
    if(version4) {
        status=rf_vfx_material_bank_open(&directory,budget-used-temporary,&a->material_bank);if(status)goto done;
        if(a->material_bank->count!=global_materials){status=RF_FORMAT;goto done;}
        used+=a->material_bank->allocated_bytes;
    }
    scratch=malloc(maximum);if(!scratch){status=RF_IO;goto done;}
    for(i=0;i<directory.count;i++)if(directory.chunks[i].type==0x4f584653u) {
        uint32_t index=a->count;
        status=rf_vfx_chunk_read(&directory,i,0,scratch,directory.chunks[i].bytes);if(status)goto done;
        status=rf_vfx_mesh_open(scratch,directory.chunks[i].bytes,directory.header.version,global_materials,NULL,budget-used-temporary,a->meshes+index);if(status)goto done;
        used+=a->meshes[index]->allocated_bytes;
        if(a->meshes[index]->prefix.vertices) {
            status=rf_vfx_instance_open(a->meshes[index],budget-used-temporary,a->instances+index);if(status)goto done;
            used+=a->instances[index]->allocated_bytes;
        }
        ++a->count;
    }
    a->resident_bytes=used;a->peak_bytes=used+temporary;*out=a;a=NULL;status=RF_OK;
done:
    free(scratch);rf_vfx_directory_close(&directory);rf_vfx_geometry_asset_close(&a);return status;
}
