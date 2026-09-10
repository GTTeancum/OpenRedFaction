/* Xbox platform input policy; not recovered Red Faction input mapping. */
#include "input.h"
#include <SDL.h>
#include <math.h>
#include <string.h>
static SDL_GameController *controller;
static int initialized;
volatile uint32_t rf_player_input_diagnostic[6]={0x5246494eu};
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
    if(SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER)) {rf_player_input_diagnostic[1]=2;return RF_IO;}
    initialized=1;rf_player_input_diagnostic[1]=1;return RF_OK;
}
void rf_xbox_input_close(void)
{
    if(controller){SDL_GameControllerClose(controller);controller=NULL;}
    if(initialized){SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);initialized=0;}
    rf_player_input_diagnostic[3]=0;
}
int rf_xbox_input_poll(void *context,uint32_t frame,rf_scene_input *input)
{
    SDL_Event event;int i;float horizontal,vertical;(void)context;
    if(!initialized || !input)return RF_RANGE;
    memset(input,0,sizeof(*input));
    while(SDL_PollEvent(&event)) { /* Pump only this application's SDL device events. */ }
    if(controller && !SDL_GameControllerGetAttached(controller)) {SDL_GameControllerClose(controller);controller=NULL;}
    if(!controller)for(i=0;i<SDL_NumJoysticks();++i)if(SDL_IsGameController(i)) {
        controller=SDL_GameControllerOpen(i);if(controller)break;
    }
    SDL_GameControllerUpdate();rf_player_input_diagnostic[2]=frame+1;
    rf_player_input_diagnostic[3]=controller!=NULL;
    if(!controller)return RF_OK;
    if(SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_BACK) &&
       SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_START)) {
        rf_player_input_diagnostic[4]=1;return RF_NOT_FOUND;
    }
    stick(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTX),
          SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_LEFTY),&horizontal,&vertical);
    input->move[0]=horizontal;input->move[2]=-vertical;
    stick(SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTX),
          SDL_GameControllerGetAxis(controller,SDL_CONTROLLER_AXIS_RIGHTY),&horizontal,&vertical);
    input->look[0]=-vertical;input->look[1]=horizontal;
    input->jump=SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_A)!=0;
    input->crouch=SDL_GameControllerGetButton(controller,SDL_CONTROLLER_BUTTON_B)!=0;
    rf_player_input_diagnostic[5]=input->crouch;return RF_OK;
}
