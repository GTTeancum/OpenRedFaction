/* Include after checkpoint_* helpers and scene_authored_digest_capture.inc.
 * RFDS2 only: caller owns RFCP/player/transport framing. Main-thread synchronous
 * snapshot; input scene must not mutate. Buffer/written are disjoint from scene
 * and each other. Entire output is staged, so every failure preserves outputs.
 * Additional peak: sizeof(writer)+payload<=109948+digest<512KiB, bounded1MiB.
 * No GPU, global diagnostics, owner pointers, RNG or live state are changed.
 */
#include "rf/authored_checkpoint_layout.h"
typedef struct scene_authored_checkpoint_writer {
    scene_stream lighting;
    scene_terrain_noise_owner empty;
    uint16_t face_maps[RF_GEOMOD_PUBLICATION_FACES];
    rf_authored_owner_expected expected;
    rf_authored_owner_extension extension;
} scene_authored_checkpoint_writer;
static inline int scene_authored_checkpoint_write(scene_stream *s,void *buffer,uint32_t capacity,uint32_t *written)
{
    scene_authored_checkpoint_writer *w=NULL;unsigned char *p=NULL;
    const scene_terrain_noise_owner *owner;scene_publication_bank *bank;
    rf_geomod_terrain_view view,core_view;rf_collision_composition_view composition;
    rf_authored_checkpoint_layout layout,checked;uint32_t core,i,j,k,at,peak,serial;
    size_t name_length;int status;
    if(!s || !buffer || !written || !s->terrain || !s->terrain_authored || !s->terrain_publication ||
       !s->terrain_noise || s->terrain_publication->has_pending || s->terrain_shadow_reference ||
       !s->terrain_atlas_registered || s->terrain_checkpoint_loaded || s->terrain_history_count>128)return RF_RANGE;
    owner=s->terrain_noise;serial=s->terrain_publication_serial;
    if(serial==UINT32_MAX || owner->count>RF_GEOMOD_LIGHTMAP_LIMIT || owner->bake!=owner->count || owner->sample)return RF_FORMAT;
    status=scene_terrain_publication_view(s,&view);if(status)return status;
    status=rf_geomod_terrain_get(s->terrain,&core_view);if(status)return status;
    if(view.cuts!=core_view.cuts || view.mesh.generation!=serial || owner->cuts!=view.cuts)return RF_FORMAT;
    bank=s->terrain_publication->banks+s->terrain_publication->active;
    status=rf_collision_composition_get(s->terrain_publication->composition,&composition);if(status)return status;
    if(view.cuts) {if(owner->generation!=serial)return RF_FORMAT;}
    else {
        if(view.mesh.face_count || owner->count || owner->x || owner->y || owner->row || owner->bake ||
           owner->random.value>1 || (owner->generation && (owner->generation!=serial || !owner->random.value)))return RF_FORMAT;
    }
    name_length=0;while(name_length<64 && campaign_current_level[name_length])name_length++;
    if(!name_length || name_length==64)return RF_FORMAT;
    for(j=0;j<3;j++)if(!isfinite(s->terrain_history_minimum[j]) || !isfinite(s->terrain_history_maximum[j]) ||
        s->terrain_history_maximum[j]<=s->terrain_history_minimum[j])return RF_FORMAT;
    for(i=0;i<s->terrain_history_count;i++) {
        const rf_geomod_shallow_history *h=s->terrain_history+i;
        if(!isfinite(h->scale) || !(h->scale>0))return RF_FORMAT;
        for(j=0;j<3;j++) {
            if(!isfinite(h->center[j]))return RF_FORMAT;
            for(k=0;k<2;k++)if(!isfinite(h->vectors[k][j]))return RF_FORMAT;
        }
    }
    status=rf_geomod_terrain_history_size(s->terrain,&core);if(status)return status;
    status=rf_authored_checkpoint_layout_size(core,s->terrain_history_count,owner->count,view.mesh.face_count,&layout);if(status)return status;
    if(capacity<layout.bytes || (uint64_t)sizeof(*w)+layout.bytes+SCENE_AUTHORED_DIGEST_BUDGET>SCENE_AUTHORED_WRITER_BUDGET)return RF_RANGE;
    w=calloc(1,sizeof(*w));if(!w)return RF_IO;p=calloc(1,layout.bytes);if(!p){status=RF_IO;goto done;}
    w->lighting=*s;
    if(!view.cuts){w->empty.random.value=owner->random.value;w->lighting.terrain_noise=&w->empty;}
    status=scene_authored_digest_capture(s,&view,bank->origins,&composition,&w->lighting,serial,
        w->face_maps,RF_GEOMOD_PUBLICATION_FACES,&w->expected,&peak);if(status)goto done;
    if((uint64_t)sizeof(*w)+layout.bytes+peak>SCENE_AUTHORED_WRITER_BUDGET){status=RF_RANGE;goto done;}
    memcpy(p,"RFDS",4);checkpoint_put(p+4,2);checkpoint_put(p+8,layout.bytes);
    memcpy(p+16,campaign_current_level,name_length);memcpy(p+80,s->terrain_checkpoint_identity,128);
    checkpoint_put(p+208,s->terrain_texture_width);checkpoint_put(p+212,s->terrain_texture_height);
    for(i=0;i<3;i++){checkpoint_put_float(p+216+i*4,s->terrain_history_minimum[i]);checkpoint_put_float(p+228+i*4,s->terrain_history_maximum[i]);}
    checkpoint_put(p+240,s->terrain_history_count);checkpoint_put(p+244,s->terrain_random.value);
    checkpoint_put(p+248,owner->count);checkpoint_put(p+252,core);checkpoint_put(p+256,owner->random.value);
    checkpoint_put(p+260,owner->x);checkpoint_put(p+264,owner->y);checkpoint_put(p+268,owner->row);checkpoint_put(p+272,view.mesh.face_count);
    checkpoint_put(p+276,416);checkpoint_put(p+280,128);checkpoint_put(p+284,2);
    w->extension.uid=w->expected.uid;w->extension.source_count=w->expected.source_count;w->extension.neighbor_count=w->expected.neighbor_count;
    w->extension.publication_policy=w->extension.collision_policy=w->extension.material_policy=1;w->extension.serial=serial;
    memcpy(w->extension.publication_digest,w->expected.publication_digest,32);
    memcpy(w->extension.collision_digest,w->expected.collision_digest,32);memcpy(w->extension.material_digest,w->expected.material_digest,32);
    status=rf_authored_owner_extension_encode(&w->extension,&w->expected,view.cuts,p+288,128);if(status)goto done;
    status=rf_geomod_terrain_history_encode(s->terrain,p+layout.core_offset,core);if(status)goto done;
    status=scene_checkpoint_materials(p+layout.core_offset,core,s->terrain_material,0);if(status)goto done;
    at=layout.admission_offset;
    for(i=0;i<layout.admissions;i++,at+=48) {
        const rf_geomod_shallow_history *h=s->terrain_history+i;
        for(j=0;j<3;j++)checkpoint_put_float(p+at+j*4,h->center[j]);
        for(j=0;j<2;j++)for(k=0;k<3;k++)checkpoint_put_float(p+at+12+(j*3+k)*4,h->vectors[j][k]);
        checkpoint_put_float(p+at+36,h->scale);
        for(j=0;j<3;j++){p[at+40+j*2]=(unsigned char)s->terrain_requested[i][j];p[at+41+j*2]=(unsigned char)(s->terrain_requested[i][j]>>8);}
    }
    for(i=0;i<layout.maps;i++,at+=88) {
        const scene_terrain_noise_map *m=owner->maps+i;
        for(j=0;j<4;j++)checkpoint_put_float(p+at+j*4,m->plane[j]);
        for(j=0;j<3;j++){checkpoint_put_float(p+at+16+j*4,m->minimum[j]);checkpoint_put_float(p+at+28+j*4,m->maximum[j]);}
        /* +40 is canonical substrate token0 from calloc, never runtime slot. */
        checkpoint_put(p+at+44,m->x);checkpoint_put(p+at+48,m->y);checkpoint_put(p+at+52,m->width);checkpoint_put(p+at+56,m->height);checkpoint_put(p+at+60,m->base_seed);
        for(j=0;j<2;j++){checkpoint_put(p+at+64+j*4,m->binding.projection.axes[j]);checkpoint_put_float(p+at+72+j*4,m->binding.projection.scale[j]);checkpoint_put_float(p+at+80+j*4,m->binding.projection.offset[j]);}
    }
    for(i=0;i<layout.faces;i++,at+=2){p[at]=(unsigned char)w->face_maps[i];p[at+1]=(unsigned char)(w->face_maps[i]>>8);}
    if(at!=layout.bytes){status=RF_FORMAT;goto done;}
    status=rf_authored_checkpoint_layout_read(p,layout.bytes,&checked);if(status)goto done;
    if(memcmp(&checked,&layout,sizeof(layout))){status=RF_FORMAT;goto done;}
    memcpy(buffer,p,layout.bytes);*written=layout.bytes;status=RF_OK;
done:
    free(p);free(w);return status;
}
