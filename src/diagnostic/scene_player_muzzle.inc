#ifndef RF_SCENE_PLAYER_MUZZLE_INC
#define RF_SCENE_PLAYER_MUZZLE_INC
/* Player barrel point. RF.exe 425dab..425dc7 starts the player's rounds at the
 * first-person model's Muzzle_1 tag (player+40); weapons.tbl from_eye weapons
 * keep the eye for gameplay. The view model draws in the eye frame with its
 * own FOV (70, scene_player_weapon_view_prepare), so the tag is re-projected:
 * the world point keeps the tag's depth and lies on the world ray through the
 * pixel where the barrel is drawn. Port choice: the original places the tag
 * without that FOV correction; corrected, tracers and rockets visibly leave the
 * drawn barrel. Updated after each view-model pose; consumed by the next shot
 * in the current eye basis, so the point follows the player's movement. */
static float scene_player_muzzle_eye[3];static uint32_t scene_player_muzzle_slot=UINT32_MAX;
/* Eye basis of the shot being fired (rows right, up, forward). */
static float scene_fire_basis[3][3];
uint32_t rf_scene_player_muzzle[4]; /* updates, shots from the barrel, eye fallbacks, slot */
float rf_scene_player_muzzle_point[8]; /* eye-frame barrel point; bind-space variant; barrel and digit-1 view-model pixels */
static void scene_player_muzzle_update(const scene_stream *s,const rf_player_weapon *w,const rf_model_projection *view,uint32_t slot)
{
    float local[12],pose[12],e[3],v[3];const rf_visibility_view *world=&s->particle_camera.view;uint32_t n;
    scene_player_muzzle_slot=UINT32_MAX;
    if(!w || !w->has_muzzle || w->muzzle_tag.parent<0 || (uint32_t)w->muzzle_tag.parent>=w->bone_count)return;
    if(rf_model_attachment_transform(w->muzzle_tag.rotation,w->muzzle_tag.position,local))return;
    if(rf_model_compose_transform(local,w->pose[w->muzzle_tag.parent],pose))return;
    for(n=0;n<3;n++)e[n]=pose[9+n]-view->camera[n];
    for(n=0;n<3;n++)v[n]=view->rotation[n*3]*e[0]+view->rotation[n*3+1]*e[1]+view->rotation[n*3+2]*e[2];
    if(!(v[2]>.01f) || !(world->scale[0]>0) || !(world->scale[1]>0) || !(world->scale[2]>0))return;
    /* Inverse of the world projection (as debris_visibility.c): same pixel offset. */
    scene_player_muzzle_eye[0]=v[0]/v[2]*(world->scale[2]/world->scale[0])*e[2];
    scene_player_muzzle_eye[1]=v[1]/v[2]*(world->scale[2]/world->scale[1])*e[2];
    scene_player_muzzle_eye[2]=e[2];
    if(!isfinite(scene_player_muzzle_eye[0]) || !isfinite(scene_player_muzzle_eye[1]) || !isfinite(e[2]))return;
    scene_player_muzzle_slot=slot;++rf_scene_player_muzzle[0];rf_scene_player_muzzle[3]=slot;
    memcpy(rf_scene_player_muzzle_point,scene_player_muzzle_eye,12);
    rf_scene_player_muzzle_point[3]=view->screen[2]+view->screen[0]*v[0]/v[2];
    rf_scene_player_muzzle_point[4]=view->screen[3]+view->screen[1]*v[1]/v[2];
    if(w->ammo_digits && !rf_model_attachment_transform(w->digit_tags[0].rotation,w->digit_tags[0].position,local) &&
       !rf_model_compose_transform(local,w->pose[w->digit_tags[0].parent],pose)) {
        for(n=0;n<3;n++)e[n]=pose[9+n]-view->camera[n];
        for(n=0;n<3;n++)v[n]=view->rotation[n*3]*e[0]+view->rotation[n*3+1]*e[1]+view->rotation[n*3+2]*e[2];
        if(v[2]>.01f){rf_scene_player_muzzle_point[5]=view->screen[2]+view->screen[0]*v[0]/v[2];rf_scene_player_muzzle_point[6]=view->screen[3]+view->screen[1]*v[1]/v[2];}
    }
    rf_scene_player_muzzle_point[7]=(float)w->current;
}
/* World barrel point for the current shot; 0 (eye kept) without a drawn tag. */
static uint32_t scene_player_muzzle_world(const float eye[3],float out[3])
{
    uint32_t k;
    if(scene_player_muzzle_slot!=campaign_view_slot()){memcpy(out,eye,12);++rf_scene_player_muzzle[2];return 0;}
    for(k=0;k<3;k++)out[k]=eye[k]+scene_fire_basis[0][k]*scene_player_muzzle_eye[0]+
        scene_fire_basis[1][k]*scene_player_muzzle_eye[1]+scene_fire_basis[2][k]*scene_player_muzzle_eye[2];
    ++rf_scene_player_muzzle[1];return 1;
}
/* Where the eye ray lands within range: first world contact, else its end. */
static void scene_player_aim_point(scene_stream *s,const float eye[3],const float path[3],float out[3])
{
    rf_collision_solid_hit hit;uint32_t matched=0,k;
    for(k=0;k<3;k++)out[k]=eye[k]+path[k];
    if(s->collision && !rf_geometry_collision_ray(s->collision,&campaign_movers,eye,out,0x26,&hit,&matched) && matched)memcpy(out,hit.hit.point,12);
}
#endif
