/* Source-derived optional scope mask; no gameplay/zoom state is mutated.
 * Include after scene_hud_assets.inc and before rf_scene_draw_combat_hud. */
uint32_t rf_hud_scope_diagnostic[8]; /* attempts,status,active,resident,peak,draw frames,draw status,kind */
static void scene_hud_scope_open_optional(scene_stream *stream,rf_vpp *maps,uint32_t map_count)
{
    int status;memset(rf_hud_scope_diagnostic,0,sizeof(rf_hud_scope_diagnostic));
    if(!campaign_spawn || !stream->hud_assets)return;
    ++rf_hud_scope_diagnostic[0];
    status=rf_hud_scope_assets_open(&stream->hud_scope,maps,map_count,RF_HUD_SCOPE_BUDGET);
    rf_hud_scope_diagnostic[1]=(uint32_t)status;rf_hud_scope_diagnostic[2]=stream->hud_scope!=NULL;
    rf_hud_scope_diagnostic[3]=rf_hud_scope_resident_bytes(stream->hud_scope);
    rf_hud_scope_diagnostic[4]=rf_hud_scope_peak_bytes(stream->hud_scope);
}
static int scene_hud_scope_draw(rf_scene_particle_sink sink,void *context,uint32_t enabled)
{
    rf_hud_scope_quad quads[RF_HUD_SCOPE_QUADS];uint32_t count,i,j,kind;int status;
    if(!enabled || !particle_draw_stream || !particle_draw_stream->hud_scope ||
       !scene_scope.active || campaign_cutscene_runtime.active || campaign_player_damage.state.effects.health<=0 ||
       (campaign_equipped_slot!=6 && campaign_equipped_slot!=15))return RF_OK;
    kind=campaign_equipped_slot==6?RF_HUD_SCOPE_SNIPER:RF_HUD_SCOPE_ASSAULT;
    status=rf_hud_scope_compose(particle_draw_stream->hud_scope,kind,640,480,quads,RF_HUD_SCOPE_QUADS,&count);
    if(status){rf_hud_scope_diagnostic[6]=(uint32_t)status;return status;}
    for(i=0;i<count;i++){
        const rf_hud_scope_quad *q=quads+i;rf_particle_draw_vertex v[4]={{0}};
        if(q->xywh[2]<=0 || q->xywh[3]<=0)continue;
        for(j=0;j<4;j++){
            uint32_t right=j==1||j==2,bottom=j>=2;
            v[j].screen[0]=q->xywh[0]+(right?q->xywh[2]:0);v[j].screen[1]=q->xywh[1]+(bottom?q->xywh[3]:0);
            v[j].uv[0]=q->uv[right?2:0];v[j].uv[1]=q->uv[bottom?3:1];v[j].reciprocal_w=1;v[j].argb=q->argb;
        }
        status=sink(context,v,4,q->image,q->image?RF_PARTICLE_NORMAL_MODE:0x18000u);
        if(status){rf_hud_scope_diagnostic[6]=(uint32_t)status;return status;}
    }
    ++rf_hud_scope_diagnostic[5];rf_hud_scope_diagnostic[6]=0;rf_hud_scope_diagnostic[7]=kind;return RF_OK;
}
