/* Include after scene_checkpoint_map_read/base and lighting stage definitions.
 * The RFDS2 owner/source/publication/collision identities and core history are
 * validated by the caller; this adapter checks and imports the saved journal.
 * origins/source_bindings must be the pending publication's authoritative arrays,
 * not saved resource IDs; final publication_finish rechecks source byte equality.
 * All writes target an uncommitted lighting clone. On failure discard the stage;
 * its private contents may be partially rebuilt. No live/global/file mutation.
 */
#include "rf/authored_checkpoint_layout.h"
static int scene_authored_journal_import_mode(scene_stream *live,
    scene_terrain_lighting_stage *stage,const rf_authored_checkpoint_layout *layout,
    const unsigned char *data,const rf_geomod_publication_origin *origins,
    const rf_preview_surface_lightmap *source_bindings,uint32_t serial,uint32_t cuts,uint32_t collection)
{
    rf_authored_checkpoint_layout checked;scene_stream *s;scene_terrain_noise_owner *owner;
    const rf_geomod_terrain_view *v;rf_random_state chain={1};uint32_t i,j,k,x=0,y=0,row=0,texels=0,index;
    int status;
    if(!live || !stage || !layout || !data || !origins || !source_bindings ||
       stage->owner!=live || stage->staged!=&stage->clone || stage->committed ||
       stage->bake_ready || stage->draw_ready || !cuts || (!collection && (cuts>RF_GEOMOD_CUT_LIMIT || cuts>serial)) || serial==UINT32_MAX)return RF_RANGE;
    status=collection?rf_authored_collection_layout_read(data,layout->bytes,&checked):rf_authored_checkpoint_layout_read(data,layout->bytes,&checked);if(status)return status;
    if(memcmp(&checked,layout,sizeof(checked)))return RF_FORMAT;
    if(collection) {
        rf_authored_sources_layout sources;uint32_t total=0;
        status=rf_authored_sources_read(data+layout->core_offset,layout->core_bytes,&sources);if(status)return status;
        if(sources.count!=live->terrain_source_count || sources.count<2)return RF_FORMAT;
        for(i=0;i<sources.count;i++) {
            uint32_t local=checkpoint_u32(data+layout->core_offset+sources.sources[i].core_offset+12);
            if(local>serial)return RF_FORMAT;total+=local;
        }
        if(total!=cuts)return RF_FORMAT;
    }
    if(checkpoint_u32(data+312)!=scene_authored_material_policy(live))return RF_FORMAT;
    s=stage->staged;owner=s->terrain_noise;v=&stage->candidate;
    if(!owner || owner==live->terrain_noise || s->terrain_atlas_pixels==live->terrain_atlas_pixels ||
       s->terrain_bindings==live->terrain_bindings || s->terrain_light_cache==live->terrain_light_cache ||
       layout->faces!=v->mesh.face_count || v->cuts!=cuts || v->mesh.generation!=serial ||
       !v->mesh.vertices || !v->mesh.faces || !v->faces || layout->maps>RF_GEOMOD_LIGHTMAP_LIMIT)return RF_FORMAT;
    if(!live->light_rgb.images || (!live->terrain_atlas_registered && live->light_rgb.count>=256) ||
       (live->terrain_atlas_registered && live->terrain_atlas_index>=256))return RF_RANGE;
    index=live->terrain_atlas_registered?live->terrain_atlas_index:live->light_rgb.count;
    memset(owner,0,sizeof(*owner));memset(s->terrain_atlas_pixels,0,512u*512u*2u);
    memset(s->terrain_light_cache,0,sizeof(*s->terrain_light_cache));
    for(i=0;i<layout->maps;i++) {
        scene_terrain_noise_map *m=owner->maps+i;
        {
            const unsigned char *record=data+layout->map_offset+i*88;unsigned char local[88];
            uint32_t token=checkpoint_u32(record+40),material;
            if(token && scene_authored_material_policy(live)!=2)return RF_FORMAT;
            status=scene_authored_material_resolve(live,token,0,&material);if(status)return status;
            memcpy(local,record,88);checkpoint_put(local+40,0);
            status=scene_checkpoint_map_read(local,m,material);if(status)return status;
        }
        if(x+m->width>512){y+=row;x=row=0;}
        if(m->x!=x || m->y!=y)return RF_FORMAT;
        x+=m->width;if(m->height>row)row=m->height;
        m->binding.image=index;status=scene_checkpoint_base(s,m,&chain);if(status)return status;
        texels+=m->width*m->height;
    }
    if(x!=checkpoint_u32(data+260) || y!=checkpoint_u32(data+264) || row!=checkpoint_u32(data+268) ||
       chain.value!=checkpoint_u32(data+256))return RF_FORMAT;
    for(i=0;i<layout->faces;i++) {
        const rf_geomod_face *f=v->mesh.faces+i;const rf_geomod_publication_origin *o=origins+i;
        uint32_t at=layout->face_offset+i*2,map=data[at]|((uint32_t)data[at+1]<<8);
        if(o->kind>RF_GEOMOD_PUBLICATION_NEIGHBOR || f->count<3 || f->first>v->mesh.vertex_count ||
           f->count>v->mesh.vertex_count-f->first)return RF_FORMAT;
        if(o->kind!=RF_GEOMOD_PUBLICATION_CRATER && o->reference!=UINT32_MAX) {
            if(map!=65535 || f->source_face==UINT32_MAX || o->source_face==UINT32_MAX ||
               o->reference!=f->source_face)return RF_FORMAT;
            if(source_bindings[i].image!=UINT32_MAX) {
                const rf_lightmap_projection *projection=&source_bindings[i].projection;
                if(source_bindings[i].image>=live->light_rgb.count || projection->axes[0]==projection->axes[1])return RF_FORMAT;
                for(j=0;j<2;j++)if(projection->axes[j]>2 || !isfinite(projection->scale[j]) || !isfinite(projection->offset[j]))return RF_FORMAT;
            }
            s->terrain_bindings[i]=source_bindings[i];continue;
        }
        if(f->source_face!=UINT32_MAX || map>=layout->maps || f->material!=owner->maps[map].material ||
           !scene_terrain_noise_plane(owner->maps[map].plane,v->faces[i].plane))return RF_FORMAT;
        if(o->kind==RF_GEOMOD_PUBLICATION_CRATER) {
            if(o->source_face!=UINT32_MAX || f->material!=live->terrain_material)return RF_FORMAT;
        } else if(scene_authored_material_policy(live)!=2 || o->kind!=RF_GEOMOD_PUBLICATION_NEIGHBOR ||
            o->reference!=UINT32_MAX || o->source_face==UINT32_MAX || f->material==live->terrain_material)return RF_FORMAT;
        for(j=0;j<f->count;j++)for(k=0;k<3;k++) {
            float value=v->mesh.vertices[f->first+j].position[k];
            if(!isfinite(value) || value<owner->maps[map].minimum[k]-1e-4f || value>owner->maps[map].maximum[k]+1e-4f)return RF_FORMAT;
        }
        s->terrain_bindings[i]=owner->maps[map].binding;
    }
    owner->count=owner->bake=layout->maps;owner->sample=0;owner->random=chain;
    owner->x=x;owner->y=y;owner->row=row;owner->generation=serial;owner->cuts=cuts;
    s->terrain_atlas_index=index;
    if(!live->terrain_atlas_registered)s->terrain_checkpoint_loaded=2;
    scene_terrain_dirty(s,0,0,512,512);
    memset(stage->telemetry.bake,0,sizeof(stage->telemetry.bake));
    memset(stage->telemetry.noise,0,sizeof(stage->telemetry.noise));
    stage->telemetry.noise[0]=1;stage->telemetry.noise[1]=layout->maps;
    stage->telemetry.noise[3]=texels;stage->telemetry.noise[6]=sizeof(*owner);stage->telemetry.noise[7]=serial;
    stage->telemetry.atlas[4]=serial;stage->telemetry.atlas[5]=texels;stage->telemetry.atlas[6]=layout->maps;
    return RF_OK;
}

static int scene_authored_journal_import(scene_stream *live,scene_terrain_lighting_stage *stage,
    const rf_authored_checkpoint_layout *layout,const unsigned char *data,const rf_geomod_publication_origin *origins,
    const rf_preview_surface_lightmap *bindings,uint32_t serial,uint32_t cuts)
{return scene_authored_journal_import_mode(live,stage,layout,data,origins,bindings,serial,cuts,0);}
static inline int scene_authored_collection_journal_import(scene_stream *live,scene_terrain_lighting_stage *stage,
    const rf_authored_checkpoint_layout *layout,const unsigned char *data,const rf_geomod_publication_origin *origins,
    const rf_preview_surface_lightmap *bindings,uint32_t serial,uint32_t cuts)
{return scene_authored_journal_import_mode(live,stage,layout,data,origins,bindings,serial,cuts,1);}
