/* Xbox platform input policy; not recovered Red Faction input mapping. */
#include "input.h"
#include <SDL.h>
#include <math.h>
#include <string.h>
static SDL_GameController *controller;
static int initialized;
/* This owner deliberately outlives controller/session open and close. */
static rf_xbox_input_options input_options={
    RF_XBOX_LOOK_SENSITIVITY_DEFAULT,0,RF_XBOX_STICK_DEADZONE_DEFAULT
};
static uint32_t pause_mode,pause_edge,start_held,release_actions,menu_held,menu_edge;
static uint32_t disconnect_edge,exit_rearm;
enum {
    INPUT_FIRE=1u,INPUT_ALT=2u,INPUT_JUMP=4u,INPUT_USE=8u,
    INPUT_RELOAD=16u,INPUT_CROUCH=32u,INPUT_NEXT=64u,INPUT_PREVIOUS=128u,
    INPUT_UP=256u,INPUT_DOWN=512u,INPUT_FINE_AIM=1024u,INPUT_HOLSTER=2048u
};
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
    input_options=next;
}
uint32_t rf_xbox_input_pause_pressed(void){return pause_edge;}
uint32_t rf_xbox_input_connected(void){return controller!=NULL;}
uint32_t rf_xbox_input_connection_lost(void){return disconnect_edge;}
uint32_t rf_xbox_input_menu_pressed(void){return menu_edge;}
void rf_xbox_input_set_paused(uint32_t paused){pause_mode=paused!=0;}
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
    pause_mode=pause_edge=start_held=release_actions=menu_held=menu_edge=0;
    disconnect_edge=exit_rearm=0;
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) {rf_player_input_diagnostic[1]=2;return RF_IO;}
    initialized=1;rf_player_input_diagnostic[1]=1;return RF_OK;
}
void rf_xbox_input_close(void)
{
    if(controller){SDL_GameControllerClose(controller);controller=NULL;}
    if(initialized){SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);initialized=0;}
    pause_mode=pause_edge=start_held=release_actions=menu_held=menu_edge=0;
    disconnect_edge=exit_rearm=0;
    rf_player_input_diagnostic[3]=0;
}
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input)
{
    int i;uint32_t attached=0,start,back,actions=0,menu=0;float horizontal,vertical;(void)context;
    rf_xbox_input_menu_observation=0;
    if(!initialized || !input)return RF_RANGE;
    pause_edge=menu_edge=disconnect_edge=0;
    memset(input,0,sizeof(*input));
    /* Bundled NXDK SDL_PollEvent pumps joysticks on EVERY call, including
     * each event discarded by a drain loop. One explicit pump obtains the
     * current controller state and hotplug changes without repolling USB
     * hubs in proportion to queue length. This owner consumes state only. */
    SDL_PumpEvents();
    if(controller && !SDL_GameControllerGetAttached(controller)) {
        SDL_GameControllerClose(controller);controller=NULL;disconnect_edge=1;
    }
    if(!controller)for(i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)) {
        controller=SDL_GameControllerOpen(i);
        if(controller){
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
    if(!controller){start_held=menu_held=0;rf_scene_save_button(0);rf_scene_load_button(0);return RF_OK;}
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
        rf_player_input_diagnostic[4]=1;return RF_NOT_FOUND;
    }
    if(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERRIGHT)>3855)actions|=INPUT_FIRE;
    if(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_TRIGGERLEFT)>3855)actions|=INPUT_ALT;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_A))actions|=INPUT_JUMP;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_X))actions|=INPUT_USE;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_Y))actions|=INPUT_RELOAD;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_B))actions|=INPUT_CROUCH;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_RIGHT) ||
       SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSHOULDER))actions|=INPUT_NEXT;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_LEFT) ||
       SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSHOULDER))actions|=INPUT_PREVIOUS;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_UP))actions|=INPUT_UP;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_DOWN))actions|=INPUT_DOWN;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_RIGHTSTICK))actions|=INPUT_FINE_AIM;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_LEFTSTICK))actions|=INPUT_HOLSTER;
    if(actions&INPUT_UP)menu|=RF_XBOX_MENU_UP;
    if(actions&INPUT_DOWN)menu|=RF_XBOX_MENU_DOWN;
    if(actions&INPUT_JUMP)menu|=RF_XBOX_MENU_ACCEPT;
    if(actions&INPUT_CROUCH)menu|=RF_XBOX_MENU_CANCEL;
    /* Only D-pad directions edit menu values; shoulders retain their
     * existing weapon-cycle role without becoming menu controls. */
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_LEFT))menu|=RF_XBOX_MENU_LEFT;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_DPAD_RIGHT))menu|=RF_XBOX_MENU_RIGHT;
    /* Hotplug is not a press. Also keep held fire/use/save/menu buttons from
     * becoming fresh actions when a section reload opens its controller. */
    if(attached){menu_held=menu;release_actions=actions;}
    menu_edge=menu&~menu_held;menu_held=menu;
    release_actions&=actions;
    rf_xbox_input_menu_observation|=menu_edge<<RF_XBOX_MENU_OBS_EDGE_SHIFT;
    if(!start && !back && !actions && !menu_held && !release_actions && !exit_rearm && !disconnect_edge)
        rf_xbox_input_menu_observation|=RF_XBOX_MENU_OBS_NEUTRAL;
    if(pause_mode || pause_edge) {
        release_actions=actions;
        rf_scene_save_button(0);rf_scene_load_button(0);return RF_OK;
    }
    actions&=~release_actions;
    {uint32_t load=back && (actions&INPUT_USE);
     uint32_t save=back && (actions&INPUT_RELOAD);
     rf_scene_save_button(save&&!load);rf_scene_load_button(load);
     if(save||load){release_actions|=actions&INPUT_HOLSTER;return RF_OK;}}
    stick(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX),
          SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY),&horizontal,&vertical);
    input->move[0]=horizontal;input->move[2]=-vertical;
    stick(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTX),
          SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTY),&horizontal,&vertical);
    {float sensitivity=input_options.look_sensitivity_percent/100.0f;
     float pitch=input_options.invert_y?vertical:-vertical;
     /* Gameplay consumers, including the turret, require normalized axes. */
     input->look[0]=fmaxf(-1,fminf(1,pitch*sensitivity));
     input->look[1]=fmaxf(-1,fminf(1,horizontal*sensitivity));
     /* Hold right-stick click for precision. Half response is first-playable
      * Xbox policy, not a recovered retail scalar. Apply AFTER saturation so
      * 200% sensitivity cannot cancel it. Keep movement/deadzone and LT scope
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
    rf_player_input_diagnostic[5]=input->crouch;return RF_OK;
}
