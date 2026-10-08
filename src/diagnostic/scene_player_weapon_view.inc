/* Normal single-player first-person placement.
 * RF.exe4aa762..4aa7a6 adds the weapon's authored descriptor+4c offset
 * through the eye basis to the eye position. With model and view expressed
 * in that same eye frame, this is model - (-offset), without another world
 * eye translation or a fitted per-slot adjustment.
 * RF.exe4ab3f0..4ab411 supplies descriptor+74 FOV to517eb0/547150.
 */
static int scene_player_weapon_view_prepare(const rf_player_weapon *weapon,
    rf_model_projection *view,rf_model_clip_projection *clip_projection)
{
    rf_visibility_viewport viewport={640,480,0,0,1,90,1000,1};
    rf_visibility_view_scale scale;rf_model_projection value={0};
    rf_model_clip_projection clip={0};uint32_t i;int status;
    if(!weapon || !view || !clip_projection)return RF_RANGE;
    for(i=0;i<3;i++) {
        if(!isfinite(weapon->position[i]))return RF_RANGE;
        value.camera[i]=-weapon->position[i];
    }
    viewport.fov=weapon->fov;
    status=rf_visibility_view_scale_build(&viewport,&scale);if(status)return status;
    /* Keep FOV/aspect in clip space, as547150 does. model_clip_mask clips
     * against +/-z and generated vertices use viewport half sizes, so moving
     * FOV into screen[] alone gives edge/near-clipped faces a different view.
     * Normalize the common depth scale to preserve the existing model-unit
     * near plane and independent first-person depth band.
     */
    value.rotation[0]=scale.scale[0]/scale.scale[2];
    value.rotation[4]=scale.scale[1]/scale.scale[2];value.rotation[8]=1;
    value.screen[0]=scale.half[0];value.screen[1]=-scale.half[1];
    value.screen[2]=scale.center[0];value.screen[3]=scale.center[1];
    value.perspective=value.compute_clip=value.clipping=1;
    clip.scale[0]=scale.half[0];clip.scale[1]=scale.half[1];clip.clamp=1;
    *view=value;*clip_projection=clip;return RF_OK;
}
