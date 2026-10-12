#include "rf/frontend_load.h"
/* First-playable platform session UI, not recovered retail menu behavior.
 * This pass is called while the scene/input provider is suspended. It uses
 * only the existing untextured fallback glyphs, with no scene/image borrower,
 * allocation, timer mutation or storage access. */
int rf_scene_draw_session_menu(rf_scene_particle_sink sink,void *context,
    uint32_t selection,uint32_t flags)
{
    static const char *const items[RF_SCENE_SESSION_COUNT]={
        "RESUME","QUICK SAVE","QUICK LOAD","RESTART SECTION","END SESSION","OPTIONS","MESSAGE HISTORY"
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
            status=combat_hud_rect(sink,context,140,116+i*32,360,28,0xff30485c);if(status)return status;
        }
        status=combat_hud_centered(sink,context,122+i*32,items[i],i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    if(flags&RF_SCENE_SESSION_UNAVAILABLE){
        status=combat_hud_centered(sink,context,352,"RESTART UNAVAILABLE IN THIS SCENE",0xffffc060);if(status)return status;
    }
    status=combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A SELECT",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,412,"B OR START RESUME",0xffb8c0d0);
}

/* Tester launch policy only; no retail menu reconstruction is implied. */
static const char *const scene_difficulty_names[4]={"EASY","MEDIUM","HARD","IMPOSSIBLE"};
int rf_scene_draw_frontend(rf_scene_particle_sink sink,void *context,
    uint32_t flags,int session_status,uint32_t page,uint32_t selection,uint32_t difficulty,
    uint32_t load_outcome,int load_status)
{
    int status;uint32_t i;
    if(!sink || particle_draw_stream || scene_actor_collision_owner || difficulty>3 ||
       page>RF_SCENE_FRONTEND_EXIT || page==RF_SCENE_FRONTEND_OPTIONS ||
       selection>(page==RF_SCENE_FRONTEND_DIFFICULTY?4u:
           page==RF_SCENE_FRONTEND_MAIN?3u:page==RF_SCENE_FRONTEND_EXIT?1u:2u) ||
       ((flags&RF_SCENE_FRONTEND_LOAD_NOTICE) && load_outcome>RF_SCENE_FRONTEND_LOAD_BLOCKED) ||
       ((flags&RF_SCENE_FRONTEND_LOADING) && page==RF_SCENE_FRONTEND_LOAD && selection>1) ||
       (flags&~(RF_SCENE_FRONTEND_DISCONNECTED|RF_SCENE_FRONTEND_LOADING|
          RF_SCENE_FRONTEND_ENDED|RF_SCENE_FRONTEND_LOAD_NOTICE)))return RF_RANGE;
    status=combat_hud_centered(sink,context,60,"RED FACTION",0xffeeeeee);if(status)return status;
    status=combat_hud_centered(sink,context,96,"XBOX TESTER BUILD",0xffb8c0d0);if(status)return status;
    if(session_status){
        char message[48];
        status=combat_hud_centered(sink,context,196,
            page==RF_SCENE_FRONTEND_EXIT?"DASHBOARD EXIT STOPPED":
            (flags&RF_SCENE_FRONTEND_LOAD_NOTICE) && load_outcome==RF_SCENE_FRONTEND_LOAD_BLOCKED?
            "STORAGE / SESSION CLEANUP FAILED":"SESSION STOPPED",0xffffc060);if(status)return status;
        snprintf(message,sizeof(message),"ERROR %d - REBOOT REQUIRED",session_status);
        status=combat_hud_centered(sink,context,244,message,0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,328,"REPORT THIS ERROR TO THE DEVELOPER",0xffb8c0d0);
    }
    if(flags&RF_SCENE_FRONTEND_LOADING){
        if(page==RF_SCENE_FRONTEND_EXIT){
            status=combat_hud_centered(sink,context,196,"RETURNING TO DASHBOARD",0xff80ffb0);if(status)return status;
            return combat_hud_centered(sink,context,244,"CHECKING RETIRED OWNERS",0xffb8c0d0);
        }
        if(page==RF_SCENE_FRONTEND_LOAD){
            status=combat_hud_centered(sink,context,196,"LOADING SAVED GAME",0xff80ffb0);if(status)return status;
            status=combat_hud_centered(sink,context,244,selection?"AUTOSAVE":"MANUAL SAVE",0xffeeeeee);if(status)return status;
            return combat_hud_centered(sink,context,292,"VALIDATING SAVED WORLD",0xffb8c0d0);
        }
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
    if(page==RF_SCENE_FRONTEND_LOAD){
        static const char *const sources[3]={"MANUAL SAVE","AUTOSAVE","BACK"};
        status=combat_hud_centered(sink,context,140,"LOAD GAME",0xffeeeeee);if(status)return status;
        for(i=0;i<3;i++){
            if(i==selection){status=combat_hud_rect(sink,context,160,176+i*38,320,30,0xff30485c);if(status)return status;}
            status=combat_hud_centered(sink,context,184+i*38,sources[i],i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
        }
        if(flags&RF_SCENE_FRONTEND_LOAD_NOTICE){
            char code[32];const char *message;
            if(load_outcome==RF_SCENE_FRONTEND_LOAD_MISSING)
                message=selection?"NO AUTOSAVE FOUND":"NO MANUAL SAVE FOUND";
            else if(load_outcome==RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE)
                message="SAVE IS INCOMPATIBLE OR DAMAGED";
            else message="COULD NOT READ SAVE OR GAME DATA";
            status=combat_hud_centered(sink,context,306,message,0xffffc060);if(status)return status;
            snprintf(code,sizeof(code),"STATUS %d",load_status);
            status=combat_hud_centered(sink,context,336,code,0xffb8c0d0);if(status)return status;
        }else {
            status=combat_hud_centered(sink,context,316,"CHOOSE WHICH SAVE TO LOAD",0xffb8c0d0);if(status)return status;
        }
        if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
            return combat_hud_centered(sink,context,390,"CONNECT A CONTROLLER",0xffffc060);
        status=combat_hud_centered(sink,context,390,"D-PAD UP/DOWN   A LOAD / SELECT",0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,422,"B OR START BACK",0xffb8c0d0);
    }
    if(page==RF_SCENE_FRONTEND_EXIT){
        status=combat_hud_centered(sink,context,152,"EXIT TO DASHBOARD?",0xffeeeeee);if(status)return status;
        for(i=0;i<2;i++){
            if(i==selection){status=combat_hud_rect(sink,context,140,202+i*44,360,34,0xff30485c);if(status)return status;}
            status=combat_hud_centered(sink,context,210+i*44,i?"EXIT TO DASHBOARD":"CANCEL",
                i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
        }
        status=combat_hud_centered(sink,context,328,"UNSAVED SETTINGS WILL BE DISCARDED",0xffb8c0d0);if(status)return status;
        if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
            return combat_hud_centered(sink,context,382,"CONNECT A CONTROLLER",0xffffc060);
        status=combat_hud_centered(sink,context,382,"D-PAD UP/DOWN   A SELECT",0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,414,"B OR START CANCEL",0xffb8c0d0);
    }
    if(flags&RF_SCENE_FRONTEND_ENDED){
        status=combat_hud_centered(sink,context,150,"SESSION ENDED",0xffb8c0d0);if(status)return status;
    }
    for(i=0;i<4;i++){
        if(i==selection){status=combat_hud_rect(sink,context,150,176+i*38,340,32,0xff30485c);if(status)return status;}
        status=combat_hud_centered(sink,context,184+i*38,i==0?"NEW GAME":i==1?"OPTIONS":i==2?"LOAD GAME":"EXIT TO DASHBOARD",
            i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    status=combat_hud_centered(sink,context,354,selection==0?"START FRESH IN LIVE MINES":
        selection==1?"ADJUST CONTROLLER SETTINGS":selection==2?"CONTINUE FROM AN EXISTING SAVE":
        "LEAVE THIS GAME AFTER CONFIRMATION",0xffb8c0d0);if(status)return status;
    if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
        return combat_hud_centered(sink,context,380,"CONNECT A CONTROLLER",0xffffc060);
    return combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A OR START SELECT",0xffeeeeee);
}
int rf_scene_draw_options(rf_scene_particle_sink sink,void *context,uint32_t selection,
    uint32_t flags,uint32_t sensitivity_percent,uint32_t invert_y,uint32_t deadzone_percent)
{
    uint32_t i,storage=flags&RF_SCENE_OPTIONS_STORAGE_MASK;int status;char text[64];
    const char *notice="CHANGES APPLY FOR THIS SESSION",*detail="SELECT SAVE SETTINGS TO KEEP CHANGES";
    if(!sink || particle_draw_stream || selection>=RF_SCENE_OPTIONS_COUNT ||
       sensitivity_percent<25 || sensitivity_percent>200 || invert_y>1 || deadzone_percent>40 ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED|
           RF_SCENE_OPTIONS_STORAGE_MASK|RF_SCENE_OPTIONS_SOUTHPAW)))return RF_RANGE;
    status=combat_hud_centered(sink,context,58,"CONTROLLER OPTIONS",0xffeeeeee);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED){
        status=combat_hud_centered(sink,context,172,"CONTROLLER DISCONNECTED",0xffffc060);if(status)return status;
        return combat_hud_centered(sink,context,224,"CONNECT A CONTROLLER TO CONTINUE",0xffeeeeee);
    }
    for(i=0;i<RF_SCENE_OPTIONS_COUNT;i++){
        if(i==RF_SCENE_OPTIONS_SENSITIVITY)snprintf(text,sizeof(text),"LOOK SENSITIVITY   %u%%",sensitivity_percent);
        else if(i==RF_SCENE_OPTIONS_INVERT)snprintf(text,sizeof(text),"INVERT Y   %s",invert_y?"ON":"OFF");
        else if(i==RF_SCENE_OPTIONS_DEADZONE)snprintf(text,sizeof(text),"STICK DEADZONE   %u%%",deadzone_percent);
        else if(i==RF_SCENE_OPTIONS_STICK_LAYOUT)snprintf(text,sizeof(text),"STICK LAYOUT   %s",
            (flags&RF_SCENE_OPTIONS_SOUTHPAW)?"SOUTHPAW":"STANDARD");
        else snprintf(text,sizeof(text),"%s",i==RF_SCENE_OPTIONS_DEFAULTS?"RESET DEFAULTS":
            i==RF_SCENE_OPTIONS_BACK?"BACK":"SAVE SETTINGS");
        if(i==selection){status=combat_hud_rect(sink,context,104,100+i*28,432,26,0xff30485c);if(status)return status;}
        status=combat_hud_centered(sink,context,106+i*28,text,i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    if(storage==RF_SCENE_OPTIONS_STORED){notice="SETTINGS SAVED FOR THE NEXT BOOT";detail="RESET DEFAULTS DOES NOT SAVE AUTOMATICALLY";}
    else if(storage==RF_SCENE_OPTIONS_CHANGED)notice="CURRENT CHANGES ARE NOT SAVED";
    else if(storage==RF_SCENE_OPTIONS_MISSING)notice="NO SAVED SETTINGS YET";
    else if(storage==RF_SCENE_OPTIONS_BLOCKED){notice="SETTINGS FILE NEEDS ATTENTION";detail="EXISTING FILES KEPT   SAVING BLOCKED";}
    else if(storage==RF_SCENE_OPTIONS_RECOVERED){notice="OLDER SETTINGS RECOVERED";detail="DAMAGED OR UNKNOWN FILE KEPT   SAVE BLOCKED";}
    else if(storage==RF_SCENE_OPTIONS_STORAGE_ERROR){notice="STORAGE ERROR   SAVE NOT CONFIRMED";detail="CURRENT CONTROLS STILL WORK";}
    else if(storage==RF_SCENE_OPTIONS_STORAGE_LOCKED){notice="STORAGE CLOSE FAILED   REBOOT TO RETRY";detail="CURRENT CONTROLS STILL WORK";}
    status=combat_hud_centered(sink,context,314,notice,storage>=RF_SCENE_OPTIONS_BLOCKED?0xffffc060:0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,334,detail,0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,360,(flags&RF_SCENE_OPTIONS_SOUTHPAW)?
        "RIGHT STICK MOVES   LEFT STICK LOOKS":"LEFT STICK MOVES   RIGHT STICK LOOKS",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,382,"STICK CLICKS AND BUTTONS UNCHANGED",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,408,"UP/DOWN SELECT   LEFT/RIGHT ADJUST",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,434,
        (flags&RF_SCENE_OPTIONS_PAUSED)?"A SELECT   B BACK   START RESUME":"A SELECT   B OR START BACK",0xffb8c0d0);
}
