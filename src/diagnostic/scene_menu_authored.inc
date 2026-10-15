#include "rf/menu_assets.h"
/* Authored source geometry plus Xbox-specific functional settings. The asset
 * owner and platform input/storage remain outside this draw-only layer. */
typedef struct scene_menu_motion_context {
    rf_scene_particle_sink sink;void *context;float dx;
} scene_menu_motion_context;
/* Body widgets use their own source panel origin; viewport decorations and
 * category/title rails remain on their independent presentation tracks. */
static int scene_menu_motion_present(void *context,const rf_particle_draw_vertex *vertices,
    uint32_t count,const rf_image *image,uint32_t mode)
{
    scene_menu_motion_context *m=context;rf_particle_draw_vertex moved[4];uint32_t i;
    if(!m || !vertices || count>4)return RF_RANGE;
    for(i=0;i<count;i++){moved[i]=vertices[i];moved[i].screen[0]+=m->dx;}
    return m->sink(m->context,moved,count,image,mode);
}
static int scene_menu_sprite(rf_scene_particle_sink sink,void *context,const rf_menu_assets *a,
    uint32_t id,float x,float y,uint32_t color,uint32_t additive)
{
    rf_hud_sprite s;rf_particle_draw_vertex v[4]={{0}};uint32_t i;int status;
    status=rf_menu_assets_sprite(a,id,&s);if(status)return status;
    if(!s.width || !s.height)return RF_OK;
    for(i=0;i<4;i++){
        uint32_t right=i==1||i==2,bottom=i>=2;
        v[i].screen[0]=x+(right?s.width:0);v[i].screen[1]=y+(bottom?s.height:0);
        v[i].uv[0]=s.uv[right?2:0];v[i].uv[1]=s.uv[bottom?3:1];
        v[i].reciprocal_w=1;v[i].argb=color;
    }
    return sink(context,v,4,s.image,(additive?RF_PARTICLE_GLOW_MODE:RF_PARTICLE_NORMAL_MODE)&~(31u<<20));
}
static int scene_menu_text(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,
    uint32_t font,float x,float y,float width,const char *text,uint32_t color,uint32_t centered)
{
    uint32_t measured,i;float scale=1;int status;
    status=rf_menu_assets_text_width(a,font,text,&measured);if(status)return status;
    if(width>0 && measured>width)scale=width/measured;
    if(centered && width>0)x+=(width-measured*scale)*.5f;
    for(i=0;text[i];i++){
        rf_hud_glyph g;rf_particle_draw_vertex v[4]={{0}};uint32_t j;
        status=rf_menu_assets_glyph(a,font,(unsigned char)text[i],&g);if(status)return status;
        if(g.sprite.width && g.sprite.height){
            for(j=0;j<4;j++){
                uint32_t right=j==1||j==2,bottom=j>=2;
                v[j].screen[0]=x+(right?g.sprite.width*scale:0);
                v[j].screen[1]=y+(bottom?g.sprite.height*scale:0);
                v[j].uv[0]=g.sprite.uv[right?2:0];v[j].uv[1]=g.sprite.uv[bottom?3:1];
                v[j].reciprocal_w=1;v[j].argb=color;
            }
            status=sink(context,v,4,g.sprite.image,RF_PARTICLE_NORMAL_MODE&~(31u<<20));if(status)return status;
        }
        x+=g.advance*scale;
    }
    return RF_OK;
}
static int scene_menu_centered(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,
    float y,const char *text,uint32_t color)
{return scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,24,y,592,text,color,1);}
static int scene_menu_background(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context)
{
    uint32_t part,i;int status=scene_menu_sprite(sink,context,a,RF_MENU_BACKGROUND,0,0,0xffffffffu,0);
    if(status)return status;
    for(part=0;part<2;part++){
        rf_hud_sprite sp;rf_particle_draw_vertex v[4]={{0}};
        status=rf_menu_assets_title_piece(a,part,&sp);
        if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
        for(i=0;i<4;i++){
            uint32_t right=i==1||i==2,bottom=i>=2;
            v[i].screen[0]=125+rf_menu_assets_title_offset(a)+(part?382:0)+(right?sp.width:0);
            v[i].screen[1]=bottom?sp.height:0;v[i].reciprocal_w=1;v[i].argb=0xffffffffu;
            v[i].uv[0]=sp.uv[right?2:0];v[i].uv[1]=sp.uv[bottom?3:1];
        }
        status=sink(context,v,4,sp.image,RF_PARTICLE_NORMAL_MODE&~(31u<<20));if(status)return status;
    }
    return RF_OK;
}
static int scene_menu_borders(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context)
{
    uint32_t side,i;int status;
    /* Original442db0 retains source width, aligns the right edge and scales
     * the border height to the viewport. The mod's16px bars are transparent. */
    for(side=0;side<2;side++){
        rf_hud_sprite sp;rf_particle_draw_vertex v[4]={{0}};
        status=rf_menu_assets_sprite(a,side?RF_MENU_BAR_RIGHT:RF_MENU_BAR_LEFT,&sp);if(status)return status;
        for(i=0;i<4;i++){
            uint32_t right=i==1||i==2,bottom=i>=2;
            v[i].screen[0]=(side?640.f-sp.width:0)+(right?sp.width:0);
            v[i].screen[1]=bottom?480:0;v[i].reciprocal_w=1;v[i].argb=0xffffffffu;
            v[i].uv[0]=sp.uv[right?2:0];v[i].uv[1]=sp.uv[bottom?3:1];
        }
        status=sink(context,v,4,sp.image,RF_PARTICLE_NORMAL_MODE&~(31u<<20));if(status)return status;
    }
    return RF_OK;
}
static int scene_menu_button(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,
    uint32_t row,const char *text,uint32_t selected,uint32_t more,uint32_t disabled)
{
    float x=11+rf_menu_assets_rail_offset(a),y=58+row*61;uint32_t height=rf_menu_assets_font_height(a,RF_MENU_FONT_MEDIUM);int status;
    status=scene_menu_sprite(sink,context,a,more?RF_MENU_BUTTON_MORE:RF_MENU_BUTTON,x,y,0xffffffffu,0);if(status)return status;
    if(selected){status=scene_menu_sprite(sink,context,a,RF_MENU_BUTTON_SELECTED,x,y,0xffffffffu,1);if(status)return status;}
    return scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,x,y+(61-height)*.5f,204,text,
        disabled?0xff606060u:selected?0xfff0f0f0u:0xffc0c0c0u,1);
}
static int scene_menu_main_rail(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,uint32_t selection)
{
    uint32_t i;int status;
    for(i=0;i<4;i++){
        const char *label=i==0?scene_text(42,"NEW GAME"):i==1?scene_text(43,"OPTIONS"):
            i==2?scene_text(986,"LOAD GAME"):"EXIT";
        status=scene_menu_button(a,sink,context,i,label,i==selection,i<3,0);if(status)return status;
    }
    return RF_OK;
}
static int scene_menu_popup_art(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context)
{
    static const uint32_t order[4]={1,3,0,2};uint32_t n;int status;
    status=combat_hud_rect(sink,context,130,158,380,163,0x80000000u);if(status)return status;
    for(n=0;n<4;n++){
        uint32_t part=order[n],i;rf_hud_sprite sp;rf_particle_draw_vertex v[4]={{0}};
        float x=part>=2?502:120,y=part&1?148:120;
        status=rf_menu_assets_popup_piece(a,part,&sp);if(status)return status;
        for(i=0;i<4;i++){
            uint32_t right=i==1||i==2,bottom=i>=2;
            v[i].screen[0]=x+(right?sp.width:0);v[i].screen[1]=y+(bottom?sp.height:0);
            v[i].uv[0]=sp.uv[right?2:0];v[i].uv[1]=sp.uv[bottom?3:1];
            v[i].reciprocal_w=1;v[i].argb=0xffffffffu;
        }
        status=sink(context,v,4,sp.image,RF_PARTICLE_NORMAL_MODE&~(31u<<20));if(status)return status;
    }
    return RF_OK;
}
static int scene_menu_options_shell(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,uint32_t category)
{
    static const char *const names[]={"GAME","VIDEO","AUDIO","CONTROLS","BACK"};
    uint32_t i;int status=scene_menu_background(a,sink,context);if(status)return status;
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_LARGE,265+rf_menu_assets_title_offset(a),12,320,scene_text(114,"Options"),0xff000000u,0);if(status)return status;
    for(i=0;i<5;i++){
        status=scene_menu_button(a,sink,context,i,scene_text(784+i,names[i]),i==category,i<4,i==1);if(status)return status;
    }
    return RF_OK;
}
static int scene_menu_storage_notice(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,uint32_t flags)
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
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,235,351,390,notice,
        storage>=RF_SCENE_OPTIONS_BLOCKED?0xffffc060u:0xffc0c0c0u,0);if(status)return status;
    return scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,235,368,390,detail,0xffc0c0c0u,0);
}
static int scene_menu_options_footer(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,uint32_t flags)
{
    int status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED){
        status=scene_menu_centered(a,sink,context,445,"CONNECT A CONTROLLER TO CONTINUE",0xffffc060u);if(status)return status;
    }else{
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,24,440,592,
            "D-PAD SELECT / ADJUST   A SELECT   B BACK",0xffc0c0c0u,1);if(status)return status;
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,24,455,592,
            (flags&RF_SCENE_OPTIONS_PAUSED)?"START RESUME":"START BACK",0xffc0c0c0u,1);if(status)return status;
    }
    return scene_menu_borders(a,sink,context);
}
static int scene_menu_list_row(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,
    float x,float y,float width,const char *label,const char *value,uint32_t selected,uint32_t indicator)
{
    int status;uint32_t color=selected?0xfff0f0f0u:0xff000000u;
    if(selected && indicator){status=scene_menu_sprite(sink,context,a,RF_MENU_INDICATOR_SELECTED,235,y-7,0xffffffffu,1);if(status)return status;}
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,x,y,value?170:width,label,color,0);if(status)return status;
    if(value)return scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,x+170,y,width-170,value,color,0);
    return RF_OK;
}
/* Source442a40/450236 binding-list palette: normal cyan/alpha192;
 * selected cyan strip and opaque black text, not panel-static text colors. */
static int scene_menu_binding_row(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,
    float y,const char *label,const char *value,uint32_t selected)
{
    uint32_t color=selected?0xff000000u:0xc032c8c8u;int status;
    if(selected){status=combat_hud_rect(sink,context,260,y,280,12,0xc032c8c8u);if(status)return status;}
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,260,y,value?170:280,label,color,0);if(status)return status;
    if(value)return scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,430,y,110,value,color,0);
    return RF_OK;
}
static int scene_menu_lit_button(const rf_menu_assets *a,rf_scene_particle_sink sink,void *context,
    float x,float y,const char *label,uint32_t selected)
{
    int status;
    if(selected){status=scene_menu_sprite(sink,context,a,RF_MENU_LIT_SELECTED,x,y,0xffffffffu,1);if(status)return status;}
    return scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,x,y+6,167,label,
        selected?0xfff0f0f0u:0xff000000u,1);
}
int rf_scene_draw_options_page(rf_scene_particle_sink sink,void *context,const rf_menu_assets *a,
    uint32_t page,uint32_t selection,uint32_t flags,uint32_t sensitivity,uint32_t invert,uint32_t deadzone)
{
    scene_menu_motion_context motion={sink,context,(float)rf_menu_assets_body_offset(a)};
    static const uint32_t game[]={RF_SCENE_OPTIONS_LANGUAGE,RF_SCENE_OPTIONS_DEFAULTS,RF_SCENE_OPTIONS_SAVE,RF_SCENE_OPTIONS_BACK};
    static const uint32_t controls[]={RF_SCENE_OPTIONS_SENSITIVITY,RF_SCENE_OPTIONS_INVERT,RF_SCENE_OPTIONS_DEADZONE,
        RF_SCENE_OPTIONS_STICK_LAYOUT,RF_SCENE_OPTIONS_BUTTONS,RF_SCENE_OPTIONS_BACK};
    const uint32_t *rows=page==RF_SCENE_OPTIONS_PAGE_GAME?game:controls;
    uint32_t count=page==RF_SCENE_OPTIONS_PAGE_GAME?4:6,i,found=0;int status;
    if(!a || !sink || particle_draw_stream || page>RF_SCENE_OPTIONS_PAGE_CONTROLS || sensitivity<25 || sensitivity>200 || invert>1 || deadzone>40 ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED|RF_SCENE_OPTIONS_STORAGE_MASK|RF_SCENE_OPTIONS_SOUTHPAW|RF_SCENE_OPTIONS_AUDIO_ERROR)))return RF_RANGE;
    if(page==RF_SCENE_OPTIONS_PAGE_ROOT){if(selection>=RF_SCENE_OPTIONS_CATEGORY_COUNT)return RF_RANGE;}
    else {for(i=0;i<count;i++)if(rows[i]==selection)found=1;if(!found)return RF_RANGE;}
    status=scene_menu_options_shell(a,sink,context,page==RF_SCENE_OPTIONS_PAGE_ROOT?selection:
        page==RF_SCENE_OPTIONS_PAGE_GAME?RF_SCENE_OPTIONS_CATEGORY_GAME:RF_SCENE_OPTIONS_CATEGORY_CONTROLS);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED)return scene_menu_options_footer(a,sink,context,flags);
    sink=scene_menu_motion_present;context=&motion;
    if(page==RF_SCENE_OPTIONS_PAGE_ROOT){
        static const char *const descriptions[]={"TEXT LANGUAGE AND SAVED SETTINGS","CONSOLE VIDEO SETTINGS ARE FIXED",
            "MASTER, EFFECTS AND MUSIC VOLUME","CONTROLLER SETTINGS AND BUTTON MAPPING","RETURN TO THE PREVIOUS MENU"};
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,235,112,385,descriptions[selection],0xffc0c0c0u,0);if(status)return status;
    }else if(page==RF_SCENE_OPTIONS_PAGE_CONTROLS){
        char value[64];float knob=263+106.f*(sensitivity-25)/175;
        uint32_t focused=selection==RF_SCENE_OPTIONS_SENSITIVITY;
        /* Original4503d0 slider(138,21), label(285,22), marker(111,17),
         * checkbox(113,49), label(215,55), all relative to panel125,58. */
        status=scene_menu_sprite(sink,context,a,RF_MENU_SLIDER,knob,79,0xffffffffu,0);if(status)return status;
        if(focused){
            status=scene_menu_sprite(sink,context,a,RF_MENU_SLIDER_ON,knob,79,0xffffffffu,1);if(status)return status;
            status=scene_menu_sprite(sink,context,a,RF_MENU_INDICATOR_SELECTED,236,75,0xffffffffu,1);if(status)return status;
        }
        snprintf(value,sizeof(value),"%s %u%%",scene_text(143,"LOOK SENSITIVITY"),sensitivity);
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,410,80,210,value,
            focused?0xfff0f0f0u:0xff000000u,0);if(status)return status;
        status=scene_menu_sprite(sink,context,a,RF_MENU_CHECKBOX,238,107,0xffffffffu,0);if(status)return status;
        if(invert){status=scene_menu_sprite(sink,context,a,RF_MENU_CHECKBOX_CHECKED,238,107,0xffffffffu,1);if(status)return status;}
        if(selection==RF_SCENE_OPTIONS_INVERT){status=scene_menu_sprite(sink,context,a,RF_MENU_CHECKBOX_SELECTED,238,107,0xffffffffu,1);if(status)return status;}
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,340,113,280,scene_text(135,"INVERT Y"),
            selection==RF_SCENE_OPTIONS_INVERT?0xfff0f0f0u:0xff000000u,0);if(status)return status;
        /* Four Xbox-only rows deliberately reuse the original binding-list
         * viewport(135,113,280,164), rather than inventing extra top widgets. */
        for(i=2;i<6;i++){
            uint32_t id=controls[i];const char *label,*v=NULL;
            if(id==RF_SCENE_OPTIONS_DEADZONE){label="STICK DEADZONE";snprintf(value,sizeof(value),"%u%%",deadzone);v=value;}
            else if(id==RF_SCENE_OPTIONS_STICK_LAYOUT){label="STICK LAYOUT";v=(flags&RF_SCENE_OPTIONS_SOUTHPAW)?"SOUTHPAW":"STANDARD";}
            else label=id==RF_SCENE_OPTIONS_BUTTONS?"BUTTON MAPPING":scene_text(788,"BACK");
            status=scene_menu_binding_row(a,sink,context,171+(i-2)*28,label,v,id==selection);if(status)return status;
        }
    }else{
        uint32_t focused=selection==RF_SCENE_OPTIONS_LANGUAGE;
        /* Xbox language reuses source453130's difficulty cycle rectangle
         * (150,18,108,26), label(285,25) and adjacent19px arrows. */
        status=scene_menu_sprite(sink,context,a,RF_MENU_CYCLE_LEFT,256,76,0xffffffffu,0);if(status)return status;
        status=scene_menu_sprite(sink,context,a,RF_MENU_CYCLE_RIGHT,383,76,0xffffffffu,0);if(status)return status;
        if(focused){
            status=scene_menu_sprite(sink,context,a,RF_MENU_CYCLE_LEFT_ON,256,76,0xffffffffu,1);if(status)return status;
            status=scene_menu_sprite(sink,context,a,RF_MENU_CYCLE_RIGHT_ON,383,76,0xffffffffu,1);if(status)return status;
            status=scene_menu_sprite(sink,context,a,RF_MENU_INDICATOR_SELECTED,235,74,0xffffffffu,1);if(status)return status;
        }
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,275,83,108,scene_text_language_name(),
            focused?0xfff0f0f0u:0xff000000u,1);if(status)return status;
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,410,83,210,scene_text(590,"TEXT LANGUAGE"),
            focused?0xfff0f0f0u:0xff000000u,0);if(status)return status;
        /* Explicit save/default/back are Xbox actions in the original lower
         * priority-list space(133,193,208,124); no false weapon options. */
        for(i=1;i<4;i++){
            const char *label=game[i]==RF_SCENE_OPTIONS_DEFAULTS?scene_text(138,"RESET DEFAULTS"):
                game[i]==RF_SCENE_OPTIONS_SAVE?"SAVE SETTINGS":scene_text(788,"BACK");
            status=scene_menu_list_row(a,sink,context,258,251+(i-1)*28,208,label,NULL,game[i]==selection,1);if(status)return status;
        }
    }
    status=scene_menu_storage_notice(a,sink,context,flags);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_AUDIO_ERROR){status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,235,391,390,
        "AUDIO APPLY FAILED   OPEN AUDIO TO RETRY",0xffffc060u,0);if(status)return status;}
    return scene_menu_options_footer(a,motion.sink,motion.context,flags);
}
int rf_scene_draw_authored_audio_options(rf_scene_particle_sink sink,void *context,const rf_menu_assets *a,
    uint32_t selection,uint32_t flags,uint32_t master,uint32_t effects,uint32_t music)
{
    scene_menu_motion_context motion={sink,context,(float)rf_menu_assets_body_offset(a)};
    static const char *const labels[]={"MASTER VOLUME","EFFECTS VOLUME","MUSIC VOLUME","RESET AUDIO","SAVE SETTINGS","BACK"};
    const uint32_t gains[]={master,effects,music};uint32_t i;int status;
    if(!a || !sink || particle_draw_stream || selection>=RF_SCENE_AUDIO_COUNT || master>100 || effects>100 || music>100 ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED|RF_SCENE_OPTIONS_STORAGE_MASK|RF_SCENE_OPTIONS_AUDIO_ERROR)))return RF_RANGE;
    status=scene_menu_options_shell(a,sink,context,RF_SCENE_OPTIONS_CATEGORY_AUDIO);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED)return scene_menu_options_footer(a,sink,context,flags);
    sink=scene_menu_motion_present;context=&motion;
    for(i=0;i<3;i++){
        char text[64];float y=132+i*34;uint32_t color=i==selection?0xfff0f0f0u:0xff000000u;
        if(i==selection){status=scene_menu_sprite(sink,context,a,RF_MENU_INDICATOR_SELECTED,235,y-4,0xffffffffu,1);if(status)return status;}
        status=scene_menu_sprite(sink,context,a,RF_MENU_SLIDER,266+106.f*gains[i]/100,y,0xffffffffu,0);if(status)return status;
        if(i==selection){status=scene_menu_sprite(sink,context,a,RF_MENU_SLIDER_ON,266+106.f*gains[i]/100,y,0xffffffffu,1);if(status)return status;}
        snprintf(text,sizeof(text),"%s %u%%",i?scene_text(178+i,labels[i]):labels[i],gains[i]);
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,410,137+i*33,210,text,color,0);if(status)return status;
    }
    for(i=3;i<RF_SCENE_AUDIO_COUNT;i++){
        float y=255+(i-3)*30;
        if(i==selection){status=scene_menu_sprite(sink,context,a,RF_MENU_INDICATOR_SELECTED,235,y-7,0xffffffffu,1);if(status)return status;}
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,266,y,350,i==RF_SCENE_AUDIO_BACK?scene_text(788,labels[i]):labels[i],
            i==selection?0xfff0f0f0u:0xffc0c0c0u,0);if(status)return status;
    }
    status=scene_menu_storage_notice(a,sink,context,flags);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_AUDIO_ERROR){status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,235,391,390,
        "AUDIO APPLY FAILED   ADJUST TO RETRY",0xffffc060u,0);if(status)return status;}
    return scene_menu_options_footer(a,motion.sink,motion.context,flags);
}
/* Compact only the fixed-width list column. The selected full source label
 * remains visible below it, including HOLD for the fine-aim binding. */
static const char *scene_menu_binding_short(const char *value)
{
    if(!strcmp(value,"RIGHT-STICK CLICK"))return "R3";
    if(!strcmp(value,"LEFT-STICK CLICK"))return "L3";
    if(!strcmp(value,"HOLD RIGHT-STICK CLICK"))return "HOLD R3";
    if(!strcmp(value,"HOLD LEFT-STICK CLICK"))return "HOLD L3";
    return value;
}
int rf_scene_draw_authored_button_mapping(rf_scene_particle_sink sink,void *context,const rf_menu_assets *a,
    uint32_t selection,uint32_t flags,const char *const *actions,const char *const *controls)
{
    scene_menu_motion_context motion={sink,context,(float)rf_menu_assets_body_offset(a)};
    static const uint32_t action_ids[RF_SCENE_BUTTON_ACTION_COUNT]={145,146,148,147,151,149,152,153,154,150};
    uint32_t i;int status;
    if(!a || !sink || particle_draw_stream || selection>=RF_SCENE_BUTTON_COUNT || !actions || !controls ||
       (flags&~(RF_SCENE_OPTIONS_PAUSED|RF_SCENE_OPTIONS_DISCONNECTED)))return RF_RANGE;
    for(i=0;i<RF_SCENE_BUTTON_ACTION_COUNT;i++)if(!actions[i] || !controls[i])return RF_RANGE;
    status=scene_menu_options_shell(a,sink,context,RF_SCENE_OPTIONS_CATEGORY_CONTROLS);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED)return scene_menu_options_footer(a,sink,context,flags);
    sink=scene_menu_motion_present;context=&motion;
    /* Original450200 list: medium-font12px rows, action x135/binding x305,
     * inside panel-relative135,113,280,164. All ten Xbox bindings fit. */
    if(selection<RF_SCENE_BUTTON_ACTION_COUNT){
        status=scene_menu_sprite(sink,context,a,RF_MENU_INDICATOR_SELECTED,235,164,0xffffffffu,1);if(status)return status;
    }
    for(i=0;i<RF_SCENE_BUTTON_ACTION_COUNT;i++){
        status=scene_menu_binding_row(a,sink,context,171+i*12,
            scene_text(action_ids[i],actions[i]),scene_menu_binding_short(controls[i]),i==selection);if(status)return status;
    }
    if(selection<RF_SCENE_BUTTON_ACTION_COUNT){
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,260,303,280,
            controls[selection],0xc032c8c8u,0);if(status)return status;
    }
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,260,317,280,
        "LEFT/RIGHT OR A CYCLES; USED BUTTONS SWAP",0xc032c8c8u,0);if(status)return status;
    /* Original Defaults button(112,328,167,25). Back is an explicit Xbox
     * addition alongside it, within the panel; it is not Change Binding. */
    status=scene_menu_lit_button(a,sink,context,237,386,scene_text(138,"RESET DEFAULTS"),selection==RF_SCENE_BUTTON_DEFAULTS);if(status)return status;
    status=scene_menu_lit_button(a,sink,context,421,386,scene_text(788,"BACK"),selection==RF_SCENE_BUTTON_BACK);if(status)return status;
    return scene_menu_options_footer(a,motion.sink,motion.context,flags);
}

/* Original Load/confirmation widget coordinates. Manual/Autosave and dashboard
 * wording are Xbox adaptations; this layer never reads a save or changes input. */
static int scene_menu_frontend_hint(const rf_menu_assets *a,rf_scene_particle_sink sink,
    void *context,uint32_t flags,const char *hint,const char *back)
{
    int status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,64,440,512,
        (flags&RF_SCENE_FRONTEND_DISCONNECTED)?"CONNECT A CONTROLLER":hint,
        (flags&RF_SCENE_FRONTEND_DISCONNECTED)?0xffffc060u:0xffc0c0c0u,1);if(status)return status;
    if(!(flags&RF_SCENE_FRONTEND_DISCONNECTED)){
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,64,455,512,back,0xffc0c0c0u,1);if(status)return status;
    }
    return scene_menu_borders(a,sink,context);
}
static int scene_menu_popup_choice(const rf_menu_assets *a,rf_scene_particle_sink sink,
    void *context,float y,const char *label,uint32_t selected)
{
    int status;
    if(selected){status=scene_menu_sprite(sink,context,a,RF_MENU_POPUP_SELECTED,241,y,0xffffffffu,1);if(status)return status;}
    /* Original161x33 button uses integer-centered12px medium text. */
    return scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,241,y+10,161,label,
        selected?0xfff0f0f0u:0xffc0c0c0u,1);
}
static int scene_menu_load_page(const rf_menu_assets *a,rf_scene_particle_sink sink,
    void *context,uint32_t flags,int session_status,uint32_t selection,uint32_t outcome,int load_status)
{
    scene_menu_motion_context motion={sink,context,(float)rf_menu_assets_body_offset(a)};
    static const char *const saves[]={"MANUAL SAVE","AUTOSAVE"};
    uint32_t i;char code[48];int status=scene_menu_background(a,sink,context);if(status)return status;
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_LARGE,265+rf_menu_assets_title_offset(a),12,320,scene_text(986,"Load Game"),0xff000000u,0);if(status)return status;
    sink=scene_menu_motion_present;context=&motion;
    /* Source440556 Name box starts at65,90, with its native one-pixel text inset.
     * Date/time/game-time columns are deliberately omitted: no such metadata
     * is provided by the existing two-source frontend. */
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,66,90,223,scene_text(987,"   Name"),0xff000000u,0);if(status)return status;
    if(session_status){
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,64,151,489,
            (flags&RF_SCENE_FRONTEND_LOAD_NOTICE) && outcome==RF_SCENE_FRONTEND_LOAD_BLOCKED?
            "STORAGE / SESSION CLEANUP FAILED":"SESSION STOPPED",0xc032c8c8u,0);if(status)return status;
        snprintf(code,sizeof(code),"ERROR %d - REBOOT REQUIRED",session_status);
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,64,175,489,code,0xc032c8c8u,0);if(status)return status;
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,64,199,489,"REPORT THIS ERROR TO THE DEVELOPER",0xc032c8c8u,0);if(status)return status;
        return scene_menu_borders(a,motion.sink,motion.context);
    }
    if(flags&RF_SCENE_FRONTEND_LOADING){
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,64,151,489,"LOADING SAVED GAME",0xc032c8c8u,0);if(status)return status;
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,64,175,489,saves[selection],0xc032c8c8u,0);if(status)return status;
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,64,199,489,"VALIDATING SAVED WORLD",0xc032c8c8u,0);if(status)return status;
        return scene_menu_borders(a,motion.sink,motion.context);
    }
    /* Source43f9f0 list: panel33,58 + viewport31,57; cyan493x12 selection
     * strip starts two pixels left of the489px viewport. */
    for(i=0;i<2;i++){
        float y=115+i*12;
        if(i==selection){status=combat_hud_rect(sink,context,62,y,493,12,0xc032c8c8u);if(status)return status;}
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,64,y,225,saves[i],
            i==selection?0xff000000u:0xc032c8c8u,0);if(status)return status;
    }
    /* A on either row already invokes Load. Its original lit button is an
     * action cue for that same selection, not a new focus/action ID. */
    status=scene_menu_lit_button(a,sink,context,61,354,scene_text(933,"Load"),selection<2);if(status)return status;
    status=scene_menu_lit_button(a,sink,context,61,388,scene_text(788,"Back"),selection==2);if(status)return status;
    if(flags&RF_SCENE_FRONTEND_LOAD_NOTICE){
        const char *message=outcome==RF_SCENE_FRONTEND_LOAD_MISSING?
            (selection?"NO AUTOSAVE FOUND":"NO MANUAL SAVE FOUND"):
            outcome==RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE?"SAVE IS INCOMPATIBLE OR DAMAGED":"COULD NOT READ SAVE OR GAME DATA";
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,64,283,489,message,0xc032c8c8u,0);if(status)return status;
        snprintf(code,sizeof(code),"STATUS %d",load_status);
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,64,307,489,code,0xc032c8c8u,0);if(status)return status;
    }else{
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,64,307,489,"CHOOSE WHICH SAVE TO LOAD",0xc032c8c8u,0);if(status)return status;
    }
    return scene_menu_frontend_hint(a,motion.sink,motion.context,flags,"D-PAD UP/DOWN   A LOAD / SELECT","B OR START BACK");
}
static int scene_menu_exit_page(const rf_menu_assets *a,rf_scene_particle_sink sink,
    void *context,uint32_t flags,int session_status,uint32_t selection)
{
    char code[48];int status=scene_menu_background(a,sink,context);if(status)return status;
    status=scene_menu_main_rail(a,sink,context,UINT32_MAX);if(status)return status;
    status=scene_menu_popup_art(a,sink,context);if(status)return status;
    /* Source455c00/4561b0: medium title/body text boxes have a1px left inset;
     * two-button popup's top120,120 and bottom120,148 overlay the inactive rail. */
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,141,128,299,"EXIT TO DASHBOARD",0xff000000u,0);if(status)return status;
    if(session_status){
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,141,160,349,"DASHBOARD EXIT STOPPED",0xff000000u,0);if(status)return status;
        snprintf(code,sizeof(code),"ERROR %d - REBOOT REQUIRED",session_status);
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,141,180,349,code,0xff000000u,0);if(status)return status;
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,141,200,349,"REPORT THIS ERROR TO THE DEVELOPER",0xff000000u,0);if(status)return status;
        return scene_menu_borders(a,sink,context);
    }
    if(flags&RF_SCENE_FRONTEND_LOADING){
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,141,160,349,"RETURNING TO DASHBOARD",0xff000000u,0);if(status)return status;
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,141,180,349,"CHECKING RETIRED OWNERS",0xff000000u,0);if(status)return status;
        return scene_menu_borders(a,sink,context);
    }
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,141,160,349,"RETURN TO THE XBOX DASHBOARD?",0xff000000u,0);if(status)return status;
    status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,141,180,349,"UNSAVED SETTINGS WILL BE DISCARDED.",0xff000000u,0);if(status)return status;
    /* Original safe No(index0) is below Yes(index1), so Cancel0 remains the
     * safe default at282 and Exit1 is at244. Input selection/action unchanged. */
    status=scene_menu_popup_choice(a,sink,context,282,scene_text(202,"Cancel"),selection==0);if(status)return status;
    status=scene_menu_popup_choice(a,sink,context,244,"EXIT TO DASHBOARD",selection==1);if(status)return status;
    return scene_menu_frontend_hint(a,sink,context,flags,"D-PAD UP/DOWN   A SELECT","B OR START CANCEL");
}

int rf_scene_draw_authored_frontend(rf_scene_particle_sink sink,void *context,const rf_menu_assets *a,
    uint32_t flags,int session_status,uint32_t page,uint32_t selection,uint32_t difficulty,
    uint32_t load_outcome,int load_status)
{
    int status;uint32_t i;
    if(!a || !sink || particle_draw_stream || scene_actor_collision_owner || difficulty>3 ||
       page>RF_SCENE_FRONTEND_EXIT || page==RF_SCENE_FRONTEND_OPTIONS ||
       selection>(page==RF_SCENE_FRONTEND_DIFFICULTY?4u:
           page==RF_SCENE_FRONTEND_MAIN?3u:page==RF_SCENE_FRONTEND_EXIT?1u:2u) ||
       ((flags&RF_SCENE_FRONTEND_LOAD_NOTICE) && load_outcome>RF_SCENE_FRONTEND_LOAD_BLOCKED) ||
       ((flags&RF_SCENE_FRONTEND_LOADING) && page==RF_SCENE_FRONTEND_LOAD && selection>1) ||
       (flags&~(RF_SCENE_FRONTEND_DISCONNECTED|RF_SCENE_FRONTEND_LOADING|
          RF_SCENE_FRONTEND_ENDED|RF_SCENE_FRONTEND_LOAD_NOTICE)))return RF_RANGE;
    if(page==RF_SCENE_FRONTEND_LOAD)
        return scene_menu_load_page(a,sink,context,flags,session_status,selection,load_outcome,load_status);
    if(page==RF_SCENE_FRONTEND_EXIT)
        return scene_menu_exit_page(a,sink,context,flags,session_status,selection);
    status=scene_menu_background(a,sink,context);if(status)return status;
    if(page!=RF_SCENE_FRONTEND_MAIN){
        const char *title=scene_text(42,"NEW GAME");
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_LARGE,265+rf_menu_assets_title_offset(a),12,320,title,0xff000000u,0);if(status)return status;
        if(session_status || (flags&RF_SCENE_FRONTEND_LOADING)){
            status=scene_menu_borders(a,sink,context);if(status)return status;
        }
    }
    if(session_status){
        char message[48];
        status=scene_menu_centered(a,sink,context,196,
            "SESSION STOPPED",0xffffc060);if(status)return status;
        snprintf(message,sizeof(message),"ERROR %d - REBOOT REQUIRED",session_status);
        status=scene_menu_centered(a,sink,context,244,message,0xffeeeeee);if(status)return status;
        return scene_menu_centered(a,sink,context,328,"REPORT THIS ERROR TO THE DEVELOPER",0xffb8c0d0);
    }
    if(flags&RF_SCENE_FRONTEND_LOADING){
        status=scene_menu_centered(a,sink,context,196,"STARTING NEW GAME",0xfff0f0f0);if(status)return status;
        status=scene_menu_centered(a,sink,context,244,"LOADING LIVE MINES",0xffb8c0d0);if(status)return status;
        return scene_menu_centered(a,sink,context,292,scene_difficulty_name(difficulty),0xffeeeeee);
    }
    if(page==RF_SCENE_FRONTEND_DIFFICULTY){
        /* Xbox adaptation of the existing four choices plus Back, using the
         * authored button widgets. This is not a reconstructed dedicated PC
         * difficulty page; action IDs, difficulty values and routing stay put. */
        status=scene_menu_text(a,sink,context,RF_MENU_FONT_MEDIUM,235,112,385,
            scene_text(123,"NEW GAME - DIFFICULTY"),0xffeeeeeeu,0);if(status)return status;
        for(i=0;i<5;i++){
            status=scene_menu_button(a,sink,context,i,
                i<4?scene_difficulty_name(i):scene_text(788,"BACK"),i==selection,0,0);if(status)return status;
        }
        return scene_menu_frontend_hint(a,sink,context,flags,
            "D-PAD UP/DOWN   A START / SELECT","B OR START BACK");
    }
    status=scene_menu_main_rail(a,sink,context,selection);if(status)return status;
    status=scene_menu_borders(a,sink,context);if(status)return status;
    if(flags&RF_SCENE_FRONTEND_DISCONNECTED)
        return scene_menu_centered(a,sink,context,445,"CONNECT A CONTROLLER",0xffffc060u);
    return scene_menu_text(a,sink,context,RF_MENU_FONT_SMALL,24,445,592,
        "D-PAD UP/DOWN   A OR START SELECT",0xffc0c0c0u,1);
}
