#include "rf/event.h"
#include "rf/level.h"
#include <math.h>
#include <string.h>
/* Original type table 5a1a3c..5a1ba4, lookup 4bd700. */
static const char *const event_names[90]={
    "Play_Sound", /* 0 */
    "Slay_Object", /* 1 */
    "Remove_Object", /* 2 */
    "Invert", /* 3 */
    "Teleport", /* 4 */
    "Goto", /* 5 */
    "Goto_Player", /* 6 */
    "Look_At", /* 7 */
    "Shoot_At", /* 8 */
    "Shoot_Once", /* 9 */
    "Explode", /* 10 */
    "Play_Animation", /* 11 */
    "Play_Custom_Animation", /* 12 */
    "Heal", /* 13 */
    "Armor", /* 14 */
    "Message", /* 15 */
    "When_Dead", /* 16 */
    "Continuous_Damage", /* 17 */
    "Shake_Player", /* 18 */
    "Give_Item_To_Player", /* 19 */
    "Cyclic_Timer", /* 20 */
    "Switch_Model", /* 21 */
    "Load_Level", /* 22 */
    "Spawn_Object", /* 23 */
    "Make_Invulnerable", /* 24 */
    "Make_Walk", /* 25 */
    "Make_Fly", /* 26 */
    "Drop_Point_Marker", /* 27 */
    "Follow_Waypoints", /* 28 */
    "Follow_Player", /* 29 */
    "Set_Friendliness", /* 30 */
    "Set_Light_State", /* 31 */
    "Switch", /* 32 */
    "Swap_Textures", /* 33 */
    "Set_AI_Mode", /* 34 */
    "Goal_Create", /* 35 */
    "Goal_Check", /* 36 */
    "Goal_Set", /* 37 */
    "Attack", /* 38 */
    "Particle_State", /* 39 */
    "Set_Liquid_Depth", /* 40 */
    "Music_Start", /* 41 */
    "Music_Stop", /* 42 */
    "Bolt_State", /* 43 */
    "Set_Gravity", /* 44 */
    "Alarm_Siren", /* 45 */
    "Alarm", /* 46 */
    "Go_Undercover", /* 47 */
    "Delay", /* 48 */
    "Monitor_State", /* 49 */
    "UnHide", /* 50 */
    "Push_Region_State", /* 51 */
    "When_Hit", /* 52 */
    "Headlamp_State", /* 53 */
    "Item_Pickup_State", /* 54 */
    "Cutscene", /* 55 */
    "Strip_Player_Weapons", /* 56 */
    "Fog_State", /* 57 */
    "Detach", /* 58 */
    "Skybox_State", /* 59 */
    "Force_Monitor_Update", /* 60 */
    "Black_Out_Player", /* 61 */
    "Turn_Off_Physics", /* 62 */
    "Teleport_Player", /* 63 */
    "Holster_Weapon", /* 64 */
    "Holster_Player_Weapon", /* 65 */
    "Modify_Rotating_Mover", /* 66 */
    "Clear_Endgame_If_Killed", /* 67 */
    "Win_PS2_Demo", /* 68 */
    "Enable_Navpoint", /* 69 */
    "Play_Vclip", /* 70 */
    "Endgame", /* 71 */
    "Mover_Pause", /* 72 */
    "Countdown_Begin", /* 73 */
    "Countdown_End", /* 74 */
    "When_Countdown_Over", /* 75 */
    "Activate_Capek_Shield", /* 76 */
    "When_Enter_Vehicle", /* 77 */
    "When_Try_Exit_Vehicle", /* 78 */
    "Fire_Weapon_No_Anim", /* 79 */
    "Never_Leave_Vehicle", /* 80 */
    "Drop_Weapon", /* 81 */
    "Ignite_Entity", /* 82 */
    "When_Cutscene_Over", /* 83 */
    "When_Countdown_Reaches", /* 84 */
    "Display_Fullscreen_Image", /* 85 */
    "Defuse_Nuke", /* 86 */
    "When_Life_Reaches", /* 87 */
    "When_Armor_Reaches", /* 88 */
    "Reverse_Mover", /* 89 */
};
int32_t rf_event_type_id(const char *name)
{
    uint32_t i,j;
    if(!name)return -1;
    for(i=0;i<90;++i) {
        for(j=0;;++j) {
            unsigned char a=(unsigned char)name[j],b=(unsigned char)event_names[i][j];
            if(a>='A' && a<='Z')a+=32;
            if(b>='A' && b<='Z')b+=32;
            if(a!=b)break;
            if(!a)return (int32_t)i;
        }
    }
    return -1;
}
/* Exact binary32 seconds * 1000, truncated toward zero. Integer arithmetic
 * avoids dependence on an ambient x87 precision mode (0.01f is below 10 ms).
 * The original extended-precision product is exact before its ftol call. */
static int trigger_milliseconds(float seconds,int32_t *result)
{
    uint32_t raw,exponent,shift;uint64_t magnitude;
    memcpy(&raw,&seconds,4);exponent=(raw>>23)&255;
    if(exponent==255)return RF_RANGE;
    magnitude=((raw&0x7fffff)|(exponent?0x800000:0))*UINT64_C(1000);
    if(!exponent)exponent=1;
    if(exponent>150) {
        shift=exponent-150;if(shift>=31)return RF_RANGE;
        magnitude<<=shift;
    } else {shift=150-exponent;magnitude=shift>=64?0:magnitude>>shift;}
    if(magnitude>((raw>>31)?UINT64_C(2147483648):(uint64_t)RF_TIMER_PERIOD))return RF_RANGE;
    *result=(raw>>31)?(int32_t)(-(int64_t)magnitude):(int32_t)magnitude;
    return RF_OK;
}
int rf_auto_trigger_init(rf_auto_trigger_state *s,const rf_level_trigger *record,
    uint32_t handle,int32_t now)
{
    static const uint32_t bits[5]={1,2,4,8,128};
    rf_auto_trigger_state value={0};uint32_t i;int status;
    if(!s || !record || record->shape>1 || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    status=trigger_milliseconds(record->timing,&value.cooldown_ms);if(status)return status;
    for(i=0;i<5;++i)if(record->flags[i]==1)value.flags|=bits[i];
    if(record->shape==1 && record->box_flag==1)value.flags|=32;
    if(record->tail_flag)value.flags|=16;
    value.deadline=now;
    value.activation_time_bits=UINT32_C(0xbf800000);value.handle=handle;
    *s=value;return RF_OK;
}
int rf_auto_trigger_fire(rf_auto_trigger_state *s,int32_t now,uint32_t clock_bits,
    int eligible,rf_auto_trigger_callback callback,void *context)
{
    int32_t deadline;int status;
    if(!s || !callback)return RF_RANGE;
    if(!eligible || !(s->flags&8) || (s->flags&16))return RF_OK;
    if(now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    deadline=s->deadline;
    if(s->cooldown_ms>0) {
        status=rf_timer_set(&deadline,now,s->cooldown_ms);if(status)return status;
    }
    callback(context,s,UINT32_MAX,0);
    ++s->count;s->deadline=deadline;s->activation_time_bits=clock_bits;s->flags|=64;
    return RF_OK;
}
int rf_event_gravity_action(rf_physics_gravity *gravity,float value,uint32_t action)
{
    if(!gravity || action>2)return RF_RANGE;
    return action==1?rf_physics_gravity_set(gravity,value):RF_OK;
}
static int propagates(uint32_t type)
{
    return type!=2 && type!=3 && type!=32 && type!=36 && type!=66 && type!=69 && type!=89;
}
int rf_event_activate(rf_event_state *s,int32_t now,uint32_t source,uint32_t actor,
    uint32_t mode,rf_event_callback callback,void *context)
{
    double milliseconds;int32_t deadline=-1;int status;
    if(!s || !callback || now<0 || now>RF_TIMER_PERIOD)return RF_RANGE;
    mode&=255;
    /* Validate before mutation; original valid inputs use x87 intermediates. */
    if(!(s->flags&1)) {
        if(!isfinite(s->delay))return RF_RANGE;
        if(s->delay>0) {
            milliseconds=(double)s->delay*(s->type==79?(double)0.9827237725257874f:1.0)*1000.0+0.5;
            if(milliseconds>=((double)RF_TIMER_PERIOD+1.0))return RF_RANGE;
            status=rf_timer_set(&deadline,now,(int32_t)milliseconds);if(status)return status;
        }
    }
    s->actor=actor;s->source=source;
    if(s->flags&1)return RF_OK;
    if(s->delay>0) {s->deadline=deadline;s->mode=mode;return RF_OK;}
    rf_timer_clear(&s->deadline);
    callback(context,s,mode==1?1:0,source,actor,mode);
    if(propagates(s->type))callback(context,s,2,source,s->actor,mode);
    return RF_OK;
}
int rf_event_tick(rf_event_state *s,int32_t now,rf_event_callback callback,void *context)
{
    int expired,status;if(!s || !callback)return RF_RANGE;
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(!expired)return RF_OK;
    callback(context,s,(s->mode&255)?1:0,s->source,s->actor,s->mode&255);
    if(propagates(s->type))callback(context,s,2,s->source,s->actor,s->mode&255);
    rf_timer_clear(&s->deadline);return RF_OK;
}
int rf_unhide_init(rf_unhide_state *s,int32_t now)
{
    int32_t deadline;int status;if(!s)return RF_RANGE;
    status=rf_timer_set(&deadline,now,0);if(status)return status;
    s->deadline=deadline;s->on=0;s->off=0;return RF_OK;
}
int rf_unhide_request(rf_unhide_state *s,int unhide)
{
    if(!s)return RF_RANGE;
    if(unhide)s->on=1;else s->off=1;
    return RF_OK;
}
int rf_unhide_tick(rf_unhide_state *s,int32_t now,const uint32_t *links,
    uint32_t count,rf_unhide_target_callback callback,void *context)
{
    uint32_t i;int expired,status,processed=1;
    if(!s || !callback || (count && !links))return RF_RANGE;
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(s->on==1 && expired) {
        status=rf_timer_set(&s->deadline,now,500);if(status)return status;
        for(i=0;i<count;++i)if(!callback(context,links[i],1))processed=0;
        if(processed)s->on=0;
    }
    status=rf_timer_expired(s->deadline,now,&expired);if(status)return status;
    if(s->off==1 && expired) {
        status=rf_timer_set(&s->deadline,now,500);if(status)return status;
        for(i=0;i<count;++i)(void)callback(context,links[i],0);
        s->off=0;
    }
    return RF_OK;
}
