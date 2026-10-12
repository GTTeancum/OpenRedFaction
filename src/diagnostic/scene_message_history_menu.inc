/* Read-only, suspended-scene history view. The live HUD atlas, when present,
 * is borrowed only for this synchronous draw; the Xbox HUD batch fences it
 * before returning, including error returns. Never set particle_draw_stream.
 * No scene tick, storage access, event dispatch or audio action occurs here. */
enum {
    SCENE_MESSAGE_HISTORY_DISPLAY_BYTES=2*SCENE_MESSAGE_HISTORY_TEXT_BYTES,
    SCENE_MESSAGE_HISTORY_PAGE_LINES=12,
    SCENE_MESSAGE_HISTORY_MAX_PAGES=(SCENE_MESSAGE_HISTORY_DISPLAY_BYTES+
        SCENE_MESSAGE_HISTORY_PAGE_LINES-2)/SCENE_MESSAGE_HISTORY_PAGE_LINES
};
/* Static singleton scratch avoids placing a second large string on the paused
 * scene's stack. Neither this buffer nor the ring retains a texture pointer. */
static struct {
    char expanded[SCENE_MESSAGE_HISTORY_DISPLAY_BYTES];
    char line[SCENE_HUD_LINE_BYTES];
} scene_message_history_draw;
typedef char scene_message_history_budget[
    sizeof(scene_message_history)+sizeof(scene_message_history_draw)<=12u*1024u?1:-1];
static uint32_t scene_message_history_fallback_char(unsigned char ch)
{
    return (ch>='a'&&ch<='z') || (ch>='A'&&ch<='Z') || (ch>='0'&&ch<='9') ||
        ch=='\r' || ch=='\n' || ch=='\t' || strchr(" .,!?'\":-()/%",ch)!=NULL;
}
static int scene_message_history_line(const rf_hud_assets *assets,const char *text,
    uint32_t length,uint32_t *consumed)
{
    uint32_t pixels,skip=0,n=0,last_space=UINT32_MAX,after;
    char *line=scene_message_history_draw.line;
    if(assets)return scene_hud_message_line(assets,text,length,552,line,consumed,&pixels);
    while(skip<length&&(text[skip]==' '||text[skip]=='\t'))++skip;
    text+=skip;length-=skip;
    while(n<length&&text[n]!='\r'&&text[n]!='\n'&&n<46){
        line[n]=text[n]=='\t'?' ':text[n];if(line[n]==' ')last_space=n;++n;
    }
    after=n;
    if(n<length&&(text[n]=='\r'||text[n]=='\n')){
        char separator=text[after++];
        if(separator=='\r'&&after<length&&text[after]=='\n')++after;
    }else if(n<length&&text[n]!=' '&&text[n]!='\t'&&last_space!=UINT32_MAX&&last_space){
        n=last_space;after=last_space+1;
    }
    while(n&&line[n-1]==' ')--n;
    line[n]=0;*consumed=skip+after;
    return *consumed?RF_OK:RF_RANGE;
}
static int scene_message_history_label(const rf_hud_assets *assets,
    rf_scene_particle_sink sink,void *context,float y,const char *text,uint32_t color)
{
    if(assets)return scene_hud_text_box(assets,sink,context,RF_HUD_FONT_TEXT,text,24,y,592,0,color);
    return combat_hud_centered(sink,context,y,text,color);
}
int rf_scene_draw_message_history(rf_scene_particle_sink sink,void *context,
    uint32_t index,uint32_t page,uint32_t flags,uint32_t *pages)
{
    const scene_message_history_entry *entry=scene_message_history_get(index);
    const rf_hud_assets *assets=scene_actor_collision_owner?scene_actor_collision_owner->hud_assets:NULL;
    uint32_t length=0,offset=0,lines=0,start=0,rows,h,pitch,i,raw=0,replaced=0;
    uint32_t bound=0,unbound=0,unknown=0,bytes=0;int status;char label[80];
    if(!sink || !pages || particle_draw_stream ||
       (flags&~RF_SCENE_HISTORY_DISCONNECTED) || page>=SCENE_MESSAGE_HISTORY_MAX_PAGES ||
       (!entry && (scene_message_history.count || index || page)))return RF_RANGE;
    *pages=1;
    h=assets?rf_hud_assets_font_height(assets,RF_HUD_FONT_TEXT):14;
    /* The installed TEXT font is12px high. A custom oversized font falls
     * back instead of shrinking body text or overrunning the twelve rows. */
    if(!h || h>14){assets=NULL;h=14;}pitch=h+4;
    status=scene_message_history_label(assets,sink,context,36,"MESSAGE HISTORY",0xffeeeeee);if(status)return status;
    if(flags&RF_SCENE_HISTORY_DISCONNECTED){
        status=scene_message_history_label(assets,sink,context,176,"CONTROLLER DISCONNECTED",0xffffc060);if(status)return status;
        return scene_message_history_label(assets,sink,context,224,"CONNECT A CONTROLLER TO CONTINUE",0xffeeeeee);
    }
    if(!entry){
        status=scene_message_history_label(assets,sink,context,176,"NO RECENT MESSAGES",0xffeeeeee);if(status)return status;
        status=scene_message_history_label(assets,sink,context,224,"RECEIVED STORY TEXT WILL APPEAR HERE",0xffb8c0d0);if(status)return status;
    }else {
        status=scene_hud_control_text(entry->text,sizeof(entry->text),scene_message_history_draw.expanded,
            sizeof(scene_message_history_draw.expanded),&bound,&unbound,&unknown,&bytes);
        if(status){
            /* Preserve full raw text if a future binding outgrows the display
             * workspace. The visible warning explains unexpanded tokens. */
            memcpy(scene_message_history_draw.expanded,entry->text,sizeof(entry->text));raw=1;
        }
        while(length<sizeof(scene_message_history_draw.expanded)&&scene_message_history_draw.expanded[length])++length;
        if(length==sizeof(scene_message_history_draw.expanded))return RF_FORMAT;
        if(!assets)for(i=0;i<length;i++)if(!scene_message_history_fallback_char((unsigned char)scene_message_history_draw.expanded[i])){
            scene_message_history_draw.expanded[i]='?';replaced=1;
        }
        /* Every wrapped line consumes at least one source byte. Thus the
         * 1023-byte display bounds this scan to1023 lines /86 pages; none of
         * a received message is hidden merely because it exceeds one page. */
        while(offset<length){
            uint32_t consumed;
            if(lines==page*SCENE_MESSAGE_HISTORY_PAGE_LINES)start=offset;
            status=scene_message_history_line(assets,scene_message_history_draw.expanded+offset,length-offset,&consumed);
            if(status)return status;if(!consumed||consumed>length-offset)return RF_FORMAT;
            offset+=consumed;++lines;
        }
        *pages=lines?(lines+SCENE_MESSAGE_HISTORY_PAGE_LINES-1)/SCENE_MESSAGE_HISTORY_PAGE_LINES:1;
        if(page>=*pages)return RF_RANGE;
        snprintf(label,sizeof(label),"MESSAGE %u/%u   PAGE %u/%u",index+1,scene_message_history.count,page+1,*pages);
        status=scene_message_history_label(assets,sink,context,64,label,0xff80ffb0);if(status)return status;
        snprintf(label,sizeof(label),"LEVEL %.40s%s",entry->level,strlen(entry->level)>40?"...":"");
        if(!assets)for(i=0;label[i];i++)if(!scene_message_history_fallback_char((unsigned char)label[i]))label[i]='?';
        status=scene_message_history_label(assets,sink,context,88,label,0xffb8c0d0);if(status)return status;
        status=combat_hud_rect(sink,context,32,116,576,224,0xff18202c);if(status)return status;
        offset=start;
        for(rows=0;offset<length&&rows<SCENE_MESSAGE_HISTORY_PAGE_LINES;rows++){
            uint32_t consumed;
            status=scene_message_history_line(assets,scene_message_history_draw.expanded+offset,length-offset,&consumed);
            if(status)return status;if(!consumed||consumed>length-offset)return RF_FORMAT;
            offset+=consumed;
            if(assets)status=scene_hud_text_box(assets,sink,context,RF_HUD_FONT_TEXT,
                scene_message_history_draw.line,44,122+rows*pitch,0,0,0xffeeeeee);
            else status=combat_hud_text(sink,context,44,122+rows*pitch,scene_message_history_draw.line,0xffeeeeee);
            if(status)return status;
        }
        if(scene_message_history.dropped)
            snprintf(label,sizeof(label),"OLDER MESSAGES DROPPED: %u",scene_message_history.dropped);
        else snprintf(label,sizeof(label),"UP OLDER / DOWN NEWER   LEFT / RIGHT PAGE");
        status=scene_message_history_label(assets,sink,context,352,label,0xffb8c0d0);if(status)return status;
        if(raw || replaced){
            status=scene_message_history_label(assets,sink,context,374,
                raw?(replaced?"RAW TOKENS / UNSUPPORTED CHARACTERS: ?":"CONTROL LABELS UNAVAILABLE - RAW TEXT"):
                "UNSUPPORTED CHARACTERS ARE SHOWN AS ?",0xffffc060);if(status)return status;
        }
    }
    status=scene_message_history_label(assets,sink,context,400,"HISTORY CLEARS ON RESTART OR LOAD",0xffb8c0d0);if(status)return status;
    status=scene_message_history_label(assets,sink,context,422,"UP/DOWN MESSAGE   LEFT/RIGHT PAGE",0xffeeeeee);if(status)return status;
    return scene_message_history_label(assets,sink,context,446,"B BACK   START RESUME",0xffeeeeee);
}
