#ifndef RF_SCENE_WEAPON_CUSTOM_ACTIONS_INC
#define RF_SCENE_WEAPON_CUSTOM_ACTIONS_INC
/* Authored custom actions for the two-magazine SMG and silenced handgun.
 * Include rf/player_weapon.h, stdlib.h, string.h first. The owner temporarily
 * transfers a payload into unused FP slot3. Close it BEFORE closing that view.
 * No gameplay mode/weapon-model change is implied by playing an action. */
typedef struct scene_weapon_custom_actions {
    rf_player_weapon *view;rf_motion_file files[2];void *payloads[2];
    rf_motion_playback_resource resources[2];uint32_t count,action[2],old_count;
    uint32_t resident_bytes;int32_t active;
} scene_weapon_custom_actions;
/* profile0 Machine Pistol,1 Machine Pistol Special,2 Undercover handgun. */
static int scene_weapon_custom_actions_open(rf_player_weapon *w,rf_vpp *motions,
    uint32_t profile,uint32_t budget,scene_weapon_custom_actions **result)
{
    static const char *names[3][2]={{"fp_mp_switchback.rfa","fp_mp_switchback.rfa"},
        {"fp_mp_switch.rfa","fp_mp_switch.rfa"},{"FP_glock_silencer_add.rfa","FP_glock_silencer_off.rfa"}};
    scene_weapon_custom_actions *o;uint32_t i,j;int status=RF_OK;
    if(!w || !motions || !result || *result || profile>2 || w->payloads[3] || w->clip_count>3 || !w->bone_count)return RF_RANGE;
    if(budget<sizeof(*o))return RF_RANGE;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;
    o->view=w;o->active=-1;o->old_count=w->clip_count;o->resident_bytes=sizeof(*o);
    o->count=profile==2?2:1;o->action[0]=0;o->action[1]=o->count-1;
    for(i=0;i<o->count;i++) {
        rf_motion_track track;
        status=rf_motion_file_open(o->files+i,motions,names[profile][i]);if(status)goto fail;
        if(o->files[i].entry.size>budget-o->resident_bytes){status=RF_RANGE;goto fail;}
        o->payloads[i]=malloc(o->files[i].entry.size);if(!o->payloads[i]){status=RF_IO;goto fail;}
        status=rf_vpp_read(motions,&o->files[i].entry,0,o->payloads[i],o->files[i].entry.size);if(status)goto fail;
        status=rf_motion_file_bind_memory(o->files+i,o->payloads[i],o->files[i].entry.size);if(status)goto fail;
        for(j=0;j<w->bone_count;j++){status=rf_motion_file_track(o->files+i,j,&track);if(status)goto fail;}
        status=rf_motion_file_track(o->files+i,0,&track);if(status)goto fail;
        o->resources[i].comparison=track.envelope;o->files[i].archive=NULL;
        o->resident_bytes+=o->files[i].entry.size;
    }
    *result=o;return RF_OK;
fail:
    for(i=0;i<2;i++)free(o->payloads[i]);
    free(o);return status;
}
static int scene_weapon_custom_actions_detach(scene_weapon_custom_actions *o,rf_player_weapon *w)
{
    int status;if(!o || !w || o->view!=w)return RF_RANGE;
    if(o->active<0)return RF_OK;
    if(w->payloads[3]!=(void*)o->files[o->active].resident)return RF_RANGE;
    status=rf_player_weapon_step(w,0,0);if(status)return status;
    o->payloads[o->active]=w->payloads[3];w->payloads[3]=NULL;
    memset(w->clips+3,0,sizeof(w->clips[3]));memset(w->resources+3,0,sizeof(w->resources[3]));
    w->clip_count=o->old_count;o->active=-1;return RF_OK;
}
/* enter=0 selects authored custom_start, leave=1 custom_leave. */
static int scene_weapon_custom_actions_start(scene_weapon_custom_actions *o,rf_player_weapon *w,uint32_t action)
{
    uint32_t index;int status;
    if(!o || !w || o->view!=w || action>1)return RF_RANGE;
    status=scene_weapon_custom_actions_detach(o,w);if(status)return status;
    if(w->payloads[3] || w->clip_count!=o->old_count)return RF_RANGE;
    index=o->action[action];w->clips[3]=o->files[index];w->payloads[3]=o->payloads[index];o->payloads[index]=NULL;
    w->resources[3]=o->resources[index];w->clip_count=4;o->active=(int32_t)index;
    return rf_player_weapon_step(w,3,0);
}
static int scene_weapon_custom_actions_close(scene_weapon_custom_actions **owner,rf_player_weapon *w)
{
    scene_weapon_custom_actions *o;uint32_t i;int status;if(!owner || !(o=*owner))return RF_OK;
    status=scene_weapon_custom_actions_detach(o,w);if(status)return status;
    for(i=0;i<2;i++)free(o->payloads[i]);
    free(o);*owner=NULL;return RF_OK;
}
#endif
