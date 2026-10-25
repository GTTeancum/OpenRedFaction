#ifndef RF_SCENE_DRILLER_EXCAVATION_INC
#define RF_SCENE_DRILLER_EXCAVATION_INC
/* Include after authored edit and debris/detached helpers. Borrow the supplied
 * actual single/double drill shape only during the synchronous transaction;
 * stream's ordinary explosion shape is restored on every return path.
 * No random rocket basis/radius substitution. Existing transaction publishes
 * replacement collision, cut rendering, atlas bindings and detached ownership.
 * First-pass policy: debris preparation follows commit, so rejected drilling
 * cannot relaunch old debris; its surface probes see the newly cut world. */
/* Refused drill cuts: count,status,frame,centerXYZ,scale,limit count. */
uint32_t rf_scene_drill_reject[8];
/* Replay inputs: last accepted then last refused cut, each requested center3,
 * aligned center3, basis9, shape (1 double); 16 words each. */
uint32_t rf_scene_drill_trace[32];
/* Accepted drill bites: new cutter, tunnel extension, live tunnel source mask. */
uint32_t rf_scene_drill_tunnel[4];
static int scene_driller_excavation_overlap(const scene_stream *s,const rf_geomod_template *shape,
    const float center[3],const float basis[9],float scale,const rf_geomod_shallow_limit *limits,uint32_t limit_count)
{
    float minimum[3],maximum[3];uint32_t i,j,k,n;int status;
    if(!s || !s->terrain_authored || s->terrain_source_count>4 || (s->terrain_source_count && !s->terrain_sources))return RF_RANGE;
    status=rf_geomod_template_bounds(shape,center,basis,scale,limits,limit_count,minimum,maximum);if(status)return status;
    n=s->terrain_source_count?s->terrain_source_count:1;
    for(i=0;i<n;i++) {
        const scene_terrain_authored_assets *a=s->terrain_source_count?s->terrain_sources[i].authored:s->terrain_authored;
        float lo[3],hi[3];
        if(!a || !a->source.vertices || !a->source.vertex_count)return RF_FORMAT;
        memcpy(lo,a->source.vertices[0].position,12);memcpy(hi,lo,12);
        for(j=0;j<a->source.vertex_count;j++)for(k=0;k<3;k++) {
            float value=a->source.vertices[j].position[k];if(!isfinite(value))return RF_FORMAT;
            if(value<lo[k])lo[k]=value;
            if(value>hi[k])hi[k]=value;
        }
        for(k=0;k<3;k++)if(maximum[k]<=lo[k] || minimum[k]>=hi[k])break;
        if(k==3)return RF_OK;
        /* A tunnel continues past the original air brush: a bite touching
         * this source's live tunnel cutter extends that same edit. */
        if((s->drill_tunnel_mask>>i)&1u) {
            const rf_geomod_terrain *core=!s->terrain_source_count || s->terrain_sources[i].authored==s->terrain_authored?
                s->terrain:s->terrain_sources[i].terrain;
            rf_geomod_terrain_view view;rf_geomod_mesh_view cut;float kernel[3];uint32_t star;
            if(core && !rf_geomod_terrain_get(core,&view) && view.cuts &&
               !rf_geomod_terrain_cutter_get(core,view.cuts-1,&cut,kernel,&star) && star &&
               !memcmp(kernel,s->drill_tunnel_kernel[i],sizeof(kernel)) && cut.vertex_count) {
                memcpy(lo,cut.vertices[0].position,12);memcpy(hi,lo,12);
                for(j=0;j<cut.vertex_count;j++)for(k=0;k<3;k++) {
                    float value=cut.vertices[j].position[k];
                    if(value<lo[k])lo[k]=value;
                    if(value>hi[k])hi[k]=value;
                }
                for(k=0;k<3;k++)if(maximum[k]<=lo[k] || minimum[k]>=hi[k])break;
                if(k==3)return RF_OK;
            }
        }
    }
    return RF_NOT_FOUND; /* Not in an admitted editable source: never report a cut. */
}
/* Last star cutter kernel of each live source (bit set when present). */
static uint32_t scene_drill_tunnel_kernels(const scene_stream *s,float kernels[4][3])
{
    uint32_t i,n=s->terrain_source_count?s->terrain_source_count:1,mask=0,star;
    for(i=0;i<n && i<4;i++) {
        /* The active source's collection alias is synced lazily; s->terrain is live. */
        const rf_geomod_terrain *core=!s->terrain_source_count || s->terrain_sources[i].authored==s->terrain_authored?
            s->terrain:s->terrain_sources[i].terrain;
        rf_geomod_terrain_view view;rf_geomod_mesh_view mesh;
        if(!core || rf_geomod_terrain_get(core,&view) || !view.cuts)continue;
        if(!rf_geomod_terrain_cutter_get(core,view.cuts-1,&mesh,kernels[i],&star) && star)mask|=1u<<i;
    }
    return mask;
}
static int scene_driller_excavate(scene_stream *s,uint32_t frame,rf_geomod_template *shape,
    const rf_weapon_flight_contact *contact,const float center[3],const float basis[9],float requested_radius,uint32_t flags,uint32_t *accepted,int *postwork_status)
{
    rf_geomod_region_result region;rf_geomod_shallow_limit limits[2];
    rf_geomod_shallow_history record={0};rf_geomod_template *saved;
    uint32_t count=0,prior[4],serial,i,k,woken=0,before,after,merged;uint16_t packed[3];
    float adjusted[3],radius,requested_scale,kernels_before[4][3],kernels_after[4][3];int status;
    if(!accepted || !postwork_status)return RF_RANGE;
    *accepted=0;*postwork_status=RF_OK;
    if(!s || !shape || !contact || !center || !basis)return RF_RANGE;
    if(!s->terrain || !s->terrain_authored || !s->terrain_template)return RF_NOT_FOUND;
    if(contact->object!=UINT32_MAX || contact->room!=s->terrain_collision.room)return RF_NOT_FOUND;
    if(!s->materials || s->terrain_material>=s->materials->count || !s->terrain_texture_width || !s->terrain_texture_height)return RF_FORMAT;
    if(!isfinite(requested_radius) || !isfinite(shape->radius) || shape->radius<=0)return RF_FORMAT;
    if(requested_radius<1)return RF_NOT_FOUND;
    /* Original467020: flag8 bypasses template-radius normalization AND
     * restores scale1 after region admission. Drill flags0x3e use full host
     * basis(flag4); the requested10/20 radius is admission metadata only. */
    requested_scale=(flags&8)?1.f:requested_radius/shape->radius;
    status=rf_geomod_regions_prepare(s->terrain_regions,s->terrain_region_count,s->terrain_default_hardness,center,requested_scale,&region);if(status)return status;
    if(!region.hardness.allowed || region.hardness.flags)return RF_NOT_FOUND;
    if(flags&8)region.hardness.scale=1;
    status=rf_geomod_shallow_align(center,shape->radius,region.limits,region.limit_count,s->terrain_history,s->terrain_history_count,adjusted);if(status)return status;
    status=rf_geomod_shallow_normalize(region.limits,region.limit_count,limits,&count);if(status)return status;
    status=scene_driller_excavation_overlap(s,shape,adjusted,basis,region.hardness.scale,limits,count);if(status)return status;
    for(i=0;i<s->terrain_history_count;i++) {
        float decoded[3];double distance=0;
        status=rf_geomod_position_decode(s->terrain_history_minimum,s->terrain_history_maximum,s->terrain_requested[i],decoded);if(status)return status;
        for(k=0;k<3;k++){double d=(double)decoded[k]-center[k];distance+=d*d;}
        if(distance<=(double)(.2f*.2f))return RF_NOT_FOUND;
    }
    if(s->terrain_history_count>=128)return RF_RANGE;
    status=rf_geomod_position_encode(s->terrain_history_minimum,s->terrain_history_maximum,center,packed);if(status)return status;
    radius=region.hardness.scale*shape->radius;if(!isfinite(radius) || radius<=0)return RF_FORMAT;
    status=scene_detached_sources_counts(s,prior);if(status)return status;
    serial=s->terrain_publication_serial;saved=s->terrain_template;s->terrain_template=shape;
    /* A bite near the previous one, along roughly the same heading, extends
     * that tunnel instead of stacking a cutter (port policy; see geomod.h). */
    {double distance=0,heading=0;
     for(k=0;k<3;k++){double d=(double)adjusted[k]-s->drill_tunnel_center[k];distance+=d*d;heading+=(double)basis[6+k]*s->drill_tunnel_forward[k];}
     s->terrain_merge_request=s->drill_tunnel_mask && s->drill_tunnel_serial==serial &&
        distance<(double)(1.5f*radius)*(1.5f*radius) && heading>.8;}
    before=scene_drill_tunnel_kernels(s,kernels_before);
    status=scene_terrain_authored_template_edit(s,adjusted,basis,region.hardness.scale,limits,count);
    merged=s->terrain_merge_request;s->terrain_merge_request=0;
    s->terrain_template=saved; /* Restore before any error/cosmetic work. */
    {uint32_t *row=rf_scene_drill_trace+(status?16:0);
     memcpy(row,center,12);memcpy(row+3,adjusted,12);memcpy(row+6,basis,36);row[15]=shape->face_count;}
    if(status==RF_FORMAT || status==RF_RANGE) {
        /* The transaction discards its candidate on rejection, so a cutter
         * the terrain cannot take is a refused drill contact, not a fatal. */
        ++rf_scene_drill_reject[0];rf_scene_drill_reject[1]=(uint32_t)status;rf_scene_drill_reject[2]=frame;
        memcpy(rf_scene_drill_reject+3,adjusted,12);memcpy(rf_scene_drill_reject+6,&region.hardness.scale,4);
        rf_scene_drill_reject[7]=count;return RF_NOT_FOUND;
    }
    if(status)return status;
    if(s->terrain_publication_serial==serial)return RF_FORMAT;
    *accepted=1; /* Publication is committed even if later effects fail. */
    /* The tunnel continues on every source whose last cutter this bite made. */
    after=scene_drill_tunnel_kernels(s,kernels_after);s->drill_tunnel_mask=0;
    for(i=0;i<4;i++)if(((after>>i)&1u) && (!((before>>i)&1u) || memcmp(kernels_after[i],kernels_before[i],sizeof(kernels_after[i])))) {
        memcpy(s->drill_tunnel_kernel[i],kernels_after[i],sizeof(kernels_after[i]));s->drill_tunnel_mask|=1u<<i;
    }
    memcpy(s->drill_tunnel_center,adjusted,12);memcpy(s->drill_tunnel_forward,basis+6,12);
    s->drill_tunnel_serial=s->terrain_publication_serial;++rf_scene_drill_tunnel[merged?1:0];rf_scene_drill_tunnel[2]=s->drill_tunnel_mask;
    memcpy(record.center,adjusted,12);record.scale=region.hardness.scale;
    for(i=0;i<region.limit_count;i++)for(k=0;k<3;k++)record.vectors[i][k]=region.limits[i].normal[k]*region.limits[i].depth;
    s->terrain_history[s->terrain_history_count]=record;
    memcpy(s->terrain_requested[s->terrain_history_count],packed,sizeof(packed));s->terrain_history_count++;
    status=scene_debris_prepare(s,contact,radius);
    if(!status)status=scene_debris_spawn(s);
    if(!status)status=scene_debris_postedit(s,adjusted,radius,frame);
    if(!status)status=scene_detached_sources_notify(s,prior,adjusted,radius,&woken);
    *postwork_status=status;return RF_OK; /* Never relabel a committed cut as failed. */
}
#endif
