#include "rf/effect.h"
#include <math.h>
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
