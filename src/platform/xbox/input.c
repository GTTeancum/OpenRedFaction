/* Xbox platform input policy; not recovered Red Faction input mapping. */
#include "input.h"
#include <SDL.h>
#include <math.h>
#include <string.h>
static SDL_GameController *controller;
static int initialized;
/* This owner deliberately outlives controller/session open and close. */
static rf_xbox_input_options input_options={
    RF_XBOX_LOOK_SENSITIVITY_DEFAULT,0,RF_XBOX_STICK_DEADZONE_DEFAULT,RF_XBOX_STICK_LAYOUT_STANDARD,0,RF_TEXT_ENGLISH,
    RF_XBOX_VOLUME_DEFAULT,RF_XBOX_VOLUME_DEFAULT,RF_XBOX_VOLUME_DEFAULT
};
static uint32_t pause_mode,pause_edge,start_held,release_controls,menu_held,menu_edge;
static uint32_t disconnect_edge,exit_rearm,alt_sample_valid,binding_rearm,chord_controls,binding_alt_rearm;
static uint32_t bindings[RF_XBOX_BIND_COUNT]={0,1,2,3,4,5,6,7,8,9};
enum {CONTROL_RIGHT=1u<<10,CONTROL_LEFT=1u<<11,CONTROL_UP=1u<<12,CONTROL_DOWN=1u<<13};
#include "input_feedback.inc"
enum {
    INPUT_FIRE=1u,INPUT_ALT=2u,INPUT_JUMP=4u,INPUT_USE=8u,
    INPUT_RELOAD=16u,INPUT_CROUCH=32u,INPUT_NEXT=64u,INPUT_PREVIOUS=128u,
    INPUT_UP=256u,INPUT_DOWN=512u,INPUT_FINE_AIM=1024u,INPUT_HOLSTER=2048u
};
/* Factoradic rank gives exactly 10! collision-free maps in one uint32.
 * The rank is serialized, never SDL enum values or a native struct. */
static void input_decode_bindings(uint32_t rank)
{
    uint32_t pool[RF_XBOX_BIND_COUNT]={0,1,2,3,4,5,6,7,8,9},factor=362880u,i,j;
    for(i=0;i<RF_XBOX_BIND_COUNT;i++){
        uint32_t index=rank/factor;rank%=factor;bindings[i]=pool[index];
        for(j=index;j+1<RF_XBOX_BIND_COUNT-i;j++)pool[j]=pool[j+1];
        if(i+1<RF_XBOX_BIND_COUNT)factor/=RF_XBOX_BIND_COUNT-i-1;
    }
}
static uint32_t input_encode_bindings(const uint32_t *map)
{
    uint32_t rank=0,i,j;
    for(i=0;i<RF_XBOX_BIND_COUNT;i++){
        uint32_t less=0;for(j=i+1;j<RF_XBOX_BIND_COUNT;j++)if(map[j]<map[i])++less;
        rank=rank*(RF_XBOX_BIND_COUNT-i)+less;
    }
    return rank;
}
const char *rf_xbox_input_action_label(uint32_t action)
{
    static const char *const labels[RF_XBOX_BIND_COUNT]={
        "FIRE","ALT FIRE","JUMP","USE","RELOAD","CROUCH","NEXT WEAPON","PREVIOUS WEAPON","FINE AIM","HOLSTER"
    };
    return action<RF_XBOX_BIND_COUNT?labels[action]:NULL;
}
const char *rf_xbox_input_binding_label(uint32_t action)
{
    static const char *const labels[RF_XBOX_BUTTON_COUNT]={
        "RT","LT","A","X","Y","B","BLACK","WHITE","RIGHT-STICK CLICK","LEFT-STICK CLICK"
    };
    static const char *const held[RF_XBOX_BUTTON_COUNT]={
        "HOLD RT","HOLD LT","HOLD A","HOLD X","HOLD Y","HOLD B","HOLD BLACK","HOLD WHITE",
        "HOLD RIGHT-STICK CLICK","HOLD LEFT-STICK CLICK"
    };
    if(action>=RF_XBOX_BIND_COUNT)return NULL;
    return action==RF_XBOX_BIND_FINE_AIM?held[bindings[action]]:labels[bindings[action]];
}
const char *rf_xbox_input_binding_short_label(uint32_t action)
{
    static const char *const labels[RF_XBOX_BUTTON_COUNT]={"RT","LT","A","X","Y","B","BLACK","WHITE","R3","L3"};
    return action<RF_XBOX_BIND_COUNT?labels[bindings[action]]:NULL;
}
uint32_t rf_xbox_input_binding_control(uint32_t action)
{return action<RF_XBOX_BIND_COUNT?bindings[action]:UINT32_MAX;}
int rf_xbox_input_rebind(uint32_t action,uint32_t control)
{
    rf_xbox_input_options next=input_options;uint32_t map[RF_XBOX_BIND_COUNT],i;
    if(action>=RF_XBOX_BIND_COUNT || control>=RF_XBOX_BUTTON_COUNT)return RF_RANGE;
    memcpy(map,bindings,sizeof(map));
    for(i=0;i<RF_XBOX_BIND_COUNT;i++)if(map[i]==control)break;
    if(i==RF_XBOX_BIND_COUNT)return RF_FORMAT;
    map[i]=map[action];map[action]=control;
    next.button_map=input_encode_bindings(map);rf_xbox_input_set_options(&next);return RF_OK;
}
volatile uint32_t rf_player_input_diagnostic[6]={0x5246494eu};
volatile uint32_t rf_xbox_input_menu_observation;
void rf_xbox_input_get_options(rf_xbox_input_options *options)
{
    if(options)*options=input_options;
}
void rf_xbox_input_set_options(const rf_xbox_input_options *options)
{
    rf_xbox_input_options next;
    if(!options)return;
    next=*options;
    if(next.look_sensitivity_percent<RF_XBOX_LOOK_SENSITIVITY_MIN)
        next.look_sensitivity_percent=RF_XBOX_LOOK_SENSITIVITY_MIN;
    if(next.look_sensitivity_percent>RF_XBOX_LOOK_SENSITIVITY_MAX)
        next.look_sensitivity_percent=RF_XBOX_LOOK_SENSITIVITY_MAX;
    if(next.deadzone_percent>RF_XBOX_STICK_DEADZONE_MAX)
        next.deadzone_percent=RF_XBOX_STICK_DEADZONE_MAX;
    next.invert_y=next.invert_y!=0;
    if(next.stick_layout!=RF_XBOX_STICK_LAYOUT_SOUTHPAW)next.stick_layout=RF_XBOX_STICK_LAYOUT_STANDARD;
    if(next.button_map>=RF_XBOX_BUTTON_PERMUTATIONS)next.button_map=0;
    if(next.text_language>=RF_TEXT_LANGUAGE_COUNT)next.text_language=RF_TEXT_ENGLISH;
    if(next.master_volume_percent>RF_XBOX_VOLUME_MAX)next.master_volume_percent=RF_XBOX_VOLUME_MAX;
    if(next.effects_volume_percent>RF_XBOX_VOLUME_MAX)next.effects_volume_percent=RF_XBOX_VOLUME_MAX;
    if(next.music_volume_percent>RF_XBOX_VOLUME_MAX)next.music_volume_percent=RF_XBOX_VOLUME_MAX;
    if(next.button_map!=input_options.button_map){
        /* Held controls cannot become fresh Fire/Alt/Holster after an edit,
         * even if the setter is called outside a paused menu. */
        binding_rearm=binding_alt_rearm=1;alt_sample_valid=0;input_decode_bindings(next.button_map);
    }
    input_options=next;
}
uint32_t rf_xbox_input_pause_pressed(void){return pause_edge;}
uint32_t rf_xbox_input_connected(void){return controller!=NULL;}
uint32_t rf_xbox_input_connection_lost(void){return disconnect_edge;}
uint32_t rf_xbox_input_alt_sample_suppressed(void){return initialized&&!alt_sample_valid;}
uint32_t rf_xbox_input_menu_pressed(void){return menu_edge;}
void rf_xbox_input_set_paused(uint32_t paused)
{pause_mode=paused!=0;if(pause_mode)input_rumble_cancel();}
static void stick(Sint16 raw_x,Sint16 raw_y,float *x,float *y)
{
    float a=raw_x/(raw_x<0?32768.0f:32767.0f),b=raw_y/(raw_y<0?32768.0f:32767.0f);
    float length=sqrtf(a*a+b*b),scale,deadzone=input_options.deadzone_percent/100.0f;
    if(length<=deadzone){*x=*y=0;return;}
    scale=(fminf(length,1)-deadzone)/((1-deadzone)*length);*x=a*scale;*y=b*scale;
}
int rf_xbox_input_open(void)
{
    if(initialized)return RF_RANGE;
    memset(&input_rumble,0,sizeof(input_rumble));
    pause_mode=pause_edge=start_held=release_controls=menu_held=menu_edge=0;
    disconnect_edge=exit_rearm=alt_sample_valid=chord_controls=0;
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) {rf_player_input_diagnostic[1]=2;return RF_IO;}
    initialized=1;rf_player_input_diagnostic[1]=1;return RF_OK;
}
void rf_xbox_input_close(void)
{
    input_rumble_cancel();
    if(controller){SDL_GameControllerClose(controller);controller=NULL;}
    memset(&input_rumble,0,sizeof(input_rumble));
    if(initialized){SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);initialized=0;}
    pause_mode=pause_edge=start_held=release_controls=menu_held=menu_edge=0;
    disconnect_edge=exit_rearm=alt_sample_valid=chord_controls=0;
    rf_player_input_diagnostic[3]=0;
}
int rf_xbox_input_idle_status(void)
{
    return initialized || controller || SDL_WasInit(SDL_INIT_GAMECONTROLLER)?RF_RANGE:RF_OK;
}
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input)
{
    int i;uint32_t attached=0,start,back,controls=0,actions=0,menu=0;float horizontal,vertical;(void)context;
    rf_xbox_input_menu_observation=0;alt_sample_valid=0;
    if(!initialized || !input)return RF_RANGE;
    pause_edge=menu_edge=disconnect_edge=0;
    memset(input,0,sizeof(*input));
    /* Bundled NXDK SDL_PollEvent pumps joysticks on EVERY call, including
     * each event discarded by a drain loop. One explicit pump obtains the
     * current controller state and hotplug changes without repolling USB
     * hubs in proportion to queue length. This owner consumes state only. */
    SDL_PumpEvents();
    if(controller && !SDL_GameControllerGetAttached(controller)) {
        input_rumble_cancel();
        SDL_GameControllerClose(controller);controller=NULL;disconnect_edge=1;
        memset(&input_rumble,0,sizeof(input_rumble));
    }
    if(!controller)for(i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)) {
        controller=SDL_GameControllerOpen(i);
        if(controller){
            memset(&input_rumble,0,sizeof(input_rumble));
            /* It was not open during the pump; sample this newly attached
             * controller now rather than adding one frame of neutral input. */
            SDL_GameControllerUpdate();attached=1;break;
        }
    }
    /* Unlike PollEvent, this bundled SDL_FlushEvents does not pump again.
     * Keep the existing policy of ignoring the application's SDL events. */
    SDL_FlushEvents(SDL_FIRSTEVENT,SDL_LASTEVENT);
    rf_player_input_diagnostic[2]=frame+1;
    rf_player_input_diagnostic[3]=controller!=NULL;
    if(!controller){input_rumble_cancel();start_held=menu_held=0;rf_scene_save_button(0);rf_scene_load_button(0);return RF_OK;}
    start=SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START)!=0;
    back=SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_BACK)!=0;
    /* Observe even an exit-chord return. This getter reads SDL's current
     * sample only; it does not repump USB or change held/edge ownership. */
    rf_xbox_input_menu_observation=RF_XBOX_MENU_OBS_CONNECTED|
        (SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_A)?RF_XBOX_MENU_OBS_A:0)|
        (start?RF_XBOX_MENU_OBS_START:0)|(back?RF_XBOX_MENU_OBS_BACK:0);
    /* Attaching a controller with Start held is not a fresh user press. */
    if(attached){start_held=start;exit_rearm=start||back;}
    pause_edge=start && !start_held && !back;start_held=start;
    if(!back && !start)exit_rearm=0;
    if(back && start && !exit_rearm) {
        input_rumble_cancel();
        rf_player_input_diagnostic[4]=1;return RF_NOT_FOUND;
    }
    if(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>3855)controls|=1u<<RF_XBOX_BUTTON_RT;
    if(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>3855)controls|=1u<<RF_XBOX_BUTTON_LT;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_A))controls|=1u<<RF_XBOX_BUTTON_A;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_X))controls|=1u<<RF_XBOX_BUTTON_X;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_Y))controls|=1u<<RF_XBOX_BUTTON_Y;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_B))controls|=1u<<RF_XBOX_BUTTON_B;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))controls|=1u<<RF_XBOX_BUTTON_BLACK;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSHOULDER))controls|=1u<<RF_XBOX_BUTTON_WHITE;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSTICK))controls|=1u<<RF_XBOX_BUTTON_RCLICK;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK))controls|=1u<<RF_XBOX_BUTTON_LCLICK;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_RIGHT))controls|=CONTROL_RIGHT;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_LEFT))controls|=CONTROL_LEFT;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_UP))controls|=CONTROL_UP;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_DOWN))controls|=CONTROL_DOWN;
    /* Navigation is physical, never derived from gameplay action labels. */
    if(controls&CONTROL_UP)menu|=RF_XBOX_MENU_UP;
    if(controls&CONTROL_DOWN)menu|=RF_XBOX_MENU_DOWN;
    if(controls&(1u<<RF_XBOX_BUTTON_A))menu|=RF_XBOX_MENU_ACCEPT;
    if(controls&(1u<<RF_XBOX_BUTTON_B))menu|=RF_XBOX_MENU_CANCEL;
    if(controls&CONTROL_LEFT)menu|=RF_XBOX_MENU_LEFT;
    if(controls&CONTROL_RIGHT)menu|=RF_XBOX_MENU_RIGHT;
    /* Suppression owns physical controls, so edits cannot retarget a held
     * action. Hotplug/section reopen/pause still require individual releases. */
    if(attached){menu_held=menu;release_controls=controls;}
    menu_edge=menu&~menu_held;menu_held=menu;
    release_controls&=controls;chord_controls&=controls;
    if(binding_rearm && !controls && !start && !back)binding_rearm=0;
    rf_xbox_input_menu_observation|=menu_edge<<RF_XBOX_MENU_OBS_EDGE_SHIFT;
    if(!start && !back && !controls && !menu_held && !release_controls && !binding_rearm && !exit_rearm && !disconnect_edge)
        rf_xbox_input_menu_observation|=RF_XBOX_MENU_OBS_NEUTRAL;
    if(pause_mode || pause_edge || binding_rearm) {
        input_rumble_cancel();
        release_controls=controls;
        rf_scene_save_button(0);rf_scene_load_button(0);return RF_OK;
    }
    {uint32_t active=controls&~release_controls;
     uint32_t load=back && (active&(1u<<RF_XBOX_BUTTON_X));
     uint32_t save=back && (active&(1u<<RF_XBOX_BUTTON_Y));
     /* Fixed physical Back+X/Back+Y retain load-over-save priority. Consume
      * chord buttons through release so a remapped Fire cannot leak on Back
      * release; preserve the existing simultaneous-Holster consumption too. */
     rf_scene_save_button(save&&!load);rf_scene_load_button(load);
     if(back && (controls&((1u<<RF_XBOX_BUTTON_X)|(1u<<RF_XBOX_BUTTON_Y)))){
         input_rumble_cancel();
         chord_controls|=controls&((1u<<RF_XBOX_BUTTON_X)|(1u<<RF_XBOX_BUTTON_Y)|
             (1u<<bindings[RF_XBOX_BIND_HOLSTER]));return RF_OK;
     }
     active&=~chord_controls;
     if(active&(1u<<bindings[RF_XBOX_BIND_ALT]))binding_alt_rearm=0;
     /* An in-progress release-fired alternate owner must observe suppression
      * across a rebind until a fresh effective Alt press, never a fake release. */
     alt_sample_valid=!attached&&!disconnect_edge&&!binding_alt_rearm&&
         !((release_controls|chord_controls)&(1u<<bindings[RF_XBOX_BIND_ALT]));
     {static const uint32_t action_bits[RF_XBOX_BIND_COUNT]={
         INPUT_FIRE,INPUT_ALT,INPUT_JUMP,INPUT_USE,INPUT_RELOAD,INPUT_CROUCH,
         INPUT_NEXT,INPUT_PREVIOUS,INPUT_FINE_AIM,INPUT_HOLSTER
      };
      for(i=0;i<RF_XBOX_BIND_COUNT;i++)if(active&(1u<<bindings[i]))actions|=action_bits[i];}
     /* D-pad aliases and defuse controls remain fixed and cannot collide with
      * a remappable physical control. Opposing cycles still cancel below. */
     if(active&CONTROL_RIGHT)actions|=INPUT_NEXT;
     if(active&CONTROL_LEFT)actions|=INPUT_PREVIOUS;
     if(active&CONTROL_UP)actions|=INPUT_UP;
     if(active&CONTROL_DOWN)actions|=INPUT_DOWN;}
    /* Port-owned Standard/Southpaw choice swaps only complete analog pairs.
     * Gameplay mapping and physical menu/chord/release ownership stay above.
     * Look options follow the selected look pair. */
    {uint32_t southpaw=input_options.stick_layout==RF_XBOX_STICK_LAYOUT_SOUTHPAW;
     stick(SDL_GameControllerGetAxis(controller,southpaw?SDL_CONTROLLER_AXIS_RIGHTX:SDL_CONTROLLER_AXIS_LEFTX),
           SDL_GameControllerGetAxis(controller,southpaw?SDL_CONTROLLER_AXIS_RIGHTY:SDL_CONTROLLER_AXIS_LEFTY),&horizontal,&vertical);
     input->move[0]=horizontal;input->move[2]=-vertical;
     stick(SDL_GameControllerGetAxis(controller,southpaw?SDL_CONTROLLER_AXIS_LEFTX:SDL_CONTROLLER_AXIS_RIGHTX),
           SDL_GameControllerGetAxis(controller,southpaw?SDL_CONTROLLER_AXIS_LEFTY:SDL_CONTROLLER_AXIS_RIGHTY),&horizontal,&vertical);}
    {float sensitivity=input_options.look_sensitivity_percent/100.0f;
     float pitch=input_options.invert_y?vertical:-vertical;
     /* Gameplay consumers, including the turret, require normalized axes. */
     input->look[0]=fmaxf(-1,fminf(1,pitch*sensitivity));
     input->look[1]=fmaxf(-1,fminf(1,horizontal*sensitivity));
     /* Hold the mapped Fine Aim control for precision. Half response is first-playable
      * Xbox policy, not a recovered retail scalar. Apply AFTER saturation so
      * 200% sensitivity cannot cancel it. Keep movement/deadzone and Alt scope
      * ownership unchanged; the ordinary scope look scalar composes later.
      * The action bit shares hotplug/pause release suppression above. */
     if(actions&INPUT_FINE_AIM){input->look[0]*=.5f;input->look[1]*=.5f;}}
    input->holster=!!(actions&INPUT_HOLSTER);
    input->fire=!!(actions&INPUT_FIRE);input->alt_fire=!!(actions&INPUT_ALT);
    {uint32_t forward=!!(actions&INPUT_NEXT),backward=!!(actions&INPUT_PREVIOUS);
     input->cycle_weapon=forward==backward?0:forward?1:2;}
    if(rf_scene_defuse[1]){
        uint32_t up=!!(actions&INPUT_UP),down=!!(actions&INPUT_DOWN);
        input->move[1]=up==down?0:up?1.0f:-1.0f;
    }
    input->reload=!!(actions&INPUT_RELOAD);input->jump=!!(actions&INPUT_JUMP);
    input->use=!!(actions&INPUT_USE);input->crouch=!!(actions&INPUT_CROUCH);
    input_rumble_update();
    rf_player_input_diagnostic[5]=input->crouch;return RF_OK;
}
