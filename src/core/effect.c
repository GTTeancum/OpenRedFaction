#include "rf/effect.h"
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
