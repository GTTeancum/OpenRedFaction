#include "ps2_frontend.h"
#include "rf/ps2_menu.h"
#include "rf/frontend_load.h"
#include "rf/effect.h"
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <windows.h>

/* PS2-style screens drawn from the user's PS2 menu pack (port-owned).
 * Reconstructed from SLUS-20073 where the code was traced; the rest is laid
 * out with the PS2 art and marked "port layout":
 *  - main menu: six template slots 1e5e60 (positions, d-pad neighbours,
 *    slide direction), slot text tables 28f5e0/28f630 (string, x, y offset),
 *    draw 1e57a0/1e61c0 (template on/off by selection, standard.vf text),
 *    0.3 s slide-in 1e6830 / slide-out 1e6a80, title centred with fade 1e3a70,
 *    animated backdrop (150 frames, cross-faded) 1e2400;
 *  - options frame 1e25d0: blacktop (0,52), blackcorner (598,333), black body
 *    (0,98,626,235)+(0,333,598,24), red edge lines, stripstop (0,48),
 *    stripscorner (598,333), titlebar pieces (270,60)/(568,60);
 *  - colours 2a7b00/2a7c24 (rf/ps2_menu.h).
 * Port layout (not traced): popups, rows inside the options body, the load
 * list, the multiplayer setup screen and the pause items in the two empty
 * PS2 slots. The bottom-row template mirroring is inferred from the art.
 * Coordinates are the PS2's 640x448 screen, scaled to the 640x480 HUD space. */
#define Y_SCALE (480.0f/448.0f)
#define NORMAL_MODE (RF_PARTICLE_NORMAL_MODE&~(31u<<20))
#define SOLID_MODE 0x18000u
#define SLIDE_MS 300u
#define CENTER 0x8000
#define RESTART_LABEL 0xffffu /* port label; strings.tbl has none */

static rf_ps2_menu *ps2;
static uint32_t ps2_refs,ps2_frontend,ps2_checked,ps2_present;
static uint32_t slide_start,slide_out,last_draw;

/* 1e5e60: x, y, mirror (port: bit0 x, bit1 y), slide direction (+2c),
 * neighbours left/right/up/down (+1c..+28). */
static const struct {int16_t x,y;uint8_t mirror;int8_t dir;uint8_t next[4];} slots[RF_XBOX_PS2_SLOTS]={
    {0,0,0,1,{5,1,3,3}},{161,0,0,0,{0,2,4,4}},{417,0,1,-1,{1,3,5,5}},
    {0,362,2,-1,{2,4,0,0}},{160,362,2,0,{3,5,1,1}},{417,362,3,1,{4,0,2,2}}};
/* 28f5e0 front end: string id, text x, text y. */
static const struct {uint16_t id;uint8_t x,y;} main_text[RF_XBOX_PS2_SLOTS]={
    {42,20,25},{47,81,25},{21,58,25},{48,29,46},{43,116,46},{44,97,46}};
/* 28f630 in game: SAVE GAME, -, LOAD GAME, OPTIONS, -, QUIT. The empty
 * slots carry the port's MESSAGES and restart (centred, port layout). */
static const struct {uint16_t id;uint8_t x,y;uint8_t item;} pause_text[RF_XBOX_PS2_SLOTS]={
    {22,20,25,RF_SCENE_SESSION_SAVE},{210,0,25,RF_SCENE_SESSION_HISTORY},{21,63,25,RF_SCENE_SESSION_LOAD},
    {43,29,46,RF_SCENE_SESSION_OPTIONS},{RESTART_LABEL,0,46,RF_SCENE_SESSION_RESTART},{46,97,46,RF_SCENE_SESSION_EXIT}};

uint32_t rf_xbox_ps2_slot_neighbor(uint32_t slot,uint32_t direction)
{return slot<RF_XBOX_PS2_SLOTS && direction<4?slots[slot].next[direction]:0;}
uint32_t rf_xbox_ps2_pause_item(uint32_t slot)
{return slot<RF_XBOX_PS2_SLOTS?pause_text[slot].item:RF_SCENE_SESSION_RESUME;}
uint32_t rf_xbox_ps2_pause_slot(uint32_t item)
{
    uint32_t i;
    for(i=0;i<RF_XBOX_PS2_SLOTS;i++)if(pause_text[i].item==item)return i;
    return 0;
}

/* ---- multiplayer lists ---- */
static const struct {const char *name,*picture;} characters[13]={
    {"Miner Parker","multichar01.tga"},{"Envirosuit Parker","multichar03.tga"},
    {"Scientist Parker","multichar12.tga"},{"Exec Parker","multichar13.tga"},
    {"Miner","multichar02.tga"},{"Envirosuit Miner","multichar04.tga"},
    {"Ultor Guard 1","multichar05.tga"},{"Ultor Guard 2","multichar06.tga"},
    {"Envirosuit Guard 1","multichar08.tga"},{"Envirosuit Guard 2","multichar09.tga"},
    {"Elite Guard 1","multichar07.tga"},{"Elite Guard 2","multichar10.tga"},
    {"Riot Guard 1","multichar11.tga"}};
/* The PS2 arenas (ps2_multi.tbl) that are PC multiplayer maps with authored
 * bot data (docs/MP-BOTS-PS2.md): PS2 screen name, picture and the PC map the
 * Xbox loads (the menu pack carries each arena's bot paths and bots). */
static const struct {const char *name,*picture,*level;} environments[]={
    {"The Lobby","multienv_m_ps201.tga","dm01.rfl"},
    {"Installation XJ5","multienv01.tga","dm02.rfl"},
    {"Waste Disposal Plant","multienv02.tga","dm03.rfl"},
    {"The Badlands","multienv03.tga","dm04.rfl"},
    {"Hangar 18","multienv04.tga","dm05.rfl"},
    {"Waterlogged","multienv_m_ps207.tga","dm06.rfl"},
    {"Mine Warfare","multienv07.tga","dm07.rfl"},
    {"Warlords","multienv_m_ps210.tga","ctf02.rfl"}};
#define ENVIRONMENT_COUNT (sizeof(environments)/sizeof(environments[0]))
static const uint16_t bot_ids[7]={78,79,80,81,82,83,84};
static const uint16_t skill_ids[4]={85,86,87,88};
static const uint16_t kill_ids[8]={89,395,90,396,91,397,399,92};
static const uint16_t time_ids[8]={89,94,401,95,402,96,97,406};
uint32_t rf_xbox_ps2_multi_count(uint32_t row)
{
    static const uint32_t counts[RF_XBOX_PS2_MP_ROWS]={13,ENVIRONMENT_COUNT,7,4,8,8,1};
    return row<RF_XBOX_PS2_MP_ROWS?counts[row]:0;
}
const char *rf_xbox_ps2_multi_level(uint32_t environment)
{return environments[environment%ENVIRONMENT_COUNT].level;}
void rf_xbox_ps2_multi_rules(const rf_xbox_ps2_multi *m,uint32_t *bots,uint32_t *kills,uint32_t *seconds)
{
    /* Same order as the value strings above (<no limit> first). */
    static const uint16_t kill_values[8]={0,10,15,20,25,30,40,50};
    static const uint16_t minutes[8]={0,5,10,15,20,25,30,50};
    *bots=m->values[RF_XBOX_PS2_MP_BOTS]%7;
    *kills=kill_values[m->values[RF_XBOX_PS2_MP_KILLS]%8];
    *seconds=minutes[m->values[RF_XBOX_PS2_MP_TIME]%8]*60u;
}

/* ---- owner ---- */
int rf_xbox_ps2_available(void)
{
    if(!ps2_checked){
        DWORD a=GetFileAttributesA("D:\\ps2menu.vpp");
        ps2_present=a!=INVALID_FILE_ATTRIBUTES && !(a&FILE_ATTRIBUTE_DIRECTORY);ps2_checked=1;
    }
    return (int)ps2_present;
}
int rf_xbox_ps2_open(uint32_t frontend)
{
    int status;
    if(ps2){++ps2_refs;return RF_OK;}
    status=rf_ps2_menu_open(&ps2,"D:\\ps2menu.vpp",
        frontend?RF_PS2_MENU_FRONTEND|RF_PS2_MENU_BACKDROP:0,frontend?14u*1024u*1024u:3u*1024u*1024u);
    if(status)return status;
    ps2_refs=1;ps2_frontend=frontend;slide_start=GetTickCount();slide_out=0;return RF_OK;
}
int rf_xbox_ps2_close(void)
{
    if(!ps2)return RF_OK;
    if(--ps2_refs)return RF_OK;
    rf_ps2_menu_close(ps2);ps2=NULL;return RF_OK;
}
int rf_xbox_ps2_active(void){return ps2!=NULL;}
uint32_t rf_xbox_ps2_resident_bytes(void){return rf_ps2_menu_resident_bytes(ps2);}
void rf_xbox_ps2_main_enter(void){slide_start=GetTickCount();slide_out=0;}
void rf_xbox_ps2_begin_departure(void){slide_start=GetTickCount();slide_out=1;}
int rf_xbox_ps2_departure_done(void){return !slide_out || GetTickCount()-slide_start>=SLIDE_MS;}
int rf_xbox_ps2_redraw_due(void)
{
    /* The backdrop animates at 30 pictures a second; in game, redraw only
     * while the buttons slide (plus one picture to land them). */
    if(!ps2)return 0;
    if(!ps2_frontend && last_draw-slide_start>SLIDE_MS+33u)return 0;
    return GetTickCount()-last_draw>=33;
}

/* ---- drawing primitives ---- */
static int quad(rf_scene_particle_sink sink,void *context,const rf_hud_sprite *s,
    float x,float y,float w,float h,uint32_t argb,uint32_t mirror)
{
    rf_particle_draw_vertex v[4];uint32_t i;
    memset(v,0,sizeof(v));
    for(i=0;i<4;i++){
        uint32_t right=i==1||i==2,bottom=i>=2,u=right^(mirror&1),t=bottom^((mirror>>1)&1);
        v[i].screen[0]=x+(right?w:0);v[i].screen[1]=(y+(bottom?h:0))*Y_SCALE;
        v[i].reciprocal_w=1;v[i].argb=argb;
        if(s){v[i].uv[0]=s->uv[u?2:0];v[i].uv[1]=s->uv[t?3:1];}
    }
    return sink(context,v,4,s?s->image:NULL,s?NORMAL_MODE:SOLID_MODE);
}
static int picture(rf_scene_particle_sink sink,void *context,uint32_t id,float x,float y,uint32_t argb,uint32_t mirror)
{
    rf_hud_sprite s;int status=rf_ps2_menu_picture(ps2,id,&s);
    if(status==RF_NOT_FOUND)return RF_OK;if(status)return status;
    return quad(sink,context,&s,x,y,(float)s.width,(float)s.height,argb,mirror);
}
static int rect(rf_scene_particle_sink sink,void *context,float x,float y,float w,float h,uint32_t argb)
{return quad(sink,context,NULL,x,y,w,h,argb,0);}
static uint32_t width_of(uint32_t font,const char *text)
{uint32_t w=0;if(rf_ps2_menu_text_width(ps2,font,text,&w))w=0;return w;}
/* 211280: x CENTER centres on the 640-wide screen; '\n' starts a new line. */
static int text(rf_scene_particle_sink sink,void *context,uint32_t font,int32_t x,float y,const char *string,uint32_t argb)
{
    float cx,height=(float)rf_ps2_menu_font_height(ps2,font);
    const char *line=string;int status;
    while(line){
        const char *end=strchr(line,'\n');char buffer[160];size_t n=end?(size_t)(end-line):strlen(line);
        if(n>=sizeof(buffer))n=sizeof(buffer)-1;memcpy(buffer,line,n);buffer[n]=0;
        cx=x==CENTER?(640.0f-(float)width_of(font,buffer))*.5f:(float)x;
        for(size_t i=0;buffer[i];i++){
            rf_hud_glyph g;
            status=rf_ps2_menu_glyph(ps2,font,(unsigned char)buffer[i],&g);if(status)return status;
            if(g.sprite.width){status=quad(sink,context,&g.sprite,cx,y,(float)g.sprite.width,(float)g.sprite.height,argb,0);if(status)return status;}
            cx+=(float)g.advance;
        }
        y+=height;line=end?end+1:NULL;
    }
    return RF_OK;
}
static const char *str(uint32_t id){return rf_ps2_menu_string(ps2,id);}

/* 1e2400: two neighbouring backdrop frames cross-faded at 30 per second. */
static int backdrop(rf_scene_particle_sink sink,void *context,uint32_t dim)
{
    uint32_t count=rf_ps2_menu_backdrop_count(ps2),now=GetTickCount(),frame,alpha;int status;
    rf_hud_sprite s={0};
    if(!count)return rect(sink,context,0,0,640,448,0xff000000u);
    frame=now/33u;alpha=(now%33u)*255u/33u;
    s.uv[2]=s.uv[3]=1;s.width=640;s.height=448;
    s.image=rf_ps2_menu_backdrop(ps2,frame);
    status=quad(sink,context,&s,0,0,640,448,0xff000000u|dim,0);if(status)return status;
    s.image=rf_ps2_menu_backdrop(ps2,frame+1);
    return quad(sink,context,&s,0,0,640,448,(alpha<<24)|dim,0);
}
/* Main-menu style frame of six slots; text and items per table. */
static int slot_frame(rf_scene_particle_sink sink,void *context,uint32_t selection,
    const uint16_t *ids,const uint8_t (*offsets)[2],const uint32_t *enabled)
{
    uint32_t now=GetTickCount(),i,elapsed=now-slide_start;
    float t=elapsed>=SLIDE_MS?1.0f:(float)elapsed/SLIDE_MS;int status;
    /* 1e6830 eases the slide-in (1f4bb0); 1e6a80 slides out linearly. */
    float settle=slide_out?1.0f-t:sinf(t*1.5707964f);
    for(i=0;i<RF_XBOX_PS2_SLOTS;i++){
        uint32_t middle=i==1||i==4,on=i==selection,top=i<3;
        uint32_t id=middle?(on?RF_PS2_MIDDLE_ON:RF_PS2_MIDDLE_OFF):(on?RF_PS2_CORNER_ON:RF_PS2_CORNER_OFF);
        rf_hud_sprite s;float x,y,off;
        if(!ids[i])continue;
        status=rf_ps2_menu_picture(ps2,id,&s);if(status)return status;
        off=(float)s.height-(float)s.height*settle;
        if(slide_out && i==selection)off=0;
        x=slots[i].x+(top?-off:off)*slots[i].dir;y=slots[i].y+(top?-off:off);
        status=quad(sink,context,&s,x,y,(float)s.width,(float)s.height,0xffffffffu,slots[i].mirror);if(status)return status;
        {
            const char *label=ids[i]==RESTART_LABEL?"RESTART":str(ids[i]);int32_t tx=offsets[i][0];
            if(!tx)tx=(int32_t)((s.width-width_of(RF_PS2_FONT_STANDARD,label))/2);
            status=text(sink,context,RF_PS2_FONT_STANDARD,(int32_t)x+tx,y+offsets[i][1],label,
                enabled && !enabled[i]?RF_PS2_COLOR_DISABLED:on?RF_PS2_COLOR_SELECTED:RF_PS2_COLOR_TEXT);
            if(status)return status;
        }
    }
    return RF_OK;
}
/* Port layout: a popup over the popup_strips frame, centred. */
static int popup(rf_scene_particle_sink sink,void *context,const char *title,const char *const *items,
    uint32_t count,uint32_t selection,const uint32_t *enabled,const char *note)
{
    rf_hud_sprite s;float x,y;uint32_t i;int status;
    status=rf_ps2_menu_picture(ps2,RF_PS2_POPUP,&s);if(status)return status;
    /* PS2 1ecxxx/1ed110: popup_strips drawn once, unmirrored, at (149,129)
     * (its single title tab is authentic); title in text colour 2ac5c8,
     * centred on x 244 at y 140 (the tab); item rows 18 apart, centred on
     * y 201 (0xc9 - count*9). */
    x=149;y=129;(void)s;
    status=quad(sink,context,&s,x,y,(float)s.width,(float)s.height,0xffffffffu,0);if(status)return status;
    status=text(sink,context,RF_PS2_FONT_STANDARD,(int32_t)(244-width_of(RF_PS2_FONT_STANDARD,title)/2),
        140,title,RF_PS2_COLOR_TEXT);if(status)return status;
    for(i=0;i<count;i++){
        float row=201.0f-(float)count*9.0f;
        if(row<167)row=167; /* keep clear of the tab's lower edge */
        status=text(sink,context,RF_PS2_FONT_STANDARD,CENTER,row+i*18,items[i],
            enabled && !enabled[i]?RF_PS2_COLOR_DISABLED:i==selection?RF_PS2_COLOR_SELECTED:RF_PS2_COLOR_TEXT);
        if(status)return status;
    }
    if(note){status=text(sink,context,RF_PS2_FONT_STANDARD,CENTER,y+s.height+8,note,RF_PS2_COLOR_TEXT);if(status)return status;}
    return RF_OK;
}
/* 1e25d0 options frame with the page title. */
static int options_frame(rf_scene_particle_sink sink,void *context,const char *title)
{
    static const uint32_t edge[5]={0x6e3c3c,0x5a2222,0x410000,0x350000,0x1e0000};
    static const uint32_t floor_lines[6]={0x453939,0x8c6464,0x551b1b,0x410000,0x2f0000,0x100000};
    uint32_t i;int status;
    status=picture(sink,context,RF_PS2_OPTIONS_BLACKTOP,0,52,0xc8ffffffu,0);if(status)return status;
    status=picture(sink,context,RF_PS2_OPTIONS_BLACKCORNER,598,333,0xc8ffffffu,0);if(status)return status;
    status=rect(sink,context,0,98,626,235,0xc8000000u);if(status)return status;
    status=rect(sink,context,0,333,598,24,0xc8000000u);if(status)return status;
    for(i=0;i<5;i++){status=rect(sink,context,624.0f+i+(i>=3),99,i==2?2.0f:1.0f,234,0xff000000u|edge[i]);if(status)return status;}
    for(i=0;i<6;i++){status=rect(sink,context,0,355.0f+i,598,1,0xff000000u|floor_lines[i]);if(status)return status;}
    status=picture(sink,context,RF_PS2_OPTIONS_STRIPSTOP,0,48,0xffffffffu,0);if(status)return status;
    status=picture(sink,context,RF_PS2_OPTIONS_STRIPSCORNER,598,333,0xffffffffu,0);if(status)return status;
    status=picture(sink,context,RF_PS2_OPTIONS_TITLEBAR1,270,60,0xffffffffu,0);if(status)return status;
    status=picture(sink,context,RF_PS2_OPTIONS_TITLEBAR2,568,60,0xffffffffu,0);if(status)return status;
    /* Every options page (1e9xxx..1f2xxx) draws its title in title.vf at
     * (287,58) in colour 2ac5d0 (230,230,230). */
    return text(sink,context,RF_PS2_FONT_TITLE,287,58,title,0xffe6e6e6u);
}
/* Port layout rows inside the options body (0,98)-(626,333). */
static int option_row(rf_scene_particle_sink sink,void *context,uint32_t row,const char *label,
    const char *value,uint32_t selected)
{
    float y=112.0f+row*26.0f;int status;
    uint32_t color=selected?RF_PS2_COLOR_SELECTED:RF_PS2_COLOR_TEXT;
    if(selected){status=rect(sink,context,40,y-3,546,22,0x60eaea53u&0x60ffffffu);if(status)return status;}
    status=text(sink,context,RF_PS2_FONT_STANDARD,60,y,label,color);if(status)return status;
    if(value){status=text(sink,context,RF_PS2_FONT_STANDARD,(int32_t)(560-width_of(RF_PS2_FONT_STANDARD,value)),y,value,color);if(status)return status;}
    return RF_OK;
}
static int slider_row(rf_scene_particle_sink sink,void *context,uint32_t row,const char *label,
    uint32_t value,uint32_t lo,uint32_t hi,uint32_t selected)
{
    float y=112.0f+row*26.0f,x;int status;char number[16];
    status=option_row(sink,context,row,label,NULL,selected);if(status)return status;
    status=picture(sink,context,RF_PS2_SLIDER_RAIL,380,y+7,0xffffffffu,0);if(status)return status;
    x=380.0f+110.0f*(float)(value-lo)/(float)(hi-lo)-8.0f;
    status=picture(sink,context,RF_PS2_SLIDER_CONTROL,x,y,0xffffffffu,0);if(status)return status;
    snprintf(number,sizeof(number),"%u%%",value);
    return text(sink,context,RF_PS2_FONT_STANDARD,(int32_t)(560-width_of(RF_PS2_FONT_STANDARD,number)),y,number,
        selected?RF_PS2_COLOR_SELECTED:RF_PS2_COLOR_TEXT);
}
static int footer(rf_scene_particle_sink sink,void *context,const char *hint)
{return text(sink,context,RF_PS2_FONT_STANDARD,CENTER,380,hint,RF_PS2_COLOR_TEXT);}
/* 1e3a70 prints its two prompt lines centred at y 320 and 338. */
static int disconnected(rf_scene_particle_sink sink,void *context)
{
    int status=text(sink,context,RF_PS2_FONT_STANDARD,CENTER,320,str(204),RF_PS2_COLOR_HIGHLIGHT);
    if(status)return status;
    return text(sink,context,RF_PS2_FONT_STANDARD,CENTER,338,"Connect a controller.",RF_PS2_COLOR_TEXT);
}
static void drawn(void){last_draw=GetTickCount();}

/* ---- screens ---- */
int rf_xbox_ps2_draw_frontend(rf_scene_particle_sink sink,void *context,uint32_t flags,int session_status,
    uint32_t page,uint32_t selection,uint32_t difficulty,uint32_t load_outcome,int load_status)
{
    static const uint16_t ids[RF_XBOX_PS2_SLOTS]={42,47,21,48,43,44};
    static const uint32_t enabled[RF_XBOX_PS2_SLOTS]={1,1,1,0,1,1};
    uint8_t offsets[RF_XBOX_PS2_SLOTS][2];uint32_t i,title_alpha;int status;
    (void)load_status;
    if(!ps2 || !sink)return RF_RANGE;
    drawn();
    status=backdrop(sink,context,0xffffffu);if(status)return status;
    if(session_status){
        char message[64];snprintf(message,sizeof(message),"Error %d - reboot required.",session_status);
        status=text(sink,context,RF_PS2_FONT_STANDARD,CENTER,200,str(6),RF_PS2_COLOR_HIGHLIGHT);if(status)return status;
        return text(sink,context,RF_PS2_FONT_STANDARD,CENTER,222,message,RF_PS2_COLOR_TEXT);
    }
    for(i=0;i<RF_XBOX_PS2_SLOTS;i++){offsets[i][0]=main_text[i].x;offsets[i][1]=main_text[i].y;}
    /* 1e3a70: the title fades in over the backdrop, centred. */
    title_alpha=GetTickCount()-slide_start>=1000u?255u:(GetTickCount()-slide_start)*255u/1000u;
    status=picture(sink,context,RF_PS2_TITLE,16,200,(title_alpha<<24)|0xffffffu,0);if(status)return status;
    status=slot_frame(sink,context,page==RF_SCENE_FRONTEND_MAIN?selection:
        page==RF_SCENE_FRONTEND_DIFFICULTY?0u:page==RF_SCENE_FRONTEND_LOAD?2u:
        page==RF_XBOX_PS2_PAGE_MULTI?1u:5u,ids,(const uint8_t (*)[2])offsets,enabled);
    if(status)return status;
    if(flags&RF_SCENE_FRONTEND_DISCONNECTED)return disconnected(sink,context);
    if(flags&RF_SCENE_FRONTEND_LOADING){
        const char *what=page==RF_SCENE_FRONTEND_EXIT?"Returning to the Xbox Dashboard":str(1);
        return popup(sink,context,what,NULL,0,0,NULL,NULL);
    }
    if(page==RF_SCENE_FRONTEND_DIFFICULTY){
        const char *items[5]={str(85),str(86),str(87),str(88),str(202)};
        (void)difficulty;
        return popup(sink,context,str(123),items,5,selection,NULL,NULL);
    }
    if(page==RF_SCENE_FRONTEND_LOAD){
        const char *items[3]={"Quick Save","Autosave",str(202)};
        const char *note=NULL;
        if(flags&RF_SCENE_FRONTEND_LOAD_NOTICE)
            note=load_outcome==RF_SCENE_FRONTEND_LOAD_MISSING?str(391):
                load_outcome==RF_SCENE_FRONTEND_LOAD_INCOMPATIBLE?"This save is from another version.":str(55);
        return popup(sink,context,str(21),items,3,selection,NULL,note);
    }
    if(page==RF_XBOX_PS2_PAGE_EXTRAS){
        const char *items[3]={str(49),str(45),str(46)};static const uint32_t extras[3]={0,0,1};
        return popup(sink,context,str(44),items,3,selection,extras,NULL);
    }
    if(page==RF_SCENE_FRONTEND_EXIT){
        const char *items[2]={str(77),str(76)};
        return popup(sink,context,str(50),items,2,selection,NULL,str(51));
    }
    return RF_OK;
}
int rf_xbox_ps2_draw_multi(rf_scene_particle_sink sink,void *context,const rf_xbox_ps2_multi *m)
{
    static const uint16_t ids[RF_XBOX_PS2_SLOTS]={42,47,21,48,43,44};
    static const uint32_t enabled[RF_XBOX_PS2_SLOTS]={1,1,1,0,1,1};
    uint8_t offsets[RF_XBOX_PS2_SLOTS][2];uint32_t i;int status;
    if(!ps2 || !sink || !m)return RF_RANGE;
    drawn();
    status=backdrop(sink,context,0x606060u);if(status)return status;
    if(m->step==RF_XBOX_PS2_MP_MODE){
        const char *items[2]={str(53),str(54)};static const uint32_t modes[2]={0,1};
        for(i=0;i<RF_XBOX_PS2_SLOTS;i++){offsets[i][0]=main_text[i].x;offsets[i][1]=main_text[i].y;}
        status=slot_frame(sink,context,1,ids,(const uint8_t (*)[2])offsets,enabled);if(status)return status;
        return popup(sink,context,str(52),items,2,m->selection,modes,NULL);
    }
    status=options_frame(sink,context,str(105));if(status)return status;
    {
        rf_hud_sprite portrait;char value[48];
        /* The arena's picture while Environment is selected, else the portrait. */
        const char *shown=m->selection==RF_XBOX_PS2_MP_ENVIRONMENT?
            environments[m->values[RF_XBOX_PS2_MP_ENVIRONMENT]%ENVIRONMENT_COUNT].picture:
            characters[m->values[RF_XBOX_PS2_MP_CHARACTER]%13].picture;
        float w,h;
        status=rf_ps2_menu_feature(ps2,shown,&portrait);if(status)return status;
        w=(float)portrait.width;h=(float)portrait.height;
        if(w>243.0f){h*=243.0f/w;w=243.0f;}
        status=quad(sink,context,&portrait,372,110,w,h,0xffffffffu,0);if(status)return status;
        for(i=0;i<RF_XBOX_PS2_MP_ROWS;i++){
            const char *label,*v=value;uint32_t x=m->values[i];
            switch(i){
            case RF_XBOX_PS2_MP_CHARACTER:label=str(62);v=characters[x%13].name;break;
            case RF_XBOX_PS2_MP_ENVIRONMENT:label=str(63);v=environments[x%ENVIRONMENT_COUNT].name;break;
            case RF_XBOX_PS2_MP_BOTS:label=str(98);v=str(bot_ids[x%7]);break;
            case RF_XBOX_PS2_MP_SKILL:label=str(99);v=str(skill_ids[x%4]);break;
            case RF_XBOX_PS2_MP_KILLS:label=str(100);v=str(kill_ids[x%8]);break;
            case RF_XBOX_PS2_MP_TIME:label=str(101);v=str(time_ids[x%8]);break;
            default:label=str(103);v=NULL;break;
            }
            /* Rows sit left of the portrait (port layout). */
            {
                float y=112.0f+i*30.0f;uint32_t sel=i==m->selection;
                uint32_t color=sel?RF_PS2_COLOR_SELECTED:RF_PS2_COLOR_TEXT;
                if(sel){status=rect(sink,context,24,y-3,330,22,0x60eaea53u&0x60ffffffu);if(status)return status;}
                status=text(sink,context,RF_PS2_FONT_STANDARD,34,y,label,color);if(status)return status;
                if(v){status=text(sink,context,RF_PS2_FONT_STANDARD,(int32_t)(344-width_of(RF_PS2_FONT_STANDARD,v)),y,v,color);if(status)return status;}
            }
        }
    }
    if(m->loading)return popup(sink,context,str(1),NULL,0,0,NULL,NULL);
    return footer(sink,context,"Bots are not active yet: the map loads with you alone.");
}
int rf_xbox_ps2_draw_session(rf_scene_particle_sink sink,void *context,uint32_t selection,uint32_t flags)
{
    uint16_t ids[RF_XBOX_PS2_SLOTS];uint8_t offsets[RF_XBOX_PS2_SLOTS][2];uint32_t i;int status;
    if(!ps2 || !sink || selection>=RF_SCENE_SESSION_COUNT)return RF_RANGE;
    drawn();
    if(flags&RF_SCENE_SESSION_ENDED)
        return text(sink,context,RF_PS2_FONT_STANDARD,CENTER,200,str(11),RF_PS2_COLOR_TEXT);
    for(i=0;i<RF_XBOX_PS2_SLOTS;i++){
        /* Multiplayer pause is OPTIONS and QUIT only (PS2 table 28f680). */
        ids[i]=rf_scene_mp[0] && i!=3 && i!=5?0:pause_text[i].id;offsets[i][0]=pause_text[i].x;offsets[i][1]=pause_text[i].y;
    }
    status=picture(sink,context,RF_PS2_PAUSED,192,208,0xffffffffu,0);if(status)return status;
    {
        /* Slot 4's label is the port's restart; strings.tbl has none. */
        static const uint32_t all[RF_XBOX_PS2_SLOTS]={1,1,1,1,1,1};
        status=slot_frame(sink,context,rf_xbox_ps2_pause_slot(selection),ids,(const uint8_t (*)[2])offsets,all);
        if(status)return status;
    }
    if(flags&RF_SCENE_SESSION_DISCONNECTED)return disconnected(sink,context);
    if(flags&RF_SCENE_SESSION_CONFIRM){
        const char *items[2]={str(77),str(76)};
        const char *question=selection==RF_SCENE_SESSION_LOAD?str(19):
            selection==RF_SCENE_SESSION_RESTART?"Restart this section?":str(51);
        return popup(sink,context,str(18),items,2,1,NULL,question);
    }
    if(flags&RF_SCENE_SESSION_UNAVAILABLE)return footer(sink,context,"Restart is unavailable here.");
    if(flags&RF_SCENE_SESSION_OPTIONS_UNAVAILABLE)return footer(sink,context,"Options could not be loaded.");
    return RF_OK;
}
int rf_xbox_ps2_draw_options(rf_scene_particle_sink sink,void *context,uint32_t page,uint32_t selection,
    uint32_t flags,uint32_t sensitivity,uint32_t invert,uint32_t deadzone)
{
    char value[32];int status;
    if(!ps2 || !sink)return RF_RANGE;
    drawn();
    if(ps2_frontend){status=backdrop(sink,context,0x606060u);if(status)return status;}
    if(page==RF_SCENE_OPTIONS_PAGE_ROOT){
        static const uint16_t ids[RF_SCENE_OPTIONS_CATEGORY_COUNT]={177,128,129,124,140};
        uint32_t i;
        status=options_frame(sink,context,str(43));if(status)return status;
        for(i=0;i<RF_SCENE_OPTIONS_CATEGORY_COUNT;i++){
            const char *label=str(ids[i]);
            status=option_row(sink,context,i,label,i==RF_SCENE_OPTIONS_CATEGORY_VIDEO?"Fixed":NULL,i==selection);
            if(status)return status;
        }
    }else if(page==RF_SCENE_OPTIONS_PAGE_CONTROLS){
        static const uint32_t rows[6]={RF_SCENE_OPTIONS_SENSITIVITY,RF_SCENE_OPTIONS_INVERT,RF_SCENE_OPTIONS_DEADZONE,
            RF_SCENE_OPTIONS_STICK_LAYOUT,RF_SCENE_OPTIONS_BUTTONS,RF_SCENE_OPTIONS_BACK};
        status=options_frame(sink,context,str(124));if(status)return status;
        status=slider_row(sink,context,0,str(143),sensitivity,25,200,selection==rows[0]);if(status)return status;
        status=option_row(sink,context,1,str(135),invert?str(169):str(170),selection==rows[1]);if(status)return status;
        snprintf(value,sizeof(value),"%u%%",deadzone);
        status=option_row(sink,context,2,"Stick Deadzone:",value,selection==rows[2]);if(status)return status;
        status=option_row(sink,context,3,str(134),(flags&RF_SCENE_OPTIONS_SOUTHPAW)?"Southpaw":"Standard",selection==rows[3]);if(status)return status;
        status=option_row(sink,context,4,str(125),NULL,selection==rows[4]);if(status)return status;
        status=option_row(sink,context,5,str(140),NULL,selection==rows[5]);if(status)return status;
    }else{
        status=options_frame(sink,context,str(177));if(status)return status;
        status=option_row(sink,context,0,"Text Language:",rf_scene_text_language_name_public(),selection==RF_SCENE_OPTIONS_LANGUAGE);if(status)return status;
        status=option_row(sink,context,1,str(141),NULL,selection==RF_SCENE_OPTIONS_DEFAULTS);if(status)return status;
        status=option_row(sink,context,2,"Save Settings",NULL,selection==RF_SCENE_OPTIONS_SAVE);if(status)return status;
        status=option_row(sink,context,3,str(140),NULL,selection==RF_SCENE_OPTIONS_BACK);if(status)return status;
    }
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED)return disconnected(sink,context);
    if(flags&RF_SCENE_OPTIONS_AUDIO_ERROR)return footer(sink,context,"Audio settings could not be applied.");
    return RF_OK;
}
int rf_xbox_ps2_draw_audio(rf_scene_particle_sink sink,void *context,uint32_t selection,uint32_t flags,
    uint32_t master,uint32_t effects,uint32_t music)
{
    int status;
    if(!ps2 || !sink)return RF_RANGE;
    drawn();
    if(ps2_frontend){status=backdrop(sink,context,0x606060u);if(status)return status;}
    status=options_frame(sink,context,str(129));if(status)return status;
    status=slider_row(sink,context,0,"Master Volume:",master,0,100,selection==RF_SCENE_AUDIO_MASTER);if(status)return status;
    status=slider_row(sink,context,1,str(179),effects,0,100,selection==RF_SCENE_AUDIO_EFFECTS);if(status)return status;
    status=slider_row(sink,context,2,str(180),music,0,100,selection==RF_SCENE_AUDIO_MUSIC);if(status)return status;
    status=option_row(sink,context,3,str(141),NULL,selection==RF_SCENE_AUDIO_DEFAULTS);if(status)return status;
    status=option_row(sink,context,4,"Save Settings",NULL,selection==RF_SCENE_AUDIO_SAVE);if(status)return status;
    status=option_row(sink,context,5,str(140),NULL,selection==RF_SCENE_AUDIO_BACK);if(status)return status;
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED)return disconnected(sink,context);
    if(flags&RF_SCENE_OPTIONS_AUDIO_ERROR)return footer(sink,context,"Audio settings could not be applied.");
    return RF_OK;
}
int rf_xbox_ps2_draw_buttons(rf_scene_particle_sink sink,void *context,uint32_t selection,uint32_t flags,
    const char *const *actions,const char *const *controls)
{
    static const uint16_t action_ids[RF_SCENE_BUTTON_ACTION_COUNT]={145,146,148,147,151,149,152,153,154,150};
    uint32_t first=selection>=8?RF_SCENE_BUTTON_COUNT-8:0,i;int status;
    if(!ps2 || !sink || !actions || !controls)return RF_RANGE;
    drawn();
    if(ps2_frontend){status=backdrop(sink,context,0x606060u);if(status)return status;}
    status=options_frame(sink,context,str(142));if(status)return status;
    for(i=first;i<first+8 && i<RF_SCENE_BUTTON_COUNT;i++){
        const char *label=i<RF_SCENE_BUTTON_ACTION_COUNT?str(action_ids[i]):
            i==RF_SCENE_BUTTON_DEFAULTS?str(141):str(140);
        status=option_row(sink,context,i-first,label,i<RF_SCENE_BUTTON_ACTION_COUNT?controls[i]:NULL,i==selection);
        if(status)return status;
    }
    if(flags&RF_SCENE_OPTIONS_DISCONNECTED)return disconnected(sink,context);
    return RF_OK;
}

/* MP STATS (2399a0, one human): options frame titled "MP STATS" (66);
 * headers in message.vf, colour 2ac5c8, at y 147: "Player" (67) x 44,
 * "Kills" (68) x 268, "Deaths" (69) x 330, "Longest" / "Killing Spree"
 * (415/416) at (418,130) / (403,147), "Accuracy" (71) x 522. Rows from
 * y 174, 18 apart, standard font: the first in 2ac5cc, the rest 2ac5c8;
 * name x 44, kills centred 281, deaths 354, longest 446, accuracy
 * "%5.1f %%" centred 558; with more than two rows every other row sits on
 * a black bar (34, y-1, 581x17) alpha 32. White: "Elapsed time: %s:%s:%s"
 * (419; hours, minutes, seconds, each "0%1d" under 10 else "%2d") centred
 * at y 320, and the choices 72/73/74 at (135,372), (355,372), (190,397)
 * with the PS2 cross/circle/square glyphs (0xad/0xb0/0xae), here the Xbox
 * A/B/X buttons. Port notes: no fade-in (1e22e0); message.vf kerning is
 * not applied. */
static void mp_stats_field(char *out,size_t size,uint32_t v){snprintf(out,size,v<10?"0%1u":"%2u",v);}
int rf_xbox_ps2_draw_mp_stats(rf_scene_particle_sink sink,void *context,const rf_scene_mp_stats_row *rows,uint32_t count,uint32_t elapsed)
{
    char line[96],h[8],m[8],s[8];uint32_t i;int status;
    if(!ps2 || !sink || (!rows && count))return RF_RANGE;
    drawn();
    status=backdrop(sink,context,0x606060u);if(status)return status;
    status=options_frame(sink,context,str(66));if(status)return status;
    {static const struct {uint16_t id,x,y;} heads[]={{67,44,147},{71,522,147},{68,268,147},{69,330,147},{415,418,130},{416,403,147}};
     for(i=0;i<6;i++){status=text(sink,context,RF_PS2_FONT_MESSAGE,heads[i].x,heads[i].y,str(heads[i].id),RF_PS2_COLOR_TEXT);if(status)return status;}}
    for(i=0;i<count;i++){
        float y=174.0f+18.0f*i;uint32_t c=i?RF_PS2_COLOR_TEXT:RF_PS2_COLOR_SELECTED,tenths;
        if(count>2 && !(i&1)){status=rect(sink,context,34,y-1,581,17,0x20000000u);if(status)return status;}
        status=text(sink,context,RF_PS2_FONT_STANDARD,44,y,rows[i].name,c);if(status)return status;
        snprintf(line,sizeof(line),"%d",(int)rows[i].kills);
        status=text(sink,context,RF_PS2_FONT_STANDARD,281-(int32_t)(width_of(RF_PS2_FONT_STANDARD,line)>>1),y,line,c);if(status)return status;
        snprintf(line,sizeof(line),"%d",(int)rows[i].deaths);
        status=text(sink,context,RF_PS2_FONT_STANDARD,354-(int32_t)(width_of(RF_PS2_FONT_STANDARD,line)>>1),y,line,c);if(status)return status;
        snprintf(line,sizeof(line),"%u",rows[i].longest);
        status=text(sink,context,RF_PS2_FONT_STANDARD,446-(int32_t)(width_of(RF_PS2_FONT_STANDARD,line)>>1),y,line,c);if(status)return status;
        /* "%5.1f %%" without float printf: width 5, one decimal. */
        tenths=(uint32_t)(rows[i].accuracy*1000.0f+.5f);snprintf(line,sizeof(line),"%3u.%u %%",tenths/10,tenths%10);
        status=text(sink,context,RF_PS2_FONT_STANDARD,558-(int32_t)(width_of(RF_PS2_FONT_STANDARD,line)>>1),y,line,c);if(status)return status;
    }
    mp_stats_field(h,sizeof(h),elapsed/3600);mp_stats_field(m,sizeof(m),elapsed/60%60);mp_stats_field(s,sizeof(s),elapsed%60);
    snprintf(line,sizeof(line),str(419),h,m,s);
    status=text(sink,context,RF_PS2_FONT_STANDARD,CENTER,320,line,0xffffffffu);if(status)return status;
    snprintf(line,sizeof(line),str(72),"A");status=text(sink,context,RF_PS2_FONT_STANDARD,135,372,line,0xffffffffu);if(status)return status;
    snprintf(line,sizeof(line),str(73),"B");status=text(sink,context,RF_PS2_FONT_STANDARD,355,372,line,0xffffffffu);if(status)return status;
    snprintf(line,sizeof(line),str(74),"X");return text(sink,context,RF_PS2_FONT_STANDARD,190,397,line,0xffffffffu);
}
/* Multiplayer HUD (PS2 1691a0 in place of the single-player health display,
 * and the clock of 1dc1a0), in the PS2's 640x448 coordinates. A separate
 * small picture set stays loaded for the match. */
static rf_ps2_menu *ps2_hud;static int ps2_hud_failed;
int rf_xbox_ps2_mp_hud_open(void)
{
    if(ps2_hud)return RF_OK;
    if(ps2_hud_failed)return ps2_hud_failed; /* tried once this match */
    if(!rf_xbox_ps2_available())return ps2_hud_failed=RF_NOT_FOUND;
    ps2_hud_failed=rf_ps2_menu_open(&ps2_hud,"D:\\ps2menu.vpp",RF_PS2_MENU_MP_HUD,1024u*1024u);
    return ps2_hud_failed;
}
void rf_xbox_ps2_mp_hud_close(void){if(ps2_hud){rf_ps2_menu_close(ps2_hud);ps2_hud=NULL;}ps2_hud_failed=0;}
int rf_xbox_ps2_draw_mp_winner(rf_scene_particle_sink sink,void *context,uint32_t alpha)
{
    rf_ps2_menu *menu=ps2;int status;
    if(!ps2_hud || !alpha)return RF_OK;
    ps2=ps2_hud;
    /* The trim's mode 0x10c41 has blend field (bits 15-19) 2, additive, as
     * the glow mode. winner_win: 15 frames at fps 0 (PEG entry byte 8;
     * 1f1d60 picks floor(ms * fps / 1000) % frames), so always frame 0. */
    {rf_hud_sprite s;status=rf_ps2_menu_picture(ps2,RF_PS2_WINNER_TRIM,&s);
     if(!status){
        rf_particle_draw_vertex v[4];uint32_t i;memset(v,0,sizeof(v));
        for(i=0;i<4;i++){uint32_t right=i==1||i==2,bottom=i>=2;
            v[i].screen[0]=264.0f+(right?(float)s.width:0);v[i].screen[1]=(123.0f+(bottom?(float)s.height:0))*Y_SCALE;
            v[i].reciprocal_w=1;v[i].argb=(alpha<<24)|0xffffffu;v[i].uv[0]=s.uv[right?2:0];v[i].uv[1]=s.uv[bottom?3:1];}
        status=sink(context,v,4,s.image,RF_PARTICLE_GLOW_MODE&~(31u<<20));
     }else if(status==RF_NOT_FOUND)status=RF_OK;}
    if(!status)status=picture(sink,context,RF_PS2_WINNER_WIN,285,113,(alpha<<24)|0xffffffu,0);
    ps2=menu;return status;
}
int rf_xbox_ps2_draw_mp_hud(rf_scene_particle_sink sink,void *context,const rf_xbox_ps2_mp_hud *h)
{
    rf_ps2_menu *menu=ps2;char line[32];int status=RF_OK;uint32_t w;
    if(!ps2_hud || !h)return RF_OK;
    ps2=ps2_hud; /* the helpers draw from the current set */
    /* 1691a0: icon at (17,21); health (at least 1) and armour centred on x 34
     * at y 22 / 43 in the small font, colour 2ac114. */
    status=picture(sink,context,RF_PS2_MULTI_HUD_HEALTH,17,21,0xffffffffu,0);
    if(!status){snprintf(line,sizeof(line),"%d",h->health<2?1:h->health);w=width_of(RF_PS2_FONT_SMALL,line);
        status=text(sink,context,RF_PS2_FONT_SMALL,34-(int32_t)(w>>1),22,line,RF_PS2_COLOR_MP_HUD);}
    if(!status){snprintf(line,sizeof(line),"%d",h->armor);w=width_of(RF_PS2_FONT_SMALL,line);
        status=text(sink,context,RF_PS2_FONT_SMALL,34-(int32_t)(w>>1),43,line,RF_PS2_COLOR_MP_HUD);}
    /* Name, "Kills:" (384) and kills, "Rank:" (385) and rank/total in the
     * standard font, colour (220,220,220). */
    if(!status)status=text(sink,context,RF_PS2_FONT_STANDARD,17,97,h->name,RF_PS2_COLOR_TEXT);
    if(!status)status=text(sink,context,RF_PS2_FONT_STANDARD,17,116,str(384),RF_PS2_COLOR_TEXT);
    if(!status){snprintf(line,sizeof(line),"%d",h->kills);status=text(sink,context,RF_PS2_FONT_STANDARD,88,116,line,RF_PS2_COLOR_TEXT);}
    if(!status){snprintf(line,sizeof(line),"%u/%u",h->rank,h->total);status=text(sink,context,RF_PS2_FONT_STANDARD,88,135,line,RF_PS2_COLOR_TEXT);}
    if(!status)status=text(sink,context,RF_PS2_FONT_STANDARD,17,135,str(385),RF_PS2_COLOR_TEXT);
    /* 1dc1a0 clock with a time limit: frame at (17,400) alpha 0x78, colon dots
     * (43,408) and (43,413), seconds "%02d" at (47,402), minutes right-aligned
     * at x 41, small font, colour 2ac114. */
    if(!status && h->seconds_left!=UINT32_MAX){
        uint32_t minutes=h->seconds_left/60,seconds=h->seconds_left%60;
        status=picture(sink,context,RF_PS2_MULTI_HUD_TIME,17,400,0x78ffffffu,0);
        if(!status)status=rect(sink,context,43,408,2,2,RF_PS2_COLOR_MP_HUD);
        if(!status)status=rect(sink,context,43,413,2,2,RF_PS2_COLOR_MP_HUD);
        if(!status){snprintf(line,sizeof(line),"%02u",seconds);status=text(sink,context,RF_PS2_FONT_SMALL,47,402,line,RF_PS2_COLOR_MP_HUD);}
        if(!status){snprintf(line,sizeof(line),"%02u",minutes);w=width_of(RF_PS2_FONT_SMALL,line);
            status=text(sink,context,RF_PS2_FONT_SMALL,41-(int32_t)w,402,line,RF_PS2_COLOR_MP_HUD);}
    }
    ps2=menu;return status;
}
