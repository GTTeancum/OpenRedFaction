/* Include after checkpoint scalar/map/material helpers, publication/lighting,
 * authored digest, journal and admission adapters. Prepare/discard only: caller
 * owns one synchronous final publication after all other gates. No frame,
 * renderer upload, reentry or second candidate may intervene. */
#include "rf/authored_owner_extension.h"
typedef struct scene_authored_checkpoint_stage {
    scene_stream *scene;rf_geomod_terrain *core;rf_geomod_piece_registry *pieces;
    scene_terrain_lighting_stage *lighting;
    scene_authored_admission_state admission;
    rf_geomod_terrain_view publication;
    const rf_geomod_publication_origin *origins;
    rf_preview_surface_lightmap *source_bindings;
    rf_authored_owner_expected expected;rf_authored_owner_extension extension;
    uint16_t face_maps[RF_GEOMOD_PUBLICATION_FACES];
    uint32_t serial,cuts,reserved_peak_bytes,digest_peak_bytes,owns_pending;
} scene_authored_checkpoint_stage;
static void scene_authored_checkpoint_stage_discard(scene_authored_checkpoint_stage **ptr)
{
    scene_authored_checkpoint_stage *p;if(!ptr || !*ptr)return;p=*ptr;
    /* Lighting borrows publication/collision; pending trees borrow bank vertices.
     * Abort publication before closing candidate core. No live core is owned. */
    scene_terrain_lighting_stage_discard(&p->lighting);
    if(p->owns_pending)scene_terrain_publication_abort(p->scene);
    rf_geomod_piece_registry_close(&p->pieces);rf_geomod_terrain_close(&p->core);free(p);*ptr=NULL;
}
static int scene_authored_checkpoint_stage_prepare(scene_stream *s,const void *input,uint32_t bytes,
    const rf_checkpoint_placement *player,scene_authored_checkpoint_stage **out)
{
    const unsigned char *data=input;rf_authored_checkpoint_layout layout;
    scene_authored_checkpoint_stage *p=NULL;scene_authored_edit_context factory={0};
    rf_geomod_terrain_view active,core,full;rf_geomod_publication_cut cuts[RF_GEOMOD_CUT_LIMIT];
    rf_collision_composition_view composition;unsigned char *history=NULL;
    uint64_t external,reserved;uint32_t serial,i;int status;
    if(!s || !input || !out || *out || !s->terrain || !s->terrain_authored || !s->terrain_publication ||
       s->terrain_publication->has_pending || !s->geometry || !s->collision || s->terrain_shadow_reference)return RF_RANGE;
    status=rf_authored_checkpoint_layout_read(data,bytes,&layout);if(status)return status;
    if(strcmp(campaign_current_level,"ctf06.rfl") || strcmp((const char *)data+16,campaign_current_level) ||
       memcmp(data+80,s->terrain_authored->source_identity,32) ||
       memcmp(data+112,s->terrain_checkpoint_identity+32,96) ||
       checkpoint_u32(data+208)!=s->terrain_texture_width || checkpoint_u32(data+212)!=s->terrain_texture_height)return RF_FORMAT;
    serial=checkpoint_u32(data+316);if(serial==UINT32_MAX)return RF_FORMAT;
    status=scene_checkpoint_materials_mode(data+layout.core_offset,layout.core_bytes,0,0,0);if(status)return status;
    status=rf_geomod_terrain_get(s->terrain,&active);if(status)return status;
    /* Same conservative13MiB destruction ceiling as live authored edit, plus
     * explicit restore-only owners/scratch. This is not total-game free RAM. */
    external=(uint64_t)SCENE_PUBLICATION_BUDGET+SCENE_LIGHTING_STAGE_BUDGET+SCENE_DETACHED_BUDGET+
        rf_geomod_piece_registry_bytes(s->detached_pieces)+
        rf_scene_terrain_atlas[3]+s->terrain_authored->resident_bytes+sizeof(*s->terrain_draw)+
        SCENE_TERRAIN_DRAW_VERTICES*sizeof(*s->terrain_colors)+sizeof(*s->terrain_light_cache)+
        sizeof(*s->debris)+sizeof(*s->terrain_template)+(uint64_t)s->terrain_region_count*sizeof(*s->terrain_regions)+
        1100u*(sizeof(uint32_t)+sizeof(rf_vfx_light_source))+SCENE_TERRAIN_FACES*sizeof(*s->terrain_ids)+
        (uint64_t)s->geometry->faces*sizeof(uint32_t)+sizeof(*p)+SCENE_AUTHORED_DIGEST_BUDGET+SCENE_CHECKPOINT_MAX;
    reserved=external+active.resident_bytes+SCENE_TERRAIN_CORE_BUDGET+layout.core_bytes;
    if(reserved>SCENE_DESTRUCTION_BUDGET){printf("DETACHED_RESTORE_BUDGET %u %u\n",(uint32_t)reserved,SCENE_DESTRUCTION_BUDGET);return RF_RANGE;}
    p=calloc(1,sizeof(*p));if(!p)return RF_IO;p->scene=s;p->serial=serial;p->reserved_peak_bytes=(uint32_t)reserved;
    status=scene_authored_admission_import(data,&layout,s->terrain_history_minimum,s->terrain_history_maximum,&p->admission);if(status)goto failed;
    history=malloc(layout.core_bytes);if(!history){status=RF_IO;goto failed;}
    memcpy(history,data+layout.core_offset,layout.core_bytes);
    status=scene_checkpoint_materials(history,layout.core_bytes,0,s->terrain_material);if(status)goto failed;
    factory.scene=s;status=scene_authored_edit_create(&factory,SCENE_TERRAIN_CORE_BUDGET,&p->core);if(status)goto failed;
    {
        rf_collision_face_filter generated=s->terrain_authored->asset_view.source_filters[0];
        generated.query_flags=0;generated.face_flags=256;
        status=scene_detached_open(s,&generated,&p->pieces);if(status)goto failed;
        status=rf_geomod_piece_registry_begin(p->pieces,1);if(status)goto failed;
        status=rf_geomod_terrain_set_extraction(p->core,rf_geomod_piece_registry_emit,p->pieces);if(status)goto failed;
    }
    status=rf_geomod_terrain_history_decode(p->core,history,layout.core_bytes);if(status)goto failed;
    free(history);history=NULL;
    /* This registry is private until the final scene publication. Closing it on
     * any later failure rolls back both reconstructed geometry and body poses. */
    rf_geomod_piece_registry_commit(p->pieces);
    if(layout.piece_bytes) {
        status=rf_geomod_piece_registry_state_decode(p->pieces,data+layout.piece_offset,layout.piece_bytes);if(status)goto failed;
    }
    status=rf_geomod_terrain_get(p->core,&core);if(status)goto failed;
    p->cuts=core.cuts;if(p->cuts>RF_GEOMOD_CUT_LIMIT || p->cuts>serial){status=RF_FORMAT;goto failed;}
    for(i=0;i<p->cuts;i++) {
        status=rf_geomod_terrain_cutter_get(p->core,i,&cuts[i].mesh,cuts[i].kernel,&cuts[i].star);if(status)goto failed;
    }
    status=scene_terrain_publication_prepare_from(s,&core,cuts,p->cuts,serial);if(status)goto failed;
    p->owns_pending=1;
    status=scene_terrain_publication_candidate(s,&p->publication,&p->origins,&p->source_bindings);if(status)goto failed;
    if(p->publication.mesh.face_count!=layout.faces){status=RF_FORMAT;goto failed;}
    status=rf_collision_composition_pending(s->terrain_publication->composition,&composition);if(status)goto failed;
    if(!p->cuts) {
        if(layout.maps || layout.faces || checkpoint_u32(data+260) || checkpoint_u32(data+264) || checkpoint_u32(data+268)){
            status=RF_FORMAT;goto failed;
        }
        /* RFDS2 policy1 empty journal is canonical lazy generation0. The scene
         * serial remains independent; RNG0 or1 is preserved exactly. */
        status=scene_terrain_lighting_stage_empty(s,&p->publication,serial,0,checkpoint_u32(data+256),&p->lighting);
        if(status)goto failed;
    } else {
        status=scene_terrain_lighting_stage_clone(s,&p->publication,&p->lighting);if(status)goto failed;
        status=scene_authored_journal_import(s,p->lighting,&layout,data,p->origins,p->source_bindings,serial,p->cuts);if(status)goto failed;
        status=scene_terrain_lighting_stage_bake(p->lighting,p->source_bindings,0);if(status)goto failed;
        status=scene_terrain_lighting_stage_draw(p->lighting);if(status)goto failed;
    }
    status=scene_authored_digest_capture(s,&p->publication,p->origins,&composition,p->lighting->staged,serial,
        p->face_maps,RF_GEOMOD_PUBLICATION_FACES,&p->expected,&p->digest_peak_bytes);if(status)goto failed;
    for(i=0;i<layout.faces;i++) {
        uint32_t at=layout.face_offset+i*2,map=data[at]|((uint32_t)data[at+1]<<8);
        if(p->face_maps[i]!=map){status=RF_FORMAT;goto failed;}
    }
    status=rf_authored_owner_extension_decode(data+288,128,&p->expected,p->cuts,&p->extension);if(status)goto failed;
    if(p->extension.serial!=serial){status=RF_FORMAT;goto failed;}
    if(player) {
        /* Only this composed tree replaces the full room. The publication
         * mesh is a subset and cannot provide cavity-style player admission. */
        full=p->publication;full.faces=composition.tree->faces;full.tree=p->lighting->candidate.tree;
        full.mesh.face_count=composition.count;
        status=rf_checkpoint_standing_check(&full,player,scene_step_seconds,rf_scene_actor_movement_values.speed,NULL);
        if(!status) {
            status=rf_geomod_piece_registry_placement_check(p->pieces,player);
            if(status)printf("DETACHED_PLAYER_RESTORE_REJECT %d\n",status);
        }
        if(status==RF_NOT_FOUND)status=RF_FORMAT;if(status)goto failed;
    }
    *out=p;return RF_OK;
failed:
    free(history);scene_authored_checkpoint_stage_discard(&p);return status;
}

/* The prepared owner is synchronous: no edit/renderer callback may intervene.
 * Publication validates bindings before its nonallocating commit. Everything
 * after it succeeds is ownership transfer or copying already validated state. */
static int scene_authored_checkpoint_stage_commit(scene_authored_checkpoint_stage *p)
{
    scene_stream *s,*staged;rf_geomod_terrain *old;uint32_t images;int status;
    if(!p || !p->owns_pending || !p->core || !p->lighting ||
       !p->lighting->draw_ready || p->lighting->committed)return RF_RANGE;
    s=p->scene;staged=p->lighting->staged;
    if(!s || !s->terrain_publication || !s->terrain_publication->has_pending ||
       p->lighting->owner!=s)return RF_RANGE;
    images=s->light_rgb.count;
    if(s->terrain_atlas_registered || staged->terrain_checkpoint_loaded==2) {
        if(staged->terrain_atlas_index>=256)return RF_RANGE;
        if(images<=staged->terrain_atlas_index)images=staged->terrain_atlas_index+1;
    }
    status=scene_terrain_publication_finish(s,staged->terrain_bindings,p->publication.mesh.face_count,images);
    if(status)return status;
    p->owns_pending=0;
    scene_terrain_lighting_stage_commit(p->lighting);
    old=s->terrain;s->terrain=p->core;p->core=NULL;
    rf_geomod_piece_registry_commit(p->pieces);
    rf_geomod_piece_registry_close(&s->detached_pieces);s->detached_pieces=p->pieces;p->pieces=NULL;
    s->terrain_publication_serial=p->serial;
    memcpy(s->terrain_history,p->admission.history,sizeof(s->terrain_history));
    memcpy(s->terrain_requested,p->admission.requested,sizeof(s->terrain_requested));
    s->terrain_history_count=p->admission.count;s->terrain_random=p->admission.random;
    s->terrain_atlas_index=staged->terrain_atlas_index;
    s->terrain_checkpoint_loaded=staged->terrain_checkpoint_loaded;
    rf_scene_geomod[0]=1;rf_scene_geomod[1]=p->cuts;rf_scene_geomod[2]=p->serial;
    rf_scene_geomod[3]=s->terrain_publication->resident_bytes+s->terrain_authored->resident_bytes;
    rf_scene_geomod[4]=s->terrain_publication->peak_bytes+s->terrain_authored->peak_bytes;
    rf_geomod_terrain_close(&old);return RF_OK;
}
