#ifndef RF_SCENE_RAIL_SCOPE_INC
#define RF_SCENE_RAIL_SCOPE_INC
/* Rail Driver scanner picture. Reconstructed from RF.exe:
 *   4adc60 (called from 431870 while the scanner is on) renders a 64x64 view
 *          from the player eye, black clear, horizontal half-angle
 *          atan(tan(15 deg) * (viewport height / 2) / viewport width). It
 *          visits the actor list (5cb2ec) and the corpse list (5cae44); an
 *          object that is not hidden (40a110), has object material +1fc == 3
 *          and passes the view sphere test (5186a0) is drawn as its model in
 *          one flat colour when its squared distance d2 from the eye is below
 *          900 (5a0b90/5a0b94): actors (255*(1-d2/900), 0, 0), corpses
 *          (127*(1-d2/900), 0, same). 128BlackCircle.tga (32x32, alpha) is
 *          then drawn over the four 32x32 quadrants, mirrored in x and/or y,
 *          rounding the picture off, and the frame becomes player bitmap +1088.
 *   4324ec..432603 HUD: with the scanner on (player+fb0), that bitmap is
 *          drawn white at full intensity as a centred square whose side is
 *          half the viewport height (single player).
 * Not reconstructed yet: the crosshair lines and railgun_text.vbm drawn after
 * it (43260a..432856); the existing "SCANNER" label stands in. Visibility is
 * the eye-forward test, not 5186a0's frustum sphere; the rasterizer clips. */
static struct {rf_image picture,circle;uint32_t ready;} scene_rail_scope;
/* renders, actors drawn, corpses drawn, lit pixels, errors, last error, HUD draws, circle loaded */
uint32_t rf_scene_rail_scope[8];
static int scene_rail_scope_open(rf_vpp *maps,uint32_t map_count)
{
    rf_image next={SCENE_IR_SIZE,SCENE_IR_SIZE,SCENE_IR_SIZE*SCENE_IR_SIZE*4,7,NULL,0,0,0};uint32_t i,x,y,black=0xff000000u;int status;
    memset(rf_scene_rail_scope,0,sizeof(rf_scene_rail_scope));
    if(scene_rail_scope.ready)return RF_OK;
    status=rf_image_allocate_pixels(&next);if(status)return status;
    for(y=0;y<SCENE_IR_SIZE;y++)for(x=0;x<SCENE_IR_SIZE;x++)memcpy(rf_image_pixel(&next,x,y),&black,4);
    scene_rail_scope.picture=next;scene_rail_scope.ready=1;
    for(i=0;i<map_count;i++){rf_vpp_entry entry;
        if(rf_vpp_find(maps+i,"128BlackCircle.tga",&entry)!=RF_OK)continue;
        status=rf_image_open(&scene_rail_scope.circle,maps+i,&entry,32*32*4+64);
        if(!status)rf_scene_rail_scope[7]=1;
        break;}
    return RF_OK;
}
static void scene_rail_scope_close(void)
{
    rf_image_close(&scene_rail_scope.picture);rf_image_close(&scene_rail_scope.circle);
    memset(&scene_rail_scope,0,sizeof(scene_rail_scope));
}
static int scene_rail_scope_update(scene_stream *s,const float eye[3],const float basis[3][3])
{
    uint32_t i,x,y,live=0,dead=0;int status=RF_OK;
    rf_model_render_buffers buffers={0};rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};rf_model_projection camera;
    /* 640x480 viewport: tan(15 deg) * 240 / 640. */
    const float factor=1.f/(tanf(15.f*.017453292519943295f)*(240.f/640.f));
    if(!scene_scanner_enabled || !scene_rail_scope.ready || !s->npc_memory || !campaign_poses.count)return RF_OK;
    scene_ir_height=SCENE_IR_SIZE;
    for(i=0;i<SCENE_IR_SIZE*SCENE_IR_SIZE;i++){scene_ir_color[i]=0xff000000u;scene_ir_depth[i]=0;}
    camera=s->npc_view;memcpy(camera.camera,eye,12);
    for(i=0;i<3;i++){camera.rotation[i]=basis[0][i]*factor;camera.rotation[3+i]=basis[1][i]*factor;camera.rotation[6+i]=basis[2][i];}
    camera.screen[0]=320;camera.screen[1]=-240;camera.screen[2]=320;camera.screen[3]=240;
    camera.perspective=camera.compute_clip=camera.clipping=camera.far_clip=1;camera.far_depth=1000;camera.screen_clip=0;
    buffers.cache=s->npc_memory;buffers.clip=(float(*)[3])((uint8_t *)s->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t *)s->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t *)s->npc_memory+4096*56);buffers.capacity=4096;
    planes.near_depth=.1f;planes.far_depth=1000;
    projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=lights.ambient[1]=lights.ambient[2]=255;
    for(i=0;i<campaign_poses.count && i<campaign_npc_body_count;i++){
        const campaign_model_owner *owner=campaign_model_owners+i;const campaign_npc_body *body=campaign_npc_bodies+i;
        rf_entity_pose *pose;const rf_entity_render_model *model;rf_model_projection view;
        float prepared[50][12],delta[3],d2=0,forward=0,temperature;uint16_t generations[50];uint32_t k,batch,corpse,level;
        if(body->object_flags&(2u|0x4000u) || body->collision_material!=3)continue;
        if(body->persistence_registered && rf_scene_defeated_actors.items[body->persistence_slot].retired)continue;
        corpse=!(body->damage.effects.health>0);
        if(corpse && !scene_ir_body_heat(i,&temperature))continue; /* No corpse object. */
        for(k=0;k<3;k++){delta[k]=owner->position[k]-eye[k];d2+=delta[k]*delta[k];forward+=delta[k]*basis[2][k];}
        if(!(d2<900.f) || !(forward>0))continue;
        level=(uint32_t)((1.f-d2*(1.f/900.f))*(corpse?127.f:255.f));
        status=campaign_model_pose(i,&pose);if(status)break;if(!pose)continue;
        if(pose->skeleton>=campaign_render_models.count){status=RF_FORMAT;break;}
        model=campaign_render_models.items+pose->skeleton;
        memset(prepared,0,sizeof(prepared));memset(generations,0,sizeof(generations));
        status=rf_model_prepare_skinning(model->stored,pose->matrices,pose->bone_count,
            (uint16_t)pose->playback.generation,prepared,generations,50);if(status)break;
        status=rf_model_local_view(&camera,owner->position,owner->basis,&view);if(status)break;
        scene_ir_argb=0xff000000u|(level<<16)|(corpse?level:0);
        if(corpse)++dead;else ++live;
        {const rf_model_geometry *g=model->lods;
         for(batch=0;batch<g->batch_count && !status;batch++){
            const rf_model_draw_batch *draw=g->batches+batch;
            if(draw->material==UINT32_MAX)continue;
            status=scene_model_scratch_prepare(&buffers,draw->vertices);
            if(!status)status=rf_model_geometry_render_batch(g,batch,prepared,pose->bone_count,&view,&lights,&attributes,&buffers);
            if(!status)status=rf_preview_visit_model(g,batch,&buffers,s->npc_indices,s->npc_pool,&view,&planes,&projection,&attributes,NULL,scene_ir_triangle,NULL);
         }}
        if(status)break;
    }
    if(status){++rf_scene_rail_scope[4];rf_scene_rail_scope[5]=(uint32_t)status;
        for(i=0;i<SCENE_IR_SIZE*SCENE_IR_SIZE;i++)scene_ir_color[i]=0xff000000u;}
    /* 128BlackCircle.tga over each 32x32 quadrant, mirrored away from the centre. */
    if(scene_rail_scope.circle.rgba && scene_rail_scope.circle.width && scene_rail_scope.circle.height)
        for(y=0;y<SCENE_IR_SIZE;y++)for(x=0;x<SCENE_IR_SIZE;x++){
            uint32_t qx=x<32?x:63-x,qy=y<32?y:63-y,c=scene_ir_color[y*SCENE_IR_SIZE+x],out=0xff000000u,k;
            const unsigned char *m=rf_image_pixel(&scene_rail_scope.circle,qx*scene_rail_scope.circle.width/32,qy*scene_rail_scope.circle.height/32);
            for(k=0;k<3;k++)out|=((((c>>(16-8*k))&255)*(255u-m[3]))/255u)<<(16-8*k);
            scene_ir_color[y*SCENE_IR_SIZE+x]=out;
        }
    {uint32_t lit=0;
     for(y=0;y<SCENE_IR_SIZE;y++)for(x=0;x<SCENE_IR_SIZE;x++){
        uint32_t c=scene_ir_color[y*SCENE_IR_SIZE+x];unsigned char p[4];
        p[0]=(unsigned char)((c>>16)&255);p[1]=(unsigned char)((c>>8)&255);p[2]=(unsigned char)(c&255);p[3]=255;
        lit+=(c&0xffffffu)!=0;memcpy(rf_image_pixel(&scene_rail_scope.picture,x,y),p,4);
     }
     rf_scene_rail_scope[3]=lit;}
    rf_scene_rail_scope[1]=live;rf_scene_rail_scope[2]=dead;++rf_scene_rail_scope[0];
    return RF_OK;
}
/* 4324ec..432603: centred square, side half the 480-line HUD, white. */
static int scene_rail_scope_draw(rf_scene_particle_sink sink,void *context)
{
    rf_particle_draw_vertex v[4]={{0}};uint32_t i;
    if(!scene_rail_scope.ready || !rf_scene_rail_scope[0])return RF_OK;
    for(i=0;i<4;i++){
        uint32_t right=i==1||i==2,bottom=i>=2;
        v[i].screen[0]=right?440.f:200.f;v[i].screen[1]=bottom?360.f:120.f;
        v[i].reciprocal_w=1;v[i].argb=0xffffffffu;v[i].uv[0]=(float)right;v[i].uv[1]=(float)bottom;
    }
    ++rf_scene_rail_scope[6];
    return sink(context,v,4,&scene_rail_scope.picture,RF_PARTICLE_NORMAL_MODE);
}
#endif
