#ifndef RF_SCENE_CORPSE_LIFECYCLE_INCLUDED
#define RF_SCENE_CORPSE_LIFECYCLE_INCLUDED
/* Include after scene_npc_render_family, before scene_npc_draw. Existing
 *416940 retention owns the five-body limit/creation-order selection and starts
 *the one-second fade.417290 and the live tick already expire/delete owners.
 *This completes their draw path without another timer, TTL or corpse pool. */
uint32_t rf_scene_corpse_lifecycle[9]; /* calls,faded calls,tagged vertices,last alpha,hidden skips,errors,first fading UID,min alpha,max alpha */
static void scene_corpse_lifecycle_reset(void)
{
    memset(rf_scene_corpse_lifecycle,0,sizeof(rf_scene_corpse_lifecycle));
    rf_scene_corpse_lifecycle[6]=UINT32_MAX;rf_scene_corpse_lifecycle[7]=255;
}

static int scene_corpse_lifecycle_alpha(const rf_corpse *corpse,uint32_t *alpha)
{
    const rf_corpse_fade_state *f;if(!corpse||!alpha)return RF_RANGE;f=&corpse->update.fade;
    if(!isfinite(f->health_34))return RF_FORMAT;
    if(f->health_34<0||(f->object_flags_7c&(2u|0x4000u))){*alpha=0;return RF_OK;}
    if(!(f->flags_29c&1u)){*alpha=255;return RF_OK;}
    if(!isfinite(f->fade_298))return RF_FORMAT;
    /* The original timer is retained. Linear opacity over its final second
     *is the first-pass presentation policy; do not mutate owner state here. */
    if(f->fade_298<=0)*alpha=0;
    else if(f->fade_298>=1)*alpha=255;
    else *alpha=(uint32_t)(f->fade_298*255.0f);
    return RF_OK;
}

static int scene_corpse_lifecycle_draw(scene_npc_render_context *context,const rf_corpse *corpse)
{
    rf_scene_model_backend retained;rf_preview_mesh *mesh;
    uint32_t alpha,start,bytes,i;int status;
    if(!context||!context->stream||!context->stream->mesh||!corpse||
       corpse->update.model!=context->actor+1)return RF_RANGE;
    ++rf_scene_corpse_lifecycle[0];status=scene_corpse_lifecycle_alpha(corpse,&alpha);if(status)goto fail;
    rf_scene_corpse_lifecycle[3]=alpha;
    if(!alpha){++rf_scene_corpse_lifecycle[4];return RF_OK;}
    if(alpha==255){status=scene_npc_render_family(context,0);if(status)goto fail;return RF_OK;}
    mesh=context->stream->mesh;start=mesh->count;bytes=mesh->bytes;
    /* The retained Xbox model API has no opacity input. Only this brief fade
     *uses the existing CPU skin/clip path and its alpha-capable mesh format.
     *The synchronous family draw has no reentrant scene-render callbacks. */
    retained=model_backend;model_backend=NULL;
    status=scene_npc_render_family(context,0);
    model_backend=retained; /* Restore on every result, before any return. */
    if(status){mesh->count=start;mesh->bytes=bytes;goto fail;}
    for(i=start;i<mesh->count;i++)mesh->vertices[i].lightmap=RF_PREVIEW_FADE_TAG|alpha;
    ++rf_scene_corpse_lifecycle[1];rf_scene_corpse_lifecycle[2]+=mesh->count-start;
    if(mesh->count>start) {
        if(rf_scene_corpse_lifecycle[6]==UINT32_MAX)rf_scene_corpse_lifecycle[6]=corpse->uid;
        if(alpha<rf_scene_corpse_lifecycle[7])rf_scene_corpse_lifecycle[7]=alpha;
        if(alpha>rf_scene_corpse_lifecycle[8])rf_scene_corpse_lifecycle[8]=alpha;
    }
    return RF_OK;
fail:++rf_scene_corpse_lifecycle[5];return status;
}
#endif
