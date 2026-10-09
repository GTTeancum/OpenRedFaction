#ifndef RF_SCENE_VEHICLE_COCKPIT_HUD_INC
#define RF_SCENE_VEHICLE_COCKPIT_HUD_INC
/* Original RF1.20 4a79f0 selects APC/Fighter 4a7a80 or submarine 4a8080.
 * These are authored VFX surface replacements, not screen-space gauges.
 * 4a7d80/4a7e00/4a82a0 supply the VBM banks; 4a7eb0 extracts decimal digits.
 * The original animated bitmap handle precedes its frames (50f961/50fa7f),
 * so its +1 based frame indices become zero-based retained image indices.
 * Gameplay is read-only. No allocation, texture upload or state change occurs
 * when selecting the already resident frames for a presentation. */
enum scene_vehicle_cockpit_channel {
    SCENE_COCKPIT_STATIC=0,SCENE_COCKPIT_HEALTH_GAUGE,SCENE_COCKPIT_PRIMARY_GAUGE,
    SCENE_COCKPIT_SECONDARY_GAUGE,SCENE_COCKPIT_HEALTH_100,SCENE_COCKPIT_HEALTH_10,
    SCENE_COCKPIT_HEALTH_1,SCENE_COCKPIT_PRIMARY_1000,SCENE_COCKPIT_PRIMARY_100,
    SCENE_COCKPIT_PRIMARY_10,SCENE_COCKPIT_PRIMARY_1,SCENE_COCKPIT_SECONDARY_100,
    SCENE_COCKPIT_SECONDARY_10,SCENE_COCKPIT_SECONDARY_1,SCENE_COCKPIT_BANK_LEFT,
    SCENE_COCKPIT_BANK_RIGHT
};
typedef struct scene_vehicle_cockpit_hud {
    uint8_t channel[64],frame[64];uint32_t profile,bound,ready;
} scene_vehicle_cockpit_hud;
uint32_t rf_scene_vehicle_cockpit_hud[16];
/* profile,bound materials,presentations,bound emitted faces,health,primary,
 * secondary,health gauge,primary gauge,secondary gauge,health digit nibbles,
 * primary digit nibbles,secondary digit nibbles,status,texture count,channels */
static uint32_t scene_vehicle_cockpit_hud_channel(const char *name,uint32_t profile)
{
    static const char *const names[]={"cockpit_armor.tga","cockpit_primary.tga","cockpit_secondary.tga",
        "cocpit_armor_digit1.tga","cocpit_armor_digit2.tga","cocpit_armor_digit3.tga",
        "cocpit_primary_digit1.tga","cocpit_primary_digit2.tga","cocpit_primary_digit3.tga","cocpit_primary_digit4.tga",
        "cocpit_second_digit1.tga","cocpit_second_digit2.tga","cocpit_second_digit3.tga",
        "cocpit_armor_L_Bank.tga","cocpit_armor_R_Bank.tga"};
    uint32_t i;for(i=0;i<sizeof(names)/sizeof(*names);i++)if(!strcmp(name,names[i])){
        uint32_t channel=i+1;
        /* Sub has three primary digits; APC/Fighter have four. */
        if(profile==4 && channel>=SCENE_COCKPIT_PRIMARY_1000 && channel<=SCENE_COCKPIT_PRIMARY_10)++channel;
        return channel;
    }
    return SCENE_COCKPIT_STATIC;
}
static const char *scene_vehicle_cockpit_hud_bitmap(uint32_t profile,uint32_t channel)
{
    if(channel>=SCENE_COCKPIT_HEALTH_100 && channel<=SCENE_COCKPIT_SECONDARY_1)
        return profile==2?"apc_digits.vbm":profile==4?"sub_digits.vbm":"Fighter01_digits.vbm";
    if(channel==SCENE_COCKPIT_HEALTH_GAUGE)return profile==4?"sub_armor.vbm":"apc_armor.vbm";
    if(channel==SCENE_COCKPIT_PRIMARY_GAUGE)return profile==4?"sub_primary.vbm":"apc_primary.vbm";
    if(channel==SCENE_COCKPIT_SECONDARY_GAUGE)return "apc_secondary.vbm";
    if(channel==SCENE_COCKPIT_BANK_LEFT)return profile==4?"sub_lb.vbm":"fighter01_lb.vbm";
    if(channel==SCENE_COCKPIT_BANK_RIGHT)return profile==4?"sub_rb.vbm":"fighter01_rb.vbm";
    return NULL;
}
static int scene_vehicle_cockpit_hud_open(scene_vehicle_cockpit_hud *hud,rf_vfx_asset_materials *m,
    const char *name,rf_vpp *maps,uint32_t map_count,uint32_t budget)
{
    uint32_t i;int status;
    hud->profile=!strcmp(name,"APC.vfx")?2:!strcmp(name,"sub.vfx")?4:!strcmp(name,"fighter01.vfx")?5:0;
    if(!hud->profile)return RF_OK;
    if(!m || m->count>64 || budget<sizeof(*m))return RF_RANGE;
    for(i=0;i<m->count;i++){
        char *bitmap=(char *)(m->views[i].words+5);
        uint32_t channel=scene_vehicle_cockpit_hud_channel(bitmap,hud->profile);
        const char *replacement=scene_vehicle_cockpit_hud_bitmap(hud->profile,channel);
        if(!replacement)continue;
        hud->channel[i]=(uint8_t)channel;++hud->bound;
        memset(bitmap,0,33);strcpy(bitmap,replacement);m->views[i].bitmap_requests|=1;
    }
    if(!hud->bound)return RF_NOT_FOUND;
    /* Release placeholder pixels before loading the real banks, preserving
     * the caller's existing total budget and avoiding duplicate residency.
     * Geometry and material tracks remain unchanged; names are loader-owned. */
    rf_vfx_material_textures_close(&m->textures);
    status=rf_vfx_material_textures_open(&m->textures,m->views,m->count,maps,map_count,
        budget-(uint32_t)sizeof(*m)+(uint32_t)sizeof(m->textures));if(status)return status;
    m->resident_bytes=(uint32_t)sizeof(*m)+m->textures.resident_bytes-(uint32_t)sizeof(m->textures);
    for(i=0;i<m->count;i++)if(hud->channel[i]){
        uint32_t slot=m->textures.bindings[i][0],minimum=hud->channel[i]<=SCENE_COCKPIT_SECONDARY_GAUGE?2:
            hud->channel[i]>=SCENE_COCKPIT_BANK_LEFT?2:10;
        if(slot>=m->textures.texture_count || m->textures.textures[slot].animation.count<minimum)return RF_FORMAT;
    }
    for(i=0;i<m->count;i++)if(hud->channel[i]>=SCENE_COCKPIT_BANK_LEFT)hud->frame[i]=1;
    hud->ready=1;return RF_OK;
}
static uint32_t scene_vehicle_cockpit_hud_integer(float value)
{return !isfinite(value)||value<=0?0:value>=99999?99999:(uint32_t)value;}
static uint32_t scene_vehicle_cockpit_hud_gauge(float value,float maximum,uint32_t count)
{
    /* Original4a7ef0/4a7f60/4a7ff0 use ceil(ratio*(count-1)), clamped.
     * 57385b selects upward x87 rounding; digits use floor at573e83. */
    if(count<2 || !isfinite(value)||!isfinite(maximum)||value<=0||maximum<=0)return 0;
    if(value>=maximum)return count-1;
    return (uint32_t)ceilf((value/maximum)*(float)(count-1));
}
static void scene_vehicle_cockpit_hud_state(scene_vehicle_cockpit_hud *hud,const rf_vfx_asset_materials *m,
    float health,float maximum,const int32_t ammo[2],const uint32_t capacity[2])
{
    static const uint32_t divisors[10]={100,10,1,1000,100,10,1,100,10,1};
    uint32_t i,values[3]={scene_vehicle_cockpit_hud_integer(health),ammo[0]>0?(uint32_t)ammo[0]:0,ammo[1]>0?(uint32_t)ammo[1]:0};
    memset(rf_scene_vehicle_cockpit_hud+7,0,7*sizeof(uint32_t));
    rf_scene_vehicle_cockpit_hud[0]=hud->profile;rf_scene_vehicle_cockpit_hud[1]=hud->bound;
    ++rf_scene_vehicle_cockpit_hud[2];rf_scene_vehicle_cockpit_hud[3]=0;
    memcpy(rf_scene_vehicle_cockpit_hud+4,values,sizeof(values));
    rf_scene_vehicle_cockpit_hud[14]=m->textures.texture_count;rf_scene_vehicle_cockpit_hud[15]=0;
    hud->ready=0;
    for(i=0;i<m->count;i++)if(hud->channel[i]){
        uint32_t channel=hud->channel[i],slot=m->textures.bindings[i][0],frame=0;
        if(slot>=m->textures.texture_count){rf_scene_vehicle_cockpit_hud[13]=(uint32_t)RF_FORMAT;return;}
        uint32_t count=m->textures.textures[slot].animation.count;
        if(channel<=SCENE_COCKPIT_SECONDARY_GAUGE){uint32_t value=channel-1;
            frame=scene_vehicle_cockpit_hud_gauge(value?(float)values[value]:health,value?(float)capacity[value-1]:maximum,count);
            rf_scene_vehicle_cockpit_hud[7+value]=frame;
        }else if(channel<=SCENE_COCKPIT_SECONDARY_1){
            uint32_t digit=channel-SCENE_COCKPIT_HEALTH_100,value=digit<3?0:digit<7?1:2;
            frame=(values[value]/divisors[digit])%10;
            uint32_t shift=divisors[digit]==1000?12:divisors[digit]==100?8:divisors[digit]==10?4:0;
            rf_scene_vehicle_cockpit_hud[10+value]|=frame<<shift;
        }else frame=1; /* Original idle bank frame; no invented tube alternation. */
        if(frame>=count){rf_scene_vehicle_cockpit_hud[13]=(uint32_t)RF_RANGE;return;}
        hud->frame[i]=(uint8_t)frame;rf_scene_vehicle_cockpit_hud[15]|=1u<<channel;
    }
    hud->ready=1;
}
#endif
