#include "rf/scene_preview.h"
#include "rf/animation_check.h"
#include "rf/entity_assets.h"
#include <stdlib.h>
#include <string.h>
int rf_scene_preview_camera(rf_level *level,int32_t uid)
{
    rf_level_entity entity;uint32_t i,j;int status;
    if(!level)return RF_RANGE;
    status=rf_level_entity_find(level,uid,&entity);if(status)return status;
    for(i=0;i<3;++i) {
        level->player_position[i]=entity.position[i]+2.2f*entity.orientation[2][i];
        for(j=0;j<3;++j)level->player_orientation[i][j]=(i==1?1.0f:-1.0f)*entity.orientation[i][j];
    }
    return RF_OK;
}
int rf_scene_preview_miner(const rf_level *level,int32_t uid,const char *meshes_path,
    const char *motions_path,const char *tables_path,rf_vpp *maps,uint32_t map_count,
    rf_preview_mesh *mesh,rf_materials *materials,uint32_t mesh_budget,uint32_t material_budget)
{
    rf_vpp archive;rf_model_file model;rf_level_actor_assets binding;
    rf_animation_placement placement;rf_preview_mesh actor={0};rf_model_materials bundle={0};
    rf_preview_vertex *vertices=NULL;rf_material *items=NULL;const char *names[64];
    uint64_t bytes,count;uint32_t i;int status;
    if(!level || !mesh || !materials || !mesh->vertices || !materials->items ||
       mesh->count%3 || mesh->bytes!=(uint64_t)mesh->count*sizeof(*mesh->vertices) ||
       materials->allocated_bytes>=material_budget)return RF_RANGE;
    status=rf_vpp_open(&archive,meshes_path);if(status)return status;
    status=rf_level_actor_assets_load(level,uid,tables_path,&archive,512*1024,&binding);if(status)goto done;
    if(strcmp(binding.mesh.name,"miner.v3c")) {status=RF_FORMAT;goto done;}
    status=rf_animation_placement_from_level(level,&binding.entity,&placement);if(status)goto done;
    status=rf_animation_preview_placed(meshes_path,motions_path,&placement,0,&actor,1024*1024);if(status)goto done;
    status=rf_model_file_open(&model,&archive,binding.mesh.name);if(status)goto done;
    for(i=0;i<binding.assets.texture_count;++i)names[i]=binding.assets.textures[i];
    status=rf_model_materials_open_skin(&bundle,&model,names,binding.assets.texture_count,maps,map_count,
        material_budget-materials->allocated_bytes);if(status)goto done;
    bytes=((uint64_t)mesh->count+actor.count)*sizeof(*vertices);
    count=(uint64_t)materials->count+bundle.textures.count;
    if(bytes>mesh_budget || bytes>SIZE_MAX || count>256) {status=RF_RANGE;goto done;}
    for(i=0;i<actor.count;++i) {
        uint32_t material=actor.vertices[i].material,slot;
        if(material>=bundle.count) {status=RF_FORMAT;goto done;}
        memcpy(&slot,bundle.items[material].record.bytes+0x10,4);
        if(slot>=bundle.textures.count) {status=RF_FORMAT;goto done;}
        actor.vertices[i].material=materials->count+slot;
    }
    vertices=malloc((size_t)bytes);items=malloc((size_t)count*sizeof(*items));
    if(!vertices || !items) {status=RF_IO;goto done;}
    memcpy(vertices,mesh->vertices,mesh->bytes);memcpy(vertices+mesh->count,actor.vertices,actor.bytes);
    memcpy(items,materials->items,materials->count*sizeof(*items));
    memcpy(items+materials->count,bundle.textures.items,bundle.textures.count*sizeof(*items));
    free(mesh->vertices);free(materials->items);
    mesh->vertices=vertices;vertices=NULL;mesh->count+=actor.count;mesh->bytes=(uint32_t)bytes;
    materials->items=items;items=NULL;materials->count=(uint32_t)count;
    materials->allocated_bytes+=bundle.textures.allocated_bytes;
    materials->loaded+=bundle.textures.loaded;materials->missing+=bundle.textures.missing;
    /* Image ownership moved to combined materials, instance records stay local. */
    free(bundle.textures.items);memset(&bundle.textures,0,sizeof(bundle.textures));
done:
    free(vertices);free(items);rf_preview_close(&actor);rf_model_materials_close(&bundle);
    rf_vpp_close(&archive);return status;
}
