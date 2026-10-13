/* Live security monitors: bounded secondary software view, no second world
 * mesh or gameplay dispatch. Original evidence: 412200, 412470, 412820.
 * Resolution/cache policy and raster quality are Xbox port choices. */
enum {SCENE_MONITOR_SIZE=128,SCENE_MONITOR_PIXELS=128*128,SCENE_MONITOR_IMAGES=8};
typedef struct scene_monitor_cached {
    uint32_t binding,camera,mode,generation,valid,age;
} scene_monitor_cached;
typedef struct scene_monitor_images {
    scene_monitor_cached cache[SCENE_MONITOR_IMAGES];
    uint32_t base,count,epoch,cursor,serial,pending,frame,noise;
    uint32_t visible[SCENE_MONITOR_BINDING_LIMIT];
    uint32_t *color;float *depth;
} scene_monitor_images;
/* frames, resident images, owned scratch+pixel bytes, refreshes, triangles,
 * evictions, failures, last error, last screen, last camera, mode, cache hits. */
uint32_t rf_scene_monitor_images[12];
typedef struct scene_monitor_raster {
    scene_stream *stream;scene_monitor_images *owner;const rf_lightmaps *lightmaps;
    const uint32_t *slots;uint32_t slot_count,material,triangles;
} scene_monitor_raster;
static int scene_monitor_screen_name(const unsigned char *record)
{
    static const char name[]="screen.tga";uint32_t i;
    for(i=0;i<sizeof(name);i++){
        unsigned char c=record[0x14+i];if(c>='A'&&c<='Z')c+='a'-'A';
        if(c!=(unsigned char)name[i])return 0;
    }return 1;
}
static void scene_monitor_open(scene_stream *s,rf_materials *materials)
{
    rf_material images[SCENE_MONITOR_IMAGES+1]={{0}},*items;
    scene_monitor_images *m;uint32_t i,n=0,bytes;int status=RF_OK;
    uint64_t used=RF_CAMPAIGN_LIGHTMAP_BUDGET+4;
    uint32_t budget=(rf_scene_player_shield_resources || rf_scene_fusion_enabled)?
        RF_PLAYER_SHIELD_IMAGE_BUDGET:RF_CAMPAIGN_IMAGE_BUDGET;
    memset(rf_scene_monitor_images,0,sizeof(rf_scene_monitor_images));
    for(i=0;i<campaign_clutter_records.count;i++)
        if(campaign_monitor_class(campaign_clutter_records.items[i].class_name))++n;
    if(!n)return;if(n>SCENE_MONITOR_IMAGES)n=SCENE_MONITOR_IMAGES;
    /* NPC/clutter/weapon images have separate load budgets; the initial
     * 12MiB world-material allowance is NOT the combined scene allowance.
     * Reserve the full ordinary lightmap budget plus the dynamic atlas before
     * admitting optional monitor images under the unchanged renderer cap. */
    for(i=0;i<materials->count;i++)used+=materials->items[i].image.bytes;
    used+=s->terrain_atlas.bytes;
    while(n && (materials->count+n+1>RF_CAMPAIGN_TEXTURE_SLOTS ||
          used+(uint64_t)n*SCENE_MONITOR_PIXELS*4+4>budget))--n;
    if(!n){status=RF_RANGE;goto failed;}
    bytes=n*SCENE_MONITOR_PIXELS*4+4+(n+1)*sizeof(rf_material);
    m=calloc(1,sizeof(*m));if(!m){status=RF_IO;goto failed;}
    m->color=malloc(SCENE_MONITOR_PIXELS*4);m->depth=malloc(SCENE_MONITOR_PIXELS*sizeof(float));
    if(!m->color || !m->depth){status=RF_IO;goto free_owner;}
    for(i=0;i<=n;i++){
        rf_image *image=&images[i].image;
        image->width=image->height=i==n?1:SCENE_MONITOR_SIZE;
        image->bytes=image->width*image->height*4;image->source_format=7;
        status=rf_image_allocate_pixels(image);if(status)goto free_owner;
        /* Opaque black until the first camera publication. */
        {uint32_t k;for(k=0;k<image->bytes/4;k++)((uint32_t *)image->rgba)[k]=0xff000000u;}
        images[i].status=RF_OK;images[i].archive_index=UINT32_MAX;
    }
    items=realloc(materials->items,(materials->count+n+1)*sizeof(*items));
    if(!items){status=RF_IO;goto free_owner;}
    m->base=materials->count;m->count=n;m->pending=UINT32_MAX;m->epoch=campaign_monitor_epoch;m->noise=1;
    for(i=0;i<n;i++)m->cache[i].binding=UINT32_MAX;
    materials->items=items;memcpy(items+materials->count,images,(n+1)*sizeof(*items));
    materials->count+=n+1;materials->loaded+=n+1;materials->allocated_bytes+=bytes;s->monitors=m;
    rf_scene_monitor_images[1]=n;
    rf_scene_monitor_images[2]=bytes+sizeof(*m)+SCENE_MONITOR_PIXELS*8;
    return;
free_owner:
    for(i=0;i<=n;i++)rf_image_close(&images[i].image);
    free(m->color);free(m->depth);free(m);
failed:
    ++rf_scene_monitor_images[6];rf_scene_monitor_images[7]=(uint32_t)status;
}
static void scene_monitor_close(scene_stream *s)
{
    if(!s->monitors)return;
    /* Appended images belong to the ordinary material owner, whose lifetime
     * already extends past the native renderer's drain/close fence. */
    free(s->monitors->color);free(s->monitors->depth);free(s->monitors);s->monitors=NULL;
}
static int scene_monitor_camera_pose(uint32_t uid,rf_level *view)
{
    uint32_t i,j;const campaign_npc_body *body=NULL;
    for(i=0;i<campaign_seeds.records.count;i++)if(campaign_seeds.records.items[i].record.uid==(int32_t)uid)break;
    if(i==campaign_seeds.records.count)return RF_NOT_FOUND;
    if(i<campaign_npc_body_count && campaign_npc_bodies){
        body=campaign_npc_bodies+i;
        if(body->persistence_registered && rf_scene_defeated_actors.items[body->persistence_slot].retired)return RF_NOT_FOUND;
        memcpy(view->player_position,body->eye_position,12);
        memcpy(view->player_orientation,body->look.orientation,36);
    }else{
        memcpy(view->player_position,campaign_seeds.records.items[i].record.position,12);
        memcpy(view->player_orientation,campaign_seeds.records.items[i].record.orientation,36);
    }
    for(i=0;i<3;i++){
        float length=0;if(!isfinite(view->player_position[i]))return RF_FORMAT;
        for(j=0;j<3;j++){float v=view->player_orientation[i][j];if(!isfinite(v))return RF_FORMAT;length+=v*v;}
        if(length<.5f || length>1.5f)return RF_FORMAT;
    }
    return RF_OK;
}
static uint32_t scene_monitor_visible(scene_stream *s,uint32_t binding)
{
    uint32_t i,j;float p[3]={0};const rf_clutter_base_owner *owner;
    for(i=0;i<campaign_clutter_records.count;i++)
        if(campaign_clutter_records.items[i].uid==campaign_monitor_bindings[binding].screen_uid)break;
    if(i==campaign_clutter_records.count || !campaign_clutter_bodies ||
       !(owner=campaign_clutter_bodies[i]) || campaign_clutter_model_slots[i]==UINT32_MAX ||
       (owner->state.flags&(2u|0x4000u)))return 0;
    {uint32_t room=owner->state.first_word?owner->state.first_word-1:UINT32_MAX;
     if(room<s->visibility.state.count && !s->visibility.state.rooms[room].visible)return 0;}
    /* Conservative screen-centre frustum with a 3m margin. It does not mutate
     * primary visibility or pretend to be a pixel occlusion query. */
    for(i=0;i<3;i++)for(j=0;j<3;j++)p[i]+=(owner->state.position[j]-s->npc_view.camera[j])*s->npc_view.rotation[i*3+j];
    return p[2]>-3 && p[2]<1000 && fabsf(p[0])<p[2]+3 && fabsf(p[1])<p[2]+4;
}
static void scene_monitor_begin(scene_stream *s,uint32_t frame)
{
    scene_monitor_images *m=s->monitors;uint32_t i,k,candidate=UINT32_MAX;
    /* State advances even when image allocation was declined. */
    for(i=0;i<campaign_monitor_count;i++)
        campaign_monitor_cycle(campaign_monitor_cameras+i,campaign_monitor_bindings+i,frame);
    if(!m)return;
    ++m->serial;m->frame=frame;m->pending=UINT32_MAX;rf_scene_monitor_images[0]++;
    if(m->epoch!=campaign_monitor_epoch){
        for(i=0;i<m->count;i++){m->cache[i].valid=0;m->cache[i].binding=UINT32_MAX;}
        m->epoch=campaign_monitor_epoch;
    }
    for(i=0;i<campaign_monitor_count;i++)m->visible[i]=scene_monitor_visible(s,i);
    /* Round robin refresh prevents one continuous screen from starving later
     * screens. Residency never changes after the primary draw list is built. */
    for(k=0;k<campaign_monitor_count;k++){
        uint32_t b=(m->cursor+k)%campaign_monitor_count;scene_monitor_cameras *list=campaign_monitor_cameras+b;
        const scene_monitor_binding *binding=campaign_monitor_bindings+b;
        uint32_t camera=list->count?list->uids[list->current]:binding->camera_uid;
        if(!m->visible[b])continue;
        for(i=0;i<m->count;i++)if(m->cache[i].binding==b)break;
        if(i<m->count && m->cache[i].valid && m->cache[i].camera==camera &&
           m->cache[i].mode==binding->raw_value1 && m->cache[i].generation==list->generation &&
           !list->continuous && binding->raw_value1!=2){
            ++rf_scene_monitor_images[11];continue;
        }
        candidate=b;break;
    }
    if(candidate==UINT32_MAX)return;
    for(i=0;i<m->count;i++)if(m->cache[i].binding==candidate)break;
    if(i==m->count){uint32_t oldest=UINT32_MAX;
        for(k=0;k<m->count;k++)if(m->cache[k].binding==UINT32_MAX){i=k;break;}
        if(i==m->count){
            /* Prefer an offscreen owner, then the least recently refreshed. */
            for(k=0;k<m->count;k++)if(!m->visible[m->cache[k].binding] && m->cache[k].age<oldest){oldest=m->cache[k].age;i=k;}
            if(i==m->count)for(k=0;k<m->count;k++)if(m->cache[k].age<oldest){oldest=m->cache[k].age;i=k;}
            ++rf_scene_monitor_images[5];
        }
        m->cache[i].valid=0;
    }
    m->cache[i].binding=candidate;m->cache[i].age=m->serial;m->pending=i;
    m->cursor=(candidate+1)%campaign_monitor_count;
}
static uint32_t scene_monitor_material(scene_stream *s,uint32_t placement,const unsigned char *record,uint32_t fallback)
{
    scene_monitor_images *m=s->monitors;uint32_t b,i;
    if(!m || !scene_monitor_screen_name(record))return fallback;
    for(b=0;b<campaign_monitor_count;b++)
        if(campaign_monitor_bindings[b].screen_uid==campaign_clutter_records.items[placement].uid)break;
    if(b==campaign_monitor_count)return fallback;
    /* No global material record or shared appearance/texture is modified. */
    if(campaign_monitor_bindings[b].raw_value1==1)return m->base+m->count;
    for(i=0;i<m->count;i++)if(m->cache[i].binding==b &&
        (m->pending==i || (m->cache[i].valid && m->cache[i].generation==campaign_monitor_cameras[b].generation)))return m->base+i;
    return m->base+m->count; /* Capacity misses are opaque black, never another feed. */
}
static void scene_monitor_texel(const rf_image *image,float u,float v,uint32_t *rgba)
{
    uint32_t x,y;const unsigned char *p;
    if(!image || !image->rgba || !image->width || !image->height || !isfinite(u) || !isfinite(v))return;
    u-=floorf(u);v-=floorf(v);x=(uint32_t)(u*image->width);y=(uint32_t)(v*image->height);
    if(x>=image->width)x=0;if(y>=image->height)y=0;
    p=rf_image_pixel(image,x,y);
    if(rf_image_is_packed_1555(image)){
        uint16_t c;memcpy(&c,p,2);
        rgba[0]=((c>>10)&31)*255/31;rgba[1]=((c>>5)&31)*255/31;rgba[2]=(c&31)*255/31;rgba[3]=(c&0x8000)?255:0;
    }else{rgba[0]=p[0];rgba[1]=p[1];rgba[2]=p[2];rgba[3]=p[3];}
}
static int scene_monitor_triangle(void *context,const rf_preview_vertex tri[3])
{
    scene_monitor_raster *r=context;scene_monitor_images *m=r->owner;
    float x[3],y[3],area;int left,right,top,bottom,px,py;uint32_t i,material=tri[0].material;
    const rf_image *texture=NULL,*light=NULL;
    for(i=0;i<3;i++){
        x[i]=tri[i].position[0]*(128.f/640.f);y[i]=tri[i].position[1]*(128.f/480.f);
        if(!isfinite(x[i]) || !isfinite(y[i]) || !isfinite(tri[i].texture[2]))return RF_FORMAT;
    }
    area=(x[1]-x[0])*(y[2]-y[0])-(y[1]-y[0])*(x[2]-x[0]);
    if(fabsf(area)<.000001f)return RF_OK;
    left=(int)fminf(128,fmaxf(0,floorf(fminf(x[0],fminf(x[1],x[2])))));
    right=(int)fmaxf(-1,fminf(127,ceilf(fmaxf(x[0],fmaxf(x[1],x[2])))));
    top=(int)fminf(128,fmaxf(0,floorf(fminf(y[0],fminf(y[1],y[2])))));
    bottom=(int)fmaxf(-1,fminf(127,ceilf(fmaxf(y[0],fmaxf(y[1],y[2])))));
    if(r->slots){if(material>=r->slot_count)return RF_FORMAT;material=r->slots[material];}
    else if(r->material!=UINT32_MAX)material=r->material;
    if(material<r->stream->materials->count)texture=&r->stream->materials->items[material].image;
    if(r->lightmaps && tri[0].lightmap<r->lightmaps->count)light=r->lightmaps->images+tri[0].lightmap;
    ++r->triangles;
    for(py=top;py<=bottom;py++)for(px=left;px<=right;px++){
        float cx=px+.5f,cy=py+.5f;
        float a=((x[1]-cx)*(y[2]-cy)-(y[1]-cy)*(x[2]-cx))/area;
        float b=((x[2]-cx)*(y[0]-cy)-(y[2]-cy)*(x[0]-cx))/area,c=1-a-b,q;
        uint32_t index=(uint32_t)py*128+px,color[4]={255,255,255,255},illum[4]={255,255,255,255},out=0xff000000u;
        if(a<-.00001f || b<-.00001f || c<-.00001f)continue;
        q=a*tri[0].texture[2]+b*tri[1].texture[2]+c*tri[2].texture[2];
        if(q<=0 || q<=m->depth[index])continue;
        scene_monitor_texel(texture,(a*tri[0].texture[0]+b*tri[1].texture[0]+c*tri[2].texture[0])/q,
            (a*tri[0].texture[1]+b*tri[1].texture[1]+c*tri[2].texture[1])/q,color);
        if(color[3]<128)continue;
        if(light)scene_monitor_texel(light,(a*tri[0].lightmap_texture[0]+b*tri[1].lightmap_texture[0]+c*tri[2].lightmap_texture[0])/q,
            (a*tri[0].lightmap_texture[1]+b*tri[1].lightmap_texture[1]+c*tri[2].lightmap_texture[1])/q,illum);
        for(i=0;i<3;i++){
            float shade=light?2.f*illum[i]/255.f:
                RF_PREVIEW_IS_VERTEX_LIT(tri[0].lightmap)?a*tri[0].color[i]+b*tri[1].color[i]+c*tri[2].color[i]:.5f;
            uint32_t channel=(uint32_t)fminf(255,fmaxf(0,color[i]*shade));out|=channel<<(i*8);
        }
        m->color[index]=out;m->depth[index]=q;
    }
    return RF_OK;
}
static int scene_monitor_geometry(scene_monitor_raster *r,const rf_level *camera)
{
    scene_stream *s=r->stream;const rf_scene_world_geometry *world=s->monitor_world;
    uint32_t i;int status;
    if(!world)return RF_NOT_FOUND;
    for(i=0;i<world->geometry_count;i++){
        const rf_geometry *g=i?&world->movers.items[i-1].geometry:world->world;
        const float *origin=NULL;const float (*matrix)[3]=NULL;
        if(i){
            const rf_group_attached_pose *pose;
            if(i>campaign_movers.count)return RF_FORMAT;
            pose=campaign_movers.poses+i-1;
            if(pose->flags&(2u|0x4000u))continue;
            origin=pose->position;matrix=(const float (*)[3])pose->output_matrix;
        }
        r->slots=world->slots+world->offsets[i];r->slot_count=world->offsets[i+1]-world->offsets[i];
        status=rf_preview_visit_geometry(g,camera,origin,matrix,scene_monitor_triangle,r);if(status)return status;
    }
    r->slots=NULL;r->material=UINT32_MAX;
    if(world==&s->terrain_render){
        if(s->campaign_geomod && s->campaign_geomod->active){
            const scene_campaign_geomod_owner *room=s->campaign_geomod;
            status=rf_preview_visit_geomod(&room->mesh,room->bound,s->materials->count,camera,room->colors,s->geometry,NULL,NULL,NULL,scene_monitor_triangle,r);if(status)return status;
        }
        if(s->campaign_wall && s->campaign_wall->active)for(i=0;i<2;i++){
            const scene_campaign_wall_room *room=s->campaign_wall->rooms+i;rf_geomod_mesh_view draw=room->mesh;
            if(i==1)draw.face_count=room->retained_faces;
            status=rf_preview_visit_geomod(&draw,room->bound,s->materials->count,camera,room->colors,s->geometry,NULL,NULL,NULL,scene_monitor_triangle,r);if(status)return status;
        }
        if(!s->campaign_geomod && !s->campaign_wall && s->terrain_draw){
            status=rf_preview_visit_geomod(&s->terrain_draw->view,s->terrain_draw->bound,s->materials->count,camera,
                s->terrain_colors,s->geometry,s->terrain_bindings,NULL,NULL,scene_monitor_triangle,r);if(status)return status;
            if(s->terrain_publication && s->terrain_publication->detail && s->terrain_publication->detail->published &&
               s->collision==&s->terrain_publication->detail->overlay.world){
                const scene_l1s2_detail_bank *detail=s->terrain_publication->detail->banks+s->terrain_publication->detail->active;
                status=rf_preview_visit_geomod(&detail->mesh,detail->bound,s->materials->count,camera,detail->colors,s->geometry,NULL,NULL,NULL,scene_monitor_triangle,r);if(status)return status;
            }
        }
    }
    return RF_OK;
}
static int scene_monitor_rigid(scene_monitor_raster *r,const rf_model_projection *camera,
    const rf_static_render_resource *resource,const rf_model_materials *materials,
    const float position[3],const float basis[9],uint32_t base,uint32_t textures)
{
    scene_stream *s=r->stream;rf_model_projection view;rf_model_render_buffers buffers={0};
    rf_model_lighting lights={0};rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};uint32_t part,batch;int status;
    if(!resource || !materials || !textures)return RF_OK;
    status=rf_model_local_view(camera,position,basis,&view);if(status)return status;
    buffers.cache=s->npc_memory;buffers.clip=(float(*)[3])((uint8_t *)s->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t *)s->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t *)s->npc_memory+4096*56);buffers.capacity=4096;
    planes.near_depth=.1f;planes.far_depth=1000;projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=40;lights.ambient[1]=50;lights.ambient[2]=60;r->slots=NULL;
    for(part=0;part<resource->part_count;part++){
        const rf_static_render_lod *lod;const rf_model_geometry *g;
        if(resource->parts[part].first_lod>=resource->lod_count)return RF_FORMAT;
        lod=resource->lods+resource->parts[part].first_lod;g=&lod->geometry;
        for(batch=0;batch<g->batch_count;batch++){
            const rf_model_draw_batch *draw=g->batches+batch;uint32_t slot;
            if(draw->material==UINT32_MAX)continue;if(draw->material>=materials->count)return RF_FORMAT;
            memcpy(&slot,materials->items[draw->material].record.bytes+0x10,4);
            if(slot>=textures)return RF_FORMAT;r->material=base+slot;
            status=scene_model_scratch_prepare(&buffers,draw->vertices);if(status)return status;
            status=rf_model_geometry_render_static_batch(g,batch,&view,&lights,&attributes,NULL,&buffers);if(status)return status;
            status=rf_preview_visit_model(g,batch,&buffers,s->npc_indices,s->npc_pool,&view,&planes,&projection,&attributes,
                lod->planes+draw->first_triangle,scene_monitor_triangle,r);if(status)return status;
        }
    }
    return RF_OK;
}
static int scene_monitor_models(scene_monitor_raster *r,const rf_model_projection *camera)
{
    scene_stream *s=r->stream;rf_model_render_buffers buffers={0};rf_model_lighting lights={0};
    rf_model_render_output attributes={1,{255,255,255},255,1,1};
    rf_model_clip_planes planes={0};rf_model_clip_projection projection={0};uint32_t i,part,batch;int status;
    if(!s->npc_memory)return RF_OK;
    buffers.cache=s->npc_memory;buffers.clip=(float(*)[3])((uint8_t *)s->npc_memory+4096*32);
    buffers.second=(float(*)[3])((uint8_t *)s->npc_memory+4096*44);
    buffers.vertices=(uint8_t(*)[40])((uint8_t *)s->npc_memory+4096*56);buffers.capacity=4096;
    planes.near_depth=.1f;planes.far_depth=1000;
    projection.scale[0]=320;projection.scale[1]=240;projection.clamp=1;
    lights.ambient[0]=40;lights.ambient[1]=50;lights.ambient[2]=60;r->slots=NULL;
    /* Draw retained poses directly. Never call object dispatch, prepare a new
     * animation pose, change main-room visibility, or append native draws. */
    for(i=0;campaign_clutter_bodies && i<campaign_clutter_records.count;i++){
        const rf_clutter_base_owner *owner=campaign_clutter_bodies[i];rf_model_projection view;
        const rf_static_render_resource *resource;uint32_t first,last,model=campaign_clutter_model_slots[i];
        if(!owner || model==UINT32_MAX || (owner->state.flags&(2u|0x4000u)))continue;
        if(model>=campaign_clutter_model_count)return RF_FORMAT;
        resource=&campaign_clutter_models[model].resource;
        if(campaign_clutter_appearance_slots[i]>=rf_scene_clutter_skins[0])return RF_FORMAT;
        first=campaign_clutter_material_offsets[campaign_clutter_appearance_slots[i]];
        last=campaign_clutter_material_offsets[campaign_clutter_appearance_slots[i]+1];
        status=rf_model_local_view(camera,owner->state.position,owner->matrix,&view);if(status)return status;
        for(part=0;part<resource->part_count;part++){
            const rf_static_render_lod *lod;const rf_model_geometry *g;
            if(resource->parts[part].first_lod>=resource->lod_count)return RF_FORMAT;
            lod=resource->lods+resource->parts[part].first_lod;g=&lod->geometry;
            for(batch=0;batch<g->batch_count;batch++){
                const rf_model_draw_batch *draw=g->batches+batch;uint32_t slot;const unsigned char *record;
                if(draw->material==UINT32_MAX)continue;if(draw->material>=last-first)return RF_FORMAT;
                record=campaign_clutter_materials.items[first+draw->material].record.bytes;
                memcpy(&slot,record+0x10,4);if(slot>=s->clutter_textures)return RF_FORMAT;
                /* Screens in a feed are black: no recursive rendering, and
                 * never read a native image being overwritten this pass. */
                r->material=scene_monitor_screen_name(record)?s->monitors->base+s->monitors->count:s->clutter_base+slot;
                status=scene_model_scratch_prepare(&buffers,draw->vertices);if(status)return status;
                status=rf_model_geometry_render_static_batch(g,batch,&view,&lights,&attributes,NULL,&buffers);if(status)return status;
                status=rf_preview_visit_model(g,batch,&buffers,s->npc_indices,s->npc_pool,&view,&planes,&projection,&attributes,
                    lod->planes+draw->first_triangle,scene_monitor_triangle,r);if(status)return status;
            }
        }
    }
    for(i=0;i<campaign_poses.count;i++){
        const campaign_model_owner *owner=campaign_model_owners+i;rf_entity_pose *pose;
        const rf_entity_render_model *model;rf_model_projection view;float prepared[50][12];uint16_t generations[50];
        uint32_t first,last,lod;
        if(i>=campaign_npc_body_count || (campaign_npc_bodies[i].object_flags&(2u|0x4000u)))continue;
        if(campaign_npc_bodies[i].persistence_registered && rf_scene_defeated_actors.items[campaign_npc_bodies[i].persistence_slot].retired)continue;
        if(!strcmp(campaign_seeds.records.items[i].record.class_name,"camera2"))continue;
        /* Borrow the pose only; campaign_model_pose is a lookup, not a tick. */
        status=campaign_model_pose(i,&pose);if(status)return status;if(!pose)continue;
        if(owner->appearance>=campaign_npc_materials.count)return RF_FORMAT;
        first=campaign_npc_materials.offsets[owner->appearance];last=campaign_npc_materials.offsets[owner->appearance+1];
        if(pose->skeleton>=campaign_render_models.count)return RF_FORMAT;
        model=campaign_render_models.items+pose->skeleton;
        memset(prepared,0,sizeof(prepared));memset(generations,0,sizeof(generations));
        status=rf_model_prepare_skinning(model->stored,pose->matrices,pose->bone_count,
            (uint16_t)pose->playback.generation,prepared,generations,50);if(status)return status;
        status=rf_model_local_view(camera,owner->position,owner->basis,&view);if(status)return status;
        for(lod=0;lod<model->file.lod_count;lod++){
            const rf_model_geometry *g=model->lods+lod;uint32_t previous;
            for(previous=0;previous<lod;previous++)if(model->file.lods[previous].section_index==model->file.lods[lod].section_index)break;
            if(previous<lod)continue;
            for(batch=0;batch<g->batch_count;batch++){
                const rf_model_draw_batch *draw=g->batches+batch;uint32_t slot;
                if(draw->material==UINT32_MAX)continue;if(draw->material>=last-first)return RF_FORMAT;
                memcpy(&slot,campaign_npc_materials.materials.items[first+draw->material].record.bytes+0x10,4);
                if(slot>=s->npc_textures)return RF_FORMAT;r->material=s->npc_base+slot;
                status=scene_model_scratch_prepare(&buffers,draw->vertices);if(status)return status;
                status=rf_model_geometry_render_batch(g,batch,prepared,pose->bone_count,&view,&lights,&attributes,&buffers);if(status)return status;
                status=rf_preview_visit_model(g,batch,&buffers,s->npc_indices,s->npc_pool,&view,&planes,&projection,&attributes,NULL,scene_monitor_triangle,r);if(status)return status;
            }
        }
    }
    if(s->driller && scene_driller_visible(s)){
        status=scene_monitor_rigid(r,camera,&s->driller->render,&s->driller->materials,
            s->driller_position,s->driller_basis,s->driller_base,s->driller_textures);if(status)return status;
    }
    for(i=0;i<campaign_passive_vehicle_count;i++){
        const scene_passive_vehicle *owner=campaign_passive_vehicles+i;const scene_driller_resources *resource;
        uint32_t kind=owner->resource_kind,base,textures;
        if(owner->damage.object_flags&(2u|0x4000u))continue;if(kind>=6)return RF_FORMAT;
        if(s->driller && campaign_selected_vehicle_resource_kind()==kind){
            resource=s->driller;base=s->driller_base;textures=s->driller_textures;
        }else{resource=s->passive_vehicle_resources[kind];base=s->passive_vehicle_base[kind];textures=s->passive_vehicle_textures[kind];}
        if(!resource)continue;
        status=scene_monitor_rigid(r,camera,&resource->render,&resource->materials,
            owner->pose.position,owner->pose.input_matrix,base,textures);if(status)return status;
    }
    for(i=0;s->pickup_taken && i<s->pickups.count;i++)if(!s->pickup_taken[i]){
        const rf_level_item *item=s->pickups.items+i;int kind=pickup_class(item->class_name);
        if(kind<0)continue;
        if(kind && s->pickup_resources){
            const scene_pickup_resource *resource=s->pickup_resources+kind-1;
            status=scene_monitor_rigid(r,camera,&resource->model,&resource->materials,item->position,
                item->orientation[0],resource->base,resource->textures);if(status)return status;
        }else if(!kind && campaign_pistol_id>=0 && campaign_pistol_id<64){
            uint32_t model=campaign_weapon_models.weapons[campaign_pistol_id].model;
            if(model && model<=campaign_weapon_models.count){
                const rf_static_render_resource *resource=&campaign_weapon_models.items[model-1].render;
                rf_model_materials materials=campaign_weapon_materials;
                materials.items+=campaign_weapon_material_offsets[model-1];materials.count=resource->material_count;
                status=scene_monitor_rigid(r,camera,resource,&materials,item->position,item->orientation[0],
                    s->weapon_base,s->weapon_textures);if(status)return status;
            }
        }
    }
    return RF_OK;
}
void rf_scene_update_monitor_images(const rf_lightmaps *lightmaps)
{
    scene_stream *s=particle_draw_stream;scene_monitor_images *m;scene_monitor_cached *cache;
    scene_monitor_cameras *list;const scene_monitor_binding *binding;rf_level camera={0};
    uint32_t i,j,uid,mode;int status=RF_OK;
    if(!s || !(m=s->monitors) || m->pending>=m->count)return;
    cache=m->cache+m->pending;binding=campaign_monitor_bindings+cache->binding;list=campaign_monitor_cameras+cache->binding;
    uid=list->count?list->uids[list->current]:binding->camera_uid;mode=binding->raw_value1;
    if(!mode && scene_monitor_camera_pose(uid,&camera))mode=2;
    for(i=0;i<SCENE_MONITOR_PIXELS;i++){m->color[i]=0xff000000u;m->depth[i]=0;}
    if(mode==2){
        /* 4123ad: multiply by65539, select bit18. Private rendering RNG. */
        for(i=0;i<SCENE_MONITOR_PIXELS;i++){m->noise*=65539u;m->color[i]=(m->noise&0x40000)?0xff000000u:0xffffffffu;}
    }else if(mode==0){
        scene_monitor_raster raster={s,m,lightmaps,NULL,0,UINT32_MAX,0};
        rf_model_projection view=s->npc_view;float factor=1.f/tanf(50.f*.008726646259971648f);
        memcpy(view.camera,camera.player_position,12);memcpy(view.rotation,camera.player_orientation,36);
        view.screen[0]=320;view.screen[1]=-240;view.screen[2]=320;view.screen[3]=240;
        view.perspective=view.compute_clip=view.clipping=view.far_clip=1;
        view.far_depth=1000;view.screen_clip=0;
        /* Square target: world Y clipping convention is +/-0.75*z, while
         * model clip convention is +/-z. Output raster rescales640x480. */
        for(i=0;i<3;i++){
            camera.player_orientation[0][i]*=factor;camera.player_orientation[1][i]*=factor*.75f;
            view.rotation[i]*=factor;view.rotation[3+i]*=factor;
        }
        status=scene_monitor_geometry(&raster,&camera);
        if(!status)status=scene_monitor_models(&raster,&view);
        rf_scene_monitor_images[4]=raster.triangles;
    }
    if(status){
        ++rf_scene_monitor_images[6];rf_scene_monitor_images[7]=(uint32_t)status;
        /* Partial images never publish; an optional feed cannot abort play. */
        for(i=0;i<SCENE_MONITOR_PIXELS;i++)m->color[i]=0xff000000u;
    }
    {rf_image *image=&s->materials->items[m->base+m->pending].image;
     for(i=0;i<128;i++)for(j=0;j<128;j++){
        uint32_t c=m->color[i*128+j],out=0xff000000u,k;
        /* 41259e monitor brightness0.7, applied once at publication. */
        for(k=0;k<3;k++)out|=(((c>>(8*k))&255)*7/10)<<(8*k);
        memcpy(rf_image_pixel(image,j,i),&out,4);
     }}
    cache->camera=uid;cache->mode=mode;cache->generation=list->generation;cache->valid=!status;
    rf_scene_monitor_images[3]++;rf_scene_monitor_images[8]=binding->screen_uid;
    rf_scene_monitor_images[9]=uid;rf_scene_monitor_images[10]=mode;m->pending=UINT32_MAX;
}
