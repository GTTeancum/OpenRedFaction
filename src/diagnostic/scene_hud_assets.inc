/* Original asset composition; gameplay owners are read-only during this pass.
 * The renderer borrows atlas pixels until the complete HUD sink returns. */
uint32_t rf_hud_assets_diagnostic[8]; /* attempts,status,active,resident,peak,art frames,fallback frames,draw status */
static uint32_t scene_hud_integer(float value)
{return isfinite(value)&&value>0?(value>=99999?99999:(uint32_t)value):0;}
static uint32_t scene_hud_bucket(float value)
{return !isfinite(value)||value<=0?0:value>=10?10:(uint32_t)value;}
static int scene_hud_sprite_draw(rf_scene_particle_sink sink,void *context,const rf_hud_sprite *sprite,
    float x,float y,float scale,uint32_t color)
{
    rf_particle_draw_vertex v[4]={{0}};uint32_t i;
    if(!sprite->width||!sprite->height)return RF_OK;
    for(i=0;i<4;i++){
        uint32_t right=i==1||i==2,bottom=i>=2;
        v[i].screen[0]=x+(right?sprite->width*scale:0);
        v[i].screen[1]=y+(bottom?sprite->height*scale:0);
        v[i].reciprocal_w=1;v[i].argb=color;
        v[i].uv[0]=sprite->uv[right?2:0];v[i].uv[1]=sprite->uv[bottom?3:1];
    }
    return sink(context,v,4,sprite->image,RF_PARTICLE_NORMAL_MODE);
}
static int scene_hud_sprite_at(const rf_hud_assets *a,rf_scene_particle_sink sink,void *context,
    uint32_t id,const int32_t xy[2])
{
    rf_hud_sprite sprite;uint32_t tint=0xffffffffu;int status=rf_hud_assets_sprite(a,id,&sprite);if(status)return status;
    /* Original439f65 draws health/suit with white alpha120. The ammo
     * backplate/signal owner4375e0 supplies white alpha70 at full fade;
     *43a643 restores alpha255 for the actual ammunition icon. Keep source
     * texture alpha intact and multiply it by the appropriate draw opacity. */
    if(id<RF_HUD_RETICLE)tint=0x78ffffffu;
    else if(id>=RF_HUD_AMMO_BAR && id<=RF_HUD_AMMO_SIGNAL_GREEN)tint=0x46ffffffu;
    return scene_hud_sprite_draw(sink,context,&sprite,(float)xy[0],(float)xy[1],1,tint);
}
static int scene_hud_text_box(const rf_hud_assets *a,rf_scene_particle_sink sink,void *context,
    uint32_t font,const char *text,float x,float y,float width,float height,uint32_t color)
{
    uint32_t w,h,i;float scale=1,left;int status;
    status=rf_hud_assets_text_width(a,font,text,&w);if(status)return status;
    h=rf_hud_assets_font_height(a,font);if(!h)return RF_FORMAT;
    if(width>0&&w>width)scale=width/w;
    if(height>0&&h*scale>height)scale=height/h;
    if(width>0)x+=(width-w*scale)*.5f;
    if(height>0)y+=(height-h*scale)*.5f;
    left=x;if(font==RF_HUD_FONT_TEXT)++rf_hud_text_diagnostic[0];
    for(i=0;text[i];i++){
        if(text[i]=='\r')continue;
        if(text[i]=='\n'){x=left;y+=h*scale;continue;}
        rf_hud_glyph glyph;status=rf_hud_assets_glyph(a,font,(unsigned char)text[i],&glyph);if(status)return status;
        status=scene_hud_sprite_draw(sink,context,&glyph.sprite,x,y,scale,color);
        if(status){rf_hud_text_diagnostic[7]=(uint32_t)status;return status;}
        if(font==RF_HUD_FONT_TEXT&&glyph.sprite.width&&glyph.sprite.height)++rf_hud_text_diagnostic[1];
        x+=glyph.advance*scale;
    }
    return RF_OK;
}
static int scene_hud_number(const rf_hud_assets *a,rf_scene_particle_sink sink,void *context,
    uint32_t font,uint32_t value,const int32_t xy[2],const int32_t wh[2],uint32_t color)
{
    char text[16];snprintf(text,sizeof(text),"%u",value>99999?99999:value);
    /* Original439f58/43a6f8 center numeric text horizontally at the region Y.
     * SMALL/BIG bitmaps contain authored blank bottom rows; fitting their full
     * bitmap height into the region would shrink the visible digits. */
    return scene_hud_text_box(a,sink,context,font,text,(float)xy[0],(float)xy[1],(float)wh[0],0,color);
}
static uint32_t scene_hud_ammo_icon(void)
{
    /* Installed weapons.tbl ammo identities, not inventory numeric indices. */
    switch(campaign_equipped_slot){
    case 1:return RF_HUD_AMMO_556;case 2:return RF_HUD_AMMO_POWERCELL;
    case 3:return RF_HUD_AMMO_SHOTGUN;case 4:case 12:return RF_HUD_AMMO_ROCKET;
    case 6:return RF_HUD_AMMO_50CAL;case 7:return RF_HUD_AMMO_ALUMINUM;
    case 10:return RF_HUD_AMMO_GAS;
    case 13:return campaign_machine_mode.special?RF_HUD_AMMO_556:RF_HUD_AMMO_BULLET;
    default:return RF_HUD_AMMO_BULLET;
    }
}
static int scene_hud_original(rf_scene_particle_sink sink,void *context,uint32_t vehicle,
    float vehicle_health,const int32_t vehicle_ammo[2],uint32_t mounted,const scene_turret_owner *turret,
    uint32_t armed,uint32_t loaded,uint32_t reserve,uint32_t magazine,uint32_t reticle_color)
{
    const rf_hud_assets *a=particle_draw_stream->hud_assets;
    int32_t p[19][2];uint32_t i,color=rf_hud_assets_color(a,RF_HUD_COLOR_DEFAULT);int status;
    const uint32_t full=rf_hud_assets_color(a,RF_HUD_COLOR_FULL),mid=rf_hud_assets_color(a,RF_HUD_COLOR_MID);
    float health=vehicle?vehicle_health*100:campaign_player_damage.state.effects.health;
    float armor=vehicle?0:campaign_player_damage.state.effects.armor;
    float maximum=campaign_player_damage.state.effects.class_armor;
    for(i=0;i<19;i++){status=rf_hud_assets_position(a,i,p[i]);if(status)return status;}
    /* Original439fbb then43a009: health first, suit second. Transparent
     * source borders overlap, so preserve this order with alpha modulation. */
    status=scene_hud_sprite_at(a,sink,context,RF_HUD_HEALTH_0+scene_hud_bucket(health*.1f),p[0]);if(status)return status;
    if(!vehicle){status=scene_hud_sprite_at(a,sink,context,RF_HUD_ARMOR_0+scene_hud_bucket(maximum>0?armor/maximum*10:0),p[3]);if(status)return status;}
    /* Original43a011/43a094 choose HUD full/mid, including authored alpha;
     * these are separate health/armor colors, not the opaque default color. */
    status=scene_hud_number(a,sink,context,RF_HUD_FONT_SMALL,scene_hud_integer(health),p[1],p[2],full);if(status)return status;
    if(!vehicle){status=scene_hud_number(a,sink,context,RF_HUD_FONT_SMALL,scene_hud_integer(armor),p[4],p[5],mid);if(status)return status;}
    if(armed&&campaign_player_damage.state.effects.health>0&&
       !(scene_scope.active && particle_draw_stream->hud_scope &&
         (campaign_equipped_slot==6 || campaign_equipped_slot==15))&&
       (mounted||(vehicle&&rf_scene_vehicle_enabled!=1)||(!vehicle&&campaign_equipped_slot!=2&&
        campaign_equipped_slot!=5&&campaign_equipped_slot!=9&&campaign_equipped_slot!=11))){
        uint32_t id=vehicle&&rf_scene_vehicle_enabled==2?RF_HUD_RETICLE_APC:
            !vehicle&&!mounted&&campaign_equipped_slot==6&&!scene_scope.active?RF_HUD_RETICLE_SCOPE:
            !vehicle&&!mounted&&(campaign_equipped_slot==4||campaign_equipped_slot==12)?RF_HUD_RETICLE_ROCKET:RF_HUD_RETICLE;
        rf_hud_sprite sprite;status=rf_hud_assets_sprite(a,id,&sprite);if(status)return status;
        /* Original43a41b centers by viewport and selected bitmap dimensions. */
        status=scene_hud_sprite_draw(sink,context,&sprite,(640.f-sprite.width)*.5f,(480.f-sprite.height)*.5f,1,reticle_color);if(status)return status;
    }
    if(mounted&&turret){
        status=scene_hud_text_box(a,sink,context,RF_HUD_FONT_TEXT,turret->view.weapons[0]==scene_turret_heap_id?"HEAP":"VAUSS",518,12,108,18,color);if(status)return status;
        return scene_hud_text_box(a,sink,context,RF_HUD_FONT_TEXT,"UNLIMITED",518,34,108,18,color);
    }
    if(vehicle){
        char text[32];status=scene_hud_text_box(a,sink,context,RF_HUD_FONT_TEXT,scene_vehicle_hud_label(particle_draw_stream),24,160,140,18,color);if(status)return status;
        if(vehicle_ammo[0]>=0){snprintf(text,sizeof(text),"GUN %d",vehicle_ammo[0]);status=scene_hud_text_box(a,sink,context,RF_HUD_FONT_TEXT,text,482,12,144,18,color);if(status)return status;}
        if(vehicle_ammo[1]>=0){snprintf(text,sizeof(text),"ROCKET %d",vehicle_ammo[1]);status=scene_hud_text_box(a,sink,context,RF_HUD_FONT_TEXT,text,482,34,144,18,color);if(status)return status;}
        return RF_OK;
    }
    if(!armed||campaign_equipped_slot==11)return RF_OK;
    {
        uint32_t clipless=!magazine||campaign_equipped_slot==5||campaign_equipped_slot==8||campaign_equipped_slot==9;
        uint32_t bar=clipless?RF_HUD_AMMO_NOCLIP:campaign_equipped_slot==2||campaign_equipped_slot==10?RF_HUD_AMMO_POWER:RF_HUD_AMMO_BAR;
        uint32_t base=clipless?14:6;
        status=scene_hud_sprite_at(a,sink,context,bar,p[base]);if(status)return status;
        status=scene_hud_sprite_at(a,sink,context,loaded?RF_HUD_AMMO_SIGNAL_GREEN:RF_HUD_AMMO_SIGNAL_RED,p[base+1]);if(status)return status;
        if(campaign_equipped_slot!=5&&campaign_equipped_slot!=8&&campaign_equipped_slot!=9){status=scene_hud_sprite_at(a,sink,context,scene_hud_ammo_icon(),p[base+2]);if(status)return status;}
        if(clipless)status=scene_hud_number(a,sink,context,RF_HUD_FONT_BIG,loaded,p[17],p[18],mid);
        else {
            status=scene_hud_number(a,sink,context,RF_HUD_FONT_BIG,loaded,p[9],p[10],full);if(status)return status;
            status=scene_hud_number(a,sink,context,RF_HUD_FONT_BIG,campaign_equipped_slot==2?reserve/magazine:reserve,p[11],p[12],mid);
        }
        if(status)return status;
        if(rf_scene_combat[6]&&pistol_reload_ticks){
            uint32_t left=rf_scene_combat[6]>pistol_reload_ticks?pistol_reload_ticks:rf_scene_combat[6];
            status=combat_hud_rect(sink,context,518,64,108,3,0xff484848);if(status)return status;
            status=combat_hud_rect(sink,context,518,64,108.f*(pistol_reload_ticks-left)/pistol_reload_ticks,3,0xffffc040);if(status)return status;
        }
    }
    return RF_OK;
}
static void scene_hud_open_optional(scene_stream *stream,const char *tables_path,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp ui={0},tables={0};char path[1024];uint32_t i,prefix=0;int status;
    memset(rf_hud_assets_diagnostic,0,sizeof(rf_hud_assets_diagnostic));
    memset(rf_hud_text_diagnostic,0,sizeof(rf_hud_text_diagnostic));
    if(!campaign_spawn)return;
    ++rf_hud_assets_diagnostic[0];
    for(i=0;i<sizeof(path)&&tables_path[i];i++)if(tables_path[i]=='/'||tables_path[i]=='\\')prefix=i+1;
    status=i>=sizeof(path)||prefix+sizeof("ui.vpp")>sizeof(path)?RF_RANGE:RF_OK;
    if(!status){memcpy(path,tables_path,prefix);memcpy(path+prefix,"ui.vpp",sizeof("ui.vpp"));status=rf_vpp_open(&ui,path);}
    if(!status)status=rf_vpp_open(&tables,tables_path);
    if(!status)status=rf_hud_assets_open(&stream->hud_assets,&ui,maps,map_count,&tables,RF_HUD_ASSETS_BUDGET);
    rf_vpp_close(&tables);rf_vpp_close(&ui);
    rf_hud_assets_diagnostic[1]=(uint32_t)status;rf_hud_assets_diagnostic[2]=stream->hud_assets!=NULL;
    rf_hud_assets_diagnostic[3]=rf_hud_assets_resident_bytes(stream->hud_assets);
    rf_hud_assets_diagnostic[4]=rf_hud_assets_peak_bytes(stream->hud_assets);
    printf("HUD_ASSETS %d %u %u\n",status,rf_hud_assets_diagnostic[3],rf_hud_assets_diagnostic[4]);
}
