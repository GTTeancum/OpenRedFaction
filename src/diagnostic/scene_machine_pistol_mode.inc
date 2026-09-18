/* Base ownership authorizes the paired mode; never grant separate ownership or
 * copy ammunition. Base uses30 rounds of12mm; Special uses20 of5.65mm AP.
 * Parent uses output.weapon to select definition, loaded[] and mapped reserve,
 * and retains base ID for ownership, cycling and pickup selection.
 * Practical transition: alternate rising edge requests source custom action,
 * suppress shooting/reload until its authored duration ends, then commit mode.
 * Reload/inhibit consumes edges and cancels pending transitions; deselection
 * cancels pending work but retains completed mode. Reset only for new player.
 * Parent must supply usable special resources before enabling toggles. */
#include "rf/entity_assets.h"
typedef struct scene_machine_pistol_mode_state {
    uint32_t special,pending,target,due,held;
} scene_machine_pistol_mode_state;
typedef struct scene_machine_pistol_mode_event {
    uint32_t started,changed,blocked,special,action;int32_t weapon;
} scene_machine_pistol_mode_event;
static void scene_machine_pistol_mode_reset(scene_machine_pistol_mode_state *state)
{scene_machine_pistol_mode_state empty={0};if(state)*state=empty;}

/* Compiled motion units4800Hz -> simulation60Hz. No resident payload or heap. */
static int scene_machine_pistol_mode_timing(rf_vpp *motions,uint32_t ticks[2])
{
    const char *names[2]={"fp_mp_switchback.rfa","fp_mp_switch.rfa"};
    uint32_t result[2],i;int status;
    if(!motions || !ticks)return RF_RANGE;
    for(i=0;i<2;i++){
        rf_motion_file motion;int64_t span;
        status=rf_motion_file_open(&motion,motions,names[i]);if(status)return status;
        span=(int64_t)(int32_t)motion.header[5]-(int32_t)motion.header[4];
        if(span<=0 || span>4800*60)return RF_RANGE;
        result[i]=(uint32_t)((span+79)/80);
    }
    ticks[0]=result[0];ticks[1]=result[1];return RF_OK;
}
static int scene_machine_pistol_mode_tick(scene_machine_pistol_mode_state *state,
    const rf_weapon_inventory *inventory,int32_t base_weapon,int32_t special_weapon,
    const uint32_t transition_ticks[2],uint32_t frame,uint32_t selected,
    uint32_t inhibited,uint32_t alternate,scene_machine_pistol_mode_event *out)
{
    scene_machine_pistol_mode_event event={0};uint32_t pressed;
    if(!state || !inventory || !transition_ticks || !out || base_weapon<0 || base_weapon>=64 ||
       special_weapon<0 || special_weapon>=64 || base_weapon==special_weapon ||
       !transition_ticks[0] || !transition_ticks[1] || transition_ticks[0]>3600 || transition_ticks[1]>3600)
        return RF_RANGE;
    pressed=alternate && !state->held;state->held=alternate!=0;
    if(!selected || inhibited || !inventory->owned[base_weapon])state->pending=0;
    else if(state->pending){
        if((int32_t)(frame-state->due)>=0){state->special=state->target;state->pending=0;event.changed=1;}
    }else if(pressed){
        state->target=!state->special;state->pending=1;
        state->due=frame+transition_ticks[state->special?1:0];
        event.started=1;event.action=state->special?11:10;
    }
    event.special=state->special;event.weapon=state->special?special_weapon:base_weapon;
    event.blocked=state->pending || !selected || inhibited || !inventory->owned[base_weapon];
    *out=event;return RF_OK;
}
