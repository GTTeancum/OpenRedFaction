#ifndef RF_SCENE_DRILLER_EXCAVATION_INC
#define RF_SCENE_DRILLER_EXCAVATION_INC
/* Include after authored edit and debris/detached helpers. Borrow the supplied
 * actual single/double drill shape only during the synchronous transaction;
 * stream's ordinary explosion shape is restored on every return path.
 * No random rocket basis/radius substitution. Existing transaction publishes
 * replacement collision, cut rendering, atlas bindings and detached ownership.
 * First-pass policy: debris preparation follows commit, so rejected drilling
 * cannot relaunch old debris; its surface probes see the newly cut world. */
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
    }
    return RF_NOT_FOUND; /* Not in an admitted editable source: never report a cut. */
}
static int scene_driller_excavate(scene_stream *s,uint32_t frame,rf_geomod_template *shape,
    const rf_weapon_flight_contact *contact,const float center[3],const float basis[9],float requested_radius,uint32_t flags,uint32_t *accepted,int *postwork_status)
{
    rf_geomod_region_result region;rf_geomod_shallow_limit limits[2];
    rf_geomod_shallow_history record={0};rf_geomod_template *saved;
    uint32_t count=0,prior[4],serial,i,k,woken=0;uint16_t packed[3];float adjusted[3],radius,requested_scale;int status;
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
    status=scene_terrain_authored_template_edit(s,adjusted,basis,region.hardness.scale,limits,count);
    s->terrain_template=saved; /* Restore before any error/cosmetic work. */
    if(status)return status;
    if(s->terrain_publication_serial==serial)return RF_FORMAT;
    *accepted=1; /* Publication is committed even if later effects fail. */
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
