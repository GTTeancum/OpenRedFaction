#include "rf/effect.h"
#include <math.h>
#include <float.h>
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
    if((mode&31u)!=2)return RF_NOT_FOUND;
    value.writes[0].value=lod_bias;
    value.writes[5].value=color==3?7u:color==4?5u:color==2?4u:2u;
    value.writes[8].value=alpha==3?4u:2u;
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
