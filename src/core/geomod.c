#include "rf/geomod.h"
#include "rf/effect.h"
#include "rf/lightmap.h"
#include <math.h>
#include <float.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

int rf_geomod_plane_corner(const float planes[3][4],float position[3])
{
    double p[3][4],cross[3][3],det;float result[3];uint32_t i,j,k;
    if(!planes || !position)return RF_RANGE;
    for(i=0;i<3;i++) {
        double norm=0,sign=1;
        for(j=0;j<4;j++)if(!isfinite(planes[i][j]))return RF_FORMAT;
        for(j=0;j<3;j++)norm+=(double)planes[i][j]*planes[i][j];
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
        for(j=0;j<3;j++)if(planes[i][j]!=0){sign=planes[i][j]<0?-1:1;break;}
        for(j=0;j<4;j++)p[i][j]=planes[i][j]==0?0:sign*planes[i][j];
    }
    /* Canonical ordering fixes arithmetic order as well as plane identity. */
    for(i=1;i<3;i++)for(j=i;j>0;j--) {
        for(k=0;k<4 && p[j-1][k]==p[j][k];k++);
        if(k==4 || p[j-1][k]<p[j][k])break;
        for(k=0;k<4;k++){double t=p[j][k];p[j][k]=p[j-1][k];p[j-1][k]=t;}
    }
    for(i=0;i<3;i++)for(j=0;j<3;j++)cross[i][j]=
        p[(i+1)%3][(j+1)%3]*p[(i+2)%3][(j+2)%3]-p[(i+1)%3][(j+2)%3]*p[(i+2)%3][(j+1)%3];
    det=p[0][0]*cross[0][0]+p[0][1]*cross[0][1]+p[0][2]*cross[0][2];
    if(!isfinite(det) || fabs(det)<1e-10)return RF_FORMAT;
    for(j=0;j<3;j++) {
        result[j]=(float)(-(p[0][3]*cross[0][j]+p[1][3]*cross[1][j]+p[2][3]*cross[2][j])/det);
        if(!isfinite(result[j]))return RF_FORMAT;
    }
    for(i=0;i<3;i++) {
        double residual=p[i][3];for(j=0;j<3;j++)residual+=p[i][j]*result[j];
        if(fabs(residual)>1e-5)return RF_FORMAT;
    }
    memcpy(position,result,sizeof(result));return RF_OK;
}

int rf_geomod_debris_age(float age,float lifetime,float dt,uint32_t paused,
    rf_geomod_debris_lifecycle *out)
{
    rf_geomod_debris_lifecycle next;double alpha;
    if(!out || !isfinite(age) || age<0 || !isfinite(lifetime) || lifetime<0 ||
        !isfinite(dt) || dt<0 || paused>1)return RF_RANGE;
    next.age=age;next.removed=(double)age>(double)lifetime+1.;next.alpha=0;
    if(!next.removed) {
        if(!paused)next.age=(float)((double)age+dt);
        if(!isfinite(next.age))return RF_RANGE;
        alpha=next.age<=lifetime?255.:(1.-((double)next.age-lifetime))*255.;
        next.alpha=alpha<=0?0:alpha>=255?255:(uint32_t)alpha;
    }
    *out=next;return RF_OK;
}

int rf_geomod_lightmap_density(const float density[2],uint32_t detail,float out[2])
{
    static const double scale[4]={.5,1,2,4};float next[2];uint32_t i;
    if(!density || !out || detail>3)return RF_RANGE;
    for(i=0;i<2;i++) {
        if(!isfinite(density[i]) || density[i]<=0)return RF_RANGE;
        next[i]=(float)((double)density[i]*scale[detail]);
        if(!isfinite(next[i]) || next[i]<=0)return RF_RANGE;
    }
    memcpy(out,next,sizeof(next));return RF_OK;
}

int rf_geomod_lightmap_size(const float span[2],const float density[2],uint32_t special,
    uint32_t dimensions[2],float adjusted_density[2])
{
    uint32_t sizes[2],i,minimum=special?8:4;float adjusted[2];
    if(!span || !density || !dimensions || !adjusted_density || special>1)return RF_RANGE;
    for(i=0;i<2;i++) {
        double rounded;
        if(!isfinite(span[i]) || span[i]<0 || !isfinite(density[i]) || density[i]<=0)return RF_RANGE;
        rounded=(double)span[i]*density[i]+.5;
        if(rounded>INT32_MAX)return RF_RANGE;
        sizes[i]=(uint32_t)rounded;adjusted[i]=density[i];
        if(sizes[i]>64){adjusted[i]=(float)((64./sizes[i])*density[i]);sizes[i]=64;}
        if(sizes[i]<minimum){adjusted[i]=(float)(((double)minimum/(sizes[i]?sizes[i]:1))*adjusted[i]);sizes[i]=minimum;}
        if(!isfinite(adjusted[i]))return RF_RANGE;
    }
    memcpy(dimensions,sizes,sizeof(sizes));memcpy(adjusted_density,adjusted,sizeof(adjusted));return RF_OK;
}

int rf_geomod_light_noise(unsigned char *rgb,uint32_t bytes,uint32_t pitch,
    uint32_t width,uint32_t height,rf_random_state *random)
{
    uint32_t x,y,draw;rf_random_state next;
    if(!rgb || !random || !width || !height || (uint64_t)width*3>pitch ||
        (uint64_t)(height-1)*pitch+(uint64_t)width*3>bytes)return RF_RANGE;
    next=*random;
    for(y=0;y<height;y++)for(x=0;x<width;x++) {
        unsigned char value;rf_random_next(&next,&draw);value=(unsigned char)((draw&63u)+32u);
        memset(rgb+(size_t)y*pitch+x*3,value,3);
    }
    *random=next;return RF_OK;
}

int rf_geomod_random_basis(rf_random_state *random,float basis[9])
{
    rf_random_state next;float v[9]={0};double inverse;uint32_t i;int status;
    if(!random || !basis)return RF_RANGE;
    next=*random;status=rf_particle_cone_sample(-1,&next,v+6);if(status)return status;
    /*4fcfa0; keep the sampled forward vector, including the vertical branch. */
    if(v[6]<.0001f && v[6]>-.0001f && v[8]<.0001f && v[8]>-.0001f) {
        v[0]=1;v[6]=v[8]=0;v[7]=v[7]<0?-1.f:1.f;v[5]=-v[7];
    } else {
        v[0]=v[8];v[2]=-v[6];
        inverse=1.0/sqrt(((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2]);
        for(i=0;i<3;i++)v[i]=(float)((double)v[i]*inverse);
        for(i=0;i<3;i++)v[3+i]=(float)((double)v[6+(i+1)%3]*v[(i+2)%3]-(double)v[6+(i+2)%3]*v[(i+1)%3]);
    }
    memcpy(basis,v,sizeof(v));*random=next;return RF_OK;
}

int rf_geomod_light_visible(const rf_geomod_terrain_view *terrain,
    const float light[3],const float sample[3],uint32_t *visible)
{
    float delta[3],limit;double length=0;uint32_t i,matched;rf_collision_tree_hit hit;int status;
    const rf_collision_tree *tree;
    if(!terrain || !terrain->tree || !light || !sample || !visible)return RF_RANGE;
    tree=terrain->tree;
    for(i=0;i<3;i++) {
        if(!isfinite(light[i]) || !isfinite(sample[i]))return RF_FORMAT;
        delta[i]=sample[i]-light[i];if(!isfinite(delta[i]))return RF_FORMAT;
        length+=(double)delta[i]*delta[i];
    }
    length=sqrt(length);if(length<=.001){*visible=1;return RF_OK;}
    limit=(float)(1.0-.001/length);
    status=rf_collision_thin_tree(tree->nodes,tree->node_count,tree->faces,tree->face_count,0x100b,
        light,delta,limit,tree->stack,tree->node_capacity,&hit,&matched);
    if(status)return status;*visible=!matched;return RF_OK;
}

int rf_geomod_light_grid_open(const rf_geomod_vertex *vertices,uint32_t count,
    const float plane[4],float spacing,rf_geomod_light_grid *out)
{
    rf_geomod_light_grid g={0};uint32_t i,j,dims[2];
    if(!vertices || !plane || !out || count<3 || count>64)return RF_RANGE;
    if(!isfinite(spacing) || spacing<=0)return RF_FORMAT;
    for(i=0;i<4;i++)if(!isfinite(plane[i]))return RF_FORMAT;
    for(i=1;i<3;i++)if(fabsf(plane[i])>fabsf(plane[g.axis]))g.axis=i;
    if(plane[g.axis]==0)return RF_FORMAT;
    g.u=(g.axis+1)%3;g.v=(g.axis+2)%3;memcpy(g.plane,plane,16);
    for(i=0;i<count;i++) {
        double distance=plane[3];
        for(j=0;j<3;j++) {
            if(!isfinite(vertices[i].position[j]))return RF_FORMAT;
            distance+=(double)plane[j]*vertices[i].position[j];
        }
        if(fabs(distance)>1e-5)return RF_FORMAT;
        for(j=0;j<2;j++) {
            float value=vertices[i].position[j?g.v:g.u];
            if(!i || value<g.minimum[j])g.minimum[j]=value;
            if(!i || value>g.maximum[j])g.maximum[j]=value;
        }
    }
    for(j=0;j<2;j++) {
        double span=(double)g.maximum[j]-g.minimum[j],needed=ceil(span/spacing)+3;
        if(span<=0)return RF_FORMAT;
        if(needed>64)return RF_RANGE;
        dims[j]=4;while(dims[j]<needed)dims[j]*=2;
    }
    g.width=dims[0];g.height=dims[1];*out=g;return RF_OK;
}
int rf_geomod_light_grid_uv(const rf_geomod_light_grid *g,const float position[3],float uv[2])
{
    float result[2];unsigned j;
    if(!g || !position || !uv || g->u>2 || g->v>2 || g->width<4 || g->height<4)return RF_RANGE;
    for(j=0;j<2;j++) {
        double span=(double)g->maximum[j]-g->minimum[j],p=position[j?g->v:g->u];unsigned size=j?g->height:g->width;
        if(!isfinite(span) || span<=0 || !isfinite(p))return RF_FORMAT;
        result[j]=(float)((1.5+(p-g->minimum[j])*(size-3)/span)/size);
    }
    memcpy(uv,result,8);return RF_OK;
}
int rf_geomod_light_grid_sample(const rf_geomod_light_grid *g,const rf_geomod_vertex *vertices,
    uint32_t count,uint32_t x,uint32_t y,float position[3])
{
    double p[2],nearest[3]={0},best=1e300,center[3]={0},inset=0;int positive=0,negative=0;unsigned i,j;float out[3];
    if(!g || !vertices || !position || count<3 || count>64 || g->axis>2 || g->u>2 || g->v>2 ||
        g->axis==g->u || g->axis==g->v || g->u==g->v || g->width<4 || g->height<4 || x>=g->width || y>=g->height)return RF_RANGE;
    for(j=0;j<4;j++)if(!isfinite(g->plane[j]))return RF_FORMAT;
    if(g->plane[g->axis]==0)return RF_FORMAT;
    for(j=0;j<2;j++) {
        unsigned at=j?y:x,size=j?g->height:g->width;double span=(double)g->maximum[j]-g->minimum[j];
        if(!isfinite(span) || span<=0)return RF_FORMAT;
        p[j]=g->minimum[j]+((double)at-1)*span/(size-3);
    }
    for(i=0;i<count;i++) {
        const float *a=vertices[i].position,*b=vertices[(i+1)%count].position;
        double ax=a[g->u],ay=a[g->v],dx=(double)b[g->u]-ax,dy=(double)b[g->v]-ay;
        double cross=dx*(p[1]-ay)-dy*(p[0]-ax),length=dx*dx+dy*dy,t,q[2],distance;
        if(!isfinite(cross) || !isfinite(length) || length==0)return RF_FORMAT;
        positive|=cross>0;negative|=cross<0;
        for(j=0;j<3;j++)center[j]+=(double)a[j]/count;
        t=((p[0]-ax)*dx+(p[1]-ay)*dy)/length;t=fmax(0,fmin(1,t));
        q[0]=ax+t*dx;q[1]=ay+t*dy;distance=(p[0]-q[0])*(p[0]-q[0])+(p[1]-q[1])*(p[1]-q[1]);
        if(distance<best){best=distance;for(j=0;j<3;j++)nearest[j]=(double)a[j]+t*((double)b[j]-a[j]);}
    }
    if(positive && negative) {
        /* Retain the actual boundary segment. Reprojecting its rounded
         * coordinates onto the approximate face plane can move a corner
         * outside an adjacent edge of a nearly collinear polygon. */
        for(j=0;j<3;j++)out[j]=(float)nearest[j];
    } else {
        out[g->u]=(float)p[0];out[g->v]=(float)p[1];
        out[g->axis]=(float)(-((double)g->plane[g->u]*out[g->u]+(double)g->plane[g->v]*out[g->v]+g->plane[3])/g->plane[g->axis]);
    }
    /* Float vertices are only approximately coplanar/convex. If rounding
     * puts a sample outside a face halfspace, intersect its segment toward
     * the centroid with that halfspace, reserving one float relative-error
     * bound for the final store. This changes sampling only, never geometry. */
    for(i=0;i<count;i++) {
        const float *a=vertices[i].position,*b=vertices[(i+1)%count].position;
        double edge[3],normal[3],side=0,inside=0,error=0;
        for(j=0;j<3;j++)edge[j]=(double)b[j]-a[j];
        for(j=0;j<3;j++) {
            normal[j]=(double)g->plane[(j+1)%3]*edge[(j+2)%3]-(double)g->plane[(j+2)%3]*edge[(j+1)%3];
            side+=normal[j]*((double)out[j]-a[j]);inside+=normal[j]*(center[j]-a[j]);
            error+=fabs(normal[j])*fmax(fabs(out[j]),fabs(center[j]))*FLT_EPSILON;
        }
        if(side<0 && inside>0) {
            double fraction=(error-side)/(inside-side);
            if(fraction>inset)inset=fmin(1,fraction);
        }
    }
    if(inset>0)for(j=0;j<3;j++)out[j]=(float)((double)out[j]+inset*(center[j]-out[j]));
    for(j=0;j<3;j++)if(!isfinite(out[j]))return RF_FORMAT;
    memcpy(position,out,12);return RF_OK;
}

int rf_geomod_light_grid_bake_range(const rf_geomod_light_grid *g,const rf_geomod_vertex *vertices,uint32_t count,
    const rf_geomod_light_bake *lighting,unsigned char *packed,uint32_t pitch,uint32_t bytes,uint32_t first,uint32_t samples,uint32_t stats[3])
{
    unsigned char weights[63],rgb[3];uint32_t x,y,i,at,total[3]={0};float point[3],color[3];int status;
    if(!g || !vertices || !lighting || !packed || !stats || lighting->count>63 ||
        (lighting->count && (!lighting->sources || !lighting->shadow_modes)) || g->width<4 || g->width>64 || g->height<4 || g->height>64 ||
        pitch%2 || pitch<g->width*2 || (uint64_t)(g->height-1)*pitch+g->width*2>bytes)return RF_RANGE;
    if(first>g->width*g->height || samples>g->width*g->height-first)return RF_RANGE;
    status=rf_geomod_light_grid_sample(g,vertices,count,0,0,point);if(status)return status;
    for(i=0;i<lighting->count;i++) {
        weights[i]=255;
        if(lighting->shadow_modes[i]) {
            if(lighting->sources[i].type!=2 && lighting->sources[i].type!=3)return RF_NOT_FOUND;
            if(!lighting->terrain || !lighting->terrain->tree)return RF_RANGE;
        }
    }
    status=rf_vfx_light_accumulate(point,g->plane,lighting->ambient,lighting->directional_scale,
        lighting->sources,lighting->count,weights,1,color);if(status)return status;
    for(at=first;at<first+samples;at++) {
        x=at%g->width;y=at/g->width;
        status=rf_geomod_light_grid_sample(g,vertices,count,x,y,point);if(status)return status;
        for(i=0;i<lighting->count;i++) {
            uint32_t visible=1;weights[i]=255;
            if(lighting->shadow_modes[i]) {
                status=rf_geomod_light_visible(lighting->terrain,lighting->sources[i].position,point,&visible);if(status)return status;
                total[1]++;if(!visible){weights[i]=0;total[2]++;}
            }
        }
        status=rf_vfx_light_accumulate(point,g->plane,lighting->ambient,lighting->directional_scale,
            lighting->sources,lighting->count,weights,1,color);if(status)return status;
        status=rf_lightmap_accumulated_rgb(color,rgb);if(status)return status;
        status=rf_lightmap_pack_1555(rgb,3,1,1,0,packed+y*pitch+x*2,2,2);if(status)return status;
        total[0]++;
    }
    memcpy(stats,total,sizeof(total));return RF_OK;
}

int rf_geomod_light_grid_bake(const rf_geomod_light_grid *g,const rf_geomod_vertex *vertices,uint32_t count,
    const rf_geomod_light_bake *lighting,unsigned char *packed,uint32_t pitch,uint32_t bytes,uint32_t stats[3])
{
    if(!g || g->width>64 || g->height>64)return RF_RANGE;
    return rf_geomod_light_grid_bake_range(g,vertices,count,lighting,packed,pitch,bytes,0,g->width*g->height,stats);
}

int rf_geomod_planar_uv(const float normal[3],const float position[3],
    uint32_t width,uint32_t height,float uv[2])
{
    static const unsigned axes[3][2]={{2,1},{0,2},{1,0}};
    uint32_t i,major,u,v;float result[2],scale_u,scale_v;
    if(!normal || !position || !uv || !width || !height || width>INT32_MAX || height>INT32_MAX)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(normal[i]) || !isfinite(position[i]))return RF_FORMAT;
    if(normal[0]==0 && normal[1]==0 && normal[2]==0)return RF_FORMAT;
    /*4fa6d0 tie rules: Z wins a tie with the X/Y winner; Y wins X/Y. */
    major=fabsf(normal[0])<=fabsf(normal[1])?1:0;
    if(fabsf(normal[2])>=fabsf(normal[major]))major=2;
    u=axes[major][normal[major]>0?0:1];v=axes[major][normal[major]>0?1:0];
    scale_u=32.f/(float)width;scale_v=32.f/(float)height;
    result[0]=scale_u*position[u];result[1]=scale_v*position[v];
    if(!isfinite(result[0]) || !isfinite(result[1]))return RF_FORMAT;
    memcpy(uv,result,sizeof(result));return RF_OK;
}

int rf_geomod_debris_probe_points(const float origin[3],float radius,float endpoints[14][3])
{
    float result[14][3],diagonal;uint32_t i,j;
    if(!origin || !endpoints)return RF_RANGE;
    if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    for(j=0;j<3;j++)if(!isfinite(origin[j]))return RF_FORMAT;
    diagonal=(float)((double)radius*.5773500204086304f);
    for(i=0;i<14;i++)for(j=0;j<3;j++) {
        float offset=i<6?(i/2==j?(i&1?-radius:radius):0):((i-6)&(1u<<j)?-diagonal:diagonal);
        result[i][j]=origin[j]+offset;if(!isfinite(result[i][j]))return RF_FORMAT;
    }
    memcpy(endpoints,result,sizeof(result));return RF_OK;
}

int rf_geomod_debris_count(float radius,const rf_geomod_debris_probe probes[14],int32_t *count)
{
    float remaining;double result;uint32_t i;
    if(!probes || !count)return RF_RANGE;
    if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    remaining=(float)((double)radius*14.0);if(!isfinite(remaining))return RF_FORMAT;
    for(i=0;i<14;i++) {
        float distance=radius;
        if(probes[i].hit==1 && probes[i].has_face && !(probes[i].face_flags&8)) {
            distance=probes[i].fraction;
            if(!isfinite(distance) || distance<0 || distance>1)return RF_FORMAT;
        }
        remaining=(float)((double)remaining-distance);
    }
    result=floor((double)remaining*2.0);
    if(result<INT32_MIN || result>INT32_MAX)return RF_RANGE;
    *count=result<16?(int32_t)result:16;return RF_OK;
}

int rf_geomod_debris_launch(const float position[3],const float origin[3],
    float radius,float resistance,rf_random_state *random,float velocity[3])
{
    float direction[3],result[3],factor;double length=0;rf_random_state next;uint32_t i;int status;
    if(!position || !origin || !random || !velocity)return RF_RANGE;
    if(!isfinite(radius) || radius<=0 || !isfinite(resistance))return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(origin[i]))return RF_FORMAT;
        direction[i]=position[i]-origin[i];if(!isfinite(direction[i]))return RF_FORMAT;
        length+=(double)direction[i]*direction[i];
    }
    if(length==0){direction[0]=1;direction[1]=direction[2]=0;}
    else {length=1.0/sqrt(length);for(i=0;i<3;i++)direction[i]=(float)(direction[i]*length);}
    next=*random;status=rf_particle_cone_oriented(direction,.5f,&next,result);if(status)return status;
    factor=(float)(1.0-(double)resistance);
    for(i=0;i<3;i++) {
        result[i]=(float)((double)result[i]*6.0);
        if(radius<.13f)result[i]=(float)((double)result[i]*2.0);
        else {result[i]=(float)((double)result[i]*factor);result[i]=(float)((double)result[i]*1.4f);}
        if(!isfinite(result[i]))return RF_FORMAT;
    }
    memcpy(velocity,result,sizeof(result));*random=next;return RF_OK;
}

int rf_geomod_debris_contact(const float velocity[3],const float normal[3],
    float dt,float gravity,rf_random_state *random,rf_geomod_debris_bounce *out)
{
    rf_geomod_debris_bounce value={0};rf_random_state next;
    double length=0,dot,raw,ratio;float incoming,gdot,rounded_ratio,scale,direction[3],impulse;
    uint32_t i,draw;int status;
    if(!velocity || !normal || !random || !out)return RF_RANGE;
    if(!isfinite(dt) || dt<=0 || !isfinite(gravity) || gravity<0)return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(velocity[i]) || !isfinite(normal[i]))return RF_FORMAT;
        length+=(double)normal[i]*normal[i];
    }
    if(fabs(length-1.0)>1e-4)return RF_FORMAT;
    dot=((double)velocity[0]*normal[0]+(double)velocity[1]*normal[1])+(double)velocity[2]*normal[2];
    incoming=(float)-dot;gdot=(float)(-(double)gravity*normal[1]);
    if(!isfinite(incoming) || !isfinite(gdot))return RF_RANGE;
    next=*random;rf_random_next(&next,&draw);
    raw=((double).4f-(double).1f)*((double)draw/32768.0)+(double).1f;
    value.coefficient=(float)raw;
    /* The original zero-denominator ratio cannot satisfy both comparisons. */
    if(gdot!=0) {
        ratio=-(raw*incoming)/((double)dt*gdot);rounded_ratio=(float)ratio;
        if(ratio>0 && rounded_ratio<1) {
            if(rounded_ratio==0)return RF_RANGE;
            value.coefficient=(float)((double)value.coefficient/rounded_ratio);
        }
    }
    if(!isfinite(value.coefficient))return RF_RANGE;
    status=rf_particle_cone_oriented(normal,.98f,&next,direction);if(status)return status;
    scale=(float)((1.0+(double)value.coefficient)*incoming);if(!isfinite(scale))return RF_RANGE;
    for(i=0;i<3;i++) {
        impulse=(float)((double)direction[i]*scale);
        value.velocity[i]=(float)((double)velocity[i]+impulse);
        if(!isfinite(value.velocity[i]))return RF_RANGE;
    }
    status=rf_particle_cone_sample(-1,&next,value.spin_axis);if(status)return status;
    rf_random_next(&next,&draw);
    value.spin_rate=(float)(((double)6.2831853071795864769f-(double)3.14159265358979323846f)*
        ((double)draw/32768.0)+(double)3.14159265358979323846f);
    *out=value;*random=next;return RF_OK;
}

int rf_geomod_debris_relaunch(const float position[3],const float origin[3],
    float blast_radius,float chunk_radius,float resistance,uint32_t detail_marked,
    rf_random_state *random,rf_geomod_debris_relaunch_result *out,uint32_t *matched)
{
    rf_geomod_debris_relaunch_result value;rf_random_state next;
    float a[3],swap;double tail,distance;uint32_t i,j,draw;int status;
    if(!position || !origin || !random || !out || !matched)return RF_RANGE;
    if(!isfinite(blast_radius) || blast_radius<=0 || !isfinite(chunk_radius) ||
       chunk_radius<=0 || !isfinite(resistance))return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(origin[i]))return RF_FORMAT;
        a[i]=(float)((double)origin[i]-position[i]);if(!isfinite(a[i]))return RF_RANGE;
        a[i]=(float)fabs(a[i]);
    }
    if(detail_marked){*matched=0;return RF_OK;}
    /*4faf30 stores differences;4fa7a0 sorts magnitudes and retains its
     * approximate length through the strict comparison (no final float spill). */
    for(i=1;i<3;i++)for(j=i;j && a[j]>a[j-1];j--){swap=a[j];a[j]=a[j-1];a[j-1]=swap;}
    tail=(double)a[2]*.125+(double)a[1]*.25;distance=(tail+tail*.5)+a[0];
    if(!(distance<blast_radius)){*matched=0;return RF_OK;}
    next=*random;rf_random_next(&next,&draw);
    value.bounces=(uint32_t)floor(3.0+2.0*((double)draw/32768.0));
    status=rf_geomod_debris_launch(position,origin,chunk_radius,resistance,&next,value.velocity);if(status)return status;
    *out=value;*random=next;*matched=1;return RF_OK;
}

int rf_geomod_debris_select_room(const float center[3],const float normal[3],float radius,
    uint32_t fallback_room,rf_geomod_debris_room_query query,void *context,
    rf_random_state *random,rf_geomod_debris_burst_room *out)
{
    rf_geomod_debris_burst_room value={0};rf_random_state next;
    float distance,direction[3],offset;uint32_t i,j;int status;
    if(!center || !normal || !query || !random || !out || !isfinite(radius) || radius<=0)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(center[i]) || !isfinite(normal[i]))return RF_RANGE;
    next=*random;distance=(float)((double)radius*(double).1f);
    memcpy(direction,normal,sizeof(direction));
    /*48fedd initial lookup;48ff3b permits seven random alternatives. */
    for(i=0;i<8;i++) {
        if(i){status=rf_particle_cone_sample(-1,&next,direction);if(status)return status;}
        for(j=0;j<3;j++) {
            offset=(float)((double)direction[j]*(double)distance);
            value.origin[j]=(float)((double)center[j]+(double)offset);
            if(!isfinite(value.origin[j]))return RF_RANGE;
        }
        value.room=UINT32_MAX;value.queries=i+1;
        status=query(context,value.origin,&value.room);if(status)return status;
        if(value.room!=UINT32_MAX){*random=next;*out=value;return RF_OK;}
    }
    /*48ff71 restores the unshifted blast center before caller fallback. */
    memcpy(value.origin,center,sizeof(value.origin));value.room=fallback_room;
    value.used_fallback=fallback_room!=UINT32_MAX;
    *random=next;*out=value;return RF_OK;
}

int rf_geomod_debris_actor_contact(const float position[3],const float velocity[3],float radius,
    const float actor_position[3],float actor_radius,uint32_t *hit,float *amount)
{
    float delta[3],combined,value=0;double distance,speed;uint32_t i,matched;
    if(!position || !velocity || !actor_position || !hit || !amount ||
       !isfinite(radius) || radius<0 || !isfinite(actor_radius) || actor_radius<0)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(position[i]) || !isfinite(velocity[i]) || !isfinite(actor_position[i]))return RF_RANGE;
        delta[i]=actor_position[i]-position[i];if(!isfinite(delta[i]))return RF_RANGE;
    }
    combined=actor_radius+radius;if(!isfinite(combined))return RF_RANGE;
    distance=(double)delta[0]*delta[0]+(double)delta[1]*delta[1]+(double)delta[2]*delta[2];
    matched=distance<(double)combined*combined;
    if(matched) {
        speed=sqrt((double)velocity[0]*velocity[0]+(double)velocity[1]*velocity[1]+(double)velocity[2]*velocity[2]);
        value=(float)(speed*radius*.5);if(!isfinite(value))return RF_RANGE;
    }
    *hit=matched;*amount=value;return RF_OK;
}

int rf_geomod_piece_recenter(const float (*vertices)[3],uint32_t count,
    float (*local)[3],rf_geomod_piece_placement *placement)
{
    rf_geomod_piece_placement value;float maximum_squared=0;uint32_t i,k;
    if(!vertices || !local || !placement || !count || count>INT32_MAX)return RF_RANGE;
    for(k=0;k<3;k++)value.minimum[k]=value.maximum[k]=vertices[0][k];
    for(i=0;i<count;i++)for(k=0;k<3;k++) {
        float v=vertices[i][k];if(!isfinite(v))return RF_FORMAT;
        if(v<value.minimum[k])value.minimum[k]=v;
        if(v>value.maximum[k])value.maximum[k]=v;
    }
    for(k=0;k<3;k++) {
        float sum;
        value.minimum[k]=(float)((double)value.minimum[k]-(double).0001f);
        value.maximum[k]=(float)((double)value.maximum[k]+(double).0001f);
        sum=(float)((double)value.minimum[k]+value.maximum[k]);value.origin[k]=(float)((double)sum*.5);
        value.minimum[k]=(float)((double)value.minimum[k]-value.origin[k]);
        value.maximum[k]=(float)((double)value.maximum[k]-value.origin[k]);
        if(!isfinite(value.origin[k]) || !isfinite(value.minimum[k]) || !isfinite(value.maximum[k]))return RF_RANGE;
    }
    for(i=0;i<count;i++) {
        float v[3];double squared;
        for(k=0;k<3;k++){v[k]=(float)((double)vertices[i][k]-value.origin[k]);if(!isfinite(v[k]))return RF_RANGE;}
        squared=((double)v[0]*v[0]+(double)v[1]*v[1])+(double)v[2]*v[2];
        if(squared>maximum_squared)maximum_squared=(float)squared;
    }
    value.radius=(float)sqrt((double)maximum_squared);if(!isfinite(value.radius))return RF_RANGE;
    for(i=0;i<count;i++)for(k=0;k<3;k++)local[i][k]=(float)((double)vertices[i][k]-value.origin[k]);
    *placement=value;return RF_OK;
}

int rf_geomod_piece_shape_get(const float minimum[3],const float maximum[3],
    float radius,uint32_t attempts,rf_geomod_piece_shape *out)
{
    float d[3],small;uint32_t i,axis;rf_geomod_piece_shape value={0};
    if(!minimum || !maximum || !out)return RF_RANGE;
    if(!isfinite(radius) || radius<0)return RF_FORMAT;
    for(i=0;i<3;i++) {
        if(!isfinite(minimum[i]) || !isfinite(maximum[i]))return RF_FORMAT;
        d[i]=(float)((double)maximum[i]-minimum[i]);
        if(!isfinite(d[i]) || d[i]<=0)return RF_FORMAT;
    }
    axis=d[0]>d[1]?(d[0]>d[2]?0:2):(d[1]>d[2]?1:2);
    small=d[0]<d[1]?d[0]:d[1];if(d[2]<small)small=d[2];
    value.axis[axis]=1;value.length=d[axis];
    value.aspect=(float)((double)value.length/small);
    if(!isfinite(value.aspect))return RF_RANGE;
    value.subdivide=radius>=1.5f && attempts<10 &&
        (value.aspect>3 || (double)value.length/value.aspect>10);
    *out=value;return RF_OK;
}

int rf_geomod_component_classify(const rf_collision_face *faces,const int32_t *labels,
    uint32_t count,int32_t selector,const rf_collision_bounds *bounds,uint32_t *solid)
{
    uint32_t i,j,k;float span,length;
    if(!faces || !labels || !bounds || !solid || count==UINT32_MAX)return RF_RANGE;
    for(k=0;k<3;k++)if(!isfinite(bounds->minimum[k]) || !isfinite(bounds->maximum[k]) || bounds->minimum[k]>bounds->maximum[k])return RF_FORMAT;
    span=(float)(((double)bounds->maximum[0]-bounds->minimum[0])+((double)bounds->maximum[1]-bounds->minimum[1])+((double)bounds->maximum[2]-bounds->minimum[2]));
    length=(float)((double)span+1);if(!isfinite(span) || !isfinite(length))return RF_RANGE;
    for(i=0;i<count;i++)if(selector<0 || labels[i]==selector) {
        const rf_collision_face *face=faces+i;rf_collision_room_query query={0};
        float center[3]={0},reciprocal;uint32_t retry=0;int status;
        if(!face->vertices || face->count<3)return RF_FORMAT;
        for(j=0;j<face->count;j++)for(k=0;k<3;k++) {
            if(!isfinite(face->vertices[j][k]))return RF_FORMAT;
            center[k]=(float)((double)center[k]+face->vertices[j][k]);
        }
        reciprocal=(float)(1.0/face->count);
        for(k=0;k<3;k++) {
            float offset,step;
            if(!isfinite(face->plane[k]))return RF_FORMAT;
            center[k]=(float)((double)center[k]*reciprocal);
            offset=(float)((double)face->plane[k]*span);
            query.start[k]=(float)((double)center[k]+offset);query.direction[k]=-face->plane[k];
            step=(float)((double)query.direction[k]*length);query.endpoint[k]=(float)((double)query.start[k]+step);
        }
        for(j=0;j<count;j++)if(selector<0 || labels[j]==selector) {
            status=rf_collision_room_query_face(&query,faces+j,j+1,&retry);if(status)return status;
            if(retry)break;
        }
        if(!retry){*solid=query.selected_face && query.front;return RF_OK;}
    }
    *solid=0;return RF_OK;
}

int rf_geomod_debris_rotate(const float basis[9],const float axis[3],float spin,float dt,float result[9])
{
    static const unsigned char order[9][3]={{0,2,1},{2,1,0},{1,0,2},{2,1,0},{2,1,0},{0,1,2},{2,1,0},{2,0,1},{1,0,2}};
    double angle,s,c,wz;float delta,x,y,z,xx,yy,zz,xy,xz,yz,wx,wy,r[9],out[9];uint32_t i,row,col;
    if(!basis || !axis || !result || !isfinite(spin) || !isfinite(dt) || dt<0)return RF_RANGE;
    for(i=0;i<9;i++)if(!isfinite(basis[i]))return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(axis[i]))return RF_RANGE;
    delta=(float)((double)dt*spin);if(!isfinite(delta))return RF_RANGE;
    angle=(double)delta*.5;s=sin(angle);c=cos(angle);
    x=(float)(s*axis[0]);y=(float)(s*axis[1]);z=(float)(s*axis[2]);
    xx=x*x;yy=y*y;zz=z*z;xy=x*y;xz=x*z;yz=y*z;wx=(float)(c*x);wy=(float)(c*y);wz=c*z;
    r[0]=(float)((1.0-2.0*yy)-2.0*zz);r[1]=(float)(2.0*(wz+xy));r[2]=(float)(2.0*xz-2.0*wy);
    r[3]=(float)(2.0*xy-2.0*wz);r[4]=(float)((1.0-2.0*xx)-2.0*zz);r[5]=(float)(2.0*((double)wx+yz));
    r[6]=(float)(2.0*((double)wy+xz));r[7]=(float)(2.0*yz-2.0*wx);r[8]=(float)((1.0-2.0*xx)-2.0*yy);
    for(row=0;row<3;row++)for(col=0;col<3;col++) {
        const unsigned char *k=order[row*3+col];double v=(double)r[row*3+k[0]]*basis[k[0]*3+col];
        for(i=1;i<3;i++)v+=(double)r[row*3+k[i]]*basis[k[i]*3+col];
        out[row*3+col]=(float)v;if(!isfinite(out[row*3+col]))return RF_RANGE;
    }
    memcpy(result,out,36);return RF_OK;
}

int rf_geomod_debris_gravity(float velocity_y,float acceleration,float dt,float *out)
{
    float value;
    if(!out || !isfinite(velocity_y) || !isfinite(acceleration) || !isfinite(dt) || dt<0)return RF_RANGE;
    value=(float)((double)velocity_y-(double)acceleration*dt);
    if(!isfinite(value))return RF_RANGE;
    *out=value;return RF_OK;
}

int rf_geomod_debris_motion(const float start[3],const float velocity[3],float dt,
    uint32_t liquid_flag,float depth,float bottom,float proposed[3])
{
    float value[3],scale=1;uint32_t i;
    if(!start || !velocity || !proposed || !isfinite(dt) || dt<0)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(start[i]) || !isfinite(velocity[i]))return RF_RANGE;
    if((liquid_flag&255u)==1) {
        if(!isfinite(depth) || !isfinite(bottom))return RF_RANGE;
        if((double)start[1]<=(double)depth+(double)bottom)scale=.2f;
    }
    for(i=0;i<3;i++) {
        float step=(float)((double)velocity[i]*dt);
        step=(float)((double)step*scale);value[i]=start[i]+step;
        if(!isfinite(value[i]))return RF_RANGE;
    }
    memcpy(proposed,value,sizeof(value));return RF_OK;
}

int rf_geomod_debris_liquid_miss(const float start[3],const float end[3],
    uint32_t liquid_flag,float depth,float bottom,rf_geomod_debris_liquid_hit *hit,uint32_t *matched)
{
    rf_geomod_debris_liquid_hit value;double raw;float height;uint32_t i;
    if(!start || !end || !hit || !matched)return RF_RANGE;
    for(i=0;i<3;i++)if(!isfinite(start[i]) || !isfinite(end[i]))return RF_RANGE;
    if(!(liquid_flag&255u)){*matched=0;return RF_OK;}
    if(!isfinite(depth) || !isfinite(bottom))return RF_RANGE;
    raw=(double)depth+(double)bottom+.5;
    if(raw<=-2147483649.0 || raw>=2147483648.0)return RF_RANGE;
    /* Bounded IEEE double truncation without a temporary x87 rounding-mode
     * switch. Keep the liquid query from perturbing following SSE simulation
     * on the Xbox emulator's mixed x87/SSE path. */
    {uint64_t bits,magnitude;int exponent;int32_t truncated;
     memcpy(&bits,&raw,sizeof(bits));exponent=(int)((bits>>52)&2047u)-1023;
     magnitude=exponent<0?0:((bits&UINT64_C(0xfffffffffffff))|UINT64_C(0x10000000000000))>>(52-exponent);
     truncated=(int32_t)((bits>>63)?-(int64_t)magnitude:(int64_t)magnitude);
     height=(float)truncated;}
    if(!(end[1]<height && start[1]>height)){*matched=0;return RF_OK;}
    value.fraction=(float)(((double)start[1]-(double)height)/((double)start[1]-(double)end[1]));
    /*48fcd8..48fd0b deliberately use start-end, not end-start. */
    for(i=0;i<3;i++) {
        value.point[i]=(float)((double)start[i]+((double)start[i]-(double)end[i])*(double)value.fraction);
        if(!isfinite(value.point[i]))return RF_RANGE;
    }
    *hit=value;*matched=1;return RF_OK;
}

int rf_geomod_debris_birth(float blast_radius,rf_random_state *random,rf_geomod_debris_birth_result *out)
{
    rf_geomod_debris_birth_result value={0};rf_random_state next;float scale;
    double unit,raw_radius;uint32_t i,draw;int status;
    if(!random || !out)return RF_RANGE;
    if(!isfinite(blast_radius) || blast_radius<=0)return RF_FORMAT;
    next=*random;status=rf_particle_cone_sample(-1,&next,value.displacement);if(status)return status;
    rf_random_next(&next,&draw);scale=(float)(((double)draw/32768.0)*.5*(double)blast_radius);
    for(i=0;i<3;i++)value.displacement[i]=(float)((double)value.displacement[i]*scale);
    rf_random_next(&next,&draw);unit=(double)draw/32768.0;
    raw_radius=unit*unit*unit*(double).2f+(double).05f;
    value.radius=(float)raw_radius;value.resistance=(float)((raw_radius-(double).05f)*5.0);
    value.flags=raw_radius<(double).15f?2u:0u;
    rf_random_next(&next,&draw);value.bounces=3+draw%3;
    status=rf_particle_cone_sample(-1,&next,value.axis);if(status)return status;
    rf_random_next(&next,&draw);value.spin=(float)(((double)draw/32768.0)*(double)3.14159265358979323846f+
        (double)3.14159265358979323846f);
    for(i=0;i<3;i++)if(!isfinite(value.displacement[i]))return RF_RANGE;
    *out=value;*random=next;return RF_OK;
}

int rf_geomod_debris_build(float radius,uint32_t width,uint32_t height,
    rf_random_state *random,rf_geomod_debris_mesh *out)
{
    static const uint32_t faces[12][3]={{0,2,6},{0,6,4},{0,3,2},{0,1,3},
        {0,1,5},{0,5,4},{7,3,1},{7,1,5},{7,4,6},{7,5,4},{7,2,3},{7,6,2}};
    rf_geomod_debris_mesh mesh={0};rf_random_state next;uint32_t i,j,k,draw;
    float low,high;int status;
    if(!random || !out || !width || !height || width>INT32_MAX || height>INT32_MAX)return RF_RANGE;
    if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    low=(float)((double)radius*.2f);high=(float)((double)radius*1.8f);
    if(!isfinite(high) || low==0)return RF_FORMAT;
    next=*random;rf_random_next(&next,&draw);mesh.lifetime=(float)(1.0+3.0*((double)draw/32768.0));
    for(i=0;i<8;i++)for(j=0;j<3;j++) {
        double a=(i&(1u<<j))?low:-low,b=(i&(1u<<j))?high:-high;
        rf_random_next(&next,&draw);mesh.positions[i][j]=(float)((b-a)*((double)draw/32768.0)+a);
    }
    memcpy(mesh.indices,faces,sizeof(faces));
    for(i=0;i<12;i++) {
        float a[3],b[3],normal[3];double length=0;
        for(j=0;j<3;j++) {
            a[j]=mesh.positions[faces[i][1]][j]-mesh.positions[faces[i][0]][j];
            b[j]=mesh.positions[faces[i][2]][j]-mesh.positions[faces[i][0]][j];
        }
        for(j=0;j<3;j++) {
            normal[j]=(float)((double)a[(j+1)%3]*b[(j+2)%3]-(double)a[(j+2)%3]*b[(j+1)%3]);
            length+=(double)normal[j]*normal[j];
        }
        if(!isfinite(length) || length==0)return RF_FORMAT;
        length=1.0/sqrt(length);for(j=0;j<3;j++)normal[j]=(float)(normal[j]*length);
        for(k=0;k<3;k++) {
            status=rf_geomod_planar_uv(normal,mesh.positions[faces[i][k]],width,height,mesh.uv[i][k]);
            if(status)return status;
        }
    }
    *random=next;*out=mesh;return RF_OK;
}

int rf_geomod_position_encode(const float minimum[3],const float maximum[3],
    const float position[3],uint16_t packed[3])
{
    uint16_t result[3]={0};uint32_t i,outside=0;
    if(!minimum || !maximum || !position || !packed)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(minimum[i]) || !isfinite(maximum[i]) || !isfinite(position[i]) || maximum[i]<=minimum[i])return RF_FORMAT;
        if(position[i]<minimum[i] || position[i]>maximum[i])outside=1;
    }
    if(!outside)for(i=0;i<3;i++) {
        /* Stores force double rounding even on an extended-precision x87 host. */
        volatile double extent=(double)maximum[i]-minimum[i];
        volatile double factor=65536.0/extent;
        volatile double offset=(double)position[i]-minimum[i];
        volatile double code=offset*factor;
        result[i]=(uint16_t)((uint32_t)code&65535u);
    }
    memcpy(packed,result,sizeof(result));return RF_OK;
}
int rf_geomod_position_decode(const float minimum[3],const float maximum[3],
    const uint16_t packed[3],float position[3])
{
    float result[3];uint32_t i;
    if(!minimum || !maximum || !packed || !position)return RF_RANGE;
    for(i=0;i<3;i++) {
        volatile double extent,step,offset,value;
        if(!isfinite(minimum[i]) || !isfinite(maximum[i]) || maximum[i]<=minimum[i])return RF_FORMAT;
        extent=(double)maximum[i]-minimum[i];step=extent/65536.0;
        offset=step*packed[i];value=offset+minimum[i];result[i]=(float)value;
        if(!isfinite(result[i]))return RF_FORMAT;
    }
    memcpy(position,result,sizeof(result));return RF_OK;
}

int rf_geomod_shallow_align(const float requested[3],float template_radius,
    const rf_geomod_shallow_limit *selected,uint32_t count,
    const rf_geomod_shallow_history *history,uint32_t history_count,float adjusted[3])
{
    double normals[2][3]={{0}},corrections[2][3]={{0}};
    uint32_t i,j,k,m,found[2]={0};float result[3];
    if(!requested || !adjusted || count>2 || history_count>128 ||
        (count && !selected) || (history_count && !history))return RF_RANGE;
    if(!isfinite(template_radius) || template_radius<=0)return RF_FORMAT;
    for(j=0;j<3;j++)if(!isfinite(requested[j]))return RF_FORMAT;
    for(i=0;i<count;i++) {
        double length=0;
        if(!isfinite(selected[i].depth))return RF_FORMAT;
        for(j=0;j<3;j++) {
            float v=selected[i].normal[j]*selected[i].depth;
            if(!isfinite(selected[i].normal[j]) || !isfinite(v))return RF_FORMAT;
            normals[i][j]=v;length+=(double)v*v;
        }
        if(length>0){length=sqrt(length);for(j=0;j<3;j++)normals[i][j]/=length;}
    }
    for(i=0;i<history_count;i++) {
        double delta[3],distance=0,radius;
        if(!isfinite(history[i].scale) || history[i].scale<0)return RF_FORMAT;
        for(j=0;j<3;j++) {
            if(!isfinite(history[i].center[j]))return RF_FORMAT;
            delta[j]=(double)requested[j]-history[i].center[j];distance+=delta[j]*delta[j];
            for(k=0;k<2;k++)if(!isfinite(history[i].vectors[k][j]))return RF_FORMAT;
        }
        if(history[i].vectors[0][0]==0 && history[i].vectors[0][1]==0 && history[i].vectors[0][2]==0)continue;
        radius=(double)template_radius*history[i].scale;
        if(distance>=radius*radius)continue;
        for(k=0;k<2;k++) {
            double length=0,n[3],signed_distance=0;
            for(j=0;j<3;j++)length+=(double)history[i].vectors[k][j]*history[i].vectors[k][j];
            if(length==0)continue;
            for(j=0;j<3;j++){n[j]=history[i].vectors[k][j]/sqrt(length);signed_distance+=n[j]*delta[j];}
            if(signed_distance<=0 || signed_distance*signed_distance>=length)continue;
            for(m=0;m<count;m++)if(!found[m]) {
                double dot=0;for(j=0;j<3;j++)dot+=n[j]*normals[m][j];
                if(dot<.95)continue;
                for(j=0;j<3;j++)corrections[m][j]=-signed_distance*n[j];
                found[m]=1;
            }
        }
    }
    for(j=0;j<3;j++) {
        result[j]=(float)(requested[j]+corrections[0][j]+corrections[1][j]);
        if(!isfinite(result[j]))return RF_FORMAT;
    }
    memcpy(adjusted,result,sizeof(result));return RF_OK;
}

int rf_geomod_shallow_normalize(const rf_geomod_shallow_limit *selected,uint32_t count,
    rf_geomod_shallow_limit out[2],uint32_t *out_count)
{
    rf_geomod_shallow_limit result[2]={0};uint32_t i,j,used=0;
    if(!out || !out_count || count>2 || (count && !selected))return RF_RANGE;
    for(i=0;i<count;i++) {
        double length=0;float vector[3];
        if(!isfinite(selected[i].depth))return RF_FORMAT;
        for(j=0;j<3;j++) {
            if(!isfinite(selected[i].normal[j]))return RF_FORMAT;
            vector[j]=selected[i].normal[j]*selected[i].depth;
            if(!isfinite(vector[j]))return RF_FORMAT;
            length+=(double)vector[j]*vector[j];
        }
        if(length==0) {if(!i)break;continue;}
        result[used].depth=(float)sqrt(length);
        if(!isfinite(result[used].depth) || result[used].depth<=0)return RF_FORMAT;
        for(j=0;j<3;j++)result[used].normal[j]=vector[j]/result[used].depth;
        used++;
    }
    memcpy(out,result,sizeof(result));*out_count=used;return RF_OK;
}

int rf_geomod_shallow_point(const float center[3],const float point[3],float radius,
    const rf_geomod_shallow_limit *limits,uint32_t count,float out[3])
{
    float original[3],current[3],result[3];uint32_t i,j;
    if(!center || !point || !out || count>2 || (count && !limits))return RF_RANGE;
    if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    for(j=0;j<3;j++) {
        if(!isfinite(center[j]) || !isfinite(point[j]))return RF_FORMAT;
        original[j]=current[j]=point[j]-center[j];
        if(!isfinite(original[j]))return RF_FORMAT;
    }
    for(i=0;i<count;i++) {
        double length=0,gate=0,dot=0,distance=0;float projected[3];
        if(!isfinite(limits[i].depth) || limits[i].depth<0)return RF_FORMAT;
        for(j=0;j<3;j++) {
            float n=limits[i].normal[j];if(!isfinite(n))return RF_FORMAT;
            length+=(double)n*n;gate+=(double)original[j]*n;dot+=(double)current[j]*n;
        }
        if(fabs(length-1)>0.00001)return RF_FORMAT;
        if(gate<=0)continue;
        for(j=0;j<3;j++) {
            double delta;projected[j]=(float)(current[j]-dot*limits[i].normal[j]);
            delta=(double)current[j]-projected[j];distance+=delta*delta;
        }
        distance=sqrt(distance)*limits[i].depth/radius;
        for(j=0;j<3;j++)current[j]=(float)(projected[j]+limits[i].normal[j]*distance);
    }
    for(j=0;j<3;j++){result[j]=count?center[j]+current[j]:point[j];if(!isfinite(result[j]))return RF_FORMAT;}
    memcpy(out,result,sizeof(result));return RF_OK;
}

int rf_geomod_regions_prepare(const rf_geo_region *regions,uint32_t count,uint32_t stored_default,
    const float position[3],float scale,rf_geomod_region_result *out)
{
    rf_geomod_hardness_result result={0,1,0,0,0};rf_geomod_region_result prepared={0};uint32_t i,j,k;
    if(!position || !out || (count && !regions) || count>4096 || stored_default>100)return RF_RANGE;
    if(!isfinite(scale) || scale<0)return RF_FORMAT;
    for(j=0;j<3;j++)if(!isfinite(position[j]))return RF_FORMAT;
    result.scale=scale;
    for(i=0;i<count;i++) {
        const rf_geo_region *r=regions+i;float delta[3];int inside=1;
        uint32_t type=r->flags&7;
        if((type!=2 && type!=4) || r->hardness>100)return RF_FORMAT;
        for(j=0;j<3;j++) {
            if(!isfinite(r->position[j]))return RF_FORMAT;
            delta[j]=position[j]-r->position[j];if(!isfinite(delta[j]))return RF_FORMAT;
        }
        if(type==2) {
            double length;if(!isfinite(r->radius) || r->radius<0)return RF_FORMAT;
            length=sqrt(((double)delta[0]*delta[0]+(double)delta[1]*delta[1])+(double)delta[2]*delta[2]);
            inside=length<r->radius;
        } else {
            /*52cac0 stores serialized forward/right/up as runtime rows R/U/F. */
            static const uint32_t row[3]={3,6,0};
            for(j=0;j<9;j++)if(!isfinite(r->file_basis[j]))return RF_FORMAT;
            for(j=0;j<3;j++) {
                float local;double dot=0;
                if(!isfinite(r->dimensions[j]) || r->dimensions[j]<0)return RF_FORMAT;
                for(k=0;k<3;k++)dot+=(double)delta[k]*r->file_basis[row[j]+k];
                local=(float)dot;
                if(local<-.5f*r->dimensions[j] || local>.5f*r->dimensions[j])inside=0;
            }
        }
        if(inside) {
            if(!result.matches || r->hardness>result.hardness)result.hardness=r->hardness;
            result.matches++;if(r->flags&64)result.flags|=0x10;
            if(r->flags&32) {
                double length=0,dot=0;rf_geomod_shallow_limit limit;
                if(!isfinite(r->shallow_depth))return RF_FORMAT;
                limit.depth=r->shallow_depth;
                for(j=0;j<3;j++) {
                    limit.normal[j]=-r->file_basis[6+j];
                    if(!isfinite(limit.normal[j]))return RF_FORMAT;
                    length+=(double)limit.normal[j]*limit.normal[j];
                    dot+=(double)limit.normal[j]*prepared.limits[0].normal[j];
                }
                if(fabs(length-1)>0.00001)return RF_FORMAT;
                if(prepared.limit_count==1 && dot>(double).95f)continue;
                if(prepared.limit_count==2 || (prepared.limit_count==1 && dot<(double)-.1f)) {
                    result.allowed=0;break;
                }
                prepared.limits[prepared.limit_count++]=limit;
            }
        }
    }
    if(!result.matches)result.hardness=stored_default?stored_default:55;
    if(result.hardness==100)result.allowed=0;
    if(result.allowed) {
        float factor=(float)(1.0-(double)result.hardness*(double).01f);
        if(factor<0)factor=0;if(factor>1)factor=1;
        result.scale*=factor;
    }
    prepared.hardness=result;*out=prepared;return RF_OK;
}
int rf_geomod_hardness(const rf_geo_region *regions,uint32_t count,uint32_t stored_default,
    const float position[3],float scale,rf_geomod_hardness_result *out)
{
    rf_geomod_region_result prepared;int status;
    if(!out)return RF_RANGE;
    status=rf_geomod_regions_prepare(regions,count,stored_default,position,scale,&prepared);
    if(status)return status;
    if(prepared.limit_count)return RF_NOT_FOUND;
    *out=prepared.hardness;return RF_OK;
}

static int append(rf_geomod_vertex *out,uint32_t *count,const rf_geomod_vertex *v)
{
    if(*count==RF_GEOMOD_POLYGON_LIMIT)return RF_RANGE;
    out[(*count)++]=*v;return RF_OK;
}
static rf_geomod_compaction_observer compaction_observer;
static void *compaction_context;
void rf_geomod_observe_compaction(rf_geomod_compaction_observer observer,void *context)
{compaction_observer=observer;compaction_context=context;}
static rf_geomod_intersection_observer intersection_observer;
static void *intersection_context;
void rf_geomod_observe_intersections(rf_geomod_intersection_observer observer,void *context)
{intersection_observer=observer;intersection_context=context;}
/* Original endpoints give every subdivision of a partition diagonal the
 * same future clipping intersection. IDs occupy a separate support domain. */
#define GEOMOD_DIAGONAL_BASE (32u+RF_GEOMOD_CUT_LIMIT*128u)
typedef struct geomod_diagonals {
    float endpoints[RF_GEOMOD_WORK_FACES][2][3];uint32_t count;
} geomod_diagonals;
static int diagonal_register(geomod_diagonals *d,const float a[3],const float b[3],uint16_t *id)
{
    const float *first=a,*last=b;uint32_t i,k;
    for(k=0;k<3 && a[k]==b[k];k++);
    if(k==3)return RF_FORMAT;
    if(a[k]>b[k]){first=b;last=a;}
    for(i=0;i<d->count;i++)if(!memcmp(d->endpoints[i][0],first,12) && !memcmp(d->endpoints[i][1],last,12))break;
    if(i==d->count) {
        if(i==RF_GEOMOD_WORK_FACES || GEOMOD_DIAGONAL_BASE+i>=UINT16_MAX)return RF_RANGE;
        memcpy(d->endpoints[i][0],first,12);memcpy(d->endpoints[i][1],last,12);d->count++;
    }
    *id=(uint16_t)(GEOMOD_DIAGONAL_BASE+i);return RF_OK;
}
static int diagonal_intersection(const geomod_diagonals *d,uint16_t id,const float plane[4],float out[3])
{
    const float *a,*b;double da=plane[3],db=plane[3],t;uint32_t k,index=id-GEOMOD_DIAGONAL_BASE;
    if(!d || index>=d->count)return RF_FORMAT;
    a=d->endpoints[index][0];b=d->endpoints[index][1];
    for(k=0;k<3;k++){da+=(double)plane[k]*a[k];db+=(double)plane[k]*b[k];}
    if(da==db)return RF_FORMAT;t=da/(da-db);if(!isfinite(t))return RF_FORMAT;
    for(k=0;k<3;k++){out[k]=(float)((1-t)*a[k]+t*b[k]);if(!isfinite(out[k]))return RF_FORMAT;}
    return RF_OK;
}
typedef struct geomod_corner_support {
    const rf_geomod_multi_work *work;
    uint16_t face;
    const rf_geomod_mesh_view *cutters;
    const geomod_diagonals *diagonals;
} geomod_corner_support;
static const float *corner_support_plane(const geomod_corner_support *support,uint16_t id)
{
    uint32_t cutter,local;
    if(id<32)return support->work->source_planes[id];
    cutter=(id-32)/128;local=(id-32)%128;
    return support->work->star_count[cutter]?support->work->star_planes[cutter][local/4][local%4]:support->work->cut_planes[cutter][local];
}
/* Resolve intersections on an actual shared star-cutter edge before using
 * rounded supporting planes. Internal tetrahedron and outer-face supports
 * then agree on the same line. No spatial-proximity matching is involved. */
static int corner_seed_edge(const geomod_corner_support *support,const uint16_t ids[3],float position[3])
{
    uint32_t pair,a,b,other,i,j,k;
    for(pair=0;pair<3;pair++) {
        const float *points[2][3],*shared[2],*plane;uint32_t counts[2],found=0,cutter;
        double da,db,t;const float *first,*last;
        a=pair;b=(pair+1)%3;other=(pair+2)%3;
        if(ids[a]<32 || ids[b]<32 || (ids[a]-32)/128!=(ids[b]-32)/128)continue;
        cutter=(ids[a]-32)/128;
        if(!support->work->star_count[cutter] || (ids[other]>=32 && (ids[other]-32)/128==cutter))continue;
        for(i=0;i<2;i++) {
            uint32_t local=(ids[i?b:a]-32)%128,face=local/4,side=local%4;
            const rf_geomod_vertex *v=support->cutters[cutter].vertices+support->cutters[cutter].faces[face].first;
            counts[i]=3;
            for(j=0;j<3;j++)points[i][j]=(side && j==2)?support->work->star_kernels[cutter]:v[side?(side-1+j)%3:j].position;
        }
        for(i=0;i<counts[0];i++)for(j=0;j<counts[1];j++) {
            for(k=0;k<3 && points[0][i][k]==points[1][j][k];k++);
            if(k==3){if(found<2)shared[found]=points[0][i];found++;}
        }
        if(found!=2)continue;
        /* Opposite versions of one internal plane do not identify an edge. */
        {
            const float *pa=corner_support_plane(support,ids[a]),*pb=corner_support_plane(support,ids[b]);double cross2=0;
            for(k=0;k<3;k++){double cross=(double)pa[(k+1)%3]*pb[(k+2)%3]-(double)pa[(k+2)%3]*pb[(k+1)%3];cross2+=cross*cross;}
            if(cross2<1e-20)continue;
        }
        first=shared[0];last=shared[1];
        for(k=0;k<3 && first[k]==last[k];k++);
        if(k==3)continue;
        if(first[k]>last[k]){const float *swap=first;first=last;last=swap;}
        plane=corner_support_plane(support,ids[other]);da=db=plane[3];
        for(k=0;k<3;k++){da+=(double)plane[k]*first[k];db+=(double)plane[k]*last[k];}
        if(da==db)continue;
        /* Supports define a line; other clipping planes bound the fragment. */
        t=da/(da-db);if(!isfinite(t))continue;
        for(k=0;k<3;k++){position[k]=(float)((1-t)*first[k]+t*last[k]);if(!isfinite(position[k]))return 0;}
        return 1;
    }
    return 0;
}
/* Interpolation can round a crossing onto an existing corner. Discard only
 * zero-area split children; do not introduce a sliver-area tolerance. */
static int split_child_has_area(const rf_geomod_vertex *v,uint32_t count)
{
    double normal[3]={0};uint32_t i,j;
    if(count<3)return 0;
    for(i=1;i+1<count;i++)for(j=0;j<3;j++) {
        uint32_t a=(j+1)%3,b=(j+2)%3;
        normal[j]+=((double)v[i].position[a]-v[0].position[a])*((double)v[i+1].position[b]-v[0].position[b])-
            ((double)v[i].position[b]-v[0].position[b])*((double)v[i+1].position[a]-v[0].position[a]);
    }
    return normal[0]!=0 || normal[1]!=0 || normal[2]!=0;
}

static int polygon_split_edges(const rf_geomod_vertex *vertices,uint32_t count,
    const float plane[4],rf_geomod_vertex *front,uint32_t front_capacity,
    rf_geomod_vertex *back,uint32_t back_capacity,uint32_t *front_count,uint32_t *back_count,
    const uint16_t *edges,uint16_t cut_edge,uint16_t *front_edges,uint16_t *back_edges,const geomod_corner_support *support)
{
    rf_geomod_vertex f[RF_GEOMOD_POLYGON_LIMIT],b[RF_GEOMOD_POLYGON_LIMIT];
    uint16_t fe[RF_GEOMOD_POLYGON_LIMIT],be[RF_GEOMOD_POLYGON_LIMIT];
    double distances[RF_GEOMOD_POLYGON_LIMIT],norm=0;
    int sides[RF_GEOMOD_POLYGON_LIMIT];uint32_t i,j,nf=0,nb=0,positive=0,negative=0;
    if(!vertices || !plane || !front_count || !back_count || front_count==back_count || count<3 || count>RF_GEOMOD_POLYGON_LIMIT)return RF_RANGE;
    for(j=0;j<4;j++)if(!isfinite(plane[j]))return RF_FORMAT;
    for(j=0;j<3;j++)norm+=(double)plane[j]*plane[j];
    if(fabs(norm-1)>1e-4)return RF_FORMAT;
    for(i=0;i<count;i++) {
        double d=plane[3];
        for(j=0;j<3;j++){if(!isfinite(vertices[i].position[j]))return RF_FORMAT;d+=(double)plane[j]*vertices[i].position[j];}
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_FORMAT;
        distances[i]=d;sides[i]=d>1e-5?1:d< -1e-5?-1:0;
        positive+=sides[i]>0;negative+=sides[i]<0;
    }
    if(!negative){memcpy(f,vertices,count*sizeof(*f));if(edges)memcpy(fe,edges,count*sizeof(*fe));nf=count;}
    else if(!positive){memcpy(b,vertices,count*sizeof(*b));if(edges)memcpy(be,edges,count*sizeof(*be));nb=count;}
    else for(i=0;i<count;i++) {
        uint32_t next=(i+1)%count,first=i,last=next;rf_geomod_vertex cut;double t;
        if(sides[i]>=0){if(nf==64)return RF_RANGE;if(edges)fe[nf]=sides[i]==0 && sides[next]<0?cut_edge:edges[i];if(append(f,&nf,vertices+i))return RF_RANGE;}
        if(sides[i]<=0){if(nb==64)return RF_RANGE;if(edges)be[nb]=sides[i]==0 && sides[next]>0?cut_edge:edges[i];if(append(b,&nb,vertices+i))return RF_RANGE;}
        if(sides[i]*sides[next]>=0)continue;
        /* Shared edges occur in opposite polygon directions. Evaluate both
         * from the same endpoint to avoid different cancellation/rounding. */
        for(j=0;j<3 && vertices[first].position[j]==vertices[last].position[j];j++);
        if(j<3 && vertices[first].position[j]>vertices[last].position[j]){first=next;last=i;}
        t=distances[first]/(distances[first]-distances[last]);
        for(j=0;j<3;j++) {
            cut.position[j]=(float)((1-t)*vertices[first].position[j]+t*vertices[last].position[j]);
            if(!isfinite(cut.position[j]))return RF_FORMAT;
        }
        if(support && edges && edges[i]>=GEOMOD_DIAGONAL_BASE && edges[i]!=UINT16_MAX) {
            int status=diagonal_intersection(support->diagonals,edges[i],plane,cut.position);
            if(status)return status;
        } else if(support && edges) {
            float planes[3][4],position[3];
            memcpy(planes[0],corner_support_plane(support,support->face),sizeof(planes[0]));
            memcpy(planes[1],corner_support_plane(support,edges[i]),sizeof(planes[1]));
            memcpy(planes[2],plane,sizeof(planes[2]));
            /* Coplanar supporting faces do not define a unique corner. */
            {uint16_t ids[3]={support->face,edges[i],cut_edge};
             if(corner_seed_edge(support,ids,position) || !rf_geomod_plane_corner(planes,position)) {
                 memcpy(cut.position,position,sizeof(position));
             }}
            /* Enforce exact axial supports even when coincident planes leave
             * interpolation as the fallback. A residue can flip the strict
             * collision approach sign on a mathematically parallel sweep. */
            {uint32_t p,a;
             for(p=0;p<3;p++)for(a=0;a<3;a++)
                 if(planes[p][a]!=0 && planes[p][(a+1)%3]==0 && planes[p][(a+2)%3]==0)
                     cut.position[a]=-planes[p][3]/planes[p][a];}
        }
        for(j=0;j<2;j++) {
            cut.uv[j]=(float)((1-t)*vertices[first].uv[j]+t*vertices[last].uv[j]);
            if(!isfinite(cut.uv[j]))return RF_FORMAT;
        }
        if(nf==64 || nb==64)return RF_RANGE;
        if(edges){fe[nf]=sides[i]>0?cut_edge:edges[i];be[nb]=sides[i]<0?cut_edge:edges[i];}
        if(append(f,&nf,&cut) || append(b,&nb,&cut))return RF_RANGE;
        if(intersection_observer)intersection_observer(intersection_context,plane,
            vertices[first].position,vertices[last].position,cut.position);
    }
    if(!split_child_has_area(f,nf))nf=0;
    if(!split_child_has_area(b,nb))nb=0;
    if((front && front_capacity<nf) || (back && back_capacity<nb))return RF_RANGE;
    if(front && nf)memcpy(front,f,nf*sizeof(*front));
    if(back && nb)memcpy(back,b,nb*sizeof(*back));
    if(edges){if(nf)memcpy(front_edges,fe,nf*sizeof(*fe));if(nb)memcpy(back_edges,be,nb*sizeof(*be));}
    *front_count=nf;*back_count=nb;return RF_OK;
}

int rf_geomod_polygon_split(const rf_geomod_vertex *v,uint32_t n,const float plane[4],
    rf_geomod_vertex *front,uint32_t fc,rf_geomod_vertex *back,uint32_t bc,uint32_t *nf,uint32_t *nb)
{return polygon_split_edges(v,n,plane,front,fc,back,bc,nf,nb,NULL,0,NULL,NULL,NULL);}
int rf_geomod_polygon_split_tracked(const rf_geomod_vertex *v,uint32_t n,const float plane[4],
    const uint16_t *edges,uint16_t cut_edge,rf_geomod_vertex *front,uint16_t *front_edges,uint32_t fc,
    rf_geomod_vertex *back,uint16_t *back_edges,uint32_t bc,uint32_t *nf,uint32_t *nb)
{
    if(!edges || !front || !back || !front_edges || !back_edges || front_edges==back_edges)return RF_RANGE;
    return polygon_split_edges(v,n,plane,front,fc,back,bc,nf,nb,edges,cut_edge,front_edges,back_edges,NULL);
}

static int polygon_subtract_tracked_policy(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count,int boundary_policy,const rf_geomod_edge_tracking *tracking,const geomod_corner_support *support)
{
    rf_geomod_vertex current[64],front[64],back[64];
    uint16_t current_edges[64],front_edges[64],back_edges[64];
    double normal[3]={0},normal_length=0;
    uint32_t pass,i,j,left,nf,nb,total=0,pieces=0,required=0,required_pieces=0;int status;
    if(!vertices || count<3 || count>64 || !planes || !plane_count || plane_count>32 ||
       !vertex_count || !fragment_count || vertex_count==fragment_count || (!!out != !!fragments))return RF_RANGE;
    if(tracking && (!tracking->input || !tracking->planes || (out && !tracking->output)))return RF_RANGE;
    /* Validate even planes beyond an early empty intersection. */
    for(i=0;i<plane_count;i++) {
        double norm=0;
        for(j=0;j<4;j++)if(!isfinite(planes[i][j]))return RF_FORMAT;
        for(j=0;j<3;j++)norm+=(double)planes[i][j]*planes[i][j];
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
    }
    for(i=0;i<count;i++) {
        uint32_t next=(i+1)%count;
        for(j=0;j<3;j++) {
            uint32_t a=(j+1)%3,b=(j+2)%3;
            if(!isfinite(vertices[i].position[j]))return RF_FORMAT;
            normal[j]+=(double)vertices[i].position[a]*vertices[next].position[b]-(double)vertices[i].position[b]*vertices[next].position[a];
        }
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_FORMAT;
    }
    for(j=0;j<3;j++)normal_length+=normal[j]*normal[j];
    if(!isfinite(normal_length) || normal_length<=1e-24)return RF_FORMAT;
    /* A separating plane proves no intersection before other planes can
     * needlessly fragment a distant polygon. Coplanar policy still runs below. */
    for(i=0;i<plane_count;i++) {
        int separated=1,positive=0;
        for(j=0;j<count;j++) {
            uint32_t k;double d=planes[i][3];
            for(k=0;k<3;k++)d+=(double)planes[i][k]*vertices[j].position[k];
            if(d< -1e-5){separated=0;break;}
            if(d>1e-5)positive=1;
        }
        if(separated && positive) {
            if(out && (capacity<count || fragment_capacity<1))return RF_RANGE;
            if(out){memcpy(out,vertices,count*sizeof(*out));fragments[0]=(rf_geomod_fragment){0,count};if(tracking)memcpy(tracking->output,tracking->input,count*sizeof(uint16_t));}
            *vertex_count=count;*fragment_count=1;return RF_OK;
        }
    }
    for(pass=0;pass<(out?2u:1u);pass++) {
        memcpy(current,vertices,count*sizeof(*current));if(tracking)memcpy(current_edges,tracking->input,count*sizeof(uint16_t));left=count;total=pieces=0;
        for(i=0;i<plane_count && left;i++) {
            status=polygon_split_edges(current,left,planes[i],front,64,back,64,&nf,&nb,
                tracking?current_edges:NULL,tracking?tracking->planes[i]:0,front_edges,back_edges,support);
            if(status)return status;
            if(nf && !nb) {
                uint32_t k;int coplanar=1;double alignment=0;
                for(j=0;j<left;j++) {
                    double d=planes[i][3];
                    for(k=0;k<3;k++)d+=(double)planes[i][k]*current[j].position[k];
                    if(fabs(d)>1e-5){coplanar=0;break;}
                }
                for(k=0;k<3;k++)alignment+=normal[k]*planes[i][k];
                /* Source surfaces keep opposite-facing contact. Union caps
                 * remove internal contact and assign coincident outer caps
                 * to one owner: policy1 removes both, policy2 keeps same-facing. */
                if(coplanar && (boundary_policy==1 || (boundary_policy==2?alignment<0:alignment>0))) {
                    nf=0;nb=left;memcpy(back,current,left*sizeof(*back));if(tracking)memcpy(back_edges,current_edges,left*sizeof(uint16_t));
                }
            }
            if(nf) {
                if(pass) {
                    memcpy(out+total,front,nf*sizeof(*out));if(tracking)memcpy(tracking->output+total,front_edges,nf*sizeof(uint16_t));
                    fragments[pieces].first=total;fragments[pieces].count=nf;
                }
                total+=nf;pieces++;
            }
            memcpy(current,back,nb*sizeof(*current));if(tracking)memcpy(current_edges,back_edges,nb*sizeof(uint16_t));left=nb;
        }
        if(!pass) {
            required=total;required_pieces=pieces;
            if(out && (capacity<total || fragment_capacity<pieces))return RF_RANGE;
        }
    }
    *vertex_count=required;*fragment_count=required_pieces;return RF_OK;
}

static int polygon_subtract_policy(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count,int policy)
{return polygon_subtract_tracked_policy(vertices,count,planes,plane_count,out,capacity,fragments,fragment_capacity,vertex_count,fragment_count,policy,NULL,NULL);}
int rf_geomod_polygon_subtract_tracked(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count,
    const rf_geomod_edge_tracking *tracking)
{
    if(!tracking)return RF_RANGE;
    return polygon_subtract_tracked_policy(vertices,count,planes,plane_count,out,capacity,fragments,fragment_capacity,vertex_count,fragment_count,0,tracking,NULL);
}

int rf_geomod_polygon_subtract(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,uint32_t capacity,
    rf_geomod_fragment *fragments,uint32_t fragment_capacity,uint32_t *vertex_count,uint32_t *fragment_count)
{
    return polygon_subtract_policy(vertices,count,planes,plane_count,out,capacity,
        fragments,fragment_capacity,vertex_count,fragment_count,0);
}

int rf_geomod_interior_face(const rf_geomod_vertex *vertices,uint32_t count,
    const float (*planes)[4],uint32_t plane_count,rf_geomod_vertex *out,
    uint32_t capacity,uint32_t *out_count)
{
    rf_geomod_vertex current[64],front[64],back[64];uint32_t i,j,k,left=count,nf,nb;int status;
    if(!vertices || count<3 || count>64 || !planes || !plane_count || plane_count>32 || !out_count)return RF_RANGE;
    for(i=0;i<plane_count;i++) {
        double norm=0;
        for(j=0;j<4;j++)if(!isfinite(planes[i][j]))return RF_FORMAT;
        for(j=0;j<3;j++)norm+=(double)planes[i][j]*planes[i][j];
        if(fabs(norm-1)>1e-4)return RF_FORMAT;
    }
    for(i=0;i<count;i++) {
        for(j=0;j<3;j++)if(!isfinite(vertices[i].position[j]))return RF_FORMAT;
        for(j=0;j<2;j++)if(!isfinite(vertices[i].uv[j]))return RF_FORMAT;
    }
    memcpy(current,vertices,count*sizeof(*current));
    for(i=0;i<plane_count && left;i++) {
        int on_boundary=1;
        for(j=0;j<left;j++) {
            double d=planes[i][3];
            for(k=0;k<3;k++)d+=(double)planes[i][k]*current[j].position[k];
            if(fabs(d)>1e-5){on_boundary=0;break;}
        }
        if(on_boundary){left=0;break;}
        status=rf_geomod_polygon_split(current,left,planes[i],front,64,back,64,&nf,&nb);
        if(status)return status;
        left=nb;memcpy(current,back,nb*sizeof(*current));
    }
    if(out && capacity<left)return RF_RANGE;
    if(out)for(i=0;i<left;i++)out[i]=current[left-1-i];
    *out_count=left;return RF_OK;
}

struct rf_geomod_storage {
    rf_geomod_vertex *vertices[3];rf_geomod_face *faces[3];
    uint32_t nv[3],nf[3],vertex_capacity,face_capacity,bytes,current,editing,generation;
};
static int storage_vertices(const rf_geomod_vertex *v,uint32_t n)
{
    uint32_t i,j;if(n && !v)return RF_RANGE;
    for(i=0;i<n;i++) {
        for(j=0;j<3;j++)if(!isfinite(v[i].position[j]))return RF_FORMAT;
        for(j=0;j<2;j++)if(!isfinite(v[i].uv[j]))return RF_FORMAT;
    }
    return RF_OK;
}
int rf_geomod_storage_open(const rf_geomod_mesh_view *source,uint32_t vc,uint32_t fc,
    uint32_t budget,rf_geomod_storage **out)
{
    rf_geomod_storage *s;unsigned char *p;uint64_t bytes;uint32_t i;int status;
    if(!source || !out || *out || !vc || !fc || source->vertex_count>vc || source->face_count>fc ||
       (source->face_count && !source->faces))return RF_RANGE;
    status=storage_vertices(source->vertices,source->vertex_count);if(status)return status;
    for(i=0;i<source->face_count;i++) {
        const rf_geomod_face *f=source->faces+i;
        if(f->count<3 || f->count>64 || f->first>source->vertex_count || f->count>source->vertex_count-f->first)return RF_FORMAT;
    }
    bytes=sizeof(*s)+(2ull*vc+source->vertex_count)*sizeof(rf_geomod_vertex)+(2ull*fc+source->face_count)*sizeof(rf_geomod_face);
    if(bytes>budget || bytes>UINT32_MAX)return RF_RANGE;
    s=calloc(1,(size_t)bytes);if(!s)return RF_IO;
    s->bytes=(uint32_t)bytes;s->vertex_capacity=vc;s->face_capacity=fc;s->generation=1;p=(unsigned char *)(s+1);
    for(i=0;i<3;i++) {
        s->vertices[i]=(rf_geomod_vertex *)p;p+=(i==2?source->vertex_count:vc)*sizeof(rf_geomod_vertex);
        s->faces[i]=(rf_geomod_face *)p;p+=(i==2?source->face_count:fc)*sizeof(rf_geomod_face);
        if(i!=1) {
            if(source->vertex_count)memcpy(s->vertices[i],source->vertices,source->vertex_count*sizeof(rf_geomod_vertex));
            if(source->face_count)memcpy(s->faces[i],source->faces,source->face_count*sizeof(rf_geomod_face));
            s->nv[i]=source->vertex_count;s->nf[i]=source->face_count;
        }
    }
    *out=s;return RF_OK;
}
void rf_geomod_storage_close(rf_geomod_storage **s)
{if(s){free(*s);*s=NULL;}}
uint32_t rf_geomod_storage_bytes(const rf_geomod_storage *s)
{return s?s->bytes:0;}
int rf_geomod_storage_view(const rf_geomod_storage *s,rf_geomod_mesh_view *out)
{
    if(!s || !out)return RF_RANGE;
    out->vertices=s->vertices[s->current];out->faces=s->faces[s->current];
    out->vertex_count=s->nv[s->current];out->face_count=s->nf[s->current];out->generation=s->generation;return RF_OK;
}
int rf_geomod_storage_begin(rf_geomod_storage *s)
{
    uint32_t next;if(!s || s->editing || s->generation==UINT32_MAX)return RF_RANGE;
    next=s->current^1;s->nv[next]=s->nf[next]=0;s->editing=1;return RF_OK;
}
int rf_geomod_storage_pending(const rf_geomod_storage *s,rf_geomod_mesh_view *out)
{
    uint32_t next;if(!s || !s->editing || !out)return RF_RANGE;next=s->current^1;
    out->vertices=s->vertices[next];out->faces=s->faces[next];out->vertex_count=s->nv[next];
    out->face_count=s->nf[next];out->generation=s->generation+1;return RF_OK;
}
int rf_geomod_storage_append(rf_geomod_storage *s,const rf_geomod_vertex *v,uint32_t n,uint32_t material,uint32_t source_face)
{
    uint32_t next;rf_geomod_face face;int status;
    if(!s || !s->editing || n<3 || n>64)return RF_RANGE;
    next=s->current^1;
    if(n>s->vertex_capacity-s->nv[next] || s->nf[next]==s->face_capacity)return RF_RANGE;
    status=storage_vertices(v,n);if(status)return status;
    face.first=s->nv[next];face.count=n;face.material=material;face.source_face=source_face;
    memcpy(s->vertices[next]+face.first,v,n*sizeof(*v));s->faces[next][s->nf[next]++]=face;s->nv[next]+=n;return RF_OK;
}
int rf_geomod_storage_commit(rf_geomod_storage *s)
{
    if(!s || !s->editing || s->generation==UINT32_MAX)return RF_RANGE;
    s->current^=1;s->generation++;s->editing=0;return RF_OK;
}
void rf_geomod_storage_abort(rf_geomod_storage *s)
{if(s)s->editing=0;}
int rf_geomod_storage_reset(rf_geomod_storage *s)
{
    uint32_t next;if(!s || s->editing || s->generation==UINT32_MAX)return RF_RANGE;
    next=s->current^1;
    if(s->nv[2])memcpy(s->vertices[next],s->vertices[2],s->nv[2]*sizeof(rf_geomod_vertex));
    if(s->nf[2])memcpy(s->faces[next],s->faces[2],s->nf[2]*sizeof(rf_geomod_face));
    s->nv[next]=s->nv[2];s->nf[next]=s->nf[2];s->current=next;s->generation++;return RF_OK;
}

static int convex_mesh_planes_oriented(const rf_geomod_mesh_view *mesh,float planes[32][4],int inward)
{
    uint32_t i,j,k;int status;
    if(!mesh || mesh->face_count<4 || mesh->face_count>32 || !mesh->faces)return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *face=mesh->faces+i;double normal[3]={0},length=0,offset=0;
        if(face->count<3 || face->count>64 || face->first>mesh->vertex_count || face->count>mesh->vertex_count-face->first)return RF_FORMAT;
        for(j=0;j<face->count;j++) {
            const float *a=mesh->vertices[face->first+j].position,*b=mesh->vertices[face->first+(j+1)%face->count].position;
            for(k=0;k<3;k++)normal[k]+=(double)a[(k+1)%3]*b[(k+2)%3]-(double)a[(k+2)%3]*b[(k+1)%3];
        }
        for(k=0;k<3;k++)length+=normal[k]*normal[k];
        if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
        length=sqrt(length);
        for(k=0;k<3;k++){planes[i][k]=(float)(normal[k]/length)*(inward?-1.f:1.f);offset-=(double)planes[i][k]*mesh->vertices[face->first].position[k];}
        planes[i][3]=(float)offset;if(!isfinite(planes[i][3]))return RF_FORMAT;
        for(j=0;j<mesh->vertex_count;j++) {
            double distance=planes[i][3];for(k=0;k<3;k++)distance+=(double)planes[i][k]*mesh->vertices[j].position[k];
            if(distance>1e-5 || (j>=face->first && j<face->first+face->count && fabs(distance)>1e-5))return RF_FORMAT;
        }
    }
    return RF_OK;
}
static int convex_mesh_planes(const rf_geomod_mesh_view *mesh,float planes[32][4])
{return convex_mesh_planes_oriented(mesh,planes,0);}
int rf_geomod_storage_prepare_convex_cut(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutter,rf_geomod_cut_work *work)
{
    rf_geomod_mesh_view source;float source_planes[32][4],cut_planes[32][4];uint32_t i,j,n,pieces;int status;
    if(!s || !cutter || !work || s->editing)return RF_RANGE;
    rf_geomod_storage_view(s,&source);
    status=convex_mesh_planes(&source,source_planes);if(status)return status;
    status=convex_mesh_planes(cutter,cut_planes);if(status)return status;
    status=rf_geomod_storage_begin(s);if(status)return status;
    for(i=0;i<source.face_count;i++) {
        const rf_geomod_face *face=source.faces+i;
        status=rf_geomod_polygon_subtract(source.vertices+face->first,face->count,cut_planes,cutter->face_count,
            work->vertices,64*32,work->fragments,32,&n,&pieces);if(status)goto failed;
        for(j=0;j<pieces;j++) {
            const rf_geomod_fragment *f=work->fragments+j;
            status=rf_geomod_storage_append(s,work->vertices+f->first,f->count,face->material,face->source_face);if(status)goto failed;
        }
    }
    for(i=0;i<cutter->face_count;i++) {
        const rf_geomod_face *face=cutter->faces+i;
        status=rf_geomod_interior_face(cutter->vertices+face->first,face->count,source_planes,source.face_count,work->vertices,64*32,&n);if(status)goto failed;
        if(n){status=rf_geomod_storage_append(s,work->vertices,n,face->material,UINT32_MAX);if(status)goto failed;}
    }
    return RF_OK;
failed:
    rf_geomod_storage_abort(s);return status;
}

static void reverse_vertices(rf_geomod_vertex *vertices,uint32_t count)
{
    uint32_t i;for(i=0;i<count/2;i++) {
        rf_geomod_vertex v=vertices[i];vertices[i]=vertices[count-1-i];vertices[count-1-i]=v;
    }
}

static int collision_mesh_face(const rf_geomod_mesh_view *,uint32_t,const rf_collision_face_filter *,rf_collision_face *);

/* Join only convex, coplanar neighbors with one common reversed edge and a
 * common affine UV field. This removes partition seams without smoothing the
 * crater shape or changing material interpolation. */
static int join_polygons(const rf_geomod_vertex *a,uint32_t na,const rf_geomod_vertex *b,uint32_t nb,
    rf_geomod_vertex out[64],uint32_t *count,const uint16_t *ae,const uint16_t *be,uint16_t *oe)
{
    uint32_t i,j,k,c,edge_a=UINT32_MAX,edge_b=0,n=na+nb-2;
    double u[3],v[3],normal[3],uu=0,vv=0,uv=0,den,length;
    if(n>64)return 0;
    for(i=0;i<na && edge_a==UINT32_MAX;i++)for(j=0;j<nb;j++) {
        int same=1;
        for(k=0;k<3;k++)if(fabs((double)a[i].position[k]-b[(j+1)%nb].position[k])>1e-6 ||
            fabs((double)a[(i+1)%na].position[k]-b[j].position[k])>1e-6){same=0;break;}
        if(same){edge_a=i;edge_b=j;break;}
    }
    if(edge_a==UINT32_MAX)return 0;
    /* Find a nondegenerate basis even when clipping retained collinear corners. */
    for(i=1;i+1<na;i++) {
        uu=vv=uv=0;
        for(k=0;k<3;k++){u[k]=(double)a[i].position[k]-a[0].position[k];v[k]=(double)a[i+1].position[k]-a[0].position[k];uu+=u[k]*u[k];vv+=v[k]*v[k];uv+=u[k]*v[k];}
        den=uu*vv-uv*uv;if(den>1e-16)break;
    }
    if(i+1==na)return 0;
    for(k=0;k<3;k++)normal[k]=u[(k+1)%3]*v[(k+2)%3]-u[(k+2)%3]*v[(k+1)%3];
    length=sqrt(den);for(k=0;k<3;k++)normal[k]/=length;
    for(j=0;j<na+nb;j++) {
        const rf_geomod_vertex *p=j<na?a+j:b+j-na;double du=0,dv=0,dn=0,s,t;
        for(k=0;k<3;k++){double d=(double)p->position[k]-a[0].position[k];du+=d*u[k];dv+=d*v[k];dn+=d*normal[k];}
        if(fabs(dn)>1e-5)return 0;
        s=(du*vv-dv*uv)/den;t=(dv*uu-du*uv)/den;
        for(c=0;c<2;c++)if(fabs(a[0].uv[c]+s*((double)a[i].uv[c]-a[0].uv[c])+t*((double)a[i+1].uv[c]-a[0].uv[c])-p->uv[c])>1e-5)return 0;
    }
    for(j=0;j<na;j++)out[j]=a[(edge_a+1+j)%na];
    for(j=0;j<nb-2;j++)out[na+j]=b[(edge_b+2+j)%nb];
    /* Require the complete merged polygon to be convex, not just its seam. */
    for(j=0;j<n;j++)for(c=0;c<n;c++) {
        double cross=0;
        for(k=0;k<3;k++) {
            uint32_t x=(k+1)%3,y=(k+2)%3;
            cross+=normal[k]*(((double)out[(j+1)%n].position[x]-out[j].position[x])*((double)out[c].position[y]-out[j].position[y])-
                ((double)out[(j+1)%n].position[y]-out[j].position[y])*((double)out[c].position[x]-out[j].position[x]));
        }
        if(cross < -1e-7)return 0;
    }
    {
        rf_geomod_face face={0,n,0,UINT32_MAX};rf_geomod_mesh_view mesh={out,&face,n,1,0};
        rf_collision_face_filter filter={0};rf_collision_face bound;
        if(collision_mesh_face(&mesh,0,&filter,&bound))return 0;
    }
    if(ae) {
        for(j=0;j<na;j++)oe[j]=ae[(edge_a+1+j)%na];
        oe[na-1]=be[(edge_b+1)%nb];
        for(j=0;j<nb-2;j++)oe[na+j]=be[(edge_b+2+j)%nb];
    }
    *count=n;return 1;
}
static void compact_bounds(const rf_geomod_vertex *v,uint32_t count,float bounds[6])
{
    uint32_t i,j;for(j=0;j<3;j++)bounds[j]=bounds[j+3]=v[0].position[j];
    for(i=1;i<count;i++)for(j=0;j<3;j++) {
        if(v[i].position[j]<bounds[j])bounds[j]=v[i].position[j];
        if(v[i].position[j]>bounds[j+3])bounds[j+3]=v[i].position[j];
    }
}
static int compact_separated(const float a[6],const float b[6])
{
    uint32_t j;for(j=0;j<3;j++)if((double)a[j]-b[j+3]>1e-6 || (double)b[j]-a[j+3]>1e-6)return 1;
    return 0;
}
/* Transient chronological-rebuild tags. Caller owns this optional scratch;
 * legacy full-union rebuilds allocate none. Never encode tags as source_face:
 * that field identifies original material/collision ownership. */
typedef struct geomod_face_lineage {
    uint8_t pending[RF_GEOMOD_WORK_FACES],repaired[RF_GEOMOD_WORK_FACES];
    geomod_diagonals *diagonals;
} geomod_face_lineage;
/* Previous chronological bank's exact clipping-plane ownership. */
typedef struct geomod_step_support {
    uint16_t edges[RF_GEOMOD_WORK_VERTICES],planes[RF_GEOMOD_WORK_FACES];
    geomod_diagonals diagonals;
} geomod_step_support;
static int repair_cavity_pending_provenance(rf_geomod_storage *,rf_geomod_multi_work *,geomod_face_lineage *,geomod_step_support *);

static int append_compact_lineage(rf_geomod_storage *s,const rf_geomod_vertex *v,uint32_t n,uint32_t material,uint32_t source_face,rf_geomod_multi_work *work,const uint16_t *edges,uint16_t plane_id,geomod_face_lineage *lineage,uint8_t birth)
{
    rf_geomod_vertex *polygon=work->split.vertices,*joined=polygon+64;uint32_t bank=s->current^1,i=0,j,count;
    float bounds[6];uint16_t polygon_edges[64],joined_edges[64];int cached=s->face_capacity<=800,status;
    if(lineage && s->face_capacity>RF_GEOMOD_WORK_FACES)return RF_RANGE;
    if(s->vertex_capacity>RF_GEOMOD_WORK_VERTICES || s->face_capacity>RF_GEOMOD_WORK_FACES)edges=NULL;
    if(n>64)return RF_RANGE;memcpy(polygon,v,n*sizeof(*v));
    if(edges){if(s->nv[bank]>RF_GEOMOD_WORK_VERTICES || s->nf[bank]>sizeof(work->compact_planes)/sizeof(work->compact_planes[0]))return RF_RANGE;memcpy(polygon_edges,edges,n*sizeof(*edges));}
    if(cached)compact_bounds(polygon,n,bounds);
    while(i<s->nf[bank]) {
        rf_geomod_face f=s->faces[bank][i];
        if(f.material!=material || f.source_face!=source_face ||
           (lineage && (lineage->pending[i]!=birth || (edges && work->compact_planes[i]!=plane_id))) ||
           (cached && compact_separated(bounds,work->compact_bounds[i])) ||
           !join_polygons(polygon,n,s->vertices[bank]+f.first,f.count,joined,&count,edges?polygon_edges:NULL,edges?work->compact_edges+f.first:NULL,joined_edges)){i++;continue;}
        if(edges) {
            if(plane_id!=work->compact_planes[i])plane_id=UINT16_MAX;
            memmove(work->compact_edges+f.first,work->compact_edges+f.first+f.count,(s->nv[bank]-f.first-f.count)*sizeof(uint16_t));
            memmove(work->compact_planes+i,work->compact_planes+i+1,(s->nf[bank]-i-1)*sizeof(uint16_t));
            memcpy(polygon_edges,joined_edges,count*sizeof(uint16_t));
        }
        memmove(s->vertices[bank]+f.first,s->vertices[bank]+f.first+f.count,
            (s->nv[bank]-f.first-f.count)*sizeof(*v));s->nv[bank]-=f.count;
        if(cached)memmove(work->compact_bounds+i,work->compact_bounds+i+1,(s->nf[bank]-i-1)*sizeof(work->compact_bounds[0]));
        if(lineage)memmove(lineage->pending+i,lineage->pending+i+1,s->nf[bank]-i-1);
        memmove(s->faces[bank]+i,s->faces[bank]+i+1,(s->nf[bank]-i-1)*sizeof(f));s->nf[bank]--;
        for(j=i;j<s->nf[bank];j++)s->faces[bank][j].first-=f.count;
        memcpy(polygon,joined,count*sizeof(*v));n=count;i=0;
        if(cached)compact_bounds(polygon,n,bounds);
    }
    if(edges && (s->nv[bank]>RF_GEOMOD_WORK_VERTICES-n || s->nf[bank]>=sizeof(work->compact_planes)/sizeof(work->compact_planes[0])))return RF_RANGE;
    status=rf_geomod_storage_append(s,polygon,n,material,source_face);
    if(!status && edges){memcpy(work->compact_edges+s->nv[bank]-n,polygon_edges,n*sizeof(uint16_t));work->compact_planes[s->nf[bank]-1]=plane_id;}
    if(!status && cached)memcpy(work->compact_bounds[s->nf[bank]-1],bounds,sizeof(bounds));
    if(!status && lineage)lineage->pending[s->nf[bank]-1]=birth;
    return status;
}
static int mesh_polygon_bounds_separated(const rf_geomod_mesh_view *mesh,const rf_geomod_vertex *v,uint32_t count)
{
    uint32_t axis,i;
    for(axis=0;axis<3;axis++) {
        float lo=v[0].position[axis],hi=lo,cut_lo=mesh->vertices[0].position[axis],cut_hi=cut_lo;
        for(i=1;i<count;i++){lo=fminf(lo,v[i].position[axis]);hi=fmaxf(hi,v[i].position[axis]);}
        for(i=1;i<mesh->vertex_count;i++){cut_lo=fminf(cut_lo,mesh->vertices[i].position[axis]);cut_hi=fmaxf(cut_hi,mesh->vertices[i].position[axis]);}
        if((double)lo-cut_hi>1e-5 || (double)cut_lo-hi>1e-5)return 1;
    }
    return 0;
}
/* Process one outward face through the cutter union. owner==UINT32_MAX is
 * original terrain; otherwise this is an outward cutter face, reversed only
 * after all exclusions, so the boundary policy sees the cutter's true normal. */
static int subtract_history_face_range(rf_geomod_storage *s,const rf_geomod_vertex *vertices,
    uint32_t count,uint32_t material,uint32_t source_face,uint32_t owner,
    const rf_geomod_mesh_view *cutters,uint32_t cutter_count,rf_geomod_multi_work *work,const uint16_t *edges,uint16_t face_id,uint32_t first_cutter,geomod_face_lineage *lineage,uint8_t birth)
{
    uint32_t bank=0,pieces=1,c,i,j;int status;
    geomod_corner_support support={work,face_id,cutters,lineage?lineage->diagonals:NULL};
    memcpy(work->vertices[0],vertices,count*sizeof(*vertices));
    if(edges)memcpy(work->edges[0],edges,count*sizeof(*edges));
    work->fragments[0][0]=(rf_geomod_fragment){0,count};
    for(c=first_cutter;c<cutter_count && pieces;c++) {
        uint32_t part,parts=work->star_count[c]?work->star_count[c]:1;
        int policy=owner==UINT32_MAX?0:c<owner?1:2;
        if(c==owner || mesh_polygon_bounds_separated(cutters+c,vertices,count))continue;
        for(part=0;part<parts && pieces;part++) {
            uint32_t next=bank^1,total=0,next_pieces=0;
            const float (*planes)[4]=work->star_count[c]?work->star_planes[c][part]:work->cut_planes[c];
            uint32_t plane_count=work->star_count[c]?4:cutters[c].face_count;
            uint16_t plane_ids[32];
            if(edges)for(j=0;j<plane_count;j++)plane_ids[j]=(uint16_t)(32+c*128+(work->star_count[c]?part*4+j:j));
            for(i=0;i<pieces;i++) {
                const rf_geomod_fragment *face=work->fragments[bank]+i;uint32_t n,nf;
                rf_geomod_edge_tracking tracking={work->edges[bank]+face->first,plane_ids,work->split_edges};
                status=polygon_subtract_tracked_policy(work->vertices[bank]+face->first,face->count,
                    planes,plane_count,work->split.vertices,64*32,
                    work->split.fragments,32,&n,&nf,policy,edges?&tracking:NULL,edges?&support:NULL);if(status)return status;
                if(n>RF_GEOMOD_WORK_VERTICES-total || nf>RF_GEOMOD_WORK_FRAGMENTS-next_pieces)return RF_RANGE;
                memcpy(work->vertices[next]+total,work->split.vertices,n*sizeof(*vertices));
                if(edges)memcpy(work->edges[next]+total,work->split_edges,n*sizeof(uint16_t));
                for(j=0;j<nf;j++) {
                    rf_geomod_fragment f=work->split.fragments[j];f.first+=total;
                    work->fragments[next][next_pieces++]=f;
                }
                total+=n;
            }
            bank=next;pieces=next_pieces;
        }
    }
    for(i=0;i<pieces;i++) {
        const rf_geomod_fragment *f=work->fragments[bank]+i;
        rf_geomod_vertex *v=work->vertices[bank]+f->first;
        if(owner!=UINT32_MAX) {
            reverse_vertices(v,f->count);
            if(edges) {
                uint16_t saved[64];uint32_t k;
                memcpy(saved,work->edges[bank]+f->first,f->count*sizeof(uint16_t));
                for(k=0;k<f->count;k++)work->edges[bank][f->first+k]=saved[(2*f->count-2-k)%f->count];
            }
        }
        status=append_compact_lineage(s,v,f->count,material,source_face,work,edges?work->edges[bank]+f->first:NULL,face_id,lineage,birth);if(status)return status;
    }
    return RF_OK;
}

static int subtract_history_face(rf_geomod_storage *s,const rf_geomod_vertex *vertices,
    uint32_t count,uint32_t material,uint32_t source_face,uint32_t owner,
    const rf_geomod_mesh_view *cutters,uint32_t cutter_count,rf_geomod_multi_work *work,const uint16_t *edges,uint16_t face_id)
{return subtract_history_face_range(s,vertices,count,material,source_face,owner,cutters,cutter_count,work,edges,face_id,0,NULL,0);}

/* One chronological solid/cavity step in a PRIVATE replay owner. Previous
 * committed surfaces retain interpolated UV; only the newest cutter creates
 * birth-tag1 surfaces. Plane caches for the entire prefix are caller prepared.
 * Mapping and commit follow separately; exact support IDs survive each step. */
static inline int prepare_chronological_step(rf_geomod_storage *s,const rf_geomod_mesh_view *cutters,
    uint32_t count,rf_geomod_multi_work *work,geomod_face_lineage *lineage,geomod_step_support *previous,uint32_t cavity)
{
    rf_geomod_mesh_view old,source;uint32_t i,n,c,j,k;int status;
    if(!s || !work || !lineage || !previous || !cutters || !count || count>RF_GEOMOD_CUT_LIMIT || s->editing || s->vertex_capacity>RF_GEOMOD_WORK_VERTICES || s->face_capacity>RF_GEOMOD_WORK_FACES || cavity>1)return RF_RANGE;
    lineage->diagonals=&previous->diagonals;if(count==1)previous->diagonals.count=0;
    source=(rf_geomod_mesh_view){s->vertices[2],s->faces[2],s->nv[2],s->nf[2],0};
    status=convex_mesh_planes_oriented(&source,work->source_planes,(int)cavity);if(status)return status;
    status=rf_geomod_storage_view(s,&old);if(status)return status;
    c=count-1;
    if(!c) {
        status=rf_geomod_seed_adjacency(&source,previous->edges,RF_GEOMOD_WORK_VERTICES);if(status)return status;
        for(i=0;i<source.face_count;i++)previous->planes[i]=(uint16_t)i;
    } else {
        memcpy(previous->edges,work->compact_edges,old.vertex_count*sizeof(uint16_t));
        memcpy(previous->planes,work->compact_planes,old.face_count*sizeof(uint16_t));
    }
    status=rf_geomod_storage_begin(s);if(status)return status;
    for(i=0;i<old.face_count;i++) {
        const rf_geomod_face *f=old.faces+i;
        status=subtract_history_face_range(s,old.vertices+f->first,f->count,f->material,
            f->source_face,UINT32_MAX,cutters,count,work,previous->edges+f->first,previous->planes[i],c,lineage,0);
        if(status)goto failed;
    }
    status=rf_geomod_seed_adjacency(cutters+c,work->initial_edges,64*32);if(status)goto failed;
    for(i=0;i<cutters[c].vertex_count;i++)work->initial_edges[i]=(uint16_t)(32+c*128+work->initial_edges[i]*(work->star_count[c]?4:1));
    for(i=0;i<cutters[c].face_count;i++) {
        const rf_geomod_face *f=cutters[c].faces+i;
        rf_geomod_vertex *current=work->seed.vertices,*front=current+64,*back=current+128;
        uint16_t *current_edges=work->seed_edges,*front_edges=current_edges+64,*back_edges=current_edges+128;
        geomod_corner_support support={work,(uint16_t)(32+c*128+i*(work->star_count[c]?4:1)),cutters,lineage->diagonals};
        if(cavity) {
            uint16_t source_ids[32];uint32_t pieces,part;
            rf_geomod_edge_tracking tracking={work->initial_edges+f->first,source_ids,work->seed_edges};
            for(j=0;j<source.face_count;j++)source_ids[j]=(uint16_t)j;
            status=polygon_subtract_tracked_policy(cutters[c].vertices+f->first,f->count,
                work->source_planes,source.face_count,work->seed.vertices,64*32,
                work->seed.fragments,32,&n,&pieces,1,&tracking,&support);if(status)goto failed;
            for(part=0;part<pieces;part++) {
                const rf_geomod_fragment *piece=work->seed.fragments+part;
                status=subtract_history_face_range(s,work->seed.vertices+piece->first,piece->count,
                    f->material,UINT32_MAX,c,cutters,count,work,work->seed_edges+piece->first,support.face,0,lineage,1);
                if(status)goto failed;
            }
            continue;
        }
        n=f->count;memcpy(current,cutters[c].vertices+f->first,n*sizeof(*current));
        memcpy(current_edges,work->initial_edges+f->first,n*sizeof(*current_edges));
        for(j=0;j<source.face_count && n;j++) {
            uint32_t v,nf,nb;int boundary=1;
            for(v=0;v<n;v++) {
                double distance=work->source_planes[j][3];
                for(k=0;k<3;k++)distance+=(double)work->source_planes[j][k]*current[v].position[k];
                if(fabs(distance)>1e-5){boundary=0;break;}
            }
            if(boundary){n=0;break;}
            status=polygon_split_edges(current,n,work->source_planes[j],front,64,back,64,&nf,&nb,
                current_edges,(uint16_t)j,front_edges,back_edges,&support);if(status)goto failed;
            n=nb;memcpy(current,back,n*sizeof(*current));memcpy(current_edges,back_edges,n*sizeof(*current_edges));
        }
        if(!n)continue;
        status=subtract_history_face_range(s,current,n,f->material,UINT32_MAX,
            c,cutters,count,work,current_edges,support.face,0,lineage,1);
        if(status)goto failed;
    }
    if(cavity){status=repair_cavity_pending_provenance(s,work,lineage,previous);if(status)goto failed;}
    return RF_OK;
failed:
    rf_geomod_storage_abort(s);return status;
}

static int prepare_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work,int prepared)
{
    rf_geomod_mesh_view source;uint32_t i,c,n;int status;
    if(!s || !work || s->editing || count>RF_GEOMOD_CUT_LIMIT || (count && !cutters))return RF_RANGE;
    source=(rf_geomod_mesh_view){s->vertices[2],s->faces[2],s->nv[2],s->nf[2],0};
    status=convex_mesh_planes(&source,work->source_planes);if(status)return status;
    /* Validate the entire history before creating an edit, including cutters
     * obscured by previous cuts. Failed input must not silently change history. */
    if(!prepared)for(c=0;c<count;c++){work->star_count[c]=0;status=convex_mesh_planes(cutters+c,work->cut_planes[c]);if(status)return status;}
    status=rf_geomod_storage_begin(s);if(status)return status;
    for(i=0;i<source.face_count;i++) {
        const rf_geomod_face *f=source.faces+i;
        status=subtract_history_face(s,source.vertices+f->first,f->count,f->material,
            f->source_face,UINT32_MAX,cutters,count,work,NULL,0);if(status)goto failed;
    }
    for(c=0;c<count;c++)for(i=0;i<cutters[c].face_count;i++) {
        const rf_geomod_face *f=cutters[c].faces+i;
        status=rf_geomod_interior_face(cutters[c].vertices+f->first,f->count,
            work->source_planes,source.face_count,work->split.vertices,64*32,&n);if(status)goto failed;
        if(!n)continue;
        reverse_vertices(work->split.vertices,n);
        status=subtract_history_face(s,work->split.vertices,n,f->material,UINT32_MAX,
            c,cutters,count,work,NULL,0);if(status)goto failed;
    }
    return RF_OK;
failed:
    rf_geomod_storage_abort(s);return status;
}

static int repair_cavity_pending(rf_geomod_storage *,rf_geomod_multi_work *);
static int prepare_cavity_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work,int prepared)
{
    rf_geomod_mesh_view source;uint32_t i,c,j,n,pieces;int status;
    /* IDs0..31 name source planes; each cutter owns128 IDs, four per star part. */
    uint16_t source_ids[32];
    if(!s || !work || s->editing || count>RF_GEOMOD_CUT_LIMIT || (count && !cutters))return RF_RANGE;
    source=(rf_geomod_mesh_view){s->vertices[2],s->faces[2],s->nv[2],s->nf[2],0};
    status=convex_mesh_planes_oriented(&source,work->source_planes,1);if(status)return status;
    if(!prepared)for(c=0;c<count;c++){work->star_count[c]=0;status=convex_mesh_planes(cutters+c,work->cut_planes[c]);if(status)return status;}
    status=rf_geomod_seed_adjacency(&source,work->initial_edges,64*32);if(status)return status;
    for(i=0;i<source.face_count;i++)source_ids[i]=(uint16_t)i;
    status=rf_geomod_storage_begin(s);if(status)return status;
    for(i=0;i<source.face_count;i++) {
        const rf_geomod_face *f=source.faces+i;
        status=subtract_history_face(s,source.vertices+f->first,f->count,f->material,
            f->source_face,UINT32_MAX,cutters,count,work,work->initial_edges+f->first,(uint16_t)i);if(status)goto failed;
    }
    for(c=0;c<count;c++) {
        status=rf_geomod_seed_adjacency(cutters+c,work->initial_edges,64*32);if(status)goto failed;
        for(i=0;i<cutters[c].vertex_count;i++)work->initial_edges[i]=(uint16_t)(32+c*128+work->initial_edges[i]*(work->star_count[c]?4:1));
        for(i=0;i<cutters[c].face_count;i++) {
            const rf_geomod_face *f=cutters[c].faces+i;
            /* Keep cutter boundaries outside the original empty room. Contact
             * between cavity and cutter is internal, for either plane orientation. */
            rf_geomod_edge_tracking tracking={work->initial_edges+f->first,source_ids,work->seed_edges};
            geomod_corner_support support={work,(uint16_t)(32+c*128+i*(work->star_count[c]?4:1)),cutters,NULL};
            status=polygon_subtract_tracked_policy(cutters[c].vertices+f->first,f->count,
                work->source_planes,source.face_count,work->seed.vertices,64*32,
                work->seed.fragments,32,&n,&pieces,1,&tracking,&support);if(status)goto failed;
            for(j=0;j<pieces;j++) {
                const rf_geomod_fragment *part=work->seed.fragments+j;
                status=subtract_history_face(s,work->seed.vertices+part->first,part->count,
                    f->material,UINT32_MAX,c,cutters,count,work,work->seed_edges+part->first,support.face);if(status)goto failed;
            }
        }
    }
    if(compaction_observer && s->vertex_capacity<=RF_GEOMOD_WORK_VERTICES && s->face_capacity<=RF_GEOMOD_WORK_FACES) {
        rf_geomod_mesh_view pending;status=rf_geomod_storage_pending(s,&pending);if(status)goto failed;
        compaction_observer(compaction_context,&pending,work->compact_planes,work->compact_edges);
    }
    if(count && s->vertex_capacity<=RF_GEOMOD_WORK_VERTICES && s->face_capacity<=RF_GEOMOD_WORK_FACES) {
        status=repair_cavity_pending(s,work);if(status)goto failed;
    }
    return RF_OK;
failed:
    rf_geomod_storage_abort(s);return status;
}

int rf_geomod_storage_prepare_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work)
{return prepare_cuts(s,cutters,count,work,0);}
int rf_geomod_storage_prepare_cavity_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,uint32_t count,rf_geomod_multi_work *work)
{return prepare_cavity_cuts(s,cutters,count,work,0);}

static int same_position(const float a[3],const float b[3])
{return a[0]==b[0] && a[1]==b[1] && a[2]==b[2];}
int rf_geomod_component_work_size(const rf_geomod_mesh_view *mesh,uint32_t *words)
{
    uint64_t slots=1,total;
    if(!mesh || !words || !mesh->face_count || !mesh->vertex_count)return RF_RANGE;
    while(slots<(uint64_t)mesh->vertex_count*2)slots*=2;
    total=slots*2+(uint64_t)mesh->face_count*3;
    if(total>UINT32_MAX/sizeof(uint32_t))return RF_RANGE;
    *words=(uint32_t)total;return RF_OK;
}
static uint32_t component_root(uint32_t *parent,uint32_t face)
{
    while(parent[face]!=face){parent[face]=parent[parent[face]];face=parent[face];}return face;
}
int rf_geomod_mesh_components(const rf_geomod_mesh_view *mesh,const rf_collision_face_filter *filters,
    uint32_t *work,uint32_t work_words,uint32_t *labels,uint32_t *count,uint32_t *largest)
{
    uint32_t words,slots,*table,*parent,*sizes,*ids,f,j,k,groups=0,biggest=UINT32_MAX,best_size=0;int status;
    if(!mesh || !mesh->vertices || !mesh->faces || !work || !labels || !count || !largest)return RF_RANGE;
    status=rf_geomod_component_work_size(mesh,&words);if(status)return status;
    if(work_words<words)return RF_RANGE;
    slots=(words-mesh->face_count*3)/2;table=work;parent=table+slots*2;sizes=parent+mesh->face_count;ids=sizes+mesh->face_count;
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;
        if(face->count<3 || face->first>mesh->vertex_count || face->count>mesh->vertex_count-face->first)return RF_FORMAT;
        if(filters && (filters[f].property_34<INT16_MIN || filters[f].property_34>INT16_MAX))return RF_FORMAT;
        for(j=0;j<face->count;j++)for(k=0;k<3;k++)if(!isfinite(mesh->vertices[face->first+j].position[k]))return RF_FORMAT;
    }
    for(k=0;k<slots*2;k++)table[k]=UINT32_MAX;
    for(f=0;f<mesh->face_count;f++){parent[f]=f;sizes[f]=0;ids[f]=UINT32_MAX;}
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;
        if(filters && ((filters[f].face_flags&12) || filters[f].property_34>0)){parent[f]=UINT32_MAX;continue;}
        for(j=0;j<face->count;j++) {
            uint32_t vertex=face->first+j,hash=2166136261u,slot;
            const float *point=mesh->vertices[vertex].position;
            for(k=0;k<3;k++){uint32_t bits=0;if(point[k]!=0)memcpy(&bits,point+k,4);hash=(hash^bits)*16777619u;}
            slot=hash&(slots-1);
            while(table[slot*2]!=UINT32_MAX && !same_position(point,mesh->vertices[table[slot*2]].position))slot=(slot+1)&(slots-1);
            if(table[slot*2]==UINT32_MAX){table[slot*2]=vertex;table[slot*2+1]=f;}
            else {
                uint32_t a=component_root(parent,f),b=component_root(parent,table[slot*2+1]);
                if(a!=b)parent[a]=b;
            }
        }
    }
    for(f=0;f<mesh->face_count;f++)if(parent[f]!=UINT32_MAX)++sizes[component_root(parent,f)];
    for(f=0;f<mesh->face_count;f++)if(parent[f]!=UINT32_MAX) {
        uint32_t root=component_root(parent,f);
        if(ids[root]==UINT32_MAX){ids[root]=groups++;if(sizes[root]>best_size){best_size=sizes[root];biggest=ids[root];}}
    }
    for(f=0;f<mesh->face_count;f++)labels[f]=parent[f]==UINT32_MAX?UINT32_MAX:ids[component_root(parent,f)];
    *count=groups;*largest=biggest;return RF_OK;
}
int rf_geomod_seed_adjacency(const rf_geomod_mesh_view *mesh,uint16_t *neighbors,uint32_t capacity)
{
    uint32_t pass,f,e,g,h,packed=0,i;int status;
    if(!mesh || !neighbors || !mesh->faces || mesh->face_count<4 || mesh->face_count>32 || capacity<mesh->vertex_count)return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(f=0;f<mesh->face_count;f++) {
        const rf_geomod_face *face=mesh->faces+f;
        if(face->first!=packed || face->count<3 || face->count>64 || face->count>mesh->vertex_count-packed)return RF_FORMAT;
        packed+=face->count;
    }
    if(packed!=mesh->vertex_count)return RF_FORMAT;
    for(pass=0;pass<2;pass++)for(f=0;f<mesh->face_count;f++)for(e=0;e<mesh->faces[f].count;e++) {
        const rf_geomod_face *face=mesh->faces+f;
        const float *a=mesh->vertices[face->first+e].position,*b=mesh->vertices[face->first+(e+1)%face->count].position;
        uint32_t found=0,opposite=0;
        if(same_position(a,b))return RF_FORMAT;
        for(g=0;g<mesh->face_count;g++)if(g!=f)for(h=0;h<mesh->faces[g].count;h++) {
            const rf_geomod_face *other=mesh->faces+g;
            const float *c=mesh->vertices[other->first+h].position,*d=mesh->vertices[other->first+(h+1)%other->count].position;
            if(same_position(a,c) && same_position(b,d))return RF_FORMAT;
            if(same_position(a,d) && same_position(b,c)){found++;opposite=g;}
        }
        if(found!=1)return RF_FORMAT;
        i=face->first+e;if(pass)neighbors[i]=(uint16_t)opposite;
    }
    return RF_OK;
}
/* Every triangle and the strict kernel bound one tetrahedron. The union
 * preserves the supplied concave boundary; no convex hull is substituted. */
static int star_mesh_planes(const rf_geomod_mesh_view *mesh,const float kernel[3],float out[32][4][4])
{
    uint32_t i,j,k,other,e;int status;
    if(!mesh || !mesh->faces || mesh->face_count<4 || mesh->face_count>32)return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(k=0;k<3;k++)if(!isfinite(kernel[k]))return RF_FORMAT;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *f=mesh->faces+i;
        if(f->count!=3 || f->first>mesh->vertex_count || 3>mesh->vertex_count-f->first)return RF_FORMAT;
    }
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_vertex *v=mesh->vertices+mesh->faces[i].first;
        float points[4][3],scratch[4][4];float (*planes)[4]=out?out[i]:scratch;
        for(j=0;j<3;j++)memcpy(points[j],v[j].position,12);
        memcpy(points[3],kernel,12);
        for(j=0;j<3;j++) {
            uint32_t matched=0;
            for(other=0;other<mesh->face_count;other++)if(other!=i) {
                const rf_geomod_vertex *w=mesh->vertices+mesh->faces[other].first;
                for(e=0;e<3;e++) {
                    if(same_position(v[j].position,w[(e+1)%3].position) &&
                       same_position(v[(j+1)%3].position,w[e].position))matched++;
                    if(same_position(v[j].position,w[e].position) &&
                       same_position(v[(j+1)%3].position,w[(e+1)%3].position))return RF_FORMAT;
                }
            }
            if(matched!=1)return RF_FORMAT;
        }
        for(j=0;j<4;j++) {
            static const unsigned char indices[4][4]={{0,1,2,3},{0,3,1,2},{1,3,2,0},{2,3,0,1}};
            const float *a=points[indices[j][0]],*b=points[indices[j][1]],*c=points[indices[j][2]],*opposite=points[indices[j][3]];
            double ab[3],ac[3],normal[3],length=0,d=0,distance;int parity=1;
            if(j) {
                const float *ordered[3]={a,b,c};uint32_t x,y;
                /* Adjacent tetrahedra must use exactly opposite versions of
                 * their shared internal plane. A different anchor changes the
                 * rounded offset even when the geometric triangle is identical. */
                for(x=0;x<2;x++)for(y=x+1;y<3;y++) {
                    for(k=0;k<3 && ordered[x][k]==ordered[y][k];k++);
                    if(k<3 && ordered[x][k]>ordered[y][k]) {
                        const float *swap=ordered[x];ordered[x]=ordered[y];ordered[y]=swap;parity=-parity;
                    }
                }
                a=ordered[0];b=ordered[1];c=ordered[2];
            }
            for(k=0;k<3;k++){ab[k]=(double)b[k]-a[k];ac[k]=(double)c[k]-a[k];}
            for(k=0;k<3;k++){normal[k]=ab[(k+1)%3]*ac[(k+2)%3]-ab[(k+2)%3]*ac[(k+1)%3];length+=normal[k]*normal[k];}
            if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
            length=sqrt(length);
            for(k=0;k<3;k++){planes[j][k]=(float)(parity*normal[k]/length);d-=(double)planes[j][k]*a[k];}
            planes[j][3]=(float)d;distance=planes[j][3];
            for(k=0;k<3;k++)distance+=(double)planes[j][k]*opposite[k];
            if(!isfinite(distance) || distance>=-1e-5)return RF_FORMAT;
        }
    }
    return RF_OK;
}
int rf_geomod_storage_prepare_star_cuts(rf_geomod_storage *s,
    const rf_geomod_mesh_view *cutters,const float (*kernels)[3],uint32_t count,
    uint32_t cavity,rf_geomod_multi_work *work)
{
    uint32_t c;int status;
    if(!s || !work || s->editing || cavity>1 || count>RF_GEOMOD_CUT_LIMIT || (count && (!cutters || !kernels)))return RF_RANGE;
    for(c=0;c<count;c++) {
        status=star_mesh_planes(cutters+c,kernels[c],work->star_planes[c]);if(status)return status;
        work->star_count[c]=cutters[c].face_count;memcpy(work->star_kernels[c],kernels[c],12);
    }
    return cavity?prepare_cavity_cuts(s,cutters,count,work,1):prepare_cuts(s,cutters,count,work,1);
}

static int collision_mesh_face(const rf_geomod_mesh_view *mesh,uint32_t index,
    const rf_collision_face_filter *filter,rf_collision_face *out)
{
    const rf_geomod_face *f=mesh->faces+index;rf_collision_face value={0};
    double normal[3]={0},length=0;uint32_t i,j,accepted;int status;
    if(f->count<3 || f->count>64 || f->first>mesh->vertex_count || f->count>mesh->vertex_count-f->first)return RF_FORMAT;
    status=rf_collision_face_accept(filter,&accepted);if(status)return status;
    for(i=0;i<f->count;i++) {
        const float *a=mesh->vertices[f->first+i].position,*b=mesh->vertices[f->first+(i+1)%f->count].position;
        for(j=0;j<3;j++) {
            normal[j]+=(double)a[(j+1)%3]*b[(j+2)%3]-(double)a[(j+2)%3]*b[(j+1)%3];
            if(!i || a[j]<value.minimum[j])value.minimum[j]=a[j];
            if(!i || a[j]>value.maximum[j])value.maximum[j]=a[j];
        }
    }
    for(j=0;j<3;j++)length+=normal[j]*normal[j];
    if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
    length=sqrt(length);
    for(j=0;j<3;j++) {
        value.plane[j]=(float)(normal[j]/length);
        value.plane[3]-=value.plane[j]*mesh->vertices[f->first].position[j];
        /* Same bound expansion as rf_geometry_collision_face /4e002b. */
        value.minimum[j]-=.0001f;value.maximum[j]+=.0001f;
        if(!isfinite(value.minimum[j]) || !isfinite(value.maximum[j]))return RF_FORMAT;
    }
    if(!isfinite(value.plane[3]))return RF_FORMAT;
    for(i=0;i<f->count;i++) {
        const float *a=mesh->vertices[f->first+i].position,*b=mesh->vertices[f->first+(i+1)%f->count].position;
        double distance=value.plane[3],edge[3],size=0;uint32_t k;
        for(j=0;j<3;j++){distance+=(double)value.plane[j]*a[j];edge[j]=(double)b[j]-a[j];size+=edge[j]*edge[j];}
        if(fabs(distance)>1e-5 || size<=1e-24)return RF_FORMAT;
        /* Every vertex must lie on the inward side of each directed edge. */
        for(k=0;k<f->count;k++) {
            const float *p=mesh->vertices[f->first+k].position;double inward=0;
            for(j=0;j<3;j++)inward+=value.plane[j]*(edge[(j+1)%3]*(p[(j+2)%3]-a[(j+2)%3])-edge[(j+2)%3]*(p[(j+1)%3]-a[(j+1)%3]));
            if(inward< -1e-5*sqrt(size))return RF_FORMAT;
        }
    }
    value.count=f->count;value.filter=*filter;value.triangle_surface=1;*out=value;return RF_OK;
}
typedef struct partition_output {
    rf_geomod_vertex *vertices;rf_geomod_face *faces;
    uint32_t vc,fc,nv,nf;int status;
} partition_output;
static int partition_valid_piece(const rf_geomod_vertex *v,uint32_t n,const double normal[3])
{
    rf_geomod_face f={0,n,0,UINT32_MAX};rf_geomod_mesh_view mesh={v,&f,n,1,0};
    rf_collision_face bound;rf_collision_face_filter filter={0};
    int status=collision_mesh_face(&mesh,0,&filter,&bound);
    if(status)return 0;
    return !normal || bound.plane[0]*normal[0]+bound.plane[1]*normal[1]+bound.plane[2]*normal[2]>0;
}
static int partition_emit(partition_output *out,const rf_geomod_vertex *v,uint32_t n,const rf_geomod_face *source)
{
    if(n>out->vc-out->nv || out->nf==out->fc){out->status=RF_RANGE;return 0;}
    memcpy(out->vertices+out->nv,v,n*sizeof(*v));
    out->faces[out->nf++]=(rf_geomod_face){out->nv,n,source->material,source->source_face};out->nv+=n;return 1;
}
static int partition_polygon_edges(partition_output *out,const rf_geomod_vertex *v,const rf_geomod_face *source,const uint16_t *edges,uint16_t face_plane)
{
    rf_geomod_vertex part[64],center={0};double normal[3]={0},sum[5]={0};uint32_t n=source->count,i,j,k,c,na,nb;
    if(n<3 || n>64)return 0;
    if(partition_valid_piece(v,n,NULL))return partition_emit(out,v,n,source);
    for(i=0;i<n;i++)for(k=0;k<3;k++)normal[k]+=(double)v[i].position[(k+1)%3]*v[(i+1)%n].position[(k+2)%3]-(double)v[i].position[(k+2)%3]*v[(i+1)%n].position[(k+1)%3];
    for(i=0;i<n;i++)for(j=i+2;j<n;j++) {
        if(i==0 && j==n-1)continue;
        /* Equal boundary supports describe one mathematical line even when
         * stored float positions bend slightly. Never split that run into
         * a separate face; retain its vertices in a nondegenerate piece. */
        if(edges) {
            uint32_t e;uint16_t support=edges[i];
            for(e=i;e<j && edges[e]==support;e++);
            if(e==j && support!=UINT16_MAX && support!=face_plane)continue;
            support=edges[j];e=j;
            do {if(edges[e]!=support)break;e=(e+1)%n;}while(e!=i);
            if(e==i && support!=UINT16_MAX && support!=face_plane)continue;
        }
        /* Do not turn a nearly collinear boundary chain into a thin extra
         * face: closure must distinguish the new diagonal from the boundary. */
        {
            double mid[3];uint32_t e;int separated=1;
            for(k=0;k<3;k++)mid[k]=((double)v[i].position[k]+v[j].position[k])*.5;
            for(e=0;e<n && separated;e++) {
                double d[3],length=0,t=0,error=0;
                for(k=0;k<3;k++){d[k]=(double)v[(e+1)%n].position[k]-v[e].position[k];length+=d[k]*d[k];t+=(mid[k]-v[e].position[k])*d[k];}
                if(length==0){separated=0;break;}t/=length;if(t<0)t=0;if(t>1)t=1;
                for(k=0;k<3;k++){double q=mid[k]-v[e].position[k]-t*d[k];error+=q*q;}
                if(error<=1e-12)separated=0;
            }
            if(separated) {
                double d[3],length=0;
                for(k=0;k<3;k++){d[k]=(double)v[j].position[k]-v[i].position[k];length+=d[k]*d[k];}
                if(length==0)separated=0;
                for(e=0;e<n && separated;e++)if(e!=i && e!=j) {
                    double t=0,error=0;
                    for(k=0;k<3;k++)t+=((double)v[e].position[k]-v[i].position[k])*d[k];
                    t/=length;if(t<=1e-8 || t>=1-1e-8)continue;
                    for(k=0;k<3;k++){double q=(double)v[e].position[k]-v[i].position[k]-t*d[k];error+=q*q;}
                    if(error<=1e-12)separated=0;
                }
            }
            if(!separated)continue;
        }
        na=j-i+1;nb=n-na+2;
        for(c=0;c<na;c++)part[c]=v[i+c];
        if(!partition_valid_piece(part,na,normal))continue;
        for(c=0;c<nb;c++)part[c]=v[(j+c)%n];
        if(!partition_valid_piece(part,nb,normal))continue;
        if(!partition_emit(out,part,nb,source))return 0;
        for(c=0;c<na;c++)part[c]=v[i+c];
        return partition_emit(out,part,na,source);
    }
    for(i=0;i<n;i++){for(k=0;k<3;k++)sum[k]+=v[i].position[k];for(k=0;k<2;k++)sum[k+3]+=v[i].uv[k];}
    for(k=0;k<3;k++)center.position[k]=(float)(sum[k]/n);
    for(k=0;k<2;k++)center.uv[k]=(float)(sum[k+3]/n);
    for(i=0;i<n;i++) {
        part[0]=center;part[1]=v[i];part[2]=v[(i+1)%n];
        if(!partition_valid_piece(part,3,normal) || !partition_emit(out,part,3,source))return 0;
    }
    return 1;
}
static int partition_polygon(partition_output *out,const rf_geomod_vertex *v,const rf_geomod_face *source)
{return partition_polygon_edges(out,v,source,NULL,UINT16_MAX);}
/* Assemble only points carrying the same unordered pair of supporting planes.
 * Reuse clipping workspace after the final subtraction. The live bank and the
 * immutable source remain untouched, including when expansion exceeds capacity. */
static int repair_cavity_pending_provenance(rf_geomod_storage *s,rf_geomod_multi_work *work,geomod_face_lineage *lineage,geomod_step_support *provenance)
{
    uint32_t bank=s->current^1,f,e,q,r,k,n;rf_geomod_vertex polygon[64];uint16_t polygon_edges[64];
    partition_output output={work->repair.vertices,work->repair.faces,
        s->vertex_capacity,s->face_capacity,0,0,RF_FORMAT};
    if(lineage && s->face_capacity>RF_GEOMOD_WORK_FACES)return RF_RANGE;
    for(f=0;f<s->nf[bank];f++) {
        const rf_geomod_face *face=s->faces[bank]+f;uint16_t plane=work->compact_planes[f];n=0;
        for(e=0;e<face->count;e++) {
            const rf_geomod_vertex *a=s->vertices[bank]+face->first+e;
            const rf_geomod_vertex *b=s->vertices[bank]+face->first+(e+1)%face->count;
            uint16_t edge=work->compact_edges[face->first+e];
            double d[3],length=0,fractions[64];const rf_geomod_vertex *points[64];uint32_t used=0,i,j;
            if(n==64)return RF_RANGE;polygon_edges[n]=edge;polygon[n++]=*a;
            if(plane==UINT16_MAX || edge==UINT16_MAX || (provenance && plane==edge))continue;
            for(k=0;k<3;k++){d[k]=(double)b->position[k]-a->position[k];length+=d[k]*d[k];}
            if(length==0)return RF_FORMAT;
            for(q=0;q<s->nf[bank];q++) {
                const rf_geomod_face *other=s->faces[bank]+q;uint16_t op=work->compact_planes[q];
                if(op!=plane && op!=edge)continue;
                for(r=0;r<other->count;r++) {
                    uint16_t oe=work->compact_edges[other->first+r];uint32_t endpoint;
                    if(!((op==plane && oe==edge)||(op==edge && oe==plane)))continue;
                    for(endpoint=0;endpoint<2;endpoint++) {
                        const rf_geomod_vertex *p=s->vertices[bank]+other->first+(r+endpoint)%other->count;
                        /* Distinct rounded endpoints can tie on the dominant axis.
                         * Project on the complete retained edge before ordering. */
                        double t=0;for(k=0;k<3;k++)t+=((double)p->position[k]-a->position[k])*d[k];
                        t/=length;
                        if(t<=0 || t>=1)continue;
                        for(i=0;i<used;i++)if(!memcmp(points[i]->position,p->position,12))break;
                        if(i<used)continue;
                        if(used==64)return RF_RANGE;
                        for(i=0;i<used;i++)if(t==fractions[i])return RF_FORMAT;
                        for(i=used;i && fractions[i-1]>t;i--){fractions[i]=fractions[i-1];points[i]=points[i-1];}
                        fractions[i]=t;points[i]=p;used++;
                    }
                }
            }
            if(used>64-n)return RF_RANGE;
            for(i=0;i<used;i++) {
                polygon_edges[n]=edge;polygon[n]=*points[i];
                /* This face owns its texture seam: interpolate from its edge,
                 * never borrow UVs from the adjacent face's supporting point. */
                for(j=0;j<2;j++)polygon[n].uv[j]=(float)((double)a->uv[j]+fractions[i]*((double)b->uv[j]-a->uv[j]));
                n++;
            }
        }
        {rf_geomod_face expanded=*face;uint32_t first=output.nf;
         expanded.first=0;expanded.count=n;
         if(!partition_polygon_edges(&output,polygon,&expanded,provenance?polygon_edges:NULL,plane))return output.status;
         if(lineage)memset(lineage->repaired+first,lineage->pending[f],output.nf-first);
         if(provenance) {
             uint32_t child,v;
             for(child=first;child<output.nf;child++) {
                 const rf_geomod_face *cf=output.faces+child;uint32_t prior,duplicate=0;
                 /* A repaired fragment can coincide exactly with a retained
                  * fragment. Keep the first same-support/same-birth surface;
                  * rounded UV interpolation alone must not duplicate geometry. */
                 for(prior=0;prior<child && !duplicate;prior++) {
                     const rf_geomod_face *pf=output.faces+prior;uint32_t offset,k;
                     if(provenance->planes[prior]!=plane || pf->count!=cf->count ||
                        pf->material!=cf->material || pf->source_face!=cf->source_face ||
                        (lineage && lineage->repaired[prior]!=lineage->repaired[child]))continue;
                     for(offset=0;offset<cf->count;offset++) {
                         for(k=0;k<cf->count;k++)if(memcmp(output.vertices[cf->first+k].position,
                             output.vertices[pf->first+(offset+k)%pf->count].position,12))break;
                         if(k==cf->count){duplicate=1;break;}
                     }
                 }
                 if(duplicate) {
                     uint32_t first_vertex=cf->first,count=cf->count,k;
                     memmove(output.vertices+first_vertex,output.vertices+first_vertex+count,
                         (output.nv-first_vertex-count)*sizeof(*output.vertices));output.nv-=count;
                     memmove(output.faces+child,output.faces+child+1,(output.nf-child-1)*sizeof(*output.faces));output.nf--;
                     for(k=child;k<output.nf;k++)output.faces[k].first-=count;
                     if(lineage)memmove(lineage->repaired+child,lineage->repaired+child+1,output.nf-child);
                     child--;continue;
                 }
                 provenance->planes[child]=plane;
                 for(v=0;v<cf->count;v++) {
                     const rf_geomod_vertex *a=output.vertices+cf->first+v,*b=output.vertices+cf->first+(v+1)%cf->count;
                     uint32_t from,to,j;uint16_t support=plane;
                     for(from=0;from<n;from++)if(!memcmp(a->position,polygon[from].position,12))break;
                     for(to=0;to<n;to++)if(!memcmp(b->position,polygon[to].position,12))break;
                     /* A center-fan partition introduces an interior vertex.
                      * Its spokes are diagonals, not original boundary runs. */
                     if(from==n || to==n) {
                         int status=diagonal_register(&provenance->diagonals,a->position,b->position,&support);if(status)return status;
                     } else {
                         /* Boundary runs retain their original supporting plane.
                          * Partition diagonals retain their original endpoints. */
                         support=polygon_edges[from];j=(from+1)%n;
                         while(j!=to && polygon_edges[j]==support)j=(j+1)%n;
                         if(j!=to){int status=diagonal_register(&provenance->diagonals,a->position,b->position,&support);if(status)return status;}
                     }
                     provenance->edges[cf->first+v]=support;
                 }
             }
         }}
    }
    memcpy(s->vertices[bank],output.vertices,output.nv*sizeof(*output.vertices));
    memcpy(s->faces[bank],output.faces,output.nf*sizeof(*output.faces));
    if(lineage)memcpy(lineage->pending,lineage->repaired,output.nf);
    if(provenance) {
        memcpy(work->compact_edges,provenance->edges,output.nv*sizeof(uint16_t));
        memcpy(work->compact_planes,provenance->planes,output.nf*sizeof(uint16_t));
    }
    s->nv[bank]=output.nv;s->nf[bank]=output.nf;return RF_OK;
}

static int repair_cavity_pending_lineage(rf_geomod_storage *s,rf_geomod_multi_work *work,geomod_face_lineage *lineage)
{return repair_cavity_pending_provenance(s,work,lineage,NULL);}

static int repair_cavity_pending(rf_geomod_storage *s,rf_geomod_multi_work *work)
{return repair_cavity_pending_lineage(s,work,NULL);}

int rf_geomod_partition_mesh(const rf_geomod_mesh_view *mesh,
    rf_geomod_vertex *vertices,uint32_t vc,rf_geomod_face *faces,uint32_t fc,
    rf_geomod_mesh_view *out)
{
    partition_output output={vertices,faces,vc,fc,0,0,RF_FORMAT};uint32_t i;int status;
    if(!mesh || !out || !vertices || !faces || !vc || !fc ||
       !mesh->faces || !mesh->face_count)return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(i=0;i<mesh->face_count;i++) {
        const rf_geomod_face *f=mesh->faces+i;
        if(f->count<3 || f->count>64 || f->first>mesh->vertex_count ||
           f->count>mesh->vertex_count-f->first)return RF_FORMAT;
    }
    for(i=0;i<mesh->face_count;i++)if(!partition_polygon(&output,
        mesh->vertices+mesh->faces[i].first,mesh->faces+i))return output.status;
    *out=(rf_geomod_mesh_view){vertices,faces,output.nv,output.nf,mesh->generation};
    return RF_OK;
}

int rf_geomod_collision_faces(const rf_geomod_mesh_view *mesh,
    const rf_collision_face_filter *filters,float (*positions)[3],uint32_t vc,
    rf_collision_face *faces,uint32_t fc)
{
    uint32_t i;rf_collision_face face;int status;
    if(!mesh || vc<mesh->vertex_count || fc<mesh->face_count ||
       (mesh->vertex_count && !positions) || (mesh->face_count && (!mesh->faces || !filters || !faces)))return RF_RANGE;
    status=storage_vertices(mesh->vertices,mesh->vertex_count);if(status)return status;
    for(i=0;i<mesh->face_count;i++){status=collision_mesh_face(mesh,i,filters+i,&face);if(status)return status;}
    for(i=0;i<mesh->vertex_count;i++)memcpy(positions[i],mesh->vertices[i].position,12);
    for(i=0;i<mesh->face_count;i++) {
        collision_mesh_face(mesh,i,filters+i,&face);face.vertices=positions+mesh->faces[i].first;faces[i]=face;
    }
    return RF_OK;
}

struct rf_geomod_terrain {
    rf_geomod_storage *mesh;rf_geomod_multi_work work;
    rf_geomod_vertex cut_vertices[RF_GEOMOD_CUT_LIMIT][60];
    rf_geomod_face cut_faces[RF_GEOMOD_CUT_LIMIT][20];
    rf_geomod_mesh_view cuts[RF_GEOMOD_CUT_LIMIT];
    float kernels[RF_GEOMOD_CUT_LIMIT][3];uint32_t star_mask,mapping_width,mapping_height;
    rf_collision_face_filter original_filters[32],generated_filter,*filters;
    rf_collision_face *faces[2];float (*positions[2])[3];rf_collision_tree tree;
    uint32_t bank,count,cavity,vc,fc,base_bytes,budget,peak_bytes;
};
static int terrain_bind(rf_geomod_terrain *t,const rf_geomod_mesh_view *mesh,uint32_t bank,
    rf_collision_tree *tree)
{
    uint32_t i,j,used;int status;
    for(i=0;i<mesh->face_count;i++) {
        uint32_t id=mesh->faces[i].source_face;
        t->filters[i]=t->generated_filter;
        if(id!=UINT32_MAX) {
            for(j=0;j<t->mesh->nf[2];j++)if(t->mesh->faces[2][j].source_face==id)break;
            if(j==t->mesh->nf[2])return RF_FORMAT;
            t->filters[i]=t->original_filters[j];
        }
    }
    status=rf_geomod_collision_faces(mesh,t->filters,t->positions[bank],t->vc,t->faces[bank],t->fc);if(status)return status;
    used=t->base_bytes+t->tree.allocated_bytes;
    if(used>t->budget)return RF_RANGE;
    status=rf_collision_tree_open_scratch(t->faces[bank],mesh->face_count,t->budget-used,tree,t->work.vertices,sizeof(t->work.vertices));if(status)return status;
    if(used+tree->peak_bytes>t->peak_bytes)t->peak_bytes=used+tree->peak_bytes;
    return RF_OK;
}
int rf_geomod_terrain_cutter_get(const rf_geomod_terrain *t,uint32_t index,
    rf_geomod_mesh_view *mesh,float kernel[3],uint32_t *star)
{
    uint32_t value;float result[3]={0,0,0};
    if(!t || !mesh || !kernel || !star || index>=t->count)return RF_RANGE;
    value=(t->star_mask>>index)&1u;
    if(value)memcpy(result,t->kernels[index],sizeof(result));
    *mesh=t->cuts[index];memcpy(kernel,result,sizeof(result));*star=value;return RF_OK;
}
void rf_geomod_terrain_close(rf_geomod_terrain **terrain)
{
    if(terrain && *terrain) {
        rf_geomod_terrain *t=*terrain;
        rf_collision_tree_close(&t->tree);rf_geomod_storage_close(&t->mesh);free(t);*terrain=NULL;
    }
}
int rf_geomod_terrain_open(const rf_geomod_mesh_view *source,
    const rf_collision_face_filter *filters,const rf_collision_face_filter *generated_filter,
    uint32_t cavity,uint32_t vc,uint32_t fc,uint32_t budget,rf_geomod_terrain **out)
{
    rf_geomod_terrain *t;rf_geomod_mesh_view mesh;rf_collision_tree tree={0};
    uint64_t bytes=sizeof(*t)+(uint64_t)vc*24+(uint64_t)fc*(2*sizeof(rf_collision_face)+sizeof(rf_collision_face_filter));
    unsigned char *p;uint32_t i,j,accepted;int status;
    if(!source || !filters || !generated_filter || !out || *out || cavity>1 || source->face_count>32 ||
       !source->faces || !vc || !fc || source->face_count>fc || source->vertex_count>vc || bytes>budget)return RF_RANGE;
    status=rf_collision_face_accept(generated_filter,&accepted);if(status)return status;
    for(i=0;i<source->face_count;i++) {
        if(source->faces[i].source_face==UINT32_MAX)return RF_FORMAT;
        for(j=0;j<i;j++)if(source->faces[i].source_face==source->faces[j].source_face)return RF_FORMAT;
        status=rf_collision_face_accept(filters+i,&accepted);if(status)return status;
    }
    t=calloc(1,(size_t)bytes);if(!t)return RF_IO;
    t->vc=vc;t->fc=fc;t->budget=budget;t->base_bytes=(uint32_t)bytes;t->cavity=cavity;
    memcpy(t->original_filters,filters,source->face_count*sizeof(*filters));t->generated_filter=*generated_filter;
    p=(unsigned char *)(t+1);
    for(i=0;i<2;i++) {
        t->positions[i]=(float(*)[3])p;p+=(size_t)vc*12;
        /* Position bytes are multiples of12. Align native pointer-bearing
         * face arrays by placing both position banks before them below. */
    }
    /* calloc base/owner alignment plus24*vc preserves pointer alignment. */
    for(i=0;i<2;i++){t->faces[i]=(rf_collision_face *)p;p+=(size_t)fc*sizeof(rf_collision_face);}
    t->filters=(rf_collision_face_filter *)p;
    status=rf_geomod_storage_open(source,vc,fc,budget-t->base_bytes,&t->mesh);if(status)goto failed;
    t->base_bytes+=rf_geomod_storage_bytes(t->mesh);
    status=convex_mesh_planes_oriented(source,t->work.source_planes,cavity);if(status)goto failed;
    rf_geomod_storage_view(t->mesh,&mesh);
    status=terrain_bind(t,&mesh,0,&tree);if(status)goto failed;
    t->tree=tree;*out=t;return RF_OK;
failed:
    rf_collision_tree_close(&tree);rf_geomod_terrain_close(&t);return status;
}
int rf_geomod_terrain_set_mapping(rf_geomod_terrain *t,uint32_t width,uint32_t height)
{
    if(!t || t->count || !width || !height || width>INT32_MAX || height>INT32_MAX)return RF_RANGE;
    t->mapping_width=width;t->mapping_height=height;return RF_OK;
}
static int terrain_map_pending_lineage(rf_geomod_terrain *t,const geomod_face_lineage *lineage)
{
    rf_geomod_storage *s=t->mesh;uint32_t bank=s->current^1,i,j,k;int status;
    if(!t->mapping_width)return RF_OK;
    for(i=0;i<s->nf[bank];i++) {
        const rf_geomod_face *f=s->faces[bank]+i;double normal[3]={0},length=0;float n[3];
        rf_geomod_vertex *v=s->vertices[bank]+f->first;
        if(f->source_face!=UINT32_MAX || (lineage && !lineage->pending[i]))continue;
        for(j=0;j<f->count;j++)for(k=0;k<3;k++) {
            const float *a=v[j].position,*b=v[(j+1)%f->count].position;
            normal[k]+=(double)a[(k+1)%3]*b[(k+2)%3]-(double)a[(k+2)%3]*b[(k+1)%3];
        }
        for(k=0;k<3;k++)length+=normal[k]*normal[k];
        if(!isfinite(length) || length<=1e-24)return RF_FORMAT;
        length=sqrt(length);for(k=0;k<3;k++)n[k]=(float)(normal[k]/length);
        for(j=0;j<f->count;j++) {
            status=rf_geomod_planar_uv(n,v[j].position,t->mapping_width,t->mapping_height,v[j].uv);
            if(status)return status;
        }
    }
    return RF_OK;
}
static int terrain_map_pending(rf_geomod_terrain *t)
{return terrain_map_pending_lineage(t,NULL);}

/* Reconstruct in a disposable storage owner so any prefix failure preserves
 * the live bank. Reuse the existing work owner; release replay before tree build. */
static inline int terrain_prepare_chronological_mesh(rf_geomod_terrain *t,uint32_t count)
{
    rf_geomod_storage *live=t->mesh,*replay=NULL;
    rf_geomod_mesh_view source={live->vertices[2],live->faces[2],live->nv[2],live->nf[2],0},result;
    geomod_face_lineage *lineage=NULL;geomod_step_support *support=NULL;
    uint64_t used=(uint64_t)t->base_bytes+t->tree.allocated_bytes+sizeof(*lineage)+sizeof(*support);
    uint32_t c,i;int status;
    if(!count || count>RF_GEOMOD_CUT_LIMIT || live->editing)return RF_RANGE;
    if(used>=t->budget)return RF_RANGE;
    {
        uint64_t bytes=sizeof(*replay)+(uint64_t)t->vc*sizeof(rf_geomod_vertex)+(uint64_t)t->fc*sizeof(rf_geomod_face);
        unsigned char *memory;uint32_t inactive=live->current^1;
        if(bytes>t->budget-used || bytes>UINT32_MAX)return RF_RANGE;
        replay=calloc(1,(size_t)bytes);if(!replay)return RF_IO;
        replay->bytes=(uint32_t)bytes;replay->vertex_capacity=t->vc;replay->face_capacity=t->fc;replay->generation=1;
        memory=(unsigned char *)(replay+1);replay->vertices[0]=(rf_geomod_vertex *)memory;
        replay->faces[0]=(rf_geomod_face *)(memory+(size_t)t->vc*sizeof(rf_geomod_vertex));
        /* Borrow only the unpublished bank and immutable source. Live current
         * geometry/tree remain intact throughout every replay prefix. */
        replay->vertices[1]=live->vertices[inactive];replay->faces[1]=live->faces[inactive];
        replay->vertices[2]=live->vertices[2];replay->faces[2]=live->faces[2];
        replay->nv[0]=replay->nv[2]=source.vertex_count;replay->nf[0]=replay->nf[2]=source.face_count;
        memcpy(replay->vertices[0],source.vertices,source.vertex_count*sizeof(*source.vertices));
        memcpy(replay->faces[0],source.faces,source.face_count*sizeof(*source.faces));
    }
    used+=rf_geomod_storage_bytes(replay);if(used>t->peak_bytes)t->peak_bytes=(uint32_t)used;
    lineage=calloc(1,sizeof(*lineage));support=calloc(1,sizeof(*support));
    if(!lineage || !support){status=RF_IO;goto done;}
    for(c=1;c<=count;c++) {
        status=prepare_chronological_step(replay,t->cuts,c,&t->work,lineage,support,t->cavity);if(status)goto done;
        /* No callbacks occur while selecting this private mapping target. */
        t->mesh=replay;status=terrain_map_pending_lineage(t,lineage);t->mesh=live;
        if(status)goto done;
        status=rf_geomod_storage_commit(replay);if(status)goto done;
    }
    status=rf_geomod_storage_view(replay,&result);if(status)goto done;
    status=rf_geomod_storage_begin(live);if(status)goto done;
    i=live->current^1;
    if(result.vertices!=live->vertices[i])memcpy(live->vertices[i],result.vertices,result.vertex_count*sizeof(*result.vertices));
    if(result.faces!=live->faces[i])memcpy(live->faces[i],result.faces,result.face_count*sizeof(*result.faces));
    live->nv[i]=result.vertex_count;live->nf[i]=result.face_count;
done:
    free(support);free(lineage);rf_geomod_storage_close(&replay);return status;
}

typedef struct terrain_pending {
    rf_geomod_mesh_view mesh;rf_collision_tree tree;uint32_t bank,count;
} terrain_pending;
static void terrain_abort(rf_geomod_terrain *t,terrain_pending *pending)
{rf_collision_tree_close(&pending->tree);rf_geomod_storage_abort(t->mesh);}
static int terrain_prepare(rf_geomod_terrain *t,uint32_t count,terrain_pending *pending)
{
    uint32_t c;int status;memset(pending,0,sizeof(*pending));pending->bank=t->bank^1;pending->count=count;
    for(c=0;c<count;c++) {
        if(t->star_mask&(1u<<c)) {
            status=star_mesh_planes(t->cuts+c,t->kernels[c],t->work.star_planes[c]);
            t->work.star_count[c]=t->cuts[c].face_count;memcpy(t->work.star_kernels[c],t->kernels[c],12);
        } else {
            status=convex_mesh_planes(t->cuts+c,t->work.cut_planes[c]);t->work.star_count[c]=0;
        }
        if(status)goto failed;
    }
#ifndef RF_GEOMOD_LEGACY_REPLAY_TEST
    if(count) {
        status=terrain_prepare_chronological_mesh(t,count);if(status)goto failed;
    } else
#endif
    {
        status=t->cavity?prepare_cavity_cuts(t->mesh,t->cuts,count,&t->work,1):
            prepare_cuts(t->mesh,t->cuts,count,&t->work,1);
        if(status)goto failed;
        status=terrain_map_pending(t);if(status)goto failed;
    }
    status=rf_geomod_storage_pending(t->mesh,&pending->mesh);if(status)goto failed;
    status=terrain_bind(t,&pending->mesh,pending->bank,&pending->tree);if(status)goto failed;
    return RF_OK;
failed:
    terrain_abort(t,pending);return status;
}
static int terrain_commit(rf_geomod_terrain *t,terrain_pending *pending)
{
    int status=rf_geomod_storage_commit(t->mesh);if(status)return status;
    rf_collision_tree_close(&t->tree);t->tree=pending->tree;memset(&pending->tree,0,sizeof(pending->tree));
    t->bank=pending->bank;t->count=pending->count;return RF_OK;
}
static int terrain_publish_checked(rf_geomod_terrain *t,uint32_t count,rf_geomod_terrain_check_fn check,void *context)
{
    terrain_pending pending;int status=terrain_prepare(t,count,&pending);
    if(!status && check) {
        rf_geomod_terrain_view candidate;
        candidate.mesh=pending.mesh;candidate.faces=t->faces[pending.bank];candidate.tree=&pending.tree;
        candidate.cuts=count;candidate.resident_bytes=t->base_bytes+t->tree.allocated_bytes+pending.tree.allocated_bytes;
        candidate.peak_bytes=t->peak_bytes;
        status=check(&candidate,context);
        if(status){terrain_abort(t,&pending);return status;}
    }
    if(!status){status=terrain_commit(t,&pending);if(status)terrain_abort(t,&pending);}
    return status;
}
static int terrain_publish(rf_geomod_terrain *t,uint32_t count)
{return terrain_publish_checked(t,count,NULL,NULL);}
int rf_geomod_terrain_cut_box_checked(rf_geomod_terrain *t,const float center[3],const float extent[3],uint32_t material,
    rf_geomod_terrain_check_fn check,void *context)
{
    float lo[3],hi[3];uint32_t axis,side,j,slot;
    const int u[4]={-1,1,1,-1},v[4]={-1,-1,1,1};
    if(!t || !center || !extent || material==UINT32_MAX || t->count==RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
    for(j=0;j<3;j++) {
        if(!isfinite(center[j]) || !isfinite(extent[j]) || extent[j]<=0)return RF_FORMAT;
        lo[j]=center[j]-extent[j];hi[j]=center[j]+extent[j];
        if(!isfinite(lo[j]) || !isfinite(hi[j]) || lo[j]>=hi[j])return RF_FORMAT;
    }
    slot=t->count;t->star_mask&=~(1u<<slot);
    for(axis=0;axis<3;axis++)for(side=0;side<2;side++) {
        uint32_t face=axis*2+side,a=(axis+1)%3,b=(axis+2)%3;
        t->cut_faces[slot][face]=(rf_geomod_face){face*4,4,material,UINT32_MAX};
        for(j=0;j<4;j++) {
            uint32_t k=side?j:3-j;rf_geomod_vertex *p=t->cut_vertices[slot]+face*4+j;
            p->position[axis]=side?hi[axis]:lo[axis];
            p->position[a]=u[k]>0?hi[a]:lo[a];p->position[b]=v[k]>0?hi[b]:lo[b];
            /* Stable world-scale planar UVs, provisional excavation material mapping. */
            p->uv[0]=p->position[a];p->uv[1]=p->position[b];
        }
    }
    t->cuts[slot]=(rf_geomod_mesh_view){t->cut_vertices[slot],t->cut_faces[slot],24,6,0};
    return terrain_publish_checked(t,t->count+1,check,context);
}
int rf_geomod_terrain_cut_box(rf_geomod_terrain *t,const float center[3],const float extent[3],uint32_t material)
{return rf_geomod_terrain_cut_box_checked(t,center,extent,material,NULL,NULL);}
/* Inscribed twenty-face sphere approximation: bounded and deliberately faceted.
 * Reuses the same transactional union history as box excavation. */
int rf_geomod_terrain_cut_crater(rf_geomod_terrain *t,const float center[3],float radius,uint32_t material)
{
    static const float points[12][3]={{-1,1.618033989f,0},{1,1.618033989f,0},{-1,-1.618033989f,0},{1,-1.618033989f,0},
        {0,-1,1.618033989f},{0,1,1.618033989f},{0,-1,-1.618033989f},{0,1,-1.618033989f},
        {1.618033989f,0,-1},{1.618033989f,0,1},{-1.618033989f,0,-1},{-1.618033989f,0,1}};
    static const unsigned char triangles[20][3]={{0,11,5},{0,5,1},{0,1,7},{0,7,10},{0,10,11},
        {1,5,9},{5,11,4},{11,10,2},{10,7,6},{7,1,8},
        {3,9,4},{3,4,2},{3,2,6},{3,6,8},{3,8,9},
        {4,9,5},{2,4,11},{6,2,10},{8,6,7},{9,8,1}};
    uint32_t i,j,k,slot;float scale=radius/1.902113033f;
    if(!t || !center || material==UINT32_MAX || t->count==RF_GEOMOD_CUT_LIMIT)return RF_RANGE;
    if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    for(k=0;k<3;k++)if(!isfinite(center[k]) || !isfinite(center[k]-radius) || !isfinite(center[k]+radius))return RF_FORMAT;
    slot=t->count;t->star_mask&=~(1u<<slot);
    for(i=0;i<20;i++) {
        float normal[3],a[3],b[3];uint32_t u,v;
        for(k=0;k<3;k++){a[k]=points[triangles[i][1]][k]-points[triangles[i][0]][k];b[k]=points[triangles[i][2]][k]-points[triangles[i][0]][k];}
        for(k=0;k<3;k++)normal[k]=a[(k+1)%3]*b[(k+2)%3]-a[(k+2)%3]*b[(k+1)%3];
        k=0;if(fabsf(normal[1])>fabsf(normal[k]))k=1;if(fabsf(normal[2])>fabsf(normal[k]))k=2;u=(k+1)%3;v=(k+2)%3;
        t->cut_faces[slot][i]=(rf_geomod_face){i*3,3,material,UINT32_MAX};
        for(j=0;j<3;j++) {
            rf_geomod_vertex *p=t->cut_vertices[slot]+i*3+j;
            for(k=0;k<3;k++)p->position[k]=center[k]+points[triangles[i][j]][k]*scale;
            p->uv[0]=p->position[u]*.25f;p->uv[1]=p->position[v]*.25f;
        }
    }
    t->cuts[slot]=(rf_geomod_mesh_view){t->cut_vertices[slot],t->cut_faces[slot],60,20,0};
    return terrain_publish(t,t->count+1);
}
static int terrain_cut_star_checked(rf_geomod_terrain *t,
    const rf_geomod_mesh_view *cutter,const float kernel[3],rf_geomod_terrain_check_fn check,void *context)
{
    uint32_t slot,i;int status;
    if(!t || !cutter || !kernel || t->count==RF_GEOMOD_CUT_LIMIT ||
       cutter->face_count>20 || cutter->vertex_count>60)return RF_RANGE;
    slot=t->count;
    status=star_mesh_planes(cutter,kernel,t->work.star_planes[slot]);if(status)return status;
    for(i=0;i<cutter->face_count;i++)if(cutter->faces[i].material==UINT32_MAX)return RF_FORMAT;
    memcpy(t->cut_vertices[slot],cutter->vertices,cutter->vertex_count*sizeof(rf_geomod_vertex));
    memcpy(t->cut_faces[slot],cutter->faces,cutter->face_count*sizeof(rf_geomod_face));
    for(i=0;i<cutter->face_count;i++)t->cut_faces[slot][i].source_face=UINT32_MAX;
    memcpy(t->kernels[slot],kernel,12);t->star_mask|=1u<<slot;
    t->cuts[slot]=(rf_geomod_mesh_view){t->cut_vertices[slot],t->cut_faces[slot],cutter->vertex_count,cutter->face_count,0};
    return terrain_publish_checked(t,t->count+1,check,context);
}
int rf_geomod_terrain_cut_star(rf_geomod_terrain *t,const rf_geomod_mesh_view *cutter,const float kernel[3])
{return terrain_cut_star_checked(t,cutter,kernel,NULL,NULL);}
static uint32_t geomod_u32(const unsigned char *p)
{return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static float geomod_float(const unsigned char *p)
{uint32_t word=geomod_u32(p);float value;memcpy(&value,&word,4);return value;}
int rf_geomod_template_decode(const void *data,uint32_t bytes,rf_geomod_template *out)
{
    const unsigned char *p=data;rf_geomod_template value={0};rf_geomod_mesh_view mesh;
    uint32_t i,j;int status;
    if(!data || !out)return RF_RANGE;
    if(bytes<28 || memcmp(p,"RFCT",4) || geomod_u32(p+4)!=1)return RF_FORMAT;
    value.face_count=geomod_u32(p+8);value.radius=geomod_float(p+12);
    if(value.face_count<4 || value.face_count>20 || bytes!=28+value.face_count*60 ||
       !isfinite(value.radius) || value.radius<=0)return RF_FORMAT;
    for(i=0;i<3;i++)value.kernel[i]=geomod_float(p+16+i*4);
    for(i=0;i<value.face_count;i++)value.faces[i]=(rf_geomod_face){i*3,3,0,UINT32_MAX};
    for(i=0;i<value.face_count*3;i++) {
        for(j=0;j<3;j++)value.vertices[i].position[j]=geomod_float(p+28+i*20+j*4);
        for(j=0;j<2;j++)value.vertices[i].uv[j]=geomod_float(p+40+i*20+j*4);
    }
    mesh=(rf_geomod_mesh_view){value.vertices,value.faces,value.face_count*3,value.face_count,0};
    status=star_mesh_planes(&mesh,value.kernel,NULL);if(status)return status;
    *out=value;return RF_OK;
}
int rf_geomod_template_load(const char *path,rf_geomod_template *out)
{
    unsigned char data[1229];FILE *file;size_t bytes;int failed;
    if(!path || !out)return RF_RANGE;
    file=fopen(path,"rb");if(!file)return RF_IO;
    bytes=fread(data,1,sizeof(data),file);failed=ferror(file);if(fclose(file))failed=1;
    if(failed)return RF_IO;
    return rf_geomod_template_decode(data,(uint32_t)bytes,out);
}
int rf_geomod_terrain_cut_template_checked(rf_geomod_terrain *t,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float scale,uint32_t material,
    const rf_geomod_shallow_limit *limits,uint32_t limit_count,rf_geomod_terrain_check_fn check,void *context)
{
    rf_geomod_vertex vertices[60];rf_geomod_face faces[20];rf_geomod_mesh_view mesh;
    float kernel[3];uint32_t i,j,k;
    if(!t || !shape || !center || !basis || material==UINT32_MAX || shape->face_count<4 || shape->face_count>20)return RF_RANGE;
    if(limit_count>2 || (limit_count && !limits))return RF_RANGE;
    if(!isfinite(scale) || scale<=0 || !isfinite(shape->radius) || shape->radius<=0)return RF_FORMAT;
    for(i=0;i<9;i++)if(!isfinite(basis[i]))return RF_FORMAT;
    for(i=0;i<3;i++)for(j=0;j<3;j++) {
        double dot=0;for(k=0;k<3;k++)dot+=(double)basis[i*3+k]*basis[j*3+k];
        if(fabs(dot-(i==j?1:0))>1e-4)return RF_FORMAT;
    }
    {double determinant=(double)basis[0]*(basis[4]*basis[8]-basis[5]*basis[7])-
        (double)basis[1]*(basis[3]*basis[8]-basis[5]*basis[6])+(double)basis[2]*(basis[3]*basis[7]-basis[4]*basis[6]);
     if(determinant<.999)return RF_FORMAT;}
    for(i=0;i<shape->face_count*3;i++) {
        for(j=0;j<3;j++)vertices[i].position[j]=(float)((((double)shape->vertices[i].position[2]*basis[6+j]+
            (double)shape->vertices[i].position[1]*basis[3+j])+(double)shape->vertices[i].position[0]*basis[j])*scale+center[j]);
        memcpy(vertices[i].uv,shape->vertices[i].uv,8);
    }
    for(j=0;j<3;j++)kernel[j]=(float)((((double)shape->kernel[2]*basis[6+j]+(double)shape->kernel[1]*basis[3+j])+
        (double)shape->kernel[0]*basis[j])*scale+center[j]);
    if(limit_count) {
        int status;
        for(i=0;i<shape->face_count*3;i++) {
            status=rf_geomod_shallow_point(center,vertices[i].position,shape->radius*scale,
                limits,limit_count,vertices[i].position);if(status)return status;
        }
        /* The strict interior point is reconstruction bookkeeping. Deform it
         * with the mesh, then let cut_star validate it before publication. */
        status=rf_geomod_shallow_point(center,kernel,shape->radius*scale,limits,limit_count,kernel);
        if(status)return status;
    }
    for(i=0;i<shape->face_count;i++)faces[i]=(rf_geomod_face){i*3,3,material,UINT32_MAX};
    mesh=(rf_geomod_mesh_view){vertices,faces,shape->face_count*3,shape->face_count,0};
    return terrain_cut_star_checked(t,&mesh,kernel,check,context);
}
int rf_geomod_terrain_cut_template_limits(rf_geomod_terrain *t,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float scale,uint32_t material,
    const rf_geomod_shallow_limit *limits,uint32_t limit_count)
{return rf_geomod_terrain_cut_template_checked(t,shape,center,basis,scale,material,limits,limit_count,NULL,NULL);}
int rf_geomod_terrain_cut_template_scale(rf_geomod_terrain *t,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float scale,uint32_t material)
{return rf_geomod_terrain_cut_template_limits(t,shape,center,basis,scale,material,NULL,0);}
int rf_geomod_terrain_cut_template(rf_geomod_terrain *t,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float radius,uint32_t material)
{
    if(!shape || !isfinite(radius) || radius<=0 || !isfinite(shape->radius) || shape->radius<=0)return RF_FORMAT;
    return rf_geomod_terrain_cut_template_scale(t,shape,center,basis,radius/shape->radius,material);
}
/* Portable committed-cutter checkpoint. Scratch doubles as rollback storage. */
typedef struct terrain_history_copy {
    rf_geomod_vertex vertices[RF_GEOMOD_CUT_LIMIT][60];
    rf_geomod_face faces[RF_GEOMOD_CUT_LIMIT][20];
    float kernels[RF_GEOMOD_CUT_LIMIT][3];
    uint32_t vc[RF_GEOMOD_CUT_LIMIT],fc[RF_GEOMOD_CUT_LIMIT],mask,count;
} terrain_history_copy;
static void history_u32(unsigned char *p,uint32_t v)
{p[0]=(unsigned char)v;p[1]=(unsigned char)(v>>8);p[2]=(unsigned char)(v>>16);p[3]=(unsigned char)(v>>24);}
static void history_float(unsigned char *p,float v)
{uint32_t word;memcpy(&word,&v,4);history_u32(p,word);}
int rf_geomod_terrain_history_size(const rf_geomod_terrain *t,uint32_t *bytes)
{
    uint32_t i,n=28;if(!t || !bytes)return RF_RANGE;
    for(i=0;i<t->count;i++)n+=24+t->cuts[i].vertex_count*20+t->cuts[i].face_count*16;
    *bytes=n;return RF_OK;
}
int rf_geomod_terrain_history_encode(const rf_geomod_terrain *t,void *data,uint32_t bytes)
{
    unsigned char *p=data;uint32_t n,i,j,k;int status;
    if(!data)return RF_RANGE;status=rf_geomod_terrain_history_size(t,&n);if(status)return status;
    if(bytes!=n)return RF_RANGE;
    memcpy(p,"RGCH",4);history_u32(p+4,1);history_u32(p+8,n);history_u32(p+12,t->count);
    history_u32(p+16,t->cavity);history_u32(p+20,t->mapping_width);history_u32(p+24,t->mapping_height);p+=28;
    for(i=0;i<t->count;i++) {
        uint32_t star=(t->star_mask>>i)&1;
        history_u32(p,star);history_u32(p+4,t->cuts[i].vertex_count);history_u32(p+8,t->cuts[i].face_count);
        for(k=0;k<3;k++)history_float(p+12+k*4,star?t->kernels[i][k]:0);p+=24;
        for(j=0;j<t->cuts[i].vertex_count;j++,p+=20) {
            for(k=0;k<3;k++)history_float(p+k*4,t->cut_vertices[i][j].position[k]);
            for(k=0;k<2;k++)history_float(p+12+k*4,t->cut_vertices[i][j].uv[k]);
        }
        for(j=0;j<t->cuts[i].face_count;j++,p+=16) {
            const rf_geomod_face *f=t->cut_faces[i]+j;
            history_u32(p,f->first);history_u32(p+4,f->count);history_u32(p+8,f->material);history_u32(p+12,f->source_face);
        }
    }
    return RF_OK;
}
static void history_exchange_bytes(void *a,void *b,size_t n)
{
    unsigned char *x=a,*y=b;size_t i;for(i=0;i<n;i++){unsigned char v=x[i];x[i]=y[i];y[i]=v;}
}
static void history_exchange(rf_geomod_terrain *t,terrain_history_copy *h)
{
    uint32_t i,v;
    history_exchange_bytes(t->cut_vertices,h->vertices,sizeof(h->vertices));
    history_exchange_bytes(t->cut_faces,h->faces,sizeof(h->faces));
    history_exchange_bytes(t->kernels,h->kernels,sizeof(h->kernels));
    for(i=0;i<RF_GEOMOD_CUT_LIMIT;i++) {
        v=t->cuts[i].vertex_count;t->cuts[i].vertex_count=h->vc[i];h->vc[i]=v;
        v=t->cuts[i].face_count;t->cuts[i].face_count=h->fc[i];h->fc[i]=v;
        t->cuts[i].vertices=t->cut_vertices[i];t->cuts[i].faces=t->cut_faces[i];t->cuts[i].generation=0;
    }
    v=t->star_mask;t->star_mask=h->mask;h->mask=v;
    v=t->count;t->count=h->count;h->count=v;
}
static int terrain_history_import(rf_geomod_terrain *t,const void *data,uint32_t bytes,
    rf_geomod_history_check_fn check,rf_geomod_history_cuts_check_fn cuts_check,void *context,uint32_t publish)
{
    const unsigned char *p=data;terrain_history_copy *h;uint32_t count,i,j,k,left;int status=RF_OK;
    uint64_t used;
    if(!t || !data)return RF_RANGE;
    if(bytes<28 || memcmp(p,"RGCH",4) || geomod_u32(p+4)!=1 || geomod_u32(p+8)!=bytes)return RF_FORMAT;
    count=geomod_u32(p+12);
    if(count>RF_GEOMOD_CUT_LIMIT || geomod_u32(p+16)!=t->cavity || geomod_u32(p+20)!=t->mapping_width ||
       geomod_u32(p+24)!=t->mapping_height)return RF_FORMAT;
    used=(uint64_t)t->base_bytes+t->tree.allocated_bytes+sizeof(*h);
    if(used>t->budget)return RF_RANGE;
    h=calloc(1,sizeof(*h));if(!h)return RF_IO;
    if(used>t->peak_bytes)t->peak_bytes=(uint32_t)used;
    h->count=count;p+=28;left=bytes-28;
    for(i=0;i<count;i++) {
        rf_geomod_mesh_view mesh;uint32_t star,n;
        if(left<24){status=RF_FORMAT;goto done;}
        star=geomod_u32(p);h->vc[i]=geomod_u32(p+4);h->fc[i]=geomod_u32(p+8);
        if(star>1 || !h->vc[i] || h->vc[i]>60 || h->fc[i]<4 || h->fc[i]>20){status=RF_FORMAT;goto done;}
        for(k=0;k<3;k++) {
            h->kernels[i][k]=geomod_float(p+12+k*4);
            if(!isfinite(h->kernels[i][k]) || (!star && h->kernels[i][k]!=0)){status=RF_FORMAT;goto done;}
        }
        if(star)h->mask|=1u<<i;p+=24;left-=24;n=h->vc[i]*20+h->fc[i]*16;
        if(left<n){status=RF_FORMAT;goto done;}
        for(j=0;j<h->vc[i];j++,p+=20) {
            for(k=0;k<3;k++)h->vertices[i][j].position[k]=geomod_float(p+k*4);
            for(k=0;k<2;k++)h->vertices[i][j].uv[k]=geomod_float(p+12+k*4);
        }
        for(j=0;j<h->fc[i];j++,p+=16) {
            rf_geomod_face *f=h->faces[i]+j;
            f->first=geomod_u32(p);f->count=geomod_u32(p+4);f->material=geomod_u32(p+8);f->source_face=geomod_u32(p+12);
            if(f->material==UINT32_MAX || f->source_face!=UINT32_MAX){status=RF_FORMAT;goto done;}
        }
        left-=n;mesh=(rf_geomod_mesh_view){h->vertices[i],h->faces[i],h->vc[i],h->fc[i],0};
        status=star?star_mesh_planes(&mesh,h->kernels[i],t->work.star_planes[i]):convex_mesh_planes(&mesh,t->work.cut_planes[i]);
        if(status)goto done;
    }
    if(left){status=RF_FORMAT;goto done;}
    /* Existing publication prepares inactive mesh/position banks and commits once.
     * Charge rollback scratch through its collision-tree allocation budget. */
    t->base_bytes+=(uint32_t)sizeof(*h);if(used>t->peak_bytes)t->peak_bytes=(uint32_t)used;
    {
        terrain_pending pending;history_exchange(t,h);status=terrain_prepare(t,count,&pending);
        if(!status) {
            if(publish)status=terrain_commit(t,&pending);
            else {
                rf_geomod_terrain_view candidate;
                candidate.mesh=pending.mesh;candidate.faces=t->faces[pending.bank];candidate.tree=&pending.tree;
                candidate.cuts=count;candidate.resident_bytes=t->base_bytes+t->tree.allocated_bytes+pending.tree.allocated_bytes;
                candidate.peak_bytes=t->peak_bytes;
                if(cuts_check){
                    rf_geomod_history_view history={t->cuts,(const float (*)[3])t->kernels,count,t->star_mask};
                    status=cuts_check(&candidate,&history,context);
                }else status=check(&candidate,context);
            }
            if(!publish || status)terrain_abort(t,&pending);
        }
        if(!publish || status)history_exchange(t,h);
    }
    t->base_bytes-=(uint32_t)sizeof(*h);
done:
    free(h);return status;
}
int rf_geomod_terrain_history_decode(rf_geomod_terrain *t,const void *data,uint32_t bytes)
{return terrain_history_import(t,data,bytes,NULL,NULL,NULL,1);}
int rf_geomod_terrain_history_check(rf_geomod_terrain *t,const void *data,uint32_t bytes,
    rf_geomod_history_check_fn check,void *context)
{if(!check)return RF_RANGE;return terrain_history_import(t,data,bytes,check,NULL,context,0);}
int rf_geomod_terrain_history_check_cuts(rf_geomod_terrain *t,const void *data,uint32_t bytes,
    rf_geomod_history_cuts_check_fn check,void *context)
{if(!check)return RF_RANGE;return terrain_history_import(t,data,bytes,NULL,check,context,0);}
int rf_geomod_terrain_reset(rf_geomod_terrain *t)
{return t?terrain_publish(t,0):RF_RANGE;}
int rf_geomod_terrain_reset_checked(rf_geomod_terrain *t,rf_geomod_terrain_check_fn check,void *context)
{return t?terrain_publish_checked(t,0,check,context):RF_RANGE;}
int rf_geomod_terrain_get(const rf_geomod_terrain *t,rf_geomod_terrain_view *out)
{
    rf_geomod_terrain_view value;if(!t || !out)return RF_RANGE;
    rf_geomod_storage_view(t->mesh,&value.mesh);value.faces=t->faces[t->bank];value.tree=&t->tree;
    value.cuts=t->count;value.resident_bytes=t->base_bytes+t->tree.allocated_bytes;value.peak_bytes=t->peak_bytes;
    *out=value;return RF_OK;
}
