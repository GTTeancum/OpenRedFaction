#ifndef RF_SCENE_PLAY_VCLIP_RUNTIME_INCLUDED
#define RF_SCENE_PLAY_VCLIP_RUNTIME_INCLUDED
/* Presentation-only state; no shared particle pool, damage or gameplay RNG. */
static float scene_vclip_random(scene_vclip_owner *v)
{uint32_t draw;rf_random_next(&v->random,&draw);return (float)draw*(1.f/32768.f);}
static float scene_vclip_envelope(float u,float in,float out)
{return u<in&&in>0?u/in:u>out&&out<1?(1-u)/(1-out):1;}
static int scene_vclip_sample(const scene_vclip_track *t,float time,float out[13])
{
    uint32_t i,a,b;float fraction;
    if(time<0)time=0;if(time>t->count-1)time=(float)(t->count-1);
    a=(uint32_t)time;b=a+1<t->count?a+1:a;fraction=time-a;
    for(i=0;i<13;i++)out[i]=t->samples[a*13+i]*(1-fraction)+t->samples[b*13+i]*fraction;
    /* Position/scalars interpolate; use existing original quaternion slerp. */
    return rf_model_interpolate_rotation(t->samples+a*13+3,t->samples+b*13+3,fraction,out+3);
}
static int scene_play_vclip_start(void *context,const rf_level_event *event,int32_t now)
{
    scene_vclip_owner *v=scene_play_vclip_owner;uint32_t k;
    if(!context||!event||now<0||now>RF_TIMER_PERIOD)return RF_RANGE;
    rf_scene_play_vclip[19]=event->uid;
    /* Optional resource refusal is never returned through the scheduler:
     * common phase2 links must still fire. One live owner is an explicit cap. */
    if(!v||!scene_vclip_name_equal(event->texts[0],v->name)||v->active){++rf_scene_play_vclip[13];return RF_OK;}
    for(k=0;k<3;k++)if(!isfinite(event->position[k])){rf_scene_play_vclip[2]=RF_FORMAT;return RF_OK;}
    scene_play_vclip_reset();v->active=1;v->random.value=1;v->last_ms=now;
    memcpy(v->position,event->position,12);++rf_scene_play_vclip[12];
    /*4bd0a0 passes NULL orientation; baked VFX coordinates use the root basis.
     * No RFEC extension: only a future queued ON survives a saved timeline. */
    if(v->foley[0])combat_sound(v->foley,event->position);
    return RF_OK;
}
static int scene_vclip_birth(scene_vclip_owner *v,const scene_vclip_track *t,scene_vclip_particle *p,
    const float s[13],float dt)
{
    float local[3],velocity[3],transform[10]={0,0,0,0,0,0,1,1,1,1};uint32_t k;int status;
    if(t->shape){
        float z=scene_vclip_random(v)*2-1,angle=scene_vclip_random(v)*6.28318530718f,r=sqrtf(fmaxf(0,1-z*z));
        float speed=(s[10]+(scene_vclip_random(v)*2-1)*s[11])*12.f;
        local[0]=r*cosf(angle);local[1]=z;local[2]=r*sinf(angle);
        for(k=0;k<3;k++){velocity[k]=local[k]*speed;local[k]*=s[7];}
    }else{
        float spread=s[11]*12.f;
        velocity[0]=(scene_vclip_random(v)*2-1)*spread;
        velocity[2]=(scene_vclip_random(v)*2-1)*spread;
        velocity[1]=-s[10]*12.f+(scene_vclip_random(v)*2-1)*spread;
        local[0]=(scene_vclip_random(v)-.5f)*s[7];local[1]=0;local[2]=(scene_vclip_random(v)-.5f)*s[8];
        memcpy(transform+3,s+3,16);
        status=rf_vfx_transform_point(transform,local,local);if(status)return status;
        status=rf_vfx_transform_point(transform,velocity,velocity);if(status)return status;
    }
    p->age=scene_vclip_random(v)*dt;p->life=t->lifetime;
    p->angle=(t->flags&16)?scene_vclip_random(v)*6.28318530718f:0;
    for(k=0;k<3;k++){
        p->velocity[k]=velocity[k];p->position[k]=v->position[k]+s[k]+local[k]+velocity[k]*p->age;
        if(!isfinite(p->position[k])||!isfinite(p->velocity[k]))return RF_RANGE;
    }
    ++rf_scene_play_vclip[14];return RF_OK;
}
static int scene_vclip_step(scene_vclip_owner *v,float dt)
{
    uint32_t i,j,k,live=0;float before=v->time;v->time+=dt*15.f;
    for(i=0;i<v->track_count;i++){
        scene_vclip_track *t=v->tracks+i;float sample[13],from,to,credit;uint32_t requested,admitted=0;int status;
        for(j=0;j<t->capacity;j++){
            scene_vclip_particle *p=v->particles+t->first+j;if(p->age<0)continue;
            p->age+=dt;if(p->age>=p->life){p->age=-1;continue;}
            for(k=0;k<3;k++)p->position[k]+=p->velocity[k]*dt;
        }
        /* Original negative emission start pre-rolls the first interval.
         * Integrate the sampled rate at the midpoint of this short step. */
        from=before;if(before==0&&t->emit_after<0)from=(float)t->emit_after;
        if(from<t->emit_after)from=(float)t->emit_after;to=v->time;if(to>v->duration)to=v->duration;
        if(to<=from)continue;
        status=scene_vclip_sample(t,(from+to)*.5f,sample);if(status)return status;credit=t->credit+sample[12]*320.f*(to-from);
        if(!isfinite(credit)||credit<0||credit>100000)return RF_RANGE;
        requested=(uint32_t)credit;t->credit=credit-requested;
        for(j=0;j<t->capacity&&admitted<requested;j++){
            scene_vclip_particle *p=v->particles+t->first+j;float when;int status;
            if(p->age>=0)continue;
            when=from+(to-from)*(float)(admitted+1)/(float)requested;
            status=scene_vclip_sample(t,when,sample);if(status)return status;
            status=scene_vclip_birth(v,t,p,sample,(to-from)/15.f);if(status)return status;++admitted;
        }
        rf_scene_play_vclip[15]+=requested-admitted;
    }
    for(i=0;i<v->particle_count;i++)live+=v->particles[i].age>=0;
    rf_scene_play_vclip[16]=live;
    if(v->time>v->duration&&!live){v->active=0;++rf_scene_play_vclip[20];}
    return RF_OK;
}
static void scene_play_vclip_tick(int32_t now)
{
    scene_vclip_owner *v=scene_play_vclip_owner;int32_t elapsed;uint32_t steps,i;float dt;int status=RF_OK;
    if(!v||!v->active||now==v->last_ms)return;
    elapsed=now-v->last_ms;if(elapsed<0)elapsed+=RF_TIMER_PERIOD;
    v->last_ms=now;
    /* No work explosion after a discontinuity; ordinary pause retains time. */
    if(elapsed<0||elapsed>250){status=RF_RANGE;goto done;}
    steps=((uint32_t)elapsed+16)/17;if(!steps)return;dt=(float)elapsed/(1000.f*steps);
    for(i=0;i<steps&&v->active;i++){status=scene_vclip_step(v,dt);if(status)break;}
done:
    if(status){rf_scene_play_vclip[2]=(uint32_t)status;scene_play_vclip_reset();}
}
static int scene_vclip_style(const scene_vclip_owner *v,uint32_t material,float time,uint32_t flags,
    const unsigned char lighting[3],float alpha,uint32_t *rgba,uint32_t *mode)
{
    const rf_vfx_material_view *m;float brightness=1,opacity=1;unsigned char rgb[3];int status;
    if(material>=v->materials->count)return RF_FORMAT;m=v->materials->views+material;
    status=rf_vfx_material_evaluate(v->materials->data,v->materials->bytes,m,1,time,&brightness);if(status&&status!=RF_NOT_FOUND)return status;
    status=rf_vfx_material_evaluate(v->materials->data,v->materials->bytes,m,2,time,&opacity);if(status&&status!=RF_NOT_FOUND)return status;
    brightness=fmaxf(0,fminf(1,brightness));alpha*=fmaxf(0,fminf(1,opacity));
    status=rf_vfx_material_color(m,lighting,brightness,flags,rgb);if(status)return status;
    *rgba=(uint32_t)(fmaxf(0,fminf(1,alpha))*255.f)<<24|(uint32_t)rgb[2]<<16|(uint32_t)rgb[1]<<8|rgb[0];
    *mode=(m->words[2]&255u)?RF_PARTICLE_GLOW_MODE:RF_PARTICLE_NORMAL_MODE;return RF_OK;
}
static int scene_vclip_submit(scene_stream *stream,const rf_particle_screen_polygon *polygon,
    const rf_image *image,uint32_t rgba,uint32_t mode,rf_scene_particle_sink sink,void *context)
{
    rf_particle_render_environment re={1,1,0,2};rf_particle_render_states states={0};
    rf_particle_vertex_environment env={0};rf_particle_draw_vertex vertices[12];uint32_t i;int status;
    if(!polygon->count||!(rgba>>24))return RF_OK;if(polygon->count>12||!image)return RF_RANGE;
    status=rf_particle_render_decode(mode,&re,&states);if(status)return status;
    env.rgba=rgba;env.vertex_color=states.vertex_color;env.vertex_alpha=states.vertex_alpha;
    env.depth_scale=env.reciprocal_scale=stream->particle_camera.view.scale[2];env.uv_scale[0]=env.uv_scale[1]=1;
    for(i=0;i<polygon->count;i++){status=rf_particle_vertex_encode(&env,polygon->vertices+i,vertices+i);if(status)return status;}
    return sink?sink(context,vertices,polygon->count,image,mode):RF_OK;
}
/* Use the existing bounded world quad clipper for a triangle: the repeated
 * final corner emits one degenerate fan triangle, with no missing near clip. */
static int scene_vclip_triangle(const rf_visibility_camera *camera,const float points[9],const float uv[6],rf_particle_screen_polygon *out)
{
    rf_particle_billboard_vertex quad[4];uint32_t j;
    for(j=0;j<3;j++){memcpy(quad[j].position,points+j*3,12);quad[j].uv[0]=uv[j];quad[j].uv[1]=uv[j+3];}
    quad[3]=quad[2];return rf_particle_world_quad(camera,quad,out);
}
static int scene_play_vclip_draw_impl(scene_stream *stream,rf_scene_particle_sink sink,void *context)
{
    scene_vclip_owner *v=scene_play_vclip_owner;uint32_t i,j,k,pass,rgba,mode;int status;
    unsigned char lighting[3];
    if(!v||!v->active)return RF_OK;
    /* Existing level ambient, with the shared directional-level fallback.
     * Dynamic point-light/specular matching is deferred presentation work. */
    for(k=0;k<3;k++)lighting[k]=stream->light_directional?64:
        (unsigned char)(255.f*fmaxf(0,fminf(1,stream->light_ambient[k])));
    /* Opaque geometry precedes every translucent mesh/particle submission. */
    for(pass=0;pass<2;pass++)if(v->time<=v->duration)for(i=0;i<v->mesh_count;i++){
        rf_vfx_mesh *m=v->meshes[i];rf_vfx_instance *instance=v->instances[i];
        if(!pass){status=rf_vfx_instance_update(instance,v->time);if(status)return status;}if(!instance->active)continue;
        for(j=0;j<m->prefix.faces;j++){
            rf_vfx_face face;rf_particle_screen_polygon polygon={0};const rf_image *image;uint32_t material,opaque;
            float points[9];uint32_t at=m->prefix.face_offset+j*96;
            status=rf_vfx_face_read(m->data+at,m->bytes-at,m->version,&face);if(status)return status;
            if(face.material>=m->materials)return RF_FORMAT;material=scene_vclip_word(m->data+m->material_offset+face.material*4);
            status=scene_vclip_style(v,material,v->time,m->edges.flags,lighting,1,&rgba,&mode);if(status)return status;if(!(rgba>>24))continue;
            if(v->materials->views[material].words[0]==2)image=&v->white;
            else {status=rf_vfx_material_texture_sample(&v->textures,v->materials->views+material,material,0,v->time,0,&image);if(status)return status;}
            opaque=!(m->edges.flags&1)&&mode==RF_PARTICLE_NORMAL_MODE&&(rgba>>24)==255&&
                (image==&v->white||!rf_image_format_has_alpha(image->source_format));
            if(!pass&&!opaque)continue;
            if(pass&&opaque&&!(v->materials->views[material].bitmap_requests&4u))continue;
            if(m->edges.flags&1){
                float center[3],size[2]={instance->extra[0],instance->extra[1]};
                /* One billboard per mesh, not one per authored quad triangle. */
                if(j)break;for(k=0;k<3;k++)center[k]=instance->center[k]+v->position[k];
                if(size[0]<=0||size[1]<=0)continue;
                status=scene_cane_flare_project(&stream->particle_camera,center,size,&polygon);
            }else{
                rf_vfx_face_output facing;
                for(k=0;k<9;k++)points[k]=instance->vertices[face.indices[k/3]*3+k%3]+v->position[k%3];
                status=rf_vfx_world_face(&stream->particle_camera,points,(int32_t)material,&facing);
                if(status==RF_RANGE||status==RF_FORMAT)continue;if(status)return status;if(!facing.visible)continue;
                status=scene_vclip_triangle(&stream->particle_camera,points,instance->uv+j*6,&polygon);
            }
            if(status)return status;
            /* Solid debris writes depth. Fading/alpha/glow material passes
             * keep the existing particle blend/depth-read policy. */
            if(opaque==!pass){
                if(opaque)mode=RF_VFX_OPAQUE_MODE;
                status=scene_vclip_submit(stream,&polygon,image,rgba,mode,sink,context);if(status)return status;
                ++rf_scene_play_vclip[17];
            }
            if(pass&&!(m->edges.flags&1)&&(v->materials->views[material].bitmap_requests&4u)){
                uint32_t slot=v->textures.bindings[material][2];float reflection,normal[3],uv[6];
                memcpy(&reflection,v->materials->views[material].words+35,4);
                if(!isfinite(reflection)||reflection<0||reflection>1||slot>=v->textures.texture_count)return RF_FORMAT;
                status=rf_vfx_face_normal(points,normal);if(status)return status;
                /* Authored Sub_P05 reflection map uses an explicit first-pass
                 * spherical reflection projection, not the base diffuse UV. */
                for(k=0;k<3;k++){
                    float ray[3],length,dot=0,r[3];uint32_t axis;
                    for(axis=0;axis<3;axis++)ray[axis]=points[k*3+axis]-stream->particle_camera.view.origin[axis];
                    length=sqrtf(ray[0]*ray[0]+ray[1]*ray[1]+ray[2]*ray[2]);if(length<.0001f)length=1;
                    for(axis=0;axis<3;axis++){ray[axis]/=length;dot+=ray[axis]*normal[axis];}
                    for(axis=0;axis<3;axis++)r[axis]=ray[axis]-2*normal[axis]*dot;
                    uv[k]=.5f+.5f*(r[0]*stream->particle_camera.view.basis[0]+r[1]*stream->particle_camera.view.basis[1]+r[2]*stream->particle_camera.view.basis[2]);
                    uv[k+3]=.5f-.5f*(r[0]*stream->particle_camera.view.basis[3]+r[1]*stream->particle_camera.view.basis[4]+r[2]*stream->particle_camera.view.basis[5]);
                }
                image=v->textures.textures[slot].animation.images;
                status=scene_vclip_triangle(&stream->particle_camera,points,uv,&polygon);if(status)return status;
                status=scene_vclip_submit(stream,&polygon,image,0x00ffffffu|((uint32_t)((rgba>>24)*reflection)<<24),RF_PARTICLE_GLOW_MODE,sink,context);if(status)return status;
            }
        }
    }
    for(i=0;i<v->track_count;i++){
        const scene_vclip_track *t=v->tracks+i;float sample[13];status=scene_vclip_sample(t,v->time,sample);if(status)return status;
        for(j=0;j<t->capacity;j++){
            const scene_vclip_particle *p=v->particles+t->first+j;const rf_image *image;
            rf_particle_screen_polygon polygon={0};float u,size,alpha;
            if(p->age<0||p->age>=p->life)continue;u=p->age/p->life;
            size=2*sample[9]*scene_vclip_envelope(u,t->size_in,t->size_out);alpha=scene_vclip_envelope(u,t->alpha_in,t->alpha_out);
            status=scene_vclip_style(v,t->material,v->time,0,lighting,alpha,&rgba,&mode);if(status)return status;
            if(size<=0||!(rgba>>24))continue;
            status=rf_vfx_material_texture_sample(&v->textures,v->materials->views+t->material,t->material,0,u,1,&image);if(status)return status;
            if(t->flags&256){
                float previous[3];
                /* Original5575ce velocity-shaped triangular streak. Preserve
                 * the authored direction/length; shared stretch clips a quad.
                 * Taper/three-corner raster parity is a documented refinement. */
                for(k=0;k<3;k++)previous[k]=p->position[k]-p->velocity[k]*(1.6f/15.f)*t->streak*scene_vclip_envelope(u,t->size_in,t->size_out);
                status=rf_particle_world_stretch(&stream->particle_camera,p->position,previous,size*.25f,image->width,image->height,&polygon);
            }else status=rf_particle_world_billboard(&stream->particle_camera,p->position,p->angle,size,image->width,image->height,&polygon);
            if(status)return status;status=scene_vclip_submit(stream,&polygon,image,rgba,mode,sink,context);if(status)return status;
            ++rf_scene_play_vclip[18];
        }
    }
    return RF_OK;
}
static int scene_play_vclip_draw(scene_stream *s,rf_scene_particle_sink sink,void *context)
{int status=scene_play_vclip_draw_impl(s,sink,context);if(status)rf_scene_play_vclip[2]=(uint32_t)status;return status;}
static void scene_play_vclip_open_optional(const char *tables_path,rf_vpp *effects,rf_vpp *maps,uint32_t map_count)
{
    rf_vpp tables={0};uint32_t i,attempted=0;int table_status=RF_OK,status;
    if(!scene_play_vclip_owner)memset(rf_scene_play_vclip,0,sizeof(rf_scene_play_vclip));
    for(i=0;i<campaign_events.count;i++)if(campaign_events.items[i].state.type==70){
        const rf_level_event *e=&campaign_events.items[i].authored->record;++rf_scene_play_vclip[0];
        if(scene_play_vclip_owner&&scene_vclip_name_equal(e->texts[0],scene_play_vclip_owner->name))continue;
        /* Reopen only on level demand. A read-only archive failure remains
         * optional and cannot overwrite the caller's earlier load status. */
        if(!attempted){table_status=rf_vpp_open(&tables,tables_path);attempted=1;}
        status=table_status?table_status:scene_vclip_open(&tables,effects,maps,map_count,e->texts[0]);
        if(status){if(!rf_scene_play_vclip[2])rf_scene_play_vclip[2]=(uint32_t)status;++rf_scene_play_vclip[22];
            fprintf(stderr,"PLAY_VCLIP_RESOURCE_REFUSED uid%u clip%s status%d\n",e->uid,e->texts[0],status);}
    }
    rf_vpp_close(&tables);
}
#endif
