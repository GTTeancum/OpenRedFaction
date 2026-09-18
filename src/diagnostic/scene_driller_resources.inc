#ifndef RF_SCENE_DRILLER_RESOURCES_INC
#define RF_SCENE_DRILLER_RESOURCES_INC
#include "rf/entity_assets.h"
#include "rf/material.h"
#include <stdlib.h>
#include <string.h>
/* Actual Driller01 static chassis foundation. No fabricated skeletal idle:
 * authored class has model kind1 and no State/Action declarations. Render
 * highest LOD per part with ordinary static geometry/material APIs. Cockpit,
 *19thruster VFX, glares, idle audio and driving remain separate integrations.
 * Returned metadata, images, geometry and tags survive source archive close.
 * Accounting includes retained descriptors and maximum sequential scratch,
 * excluding decoder stack and allocator overhead like existing model owners. */
typedef struct scene_driller_resources {
    rf_entity_assets assets;char model[64];
    rf_entity_physics_config physics;rf_entity_movement_values movement;rf_entity_rotation_values rotation;
    rf_entity_creation_vitals_class vitals;
    rf_static_render_resource render;rf_model_materials materials;rf_static_model_tags tags;
    int32_t seat;uint32_t resident_bytes,peak_bytes;
} scene_driller_resources;
static void scene_driller_resources_close(scene_driller_resources **owner)
{
    scene_driller_resources *o;if(!owner || !(o=*owner))return;
    rf_model_materials_close(&o->materials);rf_static_model_tags_close(&o->tags);
    rf_static_render_resource_close(&o->render);free(o);*owner=NULL;
}
static int scene_driller_resources_open(const char *tables_path,rf_vpp *meshes,
    rf_vpp *maps,uint32_t map_count,uint32_t budget,scene_driller_resources **result)
{
    scene_driller_resources *o=NULL;rf_model_file *file=NULL;rf_vpp tables={0};rf_vpp_entry entry;
    const char *overrides[64];void *table_text=NULL;uint32_t used,i,table_bytes;int status;
    if(!tables_path || !meshes || !maps || !map_count || !result || *result || budget<sizeof(*o)+sizeof(*file))return RF_RANGE;
    o=calloc(1,sizeof(*o));if(!o)return RF_IO;o->seat=-1;used=sizeof(*o);
    status=rf_vpp_open(&tables,tables_path);if(status)goto fail;
    status=rf_vpp_find(&tables,"entity.tbl",&entry);if(status)goto fail;table_bytes=entry.size;
    if(table_bytes>budget-used){status=RF_RANGE;goto fail;}
    o->peak_bytes=used+table_bytes;
    table_text=malloc(table_bytes);if(!table_text){status=RF_IO;goto fail;}
    status=rf_vpp_read(&tables,&entry,0,table_text,table_bytes);if(status)goto fail;
    status=rf_entity_rotation_values_read(table_text,table_bytes,"Driller01",&o->rotation);if(status)goto fail;
    free(table_text);table_text=NULL;
    status=rf_entity_assets_load(tables_path,"Driller01","",&o->assets,budget-used);if(status)goto fail;
    status=rf_model_compiled_filename(o->assets.model,o->model,".v3m");if(status)goto fail;
    status=rf_entity_physics_config_load(&tables,"Driller01",budget-used,&o->physics);if(status)goto fail;
    status=rf_entity_movement_load(&tables,"Driller01",budget-used,&o->movement);if(status)goto fail;
    status=rf_entity_vitals_config_load(&tables,"Driller01",budget-used,&o->vitals);if(status)goto fail;
    if(o->physics.authored.use_kind!=1 || o->physics.authored.movement_index!=5){status=RF_FORMAT;goto fail;}
    rf_vpp_close(&tables);
    file=malloc(sizeof(*file));if(!file){status=RF_IO;goto fail;}used+=sizeof(*file);
    status=rf_model_file_open(file,meshes,o->model);if(status)goto fail;
    status=rf_static_render_resource_open(file,budget-used,&o->render);if(status)goto fail;used+=o->render.allocated_bytes;
    status=rf_static_model_tags_open(file,budget-used,&o->tags);if(status)goto fail;used+=o->tags.allocated_bytes;
    for(i=0;i<o->tags.count;i++)if(!strcmp(o->tags.items[i].name,"interface_1")){o->seat=(int32_t)i;break;}
    if(o->seat<0){status=RF_NOT_FOUND;goto fail;}
    if(o->assets.texture_count>64){status=RF_RANGE;goto fail;}
    for(i=0;i<o->assets.texture_count;i++)overrides[i]=o->assets.textures[i];
    status=rf_model_materials_open_skin(&o->materials,file,overrides,o->assets.texture_count,maps,map_count,budget-used);if(status)goto fail;
    if(used+o->materials.peak_bytes>o->peak_bytes)o->peak_bytes=used+o->materials.peak_bytes;
    o->resident_bytes=used+o->materials.resident_bytes-sizeof(*file);
    free(file);*result=o;return RF_OK;
fail:
    rf_vpp_close(&tables);free(table_text);free(file);scene_driller_resources_close(&o);return status;
}
static int scene_driller_seat_pose(const scene_driller_resources *owner,const float position[3],
    const float basis[9],float pose[12])
{
    if(!owner)return RF_RANGE;
    return rf_static_model_tag_place(&owner->tags,owner->seat,basis,position,pose);
}
#endif
