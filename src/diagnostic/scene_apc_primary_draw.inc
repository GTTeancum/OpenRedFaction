#ifndef RF_SCENE_APC_PRIMARY_DRAW_INC
#define RF_SCENE_APC_PRIMARY_DRAW_INC
/* Actual previous/current projectile segments, projected by the existing
 * world beam clipper. First-pass tracer width .04m and amber-white RGB are
 * presentation policy, NOT a decoded Tracer01.VFX appearance claim. The
 * renderer's existing white fallback texture avoids assets/rocket stand-ins.
 * Call after particle_camera is updated for this frame and before preview
 * mesh submission. Does not alter physics/RNG or render inactive old trails.
 * Eight clipped quads cost at most240 vertices (13,440 bytes), no allocation.
 */
static int scene_apc_primary_draw(scene_stream *stream)
{
    uint32_t i,j,k,first,total=0;rf_particle_screen_polygon polygons[SCENE_APC_PRIMARY_CAPACITY];int status;
    if(!stream||!stream->mesh||!stream->mesh->vertices)return RF_RANGE;
    if(rf_scene_vehicle_enabled!=2&&rf_scene_vehicle_enabled!=3)return RF_OK;
    if(stream->mesh->count>stream->capacity/sizeof(rf_preview_vertex)||
       stream->mesh->bytes!=stream->mesh->count*sizeof(rf_preview_vertex))return RF_RANGE;
    memset(polygons,0,sizeof(polygons));
    for(i=0;i<SCENE_APC_PRIMARY_CAPACITY;i++){
        const scene_apc_primary_round *round=stream->apc_primary.rounds+i;float length=0;
        if(!round->flight.active)continue;
        for(k=0;k<3;k++){float d=round->flight.position[k]-round->previous[k];length+=d*d;}
        if(length<1.e-10f)continue; /* newly launched; no distance travelled */
        status=rf_volume_beam_project(&stream->particle_camera,round->flight.position,round->previous,.04f,polygons+i);
        if(status)return status;
        if(polygons[i].count>12)return RF_RANGE;
        if(polygons[i].count>=3)total+=(polygons[i].count-2)*3;
        for(j=0;j<polygons[i].count;j++){
            const rf_particle_screen_vertex *v=polygons[i].vertices+j;
            if(!isfinite(v->reciprocal_z)||v->reciprocal_z<=0||!isfinite(v->screen[0])||!isfinite(v->screen[1]))return RF_FORMAT;
        }
    }
    if(total>(stream->capacity-stream->mesh->bytes)/sizeof(rf_preview_vertex))return RF_RANGE;
    first=stream->mesh->count;
    for(i=0;i<SCENE_APC_PRIMARY_CAPACITY;i++)for(j=1;j+1<polygons[i].count;j++){
        uint32_t corners[3]={0,j,j+1};
        for(k=0;k<3;k++){
            const rf_particle_screen_vertex *in=polygons[i].vertices+corners[k];
            rf_preview_vertex *out=stream->mesh->vertices+first++;
            float q=in->reciprocal_z;memset(out,0,sizeof(*out));
            out->position[0]=in->screen[0];out->position[1]=in->screen[1];
            out->position[2]=(1000.f/999.9f)*(1-.1f*q)*16777215.f;
            out->color[0]=1;out->color[1]=.85f;out->color[2]=.45f;
            out->texture[0]=in->uv[0]*q;out->texture[1]=in->uv[1]*q;out->texture[2]=q;
            out->material=UINT32_MAX;out->lightmap=RF_PREVIEW_ADDITIVE_FADE_TAG|220u;
            out->lightmap_texture[2]=q;
        }
    }
    stream->mesh->count=first;stream->mesh->bytes=first*sizeof(rf_preview_vertex);return RF_OK;
}
#endif
