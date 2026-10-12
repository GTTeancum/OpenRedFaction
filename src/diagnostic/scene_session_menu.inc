/* First-playable platform session UI, not recovered retail menu behavior.
 * This pass is called while the scene/input provider is suspended. It uses
 * only the existing untextured fallback glyphs, with no scene/image borrower,
 * allocation, timer mutation or storage access. */
int rf_scene_draw_session_menu(rf_scene_particle_sink sink,void *context,
    uint32_t selection,uint32_t flags)
{
    static const char *const items[RF_SCENE_SESSION_COUNT]={
        "RESUME","QUICK SAVE","QUICK LOAD","RESTART SECTION","END SESSION","OPTIONS"
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
            status=combat_hud_rect(sink,context,140,116+i*36,360,30,0xff30485c);if(status)return status;
        }
        status=combat_hud_centered(sink,context,124+i*36,items[i],i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    if(flags&RF_SCENE_SESSION_UNAVAILABLE){
        status=combat_hud_centered(sink,context,338,"RESTART UNAVAILABLE IN THIS SCENE",0xffffc060);if(status)return status;
    }
    status=combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A SELECT",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,412,"B OR START RESUME",0xffb8c0d0);
}

/* Tester launch policy only; no retail menu reconstruction is implied. */
static const char *const scene_difficulty_names[4]={"EASY","MEDIUM","HARD","IMPOSSIBLE"};
int rf_scene_draw_frontend(rf_scene_particle_sink sink,void *context,
    uint32_t flags,int session_status,uint32_t page,uint32_t selection,uint32_t difficulty)
{
    int status;uint32_t i;
    if(!sink || particle_draw_stream || scene_actor_collision_owner || difficulty>3 ||
       page>RF_SCENE_FRONTEND_DIFFICULTY || selection>(page==RF_SCENE_FRONTEND_MAIN?1u:4u) ||
       (flags&~(RF_SCENE_FRONTEND_DISCONNECTED|RF_SCENE_FRONTEND_LOADING|
          RF_SCENE_FRONTEND_ENDED)))return RF_RANGE;
    status=combat_hud_centered(sink,context,60,"RED FACTION",0xffeeeeee);if(status)return status;
    status=combat_hud_centered(sink,context,96,"XBOX TESTER BUILD",0xffb8c0d0);if(status)return status;
    if(session_status){
        char message[48];
        status=combat_hud_centered(sink,context,196,"SESSION STOPPED",0xffffc060);if(status)return status;
        snprintf(message,sizeof(message),"ERROR %d - REBOOT REQUIRED",session_status);
        status=combat_hud_centered(sink,context,244,message,0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,328,"REPORT THIS ERROR TO THE DEVELOPER",0xffb8c0d0);
    }
    if(flags&RF_SCENE_FRONTEND_LOADING){
        status=combat_hud_centered(sink,context,196,"STARTING NEW GAME",0xff80ffb0);if(status)return status;
        status=combat_hud_centered(sink,context,244,"LOADING LIVE MINES",0xffb8c0d0);if(status)return status;
        return combat_hud_centered(sink,context,292,scene_difficulty_names[difficulty],0xffeeeeee);
    }
    if(page==RF_SCENE_FRONTEND_DIFFICULTY){
        status=combat_hud_centered(sink,context,140,"NEW GAME - DIFFICULTY",0xffeeeeee);if(status)return status;
        for(i=0;i<5;i++){
            if(i==selection){status=combat_hud_rect(sink,context,160,178+i*34,320,28,0xff30485c);if(status)return status;}
            status=combat_hud_centered(sink,context,184+i*34,i<4?scene_difficulty_names[i]:"BACK",
                i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
        }
        if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
            return combat_hud_centered(sink,context,380,"CONNECT A CONTROLLER",0xffffc060);
        status=combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A START / SELECT",0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,412,"B OR START BACK",0xffb8c0d0);
    }
    if(flags&RF_SCENE_FRONTEND_ENDED){
        status=combat_hud_centered(sink,context,150,"SESSION ENDED",0xffb8c0d0);if(status)return status;
    }
    for(i=0;i<2;i++){
        if(i==selection){status=combat_hud_rect(sink,context,170,208+i*44,300,34,0xff30485c);if(status)return status;}
        status=combat_hud_centered(sink,context,216+i*44,i?"OPTIONS":"NEW GAME",i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    status=combat_hud_centered(sink,context,326,"START FRESH IN LIVE MINES",0xffb8c0d0);if(status)return status;
    if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
        return combat_hud_centered(sink,context,380,"CONNECT A CONTROLLER",0xffffc060);
    return combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A OR START SELECT",0xffeeeeee);
}
int rf_scene_draw_options(rf_scene_particle_sink sink,void *context,uint32_t selection,
    uint32_t flags,uint32_t sensitivity_percent,uint32_t invert_y,uint32_t deadzone_percent)
{
    uint32_t i;int status;char text[64];
    if(!sink || particle_draw_stream || selection>=RF_SCENE_OPTIONS_COUNT ||
       sensitivity_percent<25 || sensitivity_percent>200 || invert_y>1 || deadzone_percent>40 ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED)))return RF_RANGE;
    status=combat_hud_centered(sink,context,58,"CONTROLLER OPTIONS",0xffeeeeee);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED){
        status=combat_hud_centered(sink,context,172,"CONTROLLER DISCONNECTED",0xffffc060);if(status)return status;
        return combat_hud_centered(sink,context,224,"CONNECT A CONTROLLER TO CONTINUE",0xffeeeeee);
    }
    for(i=0;i<RF_SCENE_OPTIONS_COUNT;i++){
        if(i==RF_SCENE_OPTIONS_SENSITIVITY)snprintf(text,sizeof(text),"LOOK SENSITIVITY   %u%%",sensitivity_percent);
        else if(i==RF_SCENE_OPTIONS_INVERT)snprintf(text,sizeof(text),"INVERT Y   %s",invert_y?"ON":"OFF");
        else if(i==RF_SCENE_OPTIONS_DEADZONE)snprintf(text,sizeof(text),"STICK DEADZONE   %u%%",deadzone_percent);
        else snprintf(text,sizeof(text),"%s",i==RF_SCENE_OPTIONS_DEFAULTS?"RESET DEFAULTS":"BACK");
        if(i==selection){status=combat_hud_rect(sink,context,104,116+i*38,432,30,0xff30485c);if(status)return status;}
        status=combat_hud_centered(sink,context,124+i*38,text,i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    status=combat_hud_centered(sink,context,326,"KEPT UNTIL THE GAME IS REBOOTED",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,366,"UP/DOWN SELECT   LEFT/RIGHT ADJUST",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,404,
        (flags&RF_SCENE_OPTIONS_PAUSED)?"A SELECT   B BACK   START RESUME":"A SELECT   B OR START BACK",0xffb8c0d0);
}
