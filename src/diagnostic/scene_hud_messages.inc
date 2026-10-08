/* Original rfpc-medium glyphs and hud.tbl message anchors. This presentation
 * layer borrows the existing strings/deadlines; it owns no message queue,
 * gameplay timer, voice lifetime or inventory state. */
#define SCENE_HUD_MESSAGE_LINES 16u
#define SCENE_HUD_LINE_BYTES 128u
static int scene_hud_message_line(const rf_hud_assets *a,const char *text,uint32_t length,
    uint32_t limit,char line[SCENE_HUD_LINE_BYTES],uint32_t *consumed,uint32_t *pixels)
{
    uint32_t skip=0,n=0,width=0,last_space=UINT32_MAX,after;int status;
    while(skip<length&&(text[skip]==' '||text[skip]=='\t'))++skip;
    text+=skip;length-=skip;
    while(n<length&&text[n]!='\r'&&text[n]!='\n'&&n+1<SCENE_HUD_LINE_BYTES){
        rf_hud_glyph glyph;unsigned char ch=text[n]=='\t'?' ':(unsigned char)text[n];
        status=rf_hud_assets_glyph(a,RF_HUD_FONT_TEXT,ch,&glyph);if(status)return status;
        if(glyph.advance<0)return RF_FORMAT;
        if(n && (uint32_t)glyph.advance>limit-width)break;
        line[n]=(char)ch;if(ch==' ')last_space=n;
        width+=(uint32_t)glyph.advance;++n;
        if(width>=limit)break;
    }
    after=n;
    if(n<length&&(text[n]=='\r'||text[n]=='\n')){
        char separator=text[after++];
        if(after<length&&separator=='\r'&&text[after]=='\n')++after;
    } else if(n<length&&text[n]!=' '&&text[n]!='\t'&&last_space!=UINT32_MAX&&last_space){
        n=last_space;after=last_space+1;
    }
    while(n&&line[n-1]==' ')--n;
    line[n]=0;*consumed=skip+after;
    /* Empty trailing whitespace is consumed without inventing a glyph. */
    if(!*consumed&&length)return RF_RANGE;
    return rf_hud_assets_text_width(a,RF_HUD_FONT_TEXT,line,pixels);
}
static int scene_hud_message_block(const rf_hud_assets *a,rf_scene_particle_sink sink,void *context,
    const char *text,uint32_t capacity,uint32_t width,float y,float bottom,uint32_t maximum_lines,
    uint32_t color,uint32_t background)
{
    char lines[SCENE_HUD_MESSAGE_LINES][SCENE_HUD_LINE_BYTES];uint32_t sizes[SCENE_HUD_MESSAGE_LINES];
    uint32_t length=0,offset=0,count=0,max_width=0,h,pitch,i;int status;
    if(!text||!capacity||!width||width>608)return RF_RANGE;
    while(length<capacity&&text[length])++length;
    if(length==capacity)return RF_RANGE;
    h=rf_hud_assets_font_height(a,RF_HUD_FONT_TEXT);if(!h)return RF_FORMAT;pitch=h+2;
    if(maximum_lines>SCENE_HUD_MESSAGE_LINES)maximum_lines=SCENE_HUD_MESSAGE_LINES;
    while(offset<length&&count<maximum_lines&&y+count*pitch+h<=bottom){
        uint32_t consumed;
        status=scene_hud_message_line(a,text+offset,length-offset,width,lines[count],&consumed,sizes+count);if(status)return status;
        if(consumed>length-offset)return RF_FORMAT;
        offset+=consumed;
        if(!sizes[count]&&offset==length)break;
        if(sizes[count]>max_width)max_width=sizes[count];++count;
    }
    if(!count)return RF_OK;
    {uint32_t tail=offset;
     while(tail<length&&(text[tail]==' '||text[tail]=='\t'||text[tail]=='\r'||text[tail]=='\n'))++tail;
     if(tail==length)offset=length;}
    if(offset<length){
        char *line=lines[count-1];uint32_t n=(uint32_t)strlen(line),w;
        /* A viewport-bounded overflow is explicit; never write below the
         * display or silently reduce every glyph to an unreadable size. */
        if(n>SCENE_HUD_LINE_BYTES-4)n=SCENE_HUD_LINE_BYTES-4;
        do {
            memcpy(line+n,"...",4);
            status=rf_hud_assets_text_width(a,RF_HUD_FONT_TEXT,line,&w);if(status)return status;
            if(w<=width||!n)break;--n;
        }while(1);
        sizes[count-1]=w;if(w>max_width)max_width=w;++rf_hud_text_diagnostic[4];
    }
    {
        float panel_width=fminf((float)width+12,(float)max_width+12);
        status=combat_hud_rect(sink,context,(640-panel_width)*.5f,y-4,panel_width,(count-1)*pitch+h+8,background);if(status)return status;
    }
    for(i=0;i<count;i++){
        status=scene_hud_text_box(a,sink,context,RF_HUD_FONT_TEXT,lines[i],(640.f-width)*.5f,
            y+i*pitch,(float)width,0,color);if(status)return status;
    }
    rf_hud_text_diagnostic[3]+=count;return RF_OK;
}
static int scene_hud_story_draw(rf_scene_particle_sink sink,void *context,const char *text)
{
    const rf_hud_assets *a=particle_draw_stream->hud_assets;int32_t message[2],subtitle[2];int status;
    uint32_t width;float y;
    status=rf_hud_assets_position(a,37,message);if(status)return status;
    status=rf_hud_assets_position(a,36,subtitle);if(status)return status;
    if(message[0]<=0||message[0]>608||message[1]<4||message[1]>460||subtitle[1]<4||subtitle[1]>460)return RF_FORMAT;
    width=(uint32_t)message[0];y=(float)message[1];
    /* The current message owner has no persona metadata. During a cinematic,
     * use the authored subtitle Y and the existing full-width subtitle region;
     * do not guess a persona/portrait from its optional voice filename. */
    if(campaign_cutscene_runtime.active){width=552;y=(float)subtitle[1];}
    status=scene_hud_message_block(a,sink,context,text,sizeof(campaign_subtitle.text),width,y,472,
        SCENE_HUD_MESSAGE_LINES,rf_hud_assets_color(a,RF_HUD_COLOR_MESSAGE),rf_hud_assets_color(a,RF_HUD_COLOR_MESSAGE_BACKGROUND));
    rf_hud_text_diagnostic[7]=(uint32_t)status;if(!status)++rf_hud_text_diagnostic[2];return status;
}
static int scene_hud_notice_draw(rf_scene_particle_sink sink,void *context,const char *text,
    uint32_t lane,uint32_t color)
{
    const rf_hud_assets *a=particle_draw_stream->hud_assets;int32_t anchor[2];uint32_t h,pitch;float y;int status;
    if(lane>1)return RF_RANGE;
    status=rf_hud_assets_position(a,37,anchor);if(status)return status;
    if(anchor[0]<=0||anchor[0]>608)return RF_FORMAT;
    h=rf_hud_assets_font_height(a,RF_HUD_FONT_TEXT);if(!h)return RF_FORMAT;pitch=h+2;
    /* Two short transient lanes sit above the authored story anchor so a
     * pickup/save notice cannot erase an active mission instruction. */
    y=(float)anchor[1]-(float)(lane+1)*(2*pitch+12);
    status=scene_hud_message_block(a,sink,context,text,sizeof(rf_scene_pickup_notice),(uint32_t)anchor[0],y,y+2*pitch,2,
        color,rf_hud_assets_color(a,RF_HUD_COLOR_MESSAGE_BACKGROUND));
    rf_hud_text_diagnostic[7]=(uint32_t)status;if(!status)++rf_hud_text_diagnostic[5];return status;
}
static int scene_hud_countdown_draw(rf_scene_particle_sink sink,void *context,const char *text,
    uint32_t seconds,uint32_t warning_color)
{
    const rf_hud_assets *a=particle_draw_stream->hud_assets;int32_t xy[2];uint32_t width,h,color;int status;
    status=rf_hud_assets_position(a,39,xy);if(status)return status;
    status=rf_hud_assets_text_width(a,RF_HUD_FONT_TEXT,text,&width);if(status)return status;
    h=rf_hud_assets_font_height(a,RF_HUD_FONT_TEXT);if(!h||xy[0]<4||xy[1]<4||xy[0]+width>636||xy[1]+h>476)return RF_FORMAT;
    color=seconds<=60?warning_color:rf_hud_assets_color(a,RF_HUD_COLOR_COUNTDOWN);
    status=combat_hud_rect(sink,context,(float)xy[0]-4,(float)xy[1]-3,(float)width+8,(float)h+6,
        rf_hud_assets_color(a,RF_HUD_COLOR_MESSAGE_BACKGROUND));if(status)return status;
    status=scene_hud_text_box(a,sink,context,RF_HUD_FONT_TEXT,text,(float)xy[0],(float)xy[1],0,0,color);
    rf_hud_text_diagnostic[7]=(uint32_t)status;if(!status)++rf_hud_text_diagnostic[6];return status;
}
