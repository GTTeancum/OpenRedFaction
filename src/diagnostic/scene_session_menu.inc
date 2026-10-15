#include "rf/frontend_load.h"
/* First-playable platform session UI, not recovered retail menu behavior.
 * This pass is called while the scene/input provider is suspended. It uses
 * the existing untextured fallback glyphs with Latin-1 UI accents, with no
 * scene/image borrower,
 * allocation, timer mutation or storage access. */
int rf_scene_draw_session_menu(rf_scene_particle_sink sink,void *context,
    uint32_t selection,uint32_t flags)
{
    static const char *const items[RF_SCENE_SESSION_COUNT]={
        "RESUME","QUICK SAVE","QUICK LOAD","RESTART SECTION","END SESSION","OPTIONS","MESSAGE HISTORY"
    };
    static const uint32_t ids[RF_SCENE_SESSION_COUNT]={59,820,821,0,0,43,156};
    uint32_t i;int status;
    if(!sink || particle_draw_stream || selection>=RF_SCENE_SESSION_COUNT ||
       (flags&~(RF_SCENE_SESSION_CONFIRM|RF_SCENE_SESSION_DISCONNECTED|
          RF_SCENE_SESSION_ENDED|RF_SCENE_SESSION_UNAVAILABLE|RF_SCENE_SESSION_FRONTEND_EXIT|RF_SCENE_SESSION_OPTIONS_UNAVAILABLE)))return RF_RANGE;
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
        status=combat_hud_centered(sink,context,122+i*32,scene_text(ids[i],items[i]),i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    if(flags&RF_SCENE_SESSION_OPTIONS_UNAVAILABLE){
        status=combat_hud_centered(sink,context,352,"OPTIONS ART COULD NOT BE LOADED",0xffffc060);if(status)return status;
    }
    if(flags&RF_SCENE_SESSION_UNAVAILABLE){
        status=combat_hud_centered(sink,context,352,"RESTART UNAVAILABLE IN THIS SCENE",0xffffc060);if(status)return status;
    }
    status=combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A SELECT",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,412,"B OR START RESUME",0xffb8c0d0);
}

/* Tester launch policy only; no retail menu reconstruction is implied. */
static const char *scene_difficulty_name(uint32_t difficulty)
{
    static const char *const fallback[4]={"EASY","MEDIUM","HARD","IMPOSSIBLE"};
    return scene_text(85u+difficulty,fallback[difficulty]);
}
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
        return combat_hud_centered(sink,context,292,scene_difficulty_name(difficulty),0xffeeeeee);
    }
    if(page==RF_SCENE_FRONTEND_DIFFICULTY){
        status=combat_hud_centered(sink,context,140,scene_text(123,"NEW GAME - DIFFICULTY"),0xffeeeeee);if(status)return status;
        for(i=0;i<5;i++){
            if(i==selection){status=combat_hud_rect(sink,context,160,178+i*34,320,28,0xff30485c);if(status)return status;}
            status=combat_hud_centered(sink,context,184+i*34,i<4?scene_difficulty_name(i):scene_text(788,"BACK"),
                i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
        }
        if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
            return combat_hud_centered(sink,context,380,"CONNECT A CONTROLLER",0xffffc060);
        status=combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A START / SELECT",0xffeeeeee);if(status)return status;
        return combat_hud_centered(sink,context,412,"B OR START BACK",0xffb8c0d0);
    }
    if(page==RF_SCENE_FRONTEND_LOAD){
        static const char *const sources[3]={"MANUAL SAVE","AUTOSAVE","BACK"};
        status=combat_hud_centered(sink,context,140,scene_text(986,"LOAD GAME"),0xffeeeeee);if(status)return status;
        for(i=0;i<3;i++){
            if(i==selection){status=combat_hud_rect(sink,context,160,176+i*38,320,30,0xff30485c);if(status)return status;}
            status=combat_hud_centered(sink,context,184+i*38,i==2?scene_text(788,sources[i]):sources[i],i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
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
            status=combat_hud_centered(sink,context,210+i*44,i?"EXIT TO DASHBOARD":scene_text(202,"CANCEL"),
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
        status=combat_hud_centered(sink,context,184+i*38,i==0?scene_text(42,"NEW GAME"):i==1?scene_text(43,"OPTIONS"):i==2?scene_text(986,"LOAD GAME"):"EXIT TO DASHBOARD",
            i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    status=combat_hud_centered(sink,context,354,selection==0?"START FRESH IN LIVE MINES":
        selection==1?"ADJUST CONTROLS, AUDIO AND TEXT":selection==2?"CONTINUE FROM AN EXISTING SAVE":
        "LEAVE THIS GAME AFTER CONFIRMATION",0xffb8c0d0);if(status)return status;
    if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
        return combat_hud_centered(sink,context,380,"CONNECT A CONTROLLER",0xffffc060);
    return combat_hud_centered(sink,context,380,"D-PAD UP/DOWN   A OR START SELECT",0xffeeeeee);
}
/* Shared notices preserve the explicit-save and failure distinctions on
 * both bounded option pages. Drawing never reads or writes storage. */
static int scene_options_storage_notice(rf_scene_particle_sink sink,void *context,uint32_t flags)
{
    uint32_t storage=flags&RF_SCENE_OPTIONS_STORAGE_MASK;int status;
    const char *notice="SETTINGS RETAINED UNTIL GAME CLOSES",*detail="SELECT SAVE SETTINGS TO KEEP CHANGES";
    if(storage==RF_SCENE_OPTIONS_STORED){notice="SETTINGS SAVED FOR THE NEXT BOOT";detail="RESET DEFAULTS DOES NOT SAVE AUTOMATICALLY";}
    else if(storage==RF_SCENE_OPTIONS_CHANGED)notice="CURRENT CHANGES ARE NOT SAVED";
    else if(storage==RF_SCENE_OPTIONS_MISSING)notice="NO SAVED SETTINGS YET";
    else if(storage==RF_SCENE_OPTIONS_BLOCKED){notice="SETTINGS FILE NEEDS ATTENTION";detail="EXISTING FILES KEPT   SAVING BLOCKED";}
    else if(storage==RF_SCENE_OPTIONS_RECOVERED){notice="OLDER SETTINGS RECOVERED";detail="DAMAGED OR UNKNOWN FILE KEPT   SAVE BLOCKED";}
    else if(storage==RF_SCENE_OPTIONS_STORAGE_ERROR){notice="STORAGE ERROR   SAVE NOT CONFIRMED";detail="CURRENT SETTINGS ARE RETAINED";}
    else if(storage==RF_SCENE_OPTIONS_STORAGE_LOCKED){notice="STORAGE CLOSE FAILED   REBOOT TO RETRY";detail="CURRENT SETTINGS ARE RETAINED";}
    status=combat_hud_centered(sink,context,310,notice,storage>=RF_SCENE_OPTIONS_BLOCKED?0xffffc060:0xffb8c0d0);if(status)return status;
    return combat_hud_centered(sink,context,330,detail,0xffb8c0d0);
}
int rf_scene_draw_options(rf_scene_particle_sink sink,void *context,uint32_t selection,
    uint32_t flags,uint32_t sensitivity_percent,uint32_t invert_y,uint32_t deadzone_percent)
{
    uint32_t i,first=(selection/5u)*5u;int status;char text[64];
    if(!sink || particle_draw_stream || selection>=RF_SCENE_OPTIONS_COUNT ||
       sensitivity_percent<25 || sensitivity_percent>200 || invert_y>1 || deadzone_percent>40 ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED|
           RF_SCENE_OPTIONS_STORAGE_MASK|RF_SCENE_OPTIONS_SOUTHPAW|RF_SCENE_OPTIONS_AUDIO_ERROR)))return RF_RANGE;
    snprintf(text,sizeof(text),"%s   %u / 2",scene_text(43,"OPTIONS"),selection/5u+1);
    status=combat_hud_centered(sink,context,48,text,0xffeeeeee);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED){
        status=combat_hud_centered(sink,context,172,"CONTROLLER DISCONNECTED",0xffffc060);if(status)return status;
        return combat_hud_centered(sink,context,224,"CONNECT A CONTROLLER TO CONTINUE",0xffeeeeee);
    }
    for(i=first;i<first+5u && i<RF_SCENE_OPTIONS_COUNT;i++){
        uint32_t y=100+(i-first)*36;
        if(i==RF_SCENE_OPTIONS_SENSITIVITY)snprintf(text,sizeof(text),"%s   %u%%",scene_text(143,"LOOK SENSITIVITY"),sensitivity_percent);
        else if(i==RF_SCENE_OPTIONS_INVERT)snprintf(text,sizeof(text),"%s   %s",scene_text(135,"INVERT Y"),invert_y?scene_text(169,"ON"):scene_text(170,"OFF"));
        else if(i==RF_SCENE_OPTIONS_DEADZONE)snprintf(text,sizeof(text),"STICK DEADZONE   %u%%",deadzone_percent);
        else if(i==RF_SCENE_OPTIONS_STICK_LAYOUT)snprintf(text,sizeof(text),"STICK LAYOUT   %s",
            (flags&RF_SCENE_OPTIONS_SOUTHPAW)?"SOUTHPAW":"STANDARD");
        else if(i==RF_SCENE_OPTIONS_LANGUAGE)snprintf(text,sizeof(text),"%s   %s",scene_text(590,"TEXT LANGUAGE"),scene_text_language_name());
        else if(i==RF_SCENE_OPTIONS_BUTTONS)snprintf(text,sizeof(text),"BUTTON MAPPING");
        else if(i==RF_SCENE_OPTIONS_AUDIO)snprintf(text,sizeof(text),"AUDIO");
        else snprintf(text,sizeof(text),"%s",i==RF_SCENE_OPTIONS_DEFAULTS?scene_text(138,"RESET DEFAULTS"):
            i==RF_SCENE_OPTIONS_BACK?scene_text(788,"BACK"):"SAVE SETTINGS");
        if(i==selection){status=combat_hud_rect(sink,context,104,y-5,432,28,0xff30485c);if(status)return status;}
        status=combat_hud_centered(sink,context,y,text,i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    status=scene_options_storage_notice(sink,context,flags);if(status)return status;
    status=combat_hud_centered(sink,context,360,(flags&RF_SCENE_OPTIONS_SOUTHPAW)?
        "RIGHT STICK MOVES   LEFT STICK LOOKS":"LEFT STICK MOVES   RIGHT STICK LOOKS",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,382,(flags&RF_SCENE_OPTIONS_AUDIO_ERROR)?
        "AUDIO APPLY FAILED   OPEN AUDIO TO RETRY":selection==RF_SCENE_OPTIONS_LANGUAGE?
        "UI LABELS   MISSING TEXT USES ENGLISH":selection==RF_SCENE_OPTIONS_AUDIO?
        "MASTER, EFFECTS AND MUSIC VOLUME":"BUTTON MAPPING CHANGES GAMEPLAY ONLY",
        (flags&RF_SCENE_OPTIONS_AUDIO_ERROR)?0xffffc060:0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,408,"UP/DOWN SELECT   LEFT/RIGHT ADJUST",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,434,
        (flags&RF_SCENE_OPTIONS_PAUSED)?"A SELECT   B BACK   START RESUME":"A SELECT   B OR START BACK",0xffb8c0d0);
}
int rf_scene_draw_audio_options(rf_scene_particle_sink sink,void *context,uint32_t selection,
    uint32_t flags,uint32_t master_percent,uint32_t effects_percent,uint32_t music_percent)
{
    uint32_t i;int status;char text[64];
    if(!sink || particle_draw_stream || selection>=RF_SCENE_AUDIO_COUNT ||
       master_percent>100 || effects_percent>100 || music_percent>100 ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED|RF_SCENE_OPTIONS_STORAGE_MASK|RF_SCENE_OPTIONS_AUDIO_ERROR)))return RF_RANGE;
    status=combat_hud_centered(sink,context,48,"AUDIO",0xffeeeeee);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED){
        status=combat_hud_centered(sink,context,172,"CONTROLLER DISCONNECTED",0xffffc060);if(status)return status;
        return combat_hud_centered(sink,context,224,"CONNECT A CONTROLLER TO CONTINUE",0xffeeeeee);
    }
    for(i=0;i<RF_SCENE_AUDIO_COUNT;i++){
        uint32_t y=100+i*32;
        if(i==RF_SCENE_AUDIO_MASTER)snprintf(text,sizeof(text),"MASTER VOLUME   %u%%",master_percent);
        else if(i==RF_SCENE_AUDIO_EFFECTS)snprintf(text,sizeof(text),"EFFECTS VOLUME   %u%%",effects_percent);
        else if(i==RF_SCENE_AUDIO_MUSIC)snprintf(text,sizeof(text),"MUSIC VOLUME   %u%%",music_percent);
        else snprintf(text,sizeof(text),"%s",i==RF_SCENE_AUDIO_DEFAULTS?"RESET AUDIO":
            i==RF_SCENE_AUDIO_SAVE?"SAVE SETTINGS":scene_text(788,"BACK"));
        if(i==selection){status=combat_hud_rect(sink,context,104,y-5,432,28,0xff30485c);if(status)return status;}
        status=combat_hud_centered(sink,context,y,text,i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    status=scene_options_storage_notice(sink,context,flags);if(status)return status;
    status=combat_hud_centered(sink,context,360,"MASTER SCALES EFFECTS AND MUSIC",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,382,(flags&RF_SCENE_OPTIONS_AUDIO_ERROR)?
        "AUDIO APPLY FAILED   ADJUST TO RETRY":"EFFECTS INCLUDES SPEECH AND AMBIENCE",
        (flags&RF_SCENE_OPTIONS_AUDIO_ERROR)?0xffffc060:0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,408,"UP/DOWN SELECT   LEFT/RIGHT ADJUST",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,434,
        (flags&RF_SCENE_OPTIONS_PAUSED)?"A SELECT   B BACK   START RESUME":"A SELECT   B OR START BACK",0xffb8c0d0);
}

int rf_scene_draw_button_mapping(rf_scene_particle_sink sink,void *context,
    uint32_t selection,uint32_t flags,const char *const *actions,const char *const *controls)
{
    static const uint32_t action_ids[RF_SCENE_BUTTON_ACTION_COUNT]={145,146,148,147,151,149,152,153,154,150};
    uint32_t first=(selection/6u)*6u,i;int status;char text[80];
    if(!sink || particle_draw_stream || selection>=RF_SCENE_BUTTON_COUNT || !actions || !controls ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED)))return RF_RANGE;
    for(i=0;i<RF_SCENE_BUTTON_ACTION_COUNT;i++)if(!actions[i] || !controls[i])return RF_RANGE;
    snprintf(text,sizeof(text),"BUTTON MAPPING   %u / 2",selection/6u+1);
    status=combat_hud_centered(sink,context,50,text,0xffeeeeee);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED){
        status=combat_hud_centered(sink,context,172,"CONTROLLER DISCONNECTED",0xffffc060);if(status)return status;
        return combat_hud_centered(sink,context,224,"CONNECT A CONTROLLER TO CONTINUE",0xffeeeeee);
    }
    for(i=first;i<first+6u;i++){
        uint32_t y=100+(i-first)*32;
        if(i<RF_SCENE_BUTTON_ACTION_COUNT)snprintf(text,sizeof(text),"%s   %s",scene_text(action_ids[i],actions[i]),controls[i]);
        else snprintf(text,sizeof(text),"%s",i==RF_SCENE_BUTTON_DEFAULTS?"RESET BUTTONS":scene_text(788,"BACK"));
        if(i==selection){status=combat_hud_rect(sink,context,66,y-5,508,28,0xff30485c);if(status)return status;}
        status=combat_hud_centered(sink,context,y,text,i==selection?0xff80ffb0:0xffd0d8e0);if(status)return status;
    }
    status=combat_hud_centered(sink,context,302,"ASSIGNING A BUTTON SWAPS BOTH ACTIONS",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,324,"D-PAD / START / BACK / MENUS STAY FIXED",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,346,"BACK+Y SAVE   BACK+X LOAD STAY FIXED",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,368,"RELEASE ALL BUTTONS BEFORE RESUMING",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,390,"KEEP CHANGES VIA OPTIONS / SAVE SETTINGS",0xffb8c0d0);if(status)return status;
    status=combat_hud_centered(sink,context,416,"UP/DOWN SELECT   LEFT/RIGHT OR A SWAP",0xffeeeeee);if(status)return status;
    return combat_hud_centered(sink,context,442,
        (flags&RF_SCENE_OPTIONS_PAUSED)?"B BACK   START RESUME":"B OR START BACK",0xffb8c0d0);
}
