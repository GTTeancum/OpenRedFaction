#ifndef RF_SCENE_DRILLER_COCKPIT_INC
#define RF_SCENE_DRILLER_COCKPIT_INC
#include "rf/preview.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
/* Authored driller01.vfx, six Scene Root meshes. Original4a77a0 parent is
 * refreshed PLAYER EYE position plus PLAYER BODY basis, not look basis/tag19.
 * Caller supplies separate final view camera. Retain owner across board/exit.
 * Elapsed driver-active seconds -> shared15Hz/endpoint loop is first-pass clock
 * policy; no separate clock per mesh. No cockpit-state selection/lighting polish.
 * Box03 alpha uses existing faded mode; exact alpha-test+depth-write is deferred. */
typedef struct scene_driller_cockpit {
    rf_vfx_geometry_asset *geometry;rf_vfx_asset_materials *materials;
    uint32_t endpoint,resident_bytes,peak_bytes,merged,texture_base[64],texture_frames;
} scene_driller_cockpit;
static void scene_driller_cockpit_close(scene_driller_cockpit **owner)
{
    scene_driller_cockpit *o;if(!owner || !(o=*owner))return;
    rf_vfx_asset_materials_close(&o->materials);rf_vfx_geometry_asset_close(&o->geometry);free(o);*owner=NULL;
}
static int scene_driller_cockpit_open(rf_vpp *meshes,rf_vpp *maps,uint32_t map_count,uint32_t budget,scene_driller_cockpit **out)
{
    scene_driller_cockpit *o;rf_vfx_directory d={0};uint32_t used,i;int status;
    if(!meshes || !maps || !map_count || !out || *out || budget<sizeof(*o))return RF_RANGE;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;used=sizeof(*o);
    status=rf_vfx_directory_open(meshes,"driller01.vfx",budget-used,&d);if(status)goto fail;
    o->endpoint=d.header.values[1];o->peak_bytes=used+d.allocated_bytes;rf_vfx_directory_close(&d);
    if(o->endpoint!=26){status=RF_FORMAT;goto fail;}
    status=rf_vfx_geometry_asset_open(meshes,"driller01.vfx",budget-used,&o->geometry);if(status)goto fail;
    if(o->peak_bytes<used+o->geometry->peak_bytes)o->peak_bytes=used+o->geometry->peak_bytes;
    used+=o->geometry->resident_bytes;
    for(i=0;i<o->geometry->count;i++)if(strcmp(o->geometry->meshes[i]->prefix.parent,"Scene Root")){status=RF_FORMAT;goto fail;}
    status=rf_vfx_asset_materials_open(o->geometry,maps,map_count,budget-used,&o->materials);if(status)goto fail;
    if(o->materials->textures.texture_count>64){status=RF_RANGE;goto fail;}
    o->resident_bytes=used+o->materials->resident_bytes;
    if(o->peak_bytes<o->resident_bytes)o->peak_bytes=o->resident_bytes;
    *out=o;return RF_OK;
fail:rf_vfx_directory_close(&d);scene_driller_cockpit_close(&o);return status;
}
/* Transfer pixels into scene material owner before renderer texture upload.
 * Retained empty image descriptors still select authored VBM frame indices. */
static int scene_driller_cockpit_merge(scene_driller_cockpit *o,rf_materials *materials,uint32_t capacity)
{
    rf_material *combined;uint32_t i,j,n=0;
    if(!o || !o->materials || !materials || o->merged)return RF_RANGE;
    for(i=0;i<o->materials->textures.texture_count;i++)n+=o->materials->textures.textures[i].animation.count;
    if(materials->count>capacity || n>capacity-materials->count)return RF_RANGE;
    combined=realloc(materials->items,(materials->count+n)*sizeof(*combined));if(!combined)return RF_IO;
    materials->items=combined;
    for(i=0;i<o->materials->textures.texture_count;i++) {
        rf_particle_animation *a=&o->materials->textures.textures[i].animation;o->texture_base[i]=materials->count;
        for(j=0;j<a->count;j++) {
            combined[materials->count++]=(rf_material){a->images[j],RF_OK,a->archive_index};
            materials->allocated_bytes+=sizeof(*combined)+a->images[j].bytes;materials->loaded++;
            memset(a->images+j,0,sizeof(*a->images));
        }
    }
    o->merged=1;o->texture_frames=n;return RF_OK;
}
static int scene_driller_cockpit_draw(scene_driller_cockpit *o,float seconds,const float eye[3],
    const float body_basis[9],const rf_level *camera,rf_preview_mesh *output,uint32_t capacity_bytes,
    uint32_t material_count,const unsigned char lighting[3],uint32_t *faces_drawn)
{
    uint32_t m,f,j,k,count=0;float phase;rf_level local_camera;int status;
    if(!o || !o->merged || !eye || !body_basis || !camera || !output || !lighting || !faces_drawn)return RF_RANGE;
    if(!isfinite(seconds) || seconds<0 || output->bytes>capacity_bytes)return RF_FORMAT;
    phase=fmodf(seconds,(float)o->endpoint*(1.f/15.f))*15.f;local_camera=*camera;
    for(k=0;k<3;k++)local_camera.player_position[k]-=eye[k];
    for(m=0;m<o->geometry->count;m++) {
        rf_vfx_mesh *source=o->geometry->meshes[m];rf_vfx_instance *instance=o->geometry->instances[m];
        status=rf_vfx_instance_update(instance,phase);if(status)return status;if(!instance->active)continue;
        for(f=0;f<source->prefix.faces;f++) {
            rf_vfx_face face;const rf_image *image;rf_geomod_vertex vertices[3];rf_geomod_face gf;
            rf_collision_face bound;rf_collision_face_filter filter={0};float positions[3][3],normal[3],triangle[9];
            rf_geomod_mesh_view mesh={0};rf_preview_mesh emitted={0};uint32_t material,slot,frame,alpha,tag;
            uint32_t offset=source->prefix.face_offset+f*(source->version<0x3000d?120:96);
            float opacity=1,brightness=0;unsigned char rgb[3];
            status=rf_vfx_face_read(source->data+offset,source->bytes-offset,source->version,&face);if(status)return status;
            material=o->materials->first[m]+face.material;if(material>=o->materials->count)return RF_FORMAT;
            slot=o->materials->textures.bindings[material][0];if(slot==UINT32_MAX)continue;
            status=rf_vfx_material_texture_sample(&o->materials->textures,o->materials->views+material,material,0,phase,0,&image);if(status)return status;
            frame=(uint32_t)(image-o->materials->textures.textures[slot].animation.images);
            status=rf_vfx_mesh_material_sample(source,o->geometry->material_bank,face.material,1,phase,&brightness);if(status && status!=RF_NOT_FOUND)return status;
            status=rf_vfx_mesh_material_sample(source,o->geometry->material_bank,face.material,2,phase,&opacity);if(status && status!=RF_NOT_FOUND)return status;
            alpha=(uint32_t)((double)(255.f*fminf(1.f,fmaxf(0.f,opacity)))+.5);if(!alpha)continue;
            status=rf_vfx_material_color(o->materials->views+material,lighting,brightness,source->edges.flags,rgb);if(status)return status;
            for(j=0;j<3;j++) {
                const float *v=instance->vertices+face.indices[j]*3;
                for(k=0;k<3;k++)vertices[j].position[k]=triangle[j*3+k]=v[0]*body_basis[k]+v[1]*body_basis[3+k]+v[2]*body_basis[6+k];
                vertices[j].uv[0]=instance->uv[f*6+j];vertices[j].uv[1]=instance->uv[f*6+3+j];
            }
            status=rf_vfx_face_normal(triangle,normal);if(status==RF_RANGE || status==RF_FORMAT)continue;if(status)return status;
            gf=(rf_geomod_face){0,3,o->texture_base[slot]+frame,UINT32_MAX};
            mesh.vertices=vertices;mesh.faces=&gf;mesh.vertex_count=3;mesh.face_count=1;
            status=rf_geomod_collision_faces(&mesh,&filter,positions,3,&bound,1);if(status)return status;
            emitted.vertices=output->vertices+output->count;
            status=rf_preview_geomod(&emitted,capacity_bytes-output->bytes,&mesh,&bound,material_count,&local_camera);if(status)return status;
            tag=(o->materials->views[material].words[2]&255)?RF_PREVIEW_ADDITIVE_FADE_TAG|alpha:
                (!strcmp(source->prefix.name,"Box03") || alpha<255)?RF_PREVIEW_FADE_TAG|alpha:RF_PREVIEW_VERTEX_LIT;
            for(j=0;j<emitted.count;j++){emitted.vertices[j].lightmap=tag;for(k=0;k<3;k++)emitted.vertices[j].color[k]=rgb[k]*(1.f/255.f);}
            output->count+=emitted.count;output->bytes+=emitted.bytes;if(emitted.count)count++;
        }
    }
    *faces_drawn=count;return RF_OK;
}
#endif
