/* First-playable platform session UI, not recovered retail menu behavior.
 * This pass is called while the scene/input provider is suspended. It uses
 * only the existing untextured fallback glyphs, with no scene/image borrower,
 * allocation, timer mutation or storage access. */
int rf_scene_draw_session_menu(rf_scene_particle_sink sink,void *context,
    uint32_t selection,uint32_t flags)
{
    static const char *const items[RF_SCENE_SESSION_COUNT]={
        "RESUME","QUICK SAVE","QUICK LOAD","RESTART SECTION","END SESSION"
    };
    uint32_t i;int status;
    if(!sink || particle_draw_stream || selection>=RF_SCENE_SESSION_COUNT ||
       (flags&~(RF_SCENE_SESSION_CONFIRM|RF_SCENE_SESSION_DISCONNECTED|
          RF_SCENE_SESSION_ENDED|RF_SCENE_SESSION_UNAVAILABLE|RF_SCENE_SESSION_FRONTEND_EXIT)))return RF_RANGE;
    if(flags&RF_SCENE_SESSION_ENDED){
        status=combat_hud_centered(sink,context,192,"SESSION ENDED",0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,248,
            (flags&RF_SCENE_SESSION_FRONTEND_EXIT)?"RETURNING TO MAIN MENU":"REBOOT THE GAME TO PLAY AGAIN",0xffb8c0d0);
    }
    status=combat_hud_centered(sink,context,58,"RED FACTION - PAUSED",0xffeeeeee);if(status)return status;
    if(flags&RF_SCENE_SESSION_DISCONNECTED){
        status=combat_hud_centered(sink,context,172,"CONTROLLER DISCONNECTED",0xffffc060);if(status)return status;
        status=combat_hud_centered(sink,context,224,"CONNECT A CONTROLLER TO CONTINUE",0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,284,"THEN PRESS START OR CHOOSE RESUME",0xffb8c0d0);
    }
    if(flags&RF_SCENE_SESSION_CONFIRM){
        const char *question=selection==RF_SCENE_SESSION_LOAD?"LOAD THE LAST MANUAL SAVE?":
            selection==RF_SCENE_SESSION_RESTART?"RESTART THIS SECTION?":"END THIS SESSION?";
        status=combat_hud_centered(sink,context,160,question,0xffffc060);if(status)return status;
        status=combat_hud_centered(sink,context,216,"UNSAVED PROGRESS WILL BE LOST",0xffeeeeee);if(status)return status;
        if(selection==RF_SCENE_SESSION_RESTART){
            status=combat_hud_centered(sink,context,252,"START WITH ITS DEFAULT LOADOUT",0xffb8c0d0);if(status)return status;
        }else if(selection==RF_SCENE_SESSION_EXIT){
            status=combat_hud_centered(sink,context,252,
                (flags&RF_SCENE_SESSION_FRONTEND_EXIT)?"RETURN TO MAIN MENU":"REBOOT THE GAME TO PLAY AGAIN",0xffb8c0d0);if(status)return status;
        }
        return combat_hud_centered(sink,context,340,"A CONFIRM   B CANCEL   START RESUME",0xffeeeeee);
    }
    for(i=0;i<RF_SCENE_SESSION_COUNT;i++){
        if(i==selection){
            status=combat_hud_rect(sink,context,140,122+i*40,360,30,0xff30485c);if(status)return status;
        }
        status=combat_hud_centered(sink,context,130+i*40,items[i],i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    if(flags&RF_SCENE_SESSION_UNAVAILABLE){
        status=combat_hud_centered(sink,context,338,"RESTART UNAVAILABLE IN THIS SCENE",0xffffc060);if(status)return status;
    }
    status=combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A SELECT",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,412,"B OR START RESUME",0xffb8c0d0);
}

/* Tester launch policy only; no retail menu reconstruction is implied. */
int rf_scene_draw_frontend(rf_scene_particle_sink sink,void *context,
    uint32_t flags,int session_status)
{
    int status;
    if(!sink || particle_draw_stream || scene_actor_collision_owner ||
       (flags&~(RF_SCENE_FRONTEND_DISCONNECTED|RF_SCENE_FRONTEND_LOADING|
          RF_SCENE_FRONTEND_ENDED)))return RF_RANGE;
    status=combat_hud_centered(sink,context,76,"RED FACTION",0xffeeeeee);if(status)return status;
    status=combat_hud_centered(sink,context,112,"XBOX TESTER BUILD",0xffb8c0d0);if(status)return status;
    if(session_status){
        char message[48];
        status=combat_hud_centered(sink,context,196,"SESSION STOPPED",0xffffc060);if(status)return status;
        snprintf(message,sizeof(message),"ERROR %d - REBOOT REQUIRED",session_status);
        status=combat_hud_centered(sink,context,244,message,0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,328,"REPORT THIS ERROR TO THE DEVELOPER",0xffb8c0d0);
    }
    if(flags&RF_SCENE_FRONTEND_LOADING){
        status=combat_hud_centered(sink,context,216,"STARTING NEW GAME",0xff80ffb0);if(status)return status;
        return combat_hud_centered(sink,context,264,"LOADING LIVE MINES",0xffb8c0d0);
    }
    if(flags&RF_SCENE_FRONTEND_ENDED){
        status=combat_hud_centered(sink,context,162,"SESSION ENDED",0xffb8c0d0);if(status)return status;
    }
    status=combat_hud_rect(sink,context,170,218,300,36,0xff30485c);if(status)return status;
    status=combat_hud_centered(sink,context,228,"NEW GAME",0xff80ffb0);if(status)return status;
    status=combat_hud_centered(sink,context,284,"START FRESH IN LIVE MINES",0xffb8c0d0);if(status)return status;
    if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
        return combat_hud_centered(sink,context,380,"CONNECT A CONTROLLER",0xffffc060);
    return combat_hud_centered(sink,context,380,"A OR START - NEW GAME",0xffeeeeee);
}
