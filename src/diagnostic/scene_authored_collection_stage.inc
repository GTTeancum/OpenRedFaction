/* RFDS3 reconstruction, separate player admission and synchronous commit.
 * Gameplay dispatch must supply validated player state before enabling load. Pending banks are
 * borrowed synchronously; no frame, other edit or renderer upload may intervene. */
typedef struct scene_authored_collection_stage {
    scene_stream candidate;
    scene_terrain_source_owner owners[4];
    scene_authored_sources_stage *sources;
    scene_authored_checkpoint_stage shared;
} scene_authored_collection_stage;
typedef struct scene_collection_support_context {
    const scene_stream *scene;const rf_checkpoint_placement *player;
} scene_collection_support_context;
static inline int scene_collection_player_support(void *opaque,const rf_physics_ground_probe *probe,float limit,
    rf_checkpoint_support_hit *out,uint32_t *matched)
{
    const scene_collection_support_context *c=opaque;rf_collision_actor_general_response actor={0};
    rf_collision_body_query query={0};rf_geomod_registry_body_hit hit;rf_checkpoint_support_hit result;
    rf_geomod_piece_batch *batch;rf_geomod_owned_piece piece;rf_physics_body *body;uint32_t found,i;int status;
    if(!c || !c->scene || !c->player || !probe || !out || !matched || c->player->count>8)return RF_RANGE;
    actor.actor.spheres=c->player->spheres;actor.actor.sphere_count=(int32_t)c->player->count;actor.actor.mass=1;
    actor.extent=probe->bounds.radius;memcpy(actor.orientation,c->player->basis,36);memcpy(actor.next_orientation,c->player->basis,36);
    memcpy(query.start,probe->start,12);memcpy(query.end,probe->end,12);query.flags=probe->query_flags;query.limit=limit;
    status=scene_detached_sources_player(c->scene,&actor,&query,1,0,&hit,&found);if(status)return status;
    if(found) {
        rf_geomod_piece_registry *registry=scene_detached_source_registry(c->scene,hit.batch/SCENE_DETACHED_BATCH_STRIDE);
        status=rf_geomod_piece_registry_get(registry,hit.batch%SCENE_DETACHED_BATCH_STRIDE,&batch);if(status)return status;
        status=rf_geomod_piece_batch_get(batch,hit.piece,&piece,&body);if(status)return status;
        result.fraction=hit.contact.fraction;memcpy(result.normal,hit.contact.normal,12);result.stable=!(body->state.flags&0x80000000u);
        for(i=0;i<3;i++)if(body->state.velocity[i]!=0 || body->state.vector_c8[i]!=0)result.stable=0;
        *out=result;
    }
    *matched=found;return RF_OK;
}
static inline int scene_collection_player_placement(const scene_stream *s,const rf_checkpoint_placement *player)
{
    uint32_t i;int status;
    if(!s || !player || !s->terrain_sources || s->terrain_source_count<2 || s->terrain_source_count>4)return RF_RANGE;
    for(i=0;i<s->terrain_source_count;i++) {
        status=rf_geomod_piece_registry_placement_check(scene_detached_source_registry(s,i),player);if(status)return status;
    }
    return RF_OK;
}
static inline int scene_authored_collection_stage_player(scene_authored_collection_stage *p,const rf_checkpoint_placement *player)
{
    rf_geomod_terrain_view full;rf_collision_composition_view composition;scene_collection_support_context support;int status;
    if(!p || !player || !p->shared.owns_pending || !p->shared.lighting || !p->shared.lighting->draw_ready)return RF_RANGE;
    status=rf_collision_composition_pending(p->candidate.terrain_publication->composition,&composition);if(status)return status;
    full=p->shared.publication;full.faces=composition.tree->faces;full.tree=p->shared.lighting->candidate.tree;full.mesh.face_count=composition.count;
    support=(scene_collection_support_context){&p->candidate,player};
    status=rf_checkpoint_standing_check_with_support(&full,player,scene_step_seconds,rf_scene_actor_movement_values.speed,
        scene_collection_player_support,&support,NULL);
    if(!status)status=scene_collection_player_placement(&p->candidate,player);
    return status==RF_NOT_FOUND?RF_FORMAT:status;
}
static inline void scene_authored_collection_stage_discard(scene_authored_collection_stage **ptr)
{
    scene_authored_collection_stage *p;if(!ptr || !(p=*ptr))return;
    scene_terrain_lighting_stage_discard(&p->shared.lighting);
    if(p->shared.owns_pending)scene_terrain_publication_abort(&p->candidate);
    scene_authored_sources_stage_discard(&p->sources);free(p);*ptr=NULL;
}
static inline int scene_authored_collection_stage_prepare(scene_stream *s,const void *input,uint32_t bytes,
    scene_authored_collection_stage **out)
{
    const unsigned char *data=input;rf_authored_checkpoint_layout layout;
    scene_authored_collection_stage *p=NULL;rf_geomod_terrain_view core;
    rf_geomod_publication_cut cuts[RF_GEOMOD_CUT_LIMIT];rf_collision_composition_view composition;
    uint64_t external,reconstruction_external,retained;uint32_t serial,i;int status;
    if(!s || !input || !out || *out || s->terrain_source_count<2 || s->terrain_source_count>4 || !s->terrain_sources ||
       !s->terrain_publication || s->terrain_publication->has_pending || !s->geometry || !s->collision || s->terrain_shadow_reference)return RF_RANGE;
    status=rf_authored_collection_layout_read(data,bytes,&layout);if(status)return status;
    if(!s->terrain_sources[0].authored || strcmp(campaign_current_level,"ctf06.rfl") || strcmp((const char *)data+16,campaign_current_level) ||
       memcmp(data+80,s->terrain_sources[0].authored->source_identity,32) || memcmp(data+112,s->terrain_checkpoint_identity+32,96) ||
       checkpoint_u32(data+208)!=s->terrain_texture_width || checkpoint_u32(data+212)!=s->terrain_texture_height)return RF_FORMAT;
    serial=checkpoint_u32(data+316);if(serial==UINT32_MAX)return RF_FORMAT;
    external=(uint64_t)SCENE_PUBLICATION_BUDGET+SCENE_LIGHTING_STAGE_BUDGET+rf_scene_terrain_atlas[3]+
        sizeof(*s->terrain_draw)+SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s->terrain_colors)+sizeof(*s->terrain_light_cache)+
        sizeof(*s->debris)+sizeof(*s->terrain_template)+(uint64_t)s->terrain_region_count*sizeof(*s->terrain_regions)+
        1100u*(sizeof(uint32_t)+sizeof(rf_vfx_light_source))+SCENE_TERRAIN_FACES*sizeof(*s->terrain_ids)+
        (uint64_t)s->geometry->faces*sizeof(uint32_t)+sizeof(*p)+SCENE_AUTHORED_DIGEST_BUDGET+SCENE_CHECKPOINT_MAX;
    for(i=0;i<s->terrain_source_count;i++) {
        const scene_terrain_source_owner *owner=s->terrain_sources+i;
        if(!owner->authored)return RF_FORMAT;
        status=rf_geomod_terrain_get(owner->authored==s->terrain_authored?s->terrain:owner->terrain,&core);if(status)return status;
        external+=core.resident_bytes+owner->authored->resident_bytes+rf_geomod_piece_registry_bytes(scene_detached_source_registry(s,i));
    }
    if(external>=scene_destruction_budget(s))return RF_RANGE;
    p=calloc(1,sizeof(*p));if(!p)return RF_IO;p->candidate=*s;p->shared.scene=s;p->shared.serial=serial;
    status=scene_authored_collection_admission_import(data,&layout,s->terrain_history_minimum,s->terrain_history_maximum,&p->shared.admission);if(status)goto failed;
    /* Atlas clone and digest scratch do not exist during source reconstruction.
     * Publication capacity remains reserved in both phases. */
    reconstruction_external=external-SCENE_LIGHTING_STAGE_BUDGET-SCENE_AUTHORED_DIGEST_BUDGET;
    status=scene_authored_sources_stage_prepare(s,data+layout.core_offset,layout.core_bytes,serial,
        (uint32_t)(scene_destruction_budget(s)-reconstruction_external),&p->sources);if(status)goto failed;
    p->shared.reserved_peak_bytes=(uint32_t)reconstruction_external+p->sources->reserved_bytes;
    retained=sizeof(*p->sources);
    for(i=0;i<p->sources->count;i++) {
        status=rf_geomod_terrain_get(p->sources->cores[i],&core);if(status)goto failed;
        retained+=core.resident_bytes+rf_geomod_piece_registry_bytes(p->sources->pieces[i]);
    }
    if(external+retained>scene_destruction_budget(s)){status=RF_RANGE;goto failed;}
    if(external+retained>p->shared.reserved_peak_bytes)p->shared.reserved_peak_bytes=(uint32_t)(external+retained);
    for(i=0;i<s->terrain_source_count;i++)p->owners[i]=(scene_terrain_source_owner){s->terrain_sources[i].authored,p->sources->cores[i],p->sources->pieces[i]};
    p->candidate.terrain_publication_serial=serial;
    p->candidate.terrain_sources=p->owners;p->candidate.terrain_authored=p->owners[0].authored;
    p->candidate.terrain=p->owners[0].terrain;p->candidate.detached_pieces=p->owners[0].pieces;
    p->candidate.terrain_fallback=p->owners[0].authored->asset_view.replaced_ids[0];
    status=rf_geomod_terrain_get(p->candidate.terrain,&core);if(status)goto failed;
    for(i=0;i<core.cuts;i++){status=rf_geomod_terrain_cutter_get(p->candidate.terrain,i,&cuts[i].mesh,cuts[i].kernel,&cuts[i].star);if(status)goto failed;}
    status=scene_terrain_publication_prepare_from(&p->candidate,&core,cuts,core.cuts,serial);if(status)goto failed;
    p->shared.owns_pending=1;
    status=scene_terrain_publication_candidate(&p->candidate,&p->shared.publication,&p->shared.origins,&p->shared.source_bindings);if(status)goto failed;
    p->shared.cuts=p->shared.publication.cuts;
    if(p->shared.publication.mesh.face_count!=layout.faces){status=RF_FORMAT;goto failed;}
    status=rf_collision_composition_pending(s->terrain_publication->composition,&composition);if(status)goto failed;
    if(!p->shared.cuts) {
        if(layout.maps || layout.faces || checkpoint_u32(data+260) || checkpoint_u32(data+264) || checkpoint_u32(data+268)){status=RF_FORMAT;goto failed;}
        status=scene_terrain_lighting_stage_empty(&p->candidate,&p->shared.publication,serial,0,checkpoint_u32(data+256),&p->shared.lighting);if(status)goto failed;
    } else {
        status=scene_terrain_lighting_stage_clone(&p->candidate,&p->shared.publication,&p->shared.lighting);if(status)goto failed;
        status=scene_authored_collection_journal_import(&p->candidate,p->shared.lighting,&layout,data,p->shared.origins,p->shared.source_bindings,serial,p->shared.cuts);if(status)goto failed;
        status=scene_terrain_lighting_stage_bake(p->shared.lighting,p->shared.source_bindings,0);if(status)goto failed;
        status=scene_terrain_lighting_stage_draw(p->shared.lighting);if(status)goto failed;
    }
    status=scene_authored_digest_capture(&p->candidate,&p->shared.publication,p->shared.origins,&composition,p->shared.lighting->staged,serial,
        p->shared.face_maps,RF_GEOMOD_PUBLICATION_FACES,&p->shared.expected,&p->shared.digest_peak_bytes);if(status)goto failed;
    for(i=0;i<layout.faces;i++) {
        uint32_t at=layout.face_offset+i*2;
        if(p->shared.face_maps[i]!=(uint32_t)(data[at]|((uint32_t)data[at+1]<<8))){status=RF_FORMAT;goto failed;}
    }
    status=rf_authored_owner_collection_decode(data+288,128,&p->shared.expected,s->terrain_source_count,p->shared.cuts,&p->shared.extension);if(status)goto failed;
    *out=p;return RF_OK;
failed:
    scene_authored_collection_stage_discard(&p);return status;
}

/* Synchronous final publication. Optional player is mandatory for a gameplay
 * load; NULL is for scene-only tools/tests. No callbacks or frames may occur
 * between prepare, player validation and this ownership transfer. */
static inline int scene_authored_collection_stage_commit(scene_authored_collection_stage *p,const rf_checkpoint_placement *player)
{
    scene_stream *s,*staged;rf_geomod_terrain *old_cores[4];rf_geomod_piece_registry *old_pieces[4];
    uint32_t i,j,selected=UINT32_MAX,images,assets=0,asset_peak=0;int status;
    if(!p || !p->sources || !p->shared.owns_pending || !p->shared.lighting ||
       !p->shared.lighting->draw_ready || p->shared.lighting->committed)return RF_RANGE;
    s=p->shared.scene;staged=p->shared.lighting->staged;
    if(!s || !s->terrain_sources || s->terrain_source_count!=p->sources->count ||
       !s->terrain_publication || !s->terrain_publication->has_pending || p->shared.lighting->owner!=&p->candidate)return RF_RANGE;
    for(i=0;i<s->terrain_source_count;i++) {
        const scene_terrain_source_owner *owner=s->terrain_sources+i;
        if(owner->authored!=p->owners[i].authored || !p->sources->cores[i] || !p->sources->pieces[i])return RF_FORMAT;
        old_cores[i]=owner->authored==s->terrain_authored?s->terrain:owner->terrain;
        old_pieces[i]=scene_detached_source_registry(s,i);
        if(!old_cores[i] || !old_pieces[i])return RF_FORMAT;
        for(j=0;j<i;j++)if(old_cores[i]==old_cores[j] || old_pieces[i]==old_pieces[j])return RF_FORMAT;
        if(owner->authored==s->terrain_authored){if(selected!=UINT32_MAX)return RF_FORMAT;selected=i;}
        assets+=owner->authored->resident_bytes;asset_peak+=owner->authored->peak_bytes;
    }
    if(selected==UINT32_MAX)return RF_FORMAT;
    if(player){status=scene_authored_collection_stage_player(p,player);if(status)return status;}
    images=s->light_rgb.count;
    if(s->terrain_atlas_registered || staged->terrain_checkpoint_loaded==2) {
        if(staged->terrain_atlas_index>=256)return RF_RANGE;
        if(images<=staged->terrain_atlas_index)images=staged->terrain_atlas_index+1;
    }
    status=scene_terrain_publication_finish(s,staged->terrain_bindings,p->shared.publication.mesh.face_count,images);if(status)return status;
    /* No fallible work below: only bounded copies, pointer transfers and frees. */
    p->shared.owns_pending=0;p->shared.lighting->owner=s;scene_terrain_lighting_stage_commit(p->shared.lighting);
    for(i=0;i<s->terrain_source_count;i++) {
        s->terrain_sources[i].terrain=p->sources->cores[i];s->terrain_sources[i].pieces=p->sources->pieces[i];
        p->sources->cores[i]=NULL;p->sources->pieces[i]=NULL;
    }
    s->terrain=s->terrain_sources[selected].terrain;s->detached_pieces=s->terrain_sources[selected].pieces;
    s->terrain_publication_serial=p->shared.serial;
    memcpy(s->terrain_history,p->shared.admission.history,sizeof(s->terrain_history));
    memcpy(s->terrain_requested,p->shared.admission.requested,sizeof(s->terrain_requested));
    s->terrain_history_count=p->shared.admission.count;s->terrain_random=p->shared.admission.random;
    s->terrain_atlas_index=staged->terrain_atlas_index;s->terrain_checkpoint_loaded=staged->terrain_checkpoint_loaded;
    rf_scene_geomod[0]=1;rf_scene_geomod[1]=p->shared.cuts;rf_scene_geomod[2]=p->shared.serial;
    rf_scene_geomod[3]=s->terrain_publication->resident_bytes+assets;rf_scene_geomod[4]=s->terrain_publication->peak_bytes+asset_peak;
    for(i=0;i<s->terrain_source_count;i++){rf_geomod_terrain_close(old_cores+i);rf_geomod_piece_registry_close(old_pieces+i);}
    scene_authored_source_cut_report(s);return RF_OK;
}
