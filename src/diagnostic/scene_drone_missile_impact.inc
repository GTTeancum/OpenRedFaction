#ifndef RF_SCENE_DRONE_MISSILE_IMPACT_INC
#define RF_SCENE_DRONE_MISSILE_IMPACT_INC
/* Include after scene_fusion_effects_runtime.inc and campaign_drone_missile. */
static int scene_drone_missile_name_equal(const char *a,const char *b)
{
    unsigned char x,y;
    do{x=(unsigned char)*a++;y=(unsigned char)*b++;if(x>='A'&&x<='Z')x+=32;if(y>='A'&&y<='Z')y+=32;if(x!=y)return 0;}while(x);
    return 1;
}
/* Drone Missile authored impact (weapons.tbl): $Impact Vclips "big_charge_explode",
 * radius7. vclip.tbl gives code_explode with $Explosion Name "shoulder mounted
 * geomod", the explosion shoulder_cannon_explode also names, so both share this
 * owner. Its "explosion flare 2" (white, alpha255, glow, pradius1.3) and
 * "explosion main part 2" (Obj_EXP01, glow) scale by radius7; at the player's
 * position they fill the view white, then fire-coloured. Previously the shared
 * Rocket "rocket hit" recipe was scaled to radius7 instead. Requires
 * campaign_drone_missile loaded; leaves the owner alone when it already
 * holds the same explosion, refuses a different one. */
static uint32_t scene_drone_missile_impact_ready;
static int scene_drone_missile_impact_open(const char *tables_path,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp tables={0};rf_vclip_definition clip;int status;
    scene_drone_missile_impact_ready=0;
    if(campaign_drone_missile.impact_count!=1 || !(campaign_drone_missile.impact_radius[0]>0))return RF_FORMAT;
    if(!scene_fusion_effects) {
        status=scene_fusion_effects_open_clip(tables_path,maps,map_count,campaign_drone_missile.impact_vclips[0]);
        if(status)return status;
    } else {
        status=rf_vpp_open(&tables,tables_path);
        if(!status)status=rf_vclip_definition_load(&tables,campaign_drone_missile.impact_vclips[0],65536,&clip);
        rf_vpp_close(&tables);if(status)return status;
        if(!(clip.flags&8u) || !scene_drone_missile_name_equal(clip.explosion,scene_fusion_effects->explosion))return RF_FORMAT;
    }
    scene_drone_missile_impact_ready=1;return RF_OK;
}
/* Impact presentation for one Drone Missile terminal contact. Falls back to
 * the shared rocket recipe only when the authored owner could not be opened. */
static int scene_drone_missile_impact_start(scene_stream *s,const rf_weapon_flight_contact *hit,uint32_t frame)
{
    if(scene_drone_missile_impact_ready && scene_fusion_effects)
        return scene_fusion_impact_start(s,hit,frame,campaign_drone_missile.impact_radius[0]);
    return scene_impact_start_sized(s,hit,frame,campaign_drone_missile.impact_count?campaign_drone_missile.impact_radius[0]:1.5f);
}
#endif
