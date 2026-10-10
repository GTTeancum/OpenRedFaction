/* Xbox platform input policy; not recovered Red Faction input mapping. */
#include "input.h"
#include <SDL.h>
#include <math.h>
#include <string.h>
static SDL_GameController *controller;
static int initialized;
static uint32_t pause_mode,pause_edge,start_held,release_actions;
enum {
    INPUT_FIRE=1u,INPUT_ALT=2u,INPUT_JUMP=4u,INPUT_USE=8u,
    INPUT_RELOAD=16u,INPUT_CROUCH=32u,INPUT_NEXT=64u,INPUT_PREVIOUS=128u,
    INPUT_UP=256u,INPUT_DOWN=512u
};
volatile uint32_t rf_player_input_diagnostic[6]={0x5246494eu};
uint32_t rf_xbox_input_pause_pressed(void){return pause_edge;}
void rf_xbox_input_set_paused(uint32_t paused){pause_mode=paused!=0;}
static void stick(Sint16 raw_x,Sint16 raw_y,float *x,float *y)
{
    float a=raw_x/(raw_x<0?32768.0f:32767.0f),b=raw_y/(raw_y<0?32768.0f:32767.0f);
    float length=sqrtf(a*a+b*b),scale;
    if(length<=.18f){*x=*y=0;return;}
    scale=(fminf(length,1)-.18f)/(.82f*length);*x=a*scale;*y=b*scale;
}
int rf_xbox_input_open(void)
{
    if(initialized)return RF_RANGE;
    pause_mode=pause_edge=start_held=release_actions=0;
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) {rf_player_input_diagnostic[1]=2;return RF_IO;}
    initialized=1;rf_player_input_diagnostic[1]=1;return RF_OK;
}
void rf_xbox_input_close(void)
{
    if(controller){SDL_GameControllerClose(controller);controller=NULL;}
    if(initialized){SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);initialized=0;}
    pause_mode=pause_edge=start_held=release_actions=0;
    rf_player_input_diagnostic[3]=0;
}
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input)
{
    int i;uint32_t attached=0,start,back,actions=0;float horizontal,vertical;(void)context;
    if(!initialized || !input)return RF_RANGE;
    pause_edge=0;
    memset(input,0,sizeof(*input));
    /* Bundled NXDK SDL_PollEvent pumps joysticks on EVERY call, including
     * each event discarded by a drain loop. One explicit pump obtains the
     * current controller state and hotplug changes without repolling USB
     * hubs in proportion to queue length. This owner consumes state only. */
    SDL_PumpEvents();
    if(controller && !SDL_GameControllerGetAttached(controller)) {SDL_GameControllerClose(controller);controller=NULL;}
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
    if(!controller){start_held=0;rf_scene_save_button(0);rf_scene_load_button(0);return RF_OK;}
    start=SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START)!=0;
    back=SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_BACK)!=0;
    /* Attaching a controller with Start held is not a fresh user press. */
    if(attached)start_held=start;
    pause_edge=start && !start_held && !back;start_held=start;
    if(back && start) {
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
    release_actions&=actions;
    if(pause_mode || pause_edge) {
        release_actions=actions;
        rf_scene_save_button(0);rf_scene_load_button(0);return RF_OK;
    }
    actions&=~release_actions;
    {uint32_t load=back && (actions&INPUT_USE);
     uint32_t save=back && (actions&INPUT_RELOAD);
     rf_scene_save_button(save&&!load);rf_scene_load_button(load);if(save||load)return RF_OK;}
    stick(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX),
          SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY),&horizontal,&vertical);
    input->move[0]=horizontal;input->move[2]=-vertical;
    stick(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTX),
          SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTY),&horizontal,&vertical);
    input->look[0]=-vertical;input->look[1]=horizontal;
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
