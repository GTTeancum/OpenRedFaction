#ifndef RF_SCENE_WEAPON_IR_SCREEN_INC
#define RF_SCENE_WEAPON_IR_SCREEN_INC
/* Infrared weapon screen (Rocket Launcher, shoulder_cannon: weapons.tbl flag
 * "infrared", 0x800). Reconstructed from RF.exe:
 *   4aeef0 per-frame update: when the held weapon has the flag, every second
 *          update (5a1020) render a 64x48 view (64x64 for the shoulder cannon,
 *          weapon 87244c) from the weapon camera, 45 degree FOV, black clear.
 *          Each live entity within 13 units (after subtracting its radius,
 *          0.001 when inside) and in front (normalized dot >= 0.17) is drawn
 *          as its model in one flat colour from heat
 *          h = (1 - d/13) * class "$Body Temperature(F)" (class+13e4); corpses
 *          use their current cooling temperature (corpse+2b0).
 *          Colour ramp (cool 7cab7c = 75,0,200, hot 7cab80 = 255,16,16):
 *          h<=20: cool*h*0.05; h<=100: cool+(hot-cool)*(h-20)*0.0125;
 *          h>100: hot+(255-hot)*(h-100)*0.0333333; each channel truncated.
 *          Anything drawn: interlace64x64.tga (4aee90) over it, then the
 *          frame becomes the screen; nothing drawn: black, once (1084).
 *   4af8a0 binds the screen bitmap to the weapon model's "screen.tga"
 *          materials and sets their material value to 0.7.
 * Port policy: the camera is the player eye (4aeef0's no-tag branch), not
 * the weapon model tag; drawing is a CPU raster into the material image the
 * renderer already reads; the 0.7 value is applied as brightness, as the
 * monitor screens do. Include after scene_monitor_render.inc. */
#define SCENE_IR_SLOTS 32u
enum {SCENE_IR_SIZE=64};
static struct {
    uint32_t material[SCENE_IR_SLOTS]; /* stream material index + 1, 0 = none */
    uint32_t weapon,counter,standby;
    rf_image interlace;
} scene_ir;
static uint32_t scene_ir_color[SCENE_IR_SIZE*SCENE_IR_SIZE];
static float scene_ir_depth[SCENE_IR_SIZE*SCENE_IR_SIZE];
static uint32_t scene_ir_height,scene_ir_argb;
/* frames rendered, entities drawn last frame, triangles, standby frames, errors, last error */
uint32_t rf_scene_weapon_ir[6];
/* Load time: give an infrared weapon's "screen.tga" texture slot its own
 * 64x64 RGBA image (the placeholder file is 4x4). Called on the weapon's
 * texture set just before it joins the stream materials at `base`. */
static int scene_ir_screen_prepare(uint32_t slot,rf_player_weapon *weapon,uint32_t base,rf_vpp *maps,uint32_t map_count)
{
    uint32_t i;int status;
    if(slot>=SCENE_IR_SLOTS)return RF_OK;
    scene_ir.material[slot]=0;
    if(!weapon || !weapon->infrared)return RF_OK;
    for(i=0;i<weapon->materials.count;i++) {
        const unsigned char *record=weapon->materials.items[i].record.bytes;uint32_t texture;
        if(!scene_monitor_screen_name(record))continue;
        memcpy(&texture,record+0x10,4);
        if(texture>=weapon->materials.textures.count)return RF_FORMAT;
        if(!scene_ir.material[slot]) {
            rf_image *image=&weapon->materials.textures.items[texture].image;
            rf_image next={SCENE_IR_SIZE,SCENE_IR_SIZE,SCENE_IR_SIZE*SCENE_IR_SIZE*4,7,NULL,0,0,0};uint32_t x,y,black=0xff000000u;
            status=rf_image_allocate_pixels(&next);if(status)return status;
            for(y=0;y<SCENE_IR_SIZE;y++)for(x=0;x<SCENE_IR_SIZE;x++)memcpy(rf_image_pixel(&next,x,y),&black,4);
            weapon->materials.textures.allocated_bytes+=next.bytes;
            weapon->materials.textures.allocated_bytes-=image->bytes<weapon->materials.textures.allocated_bytes?image->bytes:0;
            rf_image_close(image);*image=next;
            scene_ir.material[slot]=base+texture+1;
        }
    }
    if(scene_ir.material[slot] && !scene_ir.interlace.rgba) {
        for(i=0;i<map_count;i++){rf_vpp_entry entry;
            if(rf_vpp_find(maps+i,"interlace64x64.tga",&entry)!=RF_OK)continue;
            status=rf_image_open(&scene_ir.interlace,maps+i,&entry,64*64*4+64);if(status)return status;break;}
    }
    scene_ir.counter=2;scene_ir.weapon=UINT32_MAX;scene_ir.standby=0;
    return RF_OK;
}
static int scene_ir_triangle(void *context,const rf_preview_vertex tri[3])
{
    float x[3],y[3],area,sx=SCENE_IR_SIZE/640.f,sy=(float)scene_ir_height/480.f;
    int left,right,top,bottom,px,py;uint32_t i;(void)context;
    for(i=0;i<3;i++){
        x[i]=tri[i].position[0]*sx;y[i]=tri[i].position[1]*sy;
        if(!isfinite(x[i]) || !isfinite(y[i]) || !isfinite(tri[i].texture[2]))return RF_FORMAT;
    }
    area=(x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]);
    if(fabsf(area)<.000001f)return RF_OK;
    left=(int)fmaxf(0,floorf(fminf(x[0],fminf(x[1],x[2]))));
    right=(int)fminf(SCENE_IR_SIZE-1,ceilf(fmaxf(x[0],fmaxf(x[1],x[2]))));
    top=(int)fmaxf(0,floorf(fminf(y[0],fminf(y[1],y[2]))));
    bottom=(int)fminf((float)scene_ir_height-1,ceilf(fmaxf(y[0],fmaxf(y[1],y[2]))));
    ++rf_scene_weapon_ir[2];
    for(py=top;py<=bottom;py++)for(px=left;px<=right;px++){
        float cx=px+.5f,cy=py+.5f,q;
        float a=((x[1]-cx)*(y[2]-cy)-(y[1]-cy)*(x[2]-cx))/area;
        float b=((x[2]-cx)*(y[0]-cy)-(y[2]-cy)*(x[0]-cx))/area,c=1-a-b;
        uint32_t index=(uint32_t)py*SCENE_IR_SIZE+(uint32_t)px;
        if(a<-.00001f || b<-.00001f || c<-.00001f)continue;
        q=a*tri[0].texture[2]+b*tri[1].texture[2]+c*tri[2].texture[2];
        if(q<=0 || q<=scene_ir_depth[index])continue;
        scene_ir_color[index]=scene_ir_argb;scene_ir_depth[index]=q;
    }
    return RF_OK;
}
/* 4aeef0 colour ramp; channels as bytes r,g,b in 0xAARRGGBB order here. */
static uint32_t scene_ir_heat_color(float h)
{
    static const int cool[3]={75,0,200},hot[3]={255,16,16};int c[3],k;
    for(k=0;k<3;k++){
        if(h>100)c[k]=hot[k]+(int)((float)(255-hot[k])*((h-100.f)*.0333333351f));
        else if(h>20)c[k]=cool[k]+(int)((float)(hot[k]-cool[k])*((h-20.f)*.0125f));
        else c[k]=(int)((float)cool[k]*(h*.05f));
        if(c[k]<0)c[k]=0;if(c[k]>255)c[k]=255;
    }
    return 0xff000000u|((uint32_t)c[0]<<16)|((uint32_t)c[1]<<8)|(uint32_t)c[2];
}
static int scene_ir_body_heat(uint32_t i,float *temperature)
{
    const campaign_npc_body *body=campaign_npc_bodies+i;uint32_t slot,uid;
    if(body->damage.effects.health>0) {
        uint32_t cls=campaign_seeds.items[i].class_index;
        if(cls>=campaign_seeds.class_count)return 0;
        *temperature=campaign_seeds.classes[cls].corpse.body_temperature;return 1;
    }
    uid=(uint32_t)campaign_seeds.records.items[i].record.uid;
    for(slot=0;slot<RF_CORPSE_CAPACITY;slot++)
        if(((campaign_live_corpses.pool.active_mask>>slot)&1u) && campaign_live_corpses.slots[slot].corpse.uid==uid) {
            *temperature=campaign_live_corpses.slots[slot].corpse.update.value_2b0;return 1;
        }
    return 0;
}
/* One drawn frame with an eye camera (position, orthonormal basis rows). */
static int scene_ir_screen_update(scene_stream *s,const float eye[3],const float basis[3][3])
{
    uint32_t slot=campaign_view_slot(),i,x,y,drawn=0,material;int status=RF_OK;
    rf_model_render_buffers buffers={0};rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};rf_model_projection camera;
    float factor=1.f/tanf(45.f*.008726646259971648f),squash;
    if(slot>=SCENE_IR_SLOTS || !(material=scene_ir.material[slot]) || campaign_player_damage.state.effects.health<=0 ||
       scene_driller_active(s) || scene_turret_player_active() || !s->npc_memory || !campaign_poses.count){scene_ir.counter=2;return RF_OK;}
    if(slot==scene_ir.weapon)++scene_ir.counter;
    else {scene_ir.weapon=slot;scene_ir.counter=2;}
    if(scene_ir.counter<2)return RF_OK;
    scene_ir.counter=0;
    --material;if(material>=s->materials->count)return RF_FORMAT;
    /* shoulder_cannon (slot 12) keeps the full 64x64; the Rocket Launcher 64x48. */
    scene_ir_height=slot==12?64:48;squash=scene_ir_height==64?.75f:1.f;
    for(i=0;i<SCENE_IR_SIZE*SCENE_IR_SIZE;i++){scene_ir_color[i]=0xff000000u;scene_ir_depth[i]=0;}
    memset(&camera,0,sizeof(camera));
    memcpy(camera.camera,eye,12);
    for(i=0;i<3;i++){camera.rotation[i]=basis[0][i]*factor;camera.rotation[3+i]=basis[1][i]*factor*(4.f/3.f)*squash;camera.rotation[6+i]=basis[2][i];}
    camera.screen[0]=320;camera.screen[1]=-240;camera.screen[2]=320;camera.screen[3]=240;
    camera.perspective=camera.compute_clip=camera.clipping=camera.far_clip=1;camera.far_depth=1000;camera.screen_clip=0;
    buffers.cache=s->npc_memory;buffers.clip=(float(*)[3])((uint8_t *)s->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t *)s->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t *)s->npc_memory+4096*56);buffers.capacity=4096;
    planes.near_depth=.1f;planes.far_depth=1000;
    projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=lights.ambient[1]=lights.ambient[2]=255;
    for(i=0;i<campaign_poses.count && i<campaign_npc_body_count;i++){
        const campaign_model_owner *owner=campaign_model_owners+i;rf_entity_pose *pose;
        const rf_entity_render_model *model;rf_model_projection view;float prepared[50][12],delta[3],length=0,dot=0,distance,temperature,heat;
        uint16_t generations[50];uint32_t k,lod,batch;
        if(campaign_npc_bodies[i].object_flags&(2u|0x4000u))continue;
        if(campaign_npc_bodies[i].persistence_registered && rf_scene_defeated_actors.items[campaign_npc_bodies[i].persistence_slot].retired)continue;
        if(!scene_ir_body_heat(i,&temperature) || !(temperature>0))continue;
        for(k=0;k<3;k++){delta[k]=owner->position[k]-eye[k];length+=delta[k]*delta[k];}
        length=sqrtf(length);if(!(length>0))continue;
        for(k=0;k<3;k++)dot+=delta[k]/length*basis[2][k];
        if(dot<.17f)continue;
        distance=length<=campaign_npc_bodies[i].model_radius_78?.001f:length-campaign_npc_bodies[i].model_radius_78;
        if(distance>13.f)continue;
        heat=(1.f-distance*(1.f/13.f))*temperature;if(heat<0)continue;
        status=campaign_model_pose(i,&pose);if(status)break;if(!pose)continue;
        if(pose->skeleton>=campaign_render_models.count){status=RF_FORMAT;break;}
        model=campaign_render_models.items+pose->skeleton;
        memset(prepared,0,sizeof(prepared));memset(generations,0,sizeof(generations));
        status=rf_model_prepare_skinning(model->stored,pose->matrices,pose->bone_count,
            (uint16_t)pose->playback.generation,prepared,generations,50);if(status)break;
        status=rf_model_local_view(&camera,owner->position,owner->basis,&view);if(status)break;
        scene_ir_argb=scene_ir_heat_color(heat);++drawn;
        /* 4aeef0 draws one level below the distance LOD; the first unique LOD
         * is the port's only retained skeletal detail level. */
        for(lod=0;lod<model->file.lod_count && lod<1;lod++){
            const rf_model_geometry *g=model->lods+lod;
            for(batch=0;batch<g->batch_count && !status;batch++){
                const rf_model_draw_batch *draw=g->batches+batch;
                if(draw->material==UINT32_MAX)continue;
                status=scene_model_scratch_prepare(&buffers,draw->vertices);
                if(!status)status=rf_model_geometry_render_batch(g,batch,prepared,pose->bone_count,&view,&lights,&attributes,&buffers);
                if(!status)status=rf_preview_visit_model(g,batch,&buffers,s->npc_indices,s->npc_pool,&view,&planes,&projection,&attributes,NULL,scene_ir_triangle,NULL);
            }
        }
        if(status)break;
    }
    if(status){++rf_scene_weapon_ir[4];rf_scene_weapon_ir[5]=(uint32_t)status;drawn=0;
        for(i=0;i<SCENE_IR_SIZE*SCENE_IR_SIZE;i++)scene_ir_color[i]=0xff000000u;}
    rf_scene_weapon_ir[1]=drawn;
    if(!drawn) {
        if(scene_ir.standby)return RF_OK; /* Black already published (1084). */
        scene_ir.standby=1;++rf_scene_weapon_ir[3];
    } else {
        scene_ir.standby=0;
        /* interlace64x64.tga alpha-blended over the drawn rows. */
        if(scene_ir.interlace.rgba)for(y=0;y<scene_ir_height;y++)for(x=0;x<SCENE_IR_SIZE;x++){
            const unsigned char *o=rf_image_pixel(&scene_ir.interlace,x,y);uint32_t c=scene_ir_color[y*SCENE_IR_SIZE+x],out=0xff000000u,k;
            for(k=0;k<3;k++){uint32_t under=(c>>(16-8*k))&255,over=o[k];out|=((over*o[3]+under*(255u-o[3]))/255u)<<(16-8*k);}
            scene_ir_color[y*SCENE_IR_SIZE+x]=out;
        }
    }
    /* Publish into the material image (RGBA bytes), material value 0.7 as brightness. */
    {rf_image *image=&s->materials->items[material].image;
     if(image->width!=SCENE_IR_SIZE || image->height!=SCENE_IR_SIZE || !image->rgba)return RF_FORMAT;
     for(y=0;y<SCENE_IR_SIZE;y++)for(x=0;x<SCENE_IR_SIZE;x++){
        uint32_t c=scene_ir_color[y*SCENE_IR_SIZE+x];unsigned char p[4];
        p[0]=(unsigned char)(((c>>16)&255)*7/10);p[1]=(unsigned char)(((c>>8)&255)*7/10);p[2]=(unsigned char)((c&255)*7/10);p[3]=255;
        memcpy(rf_image_pixel(image,x,y),p,4);
     }}
    ++rf_scene_weapon_ir[0];return RF_OK;
}
#endif
