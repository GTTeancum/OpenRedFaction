#ifndef RF_SCENE_WEAPON_AMMO_DIGITS_INC
#define RF_SCENE_WEAPON_AMMO_DIGITS_INC
/* Assault rifle clip readout on the first-person model. Reconstructed from RF.exe:
 *   4a9ab1 loads assault_digits.vbm once (installed file: 10 frames, 32x32,
 *          frame n shows digit n).
 *   4aacf8 looks up the ammo_digit_1/ammo_digit_2 tags on the view model
 *          (player+48/+4c); only fp_aslt_rfl has them.
 *   4abbd0..4abcf8, when both tags exist and the bank has at least ten frames:
 *          colour (255,0,0,255) (50cf80); value = clip ammo of the current
 *          weapon (entity+32c[weapon]); ones digit at ammo_digit_2, tens at
 *          ammo_digit_1, each placed through the current pose (5034f0) and
 *          drawn by 515b10/555790 as a quad in the tag's right/up plane with
 *          half-extent 0.013 (square bitmap). assault_digits.vbm is opaque
 *          565, so each digit sits on its own black tile.
 * Port policy: the stream renderer forces vertex colour to white on textured
 * faces, so the red modulate is baked into the ten frame images at load. */
static struct {uint32_t base,ready;} scene_digits;
/* frames drawn, last value, quads emitted, last error */
uint32_t rf_scene_weapon_digits[4];
/* Load time: append the ten tinted digit frames to the stream materials when
 * any loaded view model carries both tags. */
static int scene_ammo_digits_merge(scene_stream *s,rf_materials *materials,rf_vpp *maps,uint32_t map_count)
{
    rf_material *combined;rf_vpp_entry entry;uint32_t i,frame,count=0,needed=0,x,y;int status;
    scene_digits.ready=0;
    for(i=0;i<scene_weapon_resource_slots();i++)
        if(scene_weapon_available(i) && s->player_weapon[i] && s->player_weapon[i]->ammo_digits)needed=1;
    if(!needed)return RF_OK;
    for(i=0;i<map_count;i++)if(rf_vpp_find(maps+i,"assault_digits.vbm",&entry)==RF_OK)break;
    if(i==map_count)return RF_OK; /* Original: missing bank draws nothing. */
    if((uint64_t)materials->count+10>RF_CAMPAIGN_TEXTURE_SLOTS)return RF_RANGE;
    combined=realloc(materials->items,(materials->count+10)*sizeof(*combined));if(!combined)return RF_IO;
    materials->items=combined;memset(combined+materials->count,0,10*sizeof(*combined));
    for(frame=0;frame<10;frame++) {
        rf_image *image=&combined[materials->count+frame].image;
        status=rf_image_vbm_frame(image,maps+i,&entry,frame,64*1024,&count,NULL);
        if(status || rf_image_is_packed_1555(image)) {
            while(frame--)rf_image_close(&combined[materials->count+frame].image);
            rf_image_close(image);return status?status:RF_FORMAT;
        }
        for(y=0;y<image->height;y++)for(x=0;x<image->width;x++){unsigned char *p=rf_image_pixel(image,x,y);p[1]=p[2]=0;}
        materials->allocated_bytes+=image->bytes;
    }
    scene_digits.base=materials->count;materials->count+=10;materials->loaded+=10;
    scene_digits.ready=count>=10;return RF_OK;
}
/* After the view model: two quads in the first-person depth band. */
static int scene_ammo_digits_draw(scene_stream *s,const rf_player_weapon *w,const rf_model_projection *view)
{
    /* 555790 corner order: u runs from the tag's -right edge (u=1) to +right (u=0). */
    static const float corner[4][4]={{-1,1,1,0},{1,1,0,0},{1,-1,0,1},{-1,-1,1,1}};
    static const uint32_t order[6]={0,1,2,0,2,3};
    int32_t loaded;uint32_t value,j,k,n;int status;
    if(!scene_digits.ready || !w->ammo_digits)return RF_OK;
    if((uint64_t)(s->mesh->count+12)*sizeof(rf_preview_vertex)>s->capacity)return RF_RANGE;
    loaded=campaign_player_inventory.loaded[campaign_slot_weapon(campaign_equipped_slot)];
    value=loaded>0?(uint32_t)loaded:0;
    for(j=0;j<2;j++) {
        const rf_model_attachment *tag=w->digit_tags+j;float local[12],pose[12];rf_preview_vertex quad[4];
        uint32_t digit=j?value%10:value/10%10;
        status=rf_model_attachment_transform(tag->rotation,tag->position,local);if(status)return status;
        status=rf_model_compose_transform(local,w->pose[tag->parent],pose);if(status)return status;
        for(k=0;k<4;k++) {
            float p[3],v[3],q;
            for(n=0;n<3;n++)p[n]=pose[9+n]+.013f*(corner[k][0]*pose[n]+corner[k][1]*pose[3+n])-view->camera[n];
            for(n=0;n<3;n++)v[n]=view->rotation[n*3]*p[0]+view->rotation[n*3+1]*p[1]+view->rotation[n*3+2]*p[2];
            if(!(v[2]>.01f))return RF_OK; /* Behind the view-model near plane: skip both. */
            q=1.f/v[2];memset(quad+k,0,sizeof(quad[k]));
            quad[k].position[0]=floorf((view->screen[2]+view->screen[0]*v[0]*q)*16)/16;
            quad[k].position[1]=floorf((view->screen[3]+view->screen[1]*v[1]*q)*16)/16;
            quad[k].position[2]=16384.0f/(1.0f+q);
            quad[k].color[0]=quad[k].color[1]=quad[k].color[2]=1;
            quad[k].texture[0]=corner[k][2]*q;quad[k].texture[1]=corner[k][3]*q;quad[k].texture[2]=q;
            quad[k].lightmap_texture[2]=q;quad[k].lightmap=UINT32_MAX;quad[k].material=scene_digits.base+digit;
        }
        for(k=0;k<6;k++)s->mesh->vertices[s->mesh->count++]=quad[order[k]];
        rf_scene_weapon_digits[2]+=2;
    }
    s->mesh->bytes=s->mesh->count*sizeof(rf_preview_vertex);
    ++rf_scene_weapon_digits[0];rf_scene_weapon_digits[1]=value;return RF_OK;
}
#endif
