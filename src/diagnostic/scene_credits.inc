/* Authored credits, loaded only at the terminal boundary. Original43dc80
 * selects the language section,43dd4f wraps to viewport width minus64, and
 *43df61 scrolls at40 pixels/second. Black backdrop, the existing TEXT font,
 * release-rearmed Use and natural return are practical first-playable choices. */
#define SCENE_CREDITS_TEXT_LIMIT 16384u
#define SCENE_CREDITS_ROWS 512u
#define SCENE_CREDITS_LINE_BYTES 128u
#define SCENE_CREDITS_WIDTH 576u
static struct {
    char *text;
    uint32_t bytes,begin,end,rows,height,pitch,ticks,finish_ticks,laid_out,presented,notice_ticks;
    struct {uint16_t offset,length;} row[SCENE_CREDITS_ROWS];
    int text_status;
} scene_credits;

static void scene_credits_close(void)
{
    free(scene_credits.text);memset(&scene_credits,0,sizeof(scene_credits));
}
static int scene_credits_section(uint32_t language)
{
    static const char *const tags[]={"#English","#German","#French"};
    uint32_t pos=0,selected=0,nonblank=0,begin=0;
    while(pos<scene_credits.bytes){
        uint32_t start=pos,end;
        while(pos<scene_credits.bytes && scene_credits.text[pos]!='\n' && scene_credits.text[pos]!='\r')++pos;
        end=pos;
        if(pos<scene_credits.bytes){char ch=scene_credits.text[pos++];
            if(ch=='\r' && pos<scene_credits.bytes && scene_credits.text[pos]=='\n')++pos;}
        while(end>start && scene_credits.text[end-1]==' ')--end;
        while(start<end && scene_credits.text[start]==' ')++start;
        if(!selected){
            if(end-start==strlen(tags[language]) && !memcmp(scene_credits.text+start,tags[language],end-start)){
                selected=1;begin=pos;
            }
        }else if(end>start && scene_credits.text[start]=='#'){
            if(end-start!=4 || memcmp(scene_credits.text+start,"#End",4))return RF_FORMAT;
            if(!nonblank)return RF_NOT_FOUND;
            scene_credits.begin=begin;scene_credits.end=start;return RF_OK;
        }else if(end>start){
            const char *text=scene_credits.text+start;uint32_t bytes=end-start;
            if(language!=RF_TEXT_ENGLISH && (!rf_text_translation_available(text,bytes) ||
               (bytes==27 && !memcmp(text,"Insert French credits here.",27)) ||
               (bytes==27 && !memcmp(text,"Insert German credits here.",27))))return RF_NOT_FOUND;
            nonblank=1;
        }
    }
    return selected?RF_FORMAT:RF_NOT_FOUND;
}
static int scene_credits_load(const char *tables_path)
{
    rf_vpp archive={0};rf_vpp_entry entry;uint32_t pos;
    int status;
    if(!tables_path)return RF_RANGE;
    status=rf_vpp_open(&archive,tables_path);if(status)return status;
    status=rf_vpp_find(&archive,"credits.tbl",&entry);if(status)goto done;
    if(!entry.size || entry.size>SCENE_CREDITS_TEXT_LIMIT){status=RF_RANGE;goto done;}
    scene_credits.text=malloc(entry.size+1);if(!scene_credits.text){status=RF_IO;goto done;}
    status=rf_vpp_read(&archive,&entry,0,scene_credits.text,entry.size);if(status)goto done;
    scene_credits.text[entry.size]=0;scene_credits.bytes=entry.size;
    /* No embedded terminator or unsupported control byte may hide a section. */
    for(pos=0;pos<entry.size;pos++){
        unsigned char ch=(unsigned char)scene_credits.text[pos];
        if((ch<32 && ch!='\r' && ch!='\n' && ch!='\t') || ch==127){status=RF_FORMAT;goto done;}
        if(ch=='\t')scene_credits.text[pos]=' ';
    }
    status=scene_credits_section(scene_text_language);
    if(status && scene_text_language!=RF_TEXT_ENGLISH)status=scene_credits_section(RF_TEXT_ENGLISH);
done:
    rf_vpp_close(&archive);
    if(status){free(scene_credits.text);scene_credits.text=NULL;scene_credits.bytes=0;}
    return status;
}
static void scene_credits_begin(const char *tables_path)
{
    rf_vpp tables={0};char music_name[64];uint32_t i;int status;
    scene_credits_close();
    scene_credits.text_status=scene_credits_load(tables_path);
    /* Failed optional text still has a readable exit and a finite10-second
     * fallback. Do not turn a completed campaign into a resource error. */
    scene_credits.finish_ticks=600;
    campaign_endgame.use_held=1; /* The winning input must be released first. */
    for(i=0;i<RF_AUDIO_VOICES;i++)if(campaign_audio_mixer.voices[i].active){
        uint32_t handle=campaign_audio_mixer.voices[i].handle;
        rf_audio_voice_stop(&campaign_audio_mixer,handle);
        if(campaign_audio_events.stop)campaign_audio_events.stop(campaign_audio_events_context,handle);
    }
    /* A stop request does not release PCM or native borrowers. Their ordinary
     * full-scene retirement remains responsible even if stopping times out. */
    rf_music_stop(&campaign_music_stream,0);++campaign_music_revision;
    /* Original completion431610 enables43dec6's configured menu-music start.
     * Read only at entry; missing/malformed optional music leaves the old
     * stream stopped and the completed campaign/credits presentation intact. */
    status=tables_path?rf_vpp_open(&tables,tables_path):RF_RANGE;
    if(!status)status=rf_game_main_menu_music_load(&tables,65536,music_name);
    rf_vpp_close(&tables);
    if(!status)status=rf_music_start(&campaign_music_stream,&campaign_music_archive,music_name);
    if(!status){
        const rf_sound_metadata *metadata=rf_sound_metadata_find(campaign_audio_metadata.rows,
            campaign_audio_metadata.order,music_name);
        /* Same existing whole-track loop policy as scripted Music_Start. */
        campaign_music_stream.looping=metadata && (metadata->loop_flags&0x40000000u) &&
            !(metadata->loop_flags&0x07ffffffu);
        ++rf_scene_music[0];
    }else ++rf_scene_music[2];
    rf_scene_music[3]=campaign_music_stream.active;rf_scene_music[5]=(uint32_t)status;
    printf("CREDITS_BEGIN text%d bytes%u music%d\n",scene_credits.text_status,scene_credits.bytes,status);
}
static int scene_credits_input(uint32_t use)
{
    uint32_t pressed=use && !campaign_endgame.use_held;
    campaign_endgame.use_held=!!use;
    if(pressed || (scene_credits.presented && scene_credits.ticks>=scene_credits.finish_ticks)){
        printf("CREDITS_RETURN automatic%u ticks%u\n",!pressed,scene_credits.ticks);
        rf_scene_session_end();return RF_NOT_FOUND;
    }
    return RF_OK;
}
static int scene_credits_add_row(uint32_t offset,uint32_t length)
{
    if(scene_credits.rows>=SCENE_CREDITS_ROWS || offset>scene_credits.bytes ||
       length>scene_credits.bytes-offset || length>=SCENE_CREDITS_LINE_BYTES)return RF_RANGE;
    scene_credits.row[scene_credits.rows].offset=(uint16_t)offset;
    scene_credits.row[scene_credits.rows++].length=(uint16_t)length;
    return RF_OK;
}
static int scene_credits_layout(const rf_hud_assets *assets)
{
    uint32_t pos=scene_credits.begin;int status;
    scene_credits.height=assets?rf_hud_assets_font_height(assets,RF_HUD_FONT_TEXT):14;
    if(!scene_credits.height || scene_credits.height>32)return RF_FORMAT;
    scene_credits.pitch=scene_credits.height+4;
    while(pos<scene_credits.end){
        uint32_t start=pos,end;
        while(pos<scene_credits.end && scene_credits.text[pos]!='\r' && scene_credits.text[pos]!='\n')++pos;
        end=pos;
        if(pos<scene_credits.end){char ch=scene_credits.text[pos++];
            if(ch=='\r' && pos<scene_credits.end && scene_credits.text[pos]=='\n')++pos;}
        while(end>start && scene_credits.text[end-1]==' ')--end;
        while(start<end && scene_credits.text[start]==' ')++start;
        if(start==end){status=scene_credits_add_row(start,0);if(status)return status;continue;}
        while(start<end){
            uint32_t stop=start,width=0,last_space=UINT32_MAX,next,length;
            while(stop<end && stop-start+1<SCENE_CREDITS_LINE_BYTES){
                uint32_t advance=12;
                if(assets){rf_hud_glyph glyph;
                    status=rf_hud_assets_glyph(assets,RF_HUD_FONT_TEXT,(unsigned char)scene_credits.text[stop],&glyph);if(status)return status;
                    if(glyph.advance<0 || glyph.advance>(int32_t)SCENE_CREDITS_WIDTH)return RF_FORMAT;
                    advance=(uint32_t)glyph.advance;
                }
                if(advance>SCENE_CREDITS_WIDTH-width)break;
                width+=advance;
                if(scene_credits.text[stop]==' ')last_space=stop;
                ++stop;
            }
            if(stop==start)return RF_RANGE;
            next=stop;
            if(stop<end && scene_credits.text[stop]!=' ' && last_space!=UINT32_MAX && last_space>start){
                stop=last_space;next=last_space+1;
            }
            length=stop-start;
            while(length && scene_credits.text[start+length-1]==' ')--length;
            status=scene_credits_add_row(start,length);if(status)return status;
            start=next;while(start<end && scene_credits.text[start]==' ')++start;
        }
    }
    if(!scene_credits.rows)return RF_FORMAT;
    /*40 pixels per simulation second; two seconds after the final line clears
     * the viewport, return through the same full-session path as Use. Retail
     *43df91 instead wraps the roll; natural exit is a practical port choice. */
    scene_credits.finish_ticks=(356u+scene_credits.rows*scene_credits.pitch)*3u/2u+120u;
    return RF_OK;
}
static int scene_credits_draw(rf_scene_particle_sink sink,void *context)
{
    uint32_t i;int status;
    if(!scene_credits.laid_out){
        scene_credits.laid_out=1;
        if(!scene_credits.text_status)scene_credits.text_status=scene_credits_layout(particle_draw_stream->hud_assets);
        if(scene_credits.text_status){scene_credits.rows=0;scene_credits.finish_ticks=600;}
        printf("CREDITS_LAYOUT status%d rows%u pitch%u ticks%u\n",scene_credits.text_status,
            scene_credits.rows,scene_credits.pitch,scene_credits.finish_ticks);
    }
    scene_credits.presented=1;
    status=combat_hud_centered(sink,context,24,scene_text(983,"CREDITS"),0xffeeeeee);if(status)return status;
    if(scene_credits.text_status){
        status=combat_hud_centered(sink,context,222,"CREDITS TEXT UNAVAILABLE",0xffeeeeee);if(status)return status;
    }else for(i=0;i<scene_credits.rows;i++){
        float y=420.0f+i*scene_credits.pitch-scene_credits.ticks*(2.0f/3.0f);
        char line[SCENE_CREDITS_LINE_BYTES];uint32_t length=scene_credits.row[i].length;
        float fade;uint32_t alpha;
        if(!length || y<64 || y+scene_credits.height>420)continue;
        memcpy(line,scene_credits.text+scene_credits.row[i].offset,length);line[length]=0;
        fade=fminf(y-64,420-y-scene_credits.height)/24.0f;
        alpha=fade>=1?255:(uint32_t)(fade*255);
        status=combat_hud_centered(sink,context,y,line,(alpha<<24)|0xeeeeeeu);if(status)return status;
    }
    if(scene_credits.notice_ticks)
        return combat_hud_centered(sink,context,444,"SAVE/LOAD UNAVAILABLE DURING CREDITS",0xffffc060);
    {static const char source[]="$USE$ TO RETURN TO MAIN MENU";
     char prompt[96];uint32_t bound,unbound,unknown,bytes;
     status=scene_hud_control_text(source,sizeof(source),prompt,sizeof(prompt),&bound,&unbound,&unknown,&bytes);
     if(status)return status;
     return combat_hud_centered(sink,context,444,prompt,0xffeeeeee);}
}
static int scene_credits_frame(scene_stream *stream,uint32_t frame)
{
    rf_preview_mesh empty=*stream->mesh;int status;
    /* Existing renderer/HUD fences stay intact; no world geometry, effects,
     * camera/weapon tick, event tick or save capture is advanced for this modal. */
    empty.count=empty.bytes=0;
    if(scene_credits.presented && scene_credits.ticks<scene_credits.finish_ticks)++scene_credits.ticks;
    if(scene_credits.notice_ticks)--scene_credits.notice_ticks;
    if(scene_live_save_pending || scene_live_load_pending){
        scene_live_save_pending=scene_live_load_pending=scene_live_load_death_recovery=0;
        scene_live_save_status=RF_NOT_FOUND;scene_credits.notice_ticks=180;
    }
    memset(campaign_audio_frame,0,sizeof(campaign_audio_frame));
    status=campaign_audio_events.music?
        campaign_audio_events.music(campaign_audio_events_context,&campaign_music_stream,campaign_music_revision):
        rf_music_mix(&campaign_music_stream,campaign_audio_frame,800);
    /* Match existing optional-music policy. Failed native borrowers remain
     * retained by that backend until the ordinary session audio close. */
    if(status){rf_scene_music[5]=(uint32_t)status;++rf_scene_music[2];rf_music_reset(&campaign_music_stream);}
    rf_scene_music[3]=campaign_music_stream.active;rf_scene_music[6]=campaign_music_stream.block_index;
    if(campaign_audio_sink)campaign_audio_sink(campaign_audio_context,campaign_audio_frame,800);
    if(campaign_audio_events.poll)campaign_audio_events.poll(campaign_audio_events_context);
    particle_draw_stream=stream;
    status=stream->sink(stream->context,frame,&empty,stream->materials,0);
    particle_draw_stream=NULL;
    return status;
}
